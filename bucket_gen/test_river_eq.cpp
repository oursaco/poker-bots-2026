#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <utility>
#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "encoding.h"
using namespace omp;


using namespace std;

int main(){
    std::ifstream map("emd_bucket_data/river_encoding_map.bin", std::ios::binary);
    int encoding_index = 0;
    map.read(reinterpret_cast<char*>(&encoding_index), sizeof(int));
    std::vector<int> encoding_map((1 << 16) * 162, -1);
    map.read(reinterpret_cast<char*>(encoding_map.data()), encoding_map.size() * sizeof(int));

    cout << encoding_index << endl;

    std::ifstream eq("emd_bucket_data/river_equity.bin", std::ios::binary);
    std::vector<float> equity(169 * encoding_index);
    eq.read(reinterpret_cast<char*>(equity.data()), equity.size() * sizeof(float));

    // Hand board = Hand::empty() + Hand(48) + Hand(49) + Hand(1) + Hand(3) + Hand(6) + Hand(10); // As, Ah, 2h, 2d, 3c, 4c
    Hand board = Hand::empty() + Hand(45) + Hand(46) + Hand(48) + Hand(47) + Hand(50) + Hand(0);
    int p1 = 49;
    int p2 = 51;
    int s1 = p1 % 4;
    int s2 = p2 % 4;
    int board_id = encoding_map[encoding::encodeRiverBoard(board, s1, s2)];
    cout << board_id << endl;
    // int hand_id = getHoleId(2, 2, 0, 1);
    int hand_id = encoding::getHoleId(p1/4, p2/4, s1, s2);
    float value = equity[hand_id * encoding_index + board_id];
    cout << value << endl;

    return 0;
}
