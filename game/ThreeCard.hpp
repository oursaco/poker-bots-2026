#ifndef THREECARD_HPP
#define THREECARD_HPP

#include "GameTree.hpp"
#include "external/omp/Random.h"
#include "external/omp/HandEvaluator.h"
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <cassert>
#include <iostream>

#include "constants/constants.h"
#include <algorithm>
#include <map>
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCardState.hpp"
using namespace std;

struct ThreeCardGameTree : GameTree {

    struct Node {
        int parent;
        int move;
        int turn;
        int pot;
        int new_cards;
        int size;
        int prv_new_card;
        int info_set;
        int winner;
        int info_set_map;
    };

    ThreeCardBucket* bucket;
    array<Node, TREE_SZ> nodes;
    vector<int> winner;
    vector<int> new_card_states;
    array<int, TREE_SZ> children;
    array<int, TREE_SZ> buckets;
    vector<int> moves_per_info_set;
    vector<int> preflop;
    vector<int> leaves;
    vector<uint64_t> board;
    vector<uint64_t> used_mask;
    vector<int> parent_board;
    omp::HandEvaluator hand_eval;
    int tree_index = 0;

    int generateTree(ThreeCardGameState* root, int par = -1, int par_move = -1, int prv_new_card = -1, int new_cards = 0){
        if(root->turn == -1){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
            assert(actions.size() == 1);
            if(root->street == 1){
                new_cards = 2;
            } else if(root->street == 2){
                new_cards = 1;
            } else if(root->street == 3){
                new_cards = 1;
            }
            int node_id = generateTree(dynamic_cast<ThreeCardGameState*>(actions.front().first.get()), par, par_move, prv_new_card, new_cards);
            new_card_states.push_back(node_id);
            return node_id;
        }
        int node_id = tree_index;
        buckets[tree_index] = bucket->countBuckets(root);
        if(root->street == 0){
            preflop.push_back(node_id);
        }
        children[tree_index] = 0;
        int size = 0;
        nodes[tree_index] = {par, par_move, root->turn, root->pot, new_cards, 0, prv_new_card};
        tree_index++;
        if(new_cards > 0) prv_new_card = node_id;
        if(root->isTerminal()){
            leaves.push_back(node_id);
            winner.push_back(root->winner);
            return node_id;
        }
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
        for(auto& action : actions){
            ThreeCardGameState* next_state = dynamic_cast<ThreeCardGameState*>(action.first.get());
            int move_id = static_cast<int>(children[node_id]);
            int child_id = generateTree(next_state, node_id, move_id, prv_new_card);
            children[node_id]++;
            size += nodes[child_id].size + 1;
        }
        nodes[node_id].size = size;
        return node_id;
    }

    // initializes the game tree
    void init(){
        assert(tree_index == 0);
        ThreeCardGameState root;
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root.generateActions();
        assert(actions.size() == 1);
        generateTree(dynamic_cast<ThreeCardGameState*>(actions.front().first.get()));
        sort(new_card_states.begin(), new_card_states.end());
        board.resize(new_card_states.size());
        used_mask.resize(new_card_states.size(), 0);
        parent_board.resize(new_card_states.size(), 0);
        map<int, int> loc;
        parent_board[0] = -1;
        loc[new_card_states[0]] = 0;
        for(int i = 1; i < new_card_states.size(); i++){
            loc[new_card_states[i]] = i;
            parent_board[i] = loc[nodes[new_card_states[i]].prv_new_card];
        }
        int st = 0;
        int strategy_sz = 0;
        for(int i = 0; i < tree_index; i++){
            nodes[i].info_set_map = st;
            for(int j = 0; j < buckets[i]; j++){
                moves_per_info_set.push_back(children[i]);
                strategy_sz += children[i];
            }
            st += buckets[i];
        }
        cout << "total info sets: " << st << endl;
        cout << "total strategy size: " << strategy_sz << endl;
    }

    void setBucket(ThreeCardBucket* bucket_){
        bucket = bucket_;
    }

    // prepares the game tree for an iteration of training
    void prepare(int seed){
        omp::XoroShiro128Plus rng(seed);
        omp::FastUniformIntDistribution2<int> card_generator(0, 51);
        uint64_t initial_mask = 0;
        auto generateCard = [&](uint64_t &used_mask){
            unsigned card;
            uint64_t card_mask;
            do {
                card = card_generator(rng);
                card_mask = 1ull << card;
            } while (used_mask & card_mask);
            used_mask |= card_mask;
            return card;
        };
        auto chooseCard = [&](uint64_t hand){
            int num = 0;
            do {
                num = card_generator(rng);
            } while(num == 51);
            for(int i = 0; i < num%3; i++){
                hand ^= 1ull << __builtin_ctzll(hand);
            }
            return __builtin_ctzll(hand);
        };
        uint64_t sb_hand = 0;
        uint64_t bb_hand = 0;
        uint64_t sb_extra_card = 0;
        uint64_t bb_extra_card = 0;
        for(int i = 0; i < 3; i++){
            sb_hand |= 1ull << generateCard(initial_mask);
            bb_hand |= 1ull << generateCard(initial_mask);
        }
        for(int i : preflop){
            assert(bucket->getPreflopBucket(sb_hand) < buckets[i]);
            if(nodes[i].turn == 0){
                nodes[i].info_set = nodes[i].info_set_map + bucket->getPreflopBucket(sb_hand);
            } else if(nodes[i].turn == 1){
                nodes[i].info_set = nodes[i].info_set_map + bucket->getPreflopBucket(bb_hand);
            } else {
                assert(false);
            }
        }
        for(int i = 0; i < new_card_states.size(); i++){
            int l = new_card_states[i];
            int r = l + nodes[l].size;
            if(nodes[l].prv_new_card == -1){
                board[i] = 0;
                used_mask[i] = initial_mask;
            } else {
                int p = parent_board[i];
                board[i] = board[p];
                used_mask[i] = used_mask[p];
            }
            for(int j = 0; j < nodes[l].new_cards; j++){
                board[i] |= 1ull << generateCard(used_mask[i]);
            }
            // postflop state
            int sb_bucket, bb_bucket;
            if(__builtin_popcountll(board[i]) == 2){
                sb_extra_card = 1ull << chooseCard(bb_hand);
                bb_extra_card = 1ull << chooseCard(sb_hand);
                sb_bucket = bucket->getFlopBucket(board[i], sb_hand);
                bb_bucket = bucket->getFlopBucket(board[i], bb_hand);
            } else if(__builtin_popcountll(board[i]) == 3){
                sb_bucket = bucket->getTurnBucket(board[i] | sb_extra_card, sb_hand);
                bb_bucket = bucket->getTurnBucket(board[i] | bb_extra_card, bb_hand);
            } else if(__builtin_popcountll(board[i]) == 4){
                sb_bucket = bucket->getRiverBucket(board[i] | sb_extra_card, sb_hand);
                bb_bucket = bucket->getRiverBucket(board[i] | bb_extra_card, bb_hand);
            }
            if(__builtin_popcountll(board[i]) == 4){
                int winner = bucket->evalStrength(board[i] | sb_extra_card, sb_hand) - bucket->evalStrength(board[i] | bb_extra_card, bb_hand);
                if(winner > 0) winner = 1;
                else if(winner < 0) winner = -1;
                else winner = 0;
                if(nodes[l].turn == 0) {
                    nodes[l].info_set = nodes[l].info_set_map + sb_bucket;
                } else {
                    nodes[l].info_set = nodes[l].info_set_map + bb_bucket;
                }
                for(unsigned j = l + 1; j <= r; j++){
                    nodes[j].winner = winner;
                    if(nodes[j].turn == 0) {
                        nodes[j].info_set = nodes[j].info_set_map + sb_bucket;
                    } else {
                        nodes[j].info_set = nodes[j].info_set_map + bb_bucket;
                    }
                }
            } else {
                if(nodes[l].turn == 0) {
                    nodes[l].info_set = nodes[l].info_set_map + sb_bucket;
                } else {
                    nodes[l].info_set = nodes[l].info_set_map + bb_bucket;
                }
                for(unsigned j = l + 1; j <= r; j++){
                    if(nodes[j].new_cards > 0){
                        j += nodes[j].size;
                    } else {
                        if(nodes[j].turn == 0) {
                            nodes[j].info_set = nodes[j].info_set_map + sb_bucket;
                        } else {
                            nodes[j].info_set = nodes[j].info_set_map + bb_bucket;
                        }
                    }
                }
            }
        }
    }

    // updates the leaf utility values for each trainer
    void updateUtility(array<float, TRAINER_SZ> &utility){
        for(int i = 0; i < leaves.size(); i++){
            int node_id = leaves[i];
            int w = winner[i];
            if(w == 0){
                w = nodes[node_id].winner;
            }
            utility[node_id] = w*(nodes[node_id].pot/2);
        }
    }

    vector<int> getMovesPerInfoSet(){
        return moves_per_info_set;
    }

    // returns the number of nodes
    int nodeCount(){
        return tree_index;
    }

    // returns parent node id
    int getParentId(int node_id){
        return nodes[node_id].parent;
    }

    // returns the move used to reach node
    int getMove(int node_id){
        return nodes[node_id].move;
    }

    // returns info set of node
    int getInfoSet(int node_id){
        return nodes[node_id].info_set;
    }

    // returns who's turn it is to move
    int getTurn(int node_id){
        return nodes[node_id].turn;
    }
};

#endif // THREECARD_HPP