#include "game/ThreeCardInference.hpp"
#include "constants/constants.h"
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCard.hpp"
#include "cfr/CFRFast.hpp"
#include "cfr/CFR.hpp"
#include <omp.h>
#include <chrono>
using namespace std;

void test_inference(){
    ThreeCardInferenceTree tree1;
    ThreeCardInferenceTree tree2;
    DynamicThreeCardBucket bucket;
    bucket.init("./emd_bucket_data");
    tree1.setBucket(&bucket);
    tree2.setBucket(&bucket);
    tree1.init();
    tree2.init();
    FastTrainer trainer;
    trainer.setTree(&tree1, &tree2);
    auto start = chrono::high_resolution_clock::now();
    trainer.train(0, 1000, 100000.0f, 100000.0f, "", "", "");
    auto end = chrono::high_resolution_clock::now();
    cout << "Training time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " milliseconds" << endl;
}

int main() {
    test_inference();
    return 0;
    ThreeCardInferenceTree tree1;
    ThreeCardInferenceTree tree3;
    ThreeCardGameTree tree2;
    DynamicThreeCardBucket bucket;
    bucket.init("./emd_bucket_data");
    tree1.setBucket(&bucket);
    tree1.init();
    tree2.setBucket(&bucket);
    tree2.init();
    tree3.setBucket(&bucket);
    tree3.init();
    FastTrainer trainer1;
    trainer1.setTree(&tree1, &tree3);
    trainer1.train(0, 1, 100000.0f, 100000.0f, "", "", "");
    DCFRTrainer trainer2;
    trainer2.setTree(&tree2);
    trainer2.train(0, 1, 100000.0f, 100000.0f, "", "", "");
    for(int i = 0; i < POLICY_SZ; i++){
        if(abs(trainer1.players[0].regret_sum[i] - trainer2.players[0].regret_sum[i]) > 1e-3){
            cout << "Mismatch at index " << i << ": " << trainer1.players[0].regret_sum[i] << " vs " << trainer2.players[0].regret_sum[i] << endl;
        }
        assert(abs(trainer1.players[0].regret_sum[i] - trainer2.players[0].regret_sum[i]) < 1e-3);
        if(abs(trainer1.players[0].strategy_sum[i] - trainer2.players[0].strategy_sum[i]) > 1e-3){
            cout << "Mismatch at index " << i << ": " << trainer1.players[0].strategy_sum[i] << " vs " << trainer2.players[0].strategy_sum[i] << endl;
        }
        assert(abs(trainer1.players[0].strategy_sum[i] - trainer2.players[0].strategy_sum[i]) < 1e-3);
    }

    return 0;
}