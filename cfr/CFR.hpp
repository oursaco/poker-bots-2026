#ifndef CFR_HPP
#define CFR_HPP

#include <array>
#include <utility>
#include <chrono>
#include <iostream>
#include <fstream>
#include "game/GameTree.hpp"
#include "external/omp/Random.h"
using namespace std;

struct CFRPolicy {
    void loadPolicy(string path);
    void savePolicy(string path);
    float getProb(int node_id, int info_set); // returns probability of moving to state from the parent of state given info_set
};

struct CFRTrainer {
    void updateUtility();
    void updateRegrets();
    void train(int seed, int iterations, float log_every_secs, float checkpoint_every_secs, string player0_dir, string player1_dir, string checkpoint_dir);
};

const int POLICY_SZ = 1'000;
const int TRAINER_SZ = 1'000;

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

    int getState(int info_set, int move_id){
        return (info_set_utils[info_set] & state_mask) | move_id;
    }

    int getMoveCount(int info_set){
        return info_set_utils[info_set] >> log_state_count;
    }

    // returns probability of moving to node_id from the parent of node_id given info_set
    float getProb(int info_set, int move_id){
        float sum = 0.0;
        int st = getState(info_set, 0);
        int move_count = getMoveCount(info_set);
        for(unsigned i = st; i < st + move_count; i++) sum += max(0.0f, regret_sum[i]);
        if(sum > 0.0f) return max(0.0f, regret_sum[st + move_id])/sum;
        return 1.0f/move_count;
    }

    void updateRegret(int info_set, int move_id, float dif, float pos, float neg){
        int st = getState(info_set, move_id);
        assert(move_id < getMoveCount(info_set));
        regret_sum[st] *= (regret_sum[st] > 0.0f ? pos : neg);
        regret_sum[st] += dif;
    }

    void updateStrategy(int info_set, int move_id, float dif){
        int st = getState(info_set, move_id);
        assert(move_id < getMoveCount(info_set));
        strategy_sum[st] += dif;
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
    array<float, TRAINER_SZ> utility;
    array<float, TRAINER_SZ> reach_probability;

    void setTree(GameTree* tree_){
        tree = tree_;
    }

    void updateUtility(){
        fill(utility.begin(), utility.end(), 0.0f);
        tree->updateUtility(utility);
        int num_nodes = tree->nodeCount();
        for(int i = num_nodes - 1; i > 0; i--){
            int parent = tree->getParentId(i);
            int par_player = tree->getTurn(parent);
            int info = tree->getInfoSet(parent);
            int move = tree->getMove(i);
            float probability = players[par_player].getProb(info, move);
            utility[parent] += probability*utility[i];
            reach_probability[i << 1 | (par_player ^ 1)] = probability;
            reach_probability[i << 1 | par_player] = 1.0f;
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
            if((par_player ^ swap_players) == target_player){
                float utility_dif = (par_player ? -1 : 1)*reach_probability[parent << 1 | par_player]*(utility[i] - utility[parent]);
                players[par_player ^ swap_players].updateRegret(info, move, utility_dif, alpha, beta);
            } else {
                float prob_dif = reach_probability[parent << 1 | par_player]*reach_probability[i << 1 | (par_player ^ 1)];
                players[par_player ^ swap_players].updateStrategy(info, move, prob_dif*gamma);
            }
            reach_probability[i << 1] *= reach_probability[parent << 1];
            reach_probability[i << 1 | 1] *= reach_probability[parent << 1 | 1];
        }
    }

    void train(int seed, int iterations, float log_every_secs, float checkpoint_every_secs, string player0_dir, string player1_dir, string checkpoint_dir){
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
        for(int i = 1; i <= iterations; i++){
            float t = i;
            float pos_mult = pow(t, alpha)/(pow(t, alpha) + 1);
            float neg_mult = pow(t, beta);
            float strat_mult = pow(t, gamma);
            tree->prepare(rng());
            updateUtility();
            updatePlayer(0, i%2, pos_mult, neg_mult, strat_mult);
            tree->prepare(rng());
            updateUtility();
            updatePlayer(1, i%2, pos_mult, neg_mult, strat_mult);
            auto cur_time = chrono::high_resolution_clock::now();
            if(chrono::duration_cast<chrono::seconds>(cur_time - last_log_time).count() >= log_every_secs){
                cout << "Finished iteration " << i << " of " << iterations << " in " << chrono::duration_cast<chrono::seconds>(cur_time - start_time).count() << " seconds" << endl;
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

#endif // CFR_HPP