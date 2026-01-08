#include "game/GameTree.hpp"
#include "game/KhunPoker.hpp"
#include "game/Poker.hpp"
#include <cstddef>
#include <iostream>
#include <string>
#include "game/ThreeCard.hpp"
#include "game/ThreeCardState.hpp"
using namespace std;

void visualizeTree(GameState* state, const string& prefix){
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

        cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString();
        if(is_terminal){
            cout << " [terminal, winner: " << next_state->getWinner() << "]";
        }
        cout << "\n";

        if(!is_terminal){
            visualizeTree(next_state, prefix + (is_last ? "    " : "|   "));
        }
    }
}

int sz = 0;

void visualizeTreeDepthLimited(GameState* state, const string& prefix, int depth){
    sz++;
    auto actions = state->generateActions();
    if(actions.empty()){
        cout << prefix << "(no actions)\n";
        return;
    }
    if(depth == 0){
        cout << prefix << "(depth limited)\n";
        return;
    }

    for(int i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();

        cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString();
        if(is_terminal){
            cout << " [terminal, winner: " << next_state->getWinner() << "]";
        }
        cout << "\n";

        if(!is_terminal){
            visualizeTreeDepthLimited(next_state, prefix + (is_last ? "    " : "|   "), depth - 1);
        }
    }
}

void visualizeKhunTree(){
    KhunPokerGameState root = KhunPokerGameState();
    visualizeTree(&root, "");
}

void visualizePokerTree(){
    PokerGameState root = PokerGameState();
    visualizeTree(&root, "");
}

void visualizeThreeCardTree(){
    ThreeCardGameState root = ThreeCardGameState();
    visualizeTreeDepthLimited(&root, "", 100);
}

int main(){
    visualizeThreeCardTree();
    cout << "total nodes: " << sz << endl;
}
