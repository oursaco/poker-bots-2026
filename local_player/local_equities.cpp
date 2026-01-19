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

void normalize(float range[22100]){
    float sum = 0;
    for(int i=0; i<22100; i++){
        sum += range[i];
    }
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

float get_river_equity(int p1, int p2, vector<int> board, float range[22100], bool opp_bb){
    assert(board.size() == 6);
    //update range
    for(int i=0; i<22100; i++){
        int cnt = 0;
        for(int j=0; j<6; j++){
            cnt += (hand_map[i][0] == board[j]) + (hand_map[i][1] == board[j]) + (hand_map[i][2] == board[j]);
        }
        if(
            cnt != 1 || // (not) one hole card on the board
            (opp_bb && hand_map[i][0] != board[2] && hand_map[i][1] != board[2] && hand_map[i][2] != board[2]) || // opp's hole card in the wrong spot
            (!opp_bb && hand_map[i][0] != board[3] && hand_map[i][1] != board[3] && hand_map[i][2] != board[3]) // opp's hole card in the wrong spot
        ){
            range[i] = 0;
        }
    }
    normalize(range);
    //calculate equity
    float eq = 0;
    for(int i=0; i<22100; i++){
        if(range[i] > 0){
            assert(
                (opp_bb && (hand_map[i][0] == board[2] || hand_map[i][1] == board[2] || hand_map[i][2] == board[2])) || // opponent hole card on bb discard spot
                (!opp_bb && (hand_map[i][0] == board[3] || hand_map[i][1] == board[3] || hand_map[i][2] == board[3])) // opponent hole card on sb discard spot
            );
            int cnt = 0;
            for(int j=0; j<6; j++){
                cnt += (hand_map[i][0] == board[j]) + (hand_map[i][1] == board[j]) + (hand_map[i][2] == board[j]);
            }
            assert(cnt == 1);
            vector<int> opp_cards;
            cnt = 0;
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
    assert(0 <= eq && eq <= 1);
    return eq;
}

float get_turn_equity(int p1, int p2, vector<int> board, float range[22100], bool opp_bb){
    assert(board.size() == 5);
    float eq = 0, cnt = 0;
    //deal river card
    for(int b=0; b<52; b++){
        if(b == board[0] || b == board[1] || b == board[2] || b == board[3] || b == board[4] || p1 == b || p2 == b) continue;
        cnt++;
        vector<int> new_board = board;
        new_board.push_back(b);
        assert(new_board.size() == 6);
        eq += get_river_equity(p1, p2, new_board, range, opp_bb);
        //get equity for specific river card
    }
    assert(0 <= eq/cnt && eq/cnt <= 1);
    return eq/cnt;
}

float get_flop_equity(int p1, int p2, vector<int> board, float range[22100], bool opp_bb){
    assert(board.size() == 4);
    float eq = 0, cnt = 0;
    //get turn and river cards
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
            //get equity for specific turn and river cards
            vector<int> new_board = board;
            new_board.push_back(b1);
            new_board.push_back(b2);
            assert(new_board.size() == 6);
            eq += get_river_equity(p1, p2, new_board, range, opp_bb);
        }
    }
    assert(0 <= eq/cnt && eq/cnt <= 1);
    return eq/cnt;
}

float get_equity(int p1, int p2, vector<int> board, float range[22100], bool opp_bb){
    assert(board.size() == 4 || board.size() == 5 || board.size() == 6);
    float val = -1;
    if(board.size() == 4){
        val = get_flop_equity(p1, p2, board, range, opp_bb);
    } else if(board.size() == 5){
        val = get_turn_equity(p1, p2, board, range, opp_bb);
    } else if(board.size() == 6){
        val = get_river_equity(p1, p2, board, range, opp_bb);
    }
    assert(0 <= val && val <= 1);
    return val;
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

int main(){

    generateHoleCards();
    cout << evaluate_river(48, 49, {50, 51, 0, 1, 2, 3}) << endl;

    return 0;
}