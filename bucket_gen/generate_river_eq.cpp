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

int board[6];
omp::HandEvaluator eval;
float wins[169][135991];
float loses[169][135991];
float ties[169][135991];

int getHoleId(int p1, int p2, int ps1, int ps2){
    if(ps1 == ps2) return p1*13 + p2;
    return p2*13 + p1;
}

int encoding_map[(1 << 16)*162];
int encoding_index = 0;

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
            suited_state = 37 + same1*5*5 + same2*5 + mx_suit;
            assert(suited_state >= 37 && suited_state <= 161);
            // suited state is 37 ... 161
        }
    }
    return eval.evaluate(board)*162 + suited_state;
}

void solve(int p1, int p2, int o1, int o2){
    for(int b1 = 0; b1 < 52; b1++){
        if(b1 == p1 || b1 == p2 || b1 == o1 || b1 == o2) continue;
        for(int b2 = b1 + 1; b2 < 52; b2++){
            if(b2 == p1 || b2 == p2 || b2 == o1 || b2 == o2) continue;
            for(int b3 = b2 + 1; b3 < 52; b3++){
                if(b3 == p1 || b3 == p2 || b3 == o1 || b3 == o2) continue;
                for(int b4 = b3 + 1; b4 < 52; b4++){
                    if(b4 == p1 || b4 == p2 || b4 == o1 || b4 == o2) continue;
                    for(int b5 = b4 + 1; b5 < 52; b5++){
                        if(b5 == p1 || b5 == p2 || b5 == o1 || b5 == o2) continue;
                        for(int b6 = b5 + 1; b6 < 52; b6++){
                            if(b6 == p1 || b6 == p2 || b6 == o1 || b6 == o2) continue;
                            Hand board1 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board2 = Hand::empty() + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board3 = Hand::empty() + Hand(b1) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board4 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board5 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b5) + Hand(b6);
                            Hand board6 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b6);
                            Hand board7 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5);
                            Hand player = Hand(p1) + Hand(p2);
                            Hand opp = Hand(o1) + Hand(o2);
                            int player_str = max({eval.evaluate(board1 + Hand(p1)), 
                                                  eval.evaluate(board1 + Hand(p2)), 
                                                  eval.evaluate(board2 + player), 
                                                  eval.evaluate(board3 + player), 
                                                  eval.evaluate(board4 + player), 
                                                  eval.evaluate(board5 + player), 
                                                  eval.evaluate(board6 + player), 
                                                  eval.evaluate(board7 + player)});
                            int opp_str = max({eval.evaluate(board1 + Hand(o1)), 
                                               eval.evaluate(board1 + Hand(o2)), 
                                               eval.evaluate(board2 + opp), 
                                               eval.evaluate(board3 + opp), 
                                               eval.evaluate(board4 + opp), 
                                               eval.evaluate(board5 + opp), 
                                               eval.evaluate(board6 + opp), 
                                               eval.evaluate(board7 + opp)});
                            int diff = player_str - opp_str;
                            int ps1 = p1%4;
                            int ps2 = p2%4;
                            int player_id = getHoleId(p1/4, p2/4, ps1, ps2);
                            if(diff > 0) wins[player_id][encoding_map[encodeBoard(board1, ps1, ps2)]]++;
                            else if(diff < 0) loses[player_id][encoding_map[encodeBoard(board1, ps1, ps2)]]++;
                            else ties[player_id][encoding_map[encodeBoard(board1, ps1, ps2)]]++;
                        }
                    }
                }
            }
        }
    }
}

void generateEncodingMap(){
    for(int b1 = 0; b1 < 52; b1++){
        for(int b2 = b1 + 1; b2 < 52; b2++){
            for(int b3 = b2 + 1; b3 < 52; b3++){
                for(int b4 = b3 + 1; b4 < 52; b4++){
                    for(int b5 = b4 + 1; b5 < 52; b5++){
                        for(int b6 = b5 + 1; b6 < 52; b6++){
                            Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            for(int s1 = 0; s1 < 4; s1++){
                                for(int s2 = 0; s2 < 4; s2++){
                                    int encoded = encodeBoard(board, s1, s2);
                                    if(encoding_map[encoded] != 0) continue;
                                    encoding_map[encoded] = encoding_index++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    cout << "number of river boards: " << encoding_index << endl;
}

void saveEncodingMap(string dir){
    ofstream ouf(dir, ios::binary);
    ouf.write(reinterpret_cast<const char*>(&encoding_index), sizeof(int));
    for(int i = 0; i < encoding_index; i++){
        ouf.write(reinterpret_cast<const char*>(&encoding_map[i]), sizeof(int));
    }
    ouf.close();
}

void saveEquity(string dir){
    ofstream ouf(dir, ios::binary);
    for(int i = 0; i < 169; i++){
        for(int j = 0; j < encoding_index; j++){
            assert(wins[i][j] + loses[i][j] + ties[i][j] > 0);
            float eq = float(wins[i][j])/float(wins[i][j] + loses[i][j] + ties[i][j]);
            ouf.write(reinterpret_cast<const char*>(&eq), sizeof(float));
        }
    }
    ouf.close();
}

int main(){
    generateEncodingMap();
    vector<pair<int, int>> cards[169];
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            cards[getHoleId(i/4, j/4, i%4, j%4)].push_back(make_pair(i, j));
        }
    }
    saveEncodingMap("./emd_bucket_data/encoding_map.bin");
    #pragma omp parallel for
    for(int t = 0; t < 169; t++){
        int st = 0;
        int tot = cards[t].size() * 50;
        for(pair<int, int> p : cards[t]){
            int i = p.first;
            int j = p.second;
            for(int k = 0; k < 52; k++){
                if(k == i || k == j) continue;
                for(int l = k + 1; l < 52; l++){
                    if(l == i || l == j) continue;
                    auto start = chrono::high_resolution_clock::now();
                    solve(i, j, k, l);
                }
                cout << "Finished " << st << " / " << tot << endl;
                st++;
            }
        }
    }
    saveEquity("./emd_bucket_data/equity.bin");
    
    return 0;
}