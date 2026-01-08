#ifndef THREECARDPLAYER_HPP
#define THREECARDPLAYER_HPP

#include "Player.hpp"
#include "game/ThreeCard.hpp"
#include "cfr/CFR.hpp"

struct ThreeCardPlayer : Player {

    ThreeCardGameTree tree;
    DCFRPolicy policy;
    int real_sb_stack;
    int real_bb_stack;
    int real_pot;
    int real_sb_bet;
    int real_bb_bet;

    void init(string policy_path) {

    }

    vector<unique_ptr<Action>> getActions() {
        return {};
    }
    vector<float> getActionProbabilities() {
        return {};
    }
    void applyAction(Action action) {
        return;
    }

};

#endif // THREECARDPLAYER_HPP