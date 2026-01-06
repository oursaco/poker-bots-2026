#ifndef THREECARD_HPP
#define THREECARD_HPP

#include "GameTree.hpp"
#include "external/omp/Random.h"
#include "external/omp/HandEvaluator.h"
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <cassert>
#include <iostream>

#include "constants/constants.h"
#include <algorithm>
#include <map>
#include "game/ThreeCardBucket.hpp"
using namespace std;

struct ThreeCardAction : Action {
    string action;
    int turn;
    int amount;
    int sb_stack;
    int bb_stack;

    ThreeCardAction(string action_, int turn_, int amount_, int sb_stack_, int bb_stack_){
        action = action_;
        turn = turn_;
        amount = amount_;
        sb_stack = sb_stack_;
        bb_stack = bb_stack_;
    }

    string toString(){
        if(turn == 0){
            return "sb: " + action + " " + to_string(amount) + " | sb stack: " + to_string(sb_stack) + " | bb stack: " + to_string(bb_stack);
        } else if (turn == 1) {
            return "bb: " + action + " " + to_string(amount) + " | sb stack: " + to_string(sb_stack) + " | bb stack: " + to_string(bb_stack);
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
        return {std::move(fold), make_unique<ThreeCardAction>("sb fold", 0, 0, fold->sb_stack, fold->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_fold(){
        auto fold = make_unique<ThreeCardGameState>(*this);  
        fold->winner = 1;
        fold->pot -= fold->sb_bet - fold->bb_bet;
        assert(fold->bb_bet < fold->sb_bet);
        return {std::move(fold), make_unique<ThreeCardAction>("bb fold", 1, 0, fold->sb_stack, fold->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_call(){
        auto call = make_unique<ThreeCardGameState>(*this);  
        assert(call->sb_bet < call->bb_bet || call->bb_bet == 0);
        int amount = call->bb_bet - call->sb_bet;
        call->pot += amount;
        call->sb_stack -= amount;
        if(bb_bet == 0){
            call->turn = 1;
            call->action_depth++;
        } else if(street == 3){
            call->winner = 0;
            call->showdown = true;
        } else {
            call->street++;
            call->sb_bet = call->bb_bet = 0;
            call->turn = -1;
            call->action_depth = 0;
        }
        return {std::move(call), make_unique<ThreeCardAction>("sb call", 0, amount, call->sb_stack, call->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_call(){
        auto call = make_unique<ThreeCardGameState>(*this);  
        assert(call->bb_bet < call->sb_bet || call->sb_bet == 0);
        int amount = call->sb_bet - call->bb_bet;
        call->pot += amount;
        call->bb_stack -= amount;
        if(street == 3){
            call->winner = 0;
            call->showdown = true;
        } else {
            call->street++;
            call->sb_bet = call->bb_bet = 0;
            call->turn = -1;
            call->action_depth = 0;
        }
        return {std::move(call), make_unique<ThreeCardAction>("bb call", 1, amount, call->sb_stack, call->bb_stack)};
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
        return {std::move(raise), make_unique<ThreeCardAction>("sb raise", 0, amount, raise->sb_stack, raise->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_raise(int amount){
        auto raise = make_unique<ThreeCardGameState>(*this);  
        assert(raise->bb_bet < raise->sb_bet || raise->sb_bet == 0);
        assert(raise->bb_bet + amount > raise->sb_bet);
        assert(raise->bb_stack >= amount);
        raise->pot += amount;
        raise->bb_stack -= amount;
        raise->bb_bet += amount;
        raise->turn = 0;
        raise->action_depth = action_depth + 1;
        return {std::move(raise), make_unique<ThreeCardAction>("bb raise", 1, amount, raise->sb_stack, raise->bb_stack)};
    }

    int pot_raise_size(){
        return pot + 2*(max(sb_bet, bb_bet) - min(sb_bet, bb_bet));
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
            if(valid_sb_raise(pot_raise_size()) && action_depth < 3){
                actions.push_back(sb_raise(pot_raise_size()));
            }
            if(bb_stack > 0){
                actions.push_back(sb_raise(sb_stack));
            }
        } else if(turn == 1){
            if(sb_bet > 0){
                actions.push_back(bb_fold());
            }
            actions.push_back(bb_call());
            if(valid_bb_raise(pot_raise_size()) && action_depth < 3){
                actions.push_back(bb_raise(pot_raise_size()));
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
            if(street == 0){
                action_name = "deal hole cards";
            } else if(street == 1){
                action_name = "deal flop";
            } else if(street == 2){
                action_name = "deal turn";
            } else if(street == 3){
                action_name = "deal river";
            } else {
                assert(false);
            }
            auto action = make_unique<ThreeCardAction>(action_name, -1, 0, sb_stack, bb_stack);
            auto next_state = make_unique<ThreeCardGameState>(*this);
            next_state->turn = 0;
            actions.push_back({std::move(next_state), std::move(action)});
            return actions;
        }
        return generateStreetActions();
    }
};

struct ThreeCardGameTree : GameTree {

    struct Node {
        int parent;
        int move;
        int turn;
        int pot;
        int new_cards;
        int size;
        int prv_new_card;
        int info_set;
        int winner;
        int info_set_map;
    };

    array<Node, TREE_SZ> nodes;
    vector<int> winner;
    vector<int> new_card_states;
    array<int, TREE_SZ> children;
    array<int, TREE_SZ> buckets;
    vector<int> moves_per_info_set;
    vector<int> preflop;
    vector<int> leaves;
    vector<uint64_t> board;
    vector<uint64_t> used_mask;
    vector<int> parent_board;
    omp::HandEvaluator hand_eval;
    int tree_index = 0;

    int generateTree(ThreeCardGameState* root, int par = -1, int par_move = -1, int prv_new_card = -1, int new_cards = 0){
        if(root->turn == -1){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
            assert(actions.size() == 1);
            if(root->street == 1){
                new_cards = 2;
            } else if(root->street == 2){
                new_cards = 1;
            } else if(root->street == 3){
                new_cards = 1;
            }
            int node_id = generateTree(dynamic_cast<ThreeCardGameState*>(actions.front().first.get()), par, par_move, prv_new_card, new_cards);
            new_card_states.push_back(node_id);
            return node_id;
        }
        int node_id = tree_index;
        if(root->street == 0){
            buckets[tree_index] = three_card::count_preflop_buckets();
            preflop.push_back(node_id);
        } else if(root->street == 1){
            buckets[tree_index] = three_card::count_flop_buckets();
        } else if(root->street == 2){
            buckets[tree_index] = three_card::count_turn_buckets();
        } else if(root->street == 3){
            buckets[tree_index] = three_card::count_river_buckets();
        } else {
            assert(false);
        }
        children[tree_index] = 0;
        int size = 0;
        nodes[tree_index] = {par, par_move, root->turn, root->pot, new_cards, 0, prv_new_card};
        tree_index++;
        if(new_cards > 0) prv_new_card = node_id;
        if(root->isTerminal()){
            leaves.push_back(node_id);
            winner.push_back(root->winner);
            return node_id;
        }
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
        for(auto& action : actions){
            ThreeCardGameState* next_state = dynamic_cast<ThreeCardGameState*>(action.first.get());
            int move_id = static_cast<int>(children[node_id]);
            int child_id = generateTree(next_state, node_id, move_id, prv_new_card);
            children[node_id]++;
            size += nodes[child_id].size + 1;
        }
        nodes[node_id].size = size;
        return node_id;
    }

    // initializes the game tree
    void init(){
        assert(tree_index == 0);
        ThreeCardGameState root;
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root.generateActions();
        assert(actions.size() == 1);
        generateTree(dynamic_cast<ThreeCardGameState*>(actions.front().first.get()));
        sort(new_card_states.begin(), new_card_states.end());
        board.resize(new_card_states.size());
        used_mask.resize(new_card_states.size(), 0);
        parent_board.resize(new_card_states.size(), 0);
        map<int, int> loc;
        parent_board[0] = -1;
        loc[new_card_states[0]] = 0;
        for(int i = 1; i < new_card_states.size(); i++){
            loc[new_card_states[i]] = i;
            parent_board[i] = loc[nodes[new_card_states[i]].prv_new_card];
        }
        int st = 0;
        int strategy_sz = 0;
        for(int i = 0; i < tree_index; i++){
            nodes[i].info_set_map = st;
            for(int j = 0; j < buckets[i]; j++){
                moves_per_info_set.push_back(children[i]);
                strategy_sz += children[i];
            }
            st += buckets[i];
        }
        cout << "total info sets: " << st << endl;
        cout << "total strategy size: " << strategy_sz << endl;
    }

    // prepares the game tree for an iteration of training
    void prepare(int seed){
        omp::XoroShiro128Plus rng(seed);
        omp::FastUniformIntDistribution2<int> card_generator(0, 51);
        uint64_t initial_mask = 0;
        auto generateCard = [&](uint64_t &used_mask){
            unsigned card;
            uint64_t card_mask;
            do {
                card = card_generator(rng);
                card_mask = 1ull << card;
            } while (used_mask & card_mask);
            used_mask |= card_mask;
            return card;
        };
        auto chooseCard = [&](uint64_t hand){
            int num = 0;
            do {
                num = card_generator(rng);
            } while(num == 51);
            for(int i = 0; i < num%3; i++){
                hand ^= 1ull << __builtin_ctzll(hand);
            }
            return __builtin_ctzll(hand);
        };
        uint64_t sb_hand = 0;
        uint64_t bb_hand = 0;
        uint64_t sb_extra_card = 0;
        uint64_t bb_extra_card = 0;
        for(int i = 0; i < 3; i++){
            sb_hand |= 1ull << generateCard(initial_mask);
            bb_hand |= 1ull << generateCard(initial_mask);
        }
        for(int i : preflop){
            assert(three_card::preflop_bucket(sb_hand) < buckets[i]);
            if(nodes[i].turn == 0){
                nodes[i].info_set = nodes[i].info_set_map + three_card::preflop_bucket(sb_hand);
            } else if(nodes[i].turn == 1){
                nodes[i].info_set = nodes[i].info_set_map + three_card::preflop_bucket(bb_hand);
            } else {
                assert(false);
            }
        }
        for(int i = 0; i < new_card_states.size(); i++){
            int l = new_card_states[i];
            int r = l + nodes[l].size;
            if(nodes[l].prv_new_card == -1){
                board[i] = 0;
                used_mask[i] = initial_mask;
            } else {
                int p = parent_board[i];
                board[i] = board[p];
                used_mask[i] = used_mask[p];
            }
            for(int j = 0; j < nodes[l].new_cards; j++){
                board[i] |= 1ull << generateCard(used_mask[i]);
            }
            // postflop state
            int sb_bucket, bb_bucket;
            if(__builtin_popcountll(board[i]) == 2){
                sb_extra_card = 1ull << chooseCard(bb_hand);
                bb_extra_card = 1ull << chooseCard(sb_hand);
                sb_bucket = three_card::flop_bucket(board[i], sb_hand);
                bb_bucket = three_card::flop_bucket(board[i], bb_hand);
            } else if(__builtin_popcountll(board[i]) == 3){
                sb_bucket = three_card::turn_bucket(board[i] | sb_extra_card, sb_hand);
                bb_bucket = three_card::turn_bucket(board[i] | bb_extra_card, bb_hand);
            } else if(__builtin_popcountll(board[i]) == 4){
                sb_bucket = three_card::river_bucket(board[i] | sb_extra_card, sb_hand);
                bb_bucket = three_card::river_bucket(board[i] | bb_extra_card, bb_hand);
            }
            if(__builtin_popcountll(board[i]) == 4){
                int winner = three_card::eval_strength(board[i] | sb_extra_card, sb_hand) - three_card::eval_strength(board[i] | bb_extra_card, bb_hand);
                if(winner > 0) winner = 1;
                else if(winner < 0) winner = -1;
                else winner = 0;
                if(nodes[l].turn == 0) {
                    nodes[l].info_set = nodes[l].info_set_map + sb_bucket;
                } else {
                    nodes[l].info_set = nodes[l].info_set_map + bb_bucket;
                }
                for(unsigned j = l + 1; j <= r; j++){
                    nodes[j].winner = winner;
                    if(nodes[j].turn == 0) {
                        nodes[j].info_set = nodes[j].info_set_map + sb_bucket;
                    } else {
                        nodes[j].info_set = nodes[j].info_set_map + bb_bucket;
                    }
                }
            } else {
                if(nodes[l].turn == 0) {
                    nodes[l].info_set = nodes[l].info_set_map + sb_bucket;
                } else {
                    nodes[l].info_set = nodes[l].info_set_map + bb_bucket;
                }
                for(unsigned j = l + 1; j <= r; j++){
                    if(nodes[j].new_cards > 0){
                        j += nodes[j].size;
                    } else {
                        if(nodes[j].turn == 0) {
                            nodes[j].info_set = nodes[j].info_set_map + sb_bucket;
                        } else {
                            nodes[j].info_set = nodes[j].info_set_map + bb_bucket;
                        }
                    }
                }
            }
        }
    }

    // updates the leaf utility values for each trainer
    void updateUtility(array<float, TRAINER_SZ> &utility){
        for(int i = 0; i < leaves.size(); i++){
            int node_id = leaves[i];
            int w = winner[i];
            if(w == 0){
                w = nodes[node_id].winner;
            }
            utility[node_id] = w*(nodes[node_id].pot/2);
        }
    }

    vector<int> getMovesPerInfoSet(){
        return moves_per_info_set;
    }

    // returns the number of nodes
    int nodeCount(){
        return tree_index;
    }

    // returns parent node id
    int getParentId(int node_id){
        return nodes[node_id].parent;
    }

    // returns the move used to reach node
    int getMove(int node_id){
        return nodes[node_id].move;
    }

    // returns info set of node
    int getInfoSet(int node_id){
        return nodes[node_id].info_set;
    }

    // returns who's turn it is to move
    int getTurn(int node_id){
        return nodes[node_id].turn;
    }
};

#endif // THREECARD_HPP