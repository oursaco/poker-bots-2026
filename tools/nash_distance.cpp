#include "cfr/CFR.hpp"
#include "tools/best_response.cpp"
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

// Global bucket that must outlive the tree
static std::unique_ptr<NaiveThreeCardBucket> g_three_card_bucket;

struct EvalConfig {
    std::string game;
    std::string player0_policy;
    std::string player1_policy;
    int seed = 0;
    int samples = 1;
    bool use_avg_strategy = true;
};

void printUsage(const char* bin) {
    std::cout << "Usage: " << bin
              << " --game <khun|poker|three_card>"
              << " --player0 <policy.bin>"
              << " --player1 <policy.bin>"
              << " [--seed <int>]"
              << " [--samples <int>]"
              << " [--current]\n";
}

bool parseArgs(int argc, char** argv, EvalConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto nextValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
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
        } else if (arg == "--current") {
            config.use_avg_strategy = false;
        } else if (arg == "--avg") {
            config.use_avg_strategy = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }

    if (config.game.empty() || config.player0_policy.empty() || config.player1_policy.empty()) {
        return false;
    }
    return true;
}

std::unique_ptr<GameTree> createTree(const std::string& game) {
    if (game == "khun") {
        return std::make_unique<KhunPokerGameTree>();
    }
    if (game == "poker") {
        return std::make_unique<PokerGameTree>();
    }
    if (game == "three_card") {
        g_three_card_bucket = std::make_unique<NaiveThreeCardBucket>();
        auto tree = std::make_unique<ThreeCardGameTree>();
        tree->setBucket(g_three_card_bucket.get());
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

    std::unique_ptr<GameTree> tree = createTree(config.game);
    if (!tree) {
        std::cerr << "Unknown game: " << config.game << "\n";
        printUsage(argv[0]);
        return 1;
    }
    tree->init();

    auto player0 = std::make_unique<DCFRPolicy>();
    auto player1 = std::make_unique<DCFRPolicy>();
    player0->initPolicy(tree.get());
    player1->initPolicy(tree.get());
    player0->loadPolicy(config.player0_policy);
    player1->loadPolicy(config.player1_policy);

    BestResponseEvaluator evaluator;
    evaluator.setTree(tree.get());

    // Use exact calculation if game supports deal enumeration, otherwise Monte Carlo
    omp::XoroShiro128Plus rng(config.seed);
    // Exact enumeration
    BestResponseResult br0 = evaluator.computeBestResponse(*player0, 0, rng());
    BestResponseResult br1 = evaluator.computeBestResponse(*player1, 1, rng());
    

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "BR value for BB (against SB's strategy): " << br0.value << "\n";
    std::cout << "BR value for SB (against BB's strategy): " << br1.value << "\n";
    return 0;
}