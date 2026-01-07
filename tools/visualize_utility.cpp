#include "game/GameTree.hpp"
#include "game/Poker.hpp"
#include "constants/constants.h"
#include "cfr/CFR.hpp"
#include <cstddef>
#include <iostream>
#include <string>
using namespace std;

int ind = 0;

void visualizeUtility(GameState* state, const string& prefix, array<float, TRAINER_SZ> &utility){
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
        if(!action->isWorldAction()){
            ind++;
        }

        cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString() << " | utility: " << utility[ind] << " ";
        if(is_terminal){
            cout << " [terminal, winner: " << next_state->getWinner() << "]";
        }
        cout << "\n";

        if(!is_terminal){
            visualizeUtility(next_state, prefix + (is_last ? "    " : "|   "), utility);
        }
    }
}

void visualizeUtilityDepthLimited(GameState* state, const string& prefix, array<float, TRAINER_SZ> &utility, int depth){
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
        if(!action->isWorldAction()){
            ind++;
        }

        if(depth > 0){
            cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString() << " | utility: " << utility[ind] << " ";
            if(is_terminal){
                cout << " [terminal, winner: " << next_state->getWinner() << "]";
            }
            cout << "\n";
        }

        if(!is_terminal){
            visualizeUtilityDepthLimited(next_state, prefix + (is_last ? "    " : "|   "), utility, depth - 1);
        }
    }
}

void visualizePokerUtility(){
    PokerGameTree tree;
    tree.init();
    tree.prepare(42);
    DCFRTrainer trainer;
    trainer.setTree(&tree);
    trainer.players[0].initPolicy(tree.getMovesPerInfoSet());
    trainer.players[1].initPolicy(tree.getMovesPerInfoSet());
    trainer.updateUtility();
    PokerGameState root = PokerGameState();
    visualizeUtilityDepthLimited(&root, "", trainer.utility, 6);
}

int main(){
    visualizePokerUtility();
}