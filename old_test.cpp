// TO DO: Modify file

#include "Orderbook.h"


#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <memory>
#include <cassert>
#include <string_view>


enum class ActionType
{
    Add,
    Cancel,
    Modify,
};

struct Information
{
    ActionType type_;
    Type orderType_;
    Side side_;
    Price price_;
    Quantity quantity_;
    OrderId orderId_;
};

using Informations = std::vector<Information>;

struct Result
{
    std::size_t allCount_;
    std::size_t bidCount_;
    std::size_t askCount_;
};

using Results = std::vector<Result>;

class InputHandler
{
private:
    uint32_t ToNumber(const std::string_view& str) const{
        std::int64_t value{};
        std::from_chars(str.data(), str.data() + str.size(), value);
        if (value < 0)
            throw std::logic_error("Value is below zero.");
        return static_cast<std::uint32_t>(value);
    }
    

    bool TryParseResult(const std::string_view& str, Result& result) const{
        if (str.at(0) != 'R')
            return false;

        auto values = Split(str, ' ');
        result.allCount_ = ToNumber(values[1]);
        result.bidCount_ = ToNumber(values[2]);
        result.askCount_ = ToNumber(values[3]);

        return true;
    }


    bool TryParseInformation(const std::string_view &str, Information &action) const{
        auto value = str.at(0);
        auto values = Split(str, ' ');
        if (value == 'A'){
            action.type_ = ActionType::Add;
            action.side_ = ParseSide(values[1]);
            action.orderType_ = ParseOrderType(values[2]);
            action.price_ = ParsePrice(values[3]);
            action.quantity_ = ParseQuantity(values[4]);
            action.orderId_ = ParseOrderId(values[5]);
        }
        else if (value == 'M'){
            action.type_ = ActionType::Modify;
            action.orderId_ = ParseOrderId(values[1]);
            action.side_ = ParseSide(values[2]);
            action.price_ = ParsePrice(values[3]);
            action.quantity_ = ParseQuantity(values[4]);
        }
        else if (value == 'C'){
            action.type_ = ActionType::Cancel;
            action.orderId_ = ParseOrderId(values[1]);
        }
        else
            return false;

        return true;
    }


    std::vector<std::string_view> Split(const std::string_view &str, char delimiter) const{
        std::vector<std::string_view> columns;
        std::size_t start_index = 0, end_index;
        while ((end_index = str.find(delimiter, start_index)) != std::string::npos){
            columns.push_back(str.substr(start_index, end_index - start_index));
            start_index = end_index + 1;
        }
        columns.push_back(str.substr(start_index));
        return columns;
    }


    Side ParseSide(const std::string_view &str) const{
        if (str == 'B')
            return Side::Bid;

        else if (str == 'S')
            return Side::Ask;
        
            throw std::logic_error("Unknown Side");
    }


    Type ParseOrderType(const std::string_view &str) const{
        if (str == "FAK")
            return Type::FAK;
        else if (str == "GTC")
            return Type::GTC;
        else if (str == "GFD")
            return Type::GFD;
        else if (str == "FOK")
            return Type::FOK;
        else if (str == "M")
            return Type::M;
        
        throw std::logic_error("Unknown OrderType");
    }

    Price ParsePrice(const std::string_view &str) const{
        return ToNumber(str);
    }

    Quantity ParseQuantity(const std::string_view &str) const{
        return ToNumber(str);
    }

    OrderId ParseOrderId(const std::string_view &str) const{
        return static_cast<OrderId>(ToNumber(str));
    }

public:
    std::tuple<Informations, Result> GetInformations(const std::filesystem::path &path) const{
        Informations actions;
        std::ifstream file{path};
        std::string line;
        while (std::getline(file, line)){
            if (line.empty())
                break;

            if (line.at(0) == 'R'){
                Result result;
                if (TryParseResult(line, result))
                    return {actions, result};
            }
            else{
                Information action;
                if (TryParseInformation(line, action))
                    actions.push_back(action);
            }
        }
        throw std::logic_error("No result specified.");
    }
};

void RunTests(){
    std::vector<std::string> testFiles = {
        "Match_GoodTillCancel.txt",
        "Match_FillAndKill.txt",
        "Match_FillOrKill_Hit.txt",
        "Match_FillOrKill_Miss.txt",
        "Cancel_Success.txt",
        "Modify_Side.txt",
        "Match_Market.txt"};

    for (const auto &file : testFiles){
        std::cout << "Running test: " << file << std::endl;

        InputHandler handler;
        auto [actions, expected] = handler.GetInformations(file);

        OrderBook orderbook;
        for (const auto& action : actions){
            switch (action.type_){
            case ActionType::Add:
                orderbook.addOrder(std::make_shared<Order>(action.orderType_, action.orderId_, action.side_, action.price_, action.quantity_));
                break;
            case ActionType::Modify:
                orderbook.amendOrder({action.orderId_, action.side_, action.price_, action.quantity_});
                break;
            case ActionType::Cancel:
                orderbook.cancelOrder(action.orderId_);
                break;
            }
        }

        const auto &orderbookInfos = orderbook.GetOrderInfos();
        assert(orderbook.Size() == expected.allCount_);
        assert(orderbookInfos.GetBids().size() == expected.bidCount_);
        assert(orderbookInfos.GetAsks().size() == expected.askCount_);
        std::cout << "Test passed: " << file << "\n";
    }
}

int main()
{
    try{
        RunTests();
    }

    catch (const std::exception &e){
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}