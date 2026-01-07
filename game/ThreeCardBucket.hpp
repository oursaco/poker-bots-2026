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
    virtual int evalStrength(uint64_t board, uint64_t hand) = 0;
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
    for(int start = 0; start <= 8; ++start){
        int present = 0;
        int missing = 0;
        for(int k = 0; k < 5; ++k) {
            if(has_rank[start + k]) present++;
            else missing++;
        }
        if(present == 4 && missing == 1){
            straight_draw = true;
            break;
        }
    }
    int ace_low_present = has_rank[12] + has_rank[0] + has_rank[1] + has_rank[2] + has_rank[3];
    if(ace_low_present == 4) straight_draw = true;
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

int getFlopBucket(uint64_t board, uint64_t hand){
    uint64_t mask = board | hand;
    omp::Hand combined = omp::Hand::empty();
    while(mask){
        unsigned suit, id;
        updateCard(mask, suit, id);
        combined += omp::Hand(4*id + suit);
    }
    int strength = hand_eval.evaluate(combined)/4096;
    mask = board | hand;
    return strength*2*2 + is_flush_draw(mask)*2 + is_straight_draw(mask);
}

int getTurnBucket(uint64_t board, uint64_t hand){
    return getFlopBucket(board, hand);
}

int evalStrength(uint64_t board, uint64_t hand){
    unsigned cards[8], suits[8];
    omp::Hand pre[8];
    for(int i = 0; i < 8; i++){
        updateCard(board, suits[i], cards[i]);
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

int getRiverBucket(uint64_t board, uint64_t hand){
    int strength = evalStrength(board, hand);
    return (strength/4096)*4 + (strength%4096)/1024;
}

int countBuckets(ThreeCardGameState* state){
    if(state->street == 0){
        return ((1 << 6) - 1)*13*13 + 12*13 + 12 + 1;
    } else if(state->street == 1){
        return 15*2*2 + 2 + 1 + 1;
    } else if(state->street == 2){
        return 15*2*2 + 2 + 1 + 1;
    } else if(state->street == 3){
        return 15*2*2 + 2 + 1 + 1;
    }
}

};

#endif // TREECARDBUCKET_HPP
