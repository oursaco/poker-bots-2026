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
        int size;
        int street;
        int info_set;
        int info_set_index;
        int winner;
        bool new_cards;
        Node(){}

        Node(int turn_, int pot_, int info_set_index_, int street_, bool new_cards_){
            turn = turn_;
            pot = pot_;
            info_set_index = info_set_index_;
            street = street_;
            new_cards = new_cards_;
        }
    };

    struct Leaf {
        int winner;
        int node_id;
        Leaf(){}
        Leaf(int winner_, int node_id_){
            winner = winner_;
            node_id = node_id_;
        }
    };

    ThreeCardBucket* bucket;
    array<Node, TREE_SZ> nodes;
    array<int, TREE_SZ> children;
    array<int, TREE_SZ> buckets;
    array<int, TREE_SZ> info_set_map;
    array<uint64_t, TREE_SZ> board;
    array<uint64_t, TREE_SZ> used_mask;

    array<int, POLICY_SZ> moves_per_info_set;
    int moves_per_info_set_index = 0;
    vector<Leaf> leaves;
    // first is node id, second is previous new card node id 
    vector<pair<int, int>> deal_flop;
    vector<pair<int, int>> deal_turn;
    vector<pair<int, int>> deal_river;
    vector<pair<int, int>> sb_discard;
    vector<pair<int, int>> bb_discard;

    omp::HandEvaluator hand_eval;
    int tree_index = 0;
    bool use_fixed_sb_hand = false;
    bool use_fixed_bb_hand = false;
    bool use_fixed_flop = false;
    bool use_fixed_turn = false;
    bool use_fixed_river = false;
    uint64_t fixed_sb_hand = 0;
    uint64_t fixed_bb_hand = 0;
    uint64_t fixed_flop = 0;
    uint64_t fixed_turn = 0;
    uint64_t fixed_river = 0;

    int generateTree(ThreeCardGameState* root, int& sb_info_set_index, int& bb_info_set_index, int prv_new_card, bool new_cards){
        if(root->turn == -1){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
            assert(actions.size() == 1);
            int node_id = generateTree(dynamic_cast<ThreeCardGameState*>(actions.front().first.get()), sb_info_set_index, bb_info_set_index, (new_cards ? node_id : prv_new_card), true);
            if(root->street == 1){
                deal_flop.emplace_back(node_id, prv_new_card);
            } else if(root->street == 5){
                deal_turn.emplace_back(node_id, prv_new_card);
            } else if(root->street == 6){
                deal_river.emplace_back(node_id, prv_new_card);
            }
            return node_id;
        }
        int node_id = tree_index;
        buckets[tree_index] = bucket->countBuckets(root);
        children[tree_index] = 0;
        int size = 0;
        nodes[tree_index] = Node(root->turn, root->pot, (root->turn == 0 ? sb_info_set_index : bb_info_set_index), root->street, new_cards);
        tree_index++;
        if(root->street == 2 || root->street == 3){
            int new_cards = 1;
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
            assert(actions.size() == 3);
            vector<int> child_infosets;
            for(auto& action : actions){
                ThreeCardGameState* next_state = dynamic_cast<ThreeCardGameState*>(action.first.get());
                int move_id = children[node_id];
                int child_id;
                if(root->street == 2){
                    bb_info_set_index++;
                    int sb_info_set_index_cpy = sb_info_set_index + 1;
                    child_id = generateTree(next_state, sb_info_set_index_cpy, bb_info_set_index, (new_cards ? node_id : prv_new_card), true);
                    child_infosets.push_back(sb_info_set_index_cpy);
                } else {
                    sb_info_set_index++;
                    int bb_info_set_index_cpy = bb_info_set_index + 1;
                    child_id = generateTree(next_state, sb_info_set_index, bb_info_set_index_cpy, (new_cards ? node_id : prv_new_card), true);
                    child_infosets.push_back(bb_info_set_index_cpy);
                }
                nodes[child_id].parent = node_id;
                nodes[child_id].move = move_id;
                children[node_id]++;
                if(root->street == 2){
                    bb_discard.emplace_back(child_id, node_id);
                } else {
                    sb_discard.emplace_back(child_id, node_id);
                }
                size += nodes[child_id].size + 1;
            }
            nodes[node_id].size = size;
            assert(child_infosets.size() == 3);
            assert(child_infosets[0] == child_infosets[1] && child_infosets[1] == child_infosets[2]);
            if(nodes[node_id].turn == 1){
                sb_info_set_index = child_infosets[0];
            } else {
                bb_info_set_index = child_infosets[0];
            }
            return node_id;
        }

        if(root->isTerminal()){
            leaves.push_back(Leaf(root->winner, node_id));
            return node_id;
        }
        vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
        for(auto& action : actions){
            ThreeCardGameState* next_state = dynamic_cast<ThreeCardGameState*>(action.first.get());
            int move_id = children[node_id];
            sb_info_set_index++;
            bb_info_set_index++;
            int child_id = generateTree(next_state, sb_info_set_index, bb_info_set_index, (new_cards ? node_id : prv_new_card), false);
            nodes[child_id].parent = node_id;
            nodes[child_id].move = move_id;
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
        int sb_info_set_index = 0;
        int bb_info_set_index = 0;
        generateTree(dynamic_cast<ThreeCardGameState*>(actions.front().first.get()), sb_info_set_index, bb_info_set_index, 0, false);

        int unique_indices = 0;
        vector<int> used_indices(sb_info_set_index + bb_info_set_index, -1);
        for(int i = 0; i < tree_index; i++){
            if(nodes[i].turn == 1) nodes[i].info_set_index += sb_info_set_index;
            if(used_indices[nodes[i].info_set_index] == -1) used_indices[nodes[i].info_set_index] = unique_indices++;
            nodes[i].info_set_index = used_indices[nodes[i].info_set_index];
        }
        int st = 0;
        int strategy_sz = 0;
        vector<int> info_set_children(unique_indices, -1);
        for(int i = 0; i < tree_index; i++){
            assert(info_set_children[nodes[i].info_set_index] == -1 || info_set_children[nodes[i].info_set_index] == children[i]);
            info_set_children[nodes[i].info_set_index] = children[i];
        }
        vector<int> info_set_buckets(unique_indices, -1);
        for(int i = 0; i < tree_index; i++){
            assert(info_set_buckets[nodes[i].info_set_index] == -1 || info_set_buckets[nodes[i].info_set_index] == buckets[i]);
            info_set_buckets[nodes[i].info_set_index] = buckets[i];
        }
        for(int i = 0; i < unique_indices; i++){
            info_set_map[i] = st;
            for(int j = 0; j < info_set_buckets[i]; j++){
                moves_per_info_set[moves_per_info_set_index++] = info_set_children[i];
                strategy_sz += info_set_children[i];
            }
            st += info_set_buckets[i];
        }
        cout << "total nodes: " << tree_index << endl;
        cout << "sb nodes: " << sb_info_set_index << endl;
        cout << "bb nodes: " << bb_info_set_index << endl;
        cout << "distinct nodes: " << unique_indices << endl;
        cout << "total info sets: " << st << endl;
        cout << "total strategy size: " << strategy_sz << endl;
        cout << "flop deals: " << deal_flop.size() << endl;
        cout << "turn deals: " << deal_turn.size() << endl;
        cout << "river deals: " << deal_river.size() << endl;
        cout << "bb discard: " << bb_discard.size() << endl;
        cout << "sb discard: " << sb_discard.size() << endl;
    }

    void setBucket(ThreeCardBucket* bucket_){
        bucket = bucket_;
    }

    void clearFixedCards(){
        use_fixed_sb_hand = false;
        use_fixed_bb_hand = false;
        use_fixed_flop = false;
        use_fixed_turn = false;
        use_fixed_river = false;
        fixed_sb_hand = 0;
        fixed_bb_hand = 0;
        fixed_flop = 0;
        fixed_turn = 0;
        fixed_river = 0;
    }

    void setFixedSbHand(uint64_t sb_hand){
        use_fixed_sb_hand = true;
        fixed_sb_hand = sb_hand;
    }

    void setFixedBbHand(uint64_t bb_hand){
        use_fixed_bb_hand = true;
        fixed_bb_hand = bb_hand;
    }

    void setFixedFlop(uint64_t flop){
        use_fixed_flop = true;
        fixed_flop = flop;
    }

    void setFixedTurn(uint64_t turn){
        use_fixed_turn = true;
        fixed_turn = turn;
    }

    void setFixedRiver(uint64_t river){
        use_fixed_river = true;
        fixed_river = river;
    }

    // prepares the game tree for an iteration of training
    void prepare(int seed){
        fill(board.begin(), board.end(), 0);
        fill(used_mask.begin(), used_mask.end(), 0);
        omp::XoroShiro128Plus rng(seed);
        omp::FastUniformIntDistribution2<int> card_generator(0, 51);
        uint64_t used_cards = 0;
        if(use_fixed_flop) used_cards |= fixed_flop;
        if(use_fixed_turn) used_cards |= fixed_turn;
        if(use_fixed_river) used_cards |= fixed_river;
        if(use_fixed_sb_hand) used_cards |= fixed_sb_hand;
        if(use_fixed_bb_hand) used_cards |= fixed_bb_hand;
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
        auto getDiscard = [&](uint64_t hand, int n){
            for(int i = 0; i < n; i++){
                hand ^= 1ull << __builtin_ctzll(hand);
            }
            return __builtin_ctzll(hand);
        };

        // generate initial hands
        uint64_t sb_hand = use_fixed_sb_hand ? fixed_sb_hand : 0;
        uint64_t bb_hand = use_fixed_bb_hand ? fixed_bb_hand : 0;
        for(int i = 0; i < 3; i++){
            if(!use_fixed_sb_hand){
                sb_hand |= 1ull << generateCard(used_cards);
            }
            if(!use_fixed_bb_hand){
                bb_hand |= 1ull << generateCard(used_cards);
            }
        }
        // fill in preflop buckets
        for(int i = 0; i < tree_index; i++){
            if(nodes[i].street > 0){
                i += nodes[i].size;
                continue;
            }
            assert(bucket->getPreflopBucket(sb_hand) < buckets[i]);
            if(nodes[i].turn == 0){
                nodes[i].info_set = info_set_map[nodes[i].info_set_index] + bucket->getPreflopBucket(sb_hand);
            } else if(nodes[i].turn == 1){
                nodes[i].info_set = info_set_map[nodes[i].info_set_index] + bucket->getPreflopBucket(bb_hand);
            } else {
                assert(false);
            }
        }
        // fill in flop buckets
        for(auto [node_id, prv_new_card] : deal_flop){
            if(use_fixed_flop){
                board[node_id] = fixed_flop;
            } else {
                used_mask[node_id] = used_cards;
                board[node_id] |= 1ull << generateCard(used_mask[node_id]);
                board[node_id] |= 1ull << generateCard(used_mask[node_id]);
            }
            assert(prv_new_card == 0);
            assert(__builtin_popcountll(board[node_id]) == 2);
            int bb_bucket = bucket->getBBDiscardBucket(board[node_id], bb_hand);
            assert(bb_bucket < buckets[node_id]);
            nodes[node_id].info_set = info_set_map[nodes[node_id].info_set_index] + bb_bucket;
            int l = node_id, r = node_id + nodes[node_id].size;
            assert(nodes[l].turn == 1);
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].new_cards){
                    i += nodes[i].size;
                    continue;
                }
                assert(bb_bucket < buckets[i]);
                assert(nodes[i].turn == 1);
                nodes[i].info_set = info_set_map[nodes[i].info_set_index] + bb_bucket;
            }
        }
        // big blind discard
        for(auto [node_id, prv_new_card] : bb_discard){
            assert(__builtin_popcountll(board[prv_new_card]) == 2);
            board[node_id] = board[prv_new_card] | 1ull << getDiscard(bb_hand, nodes[node_id].move);
            used_mask[node_id] = used_mask[prv_new_card];
            int sb_bucket = bucket->getSBDiscardBucket(board[node_id], sb_hand);
            assert(sb_bucket < buckets[node_id]);
            assert(nodes[node_id].turn == 0);
            nodes[node_id].info_set = info_set_map[nodes[node_id].info_set_index] + sb_bucket;
            int l = node_id, r = node_id + nodes[node_id].size;
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].new_cards){
                    i += nodes[i].size;
                    continue;
                }
                assert(nodes[i].turn == 0);
                assert(sb_bucket < buckets[i]);
                nodes[i].info_set = info_set_map[nodes[i].info_set_index] + sb_bucket;
            }
        }
         // small blind discard
        for(auto [node_id, prv_new_card] : sb_discard){
            assert(__builtin_popcountll(board[prv_new_card]) == 3);
            board[node_id] = board[prv_new_card] | 1ull << getDiscard(sb_hand, nodes[node_id].move);
            used_mask[node_id] = used_mask[prv_new_card];
            int bb_bucket = bucket->getFlopBucket(board[node_id], bb_hand);
            int sb_bucket = bucket->getFlopBucket(board[node_id], sb_hand);
            assert(bb_bucket < buckets[node_id]);
            assert(nodes[node_id].street == 4);
            assert(nodes[node_id].turn == 1);
            nodes[node_id].info_set = info_set_map[nodes[node_id].info_set_index] + bb_bucket;
            int l = node_id, r = node_id + nodes[node_id].size;
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].new_cards){
                    i += nodes[i].size;
                    continue;
                }
                assert((nodes[i].turn == 0 && sb_bucket < buckets[i]) || (nodes[i].turn == 1 && bb_bucket < buckets[i]));
                nodes[i].info_set = info_set_map[nodes[i].info_set_index] + (nodes[i].turn == 0 ? sb_bucket : bb_bucket);
            }
        }
        // fill in turn buckets
        for(auto [node_id, prv_new_card] : deal_turn){
            assert(__builtin_popcountll(board[prv_new_card]) == 4);
            board[node_id] = board[prv_new_card];
            used_mask[node_id] = used_mask[prv_new_card];
            if(use_fixed_turn) board[node_id] |= fixed_turn;
            else board[node_id] |= 1ull << generateCard(used_mask[node_id]);
            assert(__builtin_popcountll(board[node_id]) == 5);
            int sb_bucket = bucket->getTurnBucket(board[node_id], sb_hand);
            int bb_bucket = bucket->getTurnBucket(board[node_id], bb_hand);
            assert(sb_bucket < buckets[node_id] && bb_bucket < buckets[node_id]);
            nodes[node_id].info_set = info_set_map[nodes[node_id].info_set_index] + (nodes[node_id].turn == 0 ? sb_bucket : bb_bucket);
            int l = node_id, r = node_id + nodes[node_id].size;
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].new_cards){
                    i += nodes[i].size;
                    continue;
                }
                assert((nodes[i].turn == 0 && sb_bucket < buckets[i]) || (nodes[i].turn == 1 && bb_bucket < buckets[i]));
                nodes[i].info_set = info_set_map[nodes[i].info_set_index] + (nodes[i].turn == 0 ? sb_bucket : bb_bucket);
            }
        }
        // deal river
        for(auto [node_id, prv_new_card] : deal_river){
            assert(__builtin_popcountll(board[prv_new_card]) == 5);
            board[node_id] = board[prv_new_card];
            used_mask[node_id] = used_mask[prv_new_card];
            if(use_fixed_river) board[node_id] |= fixed_river;
            else board[node_id] |= 1ull << generateCard(used_mask[node_id]);
            assert(__builtin_popcountll(board[node_id]) == 6);
            int sb_bucket = bucket->getRiverBucket(board[node_id], sb_hand);
            int bb_bucket = bucket->getRiverBucket(board[node_id], bb_hand);
            assert(sb_bucket < buckets[node_id] && bb_bucket < buckets[node_id]);
            int winner = bucket->getWinner(board[node_id], sb_hand, bb_hand);
            nodes[node_id].info_set = info_set_map[nodes[node_id].info_set_index] + (nodes[node_id].turn == 0 ? sb_bucket : bb_bucket);
            nodes[node_id].winner = winner;
            int l = node_id, r = node_id + nodes[node_id].size;
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].new_cards){
                    i += nodes[i].size;
                    continue;
                }
                nodes[i].info_set = info_set_map[nodes[i].info_set_index] + (nodes[i].turn == 0 ? sb_bucket : bb_bucket);
                nodes[i].winner = winner;
            }
        }
    }

    // updates the leaf utility values for each trainer
    void updateUtility(array<float, TRAINER_SZ> &utility){
        for(int i = 0; i < leaves.size(); i++){
            int node_id = leaves[i].node_id;
            int w = leaves[i].winner;
            if(w == 0) w = nodes[node_id].winner;
            utility[node_id] = w*(nodes[node_id].pot/2);
        }
    }

    void fillMovesPerInfoSet(array<int, POLICY_SZ> &moves_per_info_set_){
        for(int i = 0; i < moves_per_info_set_index; i++){
            moves_per_info_set_[i] = moves_per_info_set[i];
        }
    }

    int infoSetCount(){
        return moves_per_info_set_index;
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

    // calculates the info set of a node
    int calcInfoSet(int node_id, uint64_t hand, uint64_t board){
        if(nodes[node_id].street == 0){
            return info_set_map[nodes[node_id].info_set_index] + bucket->getPreflopBucket(hand);
        } else if(nodes[node_id].street == 2){
            return info_set_map[nodes[node_id].info_set_index] + bucket->getBBDiscardBucket(board, hand);
        } else if(nodes[node_id].street == 3){
            return info_set_map[nodes[node_id].info_set_index] + bucket->getSBDiscardBucket(board, hand);
        } else if(nodes[node_id].street == 4){
            return info_set_map[nodes[node_id].info_set_index] + bucket->getFlopBucket(board, hand);
        } else if(nodes[node_id].street == 5){
            return info_set_map[nodes[node_id].info_set_index] + bucket->getTurnBucket(board, hand);
        } else if(nodes[node_id].street == 6){
            return info_set_map[nodes[node_id].info_set_index] + bucket->getRiverBucket(board, hand);
        } else {
            assert(false);
        }
    }

    // returns who's turn it is to move
    int getTurn(int node_id){
        return nodes[node_id].turn;
    }

    string cardToString(unsigned card){
        static const char* ranks = "23456789TJQKA";
        static const char suits[] = {'s', 'h', 'd', 'c'};
        string out;
        out += ranks[card/4];
        out += suits[card%4];
        return out;
    }

    string maskToString(uint64_t mask){
        string out;
        bool first = true;
        while(mask){
            unsigned card = __builtin_ctzll(mask);
            mask &= mask - 1;
            if(!first){
                out += " ";
            }
            out += cardToString(card);
            first = false;
        }
        if(first){
            return "(none)";
        }
        return out;
    }
};

#endif // THREECARD_HPP
