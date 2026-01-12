#ifndef THREECARDINFERENCE_HPP
#define THREECARDINFERENCE_HPP

#include "GameTree.hpp"
#include "external/omp/Random.h"
#include "external/omp/HandEvaluator.h"
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <set>

#include "constants/constants.h"
#include <algorithm>
#include <map>
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCardState.hpp"
using namespace std;

struct ThreeCardInferenceTree {

    struct Node {
        // parent: the node id of the parent node < 0...1e6 (stored with +1 bias to allow -1)
        // move: the move used to reach this node < 0...10
        // turn: 0...1
        // pot: 0...800
        // size: 0...1e6
        // street: 0...6
        // info_set: 2^32
        // info_set_index: 2^32
        // winner: -1, 0, 1
        // new_cards: true if the node has new cards
        Node() = default;

        Node(int turn_, int pot_, int info_set_index_, int street_, bool new_cards_){
            setTurn(turn_);
            setPot(pot_);
            setInfoSetIndex(info_set_index_);
            setStreet(street_);
            setNewCards(new_cards_);
            setWinner(0);
        }

        inline int getParent() const{
            return static_cast<int>((packed0 >> kParentShift) & kParentMask) - 1;
        }

        inline void setParent(int parent_){
            assert(parent_ >= -1 && parent_ <= 1000000);
            uint64_t value = static_cast<uint64_t>(parent_ + 1);
            packed0 = (packed0 & ~(kParentMask << kParentShift)) | (value << kParentShift);
        }

        inline int getMove() const{
            return static_cast<int>((packed0 >> kMoveShift) & kMoveMask);
        }

        inline void setMove(int move_){
            assert(move_ >= 0 && move_ < (1 << kMoveBits));
            packed0 = (packed0 & ~(kMoveMask << kMoveShift)) | (static_cast<uint64_t>(move_) << kMoveShift);
        }

        inline int getTurn() const{
            return static_cast<int>((packed0 >> kTurnShift) & kTurnMask);
        }

        inline void setTurn(int turn_){
            assert(turn_ == 0 || turn_ == 1);
            packed0 = (packed0 & ~(kTurnMask << kTurnShift)) | (static_cast<uint64_t>(turn_) << kTurnShift);
        }

        inline int getPot() const{
            return static_cast<int>((packed0 >> kPotShift) & kPotMask);
        }

        inline void setPot(int pot_){
            assert(pot_ >= 0 && pot_ <= 800);
            packed0 = (packed0 & ~(kPotMask << kPotShift)) | (static_cast<uint64_t>(pot_) << kPotShift);
        }

        inline int getSize() const{
            return static_cast<int>((packed0 >> kSizeShift) & kSizeMask);
        }

        inline void setSize(int size_){
            assert(size_ >= 0 && size_ <= 1000000);
            packed0 = (packed0 & ~(kSizeMask << kSizeShift)) | (static_cast<uint64_t>(size_) << kSizeShift);
        }

        inline int getStreet() const{
            return static_cast<int>((packed0 >> kStreetShift) & kStreetMask);
        }

        inline void setStreet(int street_){
            assert(street_ >= 0 && street_ <= 6);
            packed0 = (packed0 & ~(kStreetMask << kStreetShift)) | (static_cast<uint64_t>(street_) << kStreetShift);
        }

        inline int getWinner() const{
            uint64_t raw = (packed0 >> kWinnerShift) & kWinnerMask;
            if(raw & (1ull << (kWinnerBits - 1))){
                return static_cast<int>(raw | ~kWinnerMask);
            }
            return static_cast<int>(raw);
        }

        inline void setWinner(int winner_){
            assert(winner_ >= -1 && winner_ <= 1);
            uint64_t raw = static_cast<uint64_t>(winner_) & kWinnerMask;
            packed0 = (packed0 & ~(kWinnerMask << kWinnerShift)) | (raw << kWinnerShift);
        }

        inline bool hasNewCards() const{
            return ((packed0 >> kNewCardsShift) & kNewCardsMask) != 0;
        }

        inline void setNewCards(bool new_cards_){
            uint64_t value = new_cards_ ? 1ull : 0ull;
            packed0 = (packed0 & ~(kNewCardsMask << kNewCardsShift)) | (value << kNewCardsShift);
        }

        inline int getInfoSet() const{
            return static_cast<int>(packed1 & kInfoSetMask);
        }

        inline void setInfoSet(int info_set_){
            assert(info_set_ >= 0);
            uint64_t value = static_cast<uint64_t>(static_cast<uint32_t>(info_set_));
            packed1 = (packed1 & (kInfoSetMask << kInfoSetIndexShift)) | value;
        }

        inline int getInfoSetIndex() const{
            return static_cast<int>((packed1 >> kInfoSetIndexShift) & kInfoSetMask);
        }

        inline void setInfoSetIndex(int info_set_index_){
            assert(info_set_index_ >= 0);
            uint64_t value = static_cast<uint64_t>(static_cast<uint32_t>(info_set_index_));
            packed1 = (packed1 & kInfoSetMask) | (value << kInfoSetIndexShift);
        }

    private:
        // Packed layout:
        // packed0: parent(20) | move(4) | turn(1) | pot(10) | size(20) | street(3) | winner(2) | new_cards(1)
        // packed1: info_set(32) | info_set_index(32)
        uint64_t packed0 = 0;
        uint64_t packed1 = 0;

        static constexpr int kParentBits = 20;
        static constexpr int kMoveBits = 4;
        static constexpr int kTurnBits = 1;
        static constexpr int kPotBits = 10;
        static constexpr int kSizeBits = 20;
        static constexpr int kStreetBits = 3;
        static constexpr int kWinnerBits = 2;
        static constexpr int kNewCardsBits = 1;

        static constexpr int kParentShift = 0;
        static constexpr int kMoveShift = kParentShift + kParentBits;
        static constexpr int kTurnShift = kMoveShift + kMoveBits;
        static constexpr int kPotShift = kTurnShift + kTurnBits;
        static constexpr int kSizeShift = kPotShift + kPotBits;
        static constexpr int kStreetShift = kSizeShift + kSizeBits;
        static constexpr int kWinnerShift = kStreetShift + kStreetBits;
        static constexpr int kNewCardsShift = kWinnerShift + kWinnerBits;

        static constexpr uint64_t kParentMask = (1ull << kParentBits) - 1ull;
        static constexpr uint64_t kMoveMask = (1ull << kMoveBits) - 1ull;
        static constexpr uint64_t kTurnMask = (1ull << kTurnBits) - 1ull;
        static constexpr uint64_t kPotMask = (1ull << kPotBits) - 1ull;
        static constexpr uint64_t kSizeMask = (1ull << kSizeBits) - 1ull;
        static constexpr uint64_t kStreetMask = (1ull << kStreetBits) - 1ull;
        static constexpr uint64_t kWinnerMask = (1ull << kWinnerBits) - 1ull;
        static constexpr uint64_t kNewCardsMask = (1ull << kNewCardsBits) - 1ull;

        static constexpr uint64_t kInfoSetMask = 0xFFFFFFFFull;
        static constexpr int kInfoSetIndexShift = 32;
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
    array<int, TREE_SZ> sb_discard_card;
    array<int, TREE_SZ> bb_discard_card;
    array<int, POLICY_SZ> moves_per_info_set;
    int moves_per_info_set_index = 0;
    vector<Leaf> leaves;
    // first is node id, second is previous new card node id 
    vector<pair<int, int>> deal_flop;
    vector<pair<int, int>> deal_turn;
    vector<pair<int, int>> deal_river;
    vector<pair<int, int>> sb_discard;
    vector<pair<int, int>> bb_discard;

    int tree_index = 0;
    array<vector<int>, 100> nodes_per_depth;

    int generateTree(ThreeCardGameState* root, int& sb_info_set_index, int& bb_info_set_index, int prv_new_card, bool new_cards, int depth = 0){
        if(root->turn == -1){
            vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> actions = root->generateActions();
            assert(actions.size() == 1);
            int node_id = generateTree(dynamic_cast<ThreeCardGameState*>(actions.front().first.get()), sb_info_set_index, bb_info_set_index, (new_cards ? node_id : prv_new_card), true, depth);
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
        nodes_per_depth[depth].push_back(node_id);
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
                    child_id = generateTree(next_state, sb_info_set_index_cpy, bb_info_set_index, (new_cards ? node_id : prv_new_card), true, depth + 1);
                    child_infosets.push_back(sb_info_set_index_cpy);
                } else {
                    sb_info_set_index++;
                    int bb_info_set_index_cpy = bb_info_set_index + 1;
                    child_id = generateTree(next_state, sb_info_set_index, bb_info_set_index_cpy, (new_cards ? node_id : prv_new_card), true, depth + 1);
                    child_infosets.push_back(bb_info_set_index_cpy);
                }
                nodes[child_id].setParent(node_id);
                nodes[child_id].setMove(move_id);
                children[node_id]++;
                if(root->street == 2){
                    bb_discard.emplace_back(child_id, node_id);
                } else {
                    sb_discard.emplace_back(child_id, node_id);
                }
                size += nodes[child_id].getSize() + 1;
            }
            nodes[node_id].setSize(size);
            assert(child_infosets.size() == 3);
            assert(child_infosets[0] == child_infosets[1] && child_infosets[1] == child_infosets[2]);
            if(nodes[node_id].getTurn() == 1){
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
            int child_id = generateTree(next_state, sb_info_set_index, bb_info_set_index, (new_cards ? node_id : prv_new_card), false, depth + 1);
            nodes[child_id].setParent(node_id);
            nodes[child_id].setMove(move_id);
            children[node_id]++;
            size += nodes[child_id].getSize() + 1;
        }
        nodes[node_id].setSize(size);
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
            int info_set_index = nodes[i].getInfoSetIndex();
            if(nodes[i].getTurn() == 1) info_set_index += sb_info_set_index;
            if(used_indices[info_set_index] == -1) used_indices[info_set_index] = unique_indices++;
            nodes[i].setInfoSetIndex(used_indices[info_set_index]);
        }
        int st = 0;
        int strategy_sz = 0;
        vector<int> info_set_children(unique_indices, -1);
        for(int i = 0; i < tree_index; i++){
            int info_set_index = nodes[i].getInfoSetIndex();
            assert(info_set_children[info_set_index] == -1 || info_set_children[info_set_index] == children[i]);
            info_set_children[info_set_index] = children[i];
        }
        vector<int> info_set_buckets(unique_indices, -1);
        for(int i = 0; i < tree_index; i++){
            int info_set_index = nodes[i].getInfoSetIndex();
            assert(info_set_buckets[info_set_index] == -1 || info_set_buckets[info_set_index] == buckets[i]);
            info_set_buckets[info_set_index] = buckets[i];
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

    // prepares the game tree for an iteration of training
    void prepare(int seed){
        #pragma omp parallel for schedule(static)
        for(int i = 0; i < tree_index; i++){
            board[i] = used_mask[i] = 0;
        }
        omp::XoroShiro128Plus rng(seed);
        omp::FastUniformIntDistribution2<int> card_generator(0, 51);
        uint64_t used_cards = 0;
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
        uint64_t sb_hand = 0;
        uint64_t bb_hand = 0;
        for(int i = 0; i < 3; i++){
            sb_hand |= 1ull << generateCard(used_cards);
            bb_hand |= 1ull << generateCard(used_cards);
        }
        // fill in preflop buckets
        // no need to parallelize since preflop buckets are the same for all nodes
        int preflop_sb_bucket = bucket->getPreflopBucket(sb_hand);
        int preflop_bb_bucket = bucket->getPreflopBucket(bb_hand);
        for(int i = 0; i < tree_index; i++){
            if(nodes[i].getStreet() > 0){
                i += nodes[i].getSize();
                continue;
            }
            int info_set_index = nodes[i].getInfoSetIndex();
            int turn = nodes[i].getTurn();
            if(turn == 0){
                nodes[i].setInfoSet(info_set_map[info_set_index] + preflop_sb_bucket);
            } else if(turn == 1){
                nodes[i].setInfoSet(info_set_map[info_set_index] + preflop_bb_bucket);
            } else {
                assert(false);
            }
        }
        // fill in flop buckets
        for(auto [node_id, prv_new_card] : deal_flop){
            used_mask[node_id] = used_cards;
            board[node_id] |= 1ull << generateCard(used_mask[node_id]);
            board[node_id] |= 1ull << generateCard(used_mask[node_id]);
        }
        #pragma omp parallel for
        for(int t = 0; t < deal_flop.size(); t++){
            int node_id = deal_flop[t].first;
            int prv_new_card = deal_flop[t].second;
            int bb_bucket = bucket->getBBDiscardBucket(board[node_id], bb_hand);
            nodes[node_id].setInfoSet(info_set_map[nodes[node_id].getInfoSetIndex()] + bb_bucket);
            int l = node_id, r = node_id + nodes[node_id].getSize();
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].hasNewCards()){
                    i += nodes[i].getSize();
                    continue;
                }
                nodes[i].setInfoSet(info_set_map[nodes[i].getInfoSetIndex()] + bb_bucket);
            }
        }
        // big blind discard
        // no need to parallelize since sb discards right after
        for(auto [node_id, prv_new_card] : bb_discard){
            bb_discard_card[node_id] = getDiscard(bb_hand, nodes[node_id].getMove());
            board[node_id] = board[prv_new_card] | 1ull << bb_discard_card[node_id];
            used_mask[node_id] = used_mask[prv_new_card];
            int sb_bucket = bucket->getSBDiscardBucket(board[node_id], sb_hand, bb_discard_card[node_id]);
            nodes[node_id].setInfoSet(info_set_map[nodes[node_id].getInfoSetIndex()] + sb_bucket);
            int l = node_id, r = node_id + nodes[node_id].getSize();
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].hasNewCards()){
                    i += nodes[i].getSize();
                    continue;
                }
                nodes[i].setInfoSet(info_set_map[nodes[i].getInfoSetIndex()] + sb_bucket);
            }
        }
         // small blind discard
        for(auto [node_id, prv_new_card] : sb_discard){
            sb_discard_card[node_id] = getDiscard(sb_hand, nodes[node_id].getMove());
            bb_discard_card[node_id] = bb_discard_card[prv_new_card];
            board[node_id] = board[prv_new_card] | 1ull << sb_discard_card[node_id];
            used_mask[node_id] = used_mask[prv_new_card];
        }
        #pragma omp parallel for
        for(int t = 0; t < sb_discard.size(); t++){
            int node_id = sb_discard[t].first;
            int prv_new_card = sb_discard[t].second;
            int bb_bucket = bucket->getFlopBucket(board[node_id], bb_hand, sb_discard_card[node_id]);
            int sb_bucket = bucket->getFlopBucket(board[node_id], sb_hand, bb_discard_card[node_id]);
            nodes[node_id].setInfoSet(info_set_map[nodes[node_id].getInfoSetIndex()] + bb_bucket);
            int l = node_id, r = node_id + nodes[node_id].getSize();
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].hasNewCards()){
                    i += nodes[i].getSize();
                    continue;
                }
                int turn = nodes[i].getTurn();
                nodes[i].setInfoSet(info_set_map[nodes[i].getInfoSetIndex()] + (turn == 0 ? sb_bucket : bb_bucket));
            }
        }
        // fill in turn buckets
        for(auto [node_id, prv_new_card] : deal_turn){
            sb_discard_card[node_id] = sb_discard_card[prv_new_card];
            bb_discard_card[node_id] = bb_discard_card[prv_new_card];
            board[node_id] = board[prv_new_card];
            used_mask[node_id] = used_mask[prv_new_card];
            board[node_id] |= 1ull << generateCard(used_mask[node_id]);
        }
        #pragma omp parallel for
        for(int t = 0; t < deal_turn.size(); t++){
            int node_id = deal_turn[t].first;
            int prv_new_card = deal_turn[t].second;
            int sb_bucket = bucket->getTurnBucket(board[node_id], sb_hand, bb_discard_card[node_id]);
            int bb_bucket = bucket->getTurnBucket(board[node_id], bb_hand, sb_discard_card[node_id]);
            int turn = nodes[node_id].getTurn();
            nodes[node_id].setInfoSet(info_set_map[nodes[node_id].getInfoSetIndex()] + (turn == 0 ? sb_bucket : bb_bucket));
            int l = node_id, r = node_id + nodes[node_id].getSize();
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].hasNewCards()){
                    i += nodes[i].getSize();
                    continue;
                }
                int child_turn = nodes[i].getTurn();
                nodes[i].setInfoSet(info_set_map[nodes[i].getInfoSetIndex()] + (child_turn == 0 ? sb_bucket : bb_bucket));
            }
        }
        // deal river
        for(auto [node_id, prv_new_card] : deal_river){
            sb_discard_card[node_id] = sb_discard_card[prv_new_card];
            bb_discard_card[node_id] = bb_discard_card[prv_new_card];
            board[node_id] = board[prv_new_card];
            used_mask[node_id] = used_mask[prv_new_card];
            board[node_id] |= 1ull << generateCard(used_mask[node_id]);
        }
        #pragma omp parallel for
        for(int t = 0; t < deal_river.size(); t++){
            int node_id = deal_river[t].first;
            int prv_new_card = deal_river[t].second;
            int sb_bucket = bucket->getRiverBucket(board[node_id], sb_hand, bb_discard_card[node_id]);
            int bb_bucket = bucket->getRiverBucket(board[node_id], bb_hand, sb_discard_card[node_id]);
            int winner = bucket->getWinner(board[node_id], sb_hand, bb_hand);
            int turn = nodes[node_id].getTurn();
            nodes[node_id].setInfoSet(info_set_map[nodes[node_id].getInfoSetIndex()] + (turn == 0 ? sb_bucket : bb_bucket));
            nodes[node_id].setWinner(winner);
            int l = node_id, r = node_id + nodes[node_id].getSize();
            for(int i = l + 1; i <= r; i++){
                if(nodes[i].hasNewCards()){
                    i += nodes[i].getSize();
                    continue;
                }
                int child_turn = nodes[i].getTurn();
                nodes[i].setInfoSet(info_set_map[nodes[i].getInfoSetIndex()] + (child_turn == 0 ? sb_bucket : bb_bucket));
                nodes[i].setWinner(winner);
            }
        }
    }

    // updates the leaf utility values for each trainer
    void updateUtility(array<float, TREE_SZ> &utility){
        #pragma omp parallel for
        for(int i = 0; i < leaves.size(); i++){
            int node_id = leaves[i].node_id;
            int w = leaves[i].winner;
            if(w == 0) w = nodes[node_id].getWinner();
            utility[node_id] = w*(nodes[node_id].getPot()/2);
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
        return nodes[node_id].getParent();
    }

    // returns the move used to reach node
    int getMove(int node_id){
        return nodes[node_id].getMove();
    }

    // returns info set of node
    int getInfoSet(int node_id){
        return nodes[node_id].getInfoSet();
    }

    // calculates the info set of a node
    int calcInfoSet(int node_id, uint64_t hand, uint64_t board_mask, int discard){
        int street = nodes[node_id].getStreet();
        int info_set_index = nodes[node_id].getInfoSetIndex();
        assert(board_mask == board[node_id]);
        if(street > 2){
            if(nodes[node_id].getTurn() == 1){
                assert(discard == sb_discard_card[node_id]);
            } else {
                assert(discard == bb_discard_card[node_id]);
            }
        }
        if(street == 0){
            return info_set_map[info_set_index] + bucket->getPreflopBucket(hand);
        } else if(street == 2){
            return info_set_map[info_set_index] + bucket->getBBDiscardBucket(board_mask, hand);
        } else if(street == 3){
            return info_set_map[info_set_index] + bucket->getSBDiscardBucket(board_mask, hand, discard);
        } else if(street == 4){
            return info_set_map[info_set_index] + bucket->getFlopBucket(board_mask, hand, discard);
        } else if(street == 5){
            return info_set_map[info_set_index] + bucket->getTurnBucket(board_mask, hand, discard);
        } else if(street == 6){
            return info_set_map[info_set_index] + bucket->getRiverBucket(board_mask, hand, discard);
        } else {
            assert(false);
        }
    }

    // returns who's turn it is to move
    int getTurn(int node_id){
        return nodes[node_id].getTurn();
    }

    // returns the size of the node
    int getSize(int node_id){
        return nodes[node_id].getSize();
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

#endif // THREECARDINFERENCE_HPP