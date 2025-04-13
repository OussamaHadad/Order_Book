#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <chrono>
#include <numeric>
#include <cassert>
#include <nlohmann/json.hpp>

#include "Order.cpp"
#include "OrderBook.cpp"

//  compile: cl.exe /I "C:/Users/oussa/Desktop/vcpkg/installed/x64-windows/include" /EHsc /Fe:test.exe test.cpp
//  execute: ./out:test.exe

using json = nlohmann::json;

// The following variables are made static to make then "non-importable" by other .h or .cpp files
static std::unordered_map<std::string, Type> _map_types = {{"GTC", Type::GTC}, {"FAK", Type::FAK}, {"FOK", Type::FOK}, {"GFD", Type::GFD}, {"M", Type::M}};
static std::unordered_map<std::string, Side> _map_sides = {{"Bid", Side::Bid}, {"Ask", Side::Ask}};

auto populateOrderBook(const std::string& inputFilename, OrderBook& orderBook) {
    /*
        Given a .json file where each element is an object:
        {"type": "GTC", "side": "Bid", "price": 32.5, "shares": 100} 
        This function populates the given orderBook with the orders from the JSON file.
    */

    std::ifstream inputFile(inputFilename);
    if (!inputFile.is_open()){
        std::cerr << "Error: Cannot open file " << inputFilename << '\n';
        return -1;
    }

    json inputFileOrders;
    try {
        inputFile >> inputFileOrders;
    } 
    catch (const json::parse_error& e){
        std::cerr << "Error: Failed to parse JSON file. " << e.what() << '\n';
        return -1;
    }

    int orderId = 1;

    for (const auto& orderEntry : inputFileOrders){
        try{
            std::string typeStr = orderEntry.at("type");
            std::string sideStr = orderEntry.at("side");
            double price = orderEntry.at("price");
            int shares = orderEntry.at("shares");

            auto order = std::make_shared<Order>(orderId, _map_types[typeStr], _map_sides[sideStr], price, shares);

            orderBook.addOrder(order);
            ++orderId;
        } 
        catch (const json::out_of_range& e){
            std::cerr << "Warning: Missing fields in order " << orderId << ". Skipping. " << e.what() << '\n';
        } 
        catch (const std::exception& e){
            std::cerr << "Warning: Failed to process order " << orderId << ". " << e.what() << '\n';
        }
    }

    inputFile.close();
    return orderId; // Id of the next order in case we add any
}


void updateOrderBook(OrderBook& orderBook, int nUpdates, uint32_t newOrderId,
                    double addProb = 0.3, double cancelProb = 0.1, double amendProb = 0.6, 
                    int meanShares = 50, double meanPrice = 30.00){
    /*
        Given an order book (non-empty), this method updates this order book by adding new orders, amending or cancelling existing orders
        given the input parameters.
        At the same time, the latency of each type of orders is being computed and reported to evaluate the performance of the implementation.
    */
    orderBook.clearLatencies();

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

    for (int i = 0; i < nUpdates; ++i){
        // Randomly choose action based on these probabilities
        double actionDecision = static_cast<double>(rand()) / RAND_MAX;

        if (actionDecision < addProb){  // Add order
            std::cout << "Add a new order" << std::endl;
            newOrderId += 1;    
            Type type = types[typeDist(gen)];
            Side side = sides[sideDist(gen)];
            double newPrice = std::max(1.0, priceDist(gen)); // Ensure price is positive
            int newShares = std::max(5, static_cast<int>(shareDist(gen))); // Ensure shares are positive

            auto newOrder = std::make_shared<Order> (newOrderId, type, side, newPrice, newShares);

            orderBook.addOrder(newOrder);
        }
        else if (actionDecision < addProb + amendProb){ // Amend order
            std::cout << "Modify existing order" << std::endl;
            uint32_t orderId = orderBook.getRandomOrderId();
            auto orderPtr = orderBook.getOrderPtr(orderId);
            double newPrice = std::max(1.0, priceDist(gen)); // Ensure price is positive
            int newShares = std::max(5, static_cast<int>(shareDist(gen))); // Ensure shares are positive
            
            (void) orderBook.amendOrder(orderPtr, newPrice, newShares);
        }
        else { // Cancel order
            std::cout << "Cancel existing order" << std::endl;
            uint32_t orderId = orderBook.getRandomOrderId();

            orderBook.cancelOrder(orderId);
        }    
    }
}


int main(){
    std::mt19937 rng(42);  // 42 is the seed

    std::string ordersFilename = "orders.json";
    std::string resultsFilename = "stats.json";
    size_t nUpdates = 100000;
    
    OrderBook orderBook;

    size_t nextOrderId = populateOrderBook(ordersFilename, orderBook);
    std::cout << "\n ******************** \n Order Book initialized and populated with " 
          << (nextOrderId - 1) 
          << " orders \n ********************  \n" << std::endl;
    //orderBook.printOrderBook();

    updateOrderBook(orderBook, nUpdates, nextOrderId);

    orderBook.writeLatencyStatsToFile(resultsFilename, nUpdates);
}

