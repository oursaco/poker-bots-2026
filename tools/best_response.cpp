#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <vector>

#include "cfr/CFR.hpp"
#include "cfr/CFRFast.hpp"
#include "game/GameTree.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCardState.hpp"
#include "external/omp/Hand.h"
#include "cfr/CFRFast.hpp"

using namespace omp;
using namespace std;

tuple<int,int,int> hand_map[22100];
float range[22100];

void generateHoleCards(){
    int cnt = 0;
    for(int i=0; i<52; i++){
        for(int j=i+1; j<52; j++){
            for(int k=j+1; k<52; k++){
                tuple<int,int,int> hand = tuple(i,j,k);
                assert(cnt < 22100);
                hand_map[cnt++] = hand;
            }
        }
    }
}

float winProbability(int p1, int p2, vector<int> board, int cards_to_gen){
    
}

void localBR(DCFRPolicy& policy, ThreeCardGameState* state, Hand hole_cards){
    float wp; // win probability
    float pot = state->pot;
    auto actions = state->generateActions();
    for(auto& game_action : actions){
        auto action = static_cast<ThreeCardAction*>(game_action.second.get());
        float amount = action->amount;
        float new_range[22100];
        float fp = 0.0f;

        float sum = 0;
        for(auto u: new_range)
            sum += u;
        for(int i=0; i<22100; i++){
            new_range[i] /= sum;
        }
        float wp; // win probability
        // float utility = fp * pot + (1-fp) * (wp * (pot + asked) - (1-wp) * (asked +
    }
}

int main(){

    generateHoleCards();


    return 0;
}