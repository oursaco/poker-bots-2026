#include <iostream>
#include <fstream>
#include <vector>
#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "river_encoding.h"
using namespace omp;


using namespace std;

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
