#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

class EventBus
{
public:
    template<typename Event>
    void Subscribe(std::function<void(const Event&)> callback)
    {
        auto& list = GetOrCreateList<Event>();
        list.callbacks.push_back(std::move(callback));
    }

    template<typename Event>
    void Publish(const Event& event)
    {
        auto it = m_subscribers.find(std::type_index(typeid(Event)));
        if (it == m_subscribers.end())
        {
            return;
        }
        auto* basePtr = static_cast<HandlerList<Event>*>(it->second.get());
        for (auto& cb : basePtr->callbacks)
        {
            cb(event);
        }
    }

    void Clear()
    {
        m_subscribers.clear();
    }

private:
    struct HandlerListBase
    {
        virtual ~HandlerListBase() = default;
    };

    template<typename Event>
    struct HandlerList : HandlerListBase
    {
        std::vector<std::function<void(const Event&)>> callbacks;
    };

    template<typename Event>
    HandlerList<Event>& GetOrCreateList()
    {
        const std::type_index typeIdx(typeid(Event));
        auto it = m_subscribers.find(typeIdx);
        if (it == m_subscribers.end())
        {
            auto list = std::make_unique<HandlerList<Event>>();
            auto* ptr = list.get();
            m_subscribers.emplace(typeIdx, std::move(list));
            return *ptr;
        }
        return *static_cast<HandlerList<Event>*>(it->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<HandlerListBase>> m_subscribers;
};

