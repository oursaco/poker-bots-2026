#include <iostream>
#include <fstream>
#include <vector>
#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
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
    return eval.evaluate(board)*162 + suited_state;
}

int getHoleId(int p1, int p2, int ps1, int ps2){
    if(ps1 == ps2) return p1*13 + p2;
    return p2*13 + p1;
}

int main(){
    std::ifstream map("emd_bucket_data/encoding_map.bin", std::ios::binary);
    int encoding_index = 0;
    map.read(reinterpret_cast<char*>(&encoding_index), sizeof(int));
    std::vector<int> encoding_map((1 << 16) * 162, -1);
    map.read(reinterpret_cast<char*>(encoding_map.data()), encoding_map.size() * sizeof(int));

    cout << encoding_index << endl;

    std::ifstream eq("emd_bucket_data/river_equity.bin", std::ios::binary);
    std::vector<float> equity(169 * encoding_index);
    eq.read(reinterpret_cast<char*>(equity.data()), equity.size() * sizeof(float));

    // Hand board = Hand::empty() + Hand(48) + Hand(49) + Hand(1) + Hand(3) + Hand(6) + Hand(10); // As, Ah, 2h, 2d, 3c, 4c
    Hand board = Hand::empty() + Hand(1) + Hand(5) + Hand(13) + Hand(17) + Hand(38) + Hand(43); //2h, 3h, 5h, 6h, Jc, Qd
    int s1=2, s2=1;
    int board_id = encoding_map[encodeBoard(board, s1, s2)];
    cout << board_id << endl;
    // int hand_id = getHoleId(2, 2, 0, 1);
    int hand_id = getHoleId(8, 11, 2, 1);
    float value = equity[hand_id * encoding_index + board_id];
    cout << value << endl;

    return 0;
}
