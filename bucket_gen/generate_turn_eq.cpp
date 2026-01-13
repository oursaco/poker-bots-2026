#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"

using namespace omp;
using namespace std;

int board[5];
omp::HandEvaluator eval;
float wins[169][135991];
float loses[169][135991];
float ties[169][135991];

int encodeBoard(Hand board, int s1, int s2){
    assert(board.count() == 5);
    int suited_state = -1;
    int strength = eval.evaluate(board);
    if(s1 == s2){
        if(board.suitCount(s1) == 0){
            // there can only be one flush draw
            int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
            mx_suit = min(3, max(0,mx_suit-2));
            suited_state = max_suit;
            assert(suited_state >= 0 && suited_state <= 3);
        } else{
            // we need to check max suit outside of the same suit since there can only be one flush draw other than ours
            int mx_suit = max({board.suitCount((s1+1)%4), board.suitCount((s1+2)%4), board.suitCount((s1+3)%4)});
            int same_suit = board.suitCount(s1);
            // we don't care about 0, 1, or 2
            mx_suit = min(3, max(0, mx_suit-2));
            same_suit = min(3, max(0, same_suit-2));
            suited_state = 4 + same_suit*4 + max_suit;
            assert(suited_state >= 4 && suited_state <= 19);
        }
    } else{
        int same1 = board.suitCount(s1);
        int same2 = board.suitCount(s2);
        if(same1 == 0 && same2 == 0){
            int mx_suit = max({board.suitCount(0), board.suitCount(1), board.suitCount(2), board.suitCount(3)});
            mx_suit = min(3, max(0, mx_suit-2));
            suited_state = 20 + mx_suit;
            assert(suited_state >= 20 && suited_state <= 23);
        } else{
            same1 = min(3, max(0, same1-2));
            same2 = min(3, max(0, same2-2));

            int mx_suit = 0;
            for(int i=0; i<4; i++)
                if(i != s1 && i != s2)
                    mx_suit = max(mx_suit, board.suitCount(i));
            mx_suit = min(3, max(0, mx_suit-2)); 
            suited_state = 24 + same1*4*4 + same2*4 + mx_suit;

            assert(suited_state >= 24 && suited_state <= 88);
        }
    }
    return strength * 89 + suited_state;
}

int calculateHistogram(int p1, int p2){
    array<int, 52> deck;
    mt19937 rng(random_device{}());

    for(int b1=0; b1<52; b1++){
        if(b1 == p1 || b1 == p2) continue;
        for(int b2=b1+1; b2<52; b2++){
            if(b2 == p1 || b2 == p2) continue;
            for(int b3=b2+1; b3<52; b3++){
                if(b3 == p1 || b3 == p2) continue;
                for(int b4=b3+1; b4<52; b4++){
                    if(b4 == p1 || b4 == p2) continue;
                    for(int b5=b4+1; b5<52; b5++){
                        if(b5 == p1 || b5 == p2) continue;
                        for(int b6=0; b6<52; b6++){
                            if(
                                b6 == p1 || b6 == p2 ||
                                b6 == b1 || b6 == b2 || b6 == b3 || b6 == b4 || b6 == b5
                            ) continue;
                            Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            
                            //need to get equity of board from river_equity.bin
                           
                            }

                        }
                    }
                }
            }
        }
    }
}