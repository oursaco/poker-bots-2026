#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <vector>

#include "cfr/CFR.hpp"
#include "cfr/CFRFast.hpp"
#include "game/GameTree.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCardState.hpp"
#include "external/omp/Hand.h"
#include "cfr/CFRFast.hpp"

omp::HandEvaluator eval;

using namespace omp;
using namespace std;

array<array<int, 3>, 22100> hand_map;
float range[22100];

void normalize(){
    float sum = 0;
    for(int num: range)
        sum += num;
    for(int i=0; i<22100; i++){
        range[i] /= sum;
    }
}

int evaluate_river(int p1, int p2, vector<int> board){
    assert(board.size() == 6);
    Hand board1 = Hand::empty() + Hand(board[0]) + Hand(board[1]) + Hand(board[2]) + Hand(board[3]) + Hand(board[4]) + Hand(board[5]);
    Hand board2 = Hand::empty() + Hand(board[1]) + Hand(board[2]) + Hand(board[3]) + Hand(board[4]) + Hand(board[5]);
    Hand board3 = Hand::empty() + Hand(board[0]) + Hand(board[2]) + Hand(board[3]) + Hand(board[4]) + Hand(board[5]);
    Hand board4 = Hand::empty() + Hand(board[0]) + Hand(board[1]) + Hand(board[3]) + Hand(board[4]) + Hand(board[5]);
    Hand board5 = Hand::empty() + Hand(board[0]) + Hand(board[1]) + Hand(board[2]) + Hand(board[4]) + Hand(board[5]);
    Hand board6 = Hand::empty() + Hand(board[0]) + Hand(board[1]) + Hand(board[2]) + Hand(board[3]) + Hand(board[5]);
    Hand board7 = Hand::empty() + Hand(board[0]) + Hand(board[1]) + Hand(board[2]) + Hand(board[3]) + Hand(board[4]);
    Hand player = Hand(p1) + Hand(p2);
    int strength = max({eval.evaluate(board1 + Hand(p1)), 
        eval.evaluate(board1 + Hand(p2)), 
        eval.evaluate(board2 + player), 
        eval.evaluate(board3 + player), 
        eval.evaluate(board4 + player), 
        eval.evaluate(board5 + player), 
        eval.evaluate(board6 + player), 
        eval.evaluate(board7 + player)});
    return strength;
}

float get_river_equity(int p1, int p2, vector<int> board, bool opp_bb){
    assert(board.size() == 6);
    for(int i=0; i<22100; i++){
        if(range[i] > 0){
            if(
                !(
                ((opp_bb && (hand_map[i][0] == board[2] || hand_map[i][1] == board[2] || hand_map[i][2] == board[2])) ||
                (!opp_bb && (hand_map[i][0] == board[3] || hand_map[i][1] == board[3] || hand_map[i][2] == board[3]))) ||
                ) ||
                (hand_map[i][0] == board[0] || hand_map[i][1] == board[0] || hand_map[i][2] == board[0]) ||
                (hand_map[i][0] == board[1] || hand_map[i][1] == board[1] || hand_map[i][2] == board[1]) ||
                (hand_map[i][0] == board[2] || hand_map[i][1] == board[2] || hand_map[i][2] == board[2]) ||
                (hand_map[i][0] == board[3] || hand_map[i][1] == board[3] || hand_map[i][2] == board[3]) ||
                (hand_map[i][0] == board[4] || hand_map[i][1] == board[4] || hand_map[i][2] == board[4]) ||
                (hand_map[i][0] == board[5] || hand_map[i][1] == board[5] || hand_map[i][2] == board[5])
            ) range[i] = 0;
        }
    }
    normalize();
    float eq = 0;
    for(int i=0; i<22100; i++){
        if(range[i] > 0){
            assert(
                (opp_bb && (hand_map[i][0] == board[2] || hand_map[i][1] == board[2] || hand_map[i][2] == board[2])) ||
                (!opp_bb && (hand_map[i][0] == board[3] || hand_map[i][1] == board[3] || hand_map[i][2] == board[3]))
            );
            assert(hand_map[i][0] != board[0] && hand_map[i][1] != board[0] && hand_map[i][2] != board[0]);
            assert(hand_map[i][0] != board[1] && hand_map[i][1] != board[1] && hand_map[i][2] != board[1]);
            assert(hand_map[i][0] != board[2] && hand_map[i][1] != board[2] && hand_map[i][2] != board[2]);
            assert(hand_map[i][0] != board[3] && hand_map[i][1] != board[3] && hand_map[i][2] != board[3]);
            assert(hand_map[i][0] != board[4] && hand_map[i][1] != board[4] && hand_map[i][2] != board[4]);
            assert(hand_map[i][0] != board[5] && hand_map[i][1] != board[5] && hand_map[i][2] != board[5]);
            vector<int> opp_cards;
            for(int j=0; j<3; j++){
                if(hand_map[i][j] != board[2])
                    opp_cards.push_back(hand_map[i][j]);
            }
            assert(opp_cards.size() == 2);
            int strength = evaluate_river(p1, p2, board) - evaluate_river(opp_cards[0], opp_cards[1], board);
            if(strength > 0) eq += range[i];
            else eq -= range[i];
        }
    }
}

float get_turn_equity(int p1, int p2, vector<int> board, bool opp_bb){
    assert(board.size() == 5);
    float eq = 0, cnt = 0;
    for(int b=0; b<52; b++){
        if(b == board[0] || b == board[1] || b == board[2] || b == board[3] || b == board[4] || p1 == b || p2 == b) continue;
        cnt++;
        vector<int> new_board = board;
        new_board.push_back(b);
        eq += get_river_equity(p1, p2, new_board, opp_bb);
    }
    return eq/cnt;
}

float get_flop_equity(int p1, int p2, vector<int> board, bool opp_bb){
    assert(board.size() == 4);
    float eq = 0, cnt = 0;
    for(int b1=0; b1<52; b1++){
        for(int b2=b1+1; b2<52; b2++){
            if(
                (b1 == board[0] || b2 == board[0]) ||
                (b1 == board[1] || b2 == board[1]) ||
                (b1 == board[2] || b2 == board[2]) ||
                (b1 == board[3] || b2 == board[3]) ||
                (p1 == b1 || p2 == b1 || p1 == b2 || p2 == b2)
            ) continue;
            cnt++;
            vector<int> new_board = board;
            new_board.push_back(b1);
            new_board.push_back(b2);
            eq += get_river_equity(p1, p2, new_board, opp_bb);
        }
    }
    for(int b=0; b<52; b++){
        bool on_board = false;
        for(int i=0; i<board.size(); i++){
            if(board[i] == b)
                on_board = true;
        }
        if(p1 == b || p2 == b) continue;
        if(on_board) continue;
        cnt++;
        board.push_back(b);
        eq += get_river_equity(p1, p2, board, opp_bb);
    }
    return eq/cnt;
}

void generateHoleCards(){
    int cnt = 0;
    for(int i=0; i<52; i++){
        for(int j=i+1; j<52; j++){
            for(int k=j+1; k<52; k++){
                array<int, 3> hand = {i,j,k};
                assert(cnt < 22100);
                hand_map[cnt++] = hand;
            }
        }
    }
}

float winProbability(int p1, int p2, set<int> board, int cards_to_gen){
    Hand cur_board = Hand::empty();
    for(int card: board)
        cur_board += Hand(card);
    if(cards_to_gen == 0){
        for(int i=0; i<22100; i++){

        }
    }
}

void localBR(DCFRPolicy& policy, ThreeCardGameState* state, Hand hole_cards){
    float wp; // win probability
    float pot = state->pot;
    auto actions = state->generateActions();
    for(auto& game_action : actions){
        auto action = static_cast<ThreeCardAction*>(game_action.second.get());
        float amount = action->amount;
        float new_range[22100];
        float fp = 0.0f;

        float sum = 0;
        for(auto u: new_range)
            sum += u;
        for(int i=0; i<22100; i++){
            new_range[i] /= sum;
        }
        float wp; // win probability
        // float utility = fp * pot + (1-fp) * (wp * (pot + asked) - (1-wp) * (asked +
    }
}

int main(){

    generateHoleCards();
    // cout << get<0>(hand_map[0]) << " " << get<1>(hand_map[0]) << " " << get<2>(hand_map[0]) << endl;


    return 0;
}