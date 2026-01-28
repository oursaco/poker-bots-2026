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
//    then uint16[count] values quantized linearly into [min, max].

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

bool loadPreflop(const fs::path &path, int32_t &num_buckets, std::vector<uint32_t> &data) {
    std::ifstream inf(path, std::ios::binary);
    if (!inf) {
        std::cerr << "Error: could not open " << path.string() << "\n";
        return false;
    }
    inf.read(reinterpret_cast<char *>(&num_buckets), sizeof(int32_t));
    if (!inf) {
        std::cerr << "Error: failed reading header from " << path.string() << "\n";
        return false;
    }
    data.resize(kPreflopCombos);
    inf.read(reinterpret_cast<char *>(data.data()), data.size() * sizeof(uint32_t));
    if (!inf) {
        std::cerr << "Error: failed reading preflop data from " << path.string() << "\n";
        return false;
    }
    return true;
}

bool writePreflopCompressed(const fs::path &path, int32_t num_buckets, const std::vector<uint32_t> &data) {
    std::ofstream ouf(path, std::ios::binary);
    if (!ouf) {
        std::cerr << "Error: could not open " << path.string() << " for writing\n";
        return false;
    }
    uint32_t count = static_cast<uint32_t>(data.size());
    ouf.write(reinterpret_cast<const char *>(&num_buckets), sizeof(int32_t));
    ouf.write(reinterpret_cast<const char *>(&count), sizeof(uint32_t));
    std::vector<uint16_t> packed(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > std::numeric_limits<uint16_t>::max()) {
            std::cerr << "Error: preflop value out of range: " << data[i] << "\n";
            return false;
        }
        packed[i] = static_cast<uint16_t>(data[i]);
    }
    ouf.write(reinterpret_cast<const char *>(packed.data()), packed.size() * sizeof(uint16_t));
    if (!ouf) {
        std::cerr << "Error: failed writing " << path.string() << "\n";
        return false;
    }
    return true;
}

bool loadEncodingMap(const fs::path &path, int32_t &num_buckets, std::vector<int32_t> &data, size_t count) {
    std::ifstream inf(path, std::ios::binary);
    if (!inf) {
        std::cerr << "Error: could not open " << path.string() << "\n";
        return false;
    }
    inf.read(reinterpret_cast<char *>(&num_buckets), sizeof(int32_t));
    if (!inf) {
        std::cerr << "Error: failed reading header from " << path.string() << "\n";
        return false;
    }
    data.resize(count);
    inf.read(reinterpret_cast<char *>(data.data()), data.size() * sizeof(int32_t));
    if (!inf) {
        std::cerr << "Error: failed reading encoding map from " << path.string() << "\n";
        return false;
    }
    return true;
}

bool writeEncodingMapCompressed(const fs::path &path, int32_t num_buckets,
                                const std::vector<int32_t> &data, int bytes_per_entry) {
    if (bytes_per_entry != 2 && bytes_per_entry != 3) {
        std::cerr << "Error: invalid bytes_per_entry=" << bytes_per_entry << "\n";
        return false;
    }
    std::ofstream ouf(path, std::ios::binary);
    if (!ouf) {
        std::cerr << "Error: could not open " << path.string() << " for writing\n";
        return false;
    }
    uint32_t count = static_cast<uint32_t>(data.size());
    ouf.write(reinterpret_cast<const char *>(&num_buckets), sizeof(int32_t));
    ouf.write(reinterpret_cast<const char *>(&count), sizeof(uint32_t));
    uint8_t bpe = static_cast<uint8_t>(bytes_per_entry);
    uint8_t pad[3] = {0, 0, 0};
    ouf.write(reinterpret_cast<const char *>(&bpe), sizeof(uint8_t));
    ouf.write(reinterpret_cast<const char *>(pad), sizeof(pad));

    if (bytes_per_entry == 2) {
        std::vector<uint16_t> packed(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            int32_t v = data[i];
            packed[i] = (v < 0) ? 0xFFFFu : static_cast<uint16_t>(v);
        }
        ouf.write(reinterpret_cast<const char *>(packed.data()), packed.size() * sizeof(uint16_t));
    } else {
        std::vector<uint8_t> packed(data.size() * 3);
        for (size_t i = 0; i < data.size(); ++i) {
            int32_t v = data[i];
            uint32_t enc = (v < 0) ? 0xFFFFFFu : static_cast<uint32_t>(v);
            size_t base = i * 3;
            packed[base] = static_cast<uint8_t>(enc & 0xFFu);
            packed[base + 1] = static_cast<uint8_t>((enc >> 8) & 0xFFu);
            packed[base + 2] = static_cast<uint8_t>((enc >> 16) & 0xFFu);
        }
        ouf.write(reinterpret_cast<const char *>(packed.data()), packed.size());
    }

    if (!ouf) {
        std::cerr << "Error: failed writing " << path.string() << "\n";
        return false;
    }
    return true;
}

bool compressEncodingMap(const fs::path &in_path, const fs::path &out_path,
                         int expected_buckets, size_t map_size) {
    int32_t num_buckets = 0;
    std::vector<int32_t> data;
    if (!loadEncodingMap(in_path, num_buckets, data, map_size)) {
        return false;
    }
    if (expected_buckets > 0 && num_buckets != expected_buckets) {
        std::cerr << "Warning: " << in_path.filename().string()
                  << " bucket count " << num_buckets
                  << " (expected " << expected_buckets << ")\n";
    }
    int32_t max_val = -1;
    int32_t min_val = 0;
    for (int32_t v : data) {
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }
    if (min_val < -1) {
        std::cerr << "Error: unexpected encoding map value < -1 in "
                  << in_path.filename().string() << "\n";
        return false;
    }

    int bytes_per_entry = (max_val <= 65534) ? 2 : 3;
    if (bytes_per_entry == 3 && max_val > 0xFFFFFE) {
        std::cerr << "Error: encoding map value too large for 24-bit: " << max_val << "\n";
        return false;
    }

    if (!writeEncodingMapCompressed(out_path, num_buckets, data, bytes_per_entry)) {
        return false;
    }
    std::cout << in_path.filename().string() << ": packed to " << bytes_per_entry
              << " bytes/entry\n";
    return true;
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
                           float min_v, float max_v, const std::vector<uint16_t> &packed) {
    std::ofstream ouf(path, std::ios::binary);
    if (!ouf) {
        std::cerr << "Error: could not open " << path.string() << " for writing\n";
        return false;
    }
    ouf.write(reinterpret_cast<const char *>(&rows), sizeof(uint32_t));
    ouf.write(reinterpret_cast<const char *>(&cols), sizeof(uint32_t));
    ouf.write(reinterpret_cast<const char *>(&min_v), sizeof(float));
    ouf.write(reinterpret_cast<const char *>(&max_v), sizeof(float));
    ouf.write(reinterpret_cast<const char *>(packed.data()), packed.size() * sizeof(uint16_t));
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
    float min_v = std::numeric_limits<float>::infinity();
    float max_v = -std::numeric_limits<float>::infinity();
    for (float v : data) {
        if (!std::isfinite(v)) continue;
        min_v = std::min(min_v, v);
        max_v = std::max(max_v, v);
    }
    if (!std::isfinite(min_v) || !std::isfinite(max_v)) {
        std::cerr << "Error: non-finite equity values in " << in_path.filename().string() << "\n";
        return false;
    }
    if (max_v < min_v) {
        std::cerr << "Error: invalid min/max for " << in_path.filename().string() << "\n";
        return false;
    }
    float range = max_v - min_v;
    float scale = (range > 0.0f) ? (range / 65535.0f) : 0.0f;

    std::vector<uint16_t> packed(count);
    for (size_t i = 0; i < data.size(); ++i) {
        float v = data[i];
        if (!std::isfinite(v)) v = min_v;
        float qf = (scale == 0.0f) ? 0.0f : (v - min_v) / scale;
        long q = std::lround(qf);
        if (q < 0) q = 0;
        if (q > 65535) q = 65535;
        packed[i] = static_cast<uint16_t>(q);
    }

    if (!writeEquityCompressed(out_path, static_cast<uint32_t>(rows),
                               static_cast<uint32_t>(cols), min_v, max_v, packed)) {
        return false;
    }
    std::cout << in_path.filename().string() << ": min=" << min_v << " max=" << max_v << "\n";
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

    {
        fs::path in = input_dir / "preflop.bin";
        fs::path out = output_dir / "preflop.cbin";
        int32_t num_buckets = 0;
        std::vector<uint32_t> data;
        if (loadPreflop(in, num_buckets, data) && writePreflopCompressed(out, num_buckets, data)) {
            printSizeLine("preflop.bin", in, out);
        } else {
            ok = false;
        }
    }

    struct MapSpec {
        const char *name;
        int expected_buckets;
        size_t map_size;
    };
    const MapSpec maps[] = {
        {"river_encoding_map.bin", kRiverBuckets, kRiverMapSize},
        {"turn_encoding_map.bin", kTurnBuckets, kTurnMapSize},
        {"flop_encoding_map.bin", kFlopBuckets, kFlopMapSize},
        {"three_encoding_map.bin", kThreeBuckets, kThreeMapSize},
    };

    for (const auto &spec : maps) {
        fs::path in = input_dir / spec.name;
        fs::path out_name = fs::path(spec.name);
        out_name.replace_extension(".cbin");
        fs::path out = output_dir / out_name;
        if (compressEncodingMap(in, out, spec.expected_buckets, spec.map_size)) {
            printSizeLine(spec.name, in, out);
        } else {
            ok = false;
        }
    }

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
        out_name.replace_extension(".cbin");
        fs::path out = output_dir / out_name;
        if (compressEquity(in, out, spec.rows, spec.cols)) {
            printSizeLine(spec.name, in, out);
        } else {
            ok = false;
        }
    }

    std::unordered_set<std::string> handled = {
        "preflop.bin",
        "river_encoding_map.bin",
        "turn_encoding_map.bin",
        "flop_encoding_map.bin",
        "three_encoding_map.bin",
        "river_equity.bin",
        "turn_equity.bin",
        "turn_equity_1.bin",
        "turn_equity_2.bin",
        "flop_equity.bin",
        "three_equity.bin",
    };
    for (const auto &entry : fs::directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".bin") continue;
        std::string name = entry.path().filename().string();
        if (handled.find(name) == handled.end()) {
            std::cerr << "Warning: unhandled .bin file: " << name << "\n";
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
