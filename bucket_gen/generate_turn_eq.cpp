#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "river_encoding.h"

using namespace omp;
using namespace std;

int encoding_map[(1 << 16) * 33];
int encoding_index = 0;

float num_rivers[169][161057];
float turn_equity[169][161057];

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

void solve(int p1, int p2)
{
    assert(p1 >= 0 && p1 < 52 && p2 >= 0 && p2 < 52 && p1 != p2);
    std::ifstream map("emd_bucket_data/encoding_map.bin", std::ios::binary);
    int river_encoding_index = 0;
    map.read(reinterpret_cast<char *>(&river_encoding_index), sizeof(int));
    std::vector<int> river_encoding_map((1 << 16) * 162, -1);
    map.read(reinterpret_cast<char *>(river_encoding_map.data()), river_encoding_map.size() * sizeof(int));

    std::ifstream eq("emd_bucket_data/river_equity.bin", std::ios::binary);
    std::vector<float> river_equity(169 * river_encoding_index);
    eq.read(reinterpret_cast<char *>(river_equity.data()), river_equity.size() * sizeof(float));
    for (int b1 = 0; b1 < 52; b1++){
        if (b1 == p1 || b1 == p2) continue;
        for (int b2 = b1 + 1; b2 < 52; b2++){
            if (b2 == p1 || b2 == p2) continue;
            for (int b3 = b2 + 1; b3 < 52; b3++){
                if (b3 == p1 || b3 == p2) continue;
                for (int b4 = b3 + 1; b4 < 52; b4++){
                    if (b4 == p1 || b4 == p2) continue;
                    for (int b5 = b4 + 1; b5 < 52; b5++){
                        if (b5 == p1 || b5 == p2) continue;
                        Hand turn = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5);
                        int s1 = p1 % 4;
                        int s2 = p2 % 4;
                        // int s1 = (p1 < p2) ? (p1 % 4) : (p2 % 4); // s1 is the suit of the first player
                        // int s2 = (p1 < p2) ? (p2 % 4) : (p1 % 4); // s2 is the suit of the second player
                        int turn_id = encoding_map[encodeTurnBoard(turn, s1, s2)];
                        assert(turn_id >= 0);
                        for (int b6 = 0; b6 < 52; b6++){
                            if (b6 == p1 || b6 == p2 || b6 == b1 || b6 == b2 || b6 == b3 || b6 == b4 || b6 == b5)
                                continue;
                            Hand river = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            int board_id = river_encoding_map[encodeBoard(river, s1, s2)];
                            assert(board_id >= 0);
                            // int hand_id = (p1 < p2) ? getHoleId(p1 / 4, p2 / 4, s1, s2) : getHoleId(p2 / 4, p1 / 4, s1, s2);
                            int hand_id = getHoleId(p1/4, p2/4, s1, s2);
                            assert(hand_id >= 0);
                            float eq = river_equity[hand_id * river_encoding_index + board_id];
                            turn_equity[hand_id][turn_id] += eq;
                            num_rivers[hand_id][turn_id]++;
                        }
                    }
                }
            }
        }
    }
}

void generateEncodingMap()
{
    for (int i = 0; i < (1 << 16) * 33; i++)
        encoding_map[i] = -1;
    for (int b1 = 0; b1 < 52; b1++)
    {
        for (int b2 = b1 + 1; b2 < 52; b2++)
        {
            for (int b3 = b2 + 1; b3 < 52; b3++)
            {
                for (int b4 = b3 + 1; b4 < 52; b4++)
                {
                    for (int b5 = b4 + 1; b5 < 52; b5++)
                    {
                        Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5);
                        for (int s1 = 0; s1 < 4; s1++)
                        {
                            for (int s2 = 0; s2 < 4; s2++)
                            {
                                int encoded = encodeTurnBoard(board, s1, s2);
                                if (encoding_map[encoded] != -1)
                                    continue;
                                encoding_map[encoded] = encoding_index++;
                            }
                        }
                    }
                }
            }
        }
    }
    cout << "number of turn boards: " << encoding_index << endl;
}

void saveEncodingMap(string dir)
{
    ofstream ouf(dir, ios::binary);
    ouf.write(reinterpret_cast<const char *>(&encoding_index), sizeof(int));
    for (int i = 0; i < (1 << 16) * 33; i++)
    {
        ouf.write(reinterpret_cast<const char *>(&encoding_map[i]), sizeof(int));
    }
    ouf.close();
}

void saveEquity(string dir1, string dir2)
{
    ofstream ouf1(dir1, ios::binary);
    ofstream ouf2(dir2, ios::binary);
    int visited = 0;
    for (int i = 0; i < 100; i++)
    {
        for (int j = 0; j < encoding_index; j++)
        {
            float eq = -1.0f;
            if (num_rivers[i][j] > 0)
            {
                eq = turn_equity[i][j] / (float)num_rivers[i][j];
            }
            ouf1.write(reinterpret_cast<const char *>(&eq), sizeof(float));
        }
    }
    for (int i = 100; i < 169; i++)
    {
        for (int j = 0; j < encoding_index; j++)
        {
            float eq = -1.0f;
            if (num_rivers[i][j] > 0)
            {
                eq = turn_equity[i][j] / (float)num_rivers[i][j];
            }
            ouf2.write(reinterpret_cast<const char *>(&eq), sizeof(float));
        }
    }
    cout << "visited nodes: " << visited << endl;
    ouf1.close();
    ouf2.close();
}

void tests(){
    Hand board = Hand::empty() + Hand(48) + Hand(49) + Hand(50) + Hand(51) + Hand(3); // Ad, Ac, As, Ah, 2d
    int s1 = 0, s2 = 3;                                                               // s, d
    assert(eval.evaluate(board) * 33 + 25 == encodeTurnBoard(board, s1, s2));
    board = Hand::empty() + Hand(32) + Hand(9) + Hand(42) + Hand(23) + Hand(0); // Ts, 4h, Qc, 7d, 2s
    s1 = 0;
    s2 = 0;
    assert(eval.evaluate(board) * 33 + 6 == encodeTurnBoard(board, s1, s2));
    board = Hand::empty() + Hand(20) + Hand(27) + Hand(0) + Hand(1) + Hand(17); // 7s, 8d, 2s, 2h, 6h
    s1 = 1;
    s2 = 3;
    assert(eval.evaluate(board) * 33 + 26 == encodeTurnBoard(board, s1, s2));
}

int main(){
    
    generateEncodingMap();
    vector<pair<int, int>> cards[169];
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            cards[getHoleId(i/4, j/4, i%4, j%4)].push_back(make_pair(i, j));
        }
    }
    cout << "checkpoint 1" << endl;
    saveEncodingMap("./emd_bucket_data/turn_encoding_map.bin");
    cout << "checkpoint 2" << endl;

    #pragma omp parallel for schedule(static)
    for(int t = 0; t < 169; t++){
        int st = 0;
        cout << "Generating equity for hand class " << t << endl;
        for(pair<int, int> p : cards[t]){
            int i = p.first;
            int j = p.second;
            solve(i, j);
        }
    }
    saveEquity("./emd_bucket_data/turn_equity_1.bin", "./emd_bucket_data/turn_equity_2.bin");
    return 0;
}