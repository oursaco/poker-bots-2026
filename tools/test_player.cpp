#include "player/ThreeCardPlayer.hpp"
#include "game/ThreeCardBucket.hpp"
#include "game/ThreeCardState.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

struct CliOptions {
    string policy_path = "./final_model/player.bin";
    string seat = "random";
    bool show_help = false;
    bool has_seed = false;
    unsigned int seed = 0;
    int max_hands = 0;
};

string trim(const string& input){
    size_t start = input.find_first_not_of(" \t\r\n");
    if(start == string::npos){
        return "";
    }
    size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

string toLower(string value){
    for(char& c : value){
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool parseIndex(const string& input, int max_value, int& index){
    if(input.empty()){
        return false;
    }
    for(char c : input){
        if(!isdigit(static_cast<unsigned char>(c))){
            return false;
        }
    }
    int value = stoi(input);
    if(value < 1 || value > max_value){
        return false;
    }
    index = value - 1;
    return true;
}

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

string cardToString(unsigned card){
    static const char* ranks = "23456789TJQKA";
    static const char suits[] = {'s', 'h', 'd', 'c'};
    string out;
    out += ranks[card/4];
    out += suits[card%4];
    return out;
}

vector<unsigned> maskToCards(uint64_t mask){
    vector<unsigned> cards;
    while(mask){
        unsigned card = __builtin_ctzll(mask);
        cards.push_back(card);
        mask &= mask - 1;
    }
    return cards;
}

string maskToString(uint64_t mask){
    vector<unsigned> cards = maskToCards(mask);
    if(cards.empty()){
        return "(none)";
    }
    string out;
    for(size_t i = 0; i < cards.size(); ++i){
        if(i > 0){
            out += " ";
        }
        out += cardToString(cards[i]);
    }
    return out;
}

string seatName(int player_id){
    return player_id == 0 ? "sb" : "bb";
}

string streetName(const ThreeCardGameState& state){
    switch(state.street){
        case 0: return "preflop";
        case 1: return "deal flop";
        case 2: return "bb discard";
        case 3: return "sb discard";
        case 4: return "flop betting";
        case 5: return "turn betting";
        case 6: return "river betting";
        default: return "unknown";
    }
}

void printUsage(const char* exe){
    cout << "Usage: " << exe
         << " [--policy PATH] [--seat sb|bb|random] [--seed N] [--hands N]\n";
}

bool parseArgs(int argc, char* argv[], CliOptions& options, string& error){
    const string policy_prefix = "--policy=";
    const string seat_prefix = "--seat=";
    const string seed_prefix = "--seed=";
    const string hands_prefix = "--hands=";
    for(int i = 1; i < argc; ++i){
        string arg = argv[i];
        if(arg == "--help" || arg == "-h"){
            options.show_help = true;
            return true;
        }
        if(arg == "--policy"){
            if(i + 1 >= argc){
                error = "missing value for --policy";
                return false;
            }
            options.policy_path = argv[++i];
            continue;
        }
        if(arg.rfind(policy_prefix, 0) == 0){
            options.policy_path = arg.substr(policy_prefix.size());
            continue;
        }
        if(arg == "--seat"){
            if(i + 1 >= argc){
                error = "missing value for --seat";
                return false;
            }
            options.seat = toLower(argv[++i]);
            continue;
        }
        if(arg.rfind(seat_prefix, 0) == 0){
            options.seat = toLower(arg.substr(seat_prefix.size()));
            continue;
        }
        if(arg == "--seed"){
            if(i + 1 >= argc){
                error = "missing value for --seed";
                return false;
            }
            try{
                options.seed = static_cast<unsigned int>(stoul(argv[++i]));
                options.has_seed = true;
            } catch(const exception&){
                error = "invalid value for --seed";
                return false;
            }
            continue;
        }
        if(arg.rfind(seed_prefix, 0) == 0){
            try{
                options.seed = static_cast<unsigned int>(stoul(arg.substr(seed_prefix.size())));
                options.has_seed = true;
            } catch(const exception&){
                error = "invalid value for --seed";
                return false;
            }
            continue;
        }
        if(arg == "--hands"){
            if(i + 1 >= argc){
                error = "missing value for --hands";
                return false;
            }
            try{
                options.max_hands = stoi(argv[++i]);
            } catch(const exception&){
                error = "invalid value for --hands";
                return false;
            }
            continue;
        }
        if(arg.rfind(hands_prefix, 0) == 0){
            try{
                options.max_hands = stoi(arg.substr(hands_prefix.size()));
            } catch(const exception&){
                error = "invalid value for --hands";
                return false;
            }
            continue;
        }
        error = "unknown argument: " + arg;
        return false;
    }
    if(options.seat != "sb" && options.seat != "bb" && options.seat != "random"){
        error = "invalid seat: " + options.seat;
        return false;
    }
    if(options.max_hands < 0){
        error = "invalid value for --hands";
        return false;
    }
    return true;
}

struct Deck {
    vector<unsigned> cards;
    size_t index = 0;

    explicit Deck(std::mt19937& rng){
        cards.resize(52);
        for(unsigned i = 0; i < 52; ++i){
            cards[i] = i;
        }
        shuffle(cards.begin(), cards.end(), rng);
    }

    unsigned draw(){
        if(index >= cards.size()){
            return 0;
        }
        return cards[index++];
    }
};

int findUniqueActionIndex(const vector<pair<unique_ptr<GameState>, unique_ptr<Action>>>& actions, const string& name){
    int found = -1;
    int matches = 0;
    for(int i = 0; i < static_cast<int>(actions.size()); ++i){
        auto* action = dynamic_cast<ThreeCardAction*>(actions[i].second.get());
        if(action && action->action == name){
            found = i;
            matches++;
        }
    }
    if(matches == 1){
        return found;
    }
    if(matches > 1){
        return -2;
    }
    return -1;
}

int promptForActionIndex(const vector<pair<unique_ptr<GameState>, unique_ptr<Action>>>& actions, bool& quit){
    while(true){
        cout << "Choose action (1-" << actions.size() << ", call/fold/raise, q to quit): " << flush;
        string line;
        if(!getline(cin, line)){
            quit = true;
            return -1;
        }
        string cleaned = toLower(trim(line));
        if(cleaned.empty()){
            continue;
        }
        if(cleaned == "q" || cleaned == "quit"){
            quit = true;
            return -1;
        }
        int index = -1;
        if(parseIndex(cleaned, static_cast<int>(actions.size()), index)){
            return index;
        }
        string keyword = cleaned;
        if(keyword == "c" || keyword == "call" || keyword == "check" || keyword == "k"){
            keyword = "call";
        } else if(keyword == "f" || keyword == "fold"){
            keyword = "fold";
        } else if(keyword == "r" || keyword == "raise"){
            keyword = "raise";
        } else {
            cout << "Unknown action.\n";
            continue;
        }
        int found = findUniqueActionIndex(actions, keyword);
        if(found >= 0){
            return found;
        }
        if(found == -2){
            cout << "Multiple " << keyword << " options. Choose a number.\n";
            continue;
        }
        cout << "Action not available.\n";
    }
}

bool promptForDiscardChoice(const vector<unsigned>& cards, int& action_index, unsigned& card, bool& quit){
    while(true){
        cout << "Discard a card (1-" << cards.size() << " or card like As, q to quit): " << flush;
        string line;
        if(!getline(cin, line)){
            quit = true;
            return false;
        }
        string cleaned = trim(line);
        if(cleaned.empty()){
            continue;
        }
        string lower = toLower(cleaned);
        if(lower == "q" || lower == "quit"){
            quit = true;
            return false;
        }
        int index = -1;
        if(parseIndex(lower, static_cast<int>(cards.size()), index)){
            action_index = index;
            card = cards[index];
            return true;
        }
        istringstream iss(cleaned);
        string token;
        iss >> token;
        unsigned parsed = 0;
        if(parseCardToken(token, parsed)){
            for(size_t i = 0; i < cards.size(); ++i){
                if(cards[i] == parsed){
                    action_index = static_cast<int>(i);
                    card = parsed;
                    return true;
                }
            }
            cout << "Card not in hand.\n";
            continue;
        }
        cout << "Invalid choice.\n";
    }
}

int choosePolicyActionIndex(ThreeCardPlayer& bot, std::mt19937& rng){
    vector<float> probs = bot.getActionProbabilities();
    if(probs.empty()){
        return -1;
    }
    uniform_real_distribution<float> dist(0.0f, 1.0f);
    float roll = dist(rng);
    float cum = 0.0f;
    for(int i = 0; i < static_cast<int>(probs.size()); ++i){
        cum += probs[i];
        if(roll <= cum){
            return i;
        }
    }
    return static_cast<int>(probs.size()) - 1;
}

bool playHand(ThreeCardPlayer& bot, NaiveThreeCardBucket& bucket, std::mt19937& rng, int human_id, int hand_number){
    Deck deck(rng);
    uint64_t sb_hand = 0;
    uint64_t bb_hand = 0;
    uint64_t board = 0;
    for(int i = 0; i < 3; ++i){
        sb_hand |= 1ull << deck.draw();
        bb_hand |= 1ull << deck.draw();
    }

    int bot_id = human_id ^ 1;
    uint64_t bot_hand = (bot_id == 0) ? sb_hand : bb_hand;
    bot.startRound(bot_id, bot_hand);
    bot.state = ThreeCardGameState();
    bot.node_id = 0;
    bot.board = 0;

    ThreeCardGameState state;
    cout << "\nHand " << hand_number << " - You are " << seatName(human_id) << "\n";
    cout << "Your hand: " << maskToString(human_id == 0 ? sb_hand : bb_hand) << "\n";

    bool quit = false;
    while(!state.isTerminal()){
        if(state.turn == -1){
            auto actions = state.generateActions();
            if(actions.empty()){
                break;
            }
            if(state.street == 1){
                unsigned c1 = deck.draw();
                unsigned c2 = deck.draw();
                board |= 1ull << c1;
                board |= 1ull << c2;
                bot.updateBoard(c1);
                bot.updateBoard(c2);
                cout << "Flop: " << maskToString(board) << "\n";
            } else if(state.street == 5){
                unsigned c = deck.draw();
                board |= 1ull << c;
                bot.updateBoard(c);
                cout << "Turn: " << cardToString(c) << "\n";
            } else if(state.street == 6){
                unsigned c = deck.draw();
                board |= 1ull << c;
                bot.updateBoard(c);
                cout << "River: " << cardToString(c) << "\n";
            }
            state = *dynamic_cast<ThreeCardGameState*>(actions[0].first.get());
            bot.state = state;
            continue;
        }

        auto actions = state.generateActions();
        if(actions.empty()){
            break;
        }

        cout << "\nStreet: " << streetName(state) << "\n";
        cout << "Board: " << maskToString(board) << "\n";
        cout << "Pot: " << state.pot << " | SB stack: " << state.sb_stack
             << " BB stack: " << state.bb_stack
             << " | Bets: SB " << state.sb_bet
             << " BB " << state.bb_bet << "\n";

        if(state.turn == human_id){
            uint64_t hand_mask = (human_id == 0) ? sb_hand : bb_hand;
            uint64_t visible_hand = hand_mask & ~board;
            cout << "Your hand: " << maskToString(visible_hand) << "\n";
            if(state.street == 2 || state.street == 3){
                vector<unsigned> cards = maskToCards(hand_mask);
                cout << "Discard one card:\n";
                for(size_t i = 0; i < cards.size(); ++i){
                    cout << "  " << (i + 1) << ") " << cardToString(cards[i]) << "\n";
                }
                int action_index = -1;
                unsigned discarded = 0;
                if(!promptForDiscardChoice(cards, action_index, discarded, quit)){
                    break;
                }
                board |= 1ull << discarded;
                bot.updateBoard(discarded);
                cout << "You discard " << cardToString(discarded) << "\n";
                state = *dynamic_cast<ThreeCardGameState*>(actions[action_index].first.get());
                bot.state = state;
                bot.node_id = bot.children[bot.node_id][action_index];
            } else {
                cout << "Actions:\n";
                for(size_t i = 0; i < actions.size(); ++i){
                    cout << "  " << (i + 1) << ") " << actions[i].second->toString() << "\n";
                }
                int action_index = promptForActionIndex(actions, quit);
                if(quit){
                    break;
                }
                auto* action = dynamic_cast<ThreeCardAction*>(actions[action_index].second.get());
                if(action){
                    cout << "You: " << action->toString() << "\n";
                }
                state = *dynamic_cast<ThreeCardGameState*>(actions[action_index].first.get());
                bot.state = state;
                bot.node_id = bot.children[bot.node_id][action_index];
            }
        } else {
            int action_index = choosePolicyActionIndex(bot, rng);
            if(action_index < 0 || action_index >= static_cast<int>(actions.size())){
                cout << "Policy failed to choose action.\n";
                break;
            }
            auto* action = dynamic_cast<ThreeCardAction*>(actions[action_index].second.get());
            if(action && action->action == "discard"){
                uint64_t hand = (bot_id == 0) ? sb_hand : bb_hand;
                vector<unsigned> cards = maskToCards(hand);
                unsigned discarded = cards[action_index];
                board |= 1ull << discarded;
                bot.updateBoard(discarded);
                cout << "Bot (" << seatName(bot_id) << ") discards " << cardToString(discarded) << "\n";
            } else if(action) {
                cout << "Bot (" << seatName(bot_id) << "): " << action->toString() << "\n";
            }
            state = *dynamic_cast<ThreeCardGameState*>(actions[action_index].first.get());
            bot.state = state;
            bot.node_id = bot.children[bot.node_id][action_index];
        }
    }

    if(quit){
        return false;
    }

    int winner = state.winner;
    if(winner == 0 && state.showdown){
        winner = bucket.getWinner(board, sb_hand, bb_hand);
    }

    cout << "\nFinal board: " << maskToString(board) << "\n";
    cout << "SB hand: " << maskToString(sb_hand) << "\n";
    cout << "BB hand: " << maskToString(bb_hand) << "\n";
    string winner_label = (winner == 1 ? "sb" : (winner == -1 ? "bb" : "tie"));
    cout << "Winner: " << winner_label << " | Pot: " << state.pot << "\n";

    int human_delta = 0;
    if(winner != 0){
        human_delta = (human_id == 0 ? winner : -winner) * (state.pot/2);
    }
    cout << "Your result: " << human_delta << "\n";
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

    unsigned int seed = options.has_seed ? options.seed : static_cast<unsigned int>(time(nullptr));
    std::mt19937 rng(seed);
    cout << "Policy: " << options.policy_path << "\n";
    cout << "Seed: " << seed << "\n";

    ThreeCardPlayer bot;
    bot.init(options.policy_path);
    NaiveThreeCardBucket bucket;

    int hand_number = 1;
    while(options.max_hands == 0 || hand_number <= options.max_hands){
        int human_id = 0;
        if(options.seat == "sb"){
            human_id = 0;
        } else if(options.seat == "bb"){
            human_id = 1;
        } else {
            uniform_int_distribution<int> seat_dist(0, 1);
            human_id = seat_dist(rng);
        }
        if(!playHand(bot, bucket, rng, human_id, hand_number)){
            break;
        }
        hand_number++;
        if(options.max_hands == 0){
            cout << "\nPlay another hand? (y/n): " << flush;
            string line;
            if(!getline(cin, line)){
                break;
            }
            string cleaned = toLower(trim(line));
            if(cleaned != "y" && cleaned != "yes"){
                break;
            }
        }
    }

    return 0;
}
