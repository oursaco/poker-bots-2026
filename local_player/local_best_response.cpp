#include <array>
#include <vector>
#include <utility>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <omp.h>

#include "local_player/local_game_state.cpp"
#include "local_player/local_equities.cpp"


LocalAction get_best_action(LocalGameState state, DCFRPolicy& opp_policy, array<float, 22100> range, bool opp_bb, array<int, 3> hole_cards, int depth){
    vector<int> board = state.board;
    assert(
        (opp_bb && (hole_cards[0] == board[3] || hole_cards[1] == board[3] || hole_cards[2] == board[3])) ||
        (!opp_bb && (hole_cards[0] == board[2] || hole_cards[1] == board[2] || hole_cards[2] == board[2]))
    );
    vector<vector<pair<int,int>>> children; // children[i] is the children of the ith node
    vector<array<float, 22100>> node_ranges; // node_ranges[i] is the range of the ith node
    vector<pair<LocalGameState, int>> tree;
    vector<bool> is_terminal; // is_terminal[i] is true if the ith node is a terminal node
    tree.push_back(make_pair(state, 0));
    is_terminal.push_back(false);
    node_ranges.push_back(range);
    int index = 0, tree_size = 1;
    while(index < tree_size){
        LocalGameState* cur_state = &tree[index].first; // current state
        auto actions = cur_state->get_possible_actions(); // possible actions from current state
        bool has_children = false;
        for(int i=0; i<actions.size(); i++){
            auto action = actions[i].first;
            LocalGameState next_state = cur_state->get_next_state(action);
            if((tree[index].second >= depth && next_state.street < 3) || tree[index].second < depth){
                // if the node is at the depth limit AND the street is less than 3, we can proceed
                // if the node is not at the depth limit, we can proceed
                tree.push_back(make_pair(next_state, tree[index].second + 1));
                children[index].push_back(make_pair(tree_size, actions[i].second));
                is_terminal.push_back(false);
                // need to update range here and push to node_ranges :(
                tree_size++;
                has_children = true;
            }
        }
        if(!has_children){
            is_terminal[index] = true;
        } else{
            is_terminal[index] = false;
        }
        index++;
    }
    assert(tree.size() == children.size());
    assert(children.size() == is_terminal.size());
    vector<float> chipev;
    chipev.resize(tree.size(), 0);
    for(int i=tree.size()-1; i>=1; i--){
        if(is_terminal[i]){
            assert(board.size() >= 4); // we don't end searches before the flop is dealt
            assert(
                (opp_bb && (hole_cards[0] == board[3] || hole_cards[1] == board[3] || hole_cards[2] == board[3])) ||
                (!opp_bb && (hole_cards[0] == board[2] || hole_cards[1] == board[2] || hole_cards[2] == board[2]))
            );
            int discard_card = -1;
            if(opp_bb){
                if(hole_cards[0] == board[3]) discard_card = 0;
                else if(hole_cards[1] == board[3]) discard_card = 1;
                else if(hole_cards[2] == board[3]) discard_card = 2;
                else assert(false);
            } else{
                if(hole_cards[0] == board[2]) discard_card = 0;
                else if(hole_cards[1] == board[2]) discard_card = 1;
                else if(hole_cards[2] == board[2]) discard_card = 2;
                else assert(false);
            }
            vector<int> cards;
            for(int j=0; j<3; j++){
                if(j != discard_card)
                    cards.push_back(hole_cards[j]);
            }
            assert(cards.size() == 2);
            chipev[i] = get_equity(cards[0], cards[1], board, node_ranges[i].data(), opp_bb);
        } else{
            int turn = tree[i].first.street;
            auto action_distribution = tree[i].first.get_possible_actions();
            if((turn == 0 && opp_bb) || (turn == -1 && !opp_bb)){
                // our turn
                assert(chipev[i] == 0); // shoudln't have been modified yet
                float max_chipev = -1e9;
                for(int j=0; j<children[i].size(); j++){
                    float max_chipev = max(max_chipev, chipev[children[i][j].first]);
                }
                chipev[i] = max_chipev;
            } else{
                // opponent's turn
                assert(chipev[i] == 0); // shoudln't have been modified yet
                for(int j=0; j<children[i].size(); j++){
                    chipev[i] += chipev[children[i][j].first] * action_distribution[children[i][j].second].second;
                }
            }
        }
    }
    auto action_distribution = tree[0].first.get_possible_actions();
    float max_chipev = -1e9;
    LocalAction best_action = action_distribution[children[0][0].second].first;
    for(int i=0; i<children[0].size(); i++){
        if(chipev[children[0][i].first] > max_chipev){
            max_chipev = chipev[children[0][i].first];
            best_action = action_distribution[children[0][i].second].first;
        }
    }
    return best_action;
}