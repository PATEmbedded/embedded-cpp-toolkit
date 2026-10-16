// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// A value that notifies subscribers when it changes.

#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

namespace ect {

/// Lock that does nothing, for values used by a single task.
struct null_mutex {
    void lock() noexcept {}
    void unlock() noexcept {}
};

/// Holds a value and calls subscribers with the old and new value whenever
/// it changes. Writing the same value again notifies nobody.
///
/// - `MaxSubscribers` fixes the number of callbacks, so subscribing never
///   allocates a slot at run time.
/// - `Mutex` is any lockable type (std::mutex, or a FreeRTOS mutex wrapper)
///   when several tasks write the value. Callbacks run after the lock is
///   released, so a callback may read or write the observable itself.
/// - Subscribe during initialisation, before other tasks start writing.
template <class T, std::size_t MaxSubscribers = 4, class Mutex = null_mutex>
class observable {
public:
    using callback = std::function<void(const T& old_value, const T& new_value)>;

    explicit observable(T initial = T{}) : value_(std::move(initial)) {}

    observable(const observable&) = delete;
    observable& operator=(const observable&) = delete;

    /// Registers a callback. Returns false when all slots are taken.
    bool subscribe(callback on_change) {
        for (callback& slot : subscribers_) {
            if (!slot) {
                slot = std::move(on_change);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] T get() const {
        std::lock_guard<Mutex> lock(mutex_);
        return value_;
    }

    /// Stores a value. Returns true, after notifying, when it differs from the old one.
    bool set(const T& new_value) {
        std::unique_lock<Mutex> lock(mutex_);
        if (value_ == new_value) {
            return false;
        }
        const T old_value = std::exchange(value_, new_value);
        lock.unlock();
        notify(old_value, new_value);
        return true;
    }

    /// Replaces the value with `transform(value)` in one locked step, so
    /// read-modify-write from several tasks loses no update.
    template <class Transform>
    bool update(Transform&& transform) {
        std::unique_lock<Mutex> lock(mutex_);
        T new_value = std::forward<Transform>(transform)(static_cast<const T&>(value_));
        if (new_value == value_) {
            return false;
        }
        const T old_value = std::exchange(value_, new_value);
        lock.unlock();
        notify(old_value, new_value);
        return true;
    }

    observable& operator=(const T& new_value) {
        set(new_value);
        return *this;
    }

    operator T() const {
        return get();
    }

    template <class U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
    observable& operator+=(const T& delta) {
        update([&delta](const T& value) { return static_cast<T>(value + delta); });
        return *this;
    }

    template <class U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
    observable& operator-=(const T& delta) {
        update([&delta](const T& value) { return static_cast<T>(value - delta); });
        return *this;
    }

    template <class U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
    observable& operator++() {
        return *this += T{1};
    }

    template <class U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
    observable& operator--() {
        return *this -= T{1};
    }

private:
    void notify(const T& old_value, const T& new_value) const {
        for (const callback& subscriber : subscribers_) {
            if (subscriber) {
                subscriber(old_value, new_value);
            }
        }
    }

    mutable Mutex mutex_;
    T value_;
    std::array<callback, MaxSubscribers> subscribers_{};
};

}  // namespace ect
