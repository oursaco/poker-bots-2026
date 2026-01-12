#include <iostream>
#include <fstream>
#include "cfr/CFR.hpp"
using namespace std;

int main() {
    string target_path = "./checkpoints/player.bin";
    float iterations = 10000000;
    DCFRPolicy policy;
    policy.loadPolicy(target_path);
    for(int i = 0; i < policy.state_count; i++){
        policy.strategy_sum[i] /= iterations*iterations;
    }
    policy.savePolicy(target_path);
    return 0;
}