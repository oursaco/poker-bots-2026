#ifndef BEST_RESPONSE_HPP
#define BEST_RESPONSE_HPP

#include <array>
#include <cassert>
#include <limits>
#include <vector>
#include "cfr/CFR.hpp"
#include "external/omp/Random.h"
#include "game/GameTree.hpp"

using namespace std;

struct BestResponseResult {
    float value = 0.0f; // value from the best-response player's perspective
    vector<int> best_action; // best_action[info_set] = action index, or -1 if never reached
};

struct BestResponseEvaluator {
    GameTree* tree = nullptr;
    vector<vector<int>> children;
    vector<int> postorder;
    array<int, POLICY_SZ> moves_per_info_set;

    void setTree(GameTree* tree_){
        tree = tree_;
        buildIndex();
        tree->fillMovesPerInfoSet(moves_per_info_set);
    }

    // Monte Carlo best response: samples chance nodes instead of exact chance evaluation.
    BestResponseResult computeBestResponse(
        const DCFRPolicy& opponent_policy,
        int opponent_player,
        int seed,
        int num_samples = 100000)
    {
        assert(tree != nullptr);
        assert(opponent_player == 0 || opponent_player == 1);
        const int br_player = opponent_player ^ 1;
    
        const int num_nodes = tree->nodeCount();
        const int info_set_count = tree->infoSetCount();
    
        // Build a forward order (root -> leaves) once, for reach-prob propagation.
        // (postorder already exists for bottom-up DP)
        
        vector<int> forward_order;
        forward_order.reserve(num_nodes);
        {
            vector<char> seen(num_nodes, 0);
            vector<int> st;
            st.push_back(0);
            seen[0] = 1;
            while(!st.empty()){
                int v = st.back(); st.pop_back();
                forward_order.push_back(v);
                for(int c : children[v]){
                    if(!seen[c]){
                        seen[c] = 1;
                        st.push_back(c);
                    }
                }
            }
            assert((int)forward_order.size() == num_nodes && "Tree not fully reachable from root.");
        }

    
        // Initialize BR policy (per infoset): -1 = unseen/unused; default to 0 when needed.
        vector<int> best_action(info_set_count, -1);
        for(int I = 0; I < info_set_count; ++I){
            if(moves_per_info_set[I] > 0) best_action[I] = 0;
        }
    
        auto better = [&](double a, double b){
            // utility[] assumed to be player-0 payoff.
            // BR player chooses max if br_player==0 else min (zero-sum).
            return (br_player == 0) ? (a > b) : (a < b);
        };

    
        omp::XoroShiro128Plus rng(seed);
    
        // Policy iteration: evaluate current best_action, then improve.
        const int max_iters = 3; // usually converges much earlier
        for(int iter = 0; iter < max_iters; ++iter){
            cout << "Iteration " << iter << endl;
            vector<vector<double>> Q(opponent_policy.info_set_count);
            for(int I = 0; I < opponent_policy.info_set_count; ++I){
                Q[I].assign(moves_per_info_set[I], 0.0);
            }
    
            // Collect Q over samples
            for(int sample = 0; sample < num_samples; ++sample){
                // cout << "Iter " << sample << endl;
                const int sample_seed = (int)rng();
                tree->prepare(sample_seed);
    
                array<float, TREE_SZ> utility;
                fill(utility.begin(), utility.end(), 0.0f);
                tree->updateUtility(utility);
    
                // Counterfactual reach weights w[node] = pi_{-br}(node) under opponent + sampled chance.
                // Since chance is sampled from its distribution, we propagate with prob 1 along sampled branch.
                vector<double> w(num_nodes, 0.0);
                w[0] = 1.0 / (double)num_samples;
    
                for(int node : forward_order){
                    if(children[node].empty()) continue;
    
                    int turn = tree->getTurn(node);
                    if(turn == opponent_player){
                        int I = tree->getInfoSet(node);
                        assert(I >= 0 && I < info_set_count);
                        for(int c : children[node]){
                            int move = tree->getMove(c);
                            double p = (double)opponent_policy.getAvgProb(I, move);
                            w[c] += w[node] * p;
                        }
                    } else if(turn == br_player){
                        // Ignore BR player's action in w (counterfactual): copy weight to all children.
                        for(int c : children[node]){
                            w[c] += w[node];
                        }
                    } else {
                        assert(false && "Unexpected player index.");
                    }
                }
    
                // Bottom-up value under current best_action policy (and opponent expectations).
                vector<double> V(num_nodes, 0.0);
                for(int node : postorder){
                    if(children[node].empty()){
                        V[node] = (double)utility[node];
                        continue;
                    }
                    int turn = tree->getTurn(node);
                    if(turn == br_player){
                        int I = tree->getInfoSet(node);
                        assert(I >= 0 && I < info_set_count);
                        // int a = best_action[I];
                        // if(a < 0) a = 0;
                        // assert(a >= 0 && a < (int)children[node].size());
                        // V[node] = V[children[node][a]];
                        int chosen_move = best_action[I]; // move id in [0, moves_per_info[I])
                        int chosen_child = -1;
                        for (int c : children[node]) {
                            if (tree->getMove(c) == chosen_move) { chosen_child = c; break; }
                        }
                        assert(chosen_child != -1);
                        V[node] = V[chosen_child];
                        continue;
                    }
                    // opponent node
                    int I = tree->getInfoSet(node);
                    assert(I >= 0 && I < info_set_count);
                    double sum = 0.0;
                    for(int c : children[node]){
                        int move = tree->getMove(c);
                        double p = (double)opponent_policy.getAvgProb(I, move);
                        sum += p * V[c];
                    }
                    V[node] = sum;
                }
    
                // Accumulate counterfactual Q at BR infosets
                for(int node = 0; node < num_nodes; ++node){
                    if(tree->getTurn(node) != br_player) continue;
                    double wn = w[node];
                    if(wn == 0.0) continue;
    
                    int I = tree->getInfoSet(node);
                    assert(I >= 0 && I < info_set_count);
                    // int A = (int)children[node].size();
                    // for(int a = 0; a < A; ++a){
                    //     Q[I][a] += wn * V[children[node][a]];
                    // }
                    for (int c : children[node]) {
                        int move = tree->getMove(c);
                        Q[I][move] += wn * V[c];
                    }
                }
            }
    
            // Improve policy
            bool changed = false;
            for(int I = 0; I < info_set_count; ++I){
                int A = moves_per_info_set[I];
                if(A <= 0) continue;
    
                int best_a = 0;
                double best_q = Q[I][0];
                for(int a = 1; a < A; ++a){
                    if(better(Q[I][a], best_q)){
                        best_q = Q[I][a];
                        best_a = a;
                    }
                }
                if(best_action[I] != best_a){
                    best_action[I] = best_a;
                    changed = true;
                }
            }
            // if(!changed) { cout << "Converged in " << iter << " iterations" << endl; break;} // converged
            if(!changed) {break;}
        }
    
        // Final evaluation under converged best_action (using same sampling scheme)
        double total_value = 0.0;
        for(int sample = 0; sample < num_samples; ++sample){
            int sample_seed = (int)rng();
            tree->prepare(sample_seed);
    
            array<float, TREE_SZ> utility;
            fill(utility.begin(), utility.end(), 0.0f);
            tree->updateUtility(utility);
    
            vector<double> V(num_nodes, 0.0);
            for(int node : postorder){
                if(children[node].empty()){
                    V[node] = (double)utility[node];
                    continue;
                }
                int turn = tree->getTurn(node);
                if(turn == br_player){
                    int I = tree->getInfoSet(node);
                    // int a = best_action[I];
                    // if(a < 0) a = 0;
                    // V[node] = V[children[node][a]];
                    int chosen_move = best_action[I]; // move id in [0, moves_per_info[I])
                    int chosen_child = -1;
                    for (int c : children[node]) {
                        if (tree->getMove(c) == chosen_move) { chosen_child = c; break; }
                    }
                    assert(chosen_child != -1);
                    V[node] = V[chosen_child];
                    continue;
                }
                int I = tree->getInfoSet(node);
                double sum = 0.0;
                for(int c : children[node]){
                    int move = tree->getMove(c);
                    double p = (double)opponent_policy.getAvgProb(I, move);
                    sum += p * V[c];
                }
                V[node] = sum;
            }
    
            total_value += V[0];
        }
    
        double avg_u0 = total_value / (double)num_samples;
    
        BestResponseResult result;
        result.value = (br_player == 0) ? (float)avg_u0 : (float)(-avg_u0);
        result.best_action = std::move(best_action);
        return result;
    }

    void buildIndex(){  
        assert(tree != nullptr);
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
};

#endif // BEST_RESPONSE_HPP
