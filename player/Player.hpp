#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "game/GameTree.hpp"

struct Player {
    virtual ~Player() = default;
    virtual void init(string policy_path) = 0;
    virtual unique_ptr<Action> getResponseAction() = 0;
    virtual void receiveAction(unique_ptr<Action> action) = 0;
};

#endif // PLAYER_HPP