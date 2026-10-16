// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Weekly time-window schedule, for example "lights on 18:00-23:00 on weekdays".

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace ect {

enum class weekday : std::uint8_t { monday, tuesday, wednesday, thursday, friday, saturday, sunday };

/// Converts struct tm::tm_wday (0 = Sunday) to a weekday.
[[nodiscard]] constexpr weekday weekday_from_tm(int tm_wday) noexcept {
    return static_cast<weekday>((tm_wday + 6) % 7);
}

/// Set of weekdays, one bit per day starting with Monday.
using day_set = std::uint8_t;

[[nodiscard]] constexpr day_set days(weekday day) noexcept {
    return static_cast<day_set>(1u << static_cast<unsigned>(day));
}

inline constexpr day_set every_day = 0x7F;
inline constexpr day_set weekdays = 0x1F;
inline constexpr day_set weekend = 0x60;

/// Time of day in seconds since midnight, from 00:00:00 to 24:00:00.
struct time_of_day {
    static constexpr std::uint32_t seconds_per_day = 24u * 60u * 60u;

    std::uint32_t seconds = 0;

    /// Builds a time from hours, minutes and seconds; returns nullopt when out of range.
    [[nodiscard]] static constexpr std::optional<time_of_day> hms(unsigned h, unsigned m, unsigned s = 0) noexcept {
        if (m > 59 || s > 59 || h > 24 || (h == 24 && (m != 0 || s != 0))) {
            return std::nullopt;
        }
        return time_of_day{h * 3600u + m * 60u + s};
    }

    /// Parses "HH:MM" or "HH:MM:SS"; "24:00" is accepted as the end of the day.
    [[nodiscard]] static constexpr std::optional<time_of_day> parse(std::string_view text) noexcept {
        if (text.size() != 5 && text.size() != 8) {
            return std::nullopt;
        }
        unsigned fields[3] = {0, 0, 0};
        for (std::size_t field = 0; field * 3 < text.size(); ++field) {
            const std::size_t at = field * 3;
            const char tens = text[at];
            const char ones = text[at + 1];
            if (tens < '0' || tens > '9' || ones < '0' || ones > '9') {
                return std::nullopt;
            }
            if (at + 2 < text.size() && text[at + 2] != ':') {
                return std::nullopt;
            }
            fields[field] = static_cast<unsigned>(tens - '0') * 10u + static_cast<unsigned>(ones - '0');
        }
        return hms(fields[0], fields[1], fields[2]);
    }

    friend constexpr bool operator==(time_of_day a, time_of_day b) noexcept {
        return a.seconds == b.seconds;
    }
    friend constexpr bool operator<(time_of_day a, time_of_day b) noexcept {
        return a.seconds < b.seconds;
    }
};

/// Maps every moment of the week to a value of type T.
///
/// Time windows are set per day; where windows overlap, the later call wins.
/// A window whose end is earlier than its start runs past midnight into the
/// next day. Times not covered by any window have the default value. Storage
/// is fixed: each day holds up to `MaxWindowsPerDay` windows, and set()
/// returns false, changing nothing, when a day would need more.
template <class T, std::size_t MaxWindowsPerDay = 8>
class weekly_schedule {
public:
    explicit weekly_schedule(T default_value) : default_value_(std::move(default_value)) {}

    /// Gives `value` to [start, end) on every day in `days_to_set`.
    bool set(day_set days_to_set, time_of_day start, time_of_day end, const T& value) {
        if (start.seconds > time_of_day::seconds_per_day || end.seconds > time_of_day::seconds_per_day) {
            return false;
        }
        if (start == end) {
            return true;  // an empty window changes nothing
        }
        std::array<day_plan, 7> next = plans_;
        for (unsigned d = 0; d < 7; ++d) {
            if ((days_to_set & (1u << d)) == 0) {
                continue;
            }
            if (start < end) {
                if (!next[d].assign(start.seconds, end.seconds, value, default_value_)) {
                    return false;
                }
            } else {  // runs past midnight
                const unsigned following = (d + 1) % 7;
                if (!next[d].assign(start.seconds, time_of_day::seconds_per_day, value, default_value_) ||
                    !next[following].assign(0, end.seconds, value, default_value_)) {
                    return false;
                }
            }
        }
        plans_ = next;
        return true;
    }

    /// The value at time `t` on `day`.
    [[nodiscard]] const T& value_at(weekday day, time_of_day t) const noexcept {
        const day_plan& plan = plans_[static_cast<unsigned>(day)];
        for (std::size_t i = 0; i < plan.count; ++i) {
            if (t.seconds >= plan.windows[i].start && t.seconds < plan.windows[i].end) {
                return plan.windows[i].value;
            }
        }
        return default_value_;
    }

    /// Number of windows stored for `day` after merging.
    [[nodiscard]] std::size_t window_count(weekday day) const noexcept {
        return plans_[static_cast<unsigned>(day)].count;
    }

    /// Removes every window.
    void clear() noexcept {
        for (day_plan& plan : plans_) {
            plan.count = 0;
        }
    }

private:
    struct window {
        std::uint32_t start;
        std::uint32_t end;
        T value;
    };

    struct day_plan {
        std::array<window, MaxWindowsPerDay> windows{};
        std::size_t count = 0;

        // Cuts [start, end) out of the existing windows, inserts the new
        // window unless it holds the default value, and merges neighbours
        // with equal values. Leaves the plan unchanged when it would not fit.
        bool assign(std::uint32_t start, std::uint32_t end, const T& value, const T& default_value) {
            if (start >= end) {
                return true;  // for example the part after midnight of a window ending at 00:00
            }
            std::array<window, MaxWindowsPerDay + 2> work{};
            std::size_t n = 0;
            for (std::size_t i = 0; i < count; ++i) {
                const window& w = windows[i];
                if (w.end <= start || w.start >= end) {
                    work[n++] = w;
                    continue;
                }
                if (w.start < start) {
                    work[n++] = window{w.start, start, w.value};
                }
                if (w.end > end) {
                    work[n++] = window{end, w.end, w.value};
                }
            }
            if (!(value == default_value)) {
                work[n++] = window{start, end, value};
            }
            std::sort(work.begin(), work.begin() + static_cast<std::ptrdiff_t>(n),
                      [](const window& a, const window& b) { return a.start < b.start; });

            std::size_t merged = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (merged > 0 && work[merged - 1].end == work[i].start && work[merged - 1].value == work[i].value) {
                    work[merged - 1].end = work[i].end;
                } else {
                    work[merged++] = work[i];
                }
            }
            if (merged > MaxWindowsPerDay) {
                return false;
            }
            std::copy(work.begin(), work.begin() + static_cast<std::ptrdiff_t>(merged), windows.begin());
            count = merged;
            return true;
        }
    };

    T default_value_;
    std::array<day_plan, 7> plans_{};
};

}  // namespace ect
