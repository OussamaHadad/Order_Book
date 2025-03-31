#pragma once

#include "enums.h"
#include "LimitLevel.h"
#include "Order.h"
#include "Trade.h"

#include <map>
#include <unordered_map>
#include <mutex>
#include <random>
#include <chrono>
#include <numeric>

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

    std::unordered_map<Type, std::unordered_map<int, std::vector<double>>> addLatencies;
    std::unordered_map<int, std::vector<double>> amendLatencies, cancelLatencies;
    /*  addLatencies keys: 0 -> add order with an existing limit level; 1 -> ... new limit level;
        amendLatencies keys: same as for addLatencies excpet that we are amending orders
        cancelLatencies keys: 0 -> if the cancelled order is last in its limit level; 1 -> if not   */
    std::vector<double> matchLatencies;
    
    // TO DO: Modify the following 3 variables
    std::thread ordersPruneThread_; 
    std::condition_variable shutdownConditionVariable_; 
    std::atomic<bool> shutdown_{ false };   
    
    std::mutex _mutex;

    void cancelGFDOrders(uint32_t TRADING_CLOSE_HOUR = 16);

    void cancelOrders(std::vector<uint32_t> orderIds);

    int updateLimitLevelData(double price, uint32_t shares, Action action);

    bool canFullyFill(Side side, double price, uint32_t quantity) const;
    
    bool canMatch(Side side, double price) const;
    
    Trades matchOrders();
    
public:
    OrderBook();    
    ~OrderBook();

    uint32_t getNumberOfOrders() {return orders.size();}
    
    // Helper to get a random order ID from the current orders
    uint32_t getRandomOrderId();

    OrderPointer getOrderPtr(uint32_t orderId) {return orders[orderId].order;}

    Trades addOrder(OrderPointer orderPtr, bool newOrder = true, double initLatencyCount = 0);
    void cancelOrder(uint32_t orderId, bool lockOn = true, bool amendedOrder = false);
    Trades amendOrder(OrderPointer orderPtr, double newPrice, uint32_t newShares);

    void printOrderBook() const;

    void clearLatencies();

    void OrderBook::analyzeLatencies(int nUpdates = -1);

    void writeLatencyStatsToFile(const std::string& filename, int nUpdates = -1);
};