#ifndef LOCALGAMESTATE_HPP
#define LOCALGAMESTATE_HPP

#include <cstdint>
#include <vector>
#include <cassert>
#include "LocalAction.hpp"

using namespace std;

enum class Street : uint8_t {
    PREFLOP = 0,
    DEAL_FLOP = 1,
    BB_DISCARD = 2,
    SB_DISCARD = 3,
    FLOP = 4,
    DEAL_TURN = 5,
    TURN = 6,
    DEAL_RIVER = 7,
    RIVER = 8
};

enum class Turn : int8_t {
    SB = 0,
    BB = 1,
    WORLD = -1
};

enum class Winner : int8_t {
    SHOWDOWN = 0, // or before showdown
    SB_WINS = 1,
    BB_WINS = -1
};

struct LocalGameState {
    // This alone uniquely identifies a game state.
    vector<LocalAction> history;
    
    Street street;
    uint16_t pot;
    uint16_t sb_stack;
    uint16_t bb_stack;
    uint16_t sb_bet;
    uint16_t bb_bet;
    Turn turn;
    
    // bitmask
    uint64_t board;
    
    int8_t turn_card;
    int8_t river_card;
    int8_t bb_discard;
    int8_t sb_discard;
    
    bool is_terminal;
    Winner winner;
    
    LocalGameState() :
        street(Street::PREFLOP),
        pot(3),
        sb_stack(399),
        bb_stack(398),
        sb_bet(1),
        bb_bet(2),
        turn(Turn::SB),
        board(0),
        turn_card(-1),
        river_card(-1),
        bb_discard(-1),
        sb_discard(-1),
        is_terminal(false),
        winner(Winner::SHOWDOWN)
    {}
    
    LocalGameState applyAction(const LocalAction& action) const {
        LocalGameState next = *this;
        next.history.push_back(action);
        
        // WORLD ACTIONS
        if (action.type == ActionType::DEAL_FLOP) {
            assert(street == Street::DEAL_FLOP && turn == Turn::WORLD);
            assert(__builtin_popcountll(action.value) == 2);  // Must be 2 cards
            next.board = action.value;
            next.street = Street::BB_DISCARD;
            next.turn = Turn::BB;
            return next;
        }
        
        if (action.type == ActionType::DEAL_TURN) {
            assert(street == Street::DEAL_TURN && turn == Turn::WORLD);
            next.turn_card = action.value;
            next.board |= (1ull << action.value);
            next.street = Street::TURN;
            next.turn = Turn::BB;
            return next;
        }
        
        if (action.type == ActionType::DEAL_RIVER) {
            assert(street == Street::DEAL_RIVER && turn == Turn::WORLD);
            next.river_card = action.value;
            next.board |= (1ull << action.value);
            next.street = Street::RIVER;
            next.turn = Turn::BB;
            return next;
        }
        
        // DISCARD ACTIONS
        if (action.type == ActionType::DISCARD) {
            assert(turn != Turn::WORLD);
            assert(street == Street::BB_DISCARD || street == Street::SB_DISCARD);
            int card = action.value;
            
            if (street == Street::BB_DISCARD) {
                assert(turn == Turn::BB);
                next.bb_discard = card;
                next.board |= (1ull << card);
                next.street = Street::SB_DISCARD;
                next.turn = Turn::SB;
            } else {
                assert(turn == Turn::SB);
                next.sb_discard = card;
                next.board |= (1ull << card);
                next.street = Street::FLOP;
                next.turn = Turn::BB;
                next.sb_bet = 0;
                next.bb_bet = 0;
            }
            return next;
        }
        
        // BETTING ACTIONS
        assert(turn != Turn::WORLD);
        int player = static_cast<int>(turn);
        
        if (action.type == ActionType::FOLD) {
            next.is_terminal = true;
            next.winner = (player == 0) ? Winner::BB_WINS : Winner::SB_WINS;
            int uncalled = (player == 0) ? (bb_bet - sb_bet) : (sb_bet - bb_bet);
            next.pot -= uncalled;
            return next;
        }
        
        if (action.type == ActionType::CALL) {
            int call_amount = (player == 0) ? (bb_bet - sb_bet) : (sb_bet - bb_bet);
            next.pot += call_amount;
            
            if (player == 0) {
                next.sb_stack -= call_amount;
                next.sb_bet += call_amount;
            } else {
                next.bb_stack -= call_amount;
                next.bb_bet += call_amount;
            }
            
            bool closes = true;
            
            // CHECKS
            if (call_amount == 0) {
                if (street != Street::PREFLOP && player == 1) {
                    closes = false;
                    next.turn = Turn::SB;
                }
            } else {
                if (street == Street::PREFLOP && player == 0 && next.sb_bet == 2) {
                    closes = false;
                    next.turn = Turn::BB;
                }
            }
            
            if (closes) {
                next.sb_bet = 0;
                next.bb_bet = 0;
                
                if (street == Street::RIVER) {
                    next.is_terminal = true;
                    next.winner = Winner::SHOWDOWN;
                } else if (street == Street::PREFLOP) {
                    next.street = Street::DEAL_FLOP;
                    next.turn = Turn::WORLD;
                } else if (street == Street::FLOP) {
                    next.street = Street::DEAL_TURN;
                    next.turn = Turn::WORLD;
                } else if (street == Street::TURN) {
                    next.street = Street::DEAL_RIVER;
                    next.turn = Turn::WORLD;
                }
            }
            return next;
        }
        
        if (action.type == ActionType::RAISE) {
            // note that raise amount refers to call amount + raise amount.
            int raise_amount = action.value;
            next.pot += raise_amount;
            
            if (player == 0) {
                next.sb_stack -= raise_amount;
                next.sb_bet += raise_amount;
            } else {
                next.bb_stack -= raise_amount;
                next.bb_bet += raise_amount;
            }
            
            next.turn = (player == 0) ? Turn::BB : Turn::SB;
            return next;
        }
        
        assert(false);  // Should never reach
        return next;
    }
};

#endif // LOCALGAMESTATE_HPP