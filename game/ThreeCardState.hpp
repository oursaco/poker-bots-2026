#ifndef THREECARDSTATE_HPP
#define THREECARDSTATE_HPP

#include <string>
#include "game/GameTree.hpp"

struct ThreeCardAction : Action {
    string action;
    int turn;
    int amount;
    int sb_stack;
    int bb_stack;
    float pot_size;

    ThreeCardAction(string action_, int turn_, int amount_, int sb_stack_, int bb_stack_, float pot_size_){
        action = action_;
        turn = turn_;
        amount = amount_;
        sb_stack = sb_stack_;
        pot_size = pot_size_;
        bb_stack = bb_stack_;
    }

    string toString(){
        if(turn == 0){
            return "sb: " + action + " " + to_string(amount) + " (" + to_string(pot_size) + ")" + " | sb stack: " + to_string(sb_stack) + " | bb stack: " + to_string(bb_stack);
        } else if (turn == 1) {
            return "bb: " + action + " " + to_string(amount) + " (" + to_string(pot_size) + ")" + " | sb stack: " + to_string(sb_stack) + " | bb stack: " + to_string(bb_stack);
        } else {
            return "world: " + action;
        }
    }

    bool isWorldAction(){
        return turn == -1;
    }
};

struct ThreeCardGameState : GameState {

    // 0: preflop, 1: flop, 2: turn, 3: river
    int street;
    int pot;
    int sb_stack, bb_stack;
    int sb_bet, bb_bet;
    int action_depth;
    //0: player0, 1: player1, -1: world
    //player 0 is sb
    int turn;
    // 0: no winner, 1: sb wins, -1: bb wins
    // change in pnl is winner * pot, so sb is positive bb is negative
    int winner;
    bool showdown;

    ThreeCardGameState(){
        pot = 3;
        sb_stack = 399;
        bb_stack = 398;
        sb_bet = 1;
        bb_bet = 2;
        winner = 0;
        showdown = false;
        turn = -1;
        action_depth = 0;
        street = 0;
    }

    ThreeCardGameState(const ThreeCardGameState& other) = default;

    bool isTerminal(){
        return winner != 0 || showdown;
    }

    string getWinner(){
        return (winner == 1 ? "sb" : (winner == -1 ? "bb" : "none"));
    } 

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_fold(){
        auto fold = make_unique<ThreeCardGameState>(*this);  
        fold->winner = -1;
        fold->pot -= fold->bb_bet - fold->sb_bet;
        assert(fold->sb_bet < fold->bb_bet);
        return {std::move(fold), make_unique<ThreeCardAction>("fold", 0, 0, fold->sb_stack, fold->bb_stack, 0.0f)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_fold(){
        auto fold = make_unique<ThreeCardGameState>(*this);  
        fold->winner = 1;
        fold->pot -= fold->sb_bet - fold->bb_bet;
        assert(fold->bb_bet < fold->sb_bet);
        return {std::move(fold), make_unique<ThreeCardAction>("fold", 1, 0, fold->sb_stack, fold->bb_stack, 0.0f)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_call(){
        auto call = make_unique<ThreeCardGameState>(*this);  
        assert(call->sb_bet < call->bb_bet || call->bb_bet == 0);
        int amount = call->bb_bet - call->sb_bet;
        call->pot += amount;
        call->sb_stack -= amount;
        if(street == 0 && sb_bet == 1){
            call->sb_bet++;
            call->action_depth++;
            call->turn = 1;
        } else if(street == 6){
            call->winner = 0;
            call->showdown = true;
        } else {
            call->street++;
            call->sb_bet = call->bb_bet = 0;
            call->turn = -1;
            call->action_depth = 0;
        }
        return {std::move(call), make_unique<ThreeCardAction>("call", 0, amount, call->sb_stack, call->bb_stack, 0.0f)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_call(){
        auto call = make_unique<ThreeCardGameState>(*this);  
        assert(call->bb_bet < call->sb_bet || call->sb_bet == 0 || (street == 0 && call->sb_bet == 2));
        int amount = call->sb_bet - call->bb_bet;
        call->pot += amount;
        call->bb_stack -= amount;
        if(sb_bet == 0){
            call->turn = 0;
            call->action_depth++;
        } else if(street == 6){
            call->winner = 0;
            call->showdown = true;
        } else {
            call->street++;
            call->sb_bet = call->bb_bet = 0;
            call->turn = -1;
            call->action_depth = 0;
        }
        return {std::move(call), make_unique<ThreeCardAction>("call", 1, amount, call->sb_stack, call->bb_stack, 0.0f)};
    }

    float raiseSizeRelativeToPot(int amount){
        int bet_diff = max(sb_bet, bb_bet) - min(sb_bet, bb_bet);
        return float(amount - bet_diff)/float(pot + bet_diff);
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_raise(int amount){
        auto raise = make_unique<ThreeCardGameState>(*this);  
        assert(raise->sb_bet < raise->bb_bet || raise->bb_bet == 0);
        assert(raise->sb_bet + amount > raise->bb_bet);
        assert(raise->sb_stack >= amount);
        raise->pot += amount;
        raise->sb_stack -= amount;
        raise->sb_bet += amount;
        raise->turn = 1;
        raise->action_depth = action_depth + 1;
        return {std::move(raise), make_unique<ThreeCardAction>("raise", 0, amount, raise->sb_stack, raise->bb_stack, raiseSizeRelativeToPot(amount))};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_raise(int amount){
        auto raise = make_unique<ThreeCardGameState>(*this);  
        assert(raise->bb_bet < raise->sb_bet || raise->sb_bet == 0 || (street == 0 && raise->sb_bet == 2));
        assert(raise->bb_bet + amount > raise->sb_bet);
        assert(raise->bb_stack >= amount);
        raise->pot += amount;
        raise->bb_stack -= amount;
        raise->bb_bet += amount;
        raise->turn = 0;
        raise->action_depth = action_depth + 1;
        return {std::move(raise), make_unique<ThreeCardAction>("raise", 1, amount, raise->sb_stack, raise->bb_stack, raiseSizeRelativeToPot(amount))};
    }

    int pot_raise_size(){
        return pot + 2*(max(sb_bet, bb_bet) - min(sb_bet, bb_bet));
    }

    int half_pot_raise_size(){
        int bet_diff = max(sb_bet, bb_bet) - min(sb_bet, bb_bet);
        return (pot + bet_diff)/2 + bet_diff;
    }

    int double_pot_raise_size(){
        int bet_diff = max(sb_bet, bb_bet) - min(sb_bet, bb_bet);
        return (pot + bet_diff)*2 + bet_diff;
    }

    bool valid_sb_raise(int amount){
        return amount < sb_stack && amount >= 2*bb_bet && amount + sb_bet - bb_bet < bb_stack;
    }

    bool valid_bb_raise(int amount){
        return amount < bb_stack && amount >= 2*sb_bet && amount + bb_bet - sb_bet < sb_stack;
    }

    vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> generateStreetActions() {
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions;
        if(turn == 0){
            if(bb_bet > 0){
                actions.push_back(sb_fold());
            }
            actions.push_back(sb_call());
            vector<int> raise_sizes;
            if(bb_bet > sb_bet){
                raise_sizes = {pot_raise_size()};
            } else {
                raise_sizes = {half_pot_raise_size(), double_pot_raise_size()};
            }
            for(int size : raise_sizes){
                if(valid_sb_raise(size) && action_depth < 4){
                    actions.push_back(sb_raise(size));
                }
            }
            if(bb_stack > 0){
                actions.push_back(sb_raise(sb_stack));
            }
        } else if(turn == 1){
            if(sb_bet > 0 && !(street == 0 && sb_bet == 2)){
                actions.push_back(bb_fold());
            }
            actions.push_back(bb_call());
            vector<int> raise_sizes;
            if(sb_bet > bb_bet){
                raise_sizes = {pot_raise_size()};
            } else {
                raise_sizes = {half_pot_raise_size(), pot_raise_size(), double_pot_raise_size()};
            }
            for(int size : raise_sizes){
                if(valid_bb_raise(size) && action_depth < 4){
                    actions.push_back(bb_raise(size));
                }
            }
            if(sb_stack > 0){
                actions.push_back(bb_raise(bb_stack));
            }
        }
        return actions;
    }

    vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> generateActions() override {
        if(isTerminal()) return {};
        if(turn == -1){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions;
            string action_name = "";
            auto next_state = make_unique<ThreeCardGameState>(*this);
            if(street == 0){
                action_name = "deal hole cards";
                next_state->turn = 0;
            } else if(street == 1){
                action_name = "deal flop";
                next_state->turn = 1;
                next_state->street++;
            } else if(street == 5){
                action_name = "deal turn";
                next_state->turn = 1;
            } else if(street == 6){
                action_name = "deal river";
                next_state->turn = 1;
            } else {
                assert(false);
            }
            auto action = make_unique<ThreeCardAction>(action_name, -1, 0, sb_stack, bb_stack, 0.0f);
            actions.push_back({std::move(next_state), std::move(action)});
            return actions;
        }
        if(street == 2){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions;
            for(int i = 1; i <= 3; i++){
                auto next_state = make_unique<ThreeCardGameState>(*this);
                next_state->turn = 0;
                next_state->street++;
                actions.push_back({std::move(next_state), make_unique<ThreeCardAction>("discard", 1, i - 1, sb_stack, bb_stack, 0.0f)});
            }
            return actions;
        }
        if(street == 3){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions;
            for(int i = 1; i <= 3; i++){
                auto next_state = make_unique<ThreeCardGameState>(*this);
                next_state->turn = 1;
                next_state->street++;
                actions.push_back({std::move(next_state), make_unique<ThreeCardAction>("discard", 0, i - 1, sb_stack, bb_stack, 0.0f)});
            }
            return actions;
        }
        return generateStreetActions();
    }
};

#endif // THREECARDSTATE_HPP
