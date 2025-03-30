#pragma once

#include "enums.h"
#include "LimitLevel.h"
#include "Order.h"
#include "Trade.h"

#include <map>
#include <unordered_map>
#include <mutex>
#include <random>

struct OrderInfo{
    OrderPointer order{nullptr};
    OrderPointers::iterator orderIter;  // Used for fast access to the order in OrderPointers = std::list<OrderPointer>
};

struct LimitLevelData{
    uint32_t totalShares{};
    uint32_t totalOrders{};
};


class OrderBook{
private:
    std::unordered_map<double, LimitLevelData> data; // This map associates to each price its limit level's data
    std::unordered_map<uint32_t, OrderInfo> orders;

    // We use map not unordered_map for both bids & asks since limit levels are ordered given their prices
    std::map<double, OrderPointers, std::greater<>> bids;
    std::map<double, OrderPointers> asks;   // std::less<> is the default comparator thus no need to state it explicitly

    // TO DO: Modify the following 3 variables
    std::thread ordersPruneThread_; 
    std::condition_variable shutdownConditionVariable_; 
    std::atomic<bool> shutdown_{ false };   
    
    std::mutex _mutex;

    void cancelGFDOrders(uint32_t TRADING_CLOSE_HOUR = 16);

    void cancelOrders(std::vector<uint32_t> orderIds);

    void updateLimitLevelData(double price, uint32_t shares, Action action);

    bool canFullyFill(Side side, double price, uint32_t quantity) const;
    
    bool canMatch(Side side, double price) const;
    
    Trades matchOrders();
    
public:
    OrderBook();    
    ~OrderBook();

    uint32_t getNumberOfOrders() {return orders.size();}
    
    // Helper to get a random order ID from the current orders
    uint32_t getRandomOrderId(){
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, orders.size() - 1);
        
        auto it = orders.begin();
        std::advance(it, dis(gen)); // Move to a random position in the map
        return it->first;
    }

    OrderPointer getOrderPtr(uint32_t orderId) {return orders[orderId].order;}

    Trades addOrder(OrderPointer orderPtr, bool newOrder = true);
    void cancelOrder(uint32_t orderId, bool lockOn = true);
    Trades amendOrder(OrderPointer orderPtr, double newPrice, uint32_t newShares);

    void printOrderBook() const;
};