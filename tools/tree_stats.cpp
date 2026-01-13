#include "game/ThreeCardState.hpp"
#include <iostream>
#include <map>
#include <iomanip>
#include <algorithm>
using namespace std;

struct TreeStats {
    size_t total_nodes = 0;
    size_t terminal_nodes = 0;
    size_t decision_nodes = 0;
    size_t world_nodes = 0;
    size_t sb_nodes = 0;
    size_t bb_nodes = 0;
    size_t max_depth = 0;
    map<int, size_t> nodes_per_street;
    map<string, size_t> terminal_reasons;
    map<int, size_t> spr_histogram;  // SPR bucket -> count (overall)
    map<int, map<int, size_t>> spr_histogram_per_street;  // street -> (SPR bucket -> count)
};


struct TestState : GameState {

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

    TestState(){
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

    TestState(const TestState& other) = default;

    bool isTerminal(){
        return winner != 0 || showdown;
    }

    string getWinner(){
        return (winner == 1 ? "sb" : (winner == -1 ? "bb" : "none"));
    } 

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_fold(){
        auto fold = make_unique<TestState>(*this);  
        fold->winner = -1;
        fold->pot -= fold->bb_bet - fold->sb_bet;
        assert(fold->sb_bet < fold->bb_bet);
        return {std::move(fold), make_unique<ThreeCardAction>("fold", 0, 0, fold->sb_stack, fold->bb_stack, 0.0f)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_fold(){
        auto fold = make_unique<TestState>(*this);  
        fold->winner = 1;
        fold->pot -= fold->sb_bet - fold->bb_bet;
        assert(fold->bb_bet < fold->sb_bet);
        return {std::move(fold), make_unique<ThreeCardAction>("fold", 1, 0, fold->sb_stack, fold->bb_stack, 0.0f)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_call(){
        auto call = make_unique<TestState>(*this);  
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
        auto call = make_unique<TestState>(*this);  
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
        auto raise = make_unique<TestState>(*this);  
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
        auto raise = make_unique<TestState>(*this);  
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

    int min_click_size(){
        int bet_diff = max(sb_bet, bb_bet) - min(sb_bet, bb_bet);
        return bet_diff + max(sb_bet, bb_bet);
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
                if(action_depth == 1 && street != 0) raise_sizes.push_back(min_click_size());
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
            if(sb_bet > bb_bet || (street == 0 && sb_bet == 2)){
                raise_sizes = {pot_raise_size()};
                if(action_depth == 1 && street != 0) raise_sizes.push_back(min_click_size());
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
            auto next_state = make_unique<TestState>(*this);
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
                auto next_state = make_unique<TestState>(*this);
                next_state->turn = 0;
                next_state->street++;
                actions.push_back({std::move(next_state), make_unique<ThreeCardAction>("discard", 1, i - 1, sb_stack, bb_stack, 0.0f)});
            }
            return actions;
        }
        if(street == 3){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions;
            for(int i = 1; i <= 3; i++){
                auto next_state = make_unique<TestState>(*this);
                next_state->turn = 1;
                next_state->street++;
                actions.push_back({std::move(next_state), make_unique<ThreeCardAction>("discard", 0, i - 1, sb_stack, bb_stack, 0.0f)});
            }
            return actions;
        }
        return generateStreetActions();
    }
};


void generateTreeRecursive(TestState& state, TreeStats& stats, size_t depth) {
    stats.total_nodes++;
    stats.max_depth = max(stats.max_depth, depth);
    stats.nodes_per_street[state.street]++;

    // Calculate SPR (stack-to-pot ratio) using effective stack
    int effective_stack = min(state.sb_stack, state.bb_stack);
    int spr_bucket = (state.pot > 0) ? ((effective_stack  + state.pot - 1)/ state.pot) : 0;
    stats.spr_histogram[spr_bucket]++;
    stats.spr_histogram_per_street[state.street][spr_bucket]++;

    if (state.isTerminal()) {
        stats.terminal_nodes++;
        if (state.showdown) {
            stats.terminal_reasons["showdown"]++;
        } else if (state.winner == 1) {
            stats.terminal_reasons["sb_wins_fold"]++;
        } else if (state.winner == -1) {
            stats.terminal_reasons["bb_wins_fold"]++;
        }
        return;
    }

    // Count node types
    if (state.turn == -1) {
        stats.world_nodes++;
    } else if (state.turn == 0) {
        stats.sb_nodes++;
        stats.decision_nodes++;
    } else if (state.turn == 1) {
        stats.bb_nodes++;
        stats.decision_nodes++;
    }

    // Generate all actions and recurse
    auto actions = state.generateActions();
    for (auto& [next_state, action] : actions) {
        TestState* next = dynamic_cast<TestState*>(next_state.get());
        if (next) {
            generateTreeRecursive(*next, stats, depth + 1);
        }
    }
}

void printHistogram(const map<int, size_t>& histogram, const string& title, size_t max_width = 50) {
    cout << title << endl;
    
    if (histogram.empty()) {
        cout << "  (no data)" << endl;
        return;
    }
    
    // Find max count for scaling
    size_t max_count = 0;
    for (const auto& [bucket, count] : histogram) {
        max_count = max(max_count, count);
    }
    
    // Print histogram
    for (const auto& [bucket, count] : histogram) {
        size_t bar_len = (max_count > 0) ? (count * max_width / max_count) : 0;
        cout << "  SPR " << setw(3) << bucket << " | ";
        cout << string(bar_len, '#');
        cout << " " << count << endl;
    }
}

void printStats(const TreeStats& stats) {
    cout << "=== Game Tree Statistics ===" << endl;
    cout << endl;
    cout << "Node Counts:" << endl;
    cout << "  Total nodes:    " << stats.total_nodes << endl;
    cout << "  Terminal nodes: " << stats.terminal_nodes << endl;
    cout << "  Decision nodes: " << stats.decision_nodes << endl;
    cout << "  World nodes:    " << stats.world_nodes << endl;
    cout << "  SB nodes:       " << stats.sb_nodes << endl;
    cout << "  BB nodes:       " << stats.bb_nodes << endl;
    cout << endl;
    cout << "Max depth: " << stats.max_depth << endl;
    cout << endl;
    cout << "Nodes per street:" << endl;
    for (const auto& [street, count] : stats.nodes_per_street) {
        cout << "  Street " << street << ": " << count << endl;
    }
    cout << endl;
    cout << "Terminal reasons:" << endl;
    for (const auto& [reason, count] : stats.terminal_reasons) {
        cout << "  " << reason << ": " << count << endl;
    }
    cout << endl;
    printHistogram(stats.spr_histogram, "Stack-to-Pot Ratio Histogram (Overall):");
    
    cout << endl;
    cout << "=== SPR Histograms by Street ===" << endl;
    for (const auto& [street, histogram] : stats.spr_histogram_per_street) {
        cout << endl;
        printHistogram(histogram, "Street " + to_string(street) + ":");
    }
}

int main() {
    cout << "Generating game tree..." << endl;
    
    TestState initial_state;
    TreeStats stats;
    
    generateTreeRecursive(initial_state, stats, 0);
    
    cout << endl;
    printStats(stats);
    
    return 0;
}
