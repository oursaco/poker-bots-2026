#include "cfr/CFR.hpp"
#include "game/KhunPoker.hpp"
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
    float checkpoint_every_secs = 30.0f;
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

void printSmallBlindOpeningStrategy(DCFRTrainer& trainer) {
    DCFRPolicy& sb_policy = trainer.players[0];
    const char* action_names[] = {"sb raise", "sb check"};

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Small blind opening strategy (average):\n";
    for (int card = 0; card < 3; ++card) {
        int info_set = card;
        int move_count = sb_policy.getMoveCount(info_set);
        std::cout << "  card " << card << ": ";
        for (int move = 0; move < move_count; ++move) {
            float prob = averageMoveProb(sb_policy, info_set, move);
            if (move < 2) {
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
    config.output_dir = "./khun_poker";
    config.iterations = 1'000;

    KhunPokerGameTree tree;
    tree.init();

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

    printSmallBlindOpeningStrategy(trainer);

    return 0;
}
