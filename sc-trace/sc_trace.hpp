#ifndef CC_VRWRAPPER_SC_TRACE_HPP
#define CC_VRWRAPPER_SC_TRACE_HPP

// ============================================================================
// cc_vrwrapper :: sc-trace
// Signal watching, callback triggers, and enhanced tracing for SystemC.
//
// Features:
//   - watch(sig, cb)                   — callback on any value change
//   - watch_rising(sig, cb)            — callback on rising edge
//   - watch_falling(sig, cb)           — callback on falling edge
//   - watch_value(sig, val, cb)        — callback when signal equals val
//   - watch_transition(sig, from, to, cb) — callback on specific transition
//   - TraceManager                     — VCD tracing with regex filtering
//   - JsonTrace                        — JSON waveform for browser viewing
//
// Supported signal types: sc_signal<T>, sc_in<T>, sc_out<T>
//   where T = bool, sc_logic, sc_lv<W>, sc_bv<W>,
//            sc_int<W>, sc_uint<W>, sc_bigint<W>, sc_biguint<W>
//
// IMPORTANT: All watch() and trace() calls MUST be made BEFORE sc_start().
// SystemC does not allow creating modules or registering processes
// after simulation starts.
//
// Namespace: cc_vrwrapper::trace
//
// Supports BOTH C++17 and C++20 in the same files.
// ============================================================================

#include "utils.hpp"
#include "watch.hpp"
#include "trace.hpp"
#include "json_trace.hpp"

#endif // CC_VRWRAPPER_SC_TRACE_HPP