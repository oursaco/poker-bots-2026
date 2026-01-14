#include "cfr/CFR.hpp"
#include "game/KhunPoker.hpp"
#include "game/Poker.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

using namespace std;

// Global bucket that must outlive the tree (ThreeCardGameTree stores a raw pointer).
static unique_ptr<EHSThreeCardBucket> g_three_card_bucket;

struct EvalConfig {
    string game;
    string player0_policy;
    string player1_policy;
    int seed = 0;
    int samples = 100000;
};

static void printUsage(const char* bin) {
    cout << "Usage: " << bin
         << " --game <khun|poker|three_card>"
         << " --player0 <policy.bin>"
         << " --player1 <policy.bin>"
         << " [--seed <int>]"
         << " [--samples <int>]\n";
}

static bool parseArgs(int argc, char** argv, EvalConfig& config) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        auto nextValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--game") {
            const char* value = nextValue("--game");
            if (!value) return false;
            config.game = value;
        } else if (arg == "--player0") {
            const char* value = nextValue("--player0");
            if (!value) return false;
            config.player0_policy = value;
        } else if (arg == "--player1") {
            const char* value = nextValue("--player1");
            if (!value) return false;
            config.player1_policy = value;
        } else if (arg == "--seed") {
            const char* value = nextValue("--seed");
            if (!value) return false;
            config.seed = std::atoi(value);
        } else if (arg == "--samples") {
            const char* value = nextValue("--samples");
            if (!value) return false;
            config.samples = std::max(1, std::atoi(value));
        } else {
            cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }

    return !config.game.empty() && !config.player0_policy.empty() && !config.player1_policy.empty();
}

EHSThreeCardBucket bucket;

static unique_ptr<GameTree> createTree(const string& game) {
    if (game == "khun") return make_unique<KhunPokerGameTree>();
    if (game == "poker") return make_unique<PokerGameTree>();
    if (game == "three_card") {
        // Match the bucket used by `trainers/three_card_trainer.cpp` so policies load correctly.
        g_three_card_bucket = make_unique<EHSThreeCardBucket>();
        g_three_card_bucket->init("./bucket_data");
        auto tree = make_unique<ThreeCardGameTree>();
        tree->setBucket(&bucket);
        return tree;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    EvalConfig config;
    if (!parseArgs(argc, argv, config)) {
        printUsage(argv[0]);
        return 1;
    }

    unique_ptr<GameTree> tree = createTree(config.game);
    if (!tree) {
        cerr << "Unknown game: " << config.game << "\n";
        printUsage(argv[0]);
        return 1;
    }
    tree->init();

    auto player0 = make_unique<DCFRPolicy>();
    auto player1 = make_unique<DCFRPolicy>();
    player0->initPolicy(tree.get());
    player1->initPolicy(tree.get());
    player0->loadPolicy(config.player0_policy);
    player1->loadPolicy(config.player1_policy);

    BestResponseEvaluator evaluator;
    evaluator.setTree(tree.get());

    omp::XoroShiro128Plus rng(config.seed);
    BestResponseResult br_bb = evaluator.computeBestResponse(*player0, 0, (int)rng(), config.samples);
    BestResponseResult br_sb = evaluator.computeBestResponse(*player1, 1, (int)rng(), config.samples);

    cout << fixed << setprecision(6);
    cout << "BR value for BB (against SB's strategy): " << br_bb.value << "\n";
    cout << "BR value for SB (against BB's strategy): " << br_sb.value << "\n";
    return 0;
}
