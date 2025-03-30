#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <chrono>
#include <numeric>

#include "OrderBook.cpp"

auto populateOrderBook(const std::string& inputFilename, OrderBook& orderBook){
    /*
        Given an .txt file where each line consists of an order (Type Side Price Shares) and an orderBook (usually empty)
        This function populates this orderBook with the orders figuring in the .txt file
    */
    std::ifstream inputFile(inputFilename);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Cannot open file " << inputFilename << '\n';
        return -1;
    }
    
    std::string line;
    int orderId = 1;
    
    // TO DO: remove; check addOrder() method
    std::unordered_map<std::string, Type> map_types = {{"GTC", Type::GTC}, {"FAK", Type::FAK}, {"FOK", Type::FOK}, {"GFD", Type::GFD}, {"M", Type::M}};
    std::unordered_map<std::string, Side> map_sides = {{"Bid", Side::Bid}, {"Ask", Side::Ask}};

    while (std::getline(inputFile, line)){
        std::istringstream iss(line);

        std::string typeStr, sideStr;
        double price;
        int shares;
        
        if (!(iss >> typeStr >> sideStr >> price >> shares)){
            std::cerr << "Warning: Could not parse line: " << orderId << '\n';
            continue;   // Move to next order
        }
        
        auto order = std::make_shared<Order>(orderId, map_types[typeStr], map_sides[sideStr], price, shares);
        orderBook.addOrder(order);
        ++orderId;
    }
    
    inputFile.close();

    return orderId;
}

    
double avg_latency(const std::vector<double>& latencies) {
    return latencies.empty() ? 0.0 : std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
}


void updateOrderBook(OrderBook& orderBook, int nUpdates, 
                    double addProb = 0.3, double cancelProb = 0.1, double amendProb = 0.6, 
                    int meanShares = 50, double meanPrice = 30.00){
    /*
        Given an order book (non-empty), this method updates this order book by adding new orders, amending or cancelling existing orders
        given the input parameters.
        At the same time, the latency of each type of orders is being computed and reported to evaluate the performance of the implementation.
    */

    // Assuming mean_shares and mean_price are defined elsewhere
    std::random_device rd;
    std::mt19937 gen(rd());

    // Normal distributions for shares and price
    std::normal_distribution<> shareDist(meanShares, 50); // Adjust standard deviation as needed
    std::normal_distribution<> priceDist(meanPrice, 10.0); // Adjust standard deviation as needed

    // Uniform distributions for Side and Type
    std::uniform_int_distribution<int> sideDist(0, 1); // 0 for Bid, 1 for Ask
    std::uniform_int_distribution<int> typeDist(0, 4); // 5 types indexed from 0 to 4

    Type types[] = {Type::GTC, Type::FAK, Type::FOK, Type::GFD, Type::M};
    Side sides[] = {Side::Bid, Side::Ask};

    // Latency measurements
    std::vector<double> addLatencies, amendLatencies, cancelLatencies;

    for (int i = 0; i < nUpdates; ++i){
        // Randomly choose action based on these probabilities
        double actionDecision = static_cast<double>(rand()) / RAND_MAX;

        if (actionDecision < addProb){  // Add order
            uint32_t newOrderId = orderBook.getNumberOfOrders() + 1;    
            Type type = types[typeDist(gen)];
            Side side = sides[sideDist(gen)];
            double newPrice = std::max(1.0, priceDist(gen)); // Ensure price is positive
            int newShares = std::max(5, static_cast<int>(shareDist(gen))); // Ensure shares are positive

            auto newOrder = std::make_shared<Order> (newOrderId, type, side, newPrice, newShares);


            auto start = std::chrono::high_resolution_clock::now();
            orderBook.addOrder(newOrder);                               // Add Order
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> latency = end - start;
            addLatencies.push_back(latency.count());
        }
        else if (actionDecision < addProb + amendProb){ // Amend order
            uint32_t orderId = orderBook.getRandomOrderId();
            auto orderPtr = orderBook.getOrderPtr(orderId);
            double newPrice = std::max(1.0, priceDist(gen)); // Ensure price is positive
            int newShares = std::max(5, static_cast<int>(shareDist(gen))); // Ensure shares are positive
            
            auto start = std::chrono::high_resolution_clock::now();
            auto unused = orderBook.amendOrder(orderPtr, newPrice, newShares);  // Amend order
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> latency = end - start;
            amendLatencies.push_back(latency.count());
        }
        else { // Cancel order
            uint32_t orderId = orderBook.getRandomOrderId();

            auto start = std::chrono::high_resolution_clock::now();
            orderBook.cancelOrder(orderId);                             // Cancel order
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> latency = end - start;
            cancelLatencies.push_back(latency.count());
        }    
    }

    std::cout << "Average Add Latency: " << avg_latency(addLatencies) << " µs\n";
    std::cout << "Average Amend Latency: " << avg_latency(amendLatencies) << " µs\n";
    std::cout << "Average Cancel Latency: " << avg_latency(cancelLatencies) << " µs\n";

}


int main(){
    std::string filename = "orders.txt";
    size_t nUpdates = 1000;
    OrderBook orderBook;

    size_t nextOrderId = populateOrderBook(filename, orderBook);
    //orderBook.printOrderBook();

    updateOrderBook(orderBook, nUpdates);
    //orderBook.printOrderBook();

    return 0;
}