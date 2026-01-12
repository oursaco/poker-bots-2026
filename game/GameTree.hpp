#ifndef TREE_HPP
#define TREE_HPP

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <array>
#include <cassert>
#include "constants/constants.h"
using namespace std;

struct Action {
    virtual ~Action() = default;
    virtual string toString() = 0; // returns a string representation of the action
    virtual bool isWorldAction() = 0;
};

struct GameState {
    virtual ~GameState() = default;
    virtual vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> generateActions() = 0; 
    virtual bool isTerminal() = 0;
    virtual string getWinner() = 0;
};

struct GameTree {
    virtual ~GameTree() = default;
    // Creates an independent copy of the concrete tree instance.
    // Used to run evaluations in parallel without mutating the training tree.
    virtual unique_ptr<GameTree> clone() const = 0;
    virtual void updateUtility(array<float, TREE_SZ> &utility) = 0; // updates the leaf utility values
    virtual void init() = 0; // initializes the game tree
    virtual void prepare(int seed) = 0; // prepares the game tree for an iteration of training
    virtual void fillMovesPerInfoSet(array<int, POLICY_SZ> &moves_per_info_set_) = 0; // returns the number of moves per info set
    virtual int infoSetCount() = 0; // returns the number of info sets
    virtual int nodeCount() = 0; // returns the number of nodes
    virtual int getParentId(int node_id) = 0; // returns parent node id
    virtual int getMove(int node_id) = 0; // returns the move used to reach node
    virtual int getInfoSet(int node_id) = 0; // returns info set of node
    virtual int getTurn(int node_id) = 0; // returns who's turn it is to move
    virtual int getSize(int node_id) = 0; // returns the size of the node
};

#endif // TREE_HPP
