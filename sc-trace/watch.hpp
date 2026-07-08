#ifndef CC_VRWRAPPER_TRACE_WATCH_HPP
#define CC_VRWRAPPER_TRACE_WATCH_HPP

// ============================================================================
// cc_vrwrapper :: sc-trace :: watch.hpp
// Signal watching with callbacks.
//
// Provides:
//   watch(sig, cb)                    — any value change
//   watch_rising(sig, cb)             — rising edge (0→1)
//   watch_falling(sig, cb)            — falling edge (1→0)
//   watch_value(sig, value, cb)       — signal equals specific value
//   watch_transition(sig, from, to, cb) — specific from→to transition
//
// Callbacks may take either:
//   - void(const value_type&)  — receives the new signal value
//   - void()                   — receives nothing
// Both forms are accepted by every watch_* function.
//
// IMPORTANT: All watch() calls MUST be made AFTER all ports have been
// bound to their signals, and BEFORE sc_start() is called. Calling
// watch_*() on an unbound sc_in/sc_out port will raise SystemC error
// E112 ("port is not bound").
//
// Recommended patterns:
//   1. Call from sc_main() after all dut.port(signal) bindings are done.
//   2. Override start_of_simulation() in your module and call from there.
// ============================================================================

#include "utils.hpp"
#include <systemc>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <type_traits>

namespace cc_vrwrapper {
namespace trace {

// ========================================================================
// WatchMode — determines when a watcher fires
// ========================================================================

enum class WatchMode {
    ANY_CHANGE,
    RISING_EDGE,
    FALLING_EDGE,
    VALUE_MATCH,
    TRANSITION
};

// ========================================================================
// supports_edge_detection — trait for types that support rising/falling
// edge detection. Bus types (sc_lv, sc_bv) do NOT support edges.
// ========================================================================

template <typename T, typename = void>
struct supports_edge_detection : std::false_type {};

template <> struct supports_edge_detection<sc_logic> : std::true_type {};

template <typename T>
struct supports_edge_detection<T, std::enable_if_t<std::is_integral_v<T>>>
    : std::true_type {};

template <int W> struct supports_edge_detection<sc_int<W>>     : std::true_type {};
template <int W> struct supports_edge_detection<sc_uint<W>>    : std::true_type {};
template <int W> struct supports_edge_detection<sc_bigint<W>>  : std::true_type {};
template <int W> struct supports_edge_detection<sc_biguint<W>> : std::true_type {};

template <typename T>
inline constexpr bool supports_edge_detection_v = supports_edge_detection<T>::value;

// ========================================================================
// SignalWatcher — internal SC_MODULE that watches a signal
// ========================================================================

template <typename Signal>
class SignalWatcher : public sc_core::sc_module {
public:
    using value_type = signal_value_type_t<Signal>;

    SC_HAS_PROCESS(SignalWatcher<Signal>);

    SignalWatcher(sc_core::sc_module_name name,
                  Signal& sig,
                  std::function<void(const value_type&)> cb,
                  WatchMode mode = WatchMode::ANY_CHANGE,
                  value_type match_value = value_type{},
                  value_type from_value = value_type{})
        : sc_core::sc_module(name)
        , sig_(sig)
        , callback_(std::move(cb))
        , mode_(mode)
        , match_value_(match_value)
        , from_value_(from_value)
        , last_value_(sig.read())
    {
        SC_METHOD(process);
        sensitive << sig_.value_changed_event();
        dont_initialize();
    }

    void process()
    {
        value_type current = sig_.read();

        switch (mode_) {
            case WatchMode::ANY_CHANGE:
                callback_(current);
                break;
            case WatchMode::RISING_EDGE:
                if (is_rising_edge(last_value_, current)) {
                    callback_(current);
                }
                break;
            case WatchMode::FALLING_EDGE:
                if (is_falling_edge(last_value_, current)) {
                    callback_(current);
                }
                break;
            case WatchMode::VALUE_MATCH:
                if (current == match_value_) {
                    callback_(current);
                }
                break;
            case WatchMode::TRANSITION:
                if (last_value_ == from_value_ && current == match_value_) {
                    callback_(current);
                }
                break;
        }

        last_value_ = current;
    }

private:
    Signal& sig_;
    std::function<void(const value_type&)> callback_;
    WatchMode mode_;
    value_type match_value_;
    value_type from_value_;
    value_type last_value_;

    bool is_rising_edge(const value_type& old, const value_type& cur)
    {
        if constexpr (std::is_same_v<value_type, bool>) {
            return !old && cur;
        } else if constexpr (std::is_same_v<value_type, sc_logic>) {
            return (old == SC_LOGIC_0 && cur == SC_LOGIC_1);
        } else {
            return old == 0 && cur != 0;
        }
    }

    bool is_falling_edge(const value_type& old, const value_type& cur)
    {
        if constexpr (std::is_same_v<value_type, bool>) {
            return old && !cur;
        } else if constexpr (std::is_same_v<value_type, sc_logic>) {
            return (old == SC_LOGIC_1 && cur == SC_LOGIC_0);
        } else {
            return old != 0 && cur == 0;
        }
    }
};

// ========================================================================
// Watcher registry — keeps watchers alive during simulation
// ========================================================================

namespace detail {

inline std::vector<std::shared_ptr<sc_core::sc_module>>& watcher_registry()
{
    static std::vector<std::shared_ptr<sc_core::sc_module>> registry;
    return registry;
}

inline std::mutex& registry_mutex()
{
    static std::mutex m;
    return m;
}

inline int next_watcher_id()
{
    static std::atomic<int> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

// --------------------------------------------------------------------
// adapt_callback — wrap user callback (0-arg or 1-arg) into
// std::function<void(const value_type&)>.
// --------------------------------------------------------------------
template <typename ValueType, typename Callback>
std::function<void(const ValueType&)> adapt_callback(Callback cb)
{
    if constexpr (std::is_invocable_v<Callback, const ValueType&>) {
        return std::function<void(const ValueType&)>(std::move(cb));
    } else if constexpr (std::is_invocable_v<Callback>) {
        auto moved_cb = std::move(cb);
        return [moved_cb](const ValueType&) mutable { moved_cb(); };
    } else {
        static_assert(!sizeof(Callback),
            "Callback must be invocable with no args or with const ValueType&");
        return {};
    }
}

} // namespace detail

// ========================================================================
// PUBLIC API
// ========================================================================

template <typename Signal, typename Callback>
void watch(Signal& sig, Callback cb)
{
    static_assert(is_sc_signal_or_port_v<Signal>,
        "watch(): Signal must be sc_signal<T>, sc_in<T>, or sc_out<T>");

    using value_type = signal_value_type_t<Signal>;

    std::lock_guard<std::mutex> g(detail::registry_mutex());
    int id = detail::next_watcher_id();
    std::string name = "vrwatcher_" + std::to_string(id);

    auto watcher = std::make_shared<SignalWatcher<Signal>>(
        name.c_str(), sig,
        detail::adapt_callback<value_type>(std::move(cb)),
        WatchMode::ANY_CHANGE
    );
    detail::watcher_registry().push_back(
        std::static_pointer_cast<sc_core::sc_module>(watcher)
    );
}

template <typename Signal, typename Callback>
void watch_rising(Signal& sig, Callback cb)
{
    static_assert(is_sc_signal_or_port_v<Signal>,
        "watch_rising(): Signal must be sc_signal<T>, sc_in<T>, or sc_out<T>");

    using value_type = signal_value_type_t<Signal>;
    static_assert(supports_edge_detection_v<value_type>,
        "watch_rising(): value type must support edge detection "
        "(bool, sc_logic, integral, sc_int, sc_uint, sc_bigint, sc_biguint). "
        "For bus types (sc_lv, sc_bv), use watch_value or watch_transition.");

    std::lock_guard<std::mutex> g(detail::registry_mutex());
    int id = detail::next_watcher_id();
    std::string name = "vrwatcher_rise_" + std::to_string(id);

    auto watcher = std::make_shared<SignalWatcher<Signal>>(
        name.c_str(), sig,
        detail::adapt_callback<value_type>(std::move(cb)),
        WatchMode::RISING_EDGE
    );
    detail::watcher_registry().push_back(
        std::static_pointer_cast<sc_core::sc_module>(watcher)
    );
}

template <typename Signal, typename Callback>
void watch_falling(Signal& sig, Callback cb)
{
    static_assert(is_sc_signal_or_port_v<Signal>,
        "watch_falling(): Signal must be sc_signal<T>, sc_in<T>, or sc_out<T>");

    using value_type = signal_value_type_t<Signal>;
    static_assert(supports_edge_detection_v<value_type>,
        "watch_falling(): value type must support edge detection "
        "(bool, sc_logic, integral, sc_int, sc_uint, sc_bigint, sc_biguint). "
        "For bus types (sc_lv, sc_bv), use watch_value or watch_transition.");

    std::lock_guard<std::mutex> g(detail::registry_mutex());
    int id = detail::next_watcher_id();
    std::string name = "vrwatcher_fall_" + std::to_string(id);

    auto watcher = std::make_shared<SignalWatcher<Signal>>(
        name.c_str(), sig,
        detail::adapt_callback<value_type>(std::move(cb)),
        WatchMode::FALLING_EDGE
    );
    detail::watcher_registry().push_back(
        std::static_pointer_cast<sc_core::sc_module>(watcher)
    );
}

template <typename Signal, typename ValueType, typename Callback>
void watch_value(Signal& sig, const ValueType& value, Callback cb)
{
    static_assert(is_sc_signal_or_port_v<Signal>,
        "watch_value(): Signal must be sc_signal<T>, sc_in<T>, or sc_out<T>");

    using sig_value_type = signal_value_type_t<Signal>;

    std::lock_guard<std::mutex> g(detail::registry_mutex());
    int id = detail::next_watcher_id();
    std::string name = "vrwatcher_val_" + std::to_string(id);

    auto watcher = std::make_shared<SignalWatcher<Signal>>(
        name.c_str(), sig,
        detail::adapt_callback<sig_value_type>(std::move(cb)),
        WatchMode::VALUE_MATCH,
        static_cast<sig_value_type>(value)
    );
    detail::watcher_registry().push_back(
        std::static_pointer_cast<sc_core::sc_module>(watcher)
    );
}

template <typename Signal, typename ValueType, typename Callback>
void watch_transition(Signal& sig, const ValueType& from,
                      const ValueType& to, Callback cb)
{
    static_assert(is_sc_signal_or_port_v<Signal>,
        "watch_transition(): Signal must be sc_signal<T>, sc_in<T>, or sc_out<T>");

    using sig_value_type = signal_value_type_t<Signal>;

    std::lock_guard<std::mutex> g(detail::registry_mutex());
    int id = detail::next_watcher_id();
    std::string name = "vrwatcher_trans_" + std::to_string(id);

    auto watcher = std::make_shared<SignalWatcher<Signal>>(
        name.c_str(), sig,
        detail::adapt_callback<sig_value_type>(std::move(cb)),
        WatchMode::TRANSITION,
        static_cast<sig_value_type>(to),
        static_cast<sig_value_type>(from)
    );
    detail::watcher_registry().push_back(
        std::static_pointer_cast<sc_core::sc_module>(watcher)
    );
}

// --------------------------------------------------------------------
// clear_watchers() — destroy all watchers.
//
// WARNING: Only safe to call BEFORE sc_start() or after simulation
// has fully returned. Calling during sc_start() may crash.
// --------------------------------------------------------------------
inline void clear_watchers()
{
    std::lock_guard<std::mutex> g(detail::registry_mutex());
    detail::watcher_registry().clear();
}

} // namespace trace
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_TRACE_WATCH_HPP