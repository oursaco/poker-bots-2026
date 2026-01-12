#include "game/ThreeCardBucket.hpp"
#include <cctype>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

struct BucketOptions {
    bool show_help = false;
    bool has_hand = false;
    bool has_board = false;
    uint64_t hand = 0;
    uint64_t board = 0;
    int hand_count = 0;
    int board_count = 0;
    int discard = 0;
};

int rankFromChar(char c){
    switch(static_cast<char>(toupper(static_cast<unsigned char>(c)))){
        case '2': return 0;
        case '3': return 1;
        case '4': return 2;
        case '5': return 3;
        case '6': return 4;
        case '7': return 5;
        case '8': return 6;
        case '9': return 7;
        case 'T': return 8;
        case 'J': return 9;
        case 'Q': return 10;
        case 'K': return 11;
        case 'A': return 12;
        default: return -1;
    }
}

int suitFromChar(char c){
    switch(static_cast<char>(tolower(static_cast<unsigned char>(c)))){
        case 's': return 0;
        case 'h': return 1;
        case 'd': return 2;
        case 'c': return 3;
        default: return -1;
    }
}

bool parseCardToken(const string& token, unsigned& card){
    if(token.empty()){
        return false;
    }
    char rank_char = token[0];
    char suit_char = token.size() > 1 ? token[1] : '\0';
    if(token.size() >= 3 && token[0] == '1' && token[1] == '0'){
        rank_char = 'T';
        suit_char = token[2];
    }
    int rank = rankFromChar(rank_char);
    int suit = suitFromChar(suit_char);
    if(rank < 0 || suit < 0){
        return false;
    }
    card = static_cast<unsigned>(rank * 4 + suit);
    return true;
}

vector<string> splitCardTokens(const string& text){
    vector<string> tokens;
    string current;
    for(char c : text){
        if(isspace(static_cast<unsigned char>(c)) || c == ','){
            if(!current.empty()){
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if(!current.empty()){
        tokens.push_back(current);
    }
    return tokens;
}

bool appendTokens(const string& text, vector<string>& out, int expected, string& error){
    vector<string> tokens = splitCardTokens(text);
    for(const auto& token : tokens){
        out.push_back(token);
        if(expected > 0 && out.size() > static_cast<size_t>(expected)){
            error = "too many cards provided";
            return false;
        }
    }
    return true;
}

bool collectCardTokens(int& i, int argc, char* argv[], int expected, vector<string>& out, string& error){
    while(i + 1 < argc && out.size() < static_cast<size_t>(expected)){
        string next = argv[i + 1];
        if(next.rfind("--", 0) == 0){
            break;
        }
        ++i;
        if(!appendTokens(next, out, expected, error)){
            return false;
        }
    }
    if(out.size() != static_cast<size_t>(expected)){
        error = "expected " + to_string(expected) + " cards";
        return false;
    }
    return true;
}

bool parseCardMask(const vector<string>& tokens, int expected, uint64_t& mask, string& error){
    if(tokens.size() != static_cast<size_t>(expected)){
        error = "expected " + to_string(expected) + " cards";
        return false;
    }
    mask = 0;
    for(const auto& token : tokens){
        unsigned card = 0;
        if(!parseCardToken(token, card)){
            error = "invalid card: " + token;
            return false;
        }
        uint64_t bit = 1ull << card;
        if(mask & bit){
            error = "duplicate card: " + token;
            return false;
        }
        mask |= bit;
    }
    return true;
}

bool parseFlagTokens(const string& flag, const string& arg, int& i, int argc, char* argv[],
    vector<string>& tokens, bool& matched, string& error){
    matched = false;
    string prefix = flag + "=";
    if(arg != flag && arg.rfind(prefix, 0) != 0){
        return true;
    }
    matched = true;
    if(!tokens.empty()){
        error = flag + " specified multiple times";
        return false;
    }
    if(arg == flag){
        while(i + 1 < argc){
            string next = argv[i + 1];
            if(next.rfind("--", 0) == 0){
                break;
            }
            ++i;
            if(!appendTokens(next, tokens, 0, error)){
                return false;
            }
        }
    } else {
        if(!appendTokens(arg.substr(prefix.size()), tokens, 0, error)){
            return false;
        }
    }
    if(tokens.empty()){
        error = "expected cards for " + flag;
        return false;
    }
    return true;
}

bool parseArgs(int argc, char* argv[], BucketOptions& options, string& error){
    vector<string> hand_tokens;
    vector<string> board_tokens;
    vector<string> loose_tokens;
    vector<string> discard_tokens;
    for(int i = 1; i < argc; ++i){
        string arg = argv[i];
        if(arg == "--help" || arg == "-h"){
            options.show_help = true;
            return true;
        }
        bool matched = false;
        if(!parseFlagTokens("--hand", arg, i, argc, argv, hand_tokens, matched, error)){
            return false;
        }
        if(matched){
            continue;
        }
        if(!parseFlagTokens("--board", arg, i, argc, argv, board_tokens, matched, error)){
            return false;
        }
        if(matched){
            continue;
        }
        if(!parseFlagTokens("--discard", arg, i, argc, argv, discard_tokens, matched, error)){
            return false;
        }
        if(matched){
            continue;
        }
        if(arg.rfind("--", 0) == 0){
            error = "unknown argument: " + arg;
            return false;
        }
        if(!appendTokens(arg, loose_tokens, 0, error)){
            return false;
        }
    }
    if(!hand_tokens.empty() && !loose_tokens.empty()){
        error = "hand provided multiple ways";
        return false;
    }
    if(hand_tokens.empty()){
        hand_tokens = loose_tokens;
    }
    if(hand_tokens.empty()){
        error = "expected hand cards";
        return false;
    }
    if(hand_tokens.size() < 2 || hand_tokens.size() > 3){
        error = "expected 2 or 3 cards for hand";
        return false;
    }
    if(!board_tokens.empty()){
        if(board_tokens.size() > 6 || board_tokens.size() == 1){
            error = "board must have 0, 2, 3, 4, 5, or 6 cards";
            return false;
        }
    }
    if(!parseCardMask(hand_tokens, static_cast<int>(hand_tokens.size()), options.hand, error)){
        return false;
    }
    options.hand_count = __builtin_popcountll(options.hand);
    if(!board_tokens.empty()){
        if(!parseCardMask(board_tokens, static_cast<int>(board_tokens.size()), options.board, error)){
            return false;
        }
        options.has_board = true;
    }
    options.board_count = __builtin_popcountll(options.board);
    uint64_t overlap = options.hand & options.board;
    if(options.board_count == 0){
        if(options.hand_count != 3){
            error = "preflop requires 3 hand cards";
            return false;
        }
    } else if(options.board_count == 2 || options.board_count == 3){
        if(options.hand_count != 3){
            error = "discard streets require 3 hand cards";
            return false;
        }
        if(overlap != 0){
            error = "hand overlaps board on discard streets";
            return false;
        }
    } else if(options.board_count == 4 || options.board_count == 5 || options.board_count == 6){
        int remaining = __builtin_popcountll(options.hand & ~options.board);
        if(remaining != 2){
            error = "hand must have 2 cards after removing any board overlap";
            return false;
        }
        if(options.hand_count == 2 && overlap != 0){
            error = "2-card hand cannot overlap the board";
            return false;
        }
    } else {
        error = "board must have 0, 2, 3, 4, 5, or 6 cards";
        return false;
    }
    options.has_hand = true;
    return true;
}

void printUsage(const char* exe){
    cout << "Usage: " << exe << " --hand \"As Kd Qh\"\n"
         << "       " << exe << " As Kd Qh\n"
         << "       " << exe << " --hand \"As Kd\" --board \"2s 3s 4d 5c\"\n"
         << "       " << exe << " --hand \"As Kd Qh\" --board \"2s 3s\"\n"
         << "Board card counts supported: 0, 2, 3, 4, 5, 6\n";
}

string cardToString(unsigned card){
    static const char* ranks = "23456789TJQKA";
    static const char suits[] = {'s', 'h', 'd', 'c'};
    string out;
    out += ranks[card/4];
    out += suits[card%4];
    return out;
}

string maskToString(uint64_t mask){
    string out;
    bool first = true;
    while(mask){
        unsigned card = __builtin_ctzll(mask);
        mask &= mask - 1;
        if(!first){
            out += " ";
        }
        out += cardToString(card);
        first = false;
    }
    if(first){
        return "(none)";
    }
    return out;
}

int main(int argc, char* argv[]){
    BucketOptions options;
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
    EHSThreeCardBucket bucket;
    bucket.readPreflop("./bucket_data/preflop.bin");
    if(options.board_count > 0){
        bucket.readMap("./bucket_data/ehs_map.bin");
        bucket.readTable("./bucket_data/three.bin", 0);
        bucket.readTable("./bucket_data/four.bin", 1);
        bucket.readTable("./bucket_data/five.bin", 2);
        bucket.readTable("./bucket_data/six.bin", 3);
    }
    uint64_t hand_for_bucket = options.hand;
    if(options.board_count >= 4 && options.hand_count == 2){
        hand_for_bucket |= options.board & (~options.board + 1);
    }
    string bucket_label;
    int bucket_id = -1;
    if(options.board_count == 0){
        bucket_label = "preflop";
        bucket_id = bucket.getPreflopBucket(hand_for_bucket);
    } else if(options.board_count == 2){
        bucket_label = "bb_discard";
        bucket_id = bucket.getBBDiscardBucket(options.board, hand_for_bucket);
    } else if(options.board_count == 3){
        bucket_label = "sb_discard";
        bucket_id = bucket.getSBDiscardBucket(options.board, hand_for_bucket, options.discard);
    } else if(options.board_count == 4){
        bucket_label = "flop";
        bucket_id = bucket.getFlopBucket(options.board, hand_for_bucket, options.discard);
    } else if(options.board_count == 5){
        bucket_label = "turn";
        bucket_id = bucket.getTurnBucket(options.board, hand_for_bucket, options.discard);
    } else if(options.board_count == 6){
        bucket_label = "river";
        bucket_id = bucket.getRiverBucket(options.board, hand_for_bucket, options.discard);
    }
    cout << "hand: " << maskToString(options.hand) << "\n";
    cout << "board: " << maskToString(options.board) << "\n";
    if(options.board_count >= 4){
        cout << "hand (post-discard): " << maskToString(options.hand & ~options.board) << "\n";
    }
    cout << bucket_label << " bucket: " << bucket_id << "\n";
    return 0;
}
