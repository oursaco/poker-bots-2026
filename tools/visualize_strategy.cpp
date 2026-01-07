#include "game/GameTree.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCardState.hpp"
#include "constants/constants.h"
#include "cfr/CFR.hpp"
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
using namespace std;

int node_id_counter = 0;

float averageMoveProb(DCFRPolicy& policy, int info_set, int move_id){
    int move_count = policy.getMoveCount(info_set);
    if(move_count <= 0){
        return 0.0f;
    }
    int st = policy.getState(info_set, 0);
    float sum = 0.0f;
    for(int move = 0; move < move_count; ++move){
        sum += policy.strategy_sum[st + move];
    }
    if(sum <= 0.0f){
        return 1.0f/move_count;
    }
    return policy.strategy_sum[st + move_id]/sum;
}

float getPolicyProb(GameTree* tree, DCFRPolicy* players, int node_id, int move_id){
    int player = tree->getTurn(node_id);
    int info_set = tree->getInfoSet(node_id);
    return averageMoveProb(players[player], info_set, move_id);
}

void visualizeStrategy(GameState* state, GameTree* tree, DCFRPolicy* players, const string& prefix, int node_id){
    auto actions = state->generateActions();
    if(actions.empty()){
        cout << prefix << "(no actions)\n";
        return;
    }
    for(int i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();
        int child_node_id = node_id;
        float prob = 1.0f;
        if(!action->isWorldAction()){
            prob = getPolicyProb(tree, players, node_id, i);
            node_id_counter++;
            child_node_id = node_id_counter;
        }

        cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString() << " | prob: " << prob;
        if(is_terminal){
            cout << " [terminal, winner: " << next_state->getWinner() << "]";
        }
        cout << "\n";

        if(!is_terminal){
            visualizeStrategy(next_state, tree, players, prefix + (is_last ? "    " : "|   "), child_node_id);
        }
    }
}

void visualizeStrategyDepthLimited(GameState* state, GameTree* tree, DCFRPolicy* players, const string& prefix, int node_id, int depth){
    auto actions = state->generateActions();
    if(actions.empty()){
        cout << prefix << "(no actions)\n";
        return;
    }
    if(depth == 0){
        cout << prefix << "(depth limited)\n";
    }
    for(int i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();
        int child_node_id = node_id;
        float prob = 1.0f;
        if(!action->isWorldAction()){
            prob = getPolicyProb(tree, players, node_id, i);
            node_id_counter++;
            child_node_id = node_id_counter;
        }

        if(depth > 0){
            cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString() << " | prob: " << prob;
            if(is_terminal){
                cout << " [terminal, winner: " << next_state->getWinner() << "]";
            }
            cout << "\n";
        }

        if(!is_terminal){
            visualizeStrategyDepthLimited(next_state, tree, players, prefix + (is_last ? "    " : "|   "), child_node_id, depth - 1);
        }
    }
}

void visualizeThreeCardStrategy(){
    ThreeCardGameTree tree;
    NaiveThreeCardBucket bucket;
    tree.setBucket(&bucket);
    tree.init();
    tree.prepare(42);
    DCFRTrainer trainer;
    trainer.setTree(&tree);
    trainer.players[0].initPolicy(tree.getMovesPerInfoSet());
    trainer.players[1].initPolicy(tree.getMovesPerInfoSet());
    trainer.players[0].loadPolicy("./three_card_models/player0_final.bin");
    trainer.players[1].loadPolicy("./three_card_models/player1_final.bin");

    cout << fixed << setprecision(4);
    node_id_counter = 0;
    ThreeCardGameState root = ThreeCardGameState();
    visualizeStrategyDepthLimited(&root, &tree, trainer.players, "", 0, 5);
}

int main(){
    visualizeThreeCardStrategy();
}
