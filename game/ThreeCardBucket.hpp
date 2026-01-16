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
#include "bucket_gen/encoding.h"
using namespace std;

struct ThreeCardBucket {
    virtual ~ThreeCardBucket() = default;
    virtual int countBuckets(ThreeCardGameState* state) = 0;
    virtual int getPreflopBucket(uint64_t hand) = 0;
    virtual int getFlopBucket(uint64_t board, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn) = 0;
    virtual int getTurnBucket(uint64_t flop, uint64_t turn, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn) = 0;
    virtual int getRiverBucket(uint64_t flop, uint64_t turn_river, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn) = 0;
    virtual int getWinner(uint64_t board, uint64_t hand1, uint64_t hand2) = 0;
    virtual int getBBDiscardBucket(uint64_t board, uint64_t hand, array<int, 3> order, int equity_buckets) = 0;
    virtual int getSBDiscardBucket(uint64_t board, uint64_t hand, int discard, array<int, 3> order, int equity_buckets) = 0;
    virtual float getEquity(uint64_t board, uint64_t hand) = 0;
};

struct DynamicThreeCardBucket : ThreeCardBucket {

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
        
        // Helper to convert card string to bitmask
        auto cardMask = [this](string card) -> uint64_t {
            return 1ull << getCardId(card);
        };
        
        // Example 1: Trips on board (222) with AA hole cards
        uint64_t trips_hand = cardMask("Ac") | cardMask("Ah");
        omp::Hand trips_board_3 = omp::Hand::empty() + omp::Hand(getCardId("2c")) + omp::Hand(getCardId("2h")) + omp::Hand(getCardId("2d"));
        omp::Hand trips_board_4 = trips_board_3 + omp::Hand(getCardId("7s"));
        omp::Hand trips_board_5 = trips_board_4 + omp::Hand(getCardId("9c"));
        omp::Hand trips_board_6 = trips_board_5 + omp::Hand(getCardId("Th"));
        
        cout << "TRIPS on board with AA hole cards (Board: 222 7 9 T):\n";
        cout << "Hole cards: Ac Ah\n";
        cout << "  3 cards (222):      " << (calcEquity(trips_board_3, trips_hand) * 100) << "%" << endl;
        cout << "  4 cards (2227):     " << (calcEquity(trips_board_4, trips_hand) * 100) << "%" << endl;
        cout << "  5 cards (22279):    " << (calcEquity(trips_board_5, trips_hand) * 100) << "%" << endl;
        cout << "  6 cards (22279T):   " << (calcEquity(trips_board_6, trips_hand) * 100) << "%" << endl;
        
        // Example 2: Flush draw with AhKh hole cards
        uint64_t flush_hand = cardMask("Ac") | cardMask("Kc");
        omp::Hand flush_board_3 = omp::Hand::empty() + omp::Hand(getCardId("2c")) + omp::Hand(getCardId("5c")) + omp::Hand(getCardId("8d"));
        omp::Hand flush_board_4 = flush_board_3 + omp::Hand(getCardId("Kd"));
        omp::Hand flush_board_5 = flush_board_4 + omp::Hand(getCardId("9h"));
        omp::Hand flush_board_6 = flush_board_5 + omp::Hand(getCardId("3c"));
        
        cout << "\nFLUSH DRAW with Ah Kh hole cards (Board: 2c 5c 8d Kd 9h 3c):\n";
        cout << "Hole cards: Ah Kh\n";
        cout << "  3 cards (2c5c8d):       " << (calcEquity(flush_board_3, flush_hand) * 100) << "%" << endl;
        cout << "  4 cards (2c5c8dKd):     " << (calcEquity(flush_board_4, flush_hand) * 100) << "%" << endl;
        cout << "  5 cards (2c5c8dKd9h):   " << (calcEquity(flush_board_5, flush_hand) * 100) << "%" << endl;
        cout << "  6 cards (2c5c8dKd9h3c): " << (calcEquity(flush_board_6, flush_hand) * 100) << "%" << endl;
        
        // Example 3: Straight draw with Tc 6d hole cards (789 board)
        uint64_t straight_hand = cardMask("Tc") | cardMask("9d");
        omp::Hand straight_board_3 = omp::Hand::empty() + omp::Hand(getCardId("7c")) + omp::Hand(getCardId("8h")) + omp::Hand(getCardId("2d"));
        omp::Hand straight_board_4 = straight_board_3 + omp::Hand(getCardId("3s"));
        omp::Hand straight_board_5 = straight_board_4 + omp::Hand(getCardId("Ac"));
        omp::Hand straight_board_6 = straight_board_5 + omp::Hand(getCardId("Kh"));
        
        cout << "\nSTRAIGHT DRAW with Tc 6d hole cards (Board: 789 2 4 Q):\n";
        cout << "Hole cards: Tc 6d\n";
        cout << "  3 cards (789):      " << (calcEquity(straight_board_3, straight_hand) * 100) << "%" << endl;
        cout << "  4 cards (7892):     " << (calcEquity(straight_board_4, straight_hand) * 100) << "%" << endl;
        cout << "  5 cards (78924):    " << (calcEquity(straight_board_5, straight_hand) * 100) << "%" << endl;
        cout << "  6 cards (78924Q):   " << (calcEquity(straight_board_6, straight_hand) * 100) << "%" << endl;
    }

    float getEquity(uint64_t board, uint64_t hand){
        return calcEquity(getHand(board), hand);
    }


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
                //cur_buckets = (state->street == 3 ? 1250 : 1);
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
                // cur_buckets = (state->street == 2 ? 250 : 1);
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
        assert(cur_buckets > 0);
        return cur_buckets;
    }

    omp::HandEvaluator hand_eval;
    vector<uint64_t> three_card_straight_masks;
    vector<uint64_t> four_card_straight_masks;

    void init(string tar_dir){
        encoding::readPreflop(tar_dir + "/preflop.bin");
        encoding::readRiverEncodingMap(tar_dir + "/river_encoding_map.bin");
        encoding::readRiverEquity(tar_dir + "/river_equity.bin");
        encoding::readTurnEncodingMap(tar_dir + "/turn_encoding_map.bin");
        encoding::readTurnEquity(tar_dir + "/turn_equity.bin");
        encoding::readFlopEncodingMap(tar_dir + "/flop_encoding_map.bin");
        encoding::readFlopEquity(tar_dir + "/flop_equity.bin");
        encoding::readThreeEncodingMap(tar_dir + "/three_encoding_map.bin");
        encoding::readThreeEquity(tar_dir + "/three_equity.bin");
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
        printSpecificEquities();
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
            int id = encoding::getHoleId(p1, p2, ps1, ps2);
            int river_state = encoding::encodeRiverBoard(board, ps1, ps2);
            assert(encoding::river_encoding_map[river_state] != -1);
            float equity = encoding::river_eq[id][encoding::river_encoding_map[river_state]];
            assert(equity >= 0.0f && equity <= 1.0f);
            return equity;
        } else if(board.count() == 5){
            int id = encoding::getHoleId(p1, p2, ps1, ps2);
            int turn_state = encoding::encodeTurnBoard(board, ps1, ps2);
            assert(encoding::turn_encoding_map[turn_state] != -1);
            float equity = encoding::turn_eq[id][encoding::turn_encoding_map[turn_state]];
            assert(equity >= 0.0f && equity <= 1.0f);
            return equity;
        } else if(board.count() == 4){
            int id = encoding::getHoleId(p1, p2, ps1, ps2);
            int flop_state = encoding::encodeFlopBoard(board, ps1, ps2);
            assert(encoding::flop_encoding_map[flop_state] != -1);
            float equity = encoding::flop_eq[id][encoding::flop_encoding_map[flop_state]];
            assert(equity >= 0.0f && equity <= 1.0f);
            return equity;
        } else if(board.count() == 3){
            int id = encoding::getHoleId(p1, p2, ps1, ps2);
            int three_state = encoding::encodeThreeBoard(board, ps1, ps2);
            assert(encoding::three_encoding_map[three_state] != -1);
            float equity = encoding::three_eq[id][encoding::three_encoding_map[three_state]];
            assert(equity >= 0.0f && equity <= 1.0f);
            return equity;
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
        return encoding::preflop_buckets[cards[0]][cards[1]][cards[2]];
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

    int getBBDiscardBucket(uint64_t board, uint64_t hand, array<int, 3> order, int equity_buckets){
        if(equity_buckets == 1) return 0;
        assert(__builtin_popcountll(board) == 2);
        assert(__builtin_popcountll(hand) == 3);
        return getDiscardBucket(board, hand, order);
    }

    int getSBDiscardBucket(uint64_t board, uint64_t hand, int discard, array<int, 3> order, int equity_buckets){
        if(equity_buckets == 1) return 0;
        assert(__builtin_popcountll(board) == 3);
        assert(__builtin_popcountll(hand) == 3);
        assert((board & (1ull << discard)) > 0);
        int my_type = getDiscardBucket(board, hand, order);
        omp::Hand board_hand = getHand(board ^ (1ull << discard));
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

    int getFlopBucket(uint64_t board, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn){
        if(equity_buckets == 1) return 0;
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

    array<float, 7> turn_eq_thresholds_64 = {0.20f, 0.30f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f};

    int getTurnBucket64(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(turn_eq_thresholds_64.begin(), turn_eq_thresholds_64.end(), equity) - turn_eq_thresholds_64.begin();
        int board_draws = countDraws(getHand(board), getRankMask(board))/2;
        board_draws = min(board_draws, 3);
        int my_draws = checkMyDraws(getHand(board), getRankMask(board), hand);
        my_draws = min(my_draws, 1);
        return equity_bucket*8 + board_draws*2 + my_draws;
    }

    int getTurnBucket(uint64_t flop, uint64_t turn, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn){
        if(equity_buckets == 1) return 0;
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

    array<float, 19> river_eq_thresholds_20 = {0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.525f, 0.55f, 0.575f, 0.60f, 0.625f, 0.65f, 0.675f, 0.70f, 0.725f, 0.75f, 0.775f, 0.80f, 0.825f, 0.85f};

    int getRiverBucket20(uint64_t board, uint64_t hand){
        float equity = calcEquity(getHand(board), hand);
        int equity_bucket = lower_bound(river_eq_thresholds_20.begin(), river_eq_thresholds_20.end(), equity) - river_eq_thresholds_20.begin();
        return equity_bucket;
    }

    int getRiverBucket(uint64_t flop, uint64_t turn_river, uint64_t hand, int opp_discard, int my_discard, int equity_buckets, int player_turn){
        if(equity_buckets == 1) return 0;
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

    int eval_7(uint64_t mask){
        omp::Hand combined = omp::Hand::empty();
        while(mask){
            int suit, id;
            updateCard(mask, suit, id);
            combined += omp::Hand(4*id + suit);
        }
        return hand_eval.evaluate(combined);
    }

    int eval_8(uint64_t mask){
        int cards[8], suits[8];
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
};

#endif // TREECARDBUCKET_HPP
