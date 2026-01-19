#ifndef LOCALACTION_HPP
#define LOCALACTION_HPP

#include <cstdint>

enum class ActionType : uint8_t { 
    FOLD = 0, 
    CALL = 1,
    RAISE = 2,
    DISCARD = 3,
    // World actions
    DEAL_FLOP = 4,
    DEAL_TURN = 5,
    DEAL_RIVER = 6
};

struct LocalAction {
    ActionType type;
    uint64_t value;
    // value is either a raise amount in chips
    // or a card index 0-51
    // or a 2-card bitmask for the flop. 
    
    LocalAction() : type(ActionType::FOLD), value(0) {}
    
    LocalAction(ActionType t, uint64_t v = 0) : type(t), value(v) {}
    
    static LocalAction fold() {return LocalAction(ActionType::FOLD);}

    static LocalAction call() {return LocalAction(ActionType::CALL);}
    
    static LocalAction raise(uint64_t amount) {return LocalAction(ActionType::RAISE, amount);}
    
    static LocalAction raise(uint64_t call_amount, uint64_t raise_amount) {return LocalAction(ActionType::RAISE, call_amount + raise_amount);}
    
    static LocalAction discard(int card) {return LocalAction(ActionType::DISCARD, card);}
    
    static LocalAction dealFlop(int card1, int card2) {return LocalAction(ActionType::DEAL_FLOP, (1ull << card1) | (1ull << card2));}
    
    static LocalAction dealTurn(int card) {return LocalAction(ActionType::DEAL_TURN, card);}
    
    static LocalAction dealRiver(int card) {return LocalAction(ActionType::DEAL_RIVER, card);}
    
    bool operator==(const LocalAction& other) const {
        if (type != other.type) return false;

        if (type == ActionType::FOLD || type == ActionType::CALL) return true;
        return value == other.value;
    }
    
    bool operator!=(const LocalAction& other) const {return !(*this == other);}
};

#endif // LOCALACTION_HPP