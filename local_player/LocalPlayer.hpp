#ifndef LOCALPLAYER_HPP
#define LOCALPLAYER_HPP

#include <vector>
#include <utility>
#include <cstdint>
#include "LocalGameState.hpp"
#include "LocalAction.hpp"

using namespace std;

struct LocalPlayer {
    virtual ~LocalPlayer() = default;
    
    virtual vector<pair<LocalAction, float>> getActionDistribution(
        const LocalGameState& state,
        uint64_t hand
    ) = 0;
};

#endif // LOCALPLAYER_HPP
