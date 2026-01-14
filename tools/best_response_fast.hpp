#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <vector>

#include <omp.h>

#include "cfr/CFRFast.hpp" // FastPolicy
#include "external/omp/Random.h"
#include "game/ThreeCardInference.hpp"

struct BestResponseFastResult {
    float value = 0.0f;               // value from the best-response player's perspective
    std::vector<int> best_action;     // best_action[info_set] = move id, or -1 if never reached
};

// Best-response evaluator for FastTrainer checkpoints:
//   - Tree: ThreeCardInferenceTree
//   - Policy: FastPolicy (from cfr/CFRFast.hpp)
//
// Note: ThreeCardInferenceTree::prepare()/updateUtility() use OpenMP worksharing pragmas,
// so we run them inside an OpenMP parallel region here.
struct BestResponseFastEvaluator {
    ThreeCardInferenceTree* tree = nullptr;

    int num_nodes = 0;
    int info_set_count = 0;

    std::unique_ptr<std::array<int, POLICY_SZ>> moves_per_info_set;
    std::vector<std::vector<int>> children;
    std::vector<int> postorder;
    std::vector<int> forward_order;

    void setTree(ThreeCardInferenceTree* tree_) {
        tree = tree_;
        assert(tree != nullptr);
        num_nodes = tree->nodeCount();
        info_set_count = tree->infoSetCount();

        moves_per_info_set = std::make_unique<std::array<int, POLICY_SZ>>();
        tree->fillMovesPerInfoSet(*moves_per_info_set);

        buildIndex();
    }

    BestResponseFastResult computeBestResponse(
        const FastPolicy& opponent_policy,
        int opponent_player,
        int seed,
        int num_samples = 100000)
    {
        assert(tree != nullptr);
        assert(opponent_player == 0 || opponent_player == 1);
        const int br_player = opponent_player ^ 1;

        auto better = [&](double a, double b) {
            // utility[] assumed to be player-0 payoff.
            // BR player chooses max if br_player==0 else min (zero-sum).
            return (br_player == 0) ? (a > b) : (a < b);
        };

        // Initialize BR policy (per infoset): -1 = unseen/unused; default to 0 when needed.
        std::vector<int> best_action(info_set_count, -1);
        for (int I = 0; I < info_set_count; ++I) {
            if ((*moves_per_info_set)[I] > 0) best_action[I] = 0;
        }

        auto utility = std::make_unique<std::array<float, TREE_SZ>>();
        std::vector<double> w(num_nodes, 0.0);
        std::vector<double> V(num_nodes, 0.0);

        omp::XoroShiro128Plus rng(seed);

        // Policy iteration: evaluate current best_action, then improve.
        const int max_iters = 1;
        for (int iter = 0; iter < max_iters; ++iter) {
            cout << "Iter " << iter << endl;
            std::vector<double> Q_state(opponent_policy.state_count, 0.0);

            // rng = omp::XoroShiro128Plus(seed);
            for (int sample = 0; sample < num_samples; ++sample) {
                const int sample_seed = (int)rng();
                cout << sample_seed << endl;
                prepareAndUtility(sample_seed, *utility);

                std::fill(w.begin(), w.end(), 0.0);
                w[0] = 1.0 / (double)num_samples;

                for (int node : forward_order) {
                    if (children[node].empty()) continue;

                    int turn = tree->getTurn(node);
                    if (turn == opponent_player) {
                        int I = tree->getInfoSet(node);
                        for (int c : children[node]) {
                            int move = tree->getMove(c);
                            double p = (double)opponent_policy.getAvgProb(I, move);
                            w[c] += w[node] * p;
                        }
                    } else if (turn == br_player) {
                        // Ignore BR player's action in w (counterfactual): copy weight to all children.
                        for (int c : children[node]) w[c] += w[node];
                    }
                }

                std::fill(V.begin(), V.end(), 0.0);
                for (int node : postorder) {
                    if (children[node].empty()) {
                        V[node] = (double)(*utility)[node];
                        continue;
                    }

                    int turn = tree->getTurn(node);
                    if (turn == br_player) {
                        int I = tree->getInfoSet(node);
                        int chosen_move = best_action[I];
                        int chosen_child = -1;
                        for (int c : children[node]) {
                            if (tree->getMove(c) == chosen_move) { chosen_child = c; break; }
                        }
                        if (chosen_child == -1) {
                            // Fallback: pick the first legal child (should be extremely rare).
                            chosen_child = children[node].front();
                        }
                        V[node] = V[chosen_child];
                        continue;
                    }

                    int I = tree->getInfoSet(node);
                    double sum = 0.0;
                    for (int c : children[node]) {
                        int move = tree->getMove(c);
                        double p = (double)opponent_policy.getAvgProb(I, move);
                        sum += p * V[c];
                    }
                    V[node] = sum;
                }

                // Accumulate counterfactual Q at BR infosets
                for (int node = 0; node < num_nodes; ++node) {
                    if (tree->getTurn(node) != br_player) continue;
                    double wn = w[node];
                    if (wn == 0.0) continue;

                    int I = tree->getInfoSet(node);
                    for (int c : children[node]) {
                        int move = tree->getMove(c);
                        const int st = opponent_policy.getState(I, move);
                        Q_state[st] += wn * V[c];
                    }
                }
            }

            // Improve policy
            bool changed = false;
            for (int I = 0; I < info_set_count; ++I) {
                int A = (*moves_per_info_set)[I];
                if (A <= 0) continue;

                int best_a = 0;
                double best_q = Q_state[opponent_policy.getState(I, 0)];
                for (int a = 1; a < A; ++a) {
                    double qa = Q_state[opponent_policy.getState(I, a)];
                    if (better(qa, best_q)) {
                        best_q = qa;
                        best_a = a;
                    }
                }
                if (best_action[I] != best_a) {
                    best_action[I] = best_a;
                    changed = true;
                }
            }
            if (!changed) break;
        }

        // Final evaluation under converged best_action (using same sampling scheme)
        double total_value = 0.0;
        // rng = omp::XoroShiro128Plus(seed);
        for (int sample = 0; sample < num_samples; ++sample) {
            const int sample_seed = (int)rng();
            prepareAndUtility(sample_seed, *utility);

            std::fill(V.begin(), V.end(), 0.0);
            for (int node : postorder) {
                if (children[node].empty()) {
                    V[node] = (double)(*utility)[node];
                    continue;
                }

                int turn = tree->getTurn(node);
                if (turn == br_player) {
                    int I = tree->getInfoSet(node);
                    int chosen_move = best_action[I];
                    int chosen_child = -1;
                    for (int c : children[node]) {
                        if (tree->getMove(c) == chosen_move) { chosen_child = c; break; }
                    }
                    if (chosen_child == -1) chosen_child = children[node].front();
                    V[node] = V[chosen_child];
                    continue;
                }

                int I = tree->getInfoSet(node);
                double sum = 0.0;
                for (int c : children[node]) {
                    int move = tree->getMove(c);
                    double p = (double)opponent_policy.getAvgProb(I, move);
                    sum += p * V[c];
                }
                V[node] = sum;
            }

            total_value += V[0];
        }

        BestResponseFastResult result;
        double avg_u0 = total_value / (double)num_samples;
        result.value = (br_player == 0) ? (float)avg_u0 : (float)(-avg_u0);
        result.best_action = std::move(best_action);
        return result;
    }

private:
    void buildIndex() {
        children.assign(num_nodes, {});
        for (int i = 1; i < num_nodes; ++i) {
            int par = tree->getParentId(i);
            if (par >= 0) children[par].push_back(i);
        }

        // postorder (DFS)
        postorder.clear();
        postorder.reserve(num_nodes);
        {
            std::vector<int> stack;
            std::vector<size_t> child_index(num_nodes, 0);
            stack.push_back(0);
            while (!stack.empty()) {
                int node = stack.back();
                if (child_index[node] < children[node].size()) {
                    stack.push_back(children[node][child_index[node]++]);
                } else {
                    postorder.push_back(node);
                    stack.pop_back();
                }
            }
        }

        // forward order (reachability)
        forward_order.clear();
        forward_order.reserve(num_nodes);
        {
            std::vector<char> seen(num_nodes, 0);
            std::vector<int> st;
            st.push_back(0);
            seen[0] = 1;
            while (!st.empty()) {
                int v = st.back();
                st.pop_back();
                forward_order.push_back(v);
                for (int c : children[v]) {
                    if (!seen[c]) {
                        seen[c] = 1;
                        st.push_back(c);
                    }
                }
            }
        }
    }

    void prepareAndUtility(int sample_seed, std::array<float, TREE_SZ>& utility) {
        // ThreeCardInferenceTree::prepare/updateUtility rely on OpenMP worksharing pragmas.
        // Run them inside a parallel region.
        #pragma omp parallel
        {
            tree->prepare(sample_seed);
            #pragma omp barrier
            #pragma omp for schedule(static)
            for (int i = 0; i < num_nodes; ++i) {
                utility[i] = 0.0f;
            }
            #pragma omp barrier
            tree->updateUtility(utility);
        }
    }
};

