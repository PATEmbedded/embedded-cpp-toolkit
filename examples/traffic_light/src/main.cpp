// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Pedestrian crossing on an ESP32 with three LEDs and one button. Nothing in
// loop() blocks: the state machine, the debouncer and the status timer all
// work from millis().

#include <Arduino.h>
#include <embedded_cpp_toolkit.h>

namespace {

constexpr uint8_t kRedPin = 25;
constexpr uint8_t kYellowPin = 26;
constexpr uint8_t kGreenPin = 27;
constexpr uint8_t kButtonPin = 0;  // the BOOT button on most ESP32 boards, low when pressed
constexpr uint32_t kMinimumGreenMs = 5000;

enum class light { red, green, yellow };
enum class event { timeout, button };

struct crossing {
    uint32_t green_since_ms = 0;
};

void show(bool red, bool yellow, bool green) {
    digitalWrite(kRedPin, red ? HIGH : LOW);
    digitalWrite(kYellowPin, yellow ? HIGH : LOW);
    digitalWrite(kGreenPin, green ? HIGH : LOW);
}

void enter_red(crossing&) {
    show(true, false, false);
    Serial.println("red: pedestrians may cross");
}

void enter_green(crossing& c) {
    c.green_since_ms = millis();
    show(false, false, true);
    Serial.println("green");
}

void enter_yellow(crossing&) {
    show(false, true, false);
    Serial.println("yellow");
}

// The button shortens green, but cars always get at least five seconds.
bool green_long_enough(const crossing& c) {
    return millis() - c.green_since_ms >= kMinimumGreenMs;
}

using machine = ect::state_machine<light, event, crossing>;

constexpr machine::state_type kStates[] = {
    {light::red, 8000, enter_red, nullptr},
    {light::green, 10000, enter_green, nullptr},
    {light::yellow, 3000, enter_yellow, nullptr},
};

constexpr machine::transition_type kTransitions[] = {
    {light::red, event::timeout, light::green, nullptr, nullptr},
    {light::green, event::timeout, light::yellow, nullptr, nullptr},
    {light::green, event::button, light::yellow, green_long_enough, nullptr},
    {light::yellow, event::timeout, light::red, nullptr, nullptr},
};

crossing context;
machine traffic_light(kStates, kTransitions, context, event::timeout);
ect::debouncer button(30, /*initial_level=*/true);
ect::periodic status_report(5000, /*due_at_start=*/false);

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(kRedPin, OUTPUT);
    pinMode(kYellowPin, OUTPUT);
    pinMode(kGreenPin, OUTPUT);
    pinMode(kButtonPin, INPUT_PULLUP);
    traffic_light.start(light::red, millis());
}

void loop() {
    const uint32_t now = millis();

    if (button.update(digitalRead(kButtonPin) == HIGH, now) && button.fell()) {
        if (!traffic_light.dispatch(event::button, now)) {
            Serial.println("button: request noted, the light changes after its normal time");
        }
    }

    traffic_light.update(now);

    if (status_report.due(now)) {
        Serial.printf("%u ms in the current state\n", static_cast<unsigned>(traffic_light.time_in_state(now)));
    }
}
