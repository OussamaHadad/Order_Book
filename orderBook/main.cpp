#include "OrderBook.cpp"

int main() {
    OrderBook orderBook;

    // Adding order 1
    auto order1 = std::make_shared<Order> (55, Type::GTC, Side::Bid, 50, 10);
    orderBook.addOrder(order1);
    orderBook.printOrderBook();
    std::cout << "  ************  ************    ************ \n" << std::endl;

    // Adding order 2
    auto order2 = std::make_shared<Order> (50, Type::GTC, Side::Ask, 40, 8);
    orderBook.addOrder(order2);
    orderBook.printOrderBook();
    std::cout << "  ************  ************    ************ \n" << std::endl;

    // Modifying order 1
    orderBook.amendOrder(order1, 48, 5);
    orderBook.printOrderBook();
    std::cout << "  ************  ************    ************ \n" << std::endl;

    // Cancelling order 1
    orderBook.cancelOrder(order1->getOrderId());
    orderBook.printOrderBook();
    
    return 0;
}

