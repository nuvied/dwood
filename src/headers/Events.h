
#include <iostream>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <functional>

class Event
{
    public:
    virtual ~Event() = default;
};

struct PlayerDied :public Event
{
    int playerId;
    explicit PlayerDied(int id):playerId(id) {}
};

struct EnemySpawned :public Event
{
    int enemyId;
    explicit EnemySpawned(int id):enemyId(id) {}
};

struct InventoryButtonClicked :public Event
{
    /* data */
};

struct InventoryOpened :public Event
{
    /* data */
};

struct ItemUsed :public Event
{
    int itemId;
    explicit ItemUsed(int id):itemId(id) {}
};

struct ItemCollected :public Event
{
    int itemId;
    explicit ItemCollected(int id):itemId(id) {}
};
struct InventoryClosed :public Event
{
    /* data */
};



class EventBus
{
    public:
    template<typename EventType>
    using Handler = std::function<void(const EventType&)>;

    template<typename EventType>
    static void subscribe(Handler<EventType> handler)
    {
        auto &map  = getSubscribers();
        auto& handlers = map[typeid(EventType)];
        handlers.push_back([handler](const Event& event)
        {
            handler(static_cast<const EventType&>(event));
        });
    }
    template<typename EventType>
    static void publish(const EventType& event)
    {
         auto &map  = getSubscribers();
            auto it = map.find(typeid(EventType));
            if(it != map.end())
            {
                for(auto& handler : it->second)
                {
                    handler(event);
                }
            }
    }
    
    private:
    using HandlerList = std::vector<std::function<void(const Event&)>>;
    static std::unordered_map<std::type_index, HandlerList>& getSubscribers() {
        static std::unordered_map<std::type_index, HandlerList> subs;
        return subs;
    }
};

