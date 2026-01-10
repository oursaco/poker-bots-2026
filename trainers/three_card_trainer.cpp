#include "cfr/CFR.hpp"
#include "game/ThreeCard.hpp"
#include "constants/constants.h"
#include "game/ThreeCardBucket.hpp"
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
    float log_every_secs = 5.0f;
    float checkpoint_every_secs = 300.0f;
    std::string output_dir;
    std::string player0_policy;
    std::string player1_policy;
};

int main() {
    TrainConfig config;
    config.seed = 42;
    config.player0_policy = "";
    config.player1_policy = "";
    config.output_dir = "./three_card_models";
    config.iterations = 10'000'000;

    ThreeCardGameTree tree;
    EHSThreeCardBucket bucket;
    bucket.init("./bucket_data");
    tree.setBucket(&bucket);
    tree.init();
    tree.prepare(config.seed);
    MultiDCFRTrainer trainer;
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
