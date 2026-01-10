#include "game/GameTree.hpp"
#include "game/ThreeCard.hpp"
#include "game/Poker.hpp"
#include "external/omp/Random.h"
#include "constants/constants.h"
#include "cfr/CFR.hpp"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

struct UtilityOptions {
    bool show_help = false;
    bool has_sb = false;
    bool has_bb = false;
    bool has_flop = false;
    bool has_turn = false;
    bool has_river = false;
    uint64_t sb_hand = 0;
    uint64_t bb_hand = 0;
    uint64_t flop = 0;
    uint64_t turn = 0;
    uint64_t river = 0;
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

bool parseCardListForFlag(const string& flag, const string& arg, int& i, int argc, char* argv[], int expected,
    bool& has_value, uint64_t& out_mask, bool& matched, string& error){
    matched = false;
    string prefix = flag + "=";
    if(arg != flag && arg.rfind(prefix, 0) != 0){
        return true;
    }
    matched = true;
    if(has_value){
        error = flag + " specified multiple times";
        return false;
    }
    vector<string> tokens;
    if(arg == flag){
        if(!collectCardTokens(i, argc, argv, expected, tokens, error)){
            error = "expected " + to_string(expected) + " cards after " + flag;
            return false;
        }
    } else {
        if(!appendTokens(arg.substr(prefix.size()), tokens, expected, error)){
            return false;
        }
        if(tokens.size() != static_cast<size_t>(expected)){
            error = "expected " + to_string(expected) + " cards for " + flag;
            return false;
        }
    }
    if(!parseCardMask(tokens, expected, out_mask, error)){
        error = flag + ": " + error;
        return false;
    }
    has_value = true;
    return true;
}

bool parseArgs(int argc, char* argv[], UtilityOptions& options, string& error){
    bool has_game = false;
    for(int i = 1; i < argc; ++i){
        string arg = argv[i];
        if(arg == "--help" || arg == "-h"){
            options.show_help = true;
            return true;
        }
    }
    int sb_count = 3;
    int flop_count = 2;
    for(int i = 1; i < argc; ++i){
        string arg = argv[i];
        if(arg == "--help" || arg == "-h"){
            options.show_help = true;
            return true;
        }
        bool matched = false;
        if(!parseCardListForFlag("--sb", arg, i, argc, argv, sb_count, options.has_sb, options.sb_hand, matched, error)){
            return false;
        }
        if(matched) continue;
        if(!parseCardListForFlag("--bb", arg, i, argc, argv, sb_count, options.has_bb, options.bb_hand, matched, error)){
            return false;
        }
        if(matched) continue;
        if(!parseCardListForFlag("--flop", arg, i, argc, argv, flop_count, options.has_flop, options.flop, matched, error)){
            return false;
        }
        if(matched) continue;
        if(!parseCardListForFlag("--turn", arg, i, argc, argv, 1, options.has_turn, options.turn, matched, error)){
            return false;
        }
        if(matched) continue;
        if(!parseCardListForFlag("--river", arg, i, argc, argv, 1, options.has_river, options.river, matched, error)){
            return false;
        }
        if(matched) continue;
        error = "unknown argument: " + arg;
        return false;
    }
    uint64_t combined = 0;
    auto mergeMask = [&](uint64_t mask, const char* label) -> bool {
        if((combined & mask) != 0){
            error = string("card overlap in ") + label;
            return false;
        }
        combined |= mask;
        return true;
    };
    if(options.has_sb && !mergeMask(options.sb_hand, "sb hand")) return false;
    if(options.has_bb && !mergeMask(options.bb_hand, "bb hand")) return false;
    if(options.has_flop && !mergeMask(options.flop, "flop")) return false;
    if(options.has_turn && !mergeMask(options.turn, "turn")) return false;
    if(options.has_river && !mergeMask(options.river, "river")) return false;
    return true;
}

void printUsage(const char* exe){
    cout << "Usage: " << exe
         << " [--sb \"As Kd\"] [--bb \"9c 8d\"] [--flop \"2s 3s 4d\"] [--turn \"5c\"] [--river \"6h\"]\n"
         << "  --sb \"As Kd Qh\" --bb \"9c 8d 7h\" --flop \"2s 3s\" --turn \"4d\" --river \"5c\"\n";
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

struct HoleCards {
    uint64_t sb_hand;
    uint64_t bb_hand;
};

int ind = 0;

void visualizeUtility(GameState* state, const string& prefix, array<float, TRAINER_SZ> &utility){
    auto actions = state->generateActions();
    if(actions.empty()){
        cout << prefix << "(no actions)\n";
        return;
    }
    for(int i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();
        if(!action->isWorldAction()){
            ind++;
        }

        cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString() << " | utility: " << utility[ind] << " ";
        if(is_terminal){
            cout << " [terminal, winner: " << next_state->getWinner() << "]";
        }
        cout << "\n";

        if(!is_terminal){
            visualizeUtility(next_state, prefix + (is_last ? "    " : "|   "), utility);
        }
    }
}

void visualizeUtilityDepthLimited(GameState* state, const string& prefix, array<float, TRAINER_SZ> &utility, int depth){
    auto actions = state->generateActions();
    if(actions.empty()){
        cout << prefix << "(no actions)\n";
        return;
    }
    if(depth == 0){
        cout << prefix << "(depth limited)\n";
    }
    for(int i = 0; i < actions.size(); ++i){
        bool is_last = (i + 1 == actions.size());
        auto* next_state = actions[i].first.get();
        auto& action = actions[i].second;
        bool is_terminal = next_state->isTerminal();
        if(!action->isWorldAction()){
            ind++;
        }

        if(depth > 0){
            cout << prefix << (is_last ? "\\-- " : "|-- ") << action->toString() << " | utility: " << utility[ind] << " ";
            if(is_terminal){
                cout << " [terminal, winner: " << next_state->getWinner() << "]";
            }
            cout << "\n";
        }

        if(!is_terminal){
            visualizeUtilityDepthLimited(next_state, prefix + (is_last ? "    " : "|   "), utility, depth - 1);
        }
    }
}

void visualizeThreeCardUtility(const UtilityOptions& options){
    ThreeCardGameTree tree;
    EHSThreeCardBucket bucket;
    bucket.init("./bucket_data");
    tree.setBucket(&bucket);
    tree.init();
    srand(NULL);
    omp::XoroShiro128Plus rng(rand());
    omp::FastUniformIntDistribution2<int> card_generator(0, 51);
    uint64_t used_cards = 0;
    auto generateCard = [&](uint64_t &used_mask){
        unsigned card;
        uint64_t card_mask;
        do {
            card = card_generator(rng);
            card_mask = 1ull << card;
        } while ((used_cards & card_mask));
        used_mask |= card_mask;
        return card;
    };
    if(options.has_sb){
        uint64_t sb_hand = options.sb_hand;
        while(__builtin_popcountll(sb_hand) < 3){
            sb_hand |= 1ull << generateCard(used_cards);
        }
        tree.setFixedSbHand(sb_hand);
        used_cards |= sb_hand;
    }
    if(options.has_bb){
        uint64_t bb_hand = options.bb_hand;
        while(__builtin_popcountll(bb_hand) < 3){
            bb_hand |= 1ull << generateCard(used_cards);
        }
        tree.setFixedBbHand(bb_hand);
        used_cards |= bb_hand;
    }
    if(options.has_flop){
        uint64_t flop = options.flop;
        while(__builtin_popcountll(flop) < 2){
            flop |= 1ull << generateCard(used_cards);
        }
        tree.setFixedFlop(flop);
        used_cards |= flop;
    }
    if(options.has_turn){
        tree.setFixedTurn(options.turn);
        used_cards |= options.turn;
    }
    if(options.has_river){
        tree.setFixedRiver(options.river);
        used_cards |= options.river;
    }
    if(!options.has_sb){
        tree.setFixedSbHand(1ull << generateCard(used_cards) | 1ull << generateCard(used_cards) | 1ull << generateCard(used_cards));
    }
    if(!options.has_bb){
        tree.setFixedBbHand(1ull << generateCard(used_cards) | 1ull << generateCard(used_cards) | 1ull << generateCard(used_cards));
    }
    if(!options.has_flop){
        tree.setFixedFlop(1ull << generateCard(used_cards) | 1ull << generateCard(used_cards));
    }
    if(!options.has_turn){
        tree.setFixedTurn(1ull << generateCard(used_cards));
    }
    if(!options.has_river){
        tree.setFixedRiver(1ull << generateCard(used_cards));
    }
    tree.prepare(42);
    DCFRTrainer trainer;
    trainer.setTree(&tree);
    trainer.players[0].initPolicy(&tree);
    trainer.players[1].initPolicy(&tree);
    trainer.updateUtility();
    trainer.updatePlayer(0, 0, 0.0f, 0.0f, 0.0f);
    HoleCards hole_cards = {tree.fixed_sb_hand, tree.fixed_bb_hand};
    cout << "sb hole: " << maskToString(hole_cards.sb_hand) << "\n";
    cout << "bb hole: " << maskToString(hole_cards.bb_hand) << "\n";
    cout << "runout: flop " << maskToString(tree.fixed_flop) << " | turn " << maskToString(tree.fixed_turn) << " | river " << maskToString(tree.fixed_river) << "\n";
    cout << "\n";
    ThreeCardGameState root = ThreeCardGameState();
    ind = 0;
    visualizeUtilityDepthLimited(&root, "", trainer.utility, 4);
}

int main(int argc, char* argv[]){
    UtilityOptions options;
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
    visualizeThreeCardUtility(options);
    return 0;
}
