#ifndef CFR_HPP
#define CFR_HPP

#include <array>
#include <utility>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <cassert>
#include <algorithm>
#include <unordered_map>
#include "constants/constants.h"
#include "game/GameTree.hpp"
#include "external/omp/Random.h"
using namespace std;

struct CFRPolicy {
    void loadPolicy(string path);
    void savePolicy(string path);
    float getProb(int node_id, int info_set) const; // returns probability of moving to state from the parent of state given info_set
};

struct CFRTrainer {
    void updateUtility();
    void updateRegrets();
    void train(int seed, int iterations, float log_every_secs, float checkpoint_every_secs, string player0_dir, string player1_dir, string checkpoint_dir);
};

struct DCFRPolicy : CFRPolicy {

    // regret of moving to node i from the parent of i
    // each index is encoded as move_count*info_set_count + info_set
    array<float, POLICY_SZ> regret_sum;
    array<int, POLICY_SZ> info_set_utils;
    array<float, POLICY_SZ> strategy_sum;
    int info_set_count;
    int state_count;
    int log_state_count; // ceil log2 of state_count
    int state_mask; // mask of states (2^log_state_count - 1)

    void initPolicy(vector<int> moves_per_info){
        state_count = 0;
        info_set_count = moves_per_info.size();
        for(int i = 0; i < info_set_count; i++){
            info_set_utils[i] = state_count;
            for(int j = 0; j < moves_per_info[i]; j++){
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
        for(int i = 0; i < moves_per_info.size(); i++){
            info_set_utils[i] += moves_per_info[i] << log_state_count;
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
        assert(info_set < info_set_count);
        float sum = 0.0;
        int st = getState(info_set, 0);
        int move_count = getMoveCount(info_set);
        for(unsigned i = st; i < st + move_count; i++) sum += max(0.0f, regret_sum[i]);
        if(sum > 0.0f) return max(0.0f, regret_sum[st + move_id])/sum;
        return 1.0f/move_count;
    }

    float getAvgProb(int info_set, int move_id) const{
        assert(info_set < info_set_count);
        float sum = 0.0f;
        int st = getState(info_set, 0);
        int move_count = getMoveCount(info_set);
        for(unsigned i = st; i < st + move_count; i++) sum += strategy_sum[i];
        if(sum > 0.0f) return strategy_sum[st + move_id]/sum;
        return 1.0f/move_count;
    }

    void updateRegret(int info_set, int move_id, float dif, float pos, float neg){
        int st = getState(info_set, move_id);
        assert(move_id < getMoveCount(info_set));
        regret_sum[st] *= (regret_sum[st] > 0.0f ? pos : neg);
        regret_sum[st] += dif;
    }

    void updateStrategy(int info_set, int move_id, float prob){
        int st = getState(info_set, move_id);
        assert(move_id < getMoveCount(info_set));
        strategy_sum[st] += prob;
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

struct DCFRTrainer : CFRTrainer {
    DCFRPolicy players[2];
    GameTree* tree;
    array<float, POLICY_SZ> utility;
    array<float, 2*TRAINER_SZ> reach_probability;

    void setTree(GameTree* tree_){
        tree = tree_;
    }

    void updateUtility(int swap_players = 0){
        fill(utility.begin(), utility.end(), 0.0f);
        tree->updateUtility(utility);
        int num_nodes = tree->nodeCount();
        for(int i = num_nodes - 1; i > 0; i--){
            int parent = tree->getParentId(i);
            int par_player = tree->getTurn(parent);
            int info = tree->getInfoSet(parent);
            int move = tree->getMove(i);
            float probability = players[par_player ^ swap_players].getProb(info, move);
            utility[parent] += probability*utility[i];
            reach_probability[i << 1 | par_player] = probability;
            reach_probability[i << 1 | (par_player ^ 1)] = 1.0f;
        }
        reach_probability[0] = reach_probability[1] = 1.0f;
    }

    // Updates target player regrets
    // Updates other player's strategies
    void updatePlayer(int target_player, int swap_players, float alpha, float beta, float gamma){
        int num_nodes = tree->nodeCount();
        for(int i = 1; i < num_nodes; i++){
            int parent = tree->getParentId(i);
            int par_player = tree->getTurn(parent);
            int info = tree->getInfoSet(parent);
            int move = tree->getMove(i);
            reach_probability[i << 1] *= reach_probability[parent << 1];
            reach_probability[i << 1 | 1] *= reach_probability[parent << 1 | 1];
            if((par_player ^ swap_players) == target_player){
                float utility_dif = (target_player ? -1 : 1)*reach_probability[parent << 1 | (par_player ^ 1)]*(utility[i] - utility[parent]);
                players[par_player ^ swap_players].updateRegret(info, move, utility_dif, alpha, beta);
            } else {
                float prob_dif = reach_probability[i << 1 | par_player];
                players[par_player ^ swap_players].updateStrategy(info, move, prob_dif*gamma);
            }
        }
    }

    void train(int seed, int iterations, float log_every_secs, float checkpoint_every_secs, string player0_dir, string player1_dir, string checkpoint_dir, int previous_iteration = 0){
        omp::XoroShiro128Plus rng(seed);
        auto start_time = chrono::high_resolution_clock::now();
        auto last_log_time = start_time;
        auto last_checkpoint_time = start_time;
        players[0].initPolicy(tree->getMovesPerInfoSet());
        players[1].initPolicy(tree->getMovesPerInfoSet());
        if(player0_dir.size() > 0) players[0].loadPolicy(player0_dir);
        if(player1_dir.size() > 0) players[1].loadPolicy(player1_dir);
        float alpha = 1.5f;
        float beta = 0.0f;
        float gamma = 2.0f;
        for(int i = previous_iteration + 1; i <= iterations; i++){
            float t = i;
            float pos_mult = pow(t, alpha)/(pow(t, alpha) + 1);
            float neg_mult = pow(t, beta);
            float strat_mult = pow(t, gamma);
            tree->prepare(rng());
            updateUtility(i%2);
            updatePlayer(0, i%2, pos_mult, neg_mult, strat_mult);
            tree->prepare(rng());
            updateUtility(i%2);
            updatePlayer(1, i%2, pos_mult, neg_mult, strat_mult);
            auto cur_time = chrono::high_resolution_clock::now();
            if(chrono::duration_cast<chrono::seconds>(cur_time - last_log_time).count() >= log_every_secs){
                cout << "Finished iteration " << i << " of " << iterations << " in " << chrono::duration_cast<chrono::seconds>(cur_time - start_time).count() << " seconds" << endl;
                cout << "Utility: " << utility[0] << endl;
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
        players[0].savePolicy(checkpoint_dir + "/player0_final.bin");
        players[1].savePolicy(checkpoint_dir + "/player1_final.bin");
    }
};

struct BestResponseEvaluator {
    GameTree* tree = nullptr;
    vector<vector<int>> children;
    vector<int> postorder;

    void setTree(GameTree* tree_){
        tree = tree_;
        buildIndex();
    }

    void buildIndex(){
        int num_nodes = tree->nodeCount();
        children.assign(num_nodes, {});
        for(int i = 1; i < num_nodes; i++){
            int par = tree->getParentId(i);
            if(par >= 0){
                children[par].push_back(i);
            }
        }
        postorder.clear();
        postorder.reserve(num_nodes);
        vector<int> stack;
        vector<size_t> child_index(num_nodes, 0);
        stack.push_back(0);
        while(!stack.empty()){
            int node = stack.back();
            if(child_index[node] < children[node].size()){
                stack.push_back(children[node][child_index[node]++]);
            } else {
                postorder.push_back(node);
                stack.pop_back();
            }
        }
    }

    // Computes the best response value against a single opponent's strategy.
    // opponent_policy: the fixed strategy we're computing best response against
    // opponent_player: which player (0 or 1) the opponent_policy belongs to
    // Returns: the expected value for the best-responding player (opponent_player ^ 1)
    // 
    // This properly handles imperfect information by:
    // 1. Enumerating all possible card deals (if game supports it)
    // 2. Computing action values at each info set averaged over consistent deals
    // 3. Picking optimal actions at info sets (not individual histories)
    //
    // If the game doesn't support deal enumeration (getNumDeals() returns 0),
    // falls back to Monte Carlo sampling.
    float bestResponseValue(
        const DCFRPolicy& opponent_policy,
        int opponent_player,
        int num_samples = 10000)
    {
        assert(tree != nullptr);
        assert(opponent_player == 0 || opponent_player == 1);
        int br_player = opponent_player ^ 1;  // best response player
        int num_nodes = tree->nodeCount();
        int num_deals = tree->getNumDeals();
        
        // If game doesn't support deal enumeration, use Monte Carlo sampling
        if(num_deals == 0){
            return bestResponseValueMonteCarlo(opponent_policy, opponent_player, num_samples);
        }
        
        // For each info set, compute the best action based on averaged values
        unordered_map<int, int> best_action;  // info_set -> best action index
        
        // Collect action values for each info set across all deals
        // action_values[info_set][action] = sum of values across consistent deals
        unordered_map<int, vector<float>> action_values;
        unordered_map<int, vector<int>> action_counts;
        
        // Phase 1: Compute action values at each BR player info set
        for(int deal_idx = 0; deal_idx < num_deals; deal_idx++){
            tree->prepareDeal(deal_idx);
            
            array<float, TRAINER_SZ> utility;
            fill(utility.begin(), utility.end(), 0.0f);
            tree->updateUtility(utility);
            
            // Compute values bottom-up
            vector<float> value(num_nodes, 0.0f);
            
            for(int node : postorder){
                if(children[node].empty()){
                    value[node] = utility[node];
                    continue;
                }
                int turn = tree->getTurn(node);
                if(turn == -1){
                    // Chance node: average over outcomes
                    float sum = 0.0f;
                    for(int child : children[node]) sum += value[child];
                    value[node] = sum / static_cast<float>(children[node].size());
                    continue;
                }
                if(turn == br_player){
                    // BR player node - record action values for later
                    int info_set = tree->getInfoSet(node);
                    if(action_values.find(info_set) == action_values.end()){
                        action_values[info_set].resize(children[node].size(), 0.0f);
                        action_counts[info_set].resize(children[node].size(), 0);
                    }
                    for(size_t a = 0; a < children[node].size(); a++){
                        action_values[info_set][a] += value[children[node][a]];
                        action_counts[info_set][a]++;
                    }
                    // Use first action temporarily (will recompute with optimal)
                    value[node] = value[children[node][0]];
                    continue;
                }
                // Opponent node - use their strategy
                int info_set = tree->getInfoSet(node);
                float sum = 0.0f;
                for(int child : children[node]){
                    int move = tree->getMove(child);
                    float prob = opponent_policy.getAvgProb(info_set, move);
                    sum += prob * value[child];
                }
                value[node] = sum;
            }
        }
        
        // Phase 2: Determine optimal action at each info set
        for(auto& [info_set, vals] : action_values){
            int best_a = 0;
            float best_val = br_player == 0 ? -1e30f : 1e30f;
            for(size_t a = 0; a < vals.size(); a++){
                float avg_val = vals[a] / max(1, action_counts[info_set][a]);
                if(br_player == 0){
                    if(avg_val > best_val){
                        best_val = avg_val;
                        best_a = a;
                    }
                } else {
                    if(avg_val < best_val){
                        best_val = avg_val;
                        best_a = a;
                    }
                }
            }
            best_action[info_set] = best_a;
        }
        
        // Phase 3: Recompute values using optimal actions
        float total_value = 0.0f;
        for(int deal_idx = 0; deal_idx < num_deals; deal_idx++){
            tree->prepareDeal(deal_idx);
            
            array<float, TRAINER_SZ> utility;
            fill(utility.begin(), utility.end(), 0.0f);
            tree->updateUtility(utility);
            
            vector<float> value(num_nodes, 0.0f);
            
            for(int node : postorder){
                if(children[node].empty()){
                    value[node] = utility[node];
                    continue;
                }
                int turn = tree->getTurn(node);
                if(turn == -1){
                    float sum = 0.0f;
                    for(int child : children[node]) sum += value[child];
                    value[node] = sum / static_cast<float>(children[node].size());
                    continue;
                }
                if(turn == br_player){
                    int info_set = tree->getInfoSet(node);
                    int opt_action = best_action[info_set];
                    value[node] = value[children[node][opt_action]];
                    continue;
                }
                int info_set = tree->getInfoSet(node);
                float sum = 0.0f;
                for(int child : children[node]){
                    int move = tree->getMove(child);
                    float prob = opponent_policy.getAvgProb(info_set, move);
                    sum += prob * value[child];
                }
                value[node] = sum;
            }
            
            total_value += value[0];
        }
        
        // Average over all deals
        float avg_value = total_value / static_cast<float>(num_deals);
        
        // Return from BR player's perspective
        return br_player == 0 ? avg_value : -avg_value;
    }
    
    // Monte Carlo best response with proper info set handling
    // Samples many deals, collects action values at each info set,
    // then picks optimal actions based on averaged values
    float bestResponseValueMonteCarlo(
        const DCFRPolicy& opponent_policy,
        int opponent_player,
        int seed,
        int num_samples = 10000)
    {
        int br_player = opponent_player ^ 1;
        int num_nodes = tree->nodeCount();
        omp::XoroShiro128Plus rng(seed);
        
        // Collect action values at each BR player info set
        unordered_map<int, vector<float>> action_values;
        unordered_map<int, vector<int>> action_counts;
        
        // Phase 1: Sample many deals and collect action values
        for(int sample = 0; sample < num_samples; sample++){
            int sample_seed = rng();
            tree->prepare(sample_seed);
            
            // Sample chance node outcomes for this trajectory
            omp::XoroShiro128Plus chance_rng(sample_seed + 12345);
            vector<int> chance_child(num_nodes, -1);
            for(int node = 0; node < num_nodes; node++){
                if(tree->getTurn(node) == -1 && !children[node].empty()){
                    chance_child[node] = chance_rng() % children[node].size();
                }
            }
            
            array<float, TRAINER_SZ> utility;
            fill(utility.begin(), utility.end(), 0.0f);
            tree->updateUtility(utility);
            
            vector<float> value(num_nodes, 0.0f);
            
            for(int node : postorder){
                if(children[node].empty()){
                    value[node] = utility[node];
                    continue;
                }
                int turn = tree->getTurn(node);
                if(turn == -1){
                    // Chance node: use sampled child
                    value[node] = value[children[node][chance_child[node]]];
                    continue;
                }
                if(turn == br_player){
                    // Record action values at this info set
                    int info_set = tree->getInfoSet(node);
                    if(action_values.find(info_set) == action_values.end()){
                        action_values[info_set].resize(children[node].size(), 0.0f);
                        action_counts[info_set].resize(children[node].size(), 0);
                    }
                    for(size_t a = 0; a < children[node].size(); a++){
                        action_values[info_set][a] += value[children[node][a]];
                        action_counts[info_set][a]++;
                    }
                    // Use first action temporarily
                    value[node] = value[children[node][0]];
                    continue;
                }
                // Opponent: use their strategy
                int info_set = tree->getInfoSet(node);
                float sum = 0.0f;
                for(int child : children[node]){
                    int move = tree->getMove(child);
                    float prob = opponent_policy.getAvgProb(info_set, move);
                    sum += prob * value[child];
                }
                value[node] = sum;
            }
        }
        
        // Phase 2: Determine optimal action at each info set
        unordered_map<int, int> best_action;
        for(auto& [info_set, vals] : action_values){
            int best_a = 0;
            float best_val = br_player == 0 ? -1e30f : 1e30f;
            for(size_t a = 0; a < vals.size(); a++){
                float avg_val = action_counts[info_set][a] > 0 
                    ? vals[a] / action_counts[info_set][a] 
                    : 0.0f;
                if(br_player == 0){
                    if(avg_val > best_val){
                        best_val = avg_val;
                        best_a = a;
                    }
                } else {
                    if(avg_val < best_val){
                        best_val = avg_val;
                        best_a = a;
                    }
                }
            }
            best_action[info_set] = best_a;
        }
        
        // Phase 3: Recompute value using optimal actions
        float total_value = 0.0f;
        rng = omp::XoroShiro128Plus(seed);  // Reset RNG for same samples
        
        for(int sample = 0; sample < num_samples; sample++){
            int sample_seed = rng();
            tree->prepare(sample_seed);
            
            omp::XoroShiro128Plus chance_rng(sample_seed + 12345);
            vector<int> chance_child(num_nodes, -1);
            for(int node = 0; node < num_nodes; node++){
                if(tree->getTurn(node) == -1 && !children[node].empty()){
                    chance_child[node] = chance_rng() % children[node].size();
                }
            }
            
            array<float, TRAINER_SZ> utility;
            fill(utility.begin(), utility.end(), 0.0f);
            tree->updateUtility(utility);
            
            vector<float> value(num_nodes, 0.0f);
            
            for(int node : postorder){
                if(children[node].empty()){
                    value[node] = utility[node];
                    continue;
                }
                int turn = tree->getTurn(node);
                if(turn == -1){
                    value[node] = value[children[node][chance_child[node]]];
                    continue;
                }
                if(turn == br_player){
                    int info_set = tree->getInfoSet(node);
                    int opt_action = best_action.count(info_set) ? best_action[info_set] : 0;
                    value[node] = value[children[node][opt_action]];
                    continue;
                }
                int info_set = tree->getInfoSet(node);
                float sum = 0.0f;
                for(int child : children[node]){
                    int move = tree->getMove(child);
                    float prob = opponent_policy.getAvgProb(info_set, move);
                    sum += prob * value[child];
                }
                value[node] = sum;
            }
            
            total_value += value[0];
        }
        
        float avg_value = total_value / static_cast<float>(num_samples);
        return br_player == 0 ? avg_value : -avg_value;
    }

    // Computes best response value
    // Uses exact enumeration if available, otherwise Monte Carlo
    float bestResponseValueWithSeed(
        const DCFRPolicy& opponent_policy,
        int opponent_player,
        int seed,
        int num_samples = 10000){
        // If game supports deal enumeration, use exact calculation
        if(tree->getNumDeals() > 0){
            return bestResponseValue(opponent_policy, opponent_player);
        }
        // Otherwise use Monte Carlo
        return bestResponseValueMonteCarlo(opponent_policy, opponent_player, seed, num_samples);
    }

};

#endif // CFR_HPP
