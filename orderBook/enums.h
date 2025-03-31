#pragma once

#include <cstdint>

enum class Type {GTC = 0, FAK, FOK, GFD, M}; // GTC: GoodTillCancel, FAK: FillAndKill, FOK: FillOrKill, GFD: GoodForDay, M: Market

enum class Side {Bid = 0, Ask};

enum class Action {Add = 0, Remove, Match}; // Used to determine how the limit level should be updated

using Price = double;

using Quantity = uint32_t;

using OrderId = uint32_t;

