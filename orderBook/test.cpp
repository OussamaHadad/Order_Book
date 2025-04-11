#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <chrono>
#include <numeric>
#include <cassert>

#include "Order.cpp"
#include "OrderBook.cpp"


// The following variables are made static to make then "non-importable" by other .h or .cpp files
static std::unordered_map<std::string, Type> _map_types = {{"GTC", Type::GTC}, {"FAK", Type::FAK}, {"FOK", Type::FOK}, {"GFD", Type::GFD}, {"M", Type::M}};
static std::unordered_map<std::string, Side> _map_sides = {{"Bid", Side::Bid}, {"Ask", Side::Ask}};

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

    while (std::getline(inputFile, line)){
        std::istringstream iss(line);

        std::string typeStr, sideStr;
        double price;
        int shares;
        
        if (!(iss >> typeStr >> sideStr >> price >> shares)){
            std::cerr << "Warning: Could not parse line: " << orderId << '\n';
            continue;   // Move to next order
        }
        
        auto order = std::make_shared<Order>(orderId, _map_types[typeStr], _map_sides[sideStr], price, shares);
        orderBook.addOrder(order);
        ++orderId;
    }
    
    inputFile.close();

    return orderId;
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

    std::string ordersFilename = "orders.txt";
    std::string resultsFilename = "stats.txt";
    size_t nUpdates = 1000;
    
    OrderBook orderBook;

    size_t nextOrderId = populateOrderBook(ordersFilename, orderBook);
    std::cout << "\n ******************** \n Order Book initialized and populated with " 
          << nextOrderId 
          << " orders \n ********************  \n" << std::endl;
    //orderBook.printOrderBook();

    updateOrderBook(orderBook, nUpdates, nextOrderId);

    orderBook.writeLatencyStatsToFile(resultsFilename, nUpdates);
}

