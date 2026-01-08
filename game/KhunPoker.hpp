#ifndef KHUNPOKER_HPP
#define KHUNPOKER_HPP

#include "GameTree.hpp"
#include "external/omp/Random.h"
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <cassert>
#include <iostream>
#include "constants/constants.h"
using namespace std;

struct KhunPokerAction : Action {
    string action;
    int turn;

    KhunPokerAction(string action_, int turn_){
        action = action_;
        turn = turn_;
    }

    string toString(){
        if(turn == 0){
            return "sb: " + action;
        } else if (turn == 1) {
            return "bb: " + action;
        } else {
            return "world: " + action;
        }
    }

    bool isWorldAction(){
        return turn == -1;
    }
};

struct KhunPokerGameState : GameState {

    vector<string> action_history;
    int pot;
    int sb_stack, bb_stack;
    int sb_bet, bb_bet;
    //0: player0, 1: player1, -1: world
    int turn;
    // 0: no winner, 1: sb wins, -1: bb wins
    // change in pnl is winner * pot, so sb is positive bb is negative
    int winner;
    bool showdown;

    KhunPokerGameState(){
        pot = 2;
        sb_stack = bb_stack = 1;
        sb_bet = bb_bet = 0;
        winner = 0;
        showdown = false;
        turn = -1;
    }

    KhunPokerGameState(const KhunPokerGameState& other) = default;

    bool isTerminal(){
        return winner != 0 || showdown;
    }

    string getWinner(){
        return (winner == 1 ? "sb" : (winner == -1 ? "bb" : "none"));
    }

    vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> generateActions() override {
        if(isTerminal()) return {};
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions;
        auto add_action = [&actions](
            unique_ptr<GameState> next_state,
            unique_ptr<Action> next_action){
            actions.emplace_back(std::move(next_state), std::move(next_action));
        };

        if(action_history.empty()) // generate cards
        {
            auto new_cards = make_unique<KhunPokerGameState>(*this);
            new_cards->action_history.push_back("deal cards");
            new_cards->turn = 0;
            add_action(
                std::move(new_cards),
                make_unique<KhunPokerAction>("deal cards", -1));
        }
        if(turn == 0) // sb action
        {
            if(action_history.back() == "bb raise") // sb can call or fold
            {
                auto fold = make_unique<KhunPokerGameState>(*this);
                fold->action_history.push_back("sb fold");
                fold->winner = -1;

                auto call = make_unique<KhunPokerGameState>(*this);
                call->action_history.push_back("sb call");
                call->pot++;
                call->sb_stack--;
                call->sb_bet = call->bb_bet = 0;
                call->showdown = true;

                add_action(std::move(fold), make_unique<KhunPokerAction>("sb fold", 0));
                add_action(std::move(call), make_unique<KhunPokerAction>("sb call", 0));
            } 
            else // small blind can raise or check
            { 
                assert(action_history.back() == "deal cards");
                auto raise = make_unique<KhunPokerGameState>(*this);
                raise->action_history.push_back("sb raise");
                raise->pot++;
                raise->sb_stack--;
                raise->sb_bet = 1;
                raise->turn = 1;

                auto check = make_unique<KhunPokerGameState>(*this);
                check->action_history.push_back("sb check");
                check->turn = 1;

                add_action(std::move(raise), make_unique<KhunPokerAction>("sb raise", 0));
                add_action(std::move(check), make_unique<KhunPokerAction>("sb check", 0));
            }
        }
        else if(turn == 1) // bb action
        {
            if(action_history.back() == "sb raise") // bb can call or fold
            {
                auto fold = make_unique<KhunPokerGameState>(*this);
                fold->action_history.push_back("bb fold");
                fold->winner = 1;

                auto call = make_unique<KhunPokerGameState>(*this);
                call->action_history.push_back("bb call");
                call->pot++;
                call->bb_stack--;
                call->bb_bet = call->sb_bet = 0;
                call->showdown = true;

                add_action(std::move(fold), make_unique<KhunPokerAction>("bb fold", 1));
                add_action(std::move(call), make_unique<KhunPokerAction>("bb call", 1));
            }
            else // bb can raise or check
            {
                assert(action_history.back() == "sb check");
                auto raise = make_unique<KhunPokerGameState>(*this);
                raise->action_history.push_back("bb raise");
                raise->bb_stack--;
                raise->bb_bet++;
                raise->pot++;
                raise->turn = 0;

                auto check = make_unique<KhunPokerGameState>(*this);
                check->action_history.push_back("bb check");
                check->showdown = true;

                add_action(std::move(raise), make_unique<KhunPokerAction>("bb raise", 1));
                add_action(std::move(check), make_unique<KhunPokerAction>("bb check", 1));
            }
        }
        return actions;
    }
};

struct KhunPokerGameTree : GameTree {

    vector<int> parent;
    vector<int> move;
    vector<int> turn;
    vector<vector<int>> children;
    int sb_card, bb_card; 
    vector<int> leaves;
    vector<int> winner;
    vector<int> pot;

    int generateTree(KhunPokerGameState* root, int par = -1, int par_move = -1){
        int node_id = children.size();
        parent.push_back(par);
        move.push_back(par_move);
        turn.push_back(root->turn);
        children.push_back(vector<int>{});
        pot.push_back(root->pot - root->sb_bet - root->bb_bet);
        if(root->isTerminal()){
            leaves.push_back(node_id);
            winner.push_back(root->winner);
            return node_id;
        }
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
        for(auto& action : actions){
            KhunPokerGameState* next_state = dynamic_cast<KhunPokerGameState*>(action.first.get());
            int move_id = static_cast<int>(children[node_id].size());
            int child_id = generateTree(next_state, node_id, move_id);
            children[node_id].push_back(child_id);
        }
        return node_id;
    }

    // initializes the game tree
    void init(){
        KhunPokerGameState root;
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root.generateActions();
        assert(actions.size() == 1);
        generateTree(dynamic_cast<KhunPokerGameState*>(actions.front().first.get()));
    }

    // prepares the game tree for an iteration of training
    void prepare(int seed){
        omp::XoroShiro128Plus rng(seed);
        omp::FastUniformIntDistribution2<int> card_generator(0, 5);
        int cards[3] = {0, 1, 2};
        for(int i = 0; i < 3; i++) swap(cards[i], cards[card_generator(rng)%(i + 1)]);
        sb_card = cards[0];
        bb_card = cards[1];
    }

    // updates the leaf utility values for each trainer
    void updateUtility(array<float, TRAINER_SZ> &utility){
        for(int i = 0; i < leaves.size(); i++){
            int node_id = leaves[i];
            int w = winner[i];
            if(w == 0){
                w = (sb_card > bb_card ? 1 : -1);
            }
            utility[node_id] = w*(pot[node_id]/2);
        }
    }

    vector<int> getMovesPerInfoSet(){
        vector<int> ret;
        for(int i = 0; i < children.size(); i++){
            for(int j = 0; j < 3; j++){
                ret.push_back(children[i].size());
            }
        }
        return ret;
    }

    // returns the number of nodes
    int nodeCount(){
        return children.size();
    }

    // returns parent node id
    int getParentId(int node_id){
        return parent[node_id];
    }

    // returns the move used to reach node
    int getMove(int node_id){
        return move[node_id];
    }

    // returns info set of node
    int getInfoSet(int node_id){
        return node_id*3 + (turn[node_id] == 0 ? sb_card : bb_card);
    }

    // returns who's turn it is to move
    int getTurn(int node_id){
        return turn[node_id];
    }

    // For best response: returns number of unique deals (6 for Kuhn Poker)
    int getNumDeals() override {
        return 6;  // 3 cards, pick 2: 3 * 2 = 6 possible deals
    }

    // Prepares the tree for a specific deal index
    // deal_idx in [0, 5] maps to (sb_card, bb_card) pairs:
    // 0: (0,1), 1: (0,2), 2: (1,0), 3: (1,2), 4: (2,0), 5: (2,1)
    void prepareDeal(int deal_idx) override {
        int idx = 0;
        for(int sb = 0; sb < 3; sb++){
            for(int bb = 0; bb < 3; bb++){
                if(sb != bb){
                    if(idx == deal_idx){
                        sb_card = sb;
                        bb_card = bb;
                        return;
                    }
                    idx++;
                }
            }
        }
    }
};

#endif // KHUNPOKER_HPP
