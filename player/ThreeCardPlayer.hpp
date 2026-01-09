#ifndef THREECARDPLAYER_HPP
#define THREECARDPLAYER_HPP

#include <cstdlib>
#include "Player.hpp"
#include "game/ThreeCard.hpp"
#include "cfr/CFR.hpp"

struct ThreeCardPlayer : Player {

    NaiveThreeCardBucket bucket;
    ThreeCardGameTree tree;
    ThreeCardGameState state;
    DCFRPolicy policy;
    array<vector<int>, TREE_SZ> children;
    int node_id = 0;
    bool initialized = false;
    int real_sb_stack = 0;
    int real_bb_stack = 0;
    int real_pot = 0;
    int real_sb_bet = 0;
    int real_bb_bet = 0;
    int round = 0;
    int player_id = 0; // 0: sb, 1: bb
    int pnl = 0;
    uint64_t hand;
    uint64_t board;
    bool call_next; // if their raise gets casted to a check back
    bool all_in_next; // if their raise gets casted to an all in
    bool all_in; // if I am all in
    bool won_all_in; // they folded to all in

    void init(string policy_path){
        tree.setBucket(&bucket);
        tree.init();
        for(int i = 1; i < tree.nodeCount(); i++){
            assert(children[tree.getParentId(i)].size() == tree.getMove(i));
            children[tree.getParentId(i)].push_back(i);
        }
        state = ThreeCardGameState();
        policy.initPolicy(tree.getMovesPerInfoSet());
        policy.loadPolicy(policy_path);
        call_next = false;
        all_in_next = false;
        all_in = false;
        won_all_in = false;
        pnl = 0;
        initialized = true;
    }

    void startRound(int player_id_, uint64_t hand_){
        call_next = false;
        all_in_next = false;
        all_in = false;
        won_all_in = false;
        real_sb_stack = 399;
        real_bb_stack = 398;
        real_pot = 3;
        real_sb_bet = 1;
        real_bb_bet = 2;
        round++;
        player_id = player_id_;
        node_id = 0;
        hand = hand_;
    }

    void updateBoard(unsigned new_card){
        board |= 1ull << new_card;
    }

    vector<ThreeCardAction> getActions(){
        auto actions = state.generateActions();
        vector<ThreeCardAction> result;
        result.reserve(actions.size());
        for(auto &entry : actions){
            result.push_back(*dynamic_cast<ThreeCardAction*>(entry.second.get()));
        }
        return result;
    }

    vector<float> getActionProbabilities(){
        assert(!state.isTerminal());
        vector<float> probs;
        auto actions = state.generateActions();
        assert(!actions.empty());
        probs.assign(actions.size(), 0.0f);
        assert(state.turn == player_id);
        int info_set = tree.calcInfoSet(node_id, hand, board);
        int move_count = policy.getMoveCount(info_set);
        assert(move_count == actions.size());
        int st = policy.getState(info_set, 0);
        float sum = 0.0f;
        for(int move = 0; move < move_count; move++) sum += policy.strategy_sum[st + move];
        if(sum <= 0.0f){
            cout << "ERROR: strategy not computed for info set " << info_set << " in round " << round << endl;
            float uniform = 1.0f/move_count;
            for(int i = 0; i < move_count; i++) probs[i] = uniform;
            return probs;
        }
        for(int i = 0; i < move_count; i++) probs[i] = policy.strategy_sum[st + i]/sum;
        return probs;
    }

    void receiveAction(unique_ptr<Action> action){
        ThreeCardAction real_action = *dynamic_cast<ThreeCardAction*>(action.get());
        if(all_in){
            if(real_action.action == "fold"){
                won_all_in = true;
                return;
            }
            if(real_action.action == "call"){
                if(player_id == 1) real_sb_stack -= real_action.amount;
                if(player_id == 0) real_bb_stack -= real_action.amount;
                real_pot += real_action.amount;
                assert(real_sb_stack == 0 && real_bb_stack == 0);
                all_in = false;
                return;
            }
            assert(false);
        }
        auto actions = state.generateActions();
        assert(!actions.empty());
        if(real_action.isWorldAction()){
            assert(actions.size() == 1);
            state = *dynamic_cast<ThreeCardGameState*>(actions[0].first.get());
            return;
        }
        assert(state.turn == (player_id ^ 1));
        string type = real_action.action;
        if(type == "discard"){
            for(int i = 0; i < actions.size(); i++){
                auto action = dynamic_cast<ThreeCardAction*>(actions[i].second.get());
                if(action->action == type){
                    state = *dynamic_cast<ThreeCardGameState*>(actions[i].first.get());
                    node_id = children[node_id][i];
                    return;
                }
            }
        } if(type == "fold"){
            for(int i = 0; i < actions.size(); i++){
                auto action = dynamic_cast<ThreeCardAction*>(actions[i].second.get());
                if(action->action == type){
                    state = *dynamic_cast<ThreeCardGameState*>(actions[i].first.get());
                    node_id = children[node_id][i];
                    return;
                }
            }
            assert(false);
        } else if(type == "call"){
            if(player_id == 1){
                real_sb_stack -= real_action.amount;
                real_sb_bet += real_action.amount;
            }
            if(player_id == 0){
                real_bb_stack -= real_action.amount;
                real_bb_bet += real_action.amount;
            }
            int cur_street = state.street;
            for(int i = 0; i < actions.size(); i++){
                auto action = dynamic_cast<ThreeCardAction*>(actions[i].second.get());
                if(action->action == type){
                    state = *dynamic_cast<ThreeCardGameState*>(actions[i].first.get());
                    if(state.street != cur_street) real_sb_bet = real_bb_bet = 0;
                    node_id = children[node_id][i];
                    return;
                }
            }
        } else if(type == "raise"){
            real_sb_bet += real_action.amount;
            real_pot += real_action.amount;
            real_sb_stack -= real_action.amount;
            int lb = -1, ub = -1;
            vector<ThreeCardAction> aval_actions;
            for(int i = 0; i < actions.size(); i++){
                aval_actions.push_back(*dynamic_cast<ThreeCardAction*>(actions[i].second.get()));
                if(aval_actions[i].action == "call" || aval_actions[i].action == "raise"){
                    if(aval_actions[i].pot_size <= real_action.pot_size) lb = i;
                    if(aval_actions[i].pot_size >= real_action.pot_size) ub = i;
                }
            }
            assert(lb != -1 || ub != -1);
            int chosen = -1;
            if(lb != ub){
                float ub_dist = aval_actions[ub].pot_size - real_action.pot_size;
                float gap = aval_actions[ub].pot_size - aval_actions[lb].pot_size;
                float lb_prob = float(ub_dist*(1 + aval_actions[lb].pot_size))/float(gap*(1 + real_action.pot_size));
                float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                chosen = (roll < lb_prob) ? lb : ub;
            } else {
                chosen = lb;
            }
            int cur_street = state.street;
            state = *dynamic_cast<ThreeCardGameState*>(actions[chosen].first.get());
            node_id = children[node_id][chosen];
            // casted to a call that ends action
            if(aval_actions[chosen].action == "call" && (state.street != cur_street || state.isTerminal())) call_next = true;
            int opp_stack = (player_id == 1 ? aval_actions[chosen].sb_stack : aval_actions[chosen].bb_stack);
            // casted to an all in
            if(aval_actions[chosen].action == "raise" && opp_stack == 0){
                all_in_next = true;
            }
        }
    }

    int relativeRaiseSize(float pot_mult){
        int bet_diff = max(real_sb_bet, real_bb_bet) - min(real_sb_bet, real_bb_bet);
        return bet_diff + pot_mult*(real_pot + bet_diff);
    }

    unique_ptr<Action> getResponseAction(){
        if(call_next){
            call_next = false;
            int amount = max(real_sb_bet, real_bb_bet) - min(real_sb_bet, real_bb_bet);
            if(player_id == 0) real_sb_stack -= amount;
            if(player_id == 1) real_bb_stack -= amount;
            return make_unique<ThreeCardAction>("call", player_id, amount, real_sb_stack, real_bb_stack, 0.0f);
        }
        assert(!state.isTerminal());
        assert(state.turn == player_id);
        if(state.street == 0) assert(__builtin_popcountll(board) == 0 && __builtin_popcountll(hand) == 3);
        if(state.street == 1) assert(__builtin_popcountll(board) == 2);
        if(state.street == 2) assert(__builtin_popcountll(board) == 3);
        if(state.street == 3) assert(__builtin_popcountll(board) == 4);
        if(state.street == 4) assert(__builtin_popcountll(board) == 4);
        if(state.street == 5) assert(__builtin_popcountll(board) == 5);
        if(state.street == 6) assert(__builtin_popcountll(board) == 6);
        int info_set = tree.calcInfoSet(node_id, hand, board);
        vector<ThreeCardAction> actions = getActions();
        vector<float> probs = getActionProbabilities();
        float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float cum_prob = 0.0f;
        int chosen_action = -1;
        for(int i = 0; i < actions.size(); i++){
            cum_prob += probs[i];
            if(roll < cum_prob){
                chosen_action = i;
                break;
            }
        }
        assert(chosen_action != -1);
        if(actions[chosen_action].action == "discard"){
            return make_unique<ThreeCardAction>("discard", player_id, 0, real_sb_stack, real_bb_stack, 0.0f);
        } else if(actions[chosen_action].action == "fold"){
            if(all_in_next) all_in_next = false;
            return make_unique<ThreeCardAction>("fold", player_id, 0, real_sb_stack, real_bb_stack, 0.0f);
        } else if(actions[chosen_action].action == "call"){
            if(all_in_next){
                all_in_next = false;
                if(player_id == 0){
                    real_pot += real_sb_stack;
                    real_sb_bet += real_sb_stack;
                    real_sb_stack = 0;
                }
                if(player_id == 1){
                    real_pot += real_bb_stack;
                    real_bb_bet += real_bb_stack;
                    real_bb_stack = 0;
                }
                return make_unique<ThreeCardAction>("raise", player_id, real_bb_stack, real_sb_stack, real_bb_stack, 0.0f);
            }
            int amount = max(real_sb_bet, real_bb_bet) - min(real_sb_bet, real_bb_bet);
            real_pot += amount;
            if(player_id == 0) real_sb_stack -= amount;
            if(player_id == 1) real_bb_stack -= amount;
            // sb first to act preflop
            if(player_id == 0 && real_sb_bet == 1) real_sb_bet++;
            return make_unique<ThreeCardAction>("call", player_id, amount, real_sb_stack, real_bb_stack, 0.0f);
        } else if(actions[chosen_action].action == "raise"){
            int amount = relativeRaiseSize(actions[chosen_action].pot_size);
            if(player_id == 0){
                amount = min(amount, real_sb_stack);
                real_sb_bet += amount;
            }
            if(player_id == 1){
                amount = min(amount, real_bb_stack);
                real_bb_bet += amount;
            }
            real_pot += amount;
            return make_unique<ThreeCardAction>("raise", player_id, amount, real_sb_stack, real_bb_stack, actions[chosen_action].pot_size);
        }
        assert(false);
    }

    void verifyState(int sb_stack, int bb_stack, int pot){
        assert(real_sb_stack == sb_stack);
        assert(real_bb_stack == bb_stack);
        assert(real_pot == pot);
    }

    void verifyRound(int round_, int pnl_, int player_id_){
        assert(round == round_);
        assert(pnl == pnl_);
        assert(player_id == player_id_);
    }

    void endRound(int winner){
        assert(all_in || state.isTerminal());
        if(won_all_in){
            pnl += (real_pot - (max(real_sb_bet, real_bb_bet) - min(real_sb_bet, real_bb_bet)))/2;
        } else {
            if(winner == player_id) pnl += real_pot/2;
            else pnl -= real_pot/2;
        }
    }

};

#endif // THREECARDPLAYER_HPP
