#ifndef TREE_HPP
#define TREE_HPP

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <array>
#include <cassert>
using namespace std;

struct Action {
    virtual ~Action() = default;
    virtual string toString() = 0; // returns a string representation of the action
};

struct GameState {
    virtual ~GameState() = default;
    virtual vector<pair<unique_ptr<GameState>, unique_ptr<Action>>> generateActions() = 0; 
    virtual bool isTerminal() = 0;
    virtual string getWinner() = 0;
};

struct GameTree {
    virtual ~GameTree() = default;
    virtual void updateUtility(array<float, 1000> &utility) = 0; // updates the leaf utility values
    virtual void init() = 0; // initializes the game tree
    virtual void prepare(int seed) = 0; // prepares the game tree for an iteration of training
    virtual vector<int> getMovesPerInfoSet() = 0; // returns the number of moves per info set
    virtual int nodeCount() = 0; // returns the number of nodes
    virtual int getParentId(int node_id) = 0; // returns parent node id
    virtual int getMove(int node_id) = 0; // returns the move used to reach node
    virtual int getInfoSet(int node_id) = 0; // returns info set of node
    virtual int getTurn(int node_id) = 0; // returns who's turn it is to move
};

#endif // TREE_HPP
