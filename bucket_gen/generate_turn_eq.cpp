#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"

using namespace omp;
using namespace std;

int board[5];
omp::HandEvaluator eval;
float wins[169][135991];
float loses[169][135991];
float ties[169][135991];

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

int histogram(int p1, int p2){
    std::ifstream map("emd_bucket_data/encoding_map.bin", std::ios::binary);
    int encoding_index = 0;
    map.read(reinterpret_cast<char*>(&encoding_index), sizeof(int));
    std::vector<int> encoding_map((1 << 16) * 162, -1);
    map.read(reinterpret_cast<char*>(encoding_map.data()), encoding_map.size() * sizeof(int));

    std::ifstream eq("emd_bucket_data/river_equity.bin", std::ios::binary);
    std::vector<float> equity(169 * encoding_index);
    eq.read(reinterpret_cast<char*>(equity.data()), equity.size() * sizeof(float));
    for(int b1 = 0; b1 < 52; b1++){
        if(b1 == p1 || b1 == p2) continue;
        for(int b2 = b1 + 1; b2 < 52; b2++){
            if(b2 == p1 || b2 == p2) continue;
            for(int b3 = b2 + 1; b3 < 52; b3++){
                if(b3 == p1 || b3 == p2) continue;
                for(int b4 = b3 + 1; b4 < 52; b4++){
                    if(b4 == p1 || b4 == p2) continue;
                    for(int b5 = b4 + 1; b5 < 52; b5++){
                        if(b5 == p1 || b5 == p2) continue;
                        for(int b6 = 0; b6 < 52; b6++){
                            if(b6 == p1 || b6 == p2 || b6 == b1 || b6 == b2 || b6 == b3 || b6 == b4 || b6 == b5) continue;
                            Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            int board_id = encoding_map[encodeBoard(board, s1, s2)];
                            if(board_id < 0) continue;
                            int hand_id = getHoleId(p1, p2, ps1, ps2);
                            float value = equity[hand_id * encoding_index + board_id];

                            //stuff
                        }
                    }
                }
            }
        }
    }
}

int encodeBoard(Hand board, int s1, int s2){
    assert(board.count() == 5);
    int suited_state = -1;
    int strength = eval.evaluate(board);
    if(s1 == s2){
        if(board.suitCount(s1) == 0){
            // there can only be one flush draw
            int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
            mx_suit = min(3, max(0,mx_suit-2));
            suited_state = max_suit;
            assert(suited_state >= 0 && suited_state <= 3);
        } else{
            // we need to check max suit outside of the same suit since there can only be one flush draw other than ours
            int mx_suit = max({board.suitCount((s1+1)%4), board.suitCount((s1+2)%4), board.suitCount((s1+3)%4)});
            int same_suit = board.suitCount(s1);
            // we don't care about 0, 1, or 2
            mx_suit = min(3, max(0, mx_suit-2));
            same_suit = min(3, max(0, same_suit-2));
            suited_state = 4 + same_suit*4 + max_suit;
            assert(suited_state >= 4 && suited_state <= 19);
        }
    } else{
        int same1 = board.suitCount(s1);
        int same2 = board.suitCount(s2);
        if(same1 == 0 && same2 == 0){
            int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
            mx_suit = min(3, max(0, mx_suit-2));
            suited_state = 20 + mx_suit;
            assert(suited_state >= 20 && suited_state <= 23);
        } else{
            same1 = min(3, max(0, same1-2));
            same2 = min(3, max(0, same2-2));

            int mx_suit = 0;
            for(int i=0; i<4; i++)
                if(i != s1 && i != s2)
                    mx_suit = max(mx_suit, board.suitCount(i));
            mx_suit = min(3, max(0, mx_suit-2)); 
            suited_state = 24 + same1*4*4 + same2*4 + mx_suit;

            assert(suited_state >= 24 && suited_state <= 88);
        }
    }
    return strength * 89 + suited_state;
}

int calculateHistogram(int p1, int p2){
    array<int, 52> deck;
    mt19937 rng(random_device{}());

    for(int b1=0; b1<52; b1++){
        if(b1 == p1 || b1 == p2) continue;
        for(int b2=b1+1; b2<52; b2++){
            if(b2 == p1 || b2 == p2) continue;
            for(int b3=b2+1; b3<52; b3++){
                if(b3 == p1 || b3 == p2) continue;
                for(int b4=b3+1; b4<52; b4++){
                    if(b4 == p1 || b4 == p2) continue;
                    for(int b5=b4+1; b5<52; b5++){
                        if(b5 == p1 || b5 == p2) continue;
                        for(int b6=0; b6<52; b6++){
                            if(
                                b6 == p1 || b6 == p2 ||
                                b6 == b1 || b6 == b2 || b6 == b3 || b6 == b4 || b6 == b5
                            ) continue;
                            Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            
                            //need to get equity of board from river_equity.bin
                           
                            }

                        }
                    }
                }
            }
        }
    }
}