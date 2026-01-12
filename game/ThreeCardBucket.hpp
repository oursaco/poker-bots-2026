#ifndef TREECARDBUCKET_HPP
#define TREECARDBUCKET_HPP

#include <cstdint>
#include <vector>
#include <utility>
#include <algorithm>
#include <fstream>
#include <iostream>
#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "game/ThreeCardState.hpp"
using namespace std;

struct ThreeCardBucket {
    virtual ~ThreeCardBucket() = default;
    virtual int countBuckets(ThreeCardGameState* state) = 0;
    virtual int getPreflopBucket(uint64_t hand) = 0;
    virtual int getFlopBucket(uint64_t board, uint64_t hand, int discard) = 0;
    virtual int getTurnBucket(uint64_t board, uint64_t hand, int discard) = 0;
    virtual int getRiverBucket(uint64_t board, uint64_t hand, int discard) = 0;
    virtual int getWinner(uint64_t board, uint64_t hand1, uint64_t hand2) = 0;
    virtual int getBBDiscardBucket(uint64_t board, uint64_t hand) = 0;
    virtual int getSBDiscardBucket(uint64_t board, uint64_t hand, int discard) = 0;
};

struct EHSThreeCardBucket : ThreeCardBucket {
    unsigned preflop_buckets[52][52][52];
    int map_to[1 << 20];
    float eq[4][169][78294];
    int len = 0;
    omp::HandEvaluator hand_eval;

    void readMap(string tar_dir){
        ifstream inf(tar_dir, ios::binary);
        for(int i = 0; i < (1 << 20); i++){
            inf.read(reinterpret_cast<char*>(&map_to[i]), sizeof(int));
            len = max(len, map_to[i]);
        }
        inf.close();
        assert(len == 78292);
    }

    void readTable(string tar_dir, int ind){
        ifstream inf(tar_dir, ios::binary);
        for(int i = 0; i < 169; i++){
            for(int j = 0; j <= len; j++){
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
            int bucket_idx = map_to[trips_encodings[stage]];
            if (bucket_idx >= 0 && bucket_idx <= len) {
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
            int bucket_idx = map_to[flush_encodings[stage]];
            if (bucket_idx >= 0 && bucket_idx <= len) {
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
            int bucket_idx = map_to[straight_encodings[stage]];
            if (bucket_idx >= 0 && bucket_idx <= len) {
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
        } else if(state->street == 1){
            return 6*6*6*8;
        } else if(state->street == 2){
            return 6*6*6*8;
        } else if(state->street == 3){
            return 6*6*6*8;
        } else if(state->street == 4){
            return 12*2;
        } else if(state->street == 5){
            return 12*2;
        } else if(state->street == 6){
            return 12*2;
        }
    }

    void updateCard(uint64_t &mask, unsigned &suit, unsigned &id){
        unsigned card = __builtin_ctzll(mask);
        mask ^= 1ull << card;
        suit = card%4;
        id = card/4;
    }

    inline unsigned getHoleId(unsigned p1, unsigned p2, unsigned ps1, unsigned ps2){
        assert(p1 <= p2);
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

    int encodeDiscard(omp::Hand board, uint64_t board_mask, int discard){
        assert(discard >= 0 && discard < 52);
        int card = discard/4, suit = discard%4;
        int pairs = 0;
        for(int i = 0; i < 4; i++){
            pairs += board_mask >> (4*card + i) & 1;
        }
        pairs = min(1, pairs);
        return pairs;
    }

    float getEquity(omp::Hand board, uint64_t hand, int ind){
        assert(__builtin_popcountll(hand) == 2);

        unsigned cards[2], suits[2];
        for(int i = 0; i < 2; i++){
            updateCard(hand, suits[i], cards[i]);
        }
        int board_encoded = encodeBoard(board, suits[0] == suits[1], suits[1]);
        int hole_id = getHoleId(cards[0], cards[1], suits[0], suits[1]);
        if(map_to[board_encoded] == -1){
            cout << "Missing board: " << cards[0] << " " << suits[0] << " " << cards[1] << " " << suits[1] << endl;
            return 0.0f;
        }
        float e = eq[ind][hole_id][map_to[board_encoded]];
        if(e < 0.0f){
            // cout << "Missing equity: " << cards[0] << " " << suits[0] << " " << cards[1] << " " << suits[1] << endl;
            return 0.0f;
        }
        return e;
    }

    array<float, 5> discard_eq = {0.20f, 0.40f, 0.60f, 0.80f, 0.90f};

    int getBBDiscardBucket(uint64_t board, uint64_t hand){
        assert(__builtin_popcountll(board) == 2);
        assert(__builtin_popcountll(hand) == 3);
        unsigned cards[3], suits[3];
        for(int i = 0; i < 3; i++){
            updateCard(hand, suits[i], cards[i]);
        }
        int card_match = 0;
        for(int i = 0; i < 3; i++){
            int match = 0;
            for(int j = 0; j < 4; j++){
                if(board >> (4*cards[i] + j) & 1){
                    match = 1;
                }
            }
            card_match |= match << i;
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
            float eq = getEquity(getHand(board_mask), hand_mask, 0);
            int str = 0;
            for(int j = 0; j < discard_eq.size(); j++){
                if(eq >= discard_eq[j]) str = j + 1;
                else break;
            }
            st *= 6;
            st += str;
        }
        return st*8 + card_match;
    }

    int getSBDiscardBucket(uint64_t board, uint64_t hand, int discard){
        assert(__builtin_popcountll(board) == 3);
        assert(__builtin_popcountll(hand) == 3);
        unsigned cards[3], suits[3];
        for(int i = 0; i < 3; i++){
            updateCard(hand, suits[i], cards[i]);
        }
        int card_match = 0;
        for(int i = 0; i < 3; i++){
            int match = 0;
            for(int j = 0; j < 4; j++){
                if(board >> (4*cards[i] + j) & 1){
                    match = 1;
                }
            }
            card_match |= match << i;
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
            float eq = getEquity(getHand(board_mask), hand_mask, 1);
            int str = 0;
            for(int j = 0; j < discard_eq.size(); j++){
                if(eq >= discard_eq[j]) str = j + 1;
                else break;
            }
            st *= 6;
            st += str;
        }
        return st*8 + card_match;
    }

    const array<float, 11> flop_eq_thresholds = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.88f, 0.94f, 0.98f};

    int getFlopBucket(uint64_t board, uint64_t hand, int discard){
        assert(__builtin_popcountll(board) == 4);
        assert(__builtin_popcountll(hand) == 3);
        hand ^= hand & board;
        int discard_encoded = encodeDiscard(getHand(board), board, discard);
        float eq = getEquity(getHand(board), hand, 1);
        int str_bucket = 0;
        for(int i = 0; i < flop_eq_thresholds.size(); i++){
            if(eq >= flop_eq_thresholds[i]) str_bucket = i + 1;
            else break;
        }
        assert(str_bucket <= 11);
        return str_bucket*2 + discard_encoded;
    }

    const array<float, 11> turn_eq_thresholds = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.88f, 0.94f, 0.98f};

    int getTurnBucket(uint64_t board, uint64_t hand, int discard){
        assert(__builtin_popcountll(board) == 5);
        hand ^= hand & board;
        omp::Hand board_hand = getHand(board);
        int discard_encoded = encodeDiscard(board_hand, board, discard);
        float eq = getEquity(board_hand, hand, 2);
        int str_bucket = 0;
        for(int i = 0; i < turn_eq_thresholds.size(); i++){
            if(eq >= turn_eq_thresholds[i]) str_bucket = i + 1;
            else break;
        }
        assert(str_bucket <= 11);
        return str_bucket*2 + discard_encoded;
    }

    const array<float, 11> river_eq_thresholds = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.88f, 0.94f, 0.98f};

    int getRiverBucket(uint64_t board, uint64_t hand, int discard){
        assert(__builtin_popcountll(board) == 6);
        hand ^= hand & board;
        omp::Hand board_hand = getHand(board);
        int discard_encoded = encodeDiscard(board_hand, board, discard);
        float eq = getEquity(board_hand, hand, 3);
        int str_bucket = 0;
        for(int i = 0; i < river_eq_thresholds.size(); i++){
            if(eq >= river_eq_thresholds[i]) str_bucket = i + 1;
            else break;
        }
        assert(str_bucket <= 11);
        return str_bucket*2 + discard_encoded;
    }
};

#endif // TREECARDBUCKET_HPP
