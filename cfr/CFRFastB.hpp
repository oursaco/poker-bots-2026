#ifndef CFRFAST_B_HPP
#define CFRFAST_B_HPP

#include "cfr/CFRFast.hpp"

// Variant B: no-atomic regret/strategy updates using per-thread buffering + merge.
// This keeps parallelism in the updatePlayer phase while avoiding atomic hot-path writes.
struct FastTrainerB {
    FastPolicy players[2];
    ThreeCardInferenceTree *tree[2];
    array<float, TREE_SZ> utility[2];
    array<float, 2*TREE_SZ> reach_probability[2];
    array<int, 2*TREE_SZ> compressed_children;
    array<int, TREE_SZ> children_map;
    array<vector<int>, TREE_SZ> children_list;
    array<int, TREE_SZ> depth_to_node;
    vector<pair<int, int>> depth_ranges;

    vector<vector<pair<int, float>>> thread_regret_updates;
    vector<vector<pair<int, float>>> thread_strategy_updates;
    int buffered_update_count = 0; // approximate number of updates emitted per updatePlayer call (buffered ranges)

    void ensureThreadBuffers(){
        int n = omp_get_max_threads();
        if((int)thread_regret_updates.size() != n){
            thread_regret_updates.assign(n, {});
            thread_strategy_updates.assign(n, {});
            int per_thread = buffered_update_count > 0 ? (buffered_update_count / max(1, n) + 512) : 8192;
            for(int t = 0; t < n; t++){
                thread_regret_updates[t].reserve(per_thread);
                thread_strategy_updates[t].reserve(per_thread);
            }
        }
    }

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
            for(int j = 0; j < (int)children_list[i].size(); j++){
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
        pair<int, int>{86, 233},
        pair<int, int>{234, 672},
        pair<int, int>{673, 1830},
        pair<int, int>{1831, 4755},
        pair<int, int>{4756, 11631},
        pair<int, int>{11632, 27102},
        pair<int, int>{27103, 57684},
        pair<int, int>{57685, 103935},
        pair<int, int>{103936, 152256},
        pair<int, int>{152257, 183567},
        pair<int, int>{183568, 194241},
        pair<int, int>{194242, 195573},
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
        for(int i = 0; i < (int)depth_ranges.size(); i++){
            assert(depth_ranges[i] == expected_ranges[i]);
        }
        // FastB buffers updates for ranges [1..16] (inclusive) in updatePlayer().
        buffered_update_count = expected_ranges[16].second - expected_ranges[1].first + 1;
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

    void updatePlayerRangeBuffered(int l, int r, int target_player, int swap_players, int tree_index){
        #pragma omp for schedule(static)
        for(int x = l; x <= r; x++){
            int i = depth_to_node[x];
            int parent = tree[tree_index]->getParentId(i);
            int par_player = tree[tree_index]->getTurn(parent);
            int info = tree[tree_index]->getInfoSet(parent);
            int move = tree[tree_index]->getMove(i);
            reach_probability[tree_index][i << 1] *= reach_probability[tree_index][parent << 1];
            reach_probability[tree_index][i << 1 | 1] *= reach_probability[tree_index][parent << 1 | 1];
            int tid = omp_get_thread_num();
            if((par_player ^ swap_players) == target_player){
                float utility_dif = (par_player ? -1 : 1)*reach_probability[tree_index][parent << 1 | (par_player ^ 1)]*(utility[tree_index][i] - utility[tree_index][parent]);
                int st = players[par_player ^ swap_players].getState(info, move);
                thread_regret_updates[tid].emplace_back(st, utility_dif);
            } else {
                float prob_dif = reach_probability[tree_index][i << 1 | par_player];
                int st = players[par_player ^ swap_players].getState(info, move);
                thread_strategy_updates[tid].emplace_back(st, prob_dif);
            }
        }
    }

    void updatePlayer(int target_player, int swap_players, int tree_index){
        #pragma omp single
        {
            ensureThreadBuffers();
            int nthreads = omp_get_num_threads();
            for(int t = 0; t < nthreads; t++){
                thread_regret_updates[t].clear();
                thread_strategy_updates[t].clear();
            }
        }
        #pragma omp barrier

        for(int i = 1; i <= 16; i++){
            updatePlayerRangeBuffered(expected_ranges[i].first, expected_ranges[i].second, target_player, swap_players, tree_index);
        }

        int reg_player = target_player;
        int strat_player = target_player ^ 1;
        int tid = omp_get_thread_num();
        auto sort_by_state = [](const pair<int, float>& a, const pair<int, float>& b){
            return a.first < b.first;
        };
        std::sort(thread_regret_updates[tid].begin(), thread_regret_updates[tid].end(), sort_by_state);
        std::sort(thread_strategy_updates[tid].begin(), thread_strategy_updates[tid].end(), sort_by_state);
        #pragma omp barrier
        #pragma omp single
        {
            int nthreads = omp_get_num_threads();
            for(int t = 0; t < nthreads; t++){
                auto &rv = thread_regret_updates[t];
                for(size_t k = 0; k < rv.size(); ){
                    int st = rv[k].first;
                    float sum = 0.0f;
                    do { sum += rv[k].second; k++; } while(k < rv.size() && rv[k].first == st);
                    players[reg_player].regret_sum[st] += sum;
                }
                auto &sv = thread_strategy_updates[t];
                for(size_t k = 0; k < sv.size(); ){
                    int st = sv[k].first;
                    float sum = 0.0f;
                    do { sum += sv[k].second; k++; } while(k < sv.size() && sv[k].first == st);
                    players[strat_player].strategy_sum[st] += sum;
                }
            }
        }
        #pragma omp barrier
    }

    void train(int seed, int iterations, float log_every_secs, float checkpoint_every_secs, string player0_dir, string player1_dir, string checkpoint_dir, int previous_iteration = 0){
        omp::XoroShiro128Plus rng(seed);
        auto start_time = chrono::high_resolution_clock::now();
        auto last_log_time = start_time;
        auto last_checkpoint_time = start_time;

        players[0].initPolicy(tree[0]);
        players[1].initPolicy(tree[0]);
        if(player0_dir.size() > 0) players[0].loadPolicy(player0_dir);
        if(player1_dir.size() > 0) players[1].loadPolicy(player1_dir);

        float alpha = 1.5f;
        float beta = 0.0f;
        float gamma = 2.0f;
        #pragma omp parallel
        {
            for(int i = previous_iteration + 1; i <= iterations; i++){
                int seed0 = 0, seed1 = 0;
                float pos_mult = 0.0f, neg_mult = 0.0f, strat_mult = 0.0f;
                #pragma omp single copyprivate(seed0, seed1, pos_mult, neg_mult, strat_mult)
                {
                    float t = i;
                    pos_mult = pow(t, alpha)/(pow(t, alpha) + 1);
                    neg_mult = pow(t, beta)/(pow(t, beta) + 1);
                    strat_mult = pow(float(t)/float(t + 1), gamma);
                    seed0 = static_cast<int>(rng());
                    seed1 = static_cast<int>(rng());
                }
                #pragma omp barrier

                #pragma omp for schedule(static)
                for(int j = 0; j < players[0].state_count; j++){
                    players[0].regret_sum[j] *= (players[0].regret_sum[j] > 0.0f ? pos_mult : neg_mult);
                    players[1].regret_sum[j] *= (players[1].regret_sum[j] > 0.0f ? pos_mult : neg_mult);
                    players[0].strategy_sum[j] *= strat_mult;
                    players[1].strategy_sum[j] *= strat_mult;
                }
                #pragma omp barrier

                tree[0]->prepare(seed0);
                tree[1]->prepare(seed1);
                #pragma omp barrier

                updateUtility(i%2, 0);
                updateUtility(i%2, 1);
                #pragma omp barrier

                updatePlayer(0, i%2, 0);
                updatePlayer(1, i%2, 1);
                #pragma omp barrier

                #pragma omp single
                {
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
                #pragma omp barrier
            }
        }

        cout << "Finished training in " << chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - start_time).count() << " seconds" << endl;
        if(checkpoint_dir.size() > 0){
            players[0].savePolicy(checkpoint_dir + "/player0_final.bin");
            players[1].savePolicy(checkpoint_dir + "/player1_final.bin");
        }
    }
};

#endif // CFRFAST_B_HPP

