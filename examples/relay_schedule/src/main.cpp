// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Validates a relay schedule that arrives as JSON, for example in the body of
// a REST request, and drives the relays from it. A simulated clock runs
// through one week in about two minutes, so the example needs no RTC or
// network time.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <embedded_cpp_toolkit.h>

#include <vector>

namespace {

const char* const kRequestBody = R"({
  "relays": [
    {"pin": 4, "days": "weekdays", "on": "07:30", "off": "18:00"},
    {"pin": 5, "days": "every_day", "on": "22:00", "off": "06:00"}
  ]
})";

bool is_time(JsonVariantConst value) {
    return value.is<const char*>() && ect::time_of_day::parse(value.as<const char*>()).has_value();
}

ect::json::schema request_schema() {
    using ect::json::field;
    using ect::json::schema;
    const schema relay = schema()
                             .add("pin", field::integer().required().range(0, 39))
                             .add("days", field::string().required().one_of({"weekdays", "weekend", "every_day"}))
                             .add("on", field::string().required().check(is_time, "must be HH:MM"))
                             .add("off", field::string().required().check(is_time, "must be HH:MM"))
                             .strict();
    return schema().add("relays", field::array().required().length(1, 4).each(field::object(relay))).strict();
}

ect::day_set parse_days(const char* name) {
    const String days(name);
    if (days == "weekdays") {
        return ect::weekdays;
    }
    if (days == "weekend") {
        return ect::weekend;
    }
    return ect::every_day;
}

struct relay {
    uint8_t pin;
    ect::weekly_schedule<bool> plan{false};
};

std::vector<relay> relays;
ect::periodic tick(100);
uint32_t simulated_minute = 0;  // minutes since Monday 00:00

}  // namespace

void setup() {
    Serial.begin(115200);

    JsonDocument request;
    if (deserializeJson(request, kRequestBody) != DeserializationError::Ok) {
        Serial.println("body is not JSON");
        return;
    }
    const ect::json::result check = request_schema().validate(request.as<JsonVariantConst>());
    if (!check) {
        // A REST handler would answer 400 Bad Request with this text.
        Serial.printf("rejected: %s %s\n", check.path.c_str(), check.reason.c_str());
        return;
    }

    for (JsonObjectConst item : request["relays"].as<JsonArrayConst>()) {
        relay r{static_cast<uint8_t>(item["pin"].as<int>())};
        r.plan.set(parse_days(item["days"].as<const char*>()), *ect::time_of_day::parse(item["on"].as<const char*>()),
                   *ect::time_of_day::parse(item["off"].as<const char*>()), true);
        pinMode(r.pin, OUTPUT);
        relays.push_back(r);
    }
    Serial.printf("schedule accepted for %u relays\n", static_cast<unsigned>(relays.size()));
}

void loop() {
    if (!tick.due(millis())) {
        return;
    }
    // Every 100 ms of real time is 10 simulated minutes.
    simulated_minute = (simulated_minute + 10) % (7 * 24 * 60);
    const auto day = static_cast<ect::weekday>(simulated_minute / (24 * 60));
    const ect::time_of_day now{(simulated_minute % (24 * 60)) * 60};

    for (const relay& r : relays) {
        const bool on = r.plan.value_at(day, now);
        if (digitalRead(r.pin) != (on ? HIGH : LOW)) {
            digitalWrite(r.pin, on ? HIGH : LOW);
            Serial.printf("day %u %02u:%02u relay on pin %u %s\n", static_cast<unsigned>(day) + 1,
                          static_cast<unsigned>(now.seconds / 3600), static_cast<unsigned>(now.seconds / 60 % 60),
                          r.pin, on ? "on" : "off");
        }
    }
}
