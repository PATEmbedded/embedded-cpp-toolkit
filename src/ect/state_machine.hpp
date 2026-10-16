// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Table-driven, non-blocking finite state machine with per-state timeouts.

#pragma once

#include <cstddef>
#include <cstdint>

namespace ect {

/// One state: its timeout and the handlers that run on entry and exit.
template <class State, class Context>
struct state_info {
    State id;
    std::uint32_t timeout_ms;      ///< 0 means the state has no timeout.
    void (*on_enter)(Context&);    ///< May be nullptr.
    void (*on_exit)(Context&);     ///< May be nullptr.
};

/// One transition: in state `from`, event `event` leads to state `to`.
template <class State, class Event, class Context>
struct transition {
    State from;
    Event event;
    State to;
    bool (*guard)(const Context&); ///< nullptr, or false to ignore the event.
    void (*action)(Context&);      ///< Runs between the exit and entry handlers. May be nullptr.
};

/// Finite state machine driven by two tables that can live in flash.
///
/// Nothing blocks: dispatch() handles an event and returns, and update()
/// turns an expired state timeout into `timeout_event`. Both take the current
/// time, so the machine works with millis(), a FreeRTOS tick count or a fake
/// clock in unit tests, and stays correct when the 32-bit counter wraps.
/// Transitions are checked in table order; the first match whose guard
/// passes fires. Handlers must not call dispatch() themselves.
template <class State, class Event, class Context>
class state_machine {
public:
    using state_type = state_info<State, Context>;
    using transition_type = transition<State, Event, Context>;

    template <std::size_t StateCount, std::size_t TransitionCount>
    constexpr state_machine(const state_type (&states)[StateCount],
                            const transition_type (&transitions)[TransitionCount], Context& context,
                            Event timeout_event) noexcept
        : states_(states),
          state_count_(StateCount),
          transitions_(transitions),
          transition_count_(TransitionCount),
          context_(context),
          timeout_event_(timeout_event) {}

    /// Enters `initial` and starts its timeout.
    void start(State initial, std::uint32_t now_ms) {
        current_ = initial;
        previous_ = initial;
        entered_ms_ = now_ms;
        started_ = true;
        run(find(current_), &state_type::on_enter);
    }

    /// Handles an event. Returns true when a transition fired.
    bool dispatch(Event event, std::uint32_t now_ms) {
        if (!started_) {
            return false;
        }
        for (std::size_t i = 0; i < transition_count_; ++i) {
            const transition_type& t = transitions_[i];
            if (t.from == current_ && t.event == event && (t.guard == nullptr || t.guard(context_))) {
                fire(t, now_ms);
                return true;
            }
        }
        return false;
    }

    /// Dispatches `timeout_event` once the current state's timeout has
    /// elapsed. Call it regularly, for example from the main loop.
    bool update(std::uint32_t now_ms) {
        if (!started_) {
            return false;
        }
        const state_type* state = find(current_);
        if (state == nullptr || state->timeout_ms == 0 || now_ms - entered_ms_ < state->timeout_ms) {
            return false;
        }
        return dispatch(timeout_event_, now_ms);
    }

    [[nodiscard]] State current() const noexcept {
        return current_;
    }

    /// The state before the last transition (the initial state before any).
    [[nodiscard]] State previous() const noexcept {
        return previous_;
    }

    [[nodiscard]] std::uint32_t time_in_state(std::uint32_t now_ms) const noexcept {
        return now_ms - entered_ms_;
    }

    [[nodiscard]] bool started() const noexcept {
        return started_;
    }

private:
    const state_type* find(State id) const noexcept {
        for (std::size_t i = 0; i < state_count_; ++i) {
            if (states_[i].id == id) {
                return &states_[i];
            }
        }
        return nullptr;
    }

    void run(const state_type* state, void (*state_type::*handler)(Context&)) {
        if (state != nullptr && state->*handler != nullptr) {
            (state->*handler)(context_);
        }
    }

    void fire(const transition_type& t, std::uint32_t now_ms) {
        run(find(current_), &state_type::on_exit);
        if (t.action != nullptr) {
            t.action(context_);
        }
        previous_ = current_;
        current_ = t.to;
        entered_ms_ = now_ms;
        run(find(current_), &state_type::on_enter);
    }

    const state_type* states_;
    std::size_t state_count_;
    const transition_type* transitions_;
    std::size_t transition_count_;
    Context& context_;
    Event timeout_event_;
    State current_{};
    State previous_{};
    std::uint32_t entered_ms_ = 0;
    bool started_ = false;
};

}  // namespace ect
