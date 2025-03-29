#pragma once

#include <cstdint>
#include <vector>

struct TradeInfo{
    uint32_t orderId;
    double price;
    uint32_t shares;
};

class Trade{
private:
    TradeInfo bidTrade;
    TradeInfo askTrade;

public:
    // Constructor
    Trade(const TradeInfo& _bidTrade, const TradeInfo& _askTrade)
    : bidTrade(_bidTrade), askTrade(_askTrade)
    {}

    // Getters
    TradeInfo getBidTrade() const {return bidTrade;}
    TradeInfo getAskTrade() const {return askTrade;}

    // Method to get trade details as a string
    void getTradeDetails() const {
        std::cout   << "  Bid ID = " << bidTrade.orderId
                    << ", Ask ID = " << askTrade.orderId
                    << ", Shares = " << bidTrade.shares << std::endl;
    } 
};

using Trades = std::vector<Trade>;