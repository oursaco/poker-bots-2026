#include "game/GameTree.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardState.hpp"
#include "game/ThreeCardBucket.hpp"
#include <cstddef>
#include <iostream>
#include <string>
using namespace std;

struct SingleBucketThreeCard : ThreeCardBucket {
    int countBuckets(ThreeCardGameState* state){
        if(state->street == 0) return 1755;
        return 1;
    }

    int getPreflopBucket(uint64_t hand){
        return 0;
    }

    int getFlopBucket(uint64_t board, uint64_t hand){
        return 0;
    }

    int getTurnBucket(uint64_t board, uint64_t hand){
        return 0;
    }

    int getRiverBucket(uint64_t board, uint64_t hand){
        return 0;
    }

    int getWinner(uint64_t board, uint64_t hand1, uint64_t hand2){
        return 0;
    }

    int getBBDiscardBucket(uint64_t board, uint64_t hand){
        return 0;
    }

    int getSBDiscardBucket(uint64_t board, uint64_t hand){
        return 0;
    }
};

int node_id_counter = 0;
int sz = 0;

void visualizeInfoSets(GameState* state, GameTree* tree, const string& prefix, int node_id){
    sz++;
    auto actions = state->generateActions();
    if(actions.empty()){
        cout << prefix << "(no actions)\n";
        return;
    }

    for(int i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();
        int child_node_id = node_id;

        if(!action->isWorldAction()){
            node_id_counter++;
            child_node_id = node_id_counter;
        }

        cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString();
        if(!action->isWorldAction()){
            cout << " | infoset: " << tree->getInfoSet(node_id);
        }
        if(is_terminal){
            cout << " [terminal, winner: " << next_state->getWinner() << "]";
        }
        cout << "\n";

        if(!is_terminal){
            visualizeInfoSets(next_state, tree, prefix + (is_last ? "    " : "|   "), child_node_id);
        }
    }
}

void visualizeInfoSetsDepthLimited(GameState* state, GameTree* tree, const string& prefix, int node_id, int depth){
    auto actions = state->generateActions();
    if(actions.empty()){
        cout << prefix << "(no actions)\n";
        return;
    }
    if(depth == 0){
        cout << prefix << "(depth limited)\n";
    }

    for(int i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();
        int child_node_id = node_id;

        if(!action->isWorldAction()){
            node_id_counter++;
            child_node_id = node_id_counter;
        }

        if(depth > 0){
            cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString();
            if(!action->isWorldAction()){
                cout << " | infoset: " << tree->getInfoSet(node_id);
                // cout << " | infoset: " << dynamic_cast<ThreeCardGameTree*>(tree)->nodes[node_id].info_set_index;
            }
            if(is_terminal){
                cout << " [terminal, winner: " << next_state->getWinner() << "]";
            }
            cout << "\n";
        }

        if(!is_terminal){
            visualizeInfoSetsDepthLimited(next_state, tree, prefix + (is_last ? "    " : "|   "), child_node_id, depth - 1);
        }
    }
}

static DynamicThreeCardBucket bucket;

void visualizeThreeCardInfoSets(){
    ThreeCardGameTree tree;
    // SingleBucketThreeCard bucket;
    bucket.init("./emd_bucket_data");
    tree.setBucket(&bucket);
    tree.init();
    tree.prepare(42);

    ThreeCardGameState root = ThreeCardGameState();
    node_id_counter = 0;
    visualizeInfoSetsDepthLimited(&root, &tree, "", 0, 5);
}

int main(){
    visualizeThreeCardInfoSets();
}
