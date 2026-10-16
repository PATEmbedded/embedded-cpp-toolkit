// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Time-based debouncing for buttons, switches and other noisy digital inputs.

#pragma once

#include <cstdint>

namespace ect {

/// Accepts a new input level only after it has been stable for a set time.
///
/// Feed it raw samples from a loop or a timer, not from the interrupt that
/// detects the edge: contact bounce produces many edges, and the debouncer
/// needs to see the level settle.
class debouncer {
public:
    explicit constexpr debouncer(std::uint32_t stable_ms, bool initial_level = false) noexcept
        : stable_ms_(stable_ms), state_(initial_level), candidate_(initial_level) {}

    /// Feeds one raw sample. Returns true when the debounced level changed.
    constexpr bool update(bool raw_level, std::uint32_t now_ms) noexcept {
        rose_ = false;
        fell_ = false;
        if (raw_level != candidate_) {
            candidate_ = raw_level;
            since_ms_ = now_ms;
        }
        if (candidate_ == state_ || now_ms - since_ms_ < stable_ms_) {
            return false;
        }
        state_ = candidate_;
        rose_ = state_;
        fell_ = !state_;
        return true;
    }

    /// The debounced level.
    [[nodiscard]] constexpr bool level() const noexcept {
        return state_;
    }

    /// True right after the update() that saw the level go from low to high.
    [[nodiscard]] constexpr bool rose() const noexcept {
        return rose_;
    }

    /// True right after the update() that saw the level go from high to low.
    [[nodiscard]] constexpr bool fell() const noexcept {
        return fell_;
    }

private:
    std::uint32_t stable_ms_;
    std::uint32_t since_ms_ = 0;
    bool state_;
    bool candidate_;
    bool rose_ = false;
    bool fell_ = false;
};

}  // namespace ect
