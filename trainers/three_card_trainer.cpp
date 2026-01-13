#include "cfr/CFR.hpp"
#include "cfr/CFRFast.hpp"
#include "game/ThreeCardInference.hpp"
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
    float checkpoint_every_secs = 3600.0f;
    std::string output_dir;
    std::string player0_policy;
    std::string player1_policy;
};

int main() {
    TrainConfig config;
    config.seed = 1324132;
    config.player0_policy = "./three_card_models/player.bin";
    config.player1_policy = "./three_card_models/player.bin";
    config.output_dir = "./three_card_models";
    config.iterations = 20'000'000;

    ThreeCardInferenceTree tree1;
    ThreeCardInferenceTree tree2;
    EHSThreeCardBucket bucket;
    bucket.init("./bucket_data");
    tree1.setBucket(&bucket);
    tree1.init();
    tree2.setBucket(&bucket);
    tree2.init();
    FastTrainer trainer;
    trainer.setTree(&tree1, &tree2);
    trainer.train(
        config.seed,
        config.iterations,
        config.log_every_secs,
        config.checkpoint_every_secs,
        config.player0_policy,
        config.player1_policy,
        config.output_dir,
	10'000'000);

    return 0;
}
