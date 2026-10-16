// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include "ect/json_schema.hpp"

namespace {

using ect::json::field;
using ect::json::schema;

// The body of a "configure network" request.
schema network_request() {
    return schema()
        .add("hostname", field::string().required().check(ect::json::hostname, "must be a host name"))
        .add("dhcp", field::boolean().required())
        .add("ip", field::string().check(ect::json::ipv4, "must be an IPv4 address"))
        .add("mqtt", field::object(schema()
                                       .add("port", field::integer().required().range(1, 65535))
                                       .add("qos", field::integer().range(0, 2))
                                       .strict()))
        .add("relays", field::array().length(1, 8).each(field::object(
                           schema()
                               .add("pin", field::integer().required().range(0, 48))
                               .add("mode", field::string().required().one_of({"on", "off", "schedule"})))))
        .strict();
}

ect::json::result check(const char* body) {
    JsonDocument doc;
    EXPECT_EQ(deserializeJson(doc, body), DeserializationError::Ok) << body;
    return network_request().validate(doc.as<JsonVariantConst>());
}

TEST(JsonSchema, AcceptsAValidRequest) {
    const auto result = check(R"({
        "hostname": "relay-controller.local",
        "dhcp": false,
        "ip": "192.168.1.20",
        "mqtt": {"port": 8883, "qos": 1},
        "relays": [{"pin": 4, "mode": "on"}, {"pin": 5, "mode": "schedule"}]
    })");
    EXPECT_TRUE(result) << result.path << " " << result.reason;
}

TEST(JsonSchema, ReportsAMissingRequiredField) {
    const auto result = check(R"({"hostname": "a"})");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.path, "dhcp");
    EXPECT_EQ(result.reason, "is required");
}

TEST(JsonSchema, ReportsWrongTypes) {
    const auto result = check(R"({"hostname": "a", "dhcp": "yes"})");
    EXPECT_EQ(result.path, "dhcp");
    EXPECT_EQ(result.reason, "must be true or false");
}

TEST(JsonSchema, ChecksRangesInNestedObjects) {
    const auto result = check(R"({"hostname": "a", "dhcp": true, "mqtt": {"port": 70000}})");
    EXPECT_EQ(result.path, "mqtt.port");
    EXPECT_EQ(result.reason, "is out of range");
}

TEST(JsonSchema, ChecksEveryArrayItem) {
    const auto result = check(R"({"hostname": "a", "dhcp": true, "relays": [{"pin": 4, "mode": "on"}, {"pin": 5, "mode": "blink"}]})");
    EXPECT_EQ(result.path, "relays[1].mode");
    EXPECT_EQ(result.reason, "is not an allowed value");
}

TEST(JsonSchema, ChecksTheNumberOfArrayItems) {
    const auto result = check(R"({"hostname": "a", "dhcp": true, "relays": []})");
    EXPECT_EQ(result.path, "relays");
    EXPECT_EQ(result.reason, "has a wrong number of items");
}

TEST(JsonSchema, StrictSchemasRejectUnknownKeys) {
    const auto top = check(R"({"hostname": "a", "dhcp": true, "debug": true})");
    EXPECT_EQ(top.path, "debug");
    EXPECT_EQ(top.reason, "is not allowed");
    const auto nested = check(R"({"hostname": "a", "dhcp": true, "mqtt": {"port": 1883, "user": "x"}})");
    EXPECT_EQ(nested.path, "mqtt.user");
}

TEST(JsonSchema, RunsCustomChecks) {
    EXPECT_EQ(check(R"({"hostname": "a", "dhcp": false, "ip": "192.168.1.256"})").path, "ip");
    EXPECT_EQ(check(R"({"hostname": "-bad-", "dhcp": false})").reason, "must be a host name");
}

TEST(JsonSchema, RootMustBeAnObject) {
    const auto result = check("[1, 2, 3]");
    EXPECT_EQ(result.path, "(root)");
    EXPECT_EQ(result.reason, "must be an object");
}

TEST(JsonSchema, StringLengthAndIntegerType) {
    const schema login = schema()
                             .add("user", field::string().required().length(3, 16))
                             .add("attempts", field::integer());
    JsonDocument doc;
    deserializeJson(doc, R"({"user": "al", "attempts": 2.5})");
    EXPECT_EQ(login.validate(doc.as<JsonVariantConst>()).reason, "has a wrong length");
    deserializeJson(doc, R"({"user": "alice", "attempts": 2.5})");
    EXPECT_EQ(login.validate(doc.as<JsonVariantConst>()).reason, "must be an integer");
}

TEST(Predicates, Ipv4) {
    EXPECT_TRUE(ect::json::is_ipv4("0.0.0.0"));
    EXPECT_TRUE(ect::json::is_ipv4("255.255.255.255"));
    for (const char* bad : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "01.2.3.4", "1..2.3", "1.2.3.4 ", "a.b.c.d"}) {
        EXPECT_FALSE(ect::json::is_ipv4(bad)) << bad;
    }
}

TEST(Predicates, Hostname) {
    EXPECT_TRUE(ect::json::is_hostname("sensor-7"));
    EXPECT_TRUE(ect::json::is_hostname("relay.local"));
    for (const char* bad : {"", "-a", "a-", "a..b", "a.", "under_score", "space here"}) {
        EXPECT_FALSE(ect::json::is_hostname(bad)) << bad;
    }
}

}  // namespace
