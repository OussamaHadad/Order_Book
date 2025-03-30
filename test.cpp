#include <string>
#include <fstream>
#include <iostream>
#include <memory>

#include "OrderBook.cpp"

void readOrdersFromFile(const std::string& inputFilename, OrderBook& orderBook){
    std::ifstream inputFile(inputFilename);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Cannot open file " << inputFilename << '\n';
        return;
    }
    
    std::string line;
    int orderId = 1;
    
    std::unordered_map<std::string, Type> map_types = {{"GTC", Type::GTC}, {"FAK", Type::FAK}, {"FOK", Type::FOK}, {"GFD", Type::GFD}, {"M", Type::M}};
    std::unordered_map<std::string, Side> map_sides = {{"Bid", Side::Bid}, {"Ask", Side::Ask}};

    while (std::getline(inputFile, line)){
        std::istringstream iss(line);

        std::string typeStr, sideStr;
        double price;
        int shares;
        
        if (!(iss >> typeStr >> sideStr >> price >> shares)){
            std::cerr << "Warning: Could not parse line: " << orderId << '\n';
            continue;
        }
        
        auto order = std::make_shared<Order>(orderId, map_types[typeStr], map_sides[sideStr], price, shares);
        orderBook.addOrder(order);
        ++orderId;
    }
    
    inputFile.close();
}


int main(){
    std::string filename = "orders.txt";
    OrderBook orderBook;

    readOrdersFromFile(filename, orderBook);

    orderBook.printOrderBook();

    return 0;
}