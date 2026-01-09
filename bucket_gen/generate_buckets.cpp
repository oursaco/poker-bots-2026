/*
We want to map every combination of (3 distinct hole cards) x (strength of board + hole cards) to an equity against a randomly distributed opponent hand

For bb discard, we want to map (3 distinct cards) to an equity against a randomly distributed opponent hand

For sb discard, we want to map (4 distinct cards) to an equity against a randomly distributed opponent hand

For flop, we want to map (2 distinct hole cards) x (4 distinct cards) to an equity against a randomly distributed opponent hand

For turn, we want to map (2 distinct hole cards) x (5 distinct cards) to an equity against a randomly distributed opponent hand

For river, we want to map (2 distinct hole cards) x (6 distinct cards) to an equity against a randomly distributed opponent hand
*/

#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "external/omp/Random.h"
#include <set>
#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
using namespace std;

void extractCard(uint64_t &hand, unsigned &card, unsigned &suit) {
    unsigned c = __builtin_ctzll(hand);
    card = c/4;
    suit = c%4;
    hand ^= 1ull << c;
}

int comb_ind[13][13][13];

void fillCombInd() {
    int ind = 0;
    for(int i = 0; i < 13; i++) {
        for(int j = i + 1; j < 13; j++) {
            for(int k = j + 1; k < 13; k++) {
                comb_ind[i][j][k] = ind++;
            }
        }
    }
}



int encodeHolePreflop(uint64_t hand) {
    unsigned r0, r1, r2;
    unsigned s0, s1, s2;
    extractCard(hand, r0, s0);
    extractCard(hand, r1, s1);
    extractCard(hand, r2, s2);

    // Trips
    if(r0 == r1 && r1 == r2) return r0;

    // Pair
    if(r0 == r1 || r1 == r2){
        unsigned pair_rank = (r0 == r1) ? r0 : r1;
        unsigned kicker_rank = (r0 == r1) ? r2 : r0;
        unsigned pair_index = pair_rank * 12 + (kicker_rank > pair_rank ? kicker_rank - 1 : kicker_rank);

        // suit type
        int suit_type = 0; // 0 = kicker matches one pair suit, 1 = kicker matches none
        if (r0 == r1){
            suit_type = (s2 == s0 ||
                        s2 == s1) ? 0 : 1;
        } else {
            suit_type = (s0 == s1 || s0 == s2) ? 0 : 1;
        }
        assert(13 + pair_index * 2 + suit_type < 325);
        return 13 + pair_index * 2 + suit_type;
    }

    // Distinct
    int rank_index = comb_ind[r0][r1][r2];

    // normalize suits
    int suit_pattern;
    if (s0 == s1 && s1 == s2) {
        suit_pattern = 0; // monotone
    } else if (s0 != s1 && s0 != s2 && s1 != s2) {
        suit_pattern = 1; // rainbow
    } else {
        // single suited
        if (s0 == s1) suit_pattern = 2;
        else if (s0 == s2) suit_pattern = 3;
        else suit_pattern = 4;
    }

    return 325 + rank_index * 5 + suit_pattern;
}

int preflop_map[52][52][52];
vector<int> preflop_buckets;

void removeDuplicates(vector<int> &buckets){
    sort(buckets.begin(), buckets.end());
    buckets.erase(unique(buckets.begin(), buckets.end()), buckets.end());
}

void writePreflopBuckets(){
    ofstream ouf("./bucket_data/preflop_buckets.bin", ios::binary);
    int num_buckets = preflop_buckets.size();
    ouf.write(reinterpret_cast<const char*>(&num_buckets), sizeof(int));
    for(int i = 0; i < 52; i++){
        for(int j = 0; j < 52; j++){
            for(int k = 0; k < 52; k++){
                ouf.write(reinterpret_cast<const char*>(&preflop_map[i][j][k]), sizeof(int));
            }
        }
    }
    ouf.close();
}



int main(){
    fillCombInd();
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            for(int k = j + 1; k < 52; k++){
                int encoded = encodeHolePreflop(1ull << i | 1ull << j | 1ull << k);
                preflop_map[i][j][k] = encoded;
                preflop_buckets.push_back(encoded);
            }
        }
    }
    sort(preflop_buckets.begin(), preflop_buckets.end());
    removeDuplicates(preflop_buckets);
    cout << "num preflop buckets: " << preflop_buckets.size() << endl;
    writePreflopBuckets();
}