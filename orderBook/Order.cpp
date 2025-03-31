#include "Order.h"



Order::Order(uint32_t _orderId, Type _type, Side _side, double _price, uint32_t _shares)
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

Order::Order(uint32_t _orderId, Type _type, Side _side, uint32_t _shares)  // Market order constructor
: orderId(_orderId), type(_type), side(_side)
{
    if (_shares == 0)
        throw std::invalid_argument(
            (std::ostringstream{} << "Order (" << getOrderId() << ") can't have zero shares").str()
        );
    init_shares = shares = _shares;
}


void Order::fillOrder(uint32_t tradedShares){
    if (tradedShares > shares)
        throw std::logic_error(
            (std::ostringstream{} << "Order (" << getOrderId() << ") can't be filled as the number of traded shares exceeds the remaining number of shares.").str()
        );
    
    shares -= tradedShares;
}

void Order::marketToGTC(double _price){
    // Turn a market order into a Good till Cancel order
    if (_price <= 0)
        throw std::invalid_argument(
            (std::ostringstream{} << "Order (" << getOrderId() << ") should have a strictly positive price").str()
        );
    
    price = _price;
    type = Type::GTC;
}