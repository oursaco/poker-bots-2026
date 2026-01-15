#include "cfr/CFR.hpp"
#include "cfr/CFRFast.hpp"

#include "game/KhunPoker.hpp"
#include "game/Poker.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardInference.hpp"
#include "game/ThreeCardBucket.hpp"
#include "tools/best_response_fast.hpp"

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

static unique_ptr<GameTree> createTree(const string& game) {
    if (game == "khun") return make_unique<KhunPokerGameTree>();
    if (game == "poker") return make_unique<PokerGameTree>();
    if (game == "three_card") {
        // Match the bucket used by `trainers/three_card_trainer.cpp` so policies load correctly.
        auto g_three_card_bucket = make_unique<EHSThreeCardBucket>();
        g_three_card_bucket->init("./bucket_data");
        auto tree = make_unique<ThreeCardGameTree>();
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

    if (config.game == "khun") {
        auto tree = make_unique<KhunPokerGameTree>();
        tree->init();
        return evalGeneric(tree.get(), config.player0_policy, config.player1_policy, config.seed, config.samples);
    }
    if (config.game == "poker") {
        auto tree = make_unique<PokerGameTree>();
        tree->init();
        return evalGeneric(tree.get(), config.player0_policy, config.player1_policy, config.seed, config.samples);
    }
    if (config.game == "three_card") {
        // We support BOTH formats:
        //  - FastTrainer policies: `FastPolicy` trained on `ThreeCardInferenceTree`
        //  - DCFRTrainer policies: `DCFRPolicy` trained on `ThreeCardGameTree`
        //
        // Decide which to use by reading the policy header and matching info_set_count.
        int header_infosets = -1, header_states = -1;
        if (!readPolicyHeader(config.player0_policy, header_infosets, header_states)) {
            cerr << "Failed to read policy header from: " << config.player0_policy << "\n";
            return 1;
        }

        g_three_card_bucket = make_unique<EHSThreeCardBucket>();
        g_three_card_bucket->init("./bucket_data");

        // Build inference tree (FastTrainer format). This object is huge, so allocate on heap.
        auto inf_tree = make_unique<ThreeCardInferenceTree>();
        inf_tree->setBucket(g_three_card_bucket.get());
        inf_tree->init();

        if (header_infosets == inf_tree->infoSetCount()) {
            return evalFast(inf_tree.get(), config.player0_policy, config.player1_policy, config.seed, config.samples);
        }

        // Build full tree (DCFRTrainer format).
        auto full_tree = make_unique<ThreeCardGameTree>();
        full_tree->setBucket(g_three_card_bucket.get());
        full_tree->init();

        if (header_infosets == full_tree->infoSetCount()) {
            return evalGeneric(full_tree.get(), config.player0_policy, config.player1_policy, config.seed, config.samples);
        }

        cerr << "Policy/tree mismatch for --game three_card.\n";
        cerr << "  policy header infosets: " << header_infosets << " (states: " << header_states << ")\n";
        cerr << "  inference tree infosets: " << inf_tree->infoSetCount() << "\n";
        cerr << "  full tree infosets:      " << full_tree->infoSetCount() << "\n";
        cerr << "This usually means the policy was trained with a different abstraction/bucket config.\n";
        return 1;
    }

    cerr << "Unknown game: " << config.game << "\n";
    printUsage(argv[0]);
    return 1;
}
