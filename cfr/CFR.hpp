#ifndef CFR_HPP
#define CFR_HPP

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
    array<int, POLICY_SZ> moves_per_info_set;
    int info_set_count;
    int state_count;
    int log_state_count; // ceil log2 of state_count
    int state_mask; // mask of states (2^log_state_count - 1)

    void initPolicy(GameTree* tree){
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

#include "nash/best_response.hpp"

struct MultiDCFRTrainer : CFRTrainer {
    DCFRPolicy players[2];
    GameTree* tree;
    array<float, POLICY_SZ> utility;
    array<float, 2*TRAINER_SZ> reach_probability;

    void setTree(GameTree* tree_){
        tree = tree_;
    }

    vector<pair<int, int>> thread_ranges;
    vector<pair<int, int>> remaining_ranges;
    array<vector<int>, TREE_SZ> children;
    int len = 0;

    bool dfs(unsigned x, int block_size){
        bool split = false;
        for(int i : children[x]){
            split |= dfs(i, block_size);
        }
        if(!split && tree->getSize(x) > block_size){
            cout << "new thread: " << x + 1 << " " << x + tree->getSize(x) << endl;
            thread_ranges.push_back({x + 1, x + tree->getSize(x)});
            len += tree->getSize(x);
            return true;
        }
        return split;
    }

    void buildRanges(int block_size = 670){
        int num_nodes = tree->nodeCount();
        for(int i = 1; i < num_nodes; i++){
            children[tree->getParentId(i)].push_back(i);
        }
        dfs(0, block_size);
        cout << "number of threads: " << thread_ranges.size() << endl;
        cout << "total coverage: " << len << endl;
        if(thread_ranges[0].first > 1) remaining_ranges.push_back({1, thread_ranges[0].first - 1});
        for(int i = 1; i < thread_ranges.size(); i++){
            if(thread_ranges[i].first - 1 > thread_ranges[i - 1].second){
                remaining_ranges.push_back({thread_ranges[i - 1].second + 1, thread_ranges[i].first - 1});
            }
        }
        if(thread_ranges.back().second + 1 < num_nodes) remaining_ranges.push_back({thread_ranges.back().second + 1, num_nodes - 1});
        cout << "number of remaining ranges: " << remaining_ranges.size() << endl;
    }

    void updateUtilityRange(int l, int r, int swap_players){
        for(int i = r; i >= l; i--){
            int parent = tree->getParentId(i);
            int par_player = tree->getTurn(parent);
            int info = tree->getInfoSet(parent);
            int move = tree->getMove(i);
            float probability = players[par_player ^ swap_players].getProb(info, move);
            utility[parent] += probability*utility[i];
            reach_probability[i << 1 | par_player] = probability;
            reach_probability[i << 1 | (par_player ^ 1)] = 1.0f;
        }
    }

    void updateUtility(int swap_players = 0){
        fill(utility.begin(), utility.end(), 0.0f);
        tree->updateUtility(utility);
        int num_nodes = tree->nodeCount();
        #pragma omp parallel for
        for(int i = 0; i < thread_ranges.size(); i++){
            updateUtilityRange(thread_ranges[thread_ranges.size() - i - 1].first, thread_ranges[thread_ranges.size() - i - 1].second, swap_players);
        }
        for(unsigned i = remaining_ranges.size(); i > 0; i--){
            updateUtilityRange(remaining_ranges[i - 1].first, remaining_ranges[i - 1].second, swap_players);
        }
        reach_probability[0] = reach_probability[1] = 1.0f;
    }

    void updatePlayerRange(int l, int r, int target_player, int swap_players, float alpha, float beta, float gamma){
        for(int i = l; i <= r; i++){
            int parent = tree->getParentId(i);
            int par_player = tree->getTurn(parent);
            int info = tree->getInfoSet(parent);
            int move = tree->getMove(i);
            reach_probability[i << 1] *= reach_probability[parent << 1];
            reach_probability[i << 1 | 1] *= reach_probability[parent << 1 | 1];
            if((par_player ^ swap_players) == target_player){
                float utility_dif = (par_player ? -1 : 1)*reach_probability[parent << 1 | (par_player ^ 1)]*(utility[i] - utility[parent]);
                players[par_player ^ swap_players].updateRegret(info, move, utility_dif, alpha, beta);
            } else {
                float prob_dif = reach_probability[i << 1 | par_player];
                players[par_player ^ swap_players].updateStrategy(info, move, prob_dif*gamma);
            }
        }
    }

    // Updates target player regrets
    // Updates other player's strategies
    void updatePlayer(int target_player, int swap_players, float alpha, float beta, float gamma){
        int num_nodes = tree->nodeCount();
        for(unsigned i = 0; i < remaining_ranges.size(); i++){
            updatePlayerRange(remaining_ranges[i].first, remaining_ranges[i].second, target_player, swap_players, alpha, beta, gamma);
        }
        #pragma omp parallel for
        for(unsigned i = 0; i < thread_ranges.size(); i++){
            updatePlayerRange(thread_ranges[i].first, thread_ranges[i].second, target_player, swap_players, alpha, beta, gamma);
        }
    }

    void train(int seed, int iterations, float log_every_secs, float checkpoint_every_secs, string player0_dir, string player1_dir, string checkpoint_dir, int previous_iteration = 0){
        omp::XoroShiro128Plus rng(seed);
        auto start_time = chrono::high_resolution_clock::now();
        auto last_log_time = start_time;
        auto last_checkpoint_time = start_time;
        players[0].initPolicy(tree);
        players[1].initPolicy(tree);
        if(player0_dir.size() > 0) players[0].loadPolicy(player0_dir);
        if(player1_dir.size() > 0) players[1].loadPolicy(player1_dir);
        buildRanges();
        cout << "available processors: " << omp_get_num_procs() << endl;
        cout << "available threads: " << omp_get_max_threads() << endl;
        cout << "number of threads: " << omp_get_num_threads() << endl;
        cout << "dynamic: " << omp_get_dynamic() << endl;
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

// NOTE: This project uses `tools/best_response.cpp` as a header-style include (see tools/nash_distance.cpp).
// We include it here (after DCFRPolicy is defined) so the trainer can run BR evaluations at checkpoints.
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
                float utility_dif = (par_player ? -1 : 1)*reach_probability[parent << 1 | (par_player ^ 1)]*(utility[i] - utility[parent]);
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

        players[0].initPolicy(tree);
        players[1].initPolicy(tree);
        if(player0_dir.size() > 0) players[0].loadPolicy(player0_dir);
        if(player1_dir.size() > 0) players[1].loadPolicy(player1_dir);

        // Run best-response evaluation in parallel with training using a cloned tree,
        // so we don't mutate the training tree from a background thread.
        unique_ptr<GameTree> br_tree = tree->clone();
        future<void> br_future;
        bool br_in_flight = false;
        static std::mutex br_cout_mutex;

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

                // If an evaluation is still running, don't start another one.
                if(br_in_flight){
                    if(br_future.valid() && br_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready){
                        br_future.get();
                        br_in_flight = false;
                    } else {
                        cout << "Skipping BR eval at iteration " << i << " (previous eval still running)" << endl;
                    }
                }

                if(!br_in_flight){
                    // Snapshot policies so BR thread reads an immutable copy.
                    DCFRPolicy p0_snapshot = players[0];
                    DCFRPolicy p1_snapshot = players[1];
                    GameTree* br_tree_ptr = br_tree.get();
                    const int eval_iter = i;
                    const int eval_seed = seed ^ (int)((uint32_t)i * 0x9E3779B9u);
                    const int eval_samples = 100000;

                    br_future = std::async(std::launch::async, [=]() mutable {
                        auto t0 = chrono::high_resolution_clock::now();
                        BestResponseEvaluator evaluator;
                        evaluator.setTree(br_tree_ptr);

                        // BR vs player0 (SB) strategy -> value from player1 (BB) perspective
                        BestResponseResult br_bb = evaluator.computeBestResponse(p0_snapshot, 0, eval_seed, eval_samples);
                        // BR vs player1 (BB) strategy -> value from player0 (SB) perspective
                        BestResponseResult br_sb = evaluator.computeBestResponse(p1_snapshot, 1, eval_seed + 1, eval_samples);

                        float nashconv = br_sb.value + br_bb.value;
                        float exploitability = 0.5f * nashconv;
                        auto secs = chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - t0).count();

                        lock_guard<std::mutex> lock(br_cout_mutex);
                        cout.setf(std::ios::fixed);
                        cout.precision(6);
                        cout << "[BR @ iter " << eval_iter << "] "
                             << "BR(SB)=" << br_sb.value << " "
                             << "BR(BB)=" << br_bb.value << " "
                             << "NashConv=" << nashconv << " "
                             << "Exploitability=" << exploitability << " "
                             << "(" << secs << "s)" << endl;
                    });
                    br_in_flight = true;
                }
            }
        }

        // Ensure background BR evaluation finishes before we return (so br_tree stays alive).
        if(br_in_flight && br_future.valid()){
            br_future.get();
            br_in_flight = false;
        }

        cout << "Finished training in " << chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - start_time).count() << " seconds" << endl;
        players[0].savePolicy(checkpoint_dir + "/player0_final.bin");
        players[1].savePolicy(checkpoint_dir + "/player1_final.bin");
    }
};


#endif // CFR_HPP
