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
        assert(pairs > 0);
        pairs = min(2, pairs) - 1;
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
            cout << "Missing equity: " << cards[1] << " " << suits[0] << " " << cards[1] << " " << suits[1] << endl;
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

struct DynamicThreeCardBucket : ThreeCardBucket {

    int countBuckets(ThreeCardGameState* state){
        int spr_bucket = 0;
        if(state->turn == 0) spr_bucket = (state->sb_stack + state->pot - 1)/ state->pot;
        else spr_bucket = (state->bb_stack + state->pot - 1)/ state->pot;
        int spr_scale = 0;
        if(state->street == 4){
            if(1 <= spr_bucket && spr_bucket <= 3){
                spr_scale = 10;
            } else if(4 <= spr_bucket && spr_bucket <= 13){
                spr_scale = 64;
            } else {
                spr_scale = 32;
            }
        } else if(state->street == 5){
            if(1 <= spr_bucket && spr_bucket <= 3){
                spr_scale = 10;
            } else if(4 <= spr_bucket && spr_bucket <= 6){
                spr_scale = 64;
            } else {
                spr_scale = 32;
            }
        } else if(state->street == 6){
            if(1 <= spr_bucket && spr_bucket <= 2){
                spr_scale = 5;
            } else if(3 <= spr_bucket && spr_bucket <= 5){
                spr_scale = 10;
            } else {
                spr_scale = 20;
            }
        }
        int cur_buckets = 0;
        if(state->turn == 0){
            if(state->sb_stack == 0){
                assert(state->street != 0);
                cur_buckets = 1;
            } else {
                if(state->street == 0){
                    cur_buckets = 1755;
                } else if(state->street == 3){
                    cur_buckets = 1250;
                } else if(state->street == 4){
                    assert(spr_scale > 0);
                    cur_buckets = 10*spr_scale;
                } else if(state->street == 5){
                    assert(spr_scale > 0);
                    cur_buckets = 2*8*spr_scale;
                } else if(state->street == 6){
                    assert(spr_scale > 0);
                    cur_buckets = 2*8*spr_scale;
                }
            }
        } else {
            if(state->bb_stack == 0){
                assert(state->street != 0);
                cur_buckets = 1;
            } else {
                if(state->street == 0){
                    cur_buckets = 1755;
                } else if(state->street == 2){
                    cur_buckets = 250;
                } else if(state->street == 4){
                    assert(spr_scale > 0);
                    cur_buckets = 10*spr_scale;
                } else if(state->street == 5){
                    assert(spr_scale > 0);
                    cur_buckets = 2*8*spr_scale;
                } else if(state->street == 6){
                    assert(spr_scale > 0);
                    cur_buckets = 2*8*spr_scale;
                }
            }
        }
        return cur_buckets;
    }

    float river_eq[169][108345];
    int preflop_buckets[52][52][52];
    int river_encoding_map[(1 << 16)*162];
    omp::HandEvaluator hand_eval;
    vector<uint64_t> three_card_straight_masks;
    vector<uint64_t> four_card_straight_masks;

    void init(string tar_dir){
        readPreflop(tar_dir + "/preflop.bin");
        readRiverEncodingMap(tar_dir + "/ehs_map.bin");
        readRiverEquity(tar_dir + "/river_equity.bin");
        for(int i = 1; i + 3 < 12; i++){
            uint64_t mask = 0;
            for(int j = i; j < i + 3; j++){
                mask |= 1ull << j;
            }
            three_card_straight_masks.push_back(mask);
        }
        for(int i = 0; i + 4 < 13; i++){
            uint64_t mask = 0;
            for(int j = i; j < i + 4; j++){
                mask |= 1ull << j;
            }
            four_card_straight_masks.push_back(mask);
        }
    }

    int getHoleId(int p1, int p2, int ps1, int ps2){
        if(ps1 == ps2) return p1*13 + p2;
        return p2*13 + p1;
    }

    int encodeRiverBoard(omp::Hand board, int s1, int s2){
        int str = hand_eval.evaluate(board);
        int suited_state = -1;
        if(s1 == s2){
            // no shared suits, we check if there are 2 suits with 3 cards each
            if(board.suitCount(s1) == 0){
                int cnt = 0;
                for(int i = 0; i < 4; i++) if(i != s1) cnt += board.suitCount(i) == 3;
                if(cnt == 2){
                    suited_state = 0;
                    assert(suited_state >= 0 && suited_state <= 0);
                    // suited state is 0
                } else { // only need to check max suit because there can only be one flush draw
                    // 0 ... 6
                    // we dont care about 0, 1, or 2
                    int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
                    mx_suit = min(4, max(0, mx_suit - 2));
                    suited_state = 1 + mx_suit;
                    assert(suited_state >= 1 && suited_state <= 5);
                    //suited state is 1 ... 5
                }
            } else {
                // we need to check max suit outside of the same suit since there can only be one flush draw other than ours
                int mx_suit = max({board.suitCount((s1 + 1) % 4), board.suitCount((s1 + 2) % 4), board.suitCount((s1 + 3) % 4)});
                int same_suit = board.suitCount(s1);
                // we don't care about 0, 1, or 2
                mx_suit = min(4, max(0, mx_suit - 2));
                same_suit = min(4, max(0, same_suit - 2));
                suited_state = 6 + same_suit*5 + mx_suit;
                assert(suited_state >= 6 && suited_state <= 30);
                // suited state is 6 ... 30
            }
        } else {
            int same1 = board.suitCount(s1);
            int same2 = board.suitCount(s2);
            // no shared suits, we check if there are 2 suits with 3 cards each
            if(same1 == 0 && same2 == 0){
                int cnt = 0;
                for(int i = 0; i < 4; i++) if(i != s1 && i != s2) cnt += board.suitCount(i) == 3;
                if(cnt == 2){
                    suited_state = 31;
                    assert(suited_state >= 31 && suited_state <= 31);
                    // suited state is 31
                } else { // only need to check max suit because there can only be one flush draw
                    int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
                    mx_suit = min(4, max(0, mx_suit - 2));
                    suited_state = 32 + mx_suit;
                    assert(suited_state >= 32 && suited_state <= 36);
                    // suited state is 32 ... 36
                }
            } else {
                // we don't care about 0, 1, or 2
                same1 = min(4, max(0, same1 - 2));
                same2 = min(4, max(0, same2 - 2));
                // we need to check max suit outside of the same suit since there can only be one flush draw other than ours
                int mx_suit = 0;
                for(int i = 0; i < 4; i++) if(i != s1 && i != s2) mx_suit = max(mx_suit, (int)board.suitCount(i));
                mx_suit = min(4, max(0, mx_suit - 2));
                suited_state = 37 + same1*5*5 + same2*5 + mx_suit;
                assert(suited_state >= 37 && suited_state <= 161);
                // suited state is 37 ... 161
            }
        }
        return str*162 + suited_state;
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

    void readRiverEncodingMap(string dir){
        ifstream inf(dir, ios::binary);
        int num_river_buckets;
        inf.read(reinterpret_cast<char*>(&num_river_buckets), sizeof(int));
        assert(num_river_buckets == 108345);
        for(int i = 0; i < (1 << 16)*162; i++){
            inf.read(reinterpret_cast<char*>(&river_encoding_map[i]), sizeof(int));
        }
        inf.close();
    }

    void readRiverEquity(string dir){
        ifstream inf(dir, ios::binary);
        for(int i = 0; i < 169; i++){
            for(int j = 0; j < 108345; j++){
                inf.read(reinterpret_cast<char*>(&river_eq[i][j]), sizeof(float));
            }
        }
        inf.close();
    }

    void updateCard(uint64_t &mask, int &suit, int &id){
        int card = __builtin_ctzll(mask);
        mask ^= 1ull << card;
        suit = card%4;
        id = card/4;
    }

    float calcEquity( omp::Hand board, uint64_t hand){
        assert(__builtin_popcountll(hand) == 2);
        int p1, p2, ps1, ps2;
        updateCard(hand, ps1, p1);
        updateCard(hand, ps2, p2);
        if(board.count() == 6){
            int id = getHoleId(p1, p2, ps1, ps2);
            int river_state = encodeRiverBoard(board, ps1, ps2);
            assert(river_encoding_map[river_state] != -1);
            float equity = river_eq[id][river_encoding_map[river_state]];
            assert(equity >= 0.0f && equity <= 1.0f);
            return equity;
        } else if(board.count() == 5){
        } else if(board.count() == 4){
        } else if(board.count() == 3){
        } else {
            assert(false);
        }
    }

    omp::Hand getHand(uint64_t mask){
        omp::Hand hand = omp::Hand::empty();
        while(mask){
            int card = __builtin_ctzll(mask);
            mask ^= 1ull << card;
            hand += omp::Hand(card);
        }
        return hand;
    }

    uint64_t getRankMask(uint64_t mask){
        uint64_t rank_mask = 0;
        while(mask){
            int card = __builtin_ctzll(mask);
            rank_mask |= 1ull << (card/4);
            mask ^= 1ull << card;
        }
        return rank_mask;
    }

    int checkThreeCardStraightNoCard(uint64_t rank_mask){
        for(uint64_t mask : three_card_straight_masks){
            if((rank_mask & mask) == mask) return 1;
        }
        return 0;
    }

    // rank mask + card makes a 3 card
    // doesn't count if rank_mask contains card
    int checkThreeCardStraight(uint64_t rank_mask, uint64_t card_mask){
        for(uint64_t mask : three_card_straight_masks){
            int exists_before = 0;
            if((rank_mask & mask) == mask) exists_before = 1;
            int exists_after = 0;
            if(((rank_mask | card_mask) & mask) == mask) exists_after = 1;
            if(!exists_before && exists_after) return 1;
        }
        return 0;
    }

    // rank_mask + card makes a 4 card
    // doesn't count if rank_mask contains card
    int checkFourCardStraight(uint64_t rank_mask, uint64_t card_mask){
        for(uint64_t mask : four_card_straight_masks){
            int exists_before = 0;
            if((rank_mask & mask) == mask) exists_before = 1;
            int exists_after = 0;
            if(((rank_mask | card_mask) & mask) == mask) exists_after = 1;
            if(!exists_before && exists_after) return 1;
        }
        return 0;
    }

    int getPreflopBucket(uint64_t hand){
        assert(__builtin_popcountll(hand) == 3);
        int cards[3];
        for(int i = 0; i < 3; i++){
            cards[i] = __builtin_ctzll(hand);
            hand ^= 1ull << cards[i];
        }
        return preflop_buckets[cards[0]][cards[1]][cards[2]];
    }

    array<float, 4> discard_eq = {0.30f, 0.50f, 0.60f, 0.80f};

    // rank_mask should not include discard_card
    // board_hand should not include discard_card
    int getDiscardType(omp::Hand board_hand, uint64_t rank_mask, int discard_card, int discard_suit){
        assert(2 <= board_hand.count() && board_hand.count() <= 3);
        int suit_match = board_hand.suitCount(discard_suit);
        int straight_match = checkThreeCardStraight(rank_mask, 1ull << discard_card);
        int pair_match = rank_mask >> discard_card & 1;
        int ret = 0;
        if(suit_match + straight_match + pair_match >= 2) ret += 4;
        else {
            if(suit_match == 1) ret += 1;
            else if(straight_match == 1) ret += 2;
            else if(pair_match == 1) ret += 3;
        }
        return ret;
    }

    int getDiscardBucket(uint64_t board, uint64_t hand, array<int, 3> order){
        uint64_t hand_mask = hand;
        int ranks[3], suits[3];
        for(int i = 0; i < 3; i++){
            updateCard(hand_mask, suits[order[i]], ranks[order[i]]);
        }
        int state = 0;
        omp::Hand board_hand = getHand(board);
        uint64_t rank_mask = getRankMask(board);
        int discard_mask = 0;
        int equity_mask = 0;
        for(int i = 0; i < 3; i++){
            discard_mask *= 5;
            discard_mask += getDiscardType(board_hand, rank_mask, ranks[i], suits[i]);
            int discard_card = 4*ranks[i] + suits[i];
            float equity = calcEquity(board_hand + omp::Hand(discard_card), hand ^ (1ull << discard_card));
            equity_mask *= 5;
            equity_mask += lower_bound(discard_eq.begin(), discard_eq.end(), equity) - discard_eq.begin();
        }
        if(discard_mask == 0) return equity_mask;
        return discard_mask + 5*5*5;
    }

    int getBBDiscardBucket(uint64_t board, uint64_t hand, array<int, 3> order){
        assert(__builtin_popcountll(board) == 2);
        assert(__builtin_popcountll(hand) == 3);
        return getDiscardBucket(board, hand, order);
    }

    int getSBDiscardBucket(uint64_t board, uint64_t hand, int discard, array<int, 3> order){
        assert(__builtin_popcountll(board) == 3);
        assert(__builtin_popcountll(hand) == 3);
        assert((board & (1ull << discard)) > 0);
        int my_type = getDiscardBucket(board, hand, order);
        omp::Hand board_hand = getHand(board);
        uint64_t rank_mask = getRankMask(board ^ (1ull << discard));
        int opp_type = getDiscardType(board_hand, rank_mask, discard/4, discard%4);
        return my_type*5 + opp_type;
    }

    int countDraws(omp::Hand board_hand, uint64_t rank_mask){
        int ret = 0;
        for(int i = 0; i < 4; i++){
            if(board_hand.suitCount(i) == 2) ret += 1;
            else if(board_hand.suitCount(i) == 3) ret += 2;
        }
        ret += checkThreeCardStraightNoCard(rank_mask);
        return ret;
    }

    // rank_mask should not contain your hole cards
    int checkMyDraws(omp::Hand board_hand, uint64_t rank_mask, uint64_t hand_mask){
        int p1, p2, ps1, ps2;
        updateCard(hand_mask, ps1, p1);
        updateCard(hand_mask, ps2, p2);
        int flush_match = (ps1 == ps2 && board_hand.suitCount(ps1) == 2);
        int straight_match = checkFourCardStraight(rank_mask, (1ull << p1) | (1ull << p2));
        return min(straight_match + flush_match, 1);
    }

    array<float, 9> flop_eq_thresholds_10 = {0.40f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f};

    int getFlopBucket10(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(flop_eq_thresholds_10.begin(), flop_eq_thresholds_10.end(), equity) - flop_eq_thresholds_10.begin();
        return equity_bucket;
    }

    array<float, 3> flop_eq_thresholds_4 = {0.40f, 0.55f, 0.70f};

    int getFlopBucket32(uint64_t board, uint64_t hand){
        omp::Hand board_hand = getHand(board);
        uint64_t rank_mask = getRankMask(board);
        int board_draws = countDraws(board_hand, rank_mask);
        assert(board_draws < 4);
        int my_draws = checkMyDraws(board_hand, rank_mask, hand);
        assert(my_draws < 2);
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(flop_eq_thresholds_4.begin(), flop_eq_thresholds_4.end(), equity) - flop_eq_thresholds_4.begin();
        assert(equity_bucket < 4);
        return equity_bucket*8 + board_draws*2 + my_draws;
    }

    array<float, 7> flop_eq_thresholds_8 = {0.40f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f};

    int getFlopBucket64(uint64_t board, uint64_t hand){
        omp::Hand board_hand = getHand(board);
        uint64_t rank_mask = getRankMask(board);
        int board_draws = countDraws(board_hand, rank_mask);
        assert(board_draws < 4);
        int my_draws = checkMyDraws(board_hand, rank_mask, hand);
        assert(my_draws < 2);
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(flop_eq_thresholds_8.begin(), flop_eq_thresholds_8.end(), equity) - flop_eq_thresholds_8.begin();
        assert(equity_bucket < 8);
        return equity_bucket*8 + board_draws*2 + my_draws;
    }

    int getFlopBucket(uint64_t board, uint64_t hand, int opp_discard, int my_discard, int turn, int equity_buckets, int player_turn){
        assert(__builtin_popcountll(board) == 4);
        hand ^= hand & board;
        assert(__builtin_popcountll(hand) == 2);
        uint64_t board_mask1 = board ^ (1ull << opp_discard);
        uint64_t board_mask2 = board ^ (1ull << my_discard);
        if(player_turn == 0) board_mask1 ^= 1ull << my_discard;
        else board_mask2 ^= 1ull << opp_discard;
        int opp_type = getDiscardType(getHand(board_mask1), getRankMask(board_mask1), opp_discard/4, opp_discard%4);
        int my_type = getDiscardType(getHand(board_mask2), getRankMask(board_mask2), my_discard/4, my_discard%4);
        my_type = min(my_type, 1);
        int equity_bucket = 0;
        if(equity_buckets == 100) equity_bucket = getFlopBucket10(board, hand);
        else if(equity_buckets == 320) equity_bucket = getFlopBucket32(board, hand);
        else if(equity_buckets == 640) equity_bucket = getFlopBucket64(board, hand);
        else assert(false);
        assert(equity_bucket < equity_buckets/10);
        return equity_bucket*10 + opp_type*2 + my_type;
    } 

    int checkDiscardInteraction(uint64_t flop_mask, uint64_t new_card_mask, int discard_card, int discard_suit){
        omp::Hand flop_hand = getHand(flop_mask);
        omp::Hand new_card_hand = getHand(new_card_mask);
        int discard_type = getDiscardType(flop_hand, getRankMask(flop_mask), discard_card, discard_suit);
        int discard_suit_match = new_card_hand.suitCount(discard_suit) > 0;
        int flush_suit = -1;
        for(int i = 0; i < 4; i++){
            if(flop_hand.suitCount(i) == 2) flush_suit = i;
        }
        int flush_suit_match = flush_suit != -1 && new_card_hand.suitCount(flush_suit) > 0;
        int pair_match = (getRankMask(new_card_mask) & (1ull << discard_card)) > 0;
        if(discard_type == 0) return 0;
        else if(discard_type == 4) return 1;
        else if(discard_type == 1) return 2 + discard_suit_match;
        else if(discard_type == 2) return 4 + flush_suit_match;
        else if(discard_type == 3) return 6 + pair_match;
        else assert(false);
    }

    array<float, 4> turn_eq_thresholds_10 = {0.20f, 0.70f, 0.80f, 0.90f};

    int getTurnBucket10(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(turn_eq_thresholds_10.begin(), turn_eq_thresholds_10.end(), equity) - turn_eq_thresholds_10.begin();
        int board_draws = countDraws(getHand(board), getRankMask(board))/2;
        board_draws = min(board_draws, 1);
        return equity_bucket*2 + board_draws;
    }

    array<float, 7> turn_eq_thresholds_32 = {0.20f, 0.30f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f};

    int getTurnBucket32(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(turn_eq_thresholds_32.begin(), turn_eq_thresholds_32.end(), equity) - turn_eq_thresholds_32.begin();
        int board_draws = countDraws(getHand(board), getRankMask(board))/2;
        board_draws = min(board_draws, 1);
        int my_draws = checkMyDraws(getHand(board), getRankMask(board), hand);
        my_draws = min(my_draws, 1);
        return equity_bucket*4 + board_draws*2 + my_draws;
    }

    array<float, 11> turn_eq_thresholds_64 = {0.20f, 0.30f, 0.50f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f};

    int getTurnBucket64(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(turn_eq_thresholds_64.begin(), turn_eq_thresholds_64.end(), equity) - turn_eq_thresholds_64.begin();
        int board_draws = countDraws(getHand(board), getRankMask(board))/2;
        board_draws = min(board_draws, 2);
        int my_draws = checkMyDraws(getHand(board), getRankMask(board), hand);
        my_draws = min(my_draws, 1);
        return equity_bucket*8 + board_draws*2 + my_draws;
    }

    int getTurnBucket(uint64_t flop, uint64_t turn, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn){
        assert(__builtin_popcountll(flop) == 4);
        assert(__builtin_popcountll(turn) == 1);
        hand ^= hand & flop;
        assert(__builtin_popcountll(hand) == 2);
        uint64_t board_mask1 = flop ^ (1ull << opp_discard);
        uint64_t board_mask2 = flop ^ (1ull << my_discard);
        if(player_turn == 0) board_mask1 ^= 1ull << my_discard;
        else board_mask2 ^= 1ull << opp_discard;
        int opp_type = checkDiscardInteraction(board_mask1, turn, opp_discard/4, opp_discard%4);
        int my_type = getDiscardType(getHand(board_mask2), getRankMask(board_mask2), my_discard/4, my_discard%4);
        my_type = min(my_type, 1);
        int equity_bucket = 0;
        if(equity_buckets == 160) equity_bucket = getTurnBucket10(flop | turn, hand);
        else if(equity_buckets == 512) equity_bucket = getTurnBucket32(flop | turn, hand);
        else if(equity_buckets == 1024) equity_bucket = getTurnBucket64(flop | turn, hand);
        else assert(false);
        assert(equity_bucket < equity_buckets/16);
        return equity_bucket*16 + opp_type*2 + my_type;
    }

    array<float, 4> river_eq_thresholds_5 = {0.20f, 0.50f, 0.70f, 0.90f};

    int getRiverBucket5(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(river_eq_thresholds_5.begin(), river_eq_thresholds_5.end(), equity) - river_eq_thresholds_5.begin();
        return equity_bucket;
    }

    array<float, 9> river_eq_thresholds_10 = {0.20f, 0.30f, 0.50f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.90f};

    int getRiverBucket10(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(river_eq_thresholds_10.begin(), river_eq_thresholds_10.end(), equity) - river_eq_thresholds_10.begin();
        return equity_bucket;
    }

    array<float, 19> river_eq_thresholds_20 = {0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.525f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f};

    int getRiverBucket20(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(river_eq_thresholds_20.begin(), river_eq_thresholds_20.end(), equity) - river_eq_thresholds_20.begin();
        return equity_bucket;
    }

    int getRiverBucket(uint64_t flop, uint64_t turn_river, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn){
        assert(__builtin_popcountll(flop) == 4);
        assert(__builtin_popcountll(turn_river) == 2);
        hand ^= hand & flop;
        assert(__builtin_popcountll(hand) == 2);
        uint64_t board_mask1 = flop ^ (1ull << opp_discard);
        uint64_t board_mask2 = flop ^ (1ull << my_discard);
        if(player_turn == 0) board_mask1 ^= 1ull << my_discard;
        else board_mask2 ^= 1ull << opp_discard;
        int opp_type = checkDiscardInteraction(board_mask1, turn_river, opp_discard/4, opp_discard%4);
        int my_type = getDiscardType(getHand(board_mask2), getRankMask(board_mask2), my_discard/4, my_discard%4);
        my_type = min(my_type, 1);
        int equity_bucket = 0;
        if(equity_buckets == 80) equity_bucket = getRiverBucket5(flop | turn_river, hand);
        else if(equity_buckets == 160) equity_bucket = getRiverBucket10(flop | turn_river, hand);
        else if(equity_buckets == 320) equity_bucket = getRiverBucket20(flop | turn_river, hand);
        else assert(false);
        assert(equity_bucket < equity_buckets/16);
        return equity_bucket*16 + opp_type*2 + my_type;
    }
};

#endif // TREECARDBUCKET_HPP
