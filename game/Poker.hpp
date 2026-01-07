#ifndef POKER_HPP
#define POKER_HPP

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
using namespace std;

struct PokerAction : Action {
    string action;
    int turn;
    int amount;
    int sb_stack;
    int bb_stack;

    PokerAction(string action_, int turn_, int amount_, int sb_stack_, int bb_stack_){
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

struct PokerGameState : GameState {

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

    PokerGameState(){
        pot = 3;
        sb_stack = 199;
        bb_stack = 198;
        sb_bet = 1;
        bb_bet = 2;
        winner = 0;
        showdown = false;
        turn = -1;
        action_depth = 0;
        street = 0;
    }

    PokerGameState(const PokerGameState& other) = default;

    bool isTerminal(){
        return winner != 0 || showdown;
    }

    string getWinner(){
        return (winner == 1 ? "sb" : (winner == -1 ? "bb" : "none"));
    } 

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_fold(){
        auto fold = make_unique<PokerGameState>(*this);  
        fold->winner = -1;
        fold->pot -= fold->bb_bet - fold->sb_bet;
        assert(fold->sb_bet < fold->bb_bet);
        return {std::move(fold), make_unique<PokerAction>("sb fold", 0, 0, fold->sb_stack, fold->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_fold(){
        auto fold = make_unique<PokerGameState>(*this);  
        fold->winner = 1;
        fold->pot -= fold->sb_bet - fold->bb_bet;
        assert(fold->bb_bet < fold->sb_bet);
        return {std::move(fold), make_unique<PokerAction>("bb fold", 1, 0, fold->sb_stack, fold->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_call(){
        auto call = make_unique<PokerGameState>(*this);  
        assert(call->sb_bet < call->bb_bet || call->bb_bet == 0);
        int amount = call->bb_bet - call->sb_bet;
        call->pot += amount;
        call->sb_stack -= amount;
        if(street == 3){
            call->winner = 0;
            call->showdown = true;
        } else {
            call->street++;
            call->sb_bet = call->bb_bet = 0;
            call->turn = -1;
            call->action_depth = 0;
        }
        return {std::move(call), make_unique<PokerAction>("sb call", 0, amount, call->sb_stack, call->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_call(){
        auto call = make_unique<PokerGameState>(*this);  
        assert(call->bb_bet < call->sb_bet || call->sb_bet == 0);
        int amount = call->sb_bet - call->bb_bet;
        call->pot += amount;
        call->bb_stack -= amount;
        if(sb_bet == 0){
            call->turn = 0;
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
        return {std::move(call), make_unique<PokerAction>("bb call", 1, amount, call->sb_stack, call->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> sb_raise(int amount){
        auto raise = make_unique<PokerGameState>(*this);  
        assert(raise->sb_bet < raise->bb_bet || raise->bb_bet == 0);
        assert(raise->sb_bet + amount > raise->bb_bet);
        assert(raise->sb_stack >= amount);
        raise->pot += amount;
        raise->sb_stack -= amount;
        raise->sb_bet += amount;
        raise->turn = 1;
        raise->action_depth = action_depth + 1;
        return {std::move(raise), make_unique<PokerAction>("sb raise", 0, amount, raise->sb_stack, raise->bb_stack)};
    }

    pair<unique_ptr<GameState>, unique_ptr<Action>> bb_raise(int amount){
        auto raise = make_unique<PokerGameState>(*this);  
        assert(raise->bb_bet < raise->sb_bet || raise->sb_bet == 0);
        assert(raise->bb_bet + amount > raise->sb_bet);
        assert(raise->bb_stack >= amount);
        raise->pot += amount;
        raise->bb_stack -= amount;
        raise->bb_bet += amount;
        raise->turn = 0;
        raise->action_depth = action_depth + 1;
        return {std::move(raise), make_unique<PokerAction>("bb raise", 1, amount, raise->sb_stack, raise->bb_stack)};
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

    vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> generatePreflopActions() {
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
            }
            auto action = make_unique<PokerAction>(action_name, -1, 0, sb_stack, bb_stack);
            auto next_state = make_unique<PokerGameState>(*this);
            if(street == 0){
                next_state->turn = 0;
            } else {
                next_state->turn = 1;
            }
            actions.push_back({std::move(next_state), std::move(action)});
            return actions;
        }
        if(street == 0){
            return generatePreflopActions();
        }
        if(street == 1){
            return generatePreflopActions();
        }
        if(street == 2){
            return generatePreflopActions();
        }
        if(street == 3){
            return generatePreflopActions();
        }
        assert(false);
    }
};

struct PokerGameTree : GameTree {

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
    };

    vector<Node> nodes;
    vector<int> leaves;
    vector<int> winner;
    vector<int> new_card_states;
    vector<int> children;
    vector<int> moves_per_info_set;
    vector<int> preflop;
    vector<omp::Hand> board;
    vector<uint64_t> used_mask;
    vector<int> parent_board;
    omp::HandEvaluator hand_eval;
    bool use_fixed_sb_hand = false;
    bool use_fixed_bb_hand = false;
    bool use_fixed_flop = false;
    bool use_fixed_turn = false;
    bool use_fixed_river = false;
    uint64_t fixed_sb_hand = 0;
    uint64_t fixed_bb_hand = 0;
    uint64_t fixed_flop = 0;
    uint64_t fixed_turn = 0;
    uint64_t fixed_river = 0;
    uint64_t last_sb_hand = 0;
    uint64_t last_bb_hand = 0;

    int generateTree(PokerGameState* root, int par = -1, int par_move = -1, int prv_new_card = -1, int new_cards = 0){
        if(root->turn == -1){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
            assert(actions.size() == 1);
            if(root->street == 1){
                new_cards = 3;
            } else if(root->street == 2){
                new_cards = 1;
            } else if(root->street == 3){
                new_cards = 1;
            }
            int node_id = generateTree(dynamic_cast<PokerGameState*>(actions.front().first.get()), par, par_move, prv_new_card, new_cards);
            new_card_states.push_back(node_id);
            return node_id;
        }
        int node_id = nodes.size();
        children.push_back(0);
        int size = 0;
        nodes.push_back({par, par_move, root->turn, root->pot, new_cards, 0, prv_new_card});
        if(prv_new_card == -1) preflop.push_back(node_id);
        if(new_cards > 0) prv_new_card = node_id;
        if(root->isTerminal()){
            leaves.push_back(node_id);
            winner.push_back(root->winner);
            return node_id;
        }
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
        for(auto& action : actions){
            PokerGameState* next_state = dynamic_cast<PokerGameState*>(action.first.get());
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
        assert(nodes.size() == 0);
        PokerGameState root;
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root.generateActions();
        assert(actions.size() == 1);
        generateTree(dynamic_cast<PokerGameState*>(actions.front().first.get()));
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
        for(int j = 0; j < ((1 << 16))/4096; j++){
            for(int i = 0; i < children.size(); i++){
                moves_per_info_set.push_back(children[i]);
            }
        }
    }

    void clearFixedCards(){
        use_fixed_sb_hand = false;
        use_fixed_bb_hand = false;
        use_fixed_flop = false;
        use_fixed_turn = false;
        use_fixed_river = false;
        fixed_sb_hand = 0;
        fixed_bb_hand = 0;
        fixed_flop = 0;
        fixed_turn = 0;
        fixed_river = 0;
    }

    void setFixedSbHand(uint64_t sb_hand){
        use_fixed_sb_hand = true;
        fixed_sb_hand = sb_hand;
    }

    void setFixedBbHand(uint64_t bb_hand){
        use_fixed_bb_hand = true;
        fixed_bb_hand = bb_hand;
    }

    void setFixedFlop(uint64_t flop){
        use_fixed_flop = true;
        fixed_flop = flop;
    }

    void setFixedTurn(uint64_t turn){
        use_fixed_turn = true;
        fixed_turn = turn;
    }

    void setFixedRiver(uint64_t river){
        use_fixed_river = true;
        fixed_river = river;
    }

    // prepares the game tree for an iteration of training
    void prepare(int seed){
        omp::XoroShiro128Plus rng(seed);
        omp::FastUniformIntDistribution2<int> card_generator(0, 51);
        uint64_t initial_mask = 0;
        uint64_t reserved_mask = 0;
        if(use_fixed_flop) reserved_mask |= fixed_flop;
        if(use_fixed_turn) reserved_mask |= fixed_turn;
        if(use_fixed_river) reserved_mask |= fixed_river;
        if(use_fixed_sb_hand) initial_mask |= fixed_sb_hand;
        if(use_fixed_bb_hand) initial_mask |= fixed_bb_hand;
        auto generateCard = [&](uint64_t &used_mask){
            unsigned card;
            uint64_t card_mask;
            do {
                card = card_generator(rng);
                card_mask = 1ull << card;
            } while ((used_mask & card_mask) || (reserved_mask & card_mask));
            used_mask |= card_mask;
            return card;
        };
        auto addFixedCards = [&](omp::Hand &hand, uint64_t &used, uint64_t cards){
            uint64_t remaining = cards;
            while(remaining){
                unsigned card = __builtin_ctzll(remaining);
                hand += omp::Hand(card);
                used |= 1ull << card;
                remaining &= remaining - 1;
            }
        };
        auto handFromMask = [&](uint64_t mask){
            omp::Hand hand = omp::Hand::empty();
            uint64_t remaining = mask;
            while(remaining){
                unsigned card = __builtin_ctzll(remaining);
                hand += omp::Hand(card);
                remaining &= remaining - 1;
            }
            return hand;
        };
        omp::Hand sb_hand = use_fixed_sb_hand ? handFromMask(fixed_sb_hand) : omp::Hand::empty();
        omp::Hand bb_hand = use_fixed_bb_hand ? handFromMask(fixed_bb_hand) : omp::Hand::empty();
        for(int i = 0; i < 2; ++i){
            if(!use_fixed_sb_hand){
                sb_hand += omp::Hand(generateCard(initial_mask));
            }
            if(!use_fixed_bb_hand){
                bb_hand += omp::Hand(generateCard(initial_mask));
            }
        }
        last_sb_hand = sb_hand.getMask();
        last_bb_hand = bb_hand.getMask();
        for(int i : preflop){
            if(nodes[i].turn == 0){
                nodes[i].info_set = hand_eval.evaluate(sb_hand)/4096*nodes.size() + i;
            } else if(nodes[i].turn == 1){
                nodes[i].info_set = hand_eval.evaluate(bb_hand)/4096*nodes.size() + i;
            } else {
                assert(false);
            }
        }
        for(int i = 0; i < new_card_states.size(); i++){
            int l = new_card_states[i];
            int r = l + nodes[l].size;
            if(nodes[l].prv_new_card == -1){
                board[i] = omp::Hand::empty();
                used_mask[i] = initial_mask;
            } else {
                int p = parent_board[i];
                board[i] = board[p];
                used_mask[i] = used_mask[p];
            }
            int board_count = board[i].count();
            if(nodes[l].new_cards == 3 && board_count == 0 && use_fixed_flop){
                addFixedCards(board[i], used_mask[i], fixed_flop);
            } else if(nodes[l].new_cards == 1 && board_count == 3 && use_fixed_turn){
                addFixedCards(board[i], used_mask[i], fixed_turn);
            } else if(nodes[l].new_cards == 1 && board_count == 4 && use_fixed_river){
                addFixedCards(board[i], used_mask[i], fixed_river);
            } else {
                for(int j = 0; j < nodes[l].new_cards; j++){
                    board[i] += omp::Hand(generateCard(used_mask[i]));
                }
            }
            int sb_bucket = hand_eval.evaluate(sb_hand + board[i]);
            int bb_bucket = hand_eval.evaluate(bb_hand + board[i]);
            // postflop state
            if(board[i].count() == 5){
                int winner = sb_bucket - bb_bucket;
                if(winner > 0) winner = 1;
                else if(winner < 0) winner = -1;
                else winner = 0;
                if(nodes[l].turn == 0) {
                    nodes[l].info_set = sb_bucket/4096*nodes.size() + l;
                } else {
                    nodes[l].info_set = bb_bucket/4096*nodes.size() + l;
                }
                for(unsigned j = l + 1; j <= r; j++){
                    nodes[j].winner = winner;
                    if(nodes[j].turn == 0) {
                        nodes[j].info_set = sb_bucket/4096*nodes.size() + j;
                    } else {
                        nodes[j].info_set = bb_bucket/4096*nodes.size() + j;
                    }
                }
            } else {
                if(nodes[l].turn == 0) {
                    nodes[l].info_set = sb_bucket/4096*nodes.size() + l;
                } else {
                    nodes[l].info_set = bb_bucket/4096*nodes.size() + l;
                }
                for(unsigned j = l + 1; j <= r; j++){
                    if(nodes[j].new_cards > 0){
                        j += nodes[j].size;
                    } else {
                        if(nodes[j].turn == 0) {
                            nodes[j].info_set = sb_bucket/4096*nodes.size() + j;
                        } else {
                            nodes[j].info_set = bb_bucket/4096*nodes.size() + j;
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
};

#endif // POKER_HPP
