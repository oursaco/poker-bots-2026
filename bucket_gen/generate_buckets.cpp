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

void extractCard(uint64_t &hand, unsigned &card, unsigned &suit) {
    unsigned c = __builtin_ctzll(hand);
    card = c/4;
    suit = c%4;
    hand ^= 1ull << c;
}

int comb_ind[13][13][13];

void fillCombInd() {
    int ind = 0;
    for(int i = 0; i < 13; i++) {
        for(int j = i + 1; j < 13; j++) {
            for(int k = j + 1; k < 13; k++) {
                comb_ind[i][j][k] = ind++;
            }
        }
    }
}

int encodeHolePreflop(uint64_t hand) {
    unsigned r0, r1, r2;
    unsigned s0, s1, s2;
    extractCard(hand, r0, s0);
    extractCard(hand, r1, s1);
    extractCard(hand, r2, s2);

    // Trips
    if(r0 == r1 && r1 == r2) return r0;

    // Pair
    if(r0 == r1 || r1 == r2){
        unsigned pair_rank = (r0 == r1) ? r0 : r1;
        unsigned kicker_rank = (r0 == r1) ? r2 : r0;
        unsigned pair_index = pair_rank * 12 + (kicker_rank > pair_rank ? kicker_rank - 1 : kicker_rank);

        // suit type
        int suit_type = 0; // 0 = kicker matches one pair suit, 1 = kicker matches none
        if (r0 == r1){
            suit_type = (s2 == s0 ||
                        s2 == s1) ? 0 : 1;
        } else {
            suit_type = (s0 == s1 || s0 == s2) ? 0 : 1;
        }
        assert(13 + pair_index * 2 + suit_type < 325);
        return 13 + pair_index * 2 + suit_type;
    }

    // Distinct
    int rank_index = comb_ind[r0][r1][r2];

    // normalize suits
    int suit_pattern;
    if (s0 == s1 && s1 == s2) {
        suit_pattern = 0; // monotone
    } else if (s0 != s1 && s0 != s2 && s1 != s2) {
        suit_pattern = 1; // rainbow
    } else {
        // single suited
        if (s0 == s1) suit_pattern = 2;
        else if (s0 == s2) suit_pattern = 3;
        else suit_pattern = 4;
    }

    return 325 + rank_index * 5 + suit_pattern;
}

int preflop_map[52][52][52];
int preflop_buckets;

void writePreflop(string tar_dir){
    ofstream ouf(tar_dir, ios::binary);
    int num_buckets = preflop_buckets;
    ouf.write(reinterpret_cast<const char*>(&num_buckets), sizeof(int));
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            for(int k = j + 1; k < 52; k++){
                ouf.write(reinterpret_cast<const char*>(&preflop_map[i][j][k]), sizeof(int));
            }
        }
    }
    ouf.close();
}
void generatePreflop(){
    fillCombInd();
    int mx = 0;
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            for(int k = j + 1; k < 52; k++){
                int encoded = encodeHolePreflop(1ull << i | 1ull << j | 1ull << k);
                preflop_map[i][j][k] = encoded;
                mx = max(mx, encoded);
            }
        }
    }
    cout << "num preflop buckets: " << mx + 1 << endl;
    preflop_buckets = mx + 1;
}

XoroShiro128Plus rng{std::random_device{}()};
FastUniformIntDistribution<unsigned, 16> card_generator(0, 51);
HandEvaluator eval;

int generateCard(uint64_t &used_mask, Hand &board){
    unsigned card;
    uint64_t card_mask;
    do {
        card = card_generator(rng);
        card_mask = 1ull << card;
    } while (used_mask & card_mask);
    used_mask |= card_mask;
    board += Hand(card);
    return card;
}

int generateNewCard(uint64_t &used_mask, Hand &board){
    unsigned card;
    uint64_t card_mask;
    do {
        card = card_generator(rng);
        card_mask = 1ull << card;
    } while (used_mask & card_mask);
    used_mask |= card_mask;
    board = Hand(card);
    return card;
}

static const string SUITS = "chsd";
static const string CARDS = "23456789TJQKA";

inline unsigned getCardId(string x) {
    unsigned rank = find(CARDS.begin(), CARDS.end(), x[0]) - CARDS.begin();
    unsigned suit = find(SUITS.begin(), SUITS.end(), x[1]) - SUITS.begin();
    return rank*4 + suit;
}

unsigned map_to[1 << 20];
unsigned len = 1;

float wins[4][170][100000];
float ties[4][170][100000];
float lose[4][170][100000];
float bucket[4][169][100000];

void writeMap(string tar_dir){
    unsigned mx = 0;
    ofstream ouf(tar_dir, ios::binary);
    for(int i = 0; i < (1 << 20); i++){
        if(map_to[i]) map_to[i]--;
        mx = max(mx, map_to[i]);
        ouf.write(reinterpret_cast<const char*>(&map_to[i]), sizeof(unsigned));
    }
    ouf.close();
    cout << "mx: " << mx << endl;
}

void writeTable(string tar_dir, int ind){
    int cnt = 0;
    for(int i = 0; i < 169; i++){
        for(int j = 1; j <= len; j++){
            if(wins[ind][i][j] + ties[ind][i][j] + lose[ind][i][j] == 0){
                bucket[ind][i][j] = -1.0;
            } else {
                bucket[ind][i][j] = (wins[ind][i][j] + ties[ind][i][j]/2.0)/(wins[ind][i][j] + ties[ind][i][j] + lose[ind][i][j]);
                // cout << bucket[ind][i][j] << " " << wins[ind][i][j] << " " << ties[ind][i][j] << " " << lose[ind][i][j] << endl;
            }
        }
    }
    ofstream ouf(tar_dir, ios::binary);
    for(int i = 0; i < 169; i++){
        for(int j = 1; j <= len; j++){
            ouf.write(reinterpret_cast<const char*>(&bucket[ind][i][j]), sizeof(float));
        }
    }
    ouf.close();
}

int encodeBoard(Hand board, int suited){
    int str = eval.evaluate(board);
    int shared_suits = board.suitCount(0) + suited + 1; // if not suited it counts shared suits with the max
    int non_shared_suits = max({board.suitCount(1), board.suitCount(2), board.suitCount(3)});
    shared_suits = min(3, max(0, shared_suits - 2)); // flush is 3, flush draw is 2, backdoor flush draw is 1
    non_shared_suits = min(3, max(0, non_shared_suits - 2)); // flush is 3, flush draw is 2, backdoor flush draw is 1
    return str*16 + shared_suits*4 + non_shared_suits;
}

int vis[100000];

void generateRandomGame(int a, int b, int st, int st2){
    assert(len < 100000);
    uint64_t used_mask = (1ull << a) | (1ull << b);
    Hand boards[7] = {Hand::empty(), Hand::empty(), Hand::empty(), Hand::empty(), Hand::empty(), Hand::empty(), Hand::empty()};
    Hand card1, card2, card3, card4, card5, card6;
    int suited = (a%4 == b%4);
    generateNewCard(used_mask, card1);
    generateNewCard(used_mask, card2);
    for(int i = 0; i < 7; i++){
        if(i != 0) boards[i] += card1;
        if(i != 1) boards[i] += card2;
    }
    int two = encodeBoard(boards[2], suited);
    generateNewCard(used_mask, card3);
    for(int i = 0; i < 7; i++){
        if(i != 2) boards[i] += card3;
    }
    int three = encodeBoard(boards[3], suited);
    if(map_to[three] == 0) map_to[three] = len++;
    generateNewCard(used_mask, card4);
    for(int i = 0; i < 7; i++){
        if(i != 3) boards[i] += card4;
    }
    int four = encodeBoard(boards[4], suited);
    if(map_to[four] == 0) map_to[four] = len++;
    generateNewCard(used_mask, card5);
    for(int i = 0; i < 7; i++){
        if(i != 4) boards[i] += card5;
    }
    int five = encodeBoard(boards[5], suited);
    if(map_to[five] == 0) map_to[five] = len++;
    generateNewCard(used_mask, card6);
    for(int i = 0; i < 7; i++){
        if(i != 5) boards[i] += card6;
    }
    int six = encodeBoard(boards[6], suited);
    if(map_to[six] == 0) map_to[six] = len++;
    Hand opp1, opp2;
    generateNewCard(used_mask, opp1);
    generateNewCard(used_mask, opp2);
    Hand player1 = Hand(a), player2 = Hand(b);
    int opp_str = 0;
    int player_str = 0;
    for(int i = 0; i < 6; i++){
        player_str = max(player_str, (int)eval.evaluate(boards[i] + player1 + player2));
        opp_str = max(opp_str, (int)eval.evaluate(boards[i] + opp1 + opp2));
    }
    player_str = max(player_str, (int)eval.evaluate(boards[6] + player1));
    player_str = max(player_str, (int)eval.evaluate(boards[6] + player2));
    opp_str = max(opp_str, (int)eval.evaluate(boards[6] + opp1));
    opp_str = max(opp_str, (int)eval.evaluate(boards[6] + opp2));
    assert(__builtin_popcountll(used_mask) == 10);
    if(player_str > opp_str){
        wins[0][st][map_to[three]]++;
        wins[1][st][map_to[four]]++;
        wins[2][st][map_to[five]]++;
        wins[3][st][map_to[six]]++;
    } else if(player_str == opp_str){
        ties[0][st][map_to[three]]++;
        ties[1][st][map_to[four]]++;
        ties[2][st][map_to[five]]++;
        ties[3][st][map_to[six]]++;
    } else {
        lose[0][st][map_to[three]]++;
        lose[1][st][map_to[four]]++;
        lose[2][st][map_to[five]]++;
        lose[3][st][map_to[six]]++;
    }
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
    cout << "\nFLUSH DRAW with Ah Kh hole cards (Board: 2h 5h 8h K 9 3):\n";
    cout << "Hole cards: Ah Kh (bucket " << flush_holecard << ")\n";
    int flush_encodings[] = {flush_3, flush_4, flush_5, flush_6};
    string flush_stage_names[] = {"3 cards (2h5h8h)", "4 cards (2h5h8hK)", "5 cards (2h5h8hK9)", "6 cards (2h5h8hK93)"};
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
    generatePreflop();
    string dir = "./bucket_data";
    writePreflop(dir + "/preflop.bin");
    int iterations = 1000000;
    for(int a = 0; a < 13; a++){
        for(int b = 0; b < 13; b++){
            cout << a << " " << b << endl;
            if(a < b){
                for(int t = 0; t < iterations; t++){
                    generateRandomGame(b*4, a*4, a*13 + b, a*13 + b);
                }
            } else {
                for(int t = 0; t < iterations; t++){
                    generateRandomGame(a*4, b*4 + 1, a*13 + b, a*13 + b);
                }
            }
        }
    }
    len--;
    cout << "len: " << len << endl;
    // Print specific hand equities
    writeMap(dir + "/ehs_map.bin");
    writeTable(dir + "/three.bin", 0);
    writeTable(dir + "/four.bin", 1);
    writeTable(dir + "/five.bin", 2);
    writeTable(dir + "/six.bin", 3);
    printSpecificEquities();
    
    
    return 0;
}