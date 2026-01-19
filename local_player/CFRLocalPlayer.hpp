#ifndef CFRLOCALPLAYER_HPP
#define CFRLOCALPLAYER_HPP

#include "LocalPlayer.hpp"
#include "LocalGameState.hpp"
#include "LocalAction.hpp"
#include "CFRQueryState.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"
#include "cfr/CFR.hpp"
#include <string>
#include <cassert>
#include <memory>

using namespace std;

struct CFRLocalPlayer : LocalPlayer {
    DynamicThreeCardBucket bucket;
    ThreeCardGameTree tree;
    DCFRPolicy policy;
    array<vector<int>, TREE_SZ> children;
    unique_ptr<CFRQueryStateManager> query_manager;
    
    CFRLocalPlayer() = default;
    
    void init(const string& policy_path, const string& bucket_path = "./emd_bucket_data") {
        bucket.init(bucket_path);
        tree.setBucket(&bucket);
        tree.init();
        
        for (int i = 1; i < tree.nodeCount(); i++) {
            int parent = tree.getParentId(i);
            int move = tree.getMove(i);
            assert(children[parent].size() == move);
            children[parent].push_back(i);
        }
        
        policy.initPolicy(&tree);
        policy.loadPolicy(policy_path);
        
        query_manager = make_unique<CFRQueryStateManager>(tree, policy, children);
    }
    
    // Disable copy and move to prevent issues with internal references
    CFRLocalPlayer(const CFRLocalPlayer&) = delete;
    CFRLocalPlayer& operator=(const CFRLocalPlayer&) = delete;
    CFRLocalPlayer(CFRLocalPlayer&&) = delete;
    CFRLocalPlayer& operator=(CFRLocalPlayer&&) = delete;
    
    CFRQueryState createQueryState(const LocalGameState& state, uint64_t hand, int player_id) {
        return query_manager->createQueryState(state, hand, player_id);
    }
    
    void advanceQueryState(CFRQueryState& qs, const LocalAction& action, 
                          uint64_t hand, int player_id) {
        query_manager->advanceQueryState(qs, action, hand, player_id);
    }
    
    vector<pair<LocalAction, float>> query(const CFRQueryState& qs, uint64_t hand, int player_id) {
        return query_manager->query(qs, hand, player_id);
    }
    
    vector<pair<LocalAction, float>> getActionDistribution(
        const LocalGameState& state,
        uint64_t hand
    ) override {
        if (state.is_terminal || state.turn == Turn::WORLD) {
            return {};
        }
        
        int player_id = static_cast<int>(state.turn);
        CFRQueryState qs = createQueryState(state, hand, player_id);
        return query(qs, hand, player_id);
    }
};

#endif // CFRLOCALPLAYER_HPP
