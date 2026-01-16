#include "cfr/CFRFast.hpp"
#include "game/ThreeCardInference.hpp"
#include "game/ThreeCardBucket.hpp"
#include <string>

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
    config.seed = 389541;
    config.player0_policy = "";
    config.player1_policy = "";
    config.output_dir = "./three_card_models_fast";
    config.iterations = 100'000'000;

    ThreeCardInferenceTree tree1;
    ThreeCardInferenceTree tree2;
    DynamicThreeCardBucket bucket;
    bucket.init("./emd_bucket_data");
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
        config.output_dir);

    return 0;
}

