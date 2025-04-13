#include <thread>
#include <shared_mutex>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "OrderBook.h"

using json = nlohmann::json;

static std::unordered_map<Type, std::string> map_types = {{Type::GTC, "GTC"}, {Type::FAK, "FAK"}, {Type::FOK, "FOK"}, {Type::GFD, "GFD"}, {Type::M, "M"}};
static std::unordered_map<Side, std::string> map_sides = {{Side::Bid, "Bid"}, {Side::Ask, "Ask"}};

void OrderBook::cancelGFDOrders(uint32_t TRADING_CLOSE_HOUR){ 
    /*Cancel all Good For Day orders when the market closes at TRADING_CLOSE_HOUR*/
    using namespace std::chrono;    // Import everything from std::chrono

    const auto closeTime = hours(TRADING_CLOSE_HOUR);  // Trading close hour set to TRADING_CLOSE_HOUR:00

	while (true) {
        // Get the current system time
		const auto now = system_clock::now();
		const auto now_c = system_clock::to_time_t(now);
		std::tm now_parts;		
        localtime_s(&now_parts, &now_c);

        // Adjust the time if it's past the close hour
        if (now_parts.tm_hour >= closeTime.count())
            now_parts.tm_mday += 1;  // Move to the next day if past close time

        // Set the target time to TRADING_CLOSE_HOUR:00 (market close)
		now_parts.tm_hour = closeTime.count();
		now_parts.tm_min = 0;
		now_parts.tm_sec = 0;

        // Convert to system_clock time
        auto nextCloseTime = system_clock::from_time_t(mktime(&now_parts));
        auto waitDuration = nextCloseTime - now + milliseconds(100);  // Allow a small buffer of 100 milliseconds

        // Wait for the market close or shutdown signal
		{
            std::unique_lock<std::mutex> ordersLock{_mutex};

            // Wait for shutdown signal or timeout
            if (shutdown.load(std::memory_order_acquire) ||
                            shutdownConditionVariable.wait_for(ordersLock, waitDuration) == std::cv_status::no_timeout)
				return;
		}

		std::vector<uint32_t> GFDorderIds;

        // Collect order IDs for GFD orders
		{
            std::unique_lock<std::mutex> lock{_mutex};

            for (const auto& item : orders) {
                const OrderInfo& entry = item.second;
                const OrderPointer& orderPtr = entry.order;

				if (orderPtr->getOrderType() != Type::GFD)
					continue;

                    GFDorderIds.push_back(orderPtr->getOrderId());
			}
		}

		cancelOrders(GFDorderIds);
	}
}


void OrderBook::cancelOrders(std::vector<uint32_t> orderIds){
    std::unique_lock<std::mutex> ordersLock{_mutex};

    for (auto orderId : orderIds)
        OrderBook::cancelOrder(orderId, false);
}


int OrderBook::updateLimitLevelData(double price, uint32_t shares, Action action){
    /*  Arguments:
            price: used to identify the limit level
            shares: the number of shares subject to action
            action: the type of action that is applied to the limit level

        Returns:
            -1: if the last order from the limit level was removed
             1: if a new limit level was added
             0: else
    */

    // Check if the price exists in the data map
    auto it = data.find(price);

    if (it == data.end()){
        // If the price does not exist and the action is Add, create a new entry
        if (action == Action::Add) 
            data[price] = LimitLevelData{shares, 1}; // Initialize with shares and 1 order
        else
            // If the price does not exist and the action is not Add, do nothing
            std::cerr << "Error: Attempted to modify a non-existent limit level with price " << price << std::endl;
        return 1;
    }

    // Access the limit level
    auto& limitLevel = it->second;

    // Update the number of orders
    limitLevel.totalOrders += (action == Action::Remove) ? -1 : (action == Action::Add) ? 1 : 0;
    // Update the number of shares
    limitLevel.totalShares += (action == Action::Add) ? shares : -shares;

    // Remove the limit level if it is empty
    if (limitLevel.totalOrders == 0) {
        data.erase(price);
        return -1;
    }
    
    return 0;
}


bool OrderBook::canFullyFill(Side side, double price, uint32_t quantity) const{
    /* Tells if an order can be fully filled or not (We only use it for Fill Or Kill orders) */

    if (!canMatch(side, price)) // Early exit if the order can't match at all
        return false;

    if (side == Side::Bid){    // We are buying, thus match against asks (ascending)
        for (const auto& item : asks){
            Price askPrice = item.first;
            auto& askOrders = item.second;

            if (askPrice > price)
                break; // Can't match beyond the bid price

            for (const auto& orderPtr : askOrders) {
                quantity -= orderPtr->getOrderShares();
                if (quantity <= 0)
                    return true;
            }
        }
    }
    else {  // We are selling, thus match against bids (descending)
        for (const auto& item : bids){
                Price bidPrice = item.first;
                auto& bidOrders = item.second;

            if (bidPrice < price)
                break; // Can't match below the ask price

            for (const auto& orderPtr : bidOrders) {
                quantity -= orderPtr->getOrderShares();
                if (quantity <= 0)
                    return true;
            }
        }
    }

    return false;
}


bool OrderBook::canMatch(Side side, double price) const{
    /* Tells whether an order of 'side' side and 'price' price can match an order in the order side of the orderbook */
    if (side == Side::Bid){
        if (asks.empty())
            return false;

        const auto& item = *asks.begin();
        double bestAskPrice = item.first;

        return (bestAskPrice <= price); 
    }
    else{   // side == Side::Ask
        if (bids.empty())
            return false;

        const auto& item = *bids.begin();
        double bestBidPrice = item.first;

        return (bestBidPrice >= price);
    }
}


Trades OrderBook::matchOrders(){
    /* Match all possible orders from the orderbook, and return the trades.
        Finally, we check if there is any Fill And Kill order that was triggered but not fullt executed to cancel it. 
    */
    Trades trades;

    while (true){

        if (bids.empty() || asks.empty())
            break;

        std::cout << "pre 4" << std::endl;
        auto& itemBid = *bids.begin();
        double bestBidPrice = itemBid.first;
        OrderPointers& bestBids = itemBid.second;

        auto& itemAsk = *asks.begin();
        double bestAskPrice = itemAsk.first;
        OrderPointers& bestAsks = itemAsk.second;
        std::cout << "post 4" << std::endl;

        // If the best bid price is less than the best ask price, no match is possible
        if (bestBidPrice < bestAskPrice)
            break;

        std::cout << "pre 5" << std::endl;
        // Match orders at the best bid & ask prices
        while (!bestBids.empty() && !bestAsks.empty()){

            auto start = std::chrono::high_resolution_clock::now(); // Start of time computation

            auto headBid = bestBids.front();
            auto headAsk = bestAsks.front();

            uint32_t tradedShares = std::min(headBid->getOrderShares(), headAsk->getOrderShares());

            /*  Q: What if order is FOK? We can use canFullyFill(...) to tell if this order should pass or not
                A: FOK orders that can't be executed are discarded during the add phase    */
            
            // Fill the orders
            headBid->fillOrder(tradedShares);
            headAsk->fillOrder(tradedShares);

            // Remove fully filled orders
            if (headBid->isFilled()){
                bestBids.pop_front();
                orders.erase(headBid->getOrderId());
            }

            if (headAsk->isFilled()){
                bestAsks.pop_front();
                orders.erase(headAsk->getOrderId());
            }

            // Record the trade
            trades.push_back( Trade( 
                TradeInfo{headBid->getOrderId(), headBid->getOrderPrice(), tradedShares},
                TradeInfo{headAsk->getOrderId(), headAsk->getOrderPrice(), tradedShares}
            ));

            // Update limit level data
            (void) updateLimitLevelData(headBid->getOrderPrice(), tradedShares, headBid->isFilled() ? Action::Remove : Action::Match);
            (void) updateLimitLevelData(headAsk->getOrderPrice(), tradedShares, headAsk->isFilled() ? Action::Remove : Action::Match);

            auto end = std::chrono::high_resolution_clock::now(); // End of time computation
            std::chrono::duration<double, std::micro> latency = end - start;

            std::cout << "due to latency" << std::endl;
            matchLatencies.push_back(latency.count());
        }
        std::cout << "post 5" << std::endl;

        std::cout << "pre 6" << std::endl;
        // Remove empty price levels
        if (bestBids.empty()){
            bids.erase(bestBidPrice);
            // data.erase(bestBidPrice);
        }

        if (bestAsks.empty()){
            asks.erase(bestAskPrice);
            // data.erase(bestAskPrice);
        }
        std::cout << "post 6" << std::endl;
    }

    std::cout << "pre 7" << std::endl;
    // Handle FAK orders
    if (!bids.empty()){
        auto& item = *bids.begin();
        OrderPointers& bestBids = item.second;
        auto headOrder = bestBids.front();
        std::cout << "pre 7.1" << std::endl;
        if (headOrder->getOrderType() == Type::FAK && headOrder->getOrderInitialShares() != headOrder->getOrderShares()){
            std::cout << "pre 7.2" << std::endl;
            cancelOrder(headOrder->getOrderId());
            std::cout << "pre 7.3" << std::endl;
        }
    }
    std::cout << "post 7" << std::endl;

    std::cout << "pre 8" << std::endl;
    if (!asks.empty()){
        auto& item = *asks.begin();
        OrderPointers& bestAsks = item.second;
        auto headOrder = bestAsks.front();
        std::cout << "pre 8.1" << std::endl;
        if (headOrder->getOrderType() == Type::FAK && headOrder->getOrderInitialShares() != headOrder->getOrderShares()){
            std::cout << "pre 8.2" << std::endl;
            cancelOrder(headOrder->getOrderId());
        }
        std::cout << "pre 8.3" << std::endl;
    }

    // Print trades
    std::cout << "Trades:" << std::endl;
    for (const auto& trade : trades)
        trade.getTradeDetails();

    return trades;
}


OrderBook::OrderBook() {
    ordersPruneThread = std::thread([this] {
                                                cancelGFDOrders();
                                            }
                                    );
}

OrderBook::~OrderBook(){
    // Signal the background thread to stop
    shutdown.store(true, std::memory_order_release);

    // Wake up the thread if it's waiting
	shutdownConditionVariable.notify_one();
    
    // Wait for the background thread to finish
    if (ordersPruneThread.joinable())
	    ordersPruneThread.join();
}


uint32_t OrderBook::getRandomOrderId(){
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, orders.size() - 1);
    
    auto it = orders.begin();
    std::advance(it, dis(gen)); // Move to a random position in the map
    return it->first;
}


Trades OrderBook::addOrder(OrderPointer orderPtr, bool newOrder, double initLatencyCount){
    /*  Given an order pointer we do the following:
            1. If the order is Fill And/Or Kill, then we first check if it's possible to fill it partially/completely
            2. If the order is a Market order then we  turn it into a Good Till Cancel order with the worst possible price to make sure
                it will be fully filled except if the number of shares from the opposite side isn't enough
        Then we add the order to orders map and given order's side to bids or asks map
        After that, we update the limit level.
        Finally we match orders. 
    */
    auto start = std::chrono::high_resolution_clock::now();

    std::unique_lock<std::mutex> ordersLock{_mutex};

    if (newOrder)
        std::cout << "Adding Order: ID " << orderPtr->getOrderId()
                << ", Side " << map_sides[orderPtr->getOrderSide()]
                << " & Type " << map_types[orderPtr->getOrderType()]
                << ", Price = " << orderPtr->getOrderPrice()
                << ", Shares = " << orderPtr->getOrderShares() << std::endl;
    else
        std::cout << "Modifying Order of ID " << orderPtr->getOrderId()
                << ", Side " << map_sides[orderPtr->getOrderSide()]
                << " & Type " << map_types[orderPtr->getOrderType()]
                << ": Price = " << orderPtr->getOrderPrice()
                << ", Shares = " << orderPtr->getOrderShares() << std::endl;

    if (orders.find(orderPtr->getOrderId()) != orders.end()){
        std::cout << "Order ID " << orderPtr->getOrderId() << " already exists. Skipping." << std::endl;
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        std::cout << "due to latency" << std::endl;
        addLatencies[orderPtr->getOrderType()][0].push_back(latency.count()); // 0 is the default key
        return {};  // equivalent of None for the return type (Trades in this case)
    }

    if (orderPtr->getOrderType() == Type::FAK && !canMatch(orderPtr->getOrderSide(), orderPtr->getOrderPrice())){
        std::cout << "FAK order cannot be matched. Skipping." << std::endl;    
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        std::cout << "due to latency" << std::endl;
        addLatencies[orderPtr->getOrderType()][0].push_back(latency.count()); // 0 is the default key
        return {};
    }

    else if (orderPtr->getOrderType() == Type::FOK && !canFullyFill(orderPtr->getOrderSide(), orderPtr->getOrderPrice(), orderPtr->getOrderShares())){
        std::cout << "FOK order cannot be fully filled. Skipping." << std::endl;
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        std::cout << "due to latency" << std::endl;
        addLatencies[orderPtr->getOrderType()][0].push_back(latency.count()); // 0 is the default key
        return {};
    }

    else if (orderPtr->getOrderType() == Type::M){  // Market order
        /* Turn the market order into a Good Till Cancel order with the "worst" possible price, thus we are sure all orders
            from the opposite side match our order */
        if (orderPtr->getOrderSide() == Side::Bid && !asks.empty()){
            const auto& item = *asks.rbegin();
            double worstAskPrice = item.first;
            orderPtr->marketToGTC(worstAskPrice);
        }
        else if (orderPtr->getOrderSide() == Side::Ask && !bids.empty()) {
            const auto& item = *bids.rbegin();
            double worstBidPrice = item.first;
            orderPtr->marketToGTC(worstBidPrice);
        }
        else{
            std::cout << "Market order cannot be processed. Skipping." << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> latency = end - start;
            std::cout << "due to latency" << std::endl;
            addLatencies[orderPtr->getOrderType()][0].push_back(latency.count()); // 0 is the default key
            return {};
        }
    }

    OrderPointers::iterator iterator;

    if (orderPtr->getOrderSide() == Side::Bid){
        auto& limitLevelOrdersPtrs = bids[orderPtr->getOrderPrice()];
        limitLevelOrdersPtrs.push_back(orderPtr);
        iterator = std::prev(limitLevelOrdersPtrs.end());
        std::cout << "Order added to Bids at price " << orderPtr->getOrderPrice() << std::endl;
    }
    else if (orderPtr->getOrderSide() == Side::Ask){
        auto& limitLevelOrdersPtrs = asks[orderPtr->getOrderPrice()];
        limitLevelOrdersPtrs.push_back(orderPtr);
        iterator = std::prev(limitLevelOrdersPtrs.end());
        std::cout << "Order added to Asks at price " << orderPtr->getOrderPrice() << std::endl;
    }
    else {
        std::cerr << "Invalid order side. Skipping order." << std::endl;
        return {};
    }

    std::cout << "pre 1" << std::endl;
    orders.insert({orderPtr->getOrderId(), OrderInfo{orderPtr, iterator}});
    std::cout << "post 1" << std::endl;

    std::cout << "pre 2" << std::endl;
    auto addLatenciesKey = updateLimitLevelData(orderPtr->getOrderPrice(), orderPtr->getOrderShares(), Action::Add);
    std::cout << "post 2" << std::endl;

    std::cout << "pre 3" << std::endl;
    if (newOrder){
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        std::cout << "due to latency" << std::endl;
        addLatencies[orderPtr->getOrderType()][addLatenciesKey].push_back(latency.count());
    }
    else{
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        std::cout << "due to latency" << std::endl;
        amendLatencies[addLatenciesKey].push_back(initLatencyCount + latency.count()); // amendLatenciesKey not add...
    }
    std::cout << "post 3" << std::endl;

    return matchOrders();
}


void OrderBook::cancelOrder(uint32_t orderId, bool lockOn, bool amendedOrder){
    /* Arguments:
        orderId: used to identify the order
        lockOn: used to activate the scoped lock. As in case we are deleting many orders we would like to activate the lock only once, before we start the deletion
        
    This function cancels an order by removing it from orders map, then asks or bids map given its side, and finally updates its limit level.
    */
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "canc 1" << std::endl;
    if (lockOn)
        std::unique_lock<std::mutex> ordersLock{_mutex};    

    std::cout << "canc 2" << std::endl;
    if (orders.find(orderId) == orders.end())
        return;

    const auto& item = orders.at(orderId);
    OrderPointer orderPtr = item.order;
    OrderPointers::iterator orderIterator = item.orderIter; 
    
    std::cout << "canc 3" << std::endl;
    // Remove order from orders map
    orders.erase(orderId);

    std::cout << "canc 4" << std::endl;
    // Remove order from asks or bids given its side
    const auto price = orderPtr->getOrderPrice();

    if (orderPtr->getOrderSide() == Side::Bid){
        bids[price].erase(orderIterator);

        if (bids[price].empty())
            bids.erase(price);
    }
    else{
        asks[price].erase(orderIterator);

        if (asks[price].empty())
            asks.erase(price);
    }

    std::cout << "canc 5" << std::endl;
    // Update order's limit level
    auto cancelLatenciesKey = updateLimitLevelData(orderPtr->getOrderPrice(), orderPtr->getOrderShares(), Action::Remove);

    std::cout << "due to latency" << std::endl;
    if (!amendedOrder){
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        std::cout << "due to latency" << std::endl;
        cancelLatencies[cancelLatenciesKey].push_back(latency.count());
    }
}


Trades OrderBook::amendOrder(OrderPointer existingOrderPtr, double newPrice, uint32_t newShares){
    auto start = std::chrono::high_resolution_clock::now();

    if (newPrice < 0)
        throw std::logic_error(
            (std::ostringstream{} << "Order (" << existingOrderPtr->getOrderId() << ") can't be modified as the new price is negative").str()
        );

    if (newShares <= 0)
        throw std::logic_error(
            (std::ostringstream{} << "Order (" << existingOrderPtr->getOrderId() << ") can't be modified as the new number of shares should be strictly positive").str()
        );

    {   // Use lock to avoid executing this order while it is being modified
        std::unique_lock<std::mutex> lock{_mutex};

        Type orderType = existingOrderPtr->getOrderType();
        
        if (orders.find(existingOrderPtr->getOrderId()) == orders.end()){
            std::cout << "Inexistent order. Can't be modified." << std::endl;
            return {};
        }

        cancelOrder(existingOrderPtr->getOrderId(), false, true);
    }

    auto newOrderPtr = std::make_shared<Order> (
        existingOrderPtr->getOrderId(), existingOrderPtr->getOrderType(), existingOrderPtr->getOrderSide(), newPrice, newShares
    );

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> initLatency = end - start;

    return addOrder(newOrderPtr, false, initLatency.count());
}


void OrderBook::printOrderBook() const {
    std::cout << "Order Book:" << std::endl;

    // Print Bids
    std::cout << "Bids:" << std::endl;
    for (const auto& price_bidPtr : bids){ // [price, vector of Bids of this price]
        uint32_t totalShares = 0;
        for (const auto& orderPtr : price_bidPtr.second)
            totalShares += orderPtr->getOrderShares();

        std::cout   << "  Price = " << price_bidPtr.first 
                    << ", Number of Bids = " << price_bidPtr.second.size() 
                    << ", Number of Shares = " << totalShares << std::endl;
    }
                
    // Print Asks
    std::cout << "Asks:" << std::endl;
    for (const auto& price_askPtr : asks){ // [price, vector of Asks of this price]
        uint32_t totalShares = 0;
        for (const auto& orderPtr : price_askPtr.second)
            totalShares += orderPtr->getOrderShares();

        std::cout   << "  Price = " << price_askPtr.first 
                    << ", Number of Asks = " << price_askPtr.second.size() 
                    << ", Number of Shares = " << totalShares << std::endl;
    }
}


void OrderBook::clearLatencies() {
    addLatencies.clear();
    amendLatencies.clear();
    cancelLatencies.clear();
    matchLatencies.clear();
}


void OrderBook::writeLatencyStatsToFile(const std::string& filename, int nUpdates) {
    std::ofstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file for writing latency statistics.");

    int totalTransactions = 0;

    auto computeStats = [](const std::vector<double>& latencies) -> std::pair<double, double> {
        if (latencies.empty()) 
            return {0.0, 0.0};

        double mean = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        double variance = 0.0;
        for (double latency : latencies)
            variance += (latency - mean) * (latency - mean);
        variance /= latencies.size();
        
        return {mean, variance};
    };

    json statsJson;

    // Add Order Latencies
    for (const auto& type_latencyMap : addLatencies) {
        const auto& orderTypeStr = map_types[type_latencyMap.first];

        for (const auto& latencyEntry : type_latencyMap.second) {
            std::string limitStatusStr = (latencyEntry.first == 0) ? "existing_limit_level" : "new_limit_level";
            auto stats = computeStats(latencyEntry.second);

            statsJson["Add"].push_back({
                {"order_type", orderTypeStr},
                {"limit_level_status", limitStatusStr},
                {"mean_latency (μs)", stats.first},
                {"latency_variance (μs)", stats.second},
                {"number_of_orders", latencyEntry.second.size()}
            });

            totalTransactions += latencyEntry.second.size();
        }
    }

    // Amend Order Latencies
    for (const auto& latencyEntry : amendLatencies) {
        std::string limitStatusStr = (latencyEntry.first == 0) ? "existing_limit_level" : "new_limit_level";
        auto stats = computeStats(latencyEntry.second);

        statsJson["Amend"].push_back({
            {"limit_level_status", limitStatusStr},
            {"mean_latency (μs)", stats.first},
            {"latency_variance (μs)", stats.second},
            {"number_of_orders", latencyEntry.second.size()}
        });

        totalTransactions += latencyEntry.second.size();
    }

    // Cancel Order Latencies
    for (const auto& latencyEntry : cancelLatencies) {
        std::string limitStatusStr = (latencyEntry.first == 0) ? "last_in_limit_level" : "not_last_in_limit_level";
        auto stats = computeStats(latencyEntry.second);

        statsJson["Cancel"].push_back({
            {"limit_level_status", limitStatusStr},
            {"mean_latency (μs)", stats.first},
            {"latency_variance (μs)", stats.second},
            {"number_of_orders", latencyEntry.second.size()}
        });

        totalTransactions += latencyEntry.second.size();
    }

    // Match Latencies
    auto matchStats = computeStats(matchLatencies);
    statsJson["Match"] = {
        {"limit_level_status", "none"},
        {"mean_latency (μs)", matchStats.first},
        {"latency_variance (μs)", matchStats.second},
        {"number_of_orders", matchLatencies.size()}
    };

    // Write to file
    file << std::setw(4) << statsJson << std::endl;

    // Consistency check
    std::cout << "\nTotal Transactions Counted: " << totalTransactions << " | Expected: " << nUpdates << "\n";
    if (nUpdates != -1 && totalTransactions != nUpdates)
        throw std::runtime_error("Mismatch in total number of updates!");

    std::cout << "Latency statistics written to " << filename << std::endl;
    file.close();
}
