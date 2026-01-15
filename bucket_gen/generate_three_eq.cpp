#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "encoding.h"
#include <vector>
#include <fstream>

using namespace omp;
using namespace std;

int encoding_map[(1 << 16) * 77];
int encoding_index = 0;

float num_flops[169][161057];
float three_equity[169][161057];

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
    std::ifstream map("emd_bucket_data/flop_encoding_map.bin", std::ios::binary);
    int flop_encoding_index = 0;
    map.read(reinterpret_cast<char *>(&flop_encoding_index), sizeof(int));
    std::vector<int> flop_encoding_map((1 << 16) * 181, -1);
    map.read(reinterpret_cast<char *>(flop_encoding_map.data()), flop_encoding_map.size() * sizeof(int));

    std::ifstream eq("emd_bucket_data/flop_equity.bin", std::ios::binary);
    std::vector<float> flop_equity(169 * flop_encoding_index);
    eq.read(reinterpret_cast<char *>(flop_equity.data()), flop_equity.size() * sizeof(float));
    for (int b1 = 0; b1 < 52; b1++){
        if (b1 == p1 || b1 == p2) continue;
        for (int b2 = b1 + 1; b2 < 52; b2++){
            if (b2 == p1 || b2 == p2) continue;
            for (int b3 = b2 + 1; b3 < 52; b3++){
                if (b3 == p1 || b3 == p2) continue;
                omp::Hand three = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3);
                int s1 = p1 % 4;
                int s2 = p2 % 4;
                int three_id = encoding_map[encoding::encodeThreeBoard(three, s1, s2)];
                assert(three_id >= 0);
                for (int b4 = 0; b4 < 52; b4++){
                    if (b4 == p1 || b4 == p2 || b4 == b1 || b4 == b2 || b4 == b3)
                        continue;
                    omp::Hand flop = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4);
                    int flop_id = flop_encoding_map[encoding::encodeFlopBoard(flop, s1, s2)];
                    assert(three_id >= 0);
                    float eq = flop_equity[encoding::getHoleId(p1/4, p2/4, s1, s2) * flop_encoding_index + flop_id];
                    three_equity[encoding::getHoleId(p1/4, p2/4, s1, s2)][three_id] += eq;
                    num_flops[encoding::getHoleId(p1/4, p2/4, s1, s2)][three_id]++;
                }
            }
        }
    }
}

void generateEncodingMap()
{
    for (int i = 0; i < (1 << 16) * 77; i++)
        encoding_map[i] = -1;
    for (int b1 = 0; b1 < 52; b1++)
    {
        for (int b2 = b1 + 1; b2 < 52; b2++)
        {
            for (int b3 = b2 + 1; b3 < 52; b3++)
            {
                omp::Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3);
                for (int s1 = 0; s1 < 4; s1++)
                {
                    for (int s2 = 0; s2 < 4; s2++)
                    {
                        int encoded = encoding::encodeThreeBoard(board, s1, s2);
                        if (encoding_map[encoded] != -1)
                            continue;
                        encoding_map[encoded] = encoding_index++;
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
    for (int i = 0; i < (1 << 16) * 77; i++)
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
            if (num_flops[i][j] > 0)
            {
                eq = three_equity[i][j] / (float)num_flops[i][j];
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
    saveEncodingMap("./emd_bucket_data/three_encoding_map.bin");

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
    saveEquity("./emd_bucket_data/three_equity.bin");
    return 0;
}