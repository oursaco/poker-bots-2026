#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "encoding.h"
#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <omp.h>
#include <fstream>
using namespace omp;
using namespace std;

#include <array>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <stdexcept>

// Card encoding used here:
//   card_id = rank*4 + suit
//   rank: 0..12  (2..A)
//   suit: 0..3   (c,d,h,s)  (any fixed mapping is fine)
static inline int card_rank(uint8_t card_id) { return card_id / 4; }
static inline int card_suit(uint8_t card_id) { return card_id % 4; }

static inline uint32_t popcount13(uint16_t x) {
#if defined(__GNUG__) || defined(__clang__)
  return (uint32_t)__builtin_popcount((unsigned)x);
#else
  // portable fallback
  uint32_t c = 0;
  while (x) { x &= (uint16_t)(x - 1); ++c; }
  return c;
#endif
}

// Pack 4x 13-bit masks into a 52-bit key in uint64_t.
// masks[] must already be sorted nondecreasing.
static inline uint64_t pack_masks_sorted(const std::array<uint16_t,4>& masks) {
  // 13 * 4 = 52 bits fits in uint64_t
  return (uint64_t)masks[0]
       | ((uint64_t)masks[1] << 13)
       | ((uint64_t)masks[2] << 26)
       | ((uint64_t)masks[3] << 39);
}

// Canonical suit-isomorphism key for an n-card board (n=5 or 6 are typical).
// This key has exactly 962,988 possible values when n=6 (and 134,459 when n=5),
// but the key space itself is not dense. We'll densify later.
template <size_t N>
static inline uint64_t canonical_key(const std::array<uint8_t, N>& cards) {
  std::array<uint16_t,4> masks{0,0,0,0}; // 13-bit each
  for (uint8_t cid : cards) {
    int r = card_rank(cid);
    int s = card_suit(cid);
    masks[s] |= (uint16_t)(1u << r);
  }
  std::sort(masks.begin(), masks.end()); // canonical under suit renaming
  return pack_masks_sorted(masks);
}

// Build all canonical keys for exactly `n_cards` total cards across 4 suits,
// with masks sorted nondecreasing. For n_cards=6, this yields 962,988 keys.
static std::vector<uint64_t> build_canonical_key_table(int n_cards) {
  if (n_cards < 0 || n_cards > 13*4) throw std::invalid_argument("n_cards out of range");
  if (n_cards > 13) {
    // Still possible, but not relevant here; code supports it, but tables grow.
  }

  // Precompute all 13-bit masks grouped by popcount (only up to n_cards needed).
  std::vector<uint16_t> masks_by_pc[14];
  for (uint16_t m = 0; m < (1u << 13); ++m) {
    uint32_t pc = popcount13(m);
    if ((int)pc <= n_cards) masks_by_pc[pc].push_back(m);
  }
  for (int pc = 0; pc <= n_cards; ++pc) {
    std::sort(masks_by_pc[pc].begin(), masks_by_pc[pc].end());
  }

  std::vector<uint64_t> keys;
  keys.reserve((n_cards == 6) ? 962988u : 200000u); // heuristic

  std::array<uint16_t,4> cur{0,0,0,0};

  // DFS over sorted mask tuples m0<=m1<=m2<=m3 with total popcount = n_cards.
  // At each position choose a mask >= previous, with popcount <= remaining.
  struct Rec {
    int n_cards;
    std::vector<uint16_t> (*masks_by_pc)[14];
    std::vector<uint64_t>* keys;
    std::array<uint16_t,4>* cur;

    void run(int pos, uint16_t prev_mask, int remaining) {
      if (pos == 4) {
        if (remaining == 0) {
          // cur is already nondecreasing by construction
          keys->push_back(pack_masks_sorted(*cur));
        }
        return;
      }

      // Prune: remaining cards must fit in remaining positions (each max 13)
      int left = 4 - pos;
      if (remaining < 0 || remaining > 13 * left) return;

      for (int pc = 0; pc <= remaining; ++pc) {
        const auto& lst = (*masks_by_pc)[pc];
        // Find first mask >= prev_mask
        auto it = std::lower_bound(lst.begin(), lst.end(), prev_mask);
        for (; it != lst.end(); ++it) {
          (*cur)[pos] = *it;
          run(pos + 1, *it, remaining - pc);
        }
      }
    }
  };

  Rec rec{n_cards, &masks_by_pc, &keys, &cur};
  rec.run(0, 0, n_cards);

  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

  // Sanity checks for the common cases:
  if (n_cards == 6) assert(keys.size() == 962988u);
  if (n_cards == 5) assert(keys.size() == 134459u);

  return keys;
}

// Compressor: dense id in [0, table.size())
class RiverCompressor {
public:
  explicit RiverCompressor(int n_cards)
    : keys_(build_canonical_key_table(n_cards)) {}

  template <size_t N>
  uint32_t compress(const std::array<uint8_t, N>& cards) const {
    uint64_t k = canonical_key<N>(cards);
    auto it = std::lower_bound(keys_.begin(), keys_.end(), k);
    if (it == keys_.end() || *it != k) {
      throw std::runtime_error("Key not found: are cards valid/unique and N correct?");
    }
    return (uint32_t)std::distance(keys_.begin(), it);
  }

  size_t size() const { return keys_.size(); }

private:
  std::vector<uint64_t> keys_; // sorted
};

// --- OPTIONAL: simple string parsing helpers ("Ah", "7d", etc.) ---
static inline int rank_from_char(char c) {
  const std::string ranks = "23456789TJQKA";
  auto p = ranks.find(c);
  if (p == std::string::npos) throw std::invalid_argument("bad rank char");
  return (int)p; // 0..12
}
static inline int suit_from_char(char c) {
  // pick any consistent mapping
  switch (c) {
    case 'c': return 0;
    case 'd': return 1;
    case 'h': return 2;
    case 's': return 3;
    default: throw std::invalid_argument("bad suit char");
  }
}
static inline uint8_t card_from_str(const std::string& s) {
  if (s.size() != 2) throw std::invalid_argument("card must be 2 chars like Ah");
  int r = rank_from_char(s[0]);
  int su = suit_from_char(s[1]);
  return (uint8_t)(r*4 + su);
}

int board[6];
// omp::HandEvaluator eval;
float wins[169][135991];
float loses[169][135991];
float ties[169][135991];

int encoding_map[(1 << 16)*162];
int encoding_index = 0;

omp::HandEvaluator eval;

void solve(int p1, int p2){
    int ps1 = p1%4;
    int ps2 = p2%4;
    for(int b1 = 0; b1 < 52; b1++){
        if(b1 == p1 || b1 == p2) continue;
        for(int b2 = b1 + 1; b2 < 52; b2++){
            if(b2 == p1 || b2 == p2) continue;
            for(int b3 = b2 + 1; b3 < 52; b3++){
                if(b3 == p1 || b3 == p2) continue;
                for(int b4 = b3 + 1; b4 < 52; b4++){
                    if(b4 == p1 || b4 == p2) continue;
                    for(int b5 = b4 + 1; b5 < 52; b5++){
                        if(b5 == p1 || b5 == p2) continue;
                        for(int b6 = b5 + 1; b6 < 52; b6++){
                            if(b6 == p1 || b6 == p2) continue;
                            int w = 0;
                            int t = 0;
                            int l = 0;
                            Hand board1 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board2 = Hand::empty() + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board3 = Hand::empty() + Hand(b1) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board4 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b4) + Hand(b5) + Hand(b6);
                            Hand board5 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b5) + Hand(b6);
                            Hand board6 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b6);
                            Hand board7 = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5);
                            Hand player = Hand(p1) + Hand(p2);
                            int player_str = max({eval.evaluate(board1 + Hand(p1)), 
                                                eval.evaluate(board1 + Hand(p2)), 
                                                eval.evaluate(board2 + player), 
                                                eval.evaluate(board3 + player), 
                                                eval.evaluate(board4 + player), 
                                                eval.evaluate(board5 + player), 
                                                eval.evaluate(board6 + player), 
                                                eval.evaluate(board7 + player)});
                            for(int o1 = 0; o1 < 52; o1++){
                                if(o1 == p1 || o1 == p2 || o1 == b1 || o1 == b2 || o1 == b3 || o1 == b4 || o1 == b5 || o1 == b6) continue;
                                for(int o2 = o1 + 1; o2 < 52; o2++){
                                    if(o2 == p1 || o2 == p2 || o2 == b1 || o2 == b2 || o2 == b3 || o2 == b4 || o2 == b5 || o2 == b6) continue; 
                                    Hand opp = Hand(o1) + Hand(o2);
                                    int opp_str = max({eval.evaluate(board1 + Hand(o1)), 
                                                    eval.evaluate(board1 + Hand(o2)), 
                                                    eval.evaluate(board2 + opp), 
                                                    eval.evaluate(board3 + opp), 
                                                    eval.evaluate(board4 + opp), 
                                                    eval.evaluate(board5 + opp), 
                                                    eval.evaluate(board6 + opp), 
                                                    eval.evaluate(board7 + opp)});
                                    int diff = player_str - opp_str;
                                    if(diff > 0) w++;
                                    else if(diff < 0) l++;
                                    else t++;
                                }
                            }
                            int player_id = encoding::getHoleId(p1/4, p2/4, ps1, ps2);
                            int encoded = encoding_map[encoding::encodeRiverBoard(board1, ps1, ps2)];
                            wins[player_id][encoded] += w;
                            loses[player_id][encoded] += l;
                            ties[player_id][encoded] += t;
                        }
                    }
                }
            }
        }
    }
}

void generateEncodingMap(){
    for(int i = 0; i < (1 << 16)*162; i++) encoding_map[i] = -1;
    for(int b1 = 0; b1 < 52; b1++){
        for(int b2 = b1 + 1; b2 < 52; b2++){
            for(int b3 = b2 + 1; b3 < 52; b3++){
                for(int b4 = b3 + 1; b4 < 52; b4++){
                    for(int b5 = b4 + 1; b5 < 52; b5++){
                        for(int b6 = b5 + 1; b6 < 52; b6++){
                            Hand board = Hand::empty() + Hand(b1) + Hand(b2) + Hand(b3) + Hand(b4) + Hand(b5) + Hand(b6);
                            for(int s1 = 0; s1 < 4; s1++){
                                for(int s2 = 0; s2 < 4; s2++){
                                    int encoded = encoding::encodeRiverBoard(board, s1, s2);
                                    if(encoding_map[encoded] != -1) continue;
                                    encoding_map[encoded] = encoding_index++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    cout << "number of river boards: " << encoding_index << endl;
}

void saveEncodingMap(string dir){
    ofstream ouf(dir, ios::binary);
    ouf.write(reinterpret_cast<const char*>(&encoding_index), sizeof(int));
    for(int i = 0; i < (1 << 16)*162; i++){
        ouf.write(reinterpret_cast<const char*>(&encoding_map[i]), sizeof(int));
    }
    ouf.close();
}

void saveEquity(string dir){
    ofstream ouf(dir, ios::binary);
    int visited = 0;
    for(int i = 0; i < 169; i++){
        for(int j = 0; j < encoding_index; j++){
            float eq = -1.0f;
            if(wins[i][j] + loses[i][j] + ties[i][j] > 0){
                eq = float(wins[i][j] + ties[i][j]/2.0f)/float(wins[i][j] + loses[i][j] + ties[i][j]);
            }
            ouf.write(reinterpret_cast<const char*>(&eq), sizeof(float));
        }
    }
    cout << "visited nodes: " << visited << endl;
    ouf.close();
}

int main(){
    generateEncodingMap();
    vector<pair<int, int>> cards[169];
    for(int i = 0; i < 52; i++){
        for(int j = i + 1; j < 52; j++){
            cards[encoding::getHoleId(i/4, j/4, i%4, j%4)].push_back(make_pair(i, j));
        }
    }
    saveEncodingMap("./emd_bucket_data/encoding_map.bin");
    #pragma omp parallel for schedule(static)
    for(int t = 0; t < 169; t++){
        int st = 0;
        cout << "Generating equity for hand class " << t << endl;
        for(pair<int, int> p : cards[t]){
            int i = p.first;
            int j = p.second;
            solve(i, j);
        }
    }
    saveEquity("./emd_bucket_data/river_equity.bin");
    
    return 0;
}