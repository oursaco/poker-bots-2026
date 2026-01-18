// Compress a DCFR policy .bin file by removing regret sums and converting to unsigned char
// Usage: compress_bin <input.bin> [output.bin]
// Output defaults to "player.bin" if not specified

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <array>
#include "constants/constants.h"
#include "game/ThreeCardInference.hpp"
#include "game/ThreeCardBucket.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.bin> [output.bin]\n";
        std::cerr << "  Reads a DCFR policy .bin file and writes it without regret sums.\n";
        std::cerr << "  Strategy values are normalized per info set and converted to unsigned char (0-255).\n";
        std::cerr << "  Output defaults to 'player.bin' if not specified.\n";
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = (argc >= 3) ? argv[2] : "player.bin";

    // Initialize tree to get moves_per_info_set
    std::cout << "Initializing game tree...\n";
    ThreeCardInferenceTree tree;
    DynamicThreeCardBucket bucket;
    bucket.init("./emd_bucket_data");
    tree.setBucket(&bucket);
    tree.init();

    std::array<int, POLICY_SZ> moves_per_info_set;
    tree.fillMovesPerInfoSet(moves_per_info_set);
    int info_set_count_from_tree = tree.infoSetCount();
    std::cout << "  info_set_count from tree: " << info_set_count_from_tree << "\n";

    // Open input file
    std::ifstream inf(input_path, std::ios::binary);
    if (!inf) {
        std::cerr << "Error: Could not open input file: " << input_path << "\n";
        return 1;
    }

    // Read header
    int info_set_count, state_count;
    inf.read(reinterpret_cast<char*>(&info_set_count), sizeof(int));
    inf.read(reinterpret_cast<char*>(&state_count), sizeof(int));

    if (!inf) {
        std::cerr << "Error: Failed to read header from input file\n";
        return 1;
    }

    std::cout << "Input file: " << input_path << "\n";
    std::cout << "  info_set_count: " << info_set_count << "\n";
    std::cout << "  state_count: " << state_count << "\n";

    if (info_set_count != info_set_count_from_tree) {
        std::cerr << "Error: info_set_count mismatch! File has " << info_set_count 
                  << " but tree has " << info_set_count_from_tree << "\n";
        return 1;
    }

    // Allocate and read strategy_sum
    std::vector<float> strategy_sum(POLICY_SZ);
    inf.read(reinterpret_cast<char*>(strategy_sum.data()), POLICY_SZ * sizeof(float));

    if (!inf) {
        std::cerr << "Error: Failed to read strategy_sum from input file\n";
        return 1;
    }

    // We skip reading regret_sum since we don't need it
    inf.close();

    // Convert to unsigned char (0-255), normalized per info set
    std::vector<unsigned char> strategy_char(POLICY_SZ, 0);
    int state_idx = 0;
    for (int info_set = 0; info_set < info_set_count; info_set++) {
        int move_count = moves_per_info_set[info_set];
        
        // Sum strategy values for this info set
        float sum = 0.0f;
        for (int m = 0; m < move_count; m++) {
            sum += strategy_sum[state_idx + m];
        }
        
        // Normalize and convert to char
        for (int m = 0; m < move_count; m++) {
            float prob = (sum > 0.0f) ? (strategy_sum[state_idx + m] / sum) : (1.0f / move_count);
            strategy_char[state_idx + m] = static_cast<unsigned char>(std::round(prob * 255.0f));
        }
        
        state_idx += move_count;
    }

    std::cout << "  Verified state_count: " << state_idx << "\n";

    // Open output file
    std::ofstream ouf(output_path, std::ios::binary);
    if (!ouf) {
        std::cerr << "Error: Could not open output file: " << output_path << "\n";
        return 1;
    }

    // Write header
    ouf.write(reinterpret_cast<const char*>(&info_set_count), sizeof(int));
    ouf.write(reinterpret_cast<const char*>(&state_count), sizeof(int));

    // Write strategy as unsigned char (no regret_sum)
    ouf.write(reinterpret_cast<const char*>(strategy_char.data()), POLICY_SZ * sizeof(unsigned char));

    if (!ouf) {
        std::cerr << "Error: Failed to write output file\n";
        return 1;
    }

    ouf.close();

    std::cout << "Output file: " << output_path << "\n";
    std::cout << "  Size reduced from ~" << (8 + 2 * POLICY_SZ * sizeof(float)) / (1024 * 1024) << " MB"
              << " to ~" << (8 + POLICY_SZ * sizeof(unsigned char)) / (1024 * 1024) << " MB\n";
    std::cout << "Done.\n";

    return 0;
}
