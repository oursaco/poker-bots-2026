#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "encoding.h"
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
// omp::HandEvaluator eval;
float wins[169][135991];
float loses[169][135991];
float ties[169][135991];

int encoding_map[(1 << 16)*162];
int encoding_index = 0;

omp::HandEvaluator eval;

void solve(int p1, int p2){
    int ps1 = p1%4;
    int ps2 = p2%4;
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
                        for(int b6 = b5 + 1; b6 < 52; b6++){
                            if(b6 == p1 || b6 == p2) continue;
                            int w = 0;
                            int t = 0;
                            int l = 0;
                            Hand board1 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board2 = Hand::empty() + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board3 = Hand::empty() + Hand(b1) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board4 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board5 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b5) + Hand(b6);
                            Hand board6 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b6);
                            Hand board7 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5);
                            Hand player = Hand(p1) + Hand(p2);
                            int player_str = max({eval.evaluate(board1 + Hand(p1)), 
                                                eval.evaluate(board1 + Hand(p2)), 
                                                eval.evaluate(board2 + player), 
                                                eval.evaluate(board3 + player), 
                                                eval.evaluate(board4 + player), 
                                                eval.evaluate(board5 + player), 
                                                eval.evaluate(board6 + player), 
                                                eval.evaluate(board7 + player)});
                            for(int o1 = 0; o1 < 52; o1++){
                                if(o1 == p1 || o1 == p2 || o1 == b1 || o1 == b2 || o1 == b3 || o1 == b4 || o1 == b5 || o1 == b6) continue;
                                for(int o2 = o1 + 1; o2 < 52; o2++){
                                    if(o2 == p1 || o2 == p2 || o2 == b1 || o2 == b2 || o2 == b3 || o2 == b4 || o2 == b5 || o2 == b6) continue; 
                                    Hand opp = Hand(o1) + Hand(o2);
                                    int opp_str = max({eval.evaluate(board1 + Hand(o1)), 
                                                    eval.evaluate(board1 + Hand(o2)), 
                                                    eval.evaluate(board2 + opp), 
                                                    eval.evaluate(board3 + opp), 
                                                    eval.evaluate(board4 + opp), 
                                                    eval.evaluate(board5 + opp), 
                                                    eval.evaluate(board6 + opp), 
                                                    eval.evaluate(board7 + opp)});
                                    int diff = player_str - opp_str;
                                    if(diff > 0) w++;
                                    else if(diff < 0) l++;
                                    else t++;
                                }
                            }
                            int player_id = encoding::getHoleId(p1/4, p2/4, ps1, ps2);
                            int encoded = encoding_map[encoding::encodeRiverBoard(board1, ps1, ps2)];
                            wins[player_id][encoded] += w;
                            loses[player_id][encoded] += l;
                            ties[player_id][encoded] += t;
                        }
                    }
                }
            }
        }
    }
}

void generateEncodingMap(){
    for(int i = 0; i < (1 << 16)*162; i++) encoding_map[i] = -1;
    for(int b1 = 0; b1 < 52; b1++){
        for(int b2 = b1 + 1; b2 < 52; b2++){
            for(int b3 = b2 + 1; b3 < 52; b3++){
                for(int b4 = b3 + 1; b4 < 52; b4++){
                    for(int b5 = b4 + 1; b5 < 52; b5++){
                        for(int b6 = b5 + 1; b6 < 52; b6++){
                            Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            for(int s1 = 0; s1 < 4; s1++){
                                for(int s2 = 0; s2 < 4; s2++){
                                    int encoded = encoding::encodeRiverBoard(board, s1, s2);
                                    if(encoding_map[encoded] != -1) continue;
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
    for(int i = 0; i < (1 << 16)*162; i++){
        ouf.write(reinterpret_cast<const char*>(&encoding_map[i]), sizeof(int));
    }
    ouf.close();
}

void saveEquity(string dir){
    ofstream ouf(dir, ios::binary);
    int visited = 0;
    for(int i = 0; i < 169; i++){
        for(int j = 0; j < encoding_index; j++){
            float eq = -1.0f;
            if(wins[i][j] + loses[i][j] + ties[i][j] > 0){
                eq = float(wins[i][j] + ties[i][j]/2.0f)/float(wins[i][j] + loses[i][j] + ties[i][j]);
            }
            ouf.write(reinterpret_cast<const char*>(&eq), sizeof(float));
        }
    }
    cout << "visited nodes: " << visited << endl;
    ouf.close();
}

int main(){
    generateEncodingMap();
    vector<pair<int, int>> cards[169];
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            cards[encoding::getHoleId(i/4, j/4, i%4, j%4)].push_back(make_pair(i, j));
        }
    }
    saveEncodingMap("./emd_bucket_data/encoding_map.bin");
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
    saveEquity("./emd_bucket_data/river_equity.bin");
    
    return 0;
}