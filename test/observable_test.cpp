// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ect/observable.hpp"

namespace {

TEST(Observable, NotifiesWithOldAndNewValue) {
    ect::observable<int> temperature(20);
    std::vector<std::pair<int, int>> changes;
    ASSERT_TRUE(temperature.subscribe([&](int old_value, int new_value) { changes.emplace_back(old_value, new_value); }));

    temperature = 21;
    temperature = 21;  // same value: no notification
    temperature += 4;

    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0], std::make_pair(20, 21));
    EXPECT_EQ(changes[1], std::make_pair(21, 25));
    EXPECT_EQ(temperature.get(), 25);
}

TEST(Observable, NotifiesEverySubscriber) {
    ect::observable<std::string, 2> mode("idle");
    int calls = 0;
    EXPECT_TRUE(mode.subscribe([&](const std::string&, const std::string&) { ++calls; }));
    EXPECT_TRUE(mode.subscribe([&](const std::string&, const std::string&) { ++calls; }));
    EXPECT_FALSE(mode.subscribe([&](const std::string&, const std::string&) { ++calls; }));  // no free slot
    EXPECT_TRUE(mode.set("running"));
    EXPECT_EQ(calls, 2);
}

TEST(Observable, CallbackMayWriteTheValueItself) {
    // The callback runs after the lock is released, so this does not deadlock.
    ect::observable<int, 1, std::mutex> level(0);
    level.subscribe([&level](int, int new_value) {
        if (new_value > 10) {
            level = 10;  // clamp
        }
    });
    level = 50;
    EXPECT_EQ(level.get(), 10);
}

TEST(Observable, UpdateLosesNoIncrementAcrossThreads) {
    ect::observable<int, 1, std::mutex> counter(0);
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&counter] {
            for (int i = 0; i < 1000; ++i) {
                ++counter;
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(counter.get(), 4000);
}

TEST(Observable, UpdateReportsWhetherTheValueChanged) {
    ect::observable<int> value(3);
    EXPECT_FALSE(value.update([](int v) { return v; }));
    EXPECT_TRUE(value.update([](int v) { return v * 2; }));
    EXPECT_EQ(static_cast<int>(value), 6);
}

}  // namespace
