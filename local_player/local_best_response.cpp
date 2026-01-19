#include "local_game_state.cpp"

LocalAction get_best_response(LocalGameState* state, DCFRPolicy& opp_policy, bool opp_bb,tuple<int,int,int> hole_cards, int depth){
    vector<int> board = state->board;
    assert(
        (opp_bb && (hole_cards[0] == board[3] || hole_cards[1] == board[3] || hole_cards[2] == board[3])) ||
        (!opp_bb && (hole_cards[0] == board[2] || hole_cards[1] == board[2] || hole_cards[2] == board[2]))
    );
    vector<pair<LocalGameState, int>> tree;
    vector<bool> is_terminal;
    tree.push_back(make_pair(state, 0));
    is_terminal.push_back(false);
    int index = 0, tree_size = 1;
    while(index < tree_size){
        LocalGameState* cur_state = &tree[index].first;
        if(tree[index].second == depth) continue;
        auto actions = cur_state->get_possible_actions();
        assert(index <= is_terminal.size()-1);
        if(actions.empty()){
            is_terminal[index] = true;
        }
        assert((actions.empty() && is_terminal[index]) || (!actions.empty() && !is_terminal[index]));
        for(auto action: actions){
            LocalGameState next_state = cur_state->get_next_state(action);
            tree.push_back(make_pair(next_state, tree[index].second + 1));
            is_terminal.push_back(false);
            tree_size++;
        }
        index++;
    }
    vector<int> chipev;
    chipev.resize(tree.size(), 0);
    for(int i=tree.size()-1; i>=0; i--){
        if(is_terminal[i]){

        }
    }
}