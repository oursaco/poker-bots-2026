#ifndef LOCALACTION_HPP
#define LOCALACTION_HPP

#include <cstdint>

enum class ActionType : uint8_t { 
    FOLD = 0, 
    CALL = 1,    // also check
    RAISE = 2,   // value = raise amount added to pot
    DISCARD = 3  // value = card index 0-2 (ordered by equity)
};

struct LocalAction {
    ActionType type;
    uint16_t value;  // raise amount for RAISE, card index for DISCARD, unused otherwise
    
    LocalAction() : type(ActionType::FOLD), value(0) {}
    
    LocalAction(ActionType t, uint16_t v = 0) : type(t), value(v) {}
    
    bool operator==(const LocalAction& other) const {
        return type == other.type && value == other.value;
    }
    
    bool operator!=(const LocalAction& other) const {
        return !(*this == other);
    }
};

#endif // LOCALACTION_HPP
