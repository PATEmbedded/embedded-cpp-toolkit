// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "ect/state_machine.hpp"

namespace {

// A pedestrian crossing: red 8 s, green 10 s, yellow 3 s. The button
// shortens green, but only after the cars have had green for 5 s.
enum class light { red, green, yellow };
enum class event { timeout, button };

struct crossing {
    std::uint32_t green_time_ms = 0;
    std::string log;
};

void enter_red(crossing& c) { c.log += "R"; }
void enter_green(crossing& c) { c.log += "G"; }
void enter_yellow(crossing& c) { c.log += "Y"; }
bool green_long_enough(const crossing& c) { return c.green_time_ms >= 5000; }

using machine = ect::state_machine<light, event, crossing>;

constexpr machine::state_type states[] = {
    {light::red, 8000, enter_red, nullptr},
    {light::green, 10000, enter_green, nullptr},
    {light::yellow, 3000, enter_yellow, nullptr},
};

constexpr machine::transition_type transitions[] = {
    {light::red, event::timeout, light::green, nullptr, nullptr},
    {light::green, event::timeout, light::yellow, nullptr, nullptr},
    {light::green, event::button, light::yellow, green_long_enough, nullptr},
    {light::yellow, event::timeout, light::red, nullptr, nullptr},
};

class StateMachineTest : public ::testing::Test {
protected:
    crossing context;
    machine fsm{states, transitions, context, event::timeout};
};

TEST_F(StateMachineTest, CyclesThroughTheLightsOnTimeouts) {
    fsm.start(light::red, 0);
    EXPECT_EQ(context.log, "R");
    EXPECT_FALSE(fsm.update(7999));
    EXPECT_TRUE(fsm.update(8000));
    EXPECT_EQ(fsm.current(), light::green);
    EXPECT_TRUE(fsm.update(18000));
    EXPECT_EQ(fsm.current(), light::yellow);
    EXPECT_EQ(fsm.previous(), light::green);
    EXPECT_TRUE(fsm.update(21000));
    EXPECT_EQ(fsm.current(), light::red);
    EXPECT_EQ(context.log, "RGYR");
}

TEST_F(StateMachineTest, GuardDecidesWhetherTheButtonCounts) {
    fsm.start(light::green, 0);
    context.green_time_ms = fsm.time_in_state(2000);
    EXPECT_FALSE(fsm.dispatch(event::button, 2000));  // cars had green for 2 s only
    context.green_time_ms = fsm.time_in_state(6000);
    EXPECT_TRUE(fsm.dispatch(event::button, 6000));
    EXPECT_EQ(fsm.current(), light::yellow);
}

TEST_F(StateMachineTest, IgnoresEventsWithoutATransition) {
    fsm.start(light::red, 0);
    EXPECT_FALSE(fsm.dispatch(event::button, 100));
    EXPECT_EQ(fsm.current(), light::red);
}

TEST_F(StateMachineTest, TimeoutCountsFromTheLastTransition) {
    fsm.start(light::red, 0);
    EXPECT_TRUE(fsm.update(8000));  // green entered at 8000
    EXPECT_FALSE(fsm.update(17999));
    EXPECT_EQ(fsm.time_in_state(17999), 9999u);
}

TEST_F(StateMachineTest, WorksAcrossCounterWrapAround) {
    const std::uint32_t start = UINT32_MAX - 1000;
    fsm.start(light::red, start);
    EXPECT_FALSE(fsm.update(start + 7999));
    EXPECT_TRUE(fsm.update(start + 8000));  // the counter has wrapped
}

TEST_F(StateMachineTest, DoesNothingBeforeStart) {
    EXPECT_FALSE(fsm.started());
    EXPECT_FALSE(fsm.dispatch(event::timeout, 0));
    EXPECT_FALSE(fsm.update(100000));
}

}  // namespace
