#include "OrderBook.h"

#include <thread>
#include <shared_mutex>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>


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

// TO DO (all what's left): Reformulate this function completely as it has some logical inconsistencies
bool OrderBook::canFullyFill(Side side, double price, uint32_t quantity) const{
    /* Tells if an order can be fully filled or not (We only use it for Fill Or Kill orders) */

    if (!canMatch(side, price)) // Early exit if the order can't match at all
        return false;

    if (quantity == 0)  // If quantity is zero, we can consider it fully filled
        return true;

    // TO DO 1: Only useful if the 1st condition is written in the next loop
    double oppositeHeadPrice = -1; // or use optional<double>

    if (side == Side::Bid){
        const auto& item = *asks.begin();
        double bestAskPrice = item.first;
        oppositeHeadPrice = bestAskPrice;
    }
    else{
        const auto& item = *bids.begin();
        double bestBidPrice = item.first;
        oppositeHeadPrice = bestBidPrice;
    }

    // TO DO 2: turn data into map (not unordered_map) and then update this loop to break once nothing can change
    for (const auto& item : data){
        double limitLevelPrice = item.first;
        LimitLevelData limitLevelData = item.second;

        // TO DO 3: Add 1st condition?

        if ((side == Side::Bid && limitLevelPrice > price) || (side == Side::Ask && limitLevelPrice < price))
            continue;

        if (quantity <= limitLevelData.totalShares)
            return true;
        
        quantity -= limitLevelData.totalShares;
    }

    return false;
}


bool OrderBook::canMatch(Side side, double price) const{
    /* Tells whether an order of 'side' side and 'price' price can match an order in the order side of the orderbook */
    if (side == Side::Bid){
        if (asks.empty())
            return false;

        //const auto& [bestAskPrice, _] = *asks.begin(); // Best Ask information
        const auto& item = *asks.begin(); // Best Ask information
        double bestAskPrice = item.first;

        return (bestAskPrice <= price); 
    }
    else{   // side == Side::Ask
        if (bids.empty())
            return false;

        //const auto& [bestBidPrice, _] = *bids.begin();
        const auto& item = *bids.begin(); // Best Ask information
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

        auto& itemBid = *bids.begin();
        double bestBidPrice = itemBid.first;
        OrderPointers& bestBids = itemBid.second;

        auto& itemAsk = *asks.begin();
        double bestAskPrice = itemAsk.first;
        OrderPointers& bestAsks = itemAsk.second;

        // If the best bid price is less than the best ask price, no match is possible
        if (bestBidPrice < bestAskPrice)
            break;

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

            matchLatencies.push_back(latency.count());
        }

        // Remove empty price levels
        if (bestBids.empty()){
            bids.erase(bestBidPrice);
            // data.erase(bestBidPrice);
        }

        if (bestAsks.empty()){
            asks.erase(bestAskPrice);
            // data.erase(bestAskPrice);
        }
    }

    // Handle FAK orders
    if (!bids.empty()){
        auto& item = *bids.begin();
        OrderPointers& bestBids = item.second;
        auto headOrder = bestBids.front();
        if (headOrder->getOrderType() == Type::FAK && headOrder->getOrderInitialShares() != headOrder->getOrderShares())
            cancelOrder(headOrder->getOrderId());
    }

    if (!asks.empty()){
        auto& item = *asks.begin();
        OrderPointers& bestAsks = item.second;
        auto headOrder = bestAsks.front();
        if (headOrder->getOrderType() == Type::FAK && headOrder->getOrderInitialShares() != headOrder->getOrderShares())
            cancelOrder(headOrder->getOrderId());
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

    std::unordered_map<Type, std::string> map_types = {{Type::GTC, "GTC"}, {Type::FAK, "FAK"}, {Type::FOK, "FOK"}, {Type::GFD, "GFD"}, {Type::M, "M"}};
    std::unordered_map<Side, std::string> map_sides = {{Side::Bid, "Bid"}, {Side::Ask, "Ask"}};

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
        addLatencies[orderPtr->getOrderType()][0].push_back(latency.count()); // 0 is the default key
        return {};  // equivalent of None for the return type (Trades in this case)
    }

    if (orderPtr->getOrderType() == Type::FAK && !canMatch(orderPtr->getOrderSide(), orderPtr->getOrderPrice())){
        std::cout << "FAK order cannot be matched. Skipping." << std::endl;    
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        addLatencies[orderPtr->getOrderType()][0].push_back(latency.count()); // 0 is the default key
        return {};
    }

    else if (orderPtr->getOrderType() == Type::FOK && !canFullyFill(orderPtr->getOrderSide(), orderPtr->getOrderPrice(), orderPtr->getOrderShares())){
        std::cout << "FOK order cannot be fully filled. Skipping." << std::endl;
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
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

    orders.insert({orderPtr->getOrderId(), OrderInfo{orderPtr, iterator}});

    auto addLatenciesKey = updateLimitLevelData(orderPtr->getOrderPrice(), orderPtr->getOrderShares(), Action::Add);

    if (newOrder){
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        addLatencies[orderPtr->getOrderType()][addLatenciesKey].push_back(latency.count());
    }
    else{
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
        amendLatencies[addLatenciesKey].push_back(initLatencyCount + latency.count()); // amendLatenciesKey not add...
    }

    return matchOrders();
}


void OrderBook::cancelOrder(uint32_t orderId, bool lockOn, bool amendedOrder){
    /* Arguments:
        orderId: used to identify the order
        lockOn: used to activate the scoped lock. As in case we are deleting many orders we would like to activate the lock only once, before we start the deletion
        
    This function cancels an order by removing it from orders map, then asks or bids map given its side, and finally updates its limit level.
    */
    auto start = std::chrono::high_resolution_clock::now();

    if (lockOn)
        std::unique_lock<std::mutex> ordersLock{_mutex};    

    if (orders.find(orderId) == orders.end())
        return;

    const auto& item = orders.at(orderId);
    OrderPointer orderPtr = item.order;
    OrderPointers::iterator orderIterator = item.orderIter; 

    // Remove order from orders map
    orders.erase(orderId);

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

    // Update order's limit level
    auto cancelLatenciesKey = updateLimitLevelData(orderPtr->getOrderPrice(), orderPtr->getOrderShares(), Action::Remove);

    if (!amendedOrder){
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> latency = end - start;
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

    // Header
    file << "order,type,limit_level_status,mean_latency,latency_variance,number_of_orders\n";

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

    // Write Add Order latencies
    for (const auto& type_latencyMap : addLatencies) {
        auto orderType = static_cast<int>(type_latencyMap.first);
        for (const auto& latencyEntry : type_latencyMap.second) {
            int limitLevelStatus = latencyEntry.first;
            auto stats = computeStats(latencyEntry.second);

            file << "Add," << orderType << "," << limitLevelStatus << "," << stats.first << "," << stats.second << "," << latencyEntry.second.size() << "\n";

            totalTransactions += latencyEntry.second.size();
        }
    }

    // Write Amend Order latencies
    for (const auto& latencyEntry : amendLatencies) {
        int limitLevelStatus = latencyEntry.first;
        auto stats = computeStats(latencyEntry.second);

        file << "Amend," << limitLevelStatus << "," << stats.first << "," << stats.second << "," << latencyEntry.second.size() << "\n";

        totalTransactions += latencyEntry.second.size();
    }

    // Write Cancel Order latencies
    for (const auto& latencyEntry : cancelLatencies) {
        auto stats = computeStats(latencyEntry.second);

        file << "Cancel," << latencyEntry.first << "," << stats.first << "," << stats.second << "," << latencyEntry.second.size() << "\n";

        totalTransactions += latencyEntry.second.size();
    }

    // Write Match latencies
    auto matchStats = computeStats(matchLatencies);
    file << "Match,-1, " << matchStats.first << "," << matchStats.second << "," << matchLatencies.size() << "\n";

    // Verify that the total count equals nUpdates
    std::cout << "\nTotal Transactions Counted: " << totalTransactions << " | Expected: " << nUpdates << "\n";
    if (nUpdates != -1 && totalTransactions != nUpdates)
        throw std::runtime_error("Mismatch in total number of updates!");

    std::cout << "Latency statistics written to " << filename << std::endl;
    file.close();
}

