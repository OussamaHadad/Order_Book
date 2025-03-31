# pragma once

#include "enums.h"

#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <memory>
#include <list>

class Order{
private:
    uint32_t orderId;
    Type type;
    Side side;
    double price;   // price should be int as it's used as a key for other maps
    uint32_t init_shares;    // the initial number of shares
    uint32_t shares;    // the current number of shares

public:
    // Constructors
    Order(uint32_t _orderId, Type _type, Side _side, double _price, uint32_t _shares); // Orders Constructor

    Order(uint32_t _orderId, Type _type, Side _side, uint32_t _shares);  // Market orders Constructor

    // Getters
    uint32_t getOrderId() const {return orderId;}
    Type getOrderType() const {return type;}
    Side getOrderSide() const {return side;} 
    double getOrderPrice() const {return price;}
    uint32_t getOrderInitialShares() const {return init_shares;}
    uint32_t getOrderShares() const {return shares;}

    // Other class methods
    bool isFilled() const {return (shares == 0);}

    void fillOrder(uint32_t tradedShares);

    void marketToGTC(double _price);
};

using OrderPointer = std::shared_ptr<Order>;

using OrderPointers = std::list<OrderPointer>;  // We use list not vector as list is a doubly linked list which makes operations at the front and back faster (O(1))
