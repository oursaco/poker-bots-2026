#ifndef FAKEPOKER_HPP 
#define FAKEPOKER_HPP

#include "GameTree.hpp"
#include "external/omp/Random.h"
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <cassert>
#include <iostream>
#include "constants/constants.h"
#include <algorithm>
#include <map>
using namespace std;

struct FakePokerAction : Action {
    string action;
    int turn;
    int amount;
    int sb_stack;
    int bb_stack;

    FakePokerAction(string action_, int turn_, int amount_, int sb_stack_, int bb_stack_){
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

struct FakePokerGameState : GameState {

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

    FakePokerGameState(){
        pot = 3;
        sb_stack = 100;
        bb_stack = 100;
        sb_bet = 1;
        bb_bet = 2;
        winner = 0;
        showdown = false;
        turn = -1;
        action_depth = 0;
        street = 0;
    }

    FakePokerGameState(const FakePokerGameState& other) = default;

    bool isTerminal(){
        return winner != 0 || showdown;
    }

    string getWinner(){
        return (winner == 1 ? "sb" : (winner == -1 ? "bb" : "none"));
    } 

        pair<unique_ptr<GameState>, unique_ptr<Action>> sb_fold(){
        auto fold = make_unique<FakePokerGameState>(*this);  
        fold->winner = -1;
        fold->pot -= fold->bb_bet - fold->sb_bet;
        assert(fold->sb_bet < fold->bb_bet);
        return {std::move(fold), make_unique<FakePokerAction>("sb fold", 0, 0, fold->sb_stack, fold->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_fold(){
        auto fold = make_unique<FakePokerGameState>(*this);  
        fold->winner = 1;
        fold->pot -= fold->sb_bet - fold->bb_bet;
        assert(fold->bb_bet < fold->sb_bet);
        return {std::move(fold), make_unique<FakePokerAction>("bb fold", 1, 0, fold->sb_stack, fold->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_call(){
        auto call = make_unique<FakePokerGameState>(*this);  
        assert(call->sb_bet < call->bb_bet || call->bb_bet == 0);
        int amount = call->bb_bet - call->sb_bet;
        call->pot += amount;
        call->sb_stack -= amount;
        if(street == 0 && sb_bet == 1){
            call->sb_bet++;
            call->action_depth++;
            call->turn = 1;
        } else if(street == 2){
            call->winner = 0;
            call->showdown = true;
        } else {
            call->street++;
            call->sb_bet = call->bb_bet = 0;
            call->turn = -1;
            call->action_depth = 0;
        }
        return {std::move(call), make_unique<FakePokerAction>("sb call", 0, amount, call->sb_stack, call->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_call(){
        auto call = make_unique<FakePokerGameState>(*this);  
        assert(call->bb_bet < call->sb_bet || call->sb_bet == 0 || (street == 0 && call->sb_bet == 2));
        int amount = call->sb_bet - call->bb_bet;
        call->pot += amount;
        call->bb_stack -= amount;
        if(sb_bet == 0){
            call->turn = 0;
            call->action_depth++;
        } else if(street == 2){
            call->winner = 0;
            call->showdown = true;
        } else {
            call->street++;
            call->sb_bet = call->bb_bet = 0;
            call->turn = -1;
            call->action_depth = 0;
        }
        return {std::move(call), make_unique<FakePokerAction>("bb call", 1, amount, call->sb_stack, call->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_raise(int amount){
        auto raise = make_unique<FakePokerGameState>(*this);  
        assert(raise->sb_bet < raise->bb_bet || raise->bb_bet == 0);
        assert(raise->sb_bet + amount > raise->bb_bet);
        assert(raise->sb_stack >= amount);
        raise->pot += amount;
        raise->sb_stack -= amount;
        raise->sb_bet += amount;
        raise->turn = 1;
        raise->action_depth = action_depth + 1;
        return {std::move(raise), make_unique<FakePokerAction>("sb raise", 0, amount, raise->sb_stack, raise->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_raise(int amount){
        auto raise = make_unique<FakePokerGameState>(*this);  
        assert(raise->bb_bet < raise->sb_bet || raise->sb_bet == 0 || (street == 0 && raise->sb_bet == 2));
        assert(raise->bb_bet + amount > raise->sb_bet);
        assert(raise->bb_stack >= amount);
        raise->pot += amount;
        raise->bb_stack -= amount;
        raise->bb_bet += amount;
        raise->turn = 0;
        raise->action_depth = action_depth + 1;
        return {std::move(raise), make_unique<FakePokerAction>("bb raise", 1, amount, raise->sb_stack, raise->bb_stack)};
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
            if(sb_bet > 0 && !(street == 0 && sb_bet == 2)){
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
            }
            auto action = make_unique<FakePokerAction>(action_name, -1, 0, sb_stack, bb_stack);
            auto next_state = make_unique<FakePokerGameState>(*this);
            if(street == 0){
                next_state->turn = 0;
            } else {
                next_state->turn = 1;
            }
            actions.push_back({std::move(next_state), std::move(action)});
            return actions;
        }
        if(street == 0){
            return generateStreetActions();
        }
        if(street == 1){
            return generateStreetActions();
        }
        if(street == 2){
            return generateStreetActions();
        }
        assert(false);
    }
};

struct FakePokerGameTree : GameTree {

    struct Node {
        int parent;
        int move;
        int turn;
        int pot;
        int street;
        int size;
        int info_set;
        int winner;
    };

    vector<Node> nodes;
    vector<int> leaves;
    vector<int> winner;
    array<int, TREE_SZ> children;
    array<int, POLICY_SZ> moves_per_info_set;
    int moves_per_info_set_index = 0;
    static constexpr int kRankCount = 5;
    static constexpr int kPrivateBuckets = kRankCount * 2 * 2;
    static constexpr int kFlopRankBuckets = 15;
    static constexpr int kFlopBoardBuckets = kFlopRankBuckets * 4;
    static constexpr int kFlopBuckets = kPrivateBuckets * kFlopBoardBuckets;
    static constexpr int kRiverBoardBuckets = 15;
    static constexpr int kDrawStateBuckets = 9;
    static constexpr int kRiverBuckets = kPrivateBuckets * kDrawStateBuckets * kRiverBoardBuckets;
    static constexpr int kTotalBuckets = kRiverBuckets;

    static_assert(kFlopBuckets <= kTotalBuckets, "Bucket counts must fit total bucket space.");

    int encodePrivate(int rank, bool flush_draw, bool straight_draw) const {
        return rank * 4 + (flush_draw ? 2 : 0) + (straight_draw ? 1 : 0);
    }

    int encodeFlopRanks(int a, int b) const {
        if(a > b) swap(a, b);
        int id = 0;
        for(int i = 0; i < a; i++){
            id += kRankCount - i;
        }
        id += b - a;
        return id;
    }

    int encodeFlopBoard(int a, int b, bool flop_flush_draw, bool flop_straight_draw) const {
        int ranks = encodeFlopRanks(a, b);
        int draw_bits = (flop_flush_draw ? 2 : 0) + (flop_straight_draw ? 1 : 0);
        return ranks * 4 + draw_bits;
    }

    int encodeRiverBoard(int a, int b, int c) const {
        int counts[kRankCount] = {0};
        counts[a]++;
        counts[b]++;
        counts[c]++;
        for(int r = 0; r < kRankCount; r++){
            if(counts[r] == 3){
                return r;
            }
        }
        for(int r = 0; r < kRankCount; r++){
            if(counts[r] == 2){
                return kRankCount + r;
            }
        }
        int max_rank = max(a, max(b, c));
        return 2 * kRankCount + max_rank;
    }

    int encodeDrawState(bool flop_flush_draw, bool flush_completed, bool flop_straight_draw, bool straight_completed) const {
        int flush_state = flop_flush_draw ? (flush_completed ? 2 : 1) : 0;
        int straight_state = flop_straight_draw ? (straight_completed ? 2 : 1) : 0;
        return flush_state * 3 + straight_state;
    }

    int evaluateHand(int hole_rank,
                     int flop_a,
                     int flop_b,
                     int river,
                     bool hole_flush_draw,
                     bool hole_straight_draw,
                     bool flop_flush_draw,
                     bool flop_straight_draw,
                     bool flush_completed,
                     bool straight_completed) const {
        int counts[kRankCount] = {0};
        counts[hole_rank]++;
        counts[flop_a]++;
        counts[flop_b]++;
        counts[river]++;

        bool has_quads = false;
        bool has_trips = false;
        bool has_pair = false;
        vector<pair<int, int>> groups;
        groups.reserve(4);
        for(int r = 0; r < kRankCount; r++){
            int c = counts[r];
            if(c > 0){
                groups.emplace_back(c, r);
                if(c == 4) has_quads = true;
                else if(c == 3) has_trips = true;
                else if(c == 2) has_pair = true;
            }
        }
        sort(groups.begin(), groups.end(), [](const pair<int, int>& a, const pair<int, int>& b){
            if(a.first != b.first) return a.first > b.first;
            return a.second > b.second;
        });

        int tiebreak = 0;
        for(const auto& g : groups){
            tiebreak = tiebreak * kRankCount + g.second;
        }

        bool flush = hole_flush_draw && flop_flush_draw && flush_completed;
        bool straight = hole_straight_draw && flop_straight_draw && straight_completed;

        int category = 0;
        if(has_quads){
            category = 5;
        } else if(flush){
            category = 4;
        } else if(straight){
            category = 3;
        } else if(has_trips){
            category = 2;
        } else if(has_pair){
            category = 1;
        }
        return category * 1000 + tiebreak;
    }

    unique_ptr<GameTree> clone() const override {
        // Avoid copying any indeterminate runtime state; rebuild deterministically.
        auto t = make_unique<FakePokerGameTree>();
        t->init();
        return t;
    }

    int generateTree(FakePokerGameState* root, int par = -1, int par_move = -1){
        if(root->turn == -1){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
            assert(actions.size() == 1);
            int node_id = generateTree(dynamic_cast<FakePokerGameState*>(actions.front().first.get()), par, par_move);
            return node_id;
        }
        int node_id = nodes.size();
        assert(node_id < TREE_SZ);
        int size = 0;
        children[node_id] = 0;
        nodes.push_back({par, par_move, root->turn, root->pot, root->street, 0, 0, 0});
        if(root->isTerminal()){
            leaves.push_back(node_id);
            winner.push_back(root->winner);
            return node_id;
        }
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
        for(auto& action : actions){
            FakePokerGameState* next_state = dynamic_cast<FakePokerGameState*>(action.first.get());
            int move_id = static_cast<int>(children[node_id]);
            int child_id = generateTree(next_state, node_id, move_id);
            children[node_id]++;
            size += nodes[child_id].size + 1;
        }
        nodes[node_id].size = size;
        return node_id;
    }

    // initializes the game tree
    void init(){
        assert(nodes.size() == 0);
        leaves.clear();
        winner.clear();
        moves_per_info_set_index = 0;
        children.fill(0);
        FakePokerGameState root;
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root.generateActions();
        assert(actions.size() == 1);
        generateTree(dynamic_cast<FakePokerGameState*>(actions.front().first.get()));
        int node_count = static_cast<int>(nodes.size());
        assert(static_cast<long long>(kTotalBuckets) * node_count <= POLICY_SZ);
        for(int bucket = 0; bucket < kTotalBuckets; bucket++){
            for(int i = 0; i < node_count; i++){
                moves_per_info_set[moves_per_info_set_index++] = children[i];
            }
        }
    }

    // prepares the game tree for an iteration of training
    void prepare(int seed){
        omp::XoroShiro128Plus rng(seed);
        omp::FastUniformIntDistribution2<int> rank_generator(0, kRankCount - 1);
        omp::FastUniformIntDistribution2<int> flush_draw_generator(0, 3);
        omp::FastUniformIntDistribution2<int> straight_draw_generator(0, 9);
        omp::FastUniformIntDistribution2<int> flop_flush_generator(0, 9);
        omp::FastUniformIntDistribution2<int> flop_straight_generator(0, 3);
        omp::FastUniformIntDistribution2<int> coin_flip(0, 1);

        int sb_rank = rank_generator(rng);
        int bb_rank = rank_generator(rng);
        bool sb_flush_draw = flush_draw_generator(rng) == 0;
        bool sb_straight_draw = straight_draw_generator(rng) == 0;
        bool bb_flush_draw = flush_draw_generator(rng) == 0;
        bool bb_straight_draw = straight_draw_generator(rng) == 0;

        int flop_a = rank_generator(rng);
        int flop_b = rank_generator(rng);
        bool flop_flush_draw = flop_flush_generator(rng) == 0;
        bool flop_straight_draw = flop_straight_generator(rng) == 0;

        int river = rank_generator(rng);
        bool flush_completed = flop_flush_draw ? (coin_flip(rng) == 0) : false;
        bool straight_completed = flop_straight_draw ? (coin_flip(rng) == 0) : false;

        int sb_private = encodePrivate(sb_rank, sb_flush_draw, sb_straight_draw);
        int bb_private = encodePrivate(bb_rank, bb_flush_draw, bb_straight_draw);

        int flop_board = encodeFlopBoard(flop_a, flop_b, flop_flush_draw, flop_straight_draw);
        int sb_flop_bucket = sb_private * kFlopBoardBuckets + flop_board;
        int bb_flop_bucket = bb_private * kFlopBoardBuckets + flop_board;

        int river_board = encodeRiverBoard(flop_a, flop_b, river);
        int draw_state = encodeDrawState(flop_flush_draw, flush_completed, flop_straight_draw, straight_completed);
        int river_stride = kDrawStateBuckets * kRiverBoardBuckets;
        int sb_river_bucket = sb_private * river_stride + draw_state * kRiverBoardBuckets + river_board;
        int bb_river_bucket = bb_private * river_stride + draw_state * kRiverBoardBuckets + river_board;

        int sb_strength = evaluateHand(sb_rank,
                                       flop_a,
                                       flop_b,
                                       river,
                                       sb_flush_draw,
                                       sb_straight_draw,
                                       flop_flush_draw,
                                       flop_straight_draw,
                                       flush_completed,
                                       straight_completed);
        int bb_strength = evaluateHand(bb_rank,
                                       flop_a,
                                       flop_b,
                                       river,
                                       bb_flush_draw,
                                       bb_straight_draw,
                                       flop_flush_draw,
                                       flop_straight_draw,
                                       flush_completed,
                                       straight_completed);
        int showdown_winner = 0;
        if(sb_strength > bb_strength) showdown_winner = 1;
        else if(sb_strength < bb_strength) showdown_winner = -1;

        int node_count = static_cast<int>(nodes.size());
        for(int i = 0; i < node_count; i++){
            int bucket = 0;
            if(nodes[i].street == 0){
                bucket = (nodes[i].turn == 0) ? sb_private : bb_private;
            } else if(nodes[i].street == 1){
                bucket = (nodes[i].turn == 0) ? sb_flop_bucket : bb_flop_bucket;
            } else if(nodes[i].street == 2){
                bucket = (nodes[i].turn == 0) ? sb_river_bucket : bb_river_bucket;
                nodes[i].winner = showdown_winner;
            } else {
                assert(false);
            }
            assert(bucket >= 0 && bucket < kTotalBuckets);
            nodes[i].info_set = bucket * node_count + i;
        }
    }

    // updates the leaf utility values for each trainer
    void updateUtility(array<float, TREE_SZ> &utility){
        for(int i = 0; i < leaves.size(); i++){
            int node_id = leaves[i];
            int w = winner[i];
            if(w == 0){
                w = nodes[node_id].winner;
            }
            utility[node_id] = w*(nodes[node_id].pot/2);
        }
    }

    void fillMovesPerInfoSet(array<int, POLICY_SZ> &moves_per_info_set_){
        for(int i = 0; i < moves_per_info_set_index; i++){
            moves_per_info_set_[i] = moves_per_info_set[i];
        }
    }

    // returns the number of info sets
    int infoSetCount(){
        return moves_per_info_set_index;
    }

    // returns the number of nodes
    int nodeCount(){
        return nodes.size();
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

    // returns the size of the node
    int getSize(int node_id){
        return nodes[node_id].size;
    }
};

#endif // FAKEPOKER_HPP
