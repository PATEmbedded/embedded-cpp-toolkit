// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include "ect/weekly_schedule.hpp"

namespace {

using ect::time_of_day;
using ect::weekday;

time_of_day at(const char* text) {
    return ect::time_of_day::parse(text).value();
}

TEST(TimeOfDay, ParsesValidTimes) {
    EXPECT_EQ(at("00:00").seconds, 0u);
    EXPECT_EQ(at("07:30").seconds, 7u * 3600u + 30u * 60u);
    EXPECT_EQ(at("23:59:59").seconds, 86399u);
    EXPECT_EQ(at("24:00").seconds, 86400u);
}

TEST(TimeOfDay, RejectsInvalidTimes) {
    for (const char* text : {"", "7:30", "07:60", "24:01", "25:00", "07-30", "07:30:", "07:30:60", "ab:cd"}) {
        EXPECT_FALSE(time_of_day::parse(text).has_value()) << text;
    }
}

TEST(WeekdayFromTm, MapsSundayFirstToMondayFirst) {
    EXPECT_EQ(ect::weekday_from_tm(0), weekday::sunday);
    EXPECT_EQ(ect::weekday_from_tm(1), weekday::monday);
    EXPECT_EQ(ect::weekday_from_tm(6), weekday::saturday);
}

TEST(WeeklySchedule, ReturnsTheDefaultOutsideWindows) {
    ect::weekly_schedule<int> fan(0);
    EXPECT_TRUE(fan.set(ect::weekdays, at("08:00"), at("12:00"), 3));
    EXPECT_EQ(fan.value_at(weekday::monday, at("07:59:59")), 0);
    EXPECT_EQ(fan.value_at(weekday::monday, at("08:00")), 3);
    EXPECT_EQ(fan.value_at(weekday::monday, at("11:59:59")), 3);
    EXPECT_EQ(fan.value_at(weekday::monday, at("12:00")), 0);  // the end is exclusive
    EXPECT_EQ(fan.value_at(weekday::saturday, at("09:00")), 0);
}

TEST(WeeklySchedule, LaterWindowsOverrideEarlierOnes) {
    ect::weekly_schedule<int> heater(16);
    heater.set(ect::every_day, at("06:00"), at("22:00"), 21);
    heater.set(ect::days(weekday::sunday), at("10:00"), at("12:00"), 23);
    EXPECT_EQ(heater.value_at(weekday::sunday, at("09:00")), 21);
    EXPECT_EQ(heater.value_at(weekday::sunday, at("11:00")), 23);
    EXPECT_EQ(heater.value_at(weekday::sunday, at("13:00")), 21);
    EXPECT_EQ(heater.window_count(weekday::sunday), 3u);
    EXPECT_EQ(heater.window_count(weekday::monday), 1u);
}

TEST(WeeklySchedule, WindowsPastMidnightContinueTheNextDay) {
    ect::weekly_schedule<bool> lights(false);
    lights.set(ect::days(weekday::sunday), at("22:00"), at("02:00"), true);
    EXPECT_TRUE(lights.value_at(weekday::sunday, at("23:00")));
    EXPECT_TRUE(lights.value_at(weekday::monday, at("01:59")));  // Sunday night wraps into Monday
    EXPECT_FALSE(lights.value_at(weekday::monday, at("02:00")));
}

TEST(WeeklySchedule, MergesNeighboursWithTheSameValue) {
    ect::weekly_schedule<int> fan(0);
    fan.set(ect::days(weekday::monday), at("08:00"), at("10:00"), 2);
    fan.set(ect::days(weekday::monday), at("10:00"), at("12:00"), 2);
    EXPECT_EQ(fan.window_count(weekday::monday), 1u);
}

TEST(WeeklySchedule, SettingTheDefaultValueClearsAWindow) {
    ect::weekly_schedule<int> fan(0);
    fan.set(ect::days(weekday::monday), at("08:00"), at("12:00"), 2);
    fan.set(ect::days(weekday::monday), at("09:00"), at("10:00"), 0);
    EXPECT_EQ(fan.value_at(weekday::monday, at("09:30")), 0);
    EXPECT_EQ(fan.window_count(weekday::monday), 2u);
}

TEST(WeeklySchedule, RefusesWindowsThatDoNotFitAndKeepsTheOldPlan) {
    ect::weekly_schedule<int, 2> relay(0);
    EXPECT_TRUE(relay.set(ect::days(weekday::monday), at("01:00"), at("02:00"), 1));
    EXPECT_TRUE(relay.set(ect::days(weekday::monday), at("03:00"), at("04:00"), 1));
    EXPECT_FALSE(relay.set(ect::days(weekday::monday), at("05:00"), at("06:00"), 1));
    EXPECT_EQ(relay.window_count(weekday::monday), 2u);
    EXPECT_EQ(relay.value_at(weekday::monday, at("05:30")), 0);
}

TEST(WeeklySchedule, ClearRemovesEverything) {
    ect::weekly_schedule<int> fan(0);
    fan.set(ect::every_day, at("08:00"), at("12:00"), 1);
    fan.clear();
    EXPECT_EQ(fan.value_at(weekday::friday, at("09:00")), 0);
}

}  // namespace
