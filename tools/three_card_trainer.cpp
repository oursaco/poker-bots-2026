#include "cfr/CFR.hpp"
#include "game/ThreeCard.hpp"
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

int main() {
    TrainConfig config;
    config.seed = 42;
    config.player0_policy = "./three_card_models/player0_final.bin";
    config.player1_policy = "./three_card_models/player1_final.bin";
    config.output_dir = "./three_card_models";
    config.iterations = 1'000'000;

    ThreeCardGameTree tree;
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

    return 0;
}