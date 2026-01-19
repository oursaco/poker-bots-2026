#include <string>
#include "game/GameTree.hpp"
#include "cfr/CFR.hpp"

struct LocalAction { 
    string action; // fold, check, bet, call, raise, all-in
    int turn;
    int amount;
    int sb_stack;
    int bb_stack;
    float pot_size;

    LocalAction(string action_, int turn_, int amount_, int sb_stack_, int bb_stack_, float pot_size_){
        action = action_;
        turn = turn_;
        amount = amount_;
        sb_stack = sb_stack_;
        pot_size = pot_size_;
        bb_stack = bb_stack_;
    }
};

struct LocalGameState {
    vector<LocalAction> action_history;
    vector<int> board; // 0, 1: flop, 2: bb discard, 3: sb discard, 4: turn, 5: river
    int street; // 
    int pot;
    int sb_stack, bb_stack;
    int turn; // 0: sb, 1: bb, -1: world
    int winner; // 0: sb wins, 1: bb wins, -1: no winner

    vector<pair<LocalAction, float>> get_possible_actions(){
        //to-do
        // returns a vector of pairs of actions and the probabilites of each action
        // this function should be deterministic (i.e. should always return the same actions and probabilites in the same order)
    }

    LocalGameState get_next_state(LocalAction action){
        
    }
};

struct LocalPlayer {

    bool is_online = false;
    string cfr_policy_path = "";
    DCFRPolicy cfr_policy;
    int depth;


    void init(string cfr_policy_path_, int depth_){
        if(cfr_policy_path_ != ""){
            cfr_policy.loadPolicy(cfr_policy_path_);
            is_online = false;
            cfr_policy_path = cfr_policy_path_;
        } else{
            is_online = true;
        }
        if(depth_ > 0){
            depth = depth_;
        }
    }
};