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
    Order(uint32_t _orderId, Type _type, Side _side, double _price, uint32_t _shares)
    : orderId(_orderId), type(_type), side(_side)
    {
        if (_price <= 0)
            throw std::invalid_argument(
                (std::ostringstream{} << "Order (" << getOrderId() << ") should have a strictly positive price").str()
            );
        price = _price;
        
        if (_shares == 0)
            throw std::invalid_argument(
                (std::ostringstream{} << "Order (" << getOrderId() << ") can't have zero shares").str()
            );
        init_shares = shares = _shares;
    }

    Order(uint32_t _orderId, Type _type, Side _side, uint32_t _shares)  // Market order constructor
    : orderId(_orderId), type(_type), side(_side)
    {
        if (_shares == 0)
            throw std::invalid_argument(
                (std::ostringstream{} << "Order (" << getOrderId() << ") can't have zero shares").str()
            );
        init_shares = shares = _shares;
    }

    // Getters
    uint32_t getOrderId() const {return orderId;}
    Type getOrderType() const {return type;}
    Side getOrderSide() const {return side;} 
    double getOrderPrice() const {return price;}
    uint32_t getOrderInitialShares() const {return init_shares;}
    uint32_t getOrderShares() const {return shares;}

    // Other class methods
    bool isFilled() const {return (shares == 0);}

    void fillOrder(uint32_t tradedShares){
        if (tradedShares > shares)
            throw std::logic_error(
                (std::ostringstream{} << "Order (" << getOrderId() << ") can't be filled as the number of traded shares exceeds the remaining number of shares.").str()
            );
        
        shares -= tradedShares;
    }

    void marketToGTC(double _price){
        // Turn a market order into a Good till Cancel order
        if (_price <= 0)
            throw std::invalid_argument(
                (std::ostringstream{} << "Order (" << getOrderId() << ") should have a strictly positive price").str()
            );
        
        price = _price;
        type = Type::GTC;
    }
};

using OrderPointer = std::shared_ptr<Order>;

using OrderPointers = std::list<OrderPointer>;  // We use list not vector as list is a doubly linked list which makes operations at the front and back faster (O(1))
