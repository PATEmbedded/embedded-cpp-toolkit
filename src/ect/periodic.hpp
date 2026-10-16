// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Non-blocking "every N milliseconds" timer for super loops and tasks.

#pragma once

#include <cstdint>

namespace ect {

/// Tells a loop when a periodic job is due, without blocking.
///
/// Times are 32-bit millisecond counters such as millis() or
/// esp_timer_get_time() / 1000. The arithmetic is unsigned, so it stays
/// correct when the counter wraps around after about 49 days. When the loop
/// falls behind, missed periods are skipped instead of firing in a burst, and
/// the job keeps its original phase.
class periodic {
public:
    /// `interval_ms == 0` makes every call due.
    explicit constexpr periodic(std::uint32_t interval_ms, bool due_at_start = true) noexcept
        : interval_ms_(interval_ms), due_at_start_(due_at_start) {}

    /// True when the job should run now. Call it from the loop.
    constexpr bool due(std::uint32_t now_ms) noexcept {
        if (!started_) {
            started_ = true;
            last_ms_ = now_ms;
            return due_at_start_;
        }
        const std::uint32_t elapsed = now_ms - last_ms_;
        if (interval_ms_ == 0) {
            last_ms_ = now_ms;
            return true;
        }
        if (elapsed < interval_ms_) {
            return false;
        }
        last_ms_ += elapsed - elapsed % interval_ms_;
        return true;
    }

    /// Starts counting the interval again from `now_ms`.
    constexpr void restart(std::uint32_t now_ms) noexcept {
        started_ = true;
        last_ms_ = now_ms;
    }

    constexpr void set_interval(std::uint32_t interval_ms) noexcept {
        interval_ms_ = interval_ms;
    }

    [[nodiscard]] constexpr std::uint32_t interval() const noexcept {
        return interval_ms_;
    }

private:
    std::uint32_t interval_ms_;
    std::uint32_t last_ms_ = 0;
    bool due_at_start_;
    bool started_ = false;
};

}  // namespace ect
