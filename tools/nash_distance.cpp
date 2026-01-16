#include "cfr/CFR.hpp"
#include "cfr/CFRFast.hpp"

#include "game/KhunPoker.hpp"
#include "game/Poker.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardInference.hpp"
#include "game/ThreeCardBucket.hpp"
#include "tools/best_response.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <omp.h>

using namespace std;

// Huge; keep on heap (stack overflow otherwise).

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

int main(int argc, char** argv) {
    EvalConfig config;
    if (!parseArgs(argc, argv, config)) {
        printUsage(argv[0]);
        return 1;
    }

    if (config.game == "khun") {
        auto tree = make_unique<KhunPokerGameTree>();
        tree->init();
        BestResponseEvaluator evaluator;
        evaluator.setTree(tree.get());
        DCFRPolicy policy1;
        policy1.loadPolicy(config.player0_policy);
        cout << evaluator.computeBestResponse(policy1,config.seed, config.samples).value << endl;
        return 0;
    }
    if (config.game == "three_card") {
        auto tree = make_unique<ThreeCardGameTree>();
        tree->init();
        BestResponseEvaluator evaluator;
        evaluator.setTree(tree.get());
        FastPolicy policy1;
        policy1.loadPolicy(config.player0_policy);
        cout << evaluator.computeBestResponse(policy1, config.seed, config.samples).value << endl;
        return 0;
    }

    cerr << "Unknown game: " << config.game << "\n";
    printUsage(argv[0]);
    return 1;
}