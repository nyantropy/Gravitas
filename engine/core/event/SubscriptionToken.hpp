#pragma once

#include <functional>
#include <utility>

// RAII subscription token. Calling the destructor unsubscribes the handler.
// Move-only — no copy. Systems store tokens as members so that subscriptions
// are automatically cleaned up when the system is destroyed (scene teardown).
class SubscriptionToken
{
public:
    SubscriptionToken() = default;

    ~SubscriptionToken()
    {
        if (unsubscribeFn)
            unsubscribeFn();
    }

    SubscriptionToken(SubscriptionToken&& other) noexcept
        : unsubscribeFn(std::exchange(other.unsubscribeFn, nullptr))
    {
    }

    SubscriptionToken& operator=(SubscriptionToken&& other) noexcept
    {
        if (this != &other)
        {
            if (unsubscribeFn)
                unsubscribeFn();
            unsubscribeFn = std::exchange(other.unsubscribeFn, nullptr);
        }
        return *this;
    }

    SubscriptionToken(const SubscriptionToken&)            = delete;
    SubscriptionToken& operator=(const SubscriptionToken&) = delete;

    explicit operator bool() const { return unsubscribeFn != nullptr; }

    // Release the subscription without calling the unsubscribe function.
    // Use when the bus has already been destroyed and the lambda would dangle.
    void release() { unsubscribeFn = nullptr; }

private:
    // Only ECSWorld may construct a valid token.
    friend class ECSWorld;
    explicit SubscriptionToken(std::function<void()> fn) : unsubscribeFn(std::move(fn)) {}

    std::function<void()> unsubscribeFn;
};
