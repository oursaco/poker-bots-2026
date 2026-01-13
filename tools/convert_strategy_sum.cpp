#include <iostream>
#include <fstream>
#include "cfr/CFR.hpp"
#include "game/ThreeCard.hpp"
using namespace std;

int main() {
    string target_path = "./three_card_models/player.bin";
    float iterations = 10000000;
    DCFRPolicy policy;
    EHSThreeCardBucket bucket;
    bucket.init("./bucket_data");
    ThreeCardGameTree tree;
    tree.setBucket(&bucket);
    tree.init();
    policy.initPolicy(&tree);
    policy.loadPolicy(target_path);
    cout << "states: " << policy.state_count << endl;
    for(int i = 0; i < policy.state_count; i++){
        policy.strategy_sum[i] /= iterations*iterations;
    }
    policy.savePolicy(target_path);
    cout << "done" << endl;
    return 0;
}