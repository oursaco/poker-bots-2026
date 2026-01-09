/*
We want to map every combination of (3 distinct hole cards) x (strength of board + hole cards) to an equity against a randomly distributed opponent hand

For bb discard, we want to map (3 distinct cards) to an equity against a randomly distributed opponent hand

For sb discard, we want to map (4 distinct cards) to an equity against a randomly distributed opponent hand

For flop, we want to map (2 distinct hole cards) x (4 distinct cards) to an equity against a randomly distributed opponent hand

For turn, we want to map (2 distinct hole cards) x (5 distinct cards) to an equity against a randomly distributed opponent hand

For river, we want to map (2 distinct hole cards) x (6 distinct cards) to an equity against a randomly distributed opponent hand

EV(S, H)
*/

#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "external/omp/Random.h"
#include <set>
#include <unordered_set>
#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <fstream>
using namespace omp;
using namespace std;

static const string SUITS = "chsd";
static const string CARDS = "23456789TJQKA";

inline unsigned getCardId(string x) {
    unsigned rank = find(CARDS.begin(), CARDS.end(), x[0]) - CARDS.begin();
    unsigned suit = find(SUITS.begin(), SUITS.end(), x[1]) - SUITS.begin();
    return rank*4 + suit;
}

unsigned map_to[1 << 20];
unsigned len = 1;

float bucket[4][169][100000];

omp::HandEvaluator eval;

void readMap(string tar_dir){
    ifstream inf(tar_dir, ios::binary);
    for(int i = 0; i < (1 << 20); i++){
        inf.read(reinterpret_cast<char*>(&map_to[i]), sizeof(unsigned));
        len = max(len, map_to[i] + 1);
    }
    inf.close();
}

void readTable(string tar_dir, int ind){
    ifstream inf(tar_dir, ios::binary);
    for(int i = 0; i < 169; i++){
        for(int j = 1; j <= len; j++){
            inf.read(reinterpret_cast<char*>(&bucket[ind][i][j]), sizeof(float));
        }
    }
    inf.close();
}

int encodeBoard(Hand board, int suited){
    int str = eval.evaluate(board);
    int shared_suits = board.suitCount(0) + suited + 1; // if not suited it counts shared suits with the max
    int non_shared_suits = max({board.suitCount(1), board.suitCount(2), board.suitCount(3)});
    shared_suits = min(3, max(0, shared_suits - 2)); // flush is 3, flush draw is 2, backdoor flush draw is 1
    non_shared_suits = min(3, max(0, non_shared_suits - 2)); // flush is 3, flush draw is 2, backdoor flush draw is 1
    return str*16 + shared_suits*4 + non_shared_suits;
}

void printSpecificEquities() {
    // Hand strengths for reference (from HandEvaluator):
    // 1=high card, 2=pair, 3=two pair, 4=trips, 5=straight, 6=flush, 7=full house, 8=quads, 9=straight flush
    
    cout << "\n=== EQUITY FOR SPECIFIC HAND TYPES ===\n" << endl;
    
    // Use different hole cards for each scenario
    // Trips scenario: AA (pocket aces)
    int trips_holecard = 12*13 + 12; // AA = 168
    
    // Flush draw scenario: Ah Kh (suited hearts, to match board hearts)
    int flush_holecard = 12*13 + 11; // AKs = 167
    
    // Straight draw scenario: T6 offsuit
    int straight_holecard = 9*13 + 4; // T6o = 121
    
    // Create example boards for each hand type
    // Example 1: Trips on board (e.g., 222) - using offsuit AA
    Hand trips_board_3 = Hand::empty() + Hand(getCardId("2c")) + Hand(getCardId("2h")) + Hand(getCardId("2d"));
    Hand trips_board_4 = trips_board_3 + Hand(getCardId("7s"));
    Hand trips_board_5 = trips_board_4 + Hand(getCardId("9c"));
    Hand trips_board_6 = trips_board_5 + Hand(getCardId("Th"));

    int trips_3 = encodeBoard(trips_board_3, 0); // offsuit hole cards (AA clubs/hearts)
    int trips_4 = encodeBoard(trips_board_4, 0);
    int trips_5 = encodeBoard(trips_board_5, 0);
    int trips_6 = encodeBoard(trips_board_6, 0);
    
    // Example 2: Flush draw (3 hearts on board) - using AhKh
    Hand flush_draw_3 = Hand::empty() + Hand(getCardId("2c")) + Hand(getCardId("5c")) + Hand(getCardId("8d"));
    Hand flush_draw_4 = flush_draw_3 + Hand(getCardId("Kh"));
    Hand flush_draw_5 = flush_draw_4 + Hand(getCardId("9h"));
    Hand flush_draw_6 = flush_draw_5 + Hand(getCardId("3c"));
    
    int flush_3 = encodeBoard(flush_draw_3, 1); // suited hole cards (AhKh - flush draw)
    int flush_4 = encodeBoard(flush_draw_4, 1);
    int flush_5 = encodeBoard(flush_draw_5, 1);
    int flush_6 = encodeBoard(flush_draw_6, 1);
    
    // Example 3: Straight draw (connected cards) - using T6o
    Hand straight_draw_3 = Hand::empty() + Hand(getCardId("7c")) + Hand(getCardId("8h")) + Hand(getCardId("9d"));
    Hand straight_draw_4 = straight_draw_3 + Hand(getCardId("2s"));
    Hand straight_draw_5 = straight_draw_4 + Hand(getCardId("4c"));
    Hand straight_draw_6 = straight_draw_5 + Hand(getCardId("Qh"));
    
    int straight_3 = encodeBoard(straight_draw_3, 0); // offsuit T6
    int straight_4 = encodeBoard(straight_draw_4, 0);
    int straight_5 = encodeBoard(straight_draw_5, 0);
    int straight_6 = encodeBoard(straight_draw_6, 0);
    
    // Print trips equity with AA
    cout << "TRIPS on board with AA hole cards (Board: 222 7 9 T):\n";
    cout << "Hole cards: Ac Ah (bucket " << trips_holecard << ")\n";
    int trips_encodings[] = {trips_3, trips_4, trips_5, trips_6};
    string stage_names[] = {"3 cards (222)", "4 cards (2227)", "5 cards (22279)", "6 cards (22279T)"};
    for (int stage = 0; stage < 4; stage++) {
        int bucket_idx = map_to[trips_encodings[stage]] + 1;
        if (bucket_idx > 0 && bucket_idx <= len) {
            float equity = bucket[stage][trips_holecard][bucket_idx];
            if (equity >= 0) {
                cout << "  " << stage_names[stage] << ": " << (equity * 100) << "%" << endl;
            } else {
                cout << "  " << stage_names[stage] << ": No data" << endl;
            }
        } else {
            cout << "  " << stage_names[stage] << ": Not found (encoding=" << trips_encodings[stage] << ", idx=" << bucket_idx << ")" << endl;
        }
    }
    
    // Print flush draw equity with AhKh
    cout << "\nFLUSH DRAW with Ah Kh hole cards (Board: 2c 5c 8d Kh 9h 3c):\n";
    cout << "Hole cards: Ah Kh (bucket " << flush_holecard << ")\n";
    int flush_encodings[] = {flush_3, flush_4, flush_5, flush_6};
    string flush_stage_names[] = {"3 cards (2c5c8d)", "4 cards (2c5c8dKh)", "5 cards (2c5c8dKh9h)", "6 cards (2c5c8dKh9h3c)"};
    for (int stage = 0; stage < 4; stage++) {
        int bucket_idx = map_to[flush_encodings[stage]] + 1;
        if (bucket_idx > 0 && bucket_idx <= len) {
            float equity = bucket[stage][flush_holecard][bucket_idx];
            if (equity >= 0) {
                cout << "  " << flush_stage_names[stage] << ": " << (equity * 100) << "%" << endl;
            } else {
                cout << "  " << flush_stage_names[stage] << ": No data" << endl;
            }
        } else {
            cout << "  " << flush_stage_names[stage] << ": Not found (encoding=" << flush_encodings[stage] << ", idx=" << bucket_idx << ")" << endl;
        }
    }
    
    // Print straight draw equity with T6o
    cout << "\nSTRAIGHT DRAW with Tc 6d hole cards (Board: 789 2 4 Q):\n";
    cout << "Hole cards: Tc 6d (bucket " << straight_holecard << ")\n";
    int straight_encodings[] = {straight_3, straight_4, straight_5, straight_6};
    string straight_stage_names[] = {"3 cards (789)", "4 cards (7892)", "5 cards (78924)", "6 cards (78924Q)"};
    for (int stage = 0; stage < 4; stage++) {
        int bucket_idx = map_to[straight_encodings[stage]] + 1;
        if (bucket_idx > 0 && bucket_idx <= len) {
            float equity = bucket[stage][straight_holecard][bucket_idx];
            if (equity >= 0) {
                cout << "  " << straight_stage_names[stage] << ": " << (equity * 100) << "%" << endl;
            } else {
                cout << "  " << straight_stage_names[stage] << ": No data" << endl;
            }
        } else {
            cout << "  " << straight_stage_names[stage] << ": Not found (encoding=" << straight_encodings[stage] << ", idx=" << bucket_idx << ")" << endl;
        }
    }
    
    // Print some statistics
    cout << "\n=== STATISTICS ===\n";
    cout << "Total unique board encodings: " << len << endl;
    cout << "Encoding uses: strength*16 + shared_suits*4 + non_shared_suits" << endl;
    cout << "  - shared_suits: 3=flush, 2=flush draw, 1=backdoor flush draw" << endl;
    cout << "  - non_shared_suits: 3=flush, 2=flush draw, 1=backdoor flush draw" << endl;
}

int main(){
    string dir = "./bucket_data";
    // Print specific hand equities
    readMap(dir + "/ehs_map.bin");
    readTable(dir + "/three.bin", 0);
    readTable(dir + "/four.bin", 1);
    readTable(dir + "/five.bin", 2);
    readTable(dir + "/six.bin", 3);
    printSpecificEquities();
    
    
    return 0;
}