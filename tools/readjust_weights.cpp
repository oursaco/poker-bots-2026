#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "cfr/CFR.hpp"
#include "game/ThreeCard.hpp"
#include "game/ThreeCardBucket.hpp"
using namespace std;
namespace fs = std::filesystem;

struct CliOptions {
    string dir = "./checkpoints";
    string bucket_dir = "./emd_bucket_data";
    string out0;
    string out1;
    bool show_help = false;
};

void printUsage(const char* exe){
    cout << "Usage: " << exe << " [--dir PATH] [--bucket PATH] [--out0 PATH] [--out1 PATH]\n";
}

bool startsWith(const string& value, const string& prefix){
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const string& value, const string& suffix){
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool parseCheckpointFilename(const string& name, const string& prefix, long long& checkpoint){
    static const string suffix = ".bin";
    if(!startsWith(name, prefix) || !endsWith(name, suffix)){
        return false;
    }
    string number = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    if(number.empty()){
        return false;
    }
    for(char c : number){
        if(!isdigit(static_cast<unsigned char>(c))){
            return false;
        }
    }
    try{
        checkpoint = stoll(number);
    } catch(...){
        return false;
    }
    return true;
}

bool parseArgs(int argc, char* argv[], CliOptions& options, string& error){
    const string dir_prefix = "--dir=";
    const string bucket_prefix = "--bucket=";
    const string out0_prefix = "--out0=";
    const string out1_prefix = "--out1=";
    bool has_dir = false;
    for(int i = 1; i < argc; ++i){
        string arg = argv[i];
        if(arg == "--help" || arg == "-h"){
            options.show_help = true;
            return true;
        }
        if(arg == "--dir"){
            if(i + 1 >= argc){
                error = "missing value for --dir";
                return false;
            }
            options.dir = argv[++i];
            has_dir = true;
            continue;
        }
        if(arg == "--bucket"){
            if(i + 1 >= argc){
                error = "missing value for --bucket";
                return false;
            }
            options.bucket_dir = argv[++i];
            continue;
        }
        if(arg == "--out0"){
            if(i + 1 >= argc){
                error = "missing value for --out0";
                return false;
            }
            options.out0 = argv[++i];
            continue;
        }
        if(arg == "--out1"){
            if(i + 1 >= argc){
                error = "missing value for --out1";
                return false;
            }
            options.out1 = argv[++i];
            continue;
        }
        if(arg.rfind(dir_prefix, 0) == 0){
            options.dir = arg.substr(dir_prefix.size());
            has_dir = true;
            continue;
        }
        if(arg.rfind(bucket_prefix, 0) == 0){
            options.bucket_dir = arg.substr(bucket_prefix.size());
            continue;
        }
        if(arg.rfind(out0_prefix, 0) == 0){
            options.out0 = arg.substr(out0_prefix.size());
            continue;
        }
        if(arg.rfind(out1_prefix, 0) == 0){
            options.out1 = arg.substr(out1_prefix.size());
            continue;
        }
        if(arg.rfind("-", 0) == 0){
            error = "unknown argument: " + arg;
            return false;
        }
        if(!has_dir){
            options.dir = arg;
            has_dir = true;
            continue;
        }
        error = "unexpected argument: " + arg;
        return false;
    }
    return true;
}

struct CheckpointPaths {
    vector<long long> checkpoints;
    vector<string> player0_paths;
    vector<string> player1_paths;
    vector<long long> missing_player0;
    vector<long long> missing_player1;
};

bool gatherCheckpointPaths(const fs::path& directory, CheckpointPaths& result, string& error){
    if(!fs::exists(directory) || !fs::is_directory(directory)){
        error = "directory not found: " + directory.string();
        return false;
    }

    map<long long, fs::path> player0_map;
    map<long long, fs::path> player1_map;

    for(const auto& entry : fs::directory_iterator(directory)){
        if(!entry.is_regular_file()){
            continue;
        }
        string name = entry.path().filename().string();
        long long checkpoint = 0;
        if(parseCheckpointFilename(name, "player0_", checkpoint)){
            player0_map[checkpoint] = entry.path();
            continue;
        }
        if(parseCheckpointFilename(name, "player1_", checkpoint)){
            player1_map[checkpoint] = entry.path();
            continue;
        }
    }

    for(const auto& item : player0_map){
        long long checkpoint = item.first;
        auto it = player1_map.find(checkpoint);
        if(it == player1_map.end()){
            result.missing_player1.push_back(checkpoint);
            continue;
        }
        result.checkpoints.push_back(checkpoint);
        result.player0_paths.push_back(item.second.string());
        result.player1_paths.push_back(it->second.string());
    }

    for(const auto& item : player1_map){
        if(player0_map.find(item.first) == player0_map.end()){
            result.missing_player0.push_back(item.first);
        }
    }

    if(result.checkpoints.empty()){
        error = "no paired checkpoints found in " + directory.string();
        return false;
    }
    return true;
}

void resetPolicy(DCFRPolicyDouble& policy){
    for(int i = 0; i < POLICY_SZ; ++i){
        policy.strategy_sum[i] = 0.0;
        policy.regret_sum[i] = 0.0;
    }
}

bool buildAveragedDeltaPolicy(const vector<string>& checkpoint_paths,
                              DCFRPolicyDouble& output,
                              DCFRPolicy& prev,
                              DCFRPolicy& curr,
                              string& error){
    if(checkpoint_paths.size() < 2){
        error = "need at least two checkpoints to compute deltas";
        return false;
    }
    resetPolicy(output);
    const size_t last = checkpoint_paths.size() - 1;
    for(size_t i = 0; i < last; ++i){
        prev.loadPolicy(checkpoint_paths[i]);
        curr.loadPolicy(checkpoint_paths[i + 1]);
        for(int idx = 0; idx < output.state_count; ++idx){
            float diff = curr.strategy_sum[idx] - prev.strategy_sum[idx];
            assert(diff >= 0.0f);
            output.strategy_sum[idx] += double(diff)*double(diff)*double(diff);
        }
    }
    double denom = last;
    for(int idx = 0; idx < output.state_count; ++idx){
        output.strategy_sum[idx] /= denom;
    }
    return true;
}

int main(int argc, char* argv[]){
    CliOptions options;
    string error;
    if(!parseArgs(argc, argv, options, error)){
        if(!error.empty()){
            cerr << error << "\n";
        }
        printUsage(argv[0]);
        return 1;
    }
    if(options.show_help){
        printUsage(argv[0]);
        return 0;
    }

    fs::path checkpoint_dir = options.dir;
    if(options.out0.empty()){
        options.out0 = (checkpoint_dir / "player0_readjusted.bin").string();
    }
    if(options.out1.empty()){
        options.out1 = (checkpoint_dir / "player1_readjusted.bin").string();
    }

    CheckpointPaths paths;
    if(!gatherCheckpointPaths(options.dir, paths, error)){
        if(!error.empty()){
            cerr << error << "\n";
        }
        return 1;
    }

    DynamicThreeCardBucket bucket;
    bucket.init(options.bucket_dir);
    ThreeCardGameTree tree;
    tree.setBucket(&bucket);
    tree.init();

    DCFRPolicyDouble output0;
    DCFRPolicyDouble output1;
    DCFRPolicy prev;
    DCFRPolicy curr;
    output0.initPolicy(&tree);
    output1.initPolicy(&tree);
    prev.initPolicy(&tree);
    curr.initPolicy(&tree);

    if(!buildAveragedDeltaPolicy(paths.player0_paths, output0, prev, curr, error)){
        if(!error.empty()){
            cerr << "player0: " << error << "\n";
        }
        return 1;
    }
    if(!buildAveragedDeltaPolicy(paths.player1_paths, output1, prev, curr, error)){
        if(!error.empty()){
            cerr << "player1: " << error << "\n";
        }
        return 1;
    }

    output0.savePolicy(options.out0);
    output1.savePolicy(options.out1);

    cout << "Checkpoint dir: " << options.dir << "\n";
    cout << "Paired checkpoints: " << paths.checkpoints.size() << "\n";
    for(size_t i = 0; i < paths.checkpoints.size(); ++i){
        cout << "checkpoint " << paths.checkpoints[i] << "\n";
        cout << "  player0: " << paths.player0_paths[i] << "\n";
        cout << "  player1: " << paths.player1_paths[i] << "\n";
    }
    cout << "Player0 output: " << options.out0 << "\n";
    cout << "Player1 output: " << options.out1 << "\n";

    if(!paths.missing_player0.empty()){
        cerr << "Missing player0 checkpoints:";
        for(long long checkpoint : paths.missing_player0){
            cerr << " " << checkpoint;
        }
        cerr << "\n";
    }
    if(!paths.missing_player1.empty()){
        cerr << "Missing player1 checkpoints:";
        for(long long checkpoint : paths.missing_player1){
            cerr << " " << checkpoint;
        }
        cerr << "\n";
    }

    return 0;
}
