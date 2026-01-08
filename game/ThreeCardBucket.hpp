#ifndef TREECARDBUCKET_HPP
#define TREECARDBUCKET_HPP

#include <cstdint>
#include <vector>
#include <utility>
#include <algorithm>
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

#endif // TREECARDBUCKET_HPP
