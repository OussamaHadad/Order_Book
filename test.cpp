#include "OrderBook.h"
#include <random>
#include <chrono>
#include <algorithm>

void generate_orders(int nOrders, float amendRatio, float cancelRatio, float sideRatio, std::vector<float> typeRatios) {
    // Input validation
    if (nOrders <= 0) throw std::invalid_argument("Number of orders must be positive");
    if (amendRatio < 0 || amendRatio > 1) throw std::invalid_argument("Amend ratio must be between 0 and 1");
    if (cancelRatio < 0 || cancelRatio > 1) throw std::invalid_argument("Cancel ratio must be between 0 and 1");
    if (sideRatio < 0 || sideRatio > 1) throw std::invalid_argument("Side ratio must be between 0 and 1");
    
    // Setup random number generation
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    
    // Distributions for different parameters
    std::uniform_real_distribution<double> price_dist(1.0, 100.0);  // Price between 1 and 100
    std::uniform_int_distribution<uint32_t> shares_dist(1, 1000);   // Shares between 1 and 1000
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);     // Probability distribution

    OrderBook orderBook;
    std::vector<OrderPointer> orders;
    uint32_t orderId = 0;

    // Generate initial orders
    for (int i = 0; i < nOrders; ++i) {
        // Determine order side
        Side side = (prob_dist(generator) < sideRatio) ? Side::Bid : Side::Ask;
        
        // Determine order type based on typeRatios
        double typeProb = prob_dist(generator);
        Type type;
        double cumulative = 0.0;
        for (size_t j = 0; j < typeRatios.size(); ++j) {
            cumulative += typeRatios[j];
            if (typeProb <= cumulative) {
                type = static_cast<Type>(j);
                break;
            }
        }

        // Generate order parameters
        double price = price_dist(generator);
        uint32_t shares = shares_dist(generator);

        // Create and add order
        auto order = std::make_shared<Order>(orderId++, type, side, price, shares);
        orders.push_back(order);
        orderBook.addOrder(order);

        // Process potential amendments
        if (!orders.empty() && prob_dist(generator) < amendRatio) {
            size_t idx = std::uniform_int_distribution<size_t>(0, orders.size() - 1)(generator);
            double newPrice = price_dist(generator);
            uint32_t newShares = shares_dist(generator);
            orderBook.amendOrder(orders[idx], newPrice, newShares);
        }

        // Process potential cancellations
        if (!orders.empty() && prob_dist(generator) < cancelRatio) {
            size_t idx = std::uniform_int_distribution<size_t>(0, orders.size() - 1)(generator);
            orderBook.cancelOrder(orders[idx]->getOrderId());
            orders.erase(orders.begin() + idx);
        }

        // Print order book state periodically
        if (i % (nOrders/10) == 0) {
            std::cout << "\nOrder Book State after " << i << " orders:" << std::endl;
            orderBook.printOrderBook();
        }
    }

    // Print final state
    std::cout << "\nFinal Order Book State:" << std::endl;
    orderBook.printOrderBook();
}

// Example usage:
int main() {
    // Example: Generate 100 orders with:
    // - 20% chance of amendment
    // - 10% chance of cancellation
    // - 60% chance of bid orders
    // - Type distribution: 40% GTC, 30% FAK, 20% FOK, 10% Market
    std::vector<float> typeRatios = {0.4, 0.3, 0.2, 0.1};  // GTC, FAK, FOK, Market
    generate_orders(100, 0.2, 0.1, 0.6, typeRatios);
    return 0;
}