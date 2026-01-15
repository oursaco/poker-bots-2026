#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "encoding.h"
#include <vector>
#include <fstream>
using namespace omp;
using namespace std;

int encoding_map[(1 << 16) * 181];
int encoding_index = 0;

float num_turns[169][161057];
float flop_equity[169][161057];

/*
4000
3100
22
211
1111
*/



void solve(int p1, int p2)
{
    assert(p1 >= 0 && p1 < 52 && p2 >= 0 && p2 < 52 && p1 != p2);
    std::ifstream map("emd_bucket_data/turn_encoding_map.bin", std::ios::binary);
    int turn_encoding_index = 0;
    map.read(reinterpret_cast<char *>(&turn_encoding_index), sizeof(int));
    std::vector<int> turn_encoding_map((1 << 16) * 33, -1);
    map.read(reinterpret_cast<char *>(turn_encoding_map.data()), turn_encoding_map.size() * sizeof(int));

    std::ifstream eq("emd_bucket_data/turn_equity.bin", std::ios::binary);
    std::vector<float> turn_equity(169 * turn_encoding_index);
    eq.read(reinterpret_cast<char *>(turn_equity.data()), turn_equity.size() * sizeof(float));
    for (int b1 = 0; b1 < 52; b1++){
        if (b1 == p1 || b1 == p2) continue;
        for (int b2 = b1 + 1; b2 < 52; b2++){
            if (b2 == p1 || b2 == p2) continue;
            for (int b3 = b2 + 1; b3 < 52; b3++){
                if (b3 == p1 || b3 == p2) continue;
                for (int b4 = b3 + 1; b4 < 52; b4++){
                    if (b4 == p1 || b4 == p2) continue;
                    Hand flop = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4);
                    int s1 = p1 % 4;
                    int s2 = p2 % 4;
                    int flop_id = encoding_map[encoding::encodeFlopBoard(flop, s1, s2)];
                    assert(flop_id >= 0);
                    for (int b5 = 0; b5 < 52; b5++){
                        if (b5 == p1 || b5 == p2 || b5 == b1 || b5 == b2 || b5 == b3 || b5 == b4)
                            continue;
                        Hand turn = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5);
                        int hand_id = encoding::getHoleId(p1/4, p2/4, s1, s2);
                        int board_id = turn_encoding_map[encoding::encodeTurnBoard(turn, s1, s2)];
                        assert(board_id >= 0);
                        float eq = turn_equity[hand_id * turn_encoding_index + board_id];
                        flop_equity[hand_id][flop_id] += eq;
                        num_turns[hand_id][flop_id]++;
                    }
                }
            }
        }
    }
}

void generateEncodingMap()
{
    for (int i = 0; i < (1 << 16) * 181; i++)
        encoding_map[i] = -1;
    for (int b1 = 0; b1 < 52; b1++)
    {
        for (int b2 = b1 + 1; b2 < 52; b2++)
        {
            for (int b3 = b2 + 1; b3 < 52; b3++)
            {
                for (int b4 = b3 + 1; b4 < 52; b4++)
                {
                    Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4);
                    for (int s1 = 0; s1 < 4; s1++)
                    {
                        for (int s2 = 0; s2 < 4; s2++)
                        {
                            int encoded = encoding::encodeFlopBoard(board, s1, s2);
                            if (encoding_map[encoded] != -1)
                                continue;
                            encoding_map[encoded] = encoding_index++;
                        }
                    }
                }
            }
        }
    }
    cout << "number of flop boards: " << encoding_index << endl;
}

void saveEncodingMap(string dir)
{
    ofstream ouf(dir, ios::binary);
    ouf.write(reinterpret_cast<const char *>(&encoding_index), sizeof(int));
    for (int i = 0; i < (1 << 16) * 181; i++)
    {
        ouf.write(reinterpret_cast<const char *>(&encoding_map[i]), sizeof(int));
    }
    ouf.close();
}

void saveEquity(string dir)
{
    ofstream ouf(dir, ios::binary);
    for (int i = 0; i < 169; i++)
    {
        for (int j = 0; j < encoding_index; j++)
        {
            float eq = -1.0f;
            if (num_turns[i][j] > 0)
            {
                eq = flop_equity[i][j] / (float)num_turns[i][j];
            }
            ouf.write(reinterpret_cast<const char *>(&eq), sizeof(float));
        }
    }
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
    saveEncodingMap("./emd_bucket_data/flop_encoding_map.bin");

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
    saveEquity("./emd_bucket_data/flop_equity.bin");
    return 0;
}