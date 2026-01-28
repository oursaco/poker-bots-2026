#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

// This tool writes compressed versions of the emd_bucket_data .bin files.
// Output formats:
//  - preflop.cbin: int32 num_buckets, uint32 count, uint16[count] values
//  - *_encoding_map.cbin: int32 num_buckets, uint32 count, uint8 bytes_per_entry,
//    3 pad bytes, then packed entries (little-endian). -1 is encoded as all 1s.
//  - *_equity*.cbin: uint32 rows, uint32 cols, float min, float max,
//    then uint8[count] values quantized linearly into [min, max]. 255 encodes -1.

namespace fs = std::filesystem;

namespace {
constexpr int kHands = 169;
constexpr int kPreflopCombos = 22100; // C(52,3)
constexpr int kRiverBuckets = 108345;
constexpr int kTurnBuckets = 161057;
constexpr int kFlopBuckets = 50921;
constexpr int kThreeBuckets = 8125;
constexpr int kBoardMaskCount = 1 << 16;
constexpr int kRiverMapSize = kBoardMaskCount * 162;
constexpr int kTurnMapSize = kBoardMaskCount * 33;
constexpr int kFlopMapSize = kBoardMaskCount * 181;
constexpr int kThreeMapSize = kBoardMaskCount * 77;

uintmax_t fileSizeSafe(const fs::path &path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    return ec ? 0 : size;
}

void printSizeLine(const std::string &label, const fs::path &in_path, const fs::path &out_path) {
    uintmax_t in_size = fileSizeSafe(in_path);
    uintmax_t out_size = fileSizeSafe(out_path);
    if (in_size == 0 || out_size == 0) {
        std::cout << label << ": wrote " << out_path.string() << "\n";
        return;
    }
    double ratio = static_cast<double>(out_size) / static_cast<double>(in_size);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << ratio * 100.0;
    std::cout << label << ": " << in_size << " -> " << out_size << " bytes ("
              << oss.str() << "%)\n";
}

bool loadEquity(const fs::path &path, std::vector<float> &data, size_t count) {
    std::ifstream inf(path, std::ios::binary);
    if (!inf) {
        std::cerr << "Error: could not open " << path.string() << "\n";
        return false;
    }
    data.resize(count);
    inf.read(reinterpret_cast<char *>(data.data()), data.size() * sizeof(float));
    if (!inf) {
        std::cerr << "Error: failed reading equity data from " << path.string() << "\n";
        return false;
    }
    return true;
}

bool writeEquityCompressed(const fs::path &path, uint32_t rows, uint32_t cols,
                           const std::vector<uint8_t> &packed) {
    std::ofstream ouf(path, std::ios::binary);
    if (!ouf) {
        std::cerr << "Error: could not open " << path.string() << " for writing\n";
        return false;
    }
    ouf.write(reinterpret_cast<const char *>(packed.data()), packed.size() * sizeof(uint8_t));
    if (!ouf) {
        std::cerr << "Error: failed writing " << path.string() << "\n";
        return false;
    }
    return true;
}

bool compressEquity(const fs::path &in_path, const fs::path &out_path, int rows, int cols) {
    size_t count = static_cast<size_t>(rows) * static_cast<size_t>(cols);
    std::vector<float> data;
    if (!loadEquity(in_path, data, count)) {
        return false;
    }

    std::vector<uint8_t> packed(count);
    for (size_t i = 0; i < data.size(); ++i) {
        float v = data[i];
        if (v == -1.0f) {
            packed[i] = 255;
            continue;
        }
        float qf = v * 254.0f;
        long q = std::lround(qf);
        if (q < 0) q = 0;
        if (q > 254) q = 254;
        packed[i] = static_cast<uint8_t>(q);
    }

    if (!writeEquityCompressed(out_path, static_cast<uint32_t>(rows),
                               static_cast<uint32_t>(cols), packed)) {
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char *argv[]) {
    fs::path input_dir = (argc >= 2) ? fs::path(argv[1]) : fs::path("emd_bucket_data");
    fs::path output_dir = (argc >= 3) ? fs::path(argv[2]) : (input_dir / "compressed");

    std::error_code ec;
    fs::create_directories(output_dir, ec);
    if (ec) {
        std::cerr << "Error: could not create output dir " << output_dir.string()
                  << " (" << ec.message() << ")\n";
        return 1;
    }

    bool ok = true;

    struct MapSpec {
        const char *name;
        int expected_buckets;
        size_t map_size;
    };

    struct EquitySpec {
        const char *name;
        int rows;
        int cols;
    };
    const EquitySpec equities[] = {
        {"river_equity.bin", kHands, kRiverBuckets},
        {"turn_equity.bin", kHands, kTurnBuckets},
        {"turn_equity_1.bin", 100, kTurnBuckets},
        {"turn_equity_2.bin", 69, kTurnBuckets},
        {"flop_equity.bin", kHands, kFlopBuckets},
        {"three_equity.bin", kHands, kThreeBuckets},
    };

    for (const auto &spec : equities) {
        fs::path in = input_dir / spec.name;
        fs::path out_name = fs::path(spec.name);
        out_name.replace_extension(".bin");
        fs::path out = output_dir / out_name;
        if (compressEquity(in, out, spec.rows, spec.cols)) {
            printSizeLine(spec.name, in, out);
        } else {
            ok = false;
        }
    }

    if (!ok) {
        std::cerr << "Compression failed.\n";
        return 1;
    }
    std::cout << "Done.\n";
    return 0;
}
#include <sstream>
