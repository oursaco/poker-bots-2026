#include "game/GameTree.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCardState.hpp"
#include "constants/constants.h"
#include "cfr/CFR.hpp"
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
using namespace std;

int node_id_counter = 0;

string cardToString(unsigned card){
    static const char* ranks = "23456789TJQKA";
    static const char suits[] = {'s', 'h', 'd', 'c'};
    string out;
    out += ranks[card/4];
    out += suits[card%4];
    return out;
}

string maskToString(uint64_t mask){
    string out;
    bool first = true;
    while(mask){
        unsigned card = __builtin_ctzll(mask);
        mask &= mask - 1;
        if(!first){
            out += " ";
        }
        out += cardToString(card);
        first = false;
    }
    if(first){
        return "(none)";
    }
    return out;
}

struct HoleCards {
    uint64_t sb_hand;
    uint64_t bb_hand;
};

HoleCards sampleHoleCards(int seed){
    omp::XoroShiro128Plus rng(seed);
    omp::FastUniformIntDistribution2<int> card_generator(0, 51);
    uint64_t used_mask = 0;
    auto generateCard = [&](uint64_t &mask){
        unsigned card;
        uint64_t card_mask;
        do {
            card = card_generator(rng);
            card_mask = 1ull << card;
        } while (mask & card_mask);
        mask |= card_mask;
        return card;
    };
    uint64_t sb_hand = 0;
    uint64_t bb_hand = 0;
    for(int i = 0; i < 3; ++i){
        sb_hand |= 1ull << generateCard(used_mask);
        bb_hand |= 1ull << generateCard(used_mask);
    }
    return {sb_hand, bb_hand};
}

struct RunoutCards {
    uint64_t flop = 0;
    uint64_t turn = 0;
    uint64_t river = 0;
    bool valid = false;
};

RunoutCards selectRunout(const ThreeCardGameTree& tree){
    RunoutCards runout;
    int river_index = -1;
    for(size_t i = 0; i < tree.board.size(); ++i){
        if(__builtin_popcountll(tree.board[i]) == 4){
            river_index = static_cast<int>(i);
            break;
        }
    }
    if(river_index < 0){
        return runout;
    }
    int turn_index = tree.parent_board[river_index];
    if(turn_index < 0){
        return runout;
    }
    int flop_index = tree.parent_board[turn_index];
    if(flop_index < 0){
        return runout;
    }
    uint64_t flop = tree.board[flop_index];
    uint64_t turn = tree.board[turn_index] & ~flop;
    uint64_t river = tree.board[river_index] & ~tree.board[turn_index];
    runout.flop = flop;
    runout.turn = turn;
    runout.river = river;
    runout.valid = true;
    return runout;
}

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
    int seed = 42;
    tree.prepare(seed);
    DCFRTrainer trainer;
    trainer.setTree(&tree);
    trainer.players[0].initPolicy(tree.getMovesPerInfoSet());
    trainer.players[1].initPolicy(tree.getMovesPerInfoSet());
    trainer.players[0].loadPolicy("./trained_model/player1_3289197.bin");
    trainer.players[1].loadPolicy("./trained_model/player1_3289197.bin");

    cout << fixed << setprecision(4);
    HoleCards hole_cards = sampleHoleCards(seed);
    RunoutCards runout = selectRunout(tree);
    cout << "sb hole: " << maskToString(hole_cards.sb_hand) << "\n";
    cout << "bb hole: " << maskToString(hole_cards.bb_hand) << "\n";
    if(runout.valid){
        cout << "runout: flop " << maskToString(runout.flop) << " | turn " << maskToString(runout.turn)
             << " | river " << maskToString(runout.river) << "\n";
    } else {
        cout << "runout: (unavailable)\n";
    }
    cout << "\n";
    node_id_counter = 0;
    ThreeCardGameState root = ThreeCardGameState();
    visualizeStrategyDepthLimited(&root, &tree, trainer.players, "", 0, 8);
}

int main(){
    visualizeThreeCardStrategy();
}
