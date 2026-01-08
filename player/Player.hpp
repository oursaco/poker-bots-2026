#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "game/GameTree.hpp"

struct Player {
    virtual ~Player() = default;
    virtual void init(string policy_path) = 0;
    virtual vector<unique_ptr<Action>> getActions() = 0;
    virtual vector<float> getActionProbabilities() = 0;
    virtual void applyAction(Action action) = 0;

};

#endif // PLAYER_HPP