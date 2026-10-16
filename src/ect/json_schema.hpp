// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Declarative validation of JSON request bodies and configuration files
// parsed with ArduinoJson 7.

#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ect::json {

/// Outcome of a validation. On failure `path` names the first offending
/// value, for example "network.ip" or "relays[2].pin".
struct result {
    bool ok = true;
    std::string path;
    std::string reason;

    explicit operator bool() const noexcept {
        return ok;
    }
};

class schema;

/// Rules for one JSON value. Build it with the static functions and chain
/// the constraints: `field::string().required().length(8, 64)`.
class field {
public:
    using predicate = bool (*)(JsonVariantConst value);

    static field boolean() { return field(kind::boolean); }
    static field integer() { return field(kind::integer); }
    static field number() { return field(kind::number); }
    static field string() { return field(kind::string); }
    static field array() { return field(kind::array); }
    static field any() { return field(kind::any); }

    /// A nested object checked against `properties`.
    static field object(schema properties);

    field& required() {
        required_ = true;
        return *this;
    }

    /// Inclusive range for integers and numbers.
    field& range(double min, double max) {
        has_range_ = true;
        min_ = min;
        max_ = max;
        return *this;
    }

    /// Inclusive length for strings, or item count for arrays.
    field& length(std::size_t min, std::size_t max) {
        has_length_ = true;
        min_length_ = min;
        max_length_ = max;
        return *this;
    }

    /// Rules every array item must follow.
    field& each(field item_rules) {
        items_ = std::make_shared<const field>(std::move(item_rules));
        return *this;
    }

    /// Allowed values of a string.
    field& one_of(std::initializer_list<const char*> options) {
        options_.assign(options.begin(), options.end());
        return *this;
    }

    /// A custom check, for example ect::json::ipv4, with the message used when it fails.
    field& check(predicate test, const char* reason) {
        check_ = test;
        check_reason_ = reason;
        return *this;
    }

    [[nodiscard]] bool is_required() const noexcept {
        return required_;
    }

    /// Checks `value`; on failure fills `out` and returns false.
    bool validate(JsonVariantConst value, const std::string& path, result& out) const;

private:
    enum class kind : std::uint8_t { any, boolean, integer, number, string, object, array };

    explicit field(kind k) : kind_(k) {}

    static bool fail(result& out, const std::string& path, const char* reason) {
        out.ok = false;
        out.path = path.empty() ? std::string("(root)") : path;
        out.reason = reason;
        return false;
    }

    bool validate_type(JsonVariantConst value, const std::string& path, result& out) const;

    kind kind_;
    bool required_ = false;
    bool has_range_ = false;
    bool has_length_ = false;
    double min_ = 0;
    double max_ = 0;
    std::size_t min_length_ = 0;
    std::size_t max_length_ = 0;
    std::vector<std::string> options_;
    predicate check_ = nullptr;
    const char* check_reason_ = nullptr;
    std::shared_ptr<const field> items_;
    std::shared_ptr<const schema> properties_;
};

/// The fields of a JSON object.
class schema {
public:
    schema& add(std::string key, field rules) {
        fields_.emplace_back(std::move(key), std::move(rules));
        return *this;
    }

    /// Also rejects keys that are not in the schema, so a request cannot
    /// smuggle in settings the code does not expect.
    schema& strict() {
        strict_ = true;
        return *this;
    }

    /// Validates a whole document; the root must be an object.
    [[nodiscard]] result validate(JsonVariantConst document) const {
        result out;
        validate(document, std::string(), out);
        return out;
    }

    bool validate(JsonVariantConst value, const std::string& path, result& out) const {
        if (!value.is<JsonObjectConst>()) {
            out.ok = false;
            out.path = path.empty() ? std::string("(root)") : path;
            out.reason = "must be an object";
            return false;
        }
        const JsonObjectConst object = value.as<JsonObjectConst>();
        for (const auto& [key, rules] : fields_) {
            const std::string child = path.empty() ? key : path + "." + key;
            const JsonVariantConst member = object[key.c_str()];
            if (member.isNull()) {
                if (rules.is_required()) {
                    out.ok = false;
                    out.path = child;
                    out.reason = "is required";
                    return false;
                }
                continue;
            }
            if (!rules.validate(member, child, out)) {
                return false;
            }
        }
        if (strict_) {
            for (const JsonPairConst member : object) {
                if (!knows(member.key().c_str())) {
                    out.ok = false;
                    out.path = path.empty() ? std::string(member.key().c_str()) : path + "." + member.key().c_str();
                    out.reason = "is not allowed";
                    return false;
                }
            }
        }
        return true;
    }

private:
    bool knows(std::string_view key) const {
        for (const auto& entry : fields_) {
            if (entry.first == key) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::pair<std::string, field>> fields_;
    bool strict_ = false;
};

inline field field::object(schema properties) {
    field f(kind::object);
    f.properties_ = std::make_shared<const schema>(std::move(properties));
    return f;
}

inline bool field::validate_type(JsonVariantConst value, const std::string& path, result& out) const {
    switch (kind_) {
        case kind::any:
            return true;
        case kind::boolean:
            return value.is<bool>() || fail(out, path, "must be true or false");
        case kind::integer:
            return value.is<JsonInteger>() || value.is<JsonUInt>() || fail(out, path, "must be an integer");
        case kind::number:
            return value.is<JsonFloat>() || value.is<JsonInteger>() || value.is<JsonUInt>() ||
                   fail(out, path, "must be a number");
        case kind::string:
            return value.is<const char*>() || fail(out, path, "must be a string");
        case kind::object:
            return value.is<JsonObjectConst>() || fail(out, path, "must be an object");
        case kind::array:
            return value.is<JsonArrayConst>() || fail(out, path, "must be an array");
    }
    return fail(out, path, "has an unknown type");
}

inline bool field::validate(JsonVariantConst value, const std::string& path, result& out) const {
    if (!validate_type(value, path, out)) {
        return false;
    }
    if (has_range_ && (kind_ == kind::integer || kind_ == kind::number)) {
        const double number = value.as<double>();
        if (number < min_ || number > max_) {
            return fail(out, path, "is out of range");
        }
    }
    if (kind_ == kind::string) {
        const std::string_view text = value.as<const char*>();
        if (has_length_ && (text.size() < min_length_ || text.size() > max_length_)) {
            return fail(out, path, "has a wrong length");
        }
        if (!options_.empty()) {
            bool allowed = false;
            for (const std::string& option : options_) {
                allowed = allowed || option == text;
            }
            if (!allowed) {
                return fail(out, path, "is not an allowed value");
            }
        }
    }
    if (kind_ == kind::array) {
        const JsonArrayConst array = value.as<JsonArrayConst>();
        if (has_length_ && (array.size() < min_length_ || array.size() > max_length_)) {
            return fail(out, path, "has a wrong number of items");
        }
        if (items_) {
            std::size_t index = 0;
            for (const JsonVariantConst item : array) {
                if (!items_->validate(item, path + "[" + std::to_string(index) + "]", out)) {
                    return false;
                }
                ++index;
            }
        }
    }
    if (kind_ == kind::object && properties_ && !properties_->validate(value, path, out)) {
        return false;
    }
    if (check_ != nullptr && !check_(value)) {
        return fail(out, path, check_reason_ != nullptr ? check_reason_ : "failed a check");
    }
    return true;
}

namespace detail {

[[nodiscard]] constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

[[nodiscard]] constexpr bool is_alnum(char c) noexcept {
    return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

}  // namespace detail

/// Dotted-quad IPv4 address such as "192.168.1.20" (no leading zeros).
[[nodiscard]] constexpr bool is_ipv4(std::string_view text) noexcept {
    std::size_t parts = 0;
    std::size_t i = 0;
    while (parts < 4) {
        const std::size_t start = i;
        unsigned value = 0;
        while (i < text.size() && detail::is_digit(text[i]) && i - start < 3) {
            value = value * 10u + static_cast<unsigned>(text[i] - '0');
            ++i;
        }
        const std::size_t digits = i - start;
        if (digits == 0 || value > 255 || (digits > 1 && text[start] == '0')) {
            return false;
        }
        ++parts;
        if (parts < 4) {
            if (i >= text.size() || text[i] != '.') {
                return false;
            }
            ++i;
        }
    }
    return i == text.size();
}

/// Host name made of labels such as "sensor-7.local" (RFC 1123).
[[nodiscard]] constexpr bool is_hostname(std::string_view text) noexcept {
    if (text.empty() || text.size() > 253) {
        return false;
    }
    std::size_t label_length = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const bool end_of_label = i == text.size() || text[i] == '.';
        if (end_of_label) {
            if (label_length == 0 || text[i - 1] == '-') {
                return false;
            }
            label_length = 0;
            continue;
        }
        const char c = text[i];
        if (!detail::is_alnum(c) && !(c == '-' && label_length > 0)) {
            return false;
        }
        if (++label_length > 63) {
            return false;
        }
    }
    return true;
}

/// Adapters with the signature field::check() expects.
inline bool ipv4(JsonVariantConst value) {
    return value.is<const char*>() && is_ipv4(value.as<const char*>());
}

inline bool hostname(JsonVariantConst value) {
    return value.is<const char*>() && is_hostname(value.as<const char*>());
}

}  // namespace ect::json
