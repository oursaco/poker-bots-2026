#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include <iostream>
#include <algorithm>
#include <fstream>
using namespace std;

namespace encoding {

omp::HandEvaluator eval;

int encodeRiverBoard(omp::Hand board, int s1, int s2){
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

int encodeTurnBoard(omp::Hand board, int s1, int s2)
{
    assert(board.count() == 5);
    int strength = eval.evaluate(board);
    int suited_state = -1;
    if (s1 == s2)
    {
        // suited
        if (board.suitCount(s1) <= 1)
        {
            // 221, 320, 311, 410, 500
            // 1(ours)211, 1(ours)22 (same as above), 1(ours)31, 1(ours)40
            int cnt = 0;
            for (int i = 0; i < 4; i++)
                cnt += board.suitCount(i) == 2;
            if (cnt == 2)
            {
                suited_state = 0; // 221
            }
            else
            {
                int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
                if (mx_suit == 2)
                    suited_state = 1; // 1(ours)211
                if (mx_suit == 3)
                {
                    if (!cnt)
                        suited_state = 2; // 311
                    else
                        suited_state = 3; // 320
                }
                else if (mx_suit == 4)
                    suited_state = 4; // 410
                else if (mx_suit == 5)
                    suited_state = 5; // 500
            }
            assert(suited_state >= 0 && suited_state <= 5);
        }
        else if (board.suitCount(s1) > 1)
        {
            // 2(ours)21, 2(ours)30, 2(ours)111
            // 3(ours)11, 3(ours)20
            // 4(ours)10
            // 5(ours)00
            // we need to check max suit outside of the same suit since there can only be one flush draw other than ours
            int cnt_suit = board.suitCount(s1);
            int mx_suit = max(max(board.suitCount((s1 + 1) % 4), board.suitCount((s1 + 2) % 4)), board.suitCount((s1 + 3) % 4));
            if (cnt_suit == 2)
            {
                assert(mx_suit >= 1 && mx_suit <= 3);
                if (mx_suit == 1)
                    suited_state = 6; // 2(ours)111
                else if (mx_suit == 2)
                    suited_state = 7; // 2(ours)21
                else if (mx_suit == 3)
                    suited_state = 8; // 2(ours)30
            }
            else if (cnt_suit == 3)
            {
                assert(mx_suit >= 1 && mx_suit <= 2);
                if (mx_suit == 1)
                    suited_state = 9; // 3(ours)11
                if (mx_suit == 2)
                    suited_state = 10; // 3(ours)20
            }
            else if (cnt_suit == 4)
                suited_state = 11; // 4(ours)10
            else if (cnt_suit == 5)
                suited_state = 12; // 5(ours)00
            assert(suited_state >= 6 && suited_state <= 12);
        }
    }
    else
    {
        // not suited
        int same1 = board.suitCount(s1);
        int same2 = board.suitCount(s2);
        int mx_suit = -1;
        for (int i = 0; i < 4; i++)
            if (i != s1 && i != s2)
                mx_suit = max(mx_suit, (int)board.suitCount(i));
        assert(mx_suit >= 0 && mx_suit <= 5);
        // 0005, 0014, 0023
        // 1004, 1013, 1022
        // 2003, 2021, 1103, 1112
        // 3002, 3011, 2102, 2111
        // 2201, 3110, 4001
        // 5000, 3200, 4100
        if (same1 + same2 == 0)
        {
            assert(mx_suit >= 3 && mx_suit <= 5);
            if (mx_suit == 3)
                suited_state = 13; // 0005
            else if (mx_suit == 4)
                suited_state = 14; // 0014
            else if (mx_suit == 5)
                suited_state = 15; // 0023
            assert(suited_state >= 13 && suited_state <= 15);
        }
        else if (same1 + same2 == 1)
        {
            assert(mx_suit >= 2 && mx_suit <= 4);
            if (mx_suit == 2)
                suited_state = 16; // 1022
            else if (mx_suit == 3)
                suited_state = 17; // 1013
            else if (mx_suit == 4)
                suited_state = 18; // 1004
            assert(suited_state >= 16 && suited_state <= 18);
        }
        else if (same1 + same2 == 2)
        {
            assert(mx_suit >= 2 && mx_suit <= 3);
            if (same1 == 0 || same2 == 0)
            {
                if (mx_suit == 2)
                    suited_state = 19; // 2021
                else if (mx_suit == 3)
                    suited_state = 20; // 2003
                assert(suited_state >= 19 && suited_state <= 20);
            }
            else
            {
                if (mx_suit == 2)
                    suited_state = 21; // 1103
                else if (mx_suit == 3)
                    suited_state = 22; // 1112
                assert(suited_state >= 21 && suited_state <= 22);
            }
        }
        else if (same1 + same2 == 3)
        {
            assert(mx_suit >= 1 && mx_suit <= 2);
            if (same1 == 0 || same2 == 0)
            {
                if (mx_suit == 1)
                    suited_state = 23; // 3011
                else if (mx_suit == 2)
                    suited_state = 24; // 3002
                assert(suited_state >= 23 && suited_state <= 24);
            }
            else
            {
                if (mx_suit == 1)
                    suited_state = 25; // 2111
                else if (mx_suit == 2)
                    suited_state = 26; // 2102
                assert(suited_state >= 25 && suited_state <= 26);
            }
        }
        else if (same1 + same2 == 4)
        {
            assert(mx_suit == 1);
            if (same1 == 2 && same2 == 2)
                suited_state = 27; // 2201
            if (max(same1, same2) == 3)
                suited_state = 28; // 3110
            if (max(same1, same2) == 4)
                suited_state = 29; // 4001
            assert(suited_state >= 27 && suited_state <= 29);
        }
        else
        {
            assert(same1 + same2 == 5);
            if (max(same1, same2) == 5)
                suited_state = 30; // 5000
            if (max(same1, same2) == 4)
                suited_state = 31; // 4100
            if (max(same1, same2) == 3)
                suited_state = 32; // 3200
            assert(suited_state >= 30 && suited_state <= 32);
        }
    }
    return strength * 33 + suited_state;
}

int encodeFlopBoard(omp::Hand board, int s1, int s2)
{
    int suited_state = -1;
    if(s1 == s2){
        if(board.suitCount(s1) == 0){
            int two_cnt = 0;
            for(int i = 0; i < 4; i++) if(board.suitCount(i) == 2) two_cnt++;
            int mx = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
            if(two_cnt == 2) suited_state = 0; // 2200
            else if(two_cnt == 1) suited_state = 1; // 2110
            else if(mx == 4) suited_state = 2; // 4000
            else if(mx == 3) suited_state = 3; // 3100
            else if(mx == 1) suited_state = 4; // 1111
            else assert(false);
            assert(suited_state >= 0 && suited_state <= 4);
        } else {
            int same = board.suitCount(s1) - 1;
            int two_cnt = 0;
            for(int i = 0; i < 4; i++) if(i != s1 && board.suitCount(i) == 2) two_cnt++;
            int mx = max({board.suitCount((s1 + 1) % 4), board.suitCount((s1 + 2) % 4), board.suitCount((s1 + 3) % 4)});
            if(two_cnt == 2) suited_state = 0; // 2200
            else if(two_cnt == 1) suited_state = 1; // 2110
            else if(mx == 4) suited_state = 2; // 4000
            else if(mx == 3) suited_state = 3; // 3100
            else if(mx == 1) suited_state = 4; // 1111
            else if(mx == 0) suited_state = 5; // 0000
            else assert(false);
            suited_state *= 4;
            suited_state += same;
            suited_state += 5;
            assert(suited_state >= 5 && suited_state <= 29);
        }
    } else {
        int same1 = board.suitCount(s1);
        int same2 = board.suitCount(s2);
        int mx = 0;
        for(int i = 0; i < 4; i++) if(i != s1 && i != s2) mx = max(mx, (int)board.suitCount(i));
        int two_cnt = 0;
        for(int i = 0; i < 4; i++) if(i != s1 && i != s2 && board.suitCount(i) == 2) two_cnt++;
        if(two_cnt == 2) suited_state = 0; // 2200
        else if(two_cnt == 1) suited_state = 1; // 2110
        else if(mx == 4) suited_state = 2; // 4000
        else if(mx == 3) suited_state = 3; // 3100
        else if(mx == 1) suited_state = 4; // 1111
        else if(mx == 0) suited_state = 5; // 0000
        else assert(false);
        suited_state *= 5;
        suited_state += same1;
        suited_state *= 5;
        suited_state += same2;
        suited_state += 30;
        assert(suited_state >= 30 && suited_state <= 180);
    }
    return eval.evaluate(board) * 181 + suited_state;
}

int encodeThreeBoard(omp::Hand board, int s1, int s2)
{
    int suited_state = -1;
    if(s1 == s2){
        if(board.suitCount(s1) == 0){
            int mx = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
            if(mx == 3) suited_state = 0; // 3000
            else if(mx == 2) suited_state = 1; // 210
            else if(mx == 1) suited_state = 2; // 1110
            assert(suited_state >= 0 && suited_state <= 2);
        } else {
            int same = board.suitCount(s1) - 1;
            int mx = max({board.suitCount((s1 + 1) % 4), board.suitCount((s1 + 2) % 4), board.suitCount((s1 + 3) % 4)});
            if(mx == 2) suited_state = 0; // 200
            else if(mx == 1) suited_state = 1; // 1100
            else if(mx == 0) suited_state = 2; // 0000
            else assert(false);
            suited_state *= 3;
            suited_state += same;
            suited_state += 3;
            assert(suited_state >= 3 && suited_state <= 11);
        }
    } else {
        int same1 = board.suitCount(s1);
        int same2 = board.suitCount(s2);
        int mx = 0;
        for(int i = 0; i < 4; i++) if(i != s1 && i != s2) mx = max(mx, (int)board.suitCount(i));
        if(mx == 3) suited_state = 0; // 3000
        else if(mx == 2) suited_state = 1; // 210
        else if(mx == 1) suited_state = 2; // 1110
        else if(mx == 0) suited_state = 3; // 0000
        else assert(false);
        suited_state *= 4;
        suited_state += same1;
        suited_state *= 4;
        suited_state += same2;
        suited_state += 12;
        assert(suited_state >= 12 && suited_state <= 76);
    }
    return eval.evaluate(board) * 77 + suited_state;
}

int getHoleId(int p1, int p2, int ps1, int ps2){
    if(ps1 == ps2) return p1*13 + p2;
    return p2*13 + p1;
}

static float river_eq[169][108345];
static float turn_eq[169][161057];
static float flop_eq[169][50921];
static float three_eq[169][8125];
static int preflop_buckets[52][52][52];
static int river_encoding_map[(1 << 16)*162];
static int turn_encoding_map[(1 << 16)*33];
static int flop_encoding_map[(1 << 16)*181];
static int three_encoding_map[(1 << 16)*77];

void readPreflop(string tar_dir){
    ifstream inf(tar_dir, ios::binary);
    int num_buckets;
    inf.read(reinterpret_cast<char*>(&num_buckets), sizeof(int));
    assert(num_buckets == 1755);
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            for(int k = j + 1; k < 52; k++){
                inf.read(reinterpret_cast<char*>(&preflop_buckets[i][j][k]), sizeof(unsigned));
            }
        }
    }
    inf.close();
}

void readRiverEncodingMap(string dir){
    ifstream inf(dir, ios::binary);
    int num_river_buckets;
    inf.read(reinterpret_cast<char*>(&num_river_buckets), sizeof(int));
    assert(num_river_buckets == 108345);
    for(int i = 0; i < (1 << 16)*162; i++){
        inf.read(reinterpret_cast<char*>(&river_encoding_map[i]), sizeof(int));
    }
    inf.close();
}

void readTurnEncodingMap(string dir){
    ifstream inf(dir, ios::binary);
    int num_turn_buckets;
    inf.read(reinterpret_cast<char*>(&num_turn_buckets), sizeof(int));
    assert(num_turn_buckets == 161057);
    for(int i = 0; i < (1 << 16)*33; i++){
        inf.read(reinterpret_cast<char*>(&turn_encoding_map[i]), sizeof(int));
    }
    inf.close();
}

void readFlopEncodingMap(string dir){
    ifstream inf(dir, ios::binary);
    int num_flop_buckets;
    inf.read(reinterpret_cast<char*>(&num_flop_buckets), sizeof(int));
    assert(num_flop_buckets == 50921);
    for(int i = 0; i < (1 << 16)*181; i++){
        inf.read(reinterpret_cast<char*>(&flop_encoding_map[i]), sizeof(int));
    }
    inf.close();
}

void readThreeEncodingMap(string dir){
    ifstream inf(dir, ios::binary);
    int num_three_buckets;
    inf.read(reinterpret_cast<char*>(&num_three_buckets), sizeof(int));
    assert(num_three_buckets == 8125);
    for(int i = 0; i < (1 << 16)*77; i++){
        inf.read(reinterpret_cast<char*>(&three_encoding_map[i]), sizeof(int));
    }
    inf.close();
}

void readRiverEquity(string dir){
    ifstream inf(dir, ios::binary);
    for(int i = 0; i < 169; i++){
        for(int j = 0; j < 108345; j++){
            inf.read(reinterpret_cast<char*>(&river_eq[i][j]), sizeof(float));
        }
    }
    inf.close();
}

void readTurnEquity(string dir){
    ifstream inf(dir, ios::binary);
    for(int i = 0; i < 169; i++){
        for(int j = 0; j < 161057; j++){
            inf.read(reinterpret_cast<char*>(&turn_eq[i][j]), sizeof(float));
        }
    }
    inf.close();
}

void readThreeEquity(string dir){
    ifstream inf(dir, ios::binary);
    for(int i = 0; i < 169; i++){
        for(int j = 0; j < 8125; j++){
            inf.read(reinterpret_cast<char*>(&three_eq[i][j]), sizeof(float));
        }
    }
    inf.close();
}

void readFlopEquity(string dir){
    ifstream inf(dir, ios::binary);
    for(int i = 0; i < 169; i++){
        for(int j = 0; j < 50921; j++){
            inf.read(reinterpret_cast<char*>(&flop_eq[i][j]), sizeof(float));
        }
    }
    inf.close();
}


}