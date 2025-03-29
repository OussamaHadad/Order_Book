#pragma once

#include <cstdint>
#include <vector>

struct LimitLevelInfo{
    double price;
    uint32_t totalShares;
};

class LimitLevel {
private:
    LimitLevelInfo bids;
    LimitLevelInfo asks;

public:
    // Constructor
    LimitLevel(LimitLevelInfo _bids, LimitLevelInfo _asks)
    : bids(_bids), asks(_asks)
    {}

    // Getters
    const LimitLevelInfo& getBids() const {return bids;}
    const LimitLevelInfo& getAsks() const {return asks;}
};

using LimitLevelInfos = std::vector<LimitLevelInfo>;