#include <iostream>
#include <fstream>
#include <vector>
#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "river_encoding.h"
using namespace omp;

using namespace std;

int encodeTurnBoard(Hand board, int s1, int s2)
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

int main(){
    std::ifstream map("emd_bucket_data/turn_encoding_map.bin", std::ios::binary);
    int encoding_index = 0;
    map.read(reinterpret_cast<char*>(&encoding_index), sizeof(int));
    std::vector<int> encoding_map((1 << 16) * 33, -1);
    map.read(reinterpret_cast<char*>(encoding_map.data()), encoding_map.size() * sizeof(int));

    cout << encoding_index << endl;

    std::ifstream eq("emd_bucket_data/turn_equity_1.bin", std::ios::binary);
    std::vector<float> equity1(100 * encoding_index);
    eq.read(reinterpret_cast<char*>(equity1.data()), equity1.size() * sizeof(float));
    std::ifstream eq2("emd_bucket_data/turn_equity_2.bin", std::ios::binary);
    std::vector<float> equity2(69 * encoding_index);
    eq2.read(reinterpret_cast<char*>(equity2.data()), equity2.size() * sizeof(float));

    std::ifstream eq3("emd_bucket_data/turn_equity.bin", std::ios::binary);
    std::vector<float> equity(169 * encoding_index);
    eq3.read(reinterpret_cast<char*>(equity.data()), equity.size() * sizeof(float));

    // int cnt = 0;
    // for(int i = 0; i < equity1.size(); i++)
    //     cnt += equity1[i] != -1;
    // for(int i = 0; i < equity2.size(); i++)
    //     cnt += equity2[i] != -1;
    // cout << cnt << " " << 169 * encoding_index << endl;

    // Hand board = Hand::empty() + Hand(48) + Hand(49) + Hand(1) + Hand(3) + Hand(6) + Hand(10); // As, Ah, 2h, 2d, 3c, 4c
    // Hand board = Hand::empty() + Hand(1) + Hand(5) + Hand(13) + Hand(17) + Hand(38); //2h, 3h, 5h, 6h, Jc
    Hand board = Hand::empty() + Hand(48) + Hand(49) + Hand(2) + Hand(7) + Hand(35); // A, A, 2
    int p1 = 50;
    int p2 = 51;
    int s1 = p1 % 4;
    int s2 = p2 % 4;
    int board_id = encoding_map[encodeTurnBoard(board, s1, s2)];
    // int hand_id = getHoleId(2, 2, 0, 1);
    // int hand_id = getHoleId(12, 12, 1, 2);
    int hand_id = getHoleId(p1/4, p2/4, s1, s2);
    int index = hand_id * encoding_index + board_id;
    float value = -1.0f;
    // if(index < equity1.size())
    //     value = equity1[index];
    // else
    //     value = equity2[index - (int)equity1.size()];
    value = equity[index];
    cout << value << endl;

    return 0;
}
