#ifndef CFRQUERYSTATE_HPP
#define CFRQUERYSTATE_HPP

#include "LocalGameState.hpp"
#include "LocalAction.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"
#include "cfr/CFR.hpp"
#include <vector>
#include <utility>
#include <cassert>

using namespace std;

struct CFRQueryState {
    int node_id = 0;
    ThreeCardGameState tree_state;
    uint64_t replay_board = 0;
    uint64_t flop_mask = 0;
    int8_t bb_discard = -1;
    int8_t sb_discard = -1;
    
    CFRQueryState() {
        auto initial_actions = tree_state.generateActions();
        assert(initial_actions.size() == 1);
        tree_state = *dynamic_cast<ThreeCardGameState*>(initial_actions[0].first.get());
    }
};

class CFRQueryStateManager {
    ThreeCardGameTree& tree;
    DCFRPolicy& policy;
    array<vector<int>, TREE_SZ>& children;
    
public:
    CFRQueryStateManager(
        ThreeCardGameTree& tree_,
        DCFRPolicy& policy_,
        array<vector<int>, TREE_SZ>& children_
    ) : tree(tree_), policy(policy_), children(children_) {}
    
    CFRQueryState createQueryState(const LocalGameState& state, uint64_t hand, int player_id) {
        CFRQueryState qs;
        for (const LocalAction& action : state.history) {
            advanceQueryState(qs, action, hand, player_id);
        }
        return qs;
    }
    
    void advanceQueryState(CFRQueryState& qs, const LocalAction& action, 
                          uint64_t hand, int player_id) {
        if (action.type == ActionType::DEAL_FLOP) {
            auto tree_actions = qs.tree_state.generateActions();
            assert(tree_actions.size() == 1);
            qs.tree_state = *dynamic_cast<ThreeCardGameState*>(tree_actions[0].first.get());
            qs.replay_board = action.value;
            return;
        }
        
        if (action.type == ActionType::DEAL_TURN) {
            auto tree_actions = qs.tree_state.generateActions();
            assert(tree_actions.size() == 1);
            qs.tree_state = *dynamic_cast<ThreeCardGameState*>(tree_actions[0].first.get());
            qs.replay_board |= (1ull << action.value);
            return;
        }
        
        if (action.type == ActionType::DEAL_RIVER) {
            auto tree_actions = qs.tree_state.generateActions();
            assert(tree_actions.size() == 1);
            qs.tree_state = *dynamic_cast<ThreeCardGameState*>(tree_actions[0].first.get());
            qs.replay_board |= (1ull << action.value);
            return;
        }
        
        auto tree_actions = qs.tree_state.generateActions();
        assert(!tree_actions.empty());
        
        int action_idx = findActionIndex(action, qs.tree_state, tree_actions, 
                                         hand, qs.replay_board, player_id);
        assert(action_idx >= 0 && action_idx < (int)tree_actions.size());
        
        if (action.type == ActionType::DISCARD) {
            qs.replay_board |= (1ull << action.value);
            
            int acting_player = qs.tree_state.turn;
            if (acting_player == 0) {
                qs.sb_discard = action.value;
            } else {
                qs.bb_discard = action.value;
            }
            
            if (qs.tree_state.street == 3) {
                qs.flop_mask = qs.replay_board;
            }
        }
        
        qs.tree_state = *dynamic_cast<ThreeCardGameState*>(tree_actions[action_idx].first.get());
        qs.node_id = children[qs.node_id][action_idx];
    }
    
    vector<pair<LocalAction, float>> query(
        const CFRQueryState& qs,
        uint64_t hand,
        int player_id
    ) {
        vector<pair<LocalAction, float>> result;
        
        ThreeCardGameState tree_state_copy = qs.tree_state;
        auto tree_actions = tree_state_copy.generateActions();
        if (tree_actions.empty()) {
            return result;
        }
        
        int opp_discard, my_discard;
        if (player_id == 0) {
            opp_discard = qs.bb_discard;
            my_discard = qs.sb_discard;
        } else {
            opp_discard = qs.sb_discard;
            my_discard = qs.bb_discard;
        }
        
        int info_set = tree.calcInfoSet(qs.node_id, hand, qs.flop_mask, qs.replay_board, 
                                        opp_discard, my_discard);
        
        int move_count = policy.getMoveCount(info_set);
        assert(move_count == (int)tree_actions.size());
        
        int st = policy.getState(info_set, 0);
        float sum = 0.0f;
        for (int i = 0; i < move_count; i++) {
            sum += policy.strategy_sum[st + i];
        }
        
        for (int i = 0; i < move_count; i++) {
            float prob = (sum > 0.0f) ? policy.strategy_sum[st + i] / sum : 1.0f / move_count;
            LocalAction local_action = treeActionToLocal(
                *dynamic_cast<ThreeCardAction*>(tree_actions[i].second.get()),
                hand,
                qs.replay_board
            );
            result.push_back({local_action, prob});
        }
        
        return result;
    }

private:
    int cardToDiscardMove(int card, uint64_t hand, uint64_t board) {
        array<int, 3> order = tree.calcOrder(board, hand);
        
        uint64_t h = hand;
        for (int pos = 0; pos < 3; pos++) {
            if (__builtin_ctzll(h) == card) return order[pos];
            h ^= (1ull << __builtin_ctzll(h));
        }
        assert(false);
        return -1;
    }
    
    int discardMoveToCard(int move_idx, uint64_t hand, uint64_t board) {
        array<int, 3> order = tree.calcOrder(board, hand);
        
        for (int pos = 0; pos < 3; pos++) {
            if (order[pos] == move_idx) {
                uint64_t h = hand;
                for (int i = 0; i < pos; i++) {
                    h ^= (1ull << __builtin_ctzll(h));
                }
                return __builtin_ctzll(h);
            }
        }
        assert(false);
        return -1;
    }
    
    int findActionIndex(
        const LocalAction& action,
        const ThreeCardGameState& tree_state,
        const vector<pair<unique_ptr<GameState>, unique_ptr<Action>>>& tree_actions,
        uint64_t hand,
        uint64_t board,
        int query_player_id
    ) {
        if (action.type == ActionType::DISCARD) {
            if (tree_state.turn != query_player_id) {
                return 0;
            }
            int move_idx = cardToDiscardMove(action.value, hand, board);
            assert(move_idx >= 0 && move_idx < (int)tree_actions.size());
            return move_idx;
        }
        
        for (size_t i = 0; i < tree_actions.size(); i++) {
            auto* ta = dynamic_cast<ThreeCardAction*>(tree_actions[i].second.get());
            
            if (action.type == ActionType::FOLD && ta->action == "fold") return i;
            if (action.type == ActionType::CALL && ta->action == "call") return i;
            if (action.type == ActionType::RAISE && ta->action == "raise" && 
                ta->amount == (int)action.value) return i;
        }
        
        assert(false);
        return -1;
    }
    
    LocalAction treeActionToLocal(const ThreeCardAction& ta, uint64_t hand, uint64_t board) {
        if (ta.action == "fold") return LocalAction::fold();
        if (ta.action == "call") return LocalAction::call();
        if (ta.action == "raise") return LocalAction::raise(ta.amount);
        if (ta.action == "discard") {
            int card = discardMoveToCard(ta.amount, hand, board);
            return LocalAction::discard(card);
        }
        assert(false);
        return LocalAction::fold();
    }
};

#endif // CFRQUERYSTATE_HPP
