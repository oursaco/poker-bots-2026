#include <iostream>
#include <fstream>
#include <vector>
#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "encoding.h"
using namespace omp;

using namespace std;

int main(){
    std::ifstream map("emd_bucket_data/turn_encoding_map.bin", std::ios::binary);
    int encoding_index = 0;
    map.read(reinterpret_cast<char*>(&encoding_index), sizeof(int));
    std::vector<int> encoding_map((1 << 16) * 33, -1);
    map.read(reinterpret_cast<char*>(encoding_map.data()), encoding_map.size() * sizeof(int));

    cout << encoding_index << endl;

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
    int board_id = encoding_map[encoding::encodeTurnBoard(board, s1, s2)];
    // int hand_id = getHoleId(2, 2, 0, 1);
    // int hand_id = getHoleId(12, 12, 1, 2);
    int hand_id = encoding::getHoleId(p1/4, p2/4, s1, s2);
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
