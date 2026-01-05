#include "game/GameTree.hpp"
#include "game/KhunPoker.hpp"
#include <cstddef>
#include <iostream>
#include <string>

void visualizeTree(GameState* state, const std::string& prefix){
    auto actions = state->generateActions();
    if(actions.empty()){
        std::cout << prefix << "(no actions)\n";
        return;
    }

    for(std::size_t i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();

        std::cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString();
        if(is_terminal){
            std::cout << " [terminal, winner: " << next_state->getWinner() << "]";
        }
        std::cout << "\n";

        if(!is_terminal){
            visualizeTree(next_state, prefix + (is_last ? "    " : "|   "));
        }
    }
}

void visualizeKhunTree(){
    KhunPokerGameState root = KhunPokerGameState();
    visualizeTree(&root, "");
}

int main(){
    visualizeKhunTree();
}
