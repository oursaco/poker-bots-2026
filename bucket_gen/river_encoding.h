#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include <set>
#include <unordered_set>
#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <omp.h>
#include <fstream>
using namespace omp;
using namespace std;

omp::HandEvaluator eval;

int encodeBoard(Hand board, int s1, int s2){
    int str = eval.evaluate(board);
    int suited_state = -1;
    if(s1 == s2){
        // no shared suits, we check if there are 2 suits with 3 cards each
        if(board.suitCount(s1) == 0){
            int cnt = 0;
            for(int i = 0; i < 4; i++) if(i != s1) cnt += board.suitCount(i) == 3;
            if(cnt == 2){
                suited_state = 0;
                assert(suited_state >= 0 && suited_state <= 0);
                // suited state is 0
            } else { // only need to check max suit because there can only be one flush draw
                // 0 ... 6
                // we dont care about 0, 1, or 2
                int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
                mx_suit = min(4, max(0, mx_suit - 2));
                suited_state = 1 + mx_suit;
                assert(suited_state >= 1 && suited_state <= 5);
                //suited state is 1 ... 5
            }
        } else {
            // we need to check max suit outside of the same suit since there can only be one flush draw other than ours
            int mx_suit = max({board.suitCount((s1 + 1) % 4), board.suitCount((s1 + 2) % 4), board.suitCount((s1 + 3) % 4)});
            int same_suit = board.suitCount(s1);
            // we don't care about 0, 1, or 2
            mx_suit = min(4, max(0, mx_suit - 2));
            same_suit = min(4, max(0, same_suit - 2));
            suited_state = 6 + same_suit*5 + mx_suit;
            assert(suited_state >= 6 && suited_state <= 30);
            // suited state is 6 ... 30
        }
    } else {
        int same1 = board.suitCount(s1);
        int same2 = board.suitCount(s2);
        // no shared suits, we check if there are 2 suits with 3 cards each
        if(same1 == 0 && same2 == 0){
            int cnt = 0;
            for(int i = 0; i < 4; i++) if(i != s1 && i != s2) cnt += board.suitCount(i) == 3;
            if(cnt == 2){
                suited_state = 31;
                assert(suited_state >= 31 && suited_state <= 31);
                // suited state is 31
            } else { // only need to check max suit because there can only be one flush draw
                int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
                mx_suit = min(4, max(0, mx_suit - 2));
                suited_state = 32 + mx_suit;
                assert(suited_state >= 32 && suited_state <= 36);
                // suited state is 32 ... 36
            }
        } else {
            // we don't care about 0, 1, or 2
            same1 = min(4, max(0, same1 - 2));
            same2 = min(4, max(0, same2 - 2));
            // we need to check max suit outside of the same suit since there can only be one flush draw other than ours
            int mx_suit = 0;
            for(int i = 0; i < 4; i++) if(i != s1 && i != s2) mx_suit = max(mx_suit, (int)board.suitCount(i));
            mx_suit = min(4, max(0, mx_suit - 2));
            suited_state = 37 + same1*5*5 + same2*5 + mx_suit;
            assert(suited_state >= 37 && suited_state <= 161);
            // suited state is 37 ... 161
        }
    }
    return str*162 + suited_state;
}

int getHoleId(int p1, int p2, int ps1, int ps2){
    if(ps1 == ps2) return p1*13 + p2;
    return p2*13 + p1;
}