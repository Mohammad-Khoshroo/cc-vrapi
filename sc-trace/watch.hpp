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
// IMPORTANT: All watch() calls MUST be made BEFORE sc_start().
// SystemC does not allow creating modules (which watchers are) after
// simulation starts.
// ============================================================================

#include "utils.hpp"
#include <systemc>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace cc_vrwrapper {
namespace trace {

// ========================================================================
// WatchMode — determines when a watcher fires
// ========================================================================

enum class WatchMode {
    ANY_CHANGE,     // any value change
    RISING_EDGE,    // 0→1 (bool), 0→1/X (sc_logic), 0→non-zero (integral)
    FALLING_EDGE,   // 1→0 (bool), 1→0/X (sc_logic), non-zero→0 (integral)
    VALUE_MATCH,    // signal equals a specific value
    TRANSITION      // specific from→to transition
};

// ========================================================================
// SignalWatcher — internal SC_MODULE that watches a signal
// ========================================================================

template <typename Signal>
class SignalWatcher : public sc_core::sc_module {
public:
    using value_type = signal_value_type_t<Signal>;

    // Required for SC_METHOD in templated modules
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
        // Don't fire at time 0 — only on actual events
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

    // Edge detection — works for bool, sc_logic, and integral types
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
    static int counter = 0;
    return counter++;
}

} // namespace detail

// ========================================================================
// PUBLIC API
// ========================================================================

// --------------------------------------------------------------------
// watch() — call callback on ANY value change
//
// Example:
//   trace::watch(data, [](const sc_lv<8>& val) {
//       std::cout << "data changed to: " << val << "\n";
//   });
// --------------------------------------------------------------------
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
        std::function<void(const value_type&)>(cb),
        WatchMode::ANY_CHANGE
    );
    detail::watcher_registry().push_back(
        std::shared_ptr<sc_core::sc_module>(
            watcher, watcher.get()
        )
    );
}

// --------------------------------------------------------------------
// watch_rising() — call callback on rising edge
//
// For bool:       0 → 1
// For sc_logic:   0 → 1
// For integral:   0 → non-zero
//
// Example:
//   trace::watch_rising(clk, []() {
//       std::cout << "Clock rising edge at " << sc_time_stamp() << "\n";
//   });
// --------------------------------------------------------------------
template <typename Signal, typename Callback>
void watch_rising(Signal& sig, Callback cb)
{
    static_assert(is_sc_signal_or_port_v<Signal>,
        "watch_rising(): Signal must be sc_signal<T>, sc_in<T>, or sc_out<T>");

    using value_type = signal_value_type_t<Signal>;

    std::lock_guard<std::mutex> g(detail::registry_mutex());
    int id = detail::next_watcher_id();
    std::string name = "vrwatcher_rise_" + std::to_string(id);

    auto watcher = std::make_shared<SignalWatcher<Signal>>(
        name.c_str(), sig,
        std::function<void(const value_type&)>(cb),
        WatchMode::RISING_EDGE
    );
    detail::watcher_registry().push_back(
        std::shared_ptr<sc_core::sc_module>(
            watcher, watcher.get()
        )
    );
}

// --------------------------------------------------------------------
// watch_falling() — call callback on falling edge
//
// For bool:       1 → 0
// For sc_logic:   1 → 0
// For integral:   non-zero → 0
// --------------------------------------------------------------------
template <typename Signal, typename Callback>
void watch_falling(Signal& sig, Callback cb)
{
    static_assert(is_sc_signal_or_port_v<Signal>,
        "watch_falling(): Signal must be sc_signal<T>, sc_in<T>, or sc_out<T>");

    using value_type = signal_value_type_t<Signal>;

    std::lock_guard<std::mutex> g(detail::registry_mutex());
    int id = detail::next_watcher_id();
    std::string name = "vrwatcher_fall_" + std::to_string(id);

    auto watcher = std::make_shared<SignalWatcher<Signal>>(
        name.c_str(), sig,
        std::function<void(const value_type&)>(cb),
        WatchMode::FALLING_EDGE
    );
    detail::watcher_registry().push_back(
        std::shared_ptr<sc_core::sc_module>(
            watcher, watcher.get()
        )
    );
}

// --------------------------------------------------------------------
// watch_value() — call callback when signal equals a specific value
//
// Example:
//   trace::watch_value(data, 42, []() {
//       std::cout << "Data reached 42!\n";
//   });
//
//   trace::watch_value(status, sc_lv<4>("0b1010"), [](const sc_lv<4>& v) {
//       std::cout << "Status is 0xA\n";
//   });
// --------------------------------------------------------------------
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
        std::function<void(const sig_value_type&)>(cb),
        WatchMode::VALUE_MATCH,
        static_cast<sig_value_type>(value)
    );
    detail::watcher_registry().push_back(
        std::shared_ptr<sc_core::sc_module>(
            watcher, watcher.get()
        )
    );
}

// --------------------------------------------------------------------
// watch_transition() — call callback on specific from→to transition
//
// Example:
//   trace::watch_transition(state, 0, 1, []() {
//       std::cout << "State went from 0 to 1\n";
//   });
// --------------------------------------------------------------------
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
        std::function<void(const sig_value_type&)>(cb),
        WatchMode::TRANSITION,
        static_cast<sig_value_type>(to),     // match_value = "to"
        static_cast<sig_value_type>(from)    // from_value
    );
    detail::watcher_registry().push_back(
        std::shared_ptr<sc_core::sc_module>(
            watcher, watcher.get()
        )
    );
}

// --------------------------------------------------------------------
// clear_watchers() — remove all watchers (for cleanup / reconfiguration)
//
// NOTE: This only removes the shared_ptr references. SystemC's internal
// module registry still holds references. Watchers will not fire again
// after this call (their callbacks are destroyed), but the modules
// themselves persist until simulation ends.
// --------------------------------------------------------------------
inline void clear_watchers()
{
    std::lock_guard<std::mutex> g(detail::registry_mutex());
    detail::watcher_registry().clear();
}

} // namespace trace
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_TRACE_WATCH_HPP