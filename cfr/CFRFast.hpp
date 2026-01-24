#ifndef CFRFAST_HPP
#define CFRFAST_HPP

#include <array>
#include <utility>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <future>
#include <limits>
#include <mutex>
#include <string>
#include <vector>
#include <cassert>
#include <omp.h>
#include <set>
#include <algorithm>
#include <unordered_map>
#include "constants/constants.h"
#include "game/GameTree.hpp"
#include "external/omp/Random.h"
#include "game/ThreeCardInference.hpp"
using namespace std;

struct FastPolicy {

    // regret of moving to node i from the parent of i
    // each index is encoded as move_count*info_set_count + info_set
    array<float, POLICY_SZ> regret_sum;
    array<int, POLICY_SZ> info_set_utils;
    array<float, POLICY_SZ> strategy_sum;
    array<int, POLICY_SZ> moves_per_info_set;
    int info_set_count;
    int state_count;
    int log_state_count; // ceil log2 of state_count
    int state_mask; // mask of states (2^log_state_count - 1)

    void initPolicy(ThreeCardInferenceTree* tree){
        state_count = 0;
        info_set_count = tree->infoSetCount();
        tree->fillMovesPerInfoSet(moves_per_info_set);
        for(int i = 0; i < info_set_count; i++){
            info_set_utils[i] = state_count;
            for(int j = 0; j < moves_per_info_set[i]; j++){
                regret_sum[state_count] = 0.0f;
                strategy_sum[state_count] = 0.0f;
                state_count++;
            }
        }
        log_state_count = 0;
        while((1 << log_state_count) <= state_count){
            log_state_count++;
        }
        state_mask = (1 << log_state_count) - 1;
        for(int i = 0; i < info_set_count; i++){
            info_set_utils[i] += moves_per_info_set[i] << log_state_count;
        }
    }

    int getState(int info_set, int move_id) const{
        return (info_set_utils[info_set] & state_mask) + move_id;
    }

    int getMoveCount(int info_set) const{
        return info_set_utils[info_set] >> log_state_count;
    }

    // returns probability of moving to node_id from the parent of node_id given info_set
    float getProb(int info_set, int move_id) const{
        float sum = 0.0;
        int st = getState(info_set, 0);
        int move_count = getMoveCount(info_set);
        for(unsigned i = st; i < st + move_count; i++) sum += max(0.0f, regret_sum[i]);
        if(sum > 0.0f) return max(0.0f, regret_sum[st + move_id])/sum;
        return 1.0f/move_count;
    }

    float getAvgProb(int info_set, int move_id) const{
        float sum = 0.0f;
        int st = getState(info_set, 0);
        int move_count = getMoveCount(info_set);
        for(unsigned i = st; i < st + move_count; i++) sum += strategy_sum[i];
        if(sum > 0.0f) return strategy_sum[st + move_id]/sum;
        return 1.0f/move_count;
    }

    void savePolicy(string path){
        ofstream ouf(path, ios::binary);
        ouf.write(reinterpret_cast<char*>(&info_set_count), sizeof(int));
        ouf.write(reinterpret_cast<char*>(&state_count), sizeof(int));
        for(int i = 0; i < POLICY_SZ; i++){
            ouf.write(reinterpret_cast<char*>(&strategy_sum[i]), sizeof(float));
        }
        for(int i = 0; i < POLICY_SZ; i++){
            ouf.write(reinterpret_cast<char*>(&regret_sum[i]), sizeof(float));
        }
        ouf.close();
    }

    void loadPolicy(string path){
        ifstream inf(path, ios::binary);
        int info_set_count_, state_count_;
        inf.read(reinterpret_cast<char*>(&info_set_count_), sizeof(int));
        inf.read(reinterpret_cast<char*>(&state_count_), sizeof(int));
        assert(info_set_count_ == info_set_count);
        assert(state_count_ == state_count);
        for(int i = 0; i < POLICY_SZ; i++){
            inf.read(reinterpret_cast<char*>(&strategy_sum[i]), sizeof(float));
        }
        for(int i = 0; i < POLICY_SZ; i++){
            inf.read(reinterpret_cast<char*>(&regret_sum[i]), sizeof(float));
        }
        inf.close();
    }
};

// NOTE: This project uses `tools/best_response.cpp` as a header-style include (see tools/nash_distance.cpp).
// We include it here (after DCFRPolicy is defined) so the trainer can run BR evaluations at checkpoints.
struct FastTrainer {
    FastPolicy players[2];
    ThreeCardInferenceTree *tree[2];
    array<float, TREE_SZ> utility[2];
    array<float, 2*TREE_SZ> reach_probability[2];
    array<int, 2*TREE_SZ> compressed_children;
    array<int, TREE_SZ> children_map;
    array<vector<int>, TREE_SZ> children_list;
    array<int, TREE_SZ> depth_to_node;
    vector<pair<int, int>> depth_ranges;

    void buildChildren(){
        int num_nodes = tree[0]->nodeCount();
        for(int i = 1; i < num_nodes; i++){
            int parent = tree[0]->getParentId(i);
            children_list[parent].push_back(i);
        }
        int children_index = 0;
        for(int i = 0; i < num_nodes; i++){
            children_map[i] = children_index;
            compressed_children[children_index++] = children_list[i].size();
            for(int j = 0; j < children_list[i].size(); j++){
                compressed_children[children_index++] = children_list[i][j];
            }
        }
    }

    const array<pair<int, int>, 17> expected_ranges = {
        pair<int, int>{0, 0},
        pair<int, int>{1, 4},
        pair<int, int>{5, 13},
        pair<int, int>{14, 34},
        pair<int, int>{35, 85},
        pair<int, int>{86, 242},
        pair<int, int>{243, 735},
        pair<int, int>{736, 2118},
        pair<int, int>{2119, 5772},
        pair<int, int>{5773, 14736},
        pair<int, int>{14737, 35409},
        pair<int, int>{35410, 75072},
        pair<int, int>{75073, 130980},
        pair<int, int>{130981, 184512},
        pair<int, int>{184513, 216453},
        pair<int, int>{216454, 226839},
        pair<int, int>{226840, 228225},
    };

    void setTree(ThreeCardInferenceTree* tree1_, ThreeCardInferenceTree* tree2_){
        tree[0] = tree1_;
        tree[1] = tree2_;
        buildChildren();
        int cur_index = 0;
        for(int i = 0; i < 100; i++){
            int l = cur_index;
            for(int j : tree[0]->nodes_per_depth[i]){
                depth_to_node[cur_index++] = j;
            }
            int r = cur_index - 1;
            if(l <= r) depth_ranges.emplace_back(l, r);
        }
        assert(depth_ranges.size() == expected_ranges.size());
        for(int i = 0; i < depth_ranges.size(); i++){
            assert(depth_ranges[i] == expected_ranges[i]);
        }
    }

    void updateUtilityRangeFast(int l, int r, int swap_players, int tree_index){
        #pragma omp for schedule(static)
        for(int x = r; x >= l; x--){
            int i = depth_to_node[x];
            int children_index = children_map[i];
            for(int j = 0; j < compressed_children[children_index]; j++){
                int child = compressed_children[children_index + j + 1];
                int player = tree[tree_index]->getTurn(i);
                int info = tree[tree_index]->getInfoSet(i);
                int move = tree[tree_index]->getMove(child);
                float probability = players[player ^ swap_players].getProb(info, move);
                utility[tree_index][i] += probability*utility[tree_index][child];
                reach_probability[tree_index][child << 1 | player] = probability;
                reach_probability[tree_index][child << 1 | (player ^ 1)] = 1.0f;
            }
        }
    }

    void updateUtilityRangeSlow(int l, int r, int swap_players, int tree_index){
        for(int x = r; x >= l; x--){
            int i = depth_to_node[x];
            int children_index = children_map[i];
            for(int j = 0; j < compressed_children[children_index]; j++){
                int child = compressed_children[children_index + j + 1];
                int player = tree[tree_index]->getTurn(i);
                int info = tree[tree_index]->getInfoSet(i);
                int move = tree[tree_index]->getMove(child);
                float probability = players[player ^ swap_players].getProb(info, move);
                utility[tree_index][i] += probability*utility[tree_index][child];
                reach_probability[tree_index][child << 1 | player] = probability;
                reach_probability[tree_index][child << 1 | (player ^ 1)] = 1.0f;
            }
        }
    }

    void updateUtility(int swap_players, int tree_index){
        int num_nodes = tree[tree_index]->nodeCount();
        #pragma omp for schedule(static)
        for(int i = 0; i < num_nodes; i++){
            utility[tree_index][i] = 0.0f;
        }
        tree[tree_index]->updateUtility(utility[tree_index]);
        updateUtilityRangeFast(expected_ranges[15].first, expected_ranges[15].second, swap_players, tree_index);
        updateUtilityRangeFast(expected_ranges[14].first, expected_ranges[14].second, swap_players, tree_index);
        updateUtilityRangeFast(expected_ranges[13].first, expected_ranges[13].second, swap_players, tree_index);
        updateUtilityRangeFast(expected_ranges[12].first, expected_ranges[12].second, swap_players, tree_index);
        updateUtilityRangeFast(expected_ranges[11].first, expected_ranges[11].second, swap_players, tree_index);
        updateUtilityRangeFast(expected_ranges[10].first, expected_ranges[10].second, swap_players, tree_index);
        updateUtilityRangeFast(expected_ranges[9].first, expected_ranges[9].second, swap_players, tree_index);
        #pragma omp single
        {
            for(int i = 8; i >= 0; i--){
                updateUtilityRangeSlow(expected_ranges[i].first, expected_ranges[i].second, swap_players, tree_index);
            }
            reach_probability[tree_index][0] = reach_probability[tree_index][1] = 1.0f;
        }
    }

    void updatePlayerRangeFast(int l, int r, int target_player, int swap_players, int tree_index, float strat_mult){
        #pragma omp for schedule(static)
        for(int x = l; x <= r; x++){
            int i = depth_to_node[x];
            int parent = tree[tree_index]->getParentId(i);
            int par_player = tree[tree_index]->getTurn(parent);
            int info = tree[tree_index]->getInfoSet(parent);
            int move = tree[tree_index]->getMove(i);
            reach_probability[tree_index][i << 1] *= reach_probability[tree_index][parent << 1];
            reach_probability[tree_index][i << 1 | 1] *= reach_probability[tree_index][parent << 1 | 1];
            if((par_player ^ swap_players) == target_player){
                float utility_dif = (par_player ? -1 : 1)*reach_probability[tree_index][parent << 1 | (par_player ^ 1)]*(utility[tree_index][i] - utility[tree_index][parent]);
                int st = players[par_player ^ swap_players].getState(info, move);
                #pragma omp atomic update
                players[par_player ^ swap_players].regret_sum[st] += utility_dif;
            } else {
                float prob_dif = reach_probability[tree_index][i << 1 | par_player];
                int st = players[par_player ^ swap_players].getState(info, move);
                #pragma omp atomic update
                players[par_player ^ swap_players].strategy_sum[st] += prob_dif*strat_mult;
            }
        }
    }

    void updatePlayerRangeSlow(int l, int r, int target_player, int swap_players, int tree_index, float strat_mult){
        for(int x = l; x <= r; x++){
            int i = depth_to_node[x];
            int parent = tree[tree_index]->getParentId(i);
            int par_player = tree[tree_index]->getTurn(parent);
            int info = tree[tree_index]->getInfoSet(parent);
            int move = tree[tree_index]->getMove(i);
            reach_probability[tree_index][i << 1] *= reach_probability[tree_index][parent << 1];
            reach_probability[tree_index][i << 1 | 1] *= reach_probability[tree_index][parent << 1 | 1];
            if((par_player ^ swap_players) == target_player){
                float utility_dif = (par_player ? -1 : 1)*reach_probability[tree_index][parent << 1 | (par_player ^ 1)]*(utility[tree_index][i] - utility[tree_index][parent]);
                int st = players[par_player ^ swap_players].getState(info, move);
                players[par_player ^ swap_players].regret_sum[st] += utility_dif;
            } else {
                float prob_dif = reach_probability[tree_index][i << 1 | par_player];
                int st = players[par_player ^ swap_players].getState(info, move);
                players[par_player ^ swap_players].strategy_sum[st] += prob_dif*strat_mult;
            }
        }
    }

    // Updates target player regrets
    // Updates other player's strategies
    void updatePlayer(int target_player, int swap_players, int tree_index, float strat_mult){
        #pragma omp single
        {
            for(int i = 1; i <= 8; i++){
                updatePlayerRangeSlow(expected_ranges[i].first, expected_ranges[i].second, target_player, swap_players, tree_index, strat_mult);
            }
        }
        updatePlayerRangeFast(expected_ranges[9].first, expected_ranges[9].second, target_player, swap_players, tree_index, strat_mult);
        updatePlayerRangeFast(expected_ranges[10].first, expected_ranges[10].second, target_player, swap_players, tree_index, strat_mult);
        updatePlayerRangeFast(expected_ranges[11].first, expected_ranges[11].second, target_player, swap_players, tree_index, strat_mult);
        updatePlayerRangeFast(expected_ranges[12].first, expected_ranges[12].second, target_player, swap_players, tree_index, strat_mult);
        updatePlayerRangeFast(expected_ranges[13].first, expected_ranges[13].second, target_player, swap_players, tree_index, strat_mult);
        updatePlayerRangeFast(expected_ranges[14].first, expected_ranges[14].second, target_player, swap_players, tree_index, strat_mult);
        updatePlayerRangeFast(expected_ranges[15].first, expected_ranges[15].second, target_player, swap_players, tree_index, strat_mult);
        #pragma omp single
        {
            updatePlayerRangeSlow(expected_ranges[16].first, expected_ranges[16].second, target_player, swap_players, tree_index, strat_mult);
        }
    }

    void decayRegret(float alpha, float beta){
        #pragma omp for schedule(static)
        for(int i = 0; i < players[0].state_count; i++){
            players[0].regret_sum[i] *= (players[0].regret_sum[i] > 0.0f ? alpha : beta);
            players[1].regret_sum[i] *= (players[1].regret_sum[i] > 0.0f ? alpha : beta);
        }
    }

    void train(uint64_t seed, int iterations, float log_every_secs, float checkpoint_every_secs, string player0_dir, string player1_dir, string checkpoint_dir, int previous_iteration = 0){
        omp::XoroShiro128Plus rng(seed);
        auto start_time = chrono::high_resolution_clock::now();
        auto last_log_time = start_time;
        auto last_checkpoint_time = start_time;

        players[0].initPolicy(tree[0]);
        players[1].initPolicy(tree[0]);
        if(player0_dir.size() > 0) players[0].loadPolicy(player0_dir);
        if(player1_dir.size() > 0) players[1].loadPolicy(player1_dir);

        float alpha = 1.5f;
        float beta = 0.5f;
        float gamma = 2.0f;
        for(int i = previous_iteration + 1; i <= iterations; i++){
            float t = i;
            float pos_mult = pow(t, alpha)/(pow(t, alpha) + 1);
            float neg_mult = pow(t, beta)/(pow(t, beta) + 1);
            float strat_mult = pow(t, gamma);
            uint64_t seeds[2] = {rng(), rng()};
            #pragma omp parallel
            {
                decayRegret(pos_mult, neg_mult);
                #pragma omp barrier
                tree[0]->prepare(seeds[0]);
                tree[1]->prepare(seeds[1]);
                #pragma omp barrier
                updateUtility(i%2, 0);
                #pragma omp barrier
                updatePlayer(0, i%2, 0, strat_mult);
                #pragma omp barrier
                updateUtility(i%2, 1);
                #pragma omp barrier
                updatePlayer(1, i%2, 1, strat_mult);
            }
            auto cur_time = chrono::high_resolution_clock::now();
            if(chrono::duration_cast<chrono::seconds>(cur_time - last_log_time).count() >= log_every_secs){
                cout << "Finished iteration " << i << " of " << iterations << " in " << chrono::duration_cast<chrono::seconds>(cur_time - start_time).count() << " seconds" << endl;
                cout << "Utility: " << utility[0][0] << " " << utility[1][0] << endl;
                last_log_time = cur_time;
            }
            if(chrono::duration_cast<chrono::seconds>(cur_time - last_checkpoint_time).count() >= checkpoint_every_secs){
                cout << "Saving checkpoint at iteration " << i << endl;
                last_checkpoint_time = cur_time;
                players[0].savePolicy(checkpoint_dir + "/player0_" + to_string(i) + ".bin");
                players[1].savePolicy(checkpoint_dir + "/player1_" + to_string(i) + ".bin");
            }
        }

        cout << "Finished training in " << chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - start_time).count() << " seconds" << endl;
        if(checkpoint_dir.size() > 0){
            players[0].savePolicy(checkpoint_dir + "/player.bin");
        }
    }
};


#endif // CFRFAST_HPP

