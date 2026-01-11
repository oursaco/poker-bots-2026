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
    int iterations = 1'000'000;
    float log_every_secs = 1.0f;
    float checkpoint_every_secs = 0.1f;
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

void printBigBlindStrategy(DCFRTrainer& trainer) {
    DCFRPolicy& bb_policy = trainer.players[1];
    GameTree* tree = trainer.tree;
    const char* response_to_raise[] = {"bb fold", "bb call"};
    const char* response_to_check[] = {"bb raise", "bb check"};

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Big blind strategy (average):\n";

    auto print_node = [&](int node_id, const std::string& header, const char** action_names) {
        std::cout << "  " << header << ":\n";
        for (int card = 0; card < 3; ++card) {
            int info_set = node_id * 3 + card;
            int move_count = bb_policy.getMoveCount(info_set);
            std::cout << "    card " << card << ": ";
            for (int move = 0; move < move_count; ++move) {
                float prob = averageMoveProb(bb_policy, info_set, move);
                if (move < 2 && action_names != nullptr) {
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
    };

    int num_nodes = tree->nodeCount();
    for (int node = 0; node < num_nodes; ++node) {
        if (tree->getTurn(node) != 1) {
            continue;
        }
        std::string header = "bb node " + std::to_string(node);
        const char** action_names = nullptr;
        int parent = tree->getParentId(node);
        if (parent == 0) {
            int parent_move = tree->getMove(node);
            if (parent_move == 0) {
                header = "after sb raise";
                action_names = response_to_raise;
            } else if (parent_move == 1) {
                header = "after sb check";
                action_names = response_to_check;
            }
        }
        print_node(node, header, action_names);
    }
}

int main() {
    TrainConfig config;
    config.seed = 42;
    config.player0_policy = "";
    config.player1_policy = "";
    config.output_dir = "./khun_poker";
    config.iterations = 1'000'000;

    KhunPokerGameTree tree;
    tree.init();

    // Allocate on heap to avoid stack overflow (DCFRTrainer is ~36MB)
    auto trainer = std::make_unique<DCFRTrainer>();
    trainer->setTree(&tree);
    trainer->train(
        config.seed,
        config.iterations,
        config.log_every_secs,
        config.checkpoint_every_secs,
        config.player0_policy,
        config.player1_policy,
        config.output_dir);

    printSmallBlindOpeningStrategy(*trainer);
    printBigBlindStrategy(*trainer);

    return 0;
}
