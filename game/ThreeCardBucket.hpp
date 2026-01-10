#ifndef TREECARDBUCKET_HPP
#define TREECARDBUCKET_HPP

#include <cstdint>
#include <vector>
#include <utility>
#include <algorithm>
#include <fstream>
#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "game/ThreeCardState.hpp"
using namespace std;

struct ThreeCardBucket {
    virtual ~ThreeCardBucket() = default;
    virtual int countBuckets(ThreeCardGameState* state) = 0;
    virtual int getPreflopBucket(uint64_t hand) = 0;
    virtual int getFlopBucket(uint64_t board, uint64_t hand) = 0;
    virtual int getTurnBucket(uint64_t board, uint64_t hand) = 0;
    virtual int getRiverBucket(uint64_t board, uint64_t hand) = 0;
    virtual int getWinner(uint64_t board, uint64_t hand1, uint64_t hand2) = 0;
    virtual int getBBDiscardBucket(uint64_t board, uint64_t hand) = 0;
    virtual int getSBDiscardBucket(uint64_t board, uint64_t hand) = 0;
};

struct NaiveThreeCardBucket : ThreeCardBucket {

void updateCard(uint64_t &mask, unsigned &suit, unsigned &id){
    unsigned card = __builtin_ctzll(mask);
    mask ^= 1ull << card;
    suit = card%4;
    id = card/4;
}

int getPreflopBucket(uint64_t hand){
    vector<pair<unsigned, unsigned>> cards;
    for(int i = 0; i < 3; i++){
        unsigned suit, id;
        updateCard(hand, suit, id);
        cards.emplace_back(id, suit);
    }
    sort(cards.begin(), cards.end());
    int suited = cards[0].second == cards[1].second || cards[1].second == cards[2].second || cards[0].second == cards[2].second;
    int triple_suit = cards[0].second == cards[1].second && cards[1].second == cards[2].second;
    int connected = cards[0].first + 1 == cards[1].first || cards[1].first + 1 == cards[2].first;
    int triple_connected = cards[0].first + 1 == cards[1].first && cards[1].first + 1 == cards[2].first;
    int pair = cards[0].first == cards[1].first || cards[1].first == cards[2].first || cards[0].first == cards[2].first;
    int triple = cards[0].first == cards[1].first && cards[1].first == cards[2].first;
    vector<int> states = {suited, triple_suit, connected, triple_connected, pair, triple};
    int ret = 0;
    for(int i = 0; i < states.size(); i++){
        ret <<= 1;
        ret += states[i];
    }
    int hi = cards[2].first;
    int lo = cards[0].first;
    return ret*13*13 + hi*13 + lo;
}

omp::HandEvaluator hand_eval;

bool is_straight_draw(uint64_t mask){
    vector<bool> has_rank(13, false);
    while(mask){
        unsigned suit, id;
        updateCard(mask, suit, id);
        has_rank[id] = true;
    }
    bool straight_draw = false;
    for(int start = 1; start <= 8; ++start){
        int present = 0;
        int missing = 0;
        for(int k = 0; k < 4; ++k) {
            if(has_rank[start + k]) present++;
        }
        if(present == 4){
            straight_draw = true;
            break;
        }
    }
    return straight_draw;
}

bool is_flush_draw(uint64_t mask){
    vector<int32_t> suits(4, 0);
    while(mask){
        unsigned suit, id;
        updateCard(mask, suit, id);
        suits[suit]++;
    }
    for(int i = 0; i < 4; i++){
        if(suits[i] == 4){
            return true;
        }
    }
    return false;
}

int eval_7(uint64_t mask){
    omp::Hand combined = omp::Hand::empty();
    while(mask){
        unsigned suit, id;
        updateCard(mask, suit, id);
        combined += omp::Hand(4*id + suit);
    }
    return hand_eval.evaluate(combined);
}

int eval_8(uint64_t mask){
    if(__builtin_popcountll(mask) < 8) return eval_7(mask);
    unsigned cards[8], suits[8];
    omp::Hand pre[8];
    for(int i = 0; i < 8; i++){
        updateCard(mask, suits[i], cards[i]);
        pre[i] = omp::Hand(4*cards[i] + suits[i]);
        if(i > 0) pre[i] += pre[i-1];
    }
    omp::Hand suf = omp::Hand::empty();
    int strength = 0;
    for(int i = 7; i >= 1; i--){
        strength = max(strength, (int)hand_eval.evaluate(pre[i-1] + suf));
        suf += omp::Hand(4*cards[i] + suits[i]);
    }
    strength = max(strength, (int)hand_eval.evaluate(suf));
    return strength;
}

int eval_3_subsets(uint64_t board, uint64_t hand){
    unsigned cards[3], suits[3];
    for(int i = 0; i < 3; i++){
        updateCard(hand, suits[i], cards[i]);
    }
    // try every subset
    pair<int, int> best_subset = {0, 0};
    for(int i = 1; i < (1 << 3); i++){
        uint64_t mask = board;
        for(int j = 0; j < 3; j++){
            if(j >> i & 1){
                mask |= 1ull << (4*cards[j] + suits[j]);
            }
        }
        int strength = eval_7(mask); // 0 ... 15
        int flush = is_flush_draw(mask); // 0 ... 1
        int straight = is_straight_draw(mask); // 0 ... 1
        best_subset = max(best_subset, {strength/4096, -((i - 1)*64 + strength/4096*2*2 + flush*2 + straight)});
    }
    return -best_subset.second;
}

int eval_2_subsets(uint64_t board, uint64_t hand){
    hand ^= hand & board;
    assert(__builtin_popcountll(hand) == 2);
    unsigned cards[2], suits[2];
    for(int i = 0; i < 2; i++){
        updateCard(hand, suits[i], cards[i]);
    }
    // try every subset
    pair<int, int> best_subset = {0, 0};
    for(int i = 1; i < (1 << 2); i++){
        uint64_t mask = board;
        for(int j = 0; j < 2; j++){
            if(j >> i & 1){
                mask |= 1ull << (4*cards[j] + suits[j]);
            }
        }
        int strength = (__builtin_popcountll(mask) == 8 ? eval_8(mask) : eval_7(mask)); // 0 ... 15
        int flush = is_flush_draw(mask); // 0 ... 1
        int straight = is_straight_draw(mask); // 0 ... 1
        best_subset = max(best_subset, {strength/4096, -((i - 1)*64 + strength/4096*2*2 + flush*2 + straight)});
    }
    return -best_subset.second;
}

int getFlopBucket(uint64_t board, uint64_t hand){
    assert(__builtin_popcountll(board) == 4);
    return eval_2_subsets(board, hand);
}

int getTurnBucket(uint64_t board, uint64_t hand){
    assert(__builtin_popcountll(board) == 5);
    return eval_2_subsets(board, hand);
}

int getRiverBucket(uint64_t board, uint64_t hand){
    assert(__builtin_popcountll(board) == 6);
    return eval_2_subsets(board, hand)/4;
}

int countBuckets(ThreeCardGameState* state){
    if(state->street == 0){
        return (1 << 6)*13*13;
    } else if(state->street == 2 || state->street == 3){
        return 7*64;
    } else if(state->street == 4){
        return 3*64;
    } else if(state->street == 5){
        return 3*64;
    } else if(state->street == 6){
        return 3*64/4;
    } else {
        assert(false);
    }
}

int getWinner(uint64_t board, uint64_t hand1, uint64_t hand2){
    int dif = eval_8(board | hand1) - eval_8(board | hand2);
    if(dif > 0) return 1;
    else if(dif < 0) return -1;
    else return 0;
}

int getBBDiscardBucket(uint64_t board, uint64_t hand){
    assert(__builtin_popcountll(board) == 2);
    return eval_3_subsets(board, hand);
}

int getSBDiscardBucket(uint64_t board, uint64_t hand){
    assert(__builtin_popcountll(board) == 3);
    return eval_3_subsets(board, hand);
}

};

struct EHSThreeCardBucket : ThreeCardBucket {
    unsigned preflop_buckets[52][52][52];
    unsigned map_to[1 << 20];
    float eq[4][169][78294];
    unsigned len = 1;
    omp::HandEvaluator hand_eval;

    void readMap(string tar_dir){
        ifstream inf(tar_dir, ios::binary);
        for(int i = 0; i < (1 << 20); i++){
            inf.read(reinterpret_cast<char*>(&map_to[i]), sizeof(unsigned));
            len = max(len, map_to[i] + 1);
        }
        inf.close();
        assert(len == 78293);
    }

    void readTable(string tar_dir, int ind){
        ifstream inf(tar_dir, ios::binary);
        for(int i = 0; i < 169; i++){
            for(int j = 1; j <= len; j++){
                inf.read(reinterpret_cast<char*>(&eq[ind][i][j]), sizeof(float));
            }
        }
        inf.close();
    }
    void readPreflop(string tar_dir){
        ifstream inf(tar_dir, ios::binary);
        int num_buckets;
        inf.read(reinterpret_cast<char*>(&num_buckets), sizeof(int));
        assert(num_buckets == 1755);
        for(int i = 0; i < 52; i++){
            for(int j = i + 1; j < 52; j++){
                for(int k = j + 1; k < 52; k++){
                    inf.read(reinterpret_cast<char*>(&preflop_buckets[i][j][k]), sizeof(unsigned));
                }
            }
        }
        inf.close();
    }

    string SUITS = "chsd";
    string CARDS = "23456789TJQKA";

    inline unsigned getCardId(string x) {
        unsigned rank = find(CARDS.begin(), CARDS.end(), x[0]) - CARDS.begin();
        unsigned suit = find(SUITS.begin(), SUITS.end(), x[1]) - SUITS.begin();
        return rank*4 + suit;
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
        omp::Hand trips_board_3 = omp::Hand::empty() + omp::Hand(getCardId("2c")) + omp::Hand(getCardId("2h")) + omp::Hand(getCardId("2d"));
        omp::Hand trips_board_4 = trips_board_3 + omp::Hand(getCardId("7s"));
        omp::Hand trips_board_5 = trips_board_4 + omp::Hand(getCardId("9c"));
        omp::Hand trips_board_6 = trips_board_5 + omp::Hand(getCardId("Th"));

        int trips_3 = encodeBoard(trips_board_3, 0, 0); // offsuit hole cards (AA clubs/hearts)
        int trips_4 = encodeBoard(trips_board_4, 0, 0);
        int trips_5 = encodeBoard(trips_board_5, 0, 0);
        int trips_6 = encodeBoard(trips_board_6, 0, 0);
        
        // Example 2: Flush draw (3 hearts on board) - using AhKh
        omp::Hand flush_draw_3 = omp::Hand::empty() + omp::Hand(getCardId("2c")) + omp::Hand(getCardId("5c")) + omp::Hand(getCardId("8d"));
        omp::Hand flush_draw_4 = flush_draw_3 + omp::Hand(getCardId("Kh"));
        omp::Hand flush_draw_5 = flush_draw_4 + omp::Hand(getCardId("9h"));
        omp::Hand flush_draw_6 = flush_draw_5 + omp::Hand(getCardId("3c"));
        
        int flush_3 = encodeBoard(flush_draw_3, 1, 0); // suited hole cards (AhKh - flush draw)
        int flush_4 = encodeBoard(flush_draw_4, 1, 0);
        int flush_5 = encodeBoard(flush_draw_5, 1, 0);
        int flush_6 = encodeBoard(flush_draw_6, 1, 0);
        
        // Example 3: Straight draw (connected cards) - using T6o
        omp::Hand straight_draw_3 = omp::Hand::empty() + omp::Hand(getCardId("7c")) + omp::Hand(getCardId("8h")) + omp::Hand(getCardId("9d"));
        omp::Hand straight_draw_4 = straight_draw_3 + omp::Hand(getCardId("2s"));
        omp::Hand straight_draw_5 = straight_draw_4 + omp::Hand(getCardId("4c"));
        omp::Hand straight_draw_6 = straight_draw_5 + omp::Hand(getCardId("Qh"));
        
        int straight_3 = encodeBoard(straight_draw_3, 0, 0); // offsuit T6
        int straight_4 = encodeBoard(straight_draw_4, 0, 0);
        int straight_5 = encodeBoard(straight_draw_5, 0, 0);
        int straight_6 = encodeBoard(straight_draw_6, 0, 0);
        
        // Print trips equity with AA
        cout << "TRIPS on board with AA hole cards (Board: 222 7 9 T):\n";
        cout << "Hole cards: Ac Ah (bucket " << trips_holecard << ")\n";
        int trips_encodings[] = {trips_3, trips_4, trips_5, trips_6};
        string stage_names[] = {"3 cards (222)", "4 cards (2227)", "5 cards (22279)", "6 cards (22279T)"};
        for (int stage = 0; stage < 4; stage++) {
            int bucket_idx = map_to[trips_encodings[stage]] + 1;
            if (bucket_idx > 0 && bucket_idx <= len) {
                float equity = eq[stage][trips_holecard][bucket_idx];
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
                float equity = eq[stage][flush_holecard][bucket_idx];
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
                float equity = eq[stage][straight_holecard][bucket_idx];
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

    void init(string tar_dir){
        readPreflop(tar_dir + "/preflop.bin");
        readMap(tar_dir + "/ehs_map.bin");
        readTable(tar_dir + "/three.bin", 0);
        readTable(tar_dir + "/four.bin", 1);
        readTable(tar_dir + "/five.bin", 2);
        readTable(tar_dir + "/six.bin", 3);
        printSpecificEquities();
    }

    omp::Hand getHand(uint64_t mask){
        omp::Hand combined = omp::Hand::empty();
        while(mask){
            unsigned suit, id;
            updateCard(mask, suit, id);
            combined += omp::Hand(4*id + suit);
        }
        return combined;
    }

    int eval_7(uint64_t mask){
        omp::Hand combined = omp::Hand::empty();
        while(mask){
            unsigned suit, id;
            updateCard(mask, suit, id);
            combined += omp::Hand(4*id + suit);
        }
        return hand_eval.evaluate(combined);
    }

    int eval_8(uint64_t mask){
        unsigned cards[8], suits[8];
        omp::Hand pre[8];
        for(int i = 0; i < 8; i++){
            updateCard(mask, suits[i], cards[i]);
            pre[i] = omp::Hand(4*cards[i] + suits[i]);
            if(i > 0) pre[i] += pre[i-1];
        }
        omp::Hand suf = omp::Hand::empty();
        int strength = 0;
        for(int i = 7; i >= 1; i--){
            strength = max(strength, (int)hand_eval.evaluate(pre[i-1] + suf));
            suf += omp::Hand(4*cards[i] + suits[i]);
        }
        strength = max(strength, (int)hand_eval.evaluate(suf));
        return strength;
    }

    int getWinner(uint64_t board, uint64_t hand1, uint64_t hand2){
        int dif = eval_8(board | hand1) - eval_8(board | hand2);
        if(dif > 0) return 1;
        else if(dif < 0) return -1;
        else return 0;
    }

    int countBuckets(ThreeCardGameState* state){
        if(state->street == 0){
            return 1755;
        } else if(state->street == 2 || state->street == 3){
            return 6*6*6;
        } else if(state->street == 4){
            return 50;
        } else if(state->street == 5){
            return 50;
        } else if(state->street == 6){
            return 50;
        }
    }

    void updateCard(uint64_t &mask, unsigned &suit, unsigned &id){
        unsigned card = __builtin_ctzll(mask);
        mask ^= 1ull << card;
        suit = card%4;
        id = card/4;
    }

    inline unsigned getHoleId(unsigned p1, unsigned p2, unsigned ps1, unsigned ps2){
        if(ps1 == ps2) return p1*13 + p2;
        return p2*13 + p1;
    }

    int getPreflopBucket(uint64_t hand){
        unsigned cards[3], suits[3];
        for(int i = 0; i < 3; i++){
            updateCard(hand, suits[i], cards[i]);
        }
        return preflop_buckets[cards[0]*4 + suits[0]][cards[1]*4 + suits[1]][cards[2]*4 + suits[2]];
    }

    int encodeBoard(omp::Hand board, int suited, int tar_suit){
        int str = hand_eval.evaluate(board);
        int shared_suits = board.suitCount(tar_suit) + suited + 1; // if not suited it counts shared suits with the max
        int non_shared_suits = max({board.suitCount((tar_suit + 1) % 4), board.suitCount((tar_suit + 2) % 4), board.suitCount((tar_suit + 3) % 4)});
        shared_suits = min(3, max(0, shared_suits - 2)); // flush is 3, flush draw is 2, backdoor flush draw is 1
        non_shared_suits = min(3, max(0, non_shared_suits - 2)); // flush is 3, flush draw is 2, backdoor flush draw is 1
        return str*16 + shared_suits*4 + non_shared_suits;
    }

    float getEquity(uint64_t board, uint64_t hand, int ind){
        assert(__builtin_popcountll(hand) == 2);
        if(ind == 0) assert(__builtin_popcountll(board) == 3);
        if(ind == 1) assert(__builtin_popcountll(board) == 4);
        if(ind == 2) assert(__builtin_popcountll(board) == 5);
        if(ind == 3) assert(__builtin_popcountll(board) == 6);

        unsigned cards[2], suits[2];
        for(int i = 0; i < 2; i++){
            updateCard(hand, suits[i], cards[i]);
        }
        int board_encoded = encodeBoard(getHand(board), suits[0] == suits[1], suits[1]);
        int hole_id = getHoleId(cards[0], cards[1], suits[0], suits[1]);
        float e = eq[ind][hole_id][map_to[board_encoded] + 1];
        if(e <= 0.0f){
            cout << "Missing equity for: " << board << " hand: " << hand << " ind: " << ind << endl;
            return 0.0f;
        }
        return e;
    }

    int getBBDiscardBucket(uint64_t board, uint64_t hand){
        assert(__builtin_popcountll(board) == 2);
        assert(__builtin_popcountll(hand) == 3);
        unsigned cards[3], suits[3];
        for(int i = 0; i < 3; i++){
            updateCard(hand, suits[i], cards[i]);
        }
        int st = 0;
        for(int i = 0; i < 3; i++){
            uint64_t hand_mask = 0;
            uint64_t board_mask = board;
            for(int j = 0; j < 3; j++){
                if(j != i){
                    hand_mask |= 1ull << (4*cards[j] + suits[j]);
                } else {
                    board_mask |= 1ull << (4*cards[j] + suits[j]);
                }
            }
            float eq = getEquity(board_mask, hand_mask, 0);
            int str = int(eq*6);
            if(str == 6) str = 5;
            st *= 6;
            st += str;
        }
        return st;
    }

    int getSBDiscardBucket(uint64_t board, uint64_t hand){
        assert(__builtin_popcountll(board) == 3);
        assert(__builtin_popcountll(hand) == 3);
        unsigned cards[3], suits[3];
        for(int i = 0; i < 3; i++){
            updateCard(hand, suits[i], cards[i]);
        }
        int st = 0;
        for(int i = 0; i < 3; i++){
            uint64_t hand_mask = 0;
            uint64_t board_mask = board;
            for(int j = 0; j < 3; j++){
                if(j != i){
                    hand_mask |= 1ull << (4*cards[j] + suits[j]);
                } else {
                    board_mask |= 1ull << (4*cards[j] + suits[j]);
                }
            }
            float eq = getEquity(board_mask, hand_mask, 1);
            int str = int(eq*6);
            if(str == 6) str = 5;
            st *= 6;
            st += str;
        }
        return st;
    }

    int getFlopBucket(uint64_t board, uint64_t hand){
        assert(__builtin_popcountll(board) == 4);
        assert(__builtin_popcountll(hand) == 3);
        hand ^= hand & board;
        float eq = getEquity(board, hand, 1);
        int str = int(eq*50);
        if(str == 50) str = 49;
        return str;
    }

    int getTurnBucket(uint64_t board, uint64_t hand){
        assert(__builtin_popcountll(board) == 5);
        hand ^= hand & board;
        float eq = getEquity(board, hand, 2);
        int str = int(eq*50);
        if(str == 50) str = 49;
        return str;
    }

    int getRiverBucket(uint64_t board, uint64_t hand){
        assert(__builtin_popcountll(board) == 6);
        hand ^= hand & board;
        float eq = getEquity(board, hand, 3);
        int str = int(eq*50);
        if(str == 50) str = 49;
        return str;
    }
};

#endif // TREECARDBUCKET_HPP
