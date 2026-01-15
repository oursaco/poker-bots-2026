#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "river_encoding.h"

using namespace omp;
using namespace std;

int encoding_map[(1 << 16) * fill in the blank];
int encoding_index = 0;

float num_turns[169][fill in the blank];
float flop_equity[169][fill in the blank];

int encodeFlopBoard(Hand board, int s1, int s2)
{
    // to-do
}

void solve(int p1, int p2)
{
    assert(p1 >= 0 && p1 < 52 && p2 >= 0 && p2 < 52 && p1 != p2);
    std::ifstream map("emd_bucket_data/encoding_map.bin", std::ios::binary);
    int river_encoding_index = 0;
    map.read(reinterpret_cast<char *>(&river_encoding_index), sizeof(int));
    std::vector<int> turn_encoding_map((1 << 16) * 162, -1);
    map.read(reinterpret_cast<char *>(turn_encodin.data()), river_encoding_map.size() * sizeof(int));

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
                        Hand flop = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4);
                        int s1 = p1 % 4;
                        int s2 = p2 % 4;
                        int turn_id = encoding_map[encodeFlopBoard(flop, s1, s2)];
                        assert(turn_id >= 0);
                        for (int b5 = 0; b5 < 52; b5++){
                            if (b5 == p1 || b5 == p2 || b5 == b1 || b5 == b2 || b5 == b3 || b5 == b4)
                                continue;
                            Hand turn = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5);
                            int board_id = river_encoding_map[encodeTurn(river, s1, s2)];
                            assert(board_id >= 0);
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
            if (num_turns[i][j] > 0)
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