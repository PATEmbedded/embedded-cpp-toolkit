// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include <cstdint>

#include "ect/debouncer.hpp"
#include "ect/periodic.hpp"

namespace {

TEST(Periodic, IsDueAtStartThenOncePerInterval) {
    ect::periodic timer(100);
    EXPECT_TRUE(timer.due(1000));
    EXPECT_FALSE(timer.due(1050));
    EXPECT_TRUE(timer.due(1100));
    EXPECT_FALSE(timer.due(1150));
}

TEST(Periodic, CanWaitForTheFirstInterval) {
    ect::periodic timer(100, /*due_at_start=*/false);
    EXPECT_FALSE(timer.due(0));
    EXPECT_FALSE(timer.due(99));
    EXPECT_TRUE(timer.due(100));
}

TEST(Periodic, SkipsMissedPeriodsAndKeepsPhase) {
    ect::periodic timer(100);
    EXPECT_TRUE(timer.due(0));
    EXPECT_TRUE(timer.due(350));   // three periods late: fires once
    EXPECT_FALSE(timer.due(399));  // phase stays at multiples of 100
    EXPECT_TRUE(timer.due(400));
}

TEST(Periodic, SurvivesCounterWrapAround) {
    ect::periodic timer(100);
    const std::uint32_t near_max = UINT32_MAX - 49;
    EXPECT_TRUE(timer.due(near_max));
    EXPECT_FALSE(timer.due(near_max + 99));  // wraps to 49
    EXPECT_TRUE(timer.due(near_max + 100));  // wraps to 50
}

TEST(Periodic, ZeroIntervalIsAlwaysDue) {
    ect::periodic timer(0);
    EXPECT_TRUE(timer.due(5));
    EXPECT_TRUE(timer.due(5));
}

TEST(Debouncer, IgnoresBounceShorterThanTheStableTime) {
    ect::debouncer button(20);
    EXPECT_FALSE(button.update(true, 0));
    EXPECT_FALSE(button.update(false, 5));  // bounce
    EXPECT_FALSE(button.update(true, 8));
    EXPECT_FALSE(button.update(true, 27));  // stable for 19 ms only
    EXPECT_TRUE(button.update(true, 28));   // stable for 20 ms
    EXPECT_TRUE(button.level());
    EXPECT_TRUE(button.rose());
    EXPECT_FALSE(button.fell());
}

TEST(Debouncer, ReportsTheFallingEdgeOnce) {
    ect::debouncer button(10, /*initial_level=*/true);
    EXPECT_FALSE(button.update(false, 100));
    EXPECT_TRUE(button.update(false, 110));
    EXPECT_TRUE(button.fell());
    EXPECT_FALSE(button.update(false, 120));
    EXPECT_FALSE(button.fell());
    EXPECT_FALSE(button.level());
}

}  // namespace
