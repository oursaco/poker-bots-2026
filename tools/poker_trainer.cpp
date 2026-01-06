#include "cfr/CFR.hpp"
#include "game/Poker.hpp"
#include "constants/constants.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <system_error>

struct TrainConfig {
    int seed = 0;
    int iterations = 1'000;
    float log_every_secs = 1.0f;
    float checkpoint_every_secs = 120.0f;
    std::string output_dir;
    std::string player0_policy;
    std::string player1_policy;
};

float averageMoveProb(DCFRPolicy& policy, int info_set, int move_id) {
    int move_count = policy.getMoveCount(info_set);
    if (move_count <= 0) {
        return 0.0f;
    }
    int st = policy.getState(info_set, 0);
    float sum = 0.0f;
    for (int move = 0; move < move_count; ++move) {
        sum += policy.strategy_sum[st + move];
    }
    if (sum <= 0.0f) {
        return 1.0f / move_count;
    }
    return policy.strategy_sum[st + move_id] / sum;
}

int findSmallBlindOpeningNode(GameTree& tree) {
    int num_nodes = tree.nodeCount();
    for (int node = 0; node < num_nodes; ++node) {
        if (tree.getParentId(node) == -1 && tree.getTurn(node) == 0) {
            return node;
        }
    }
    return 0;
}

void printSmallBlindOpeningRange(DCFRTrainer& trainer) {
    GameTree* tree = trainer.tree;
    int opening_node = findSmallBlindOpeningNode(*tree);
    DCFRPolicy& sb_policy = trainer.players[0];
    const char* action_names[] = {"sb fold", "sb call", "sb pot-raise", "sb all-in"};
    const int bucket_count = (1 << 16) / 4096;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Small blind opening range (average):\n";
    for (int bucket = 0; bucket < bucket_count; ++bucket) {
        int info_set = bucket * tree->nodeCount() + opening_node;
        int move_count = sb_policy.getMoveCount(info_set);
        std::cout << "  bucket " << bucket << ": ";
        for (int move = 0; move < move_count; ++move) {
            float prob = averageMoveProb(sb_policy, info_set, move);
            if (move < 4) {
                std::cout << action_names[move] << "=" << prob;
            } else {
                std::cout << "move " << move << "=" << prob;
            }
            if (move + 1 < move_count) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";
    }
}

int main() {
    TrainConfig config;
    config.seed = 42;
    config.player0_policy = "";
    config.player1_policy = "";
    config.output_dir = "./poker_models";
    config.iterations = 100'000;

    PokerGameTree tree;
    tree.init();
    tree.prepare(config.seed);
    DCFRTrainer trainer;
    trainer.setTree(&tree);
    trainer.train(
        config.seed,
        config.iterations,
        config.log_every_secs,
        config.checkpoint_every_secs,
        config.player0_policy,
        config.player1_policy,
        config.output_dir);

    printSmallBlindOpeningRange(trainer);

    return 0;
}
