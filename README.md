```bash
mkdir -p build
cd build
cmake ..
make three_card_trainer
cd ..
ulimit -s unlimited
export OMP_PROC_BIND="close" // can do true and spread as well, but close is better
export OMP_PLACES="cores"
export OMP_NUM_THREADS=30 // two less than max number of threads
./build/three_card_trainer
```

```bash
cmake -S . -B build
cmake --build build -j --target three_card_trainer
ulimit -s unlimited
export OMP_PROC_BIND="close" // can do true and spread as well, but close is better
export OMP_PLACES="cores"
export OMP_NUM_THREADS=30 // two less than max number of threads
./build/three_card_trainer
```

The variant is as follows:
Toss or Hold’em
We strongly recommend learning the rules of Texas Hold’em before learning Toss or Hold’em. Two
helpful resources are pkr .bot/poker-rules and pkr .bot/poker-video.
Overview
The poker variant for the 6.9630 Pokerbots Competition in IAP 2026 is “Toss or Hold’em,
” a game
based on the popular poker variant No-Limit Texas Hold’em. The main modifications are as follows:
each player starts with 3 pre-discard hole cards. The flop is two cards, and after the flop has been
dealt out but before flop betting, each player (sequentially) chooses to “discard” one of their three
hole cards and move them to the board cards (so that after discarding, each player has 2
post-discard hole cards and there are 4 board cards). Any discarded cards are public information as
they now function as normal public-use board cards. The turn and river then happen normally, so
that (without folding), there are 6 board cards by showdown. Showdown is still evaluated by each
player’s best 5-card hand.
It is important to note that at every round of play, the players’ stack sizes are reset to 400.
Another useful observation is when a player discards one of their pre-discard hole cards, it gets
moved to the board, meaning the player still retains access to it when making their best 5-card
hand. However, the villain also has access to that card now.
Glossary
Pre-discard hole
cards
A player’s 3 private cards, prior to discarding one card to the board
during the flop
Post-discard hole
cards
A player’s 2 private cards, after discarding one card to the board during
the flop
Board cards Cards that are public and shared between the players
Pot The accumulation of bets and other payments made by the players
during a round to be claimed by the winner of the round
Stack A player’s individual resources they use for bets and other payments
during a round
Pip A player’s contribution so far to the pot during a round of betting
Blinds Small, forced bets at the beginning of the round to kick off the pot
Call A minimum pot contribution to stay in the round in response to a bet or
raise
Check A “pass” or bet of 0
Fold To quit the round and let the other player claim the pot
Flop When the first two board cards are dealt.
Discard Round (aka
Toss Round)
When the players sequentially (dealer’s opponent aka out of position
goes first) discard one of their pre-discard hole cards and moves it to be
a board card during the flop. Flop betting occurs after both discards
occur.
Turn When the fifth board card is dealt
Showdown When the players’ post-discard hole cards are revealed to determine the
winner of the round who claims the pot
Game Logistics
A game of Toss or Hold’em consists of a number of rounds played between two players. In every
round, each player is allocated a stack before the cards are dealt. The change in a player’s stack at
the end of the round is used to update that player’s bankroll, which starts at 0. The player with the
highest cumulative bankroll after the last round is played wins the game.
Parameters
Rounds: 1000
Stack allocated per round: 400
Big blind: 2
Small blind: 1
Sequence of Play
A round of No-Limit Toss or Hold’em has all the stages as standard Texas Hold’em, with the
exception of dealing pre-discard hole cards before the flop as well as the discard round.
1. Pay blinds
2. Deal pre-discard hole cards
3. Round of betting
4. Deal flop
5. Discard round
6. Round of betting
7. Deal turn
8. Round of betting
9. Deal river
10. Round of betting
11. Showdown
Deal
Three pre-discard hole cards are dealt after the blinds.
Only two board cards are dealt after the flop initially. After discarding, there will be four board
cards.
The turn and river deals are the same as in standard Texas Hold’em.
Blinds
In each round, one player is designated as the dealer. The dealer alternates between successive
rounds. To start the round, the dealer pays the small blind and their opponent pays the big blind.
The blinds are a mandatory bet of 1 by the dealer followed by a mandatory raise to 2 by their
opponent, which leaves the dealer to act next.
Betting
In the first round of betting (labeled 3 above), the dealer is the first player to act. In this first action,
the dealer may fold (cost: 0), call (cost: 1), or raise (cost: 3+).
In all other rounds of betting, the dealer’s opponent is the first player to act. In this action, the
player may check (cost: 0) or bet (cost: 2+). The minimum legal bet is 1 big blind. The maximum
legal bet is bounded by both players’ remaining stack sizes; this ensures that neither player can
make a bet that their opponent is unable to call.
When a player is faced with a bet or raise from their opponent, that player is allowed to raise. There
is no limit on the number of consecutive raises that may occur in a round of betting.
The raise amount is defined as the amount by which the raising player’s pip exceeds their
opponent’s pip. Equivalently, this is the opponent’s cost of calling after the raise. It is common in
Hold’em to place restrictions on the raise amount in order for a raise to be legal.
In Toss or Hold’em, the minimum legal raise amount follows the same rules as standard No-Limit
Texas Hold’em: the size of the previous bet if responding to a bet or the previous raise amount if
responding to a raise. The maximum legal raise amount is bounded by both players’ remaining stack
sizes so that neither player can make a raise that their opponent is unable to call.
If the remaining stack sizes do not allow for a minimum legal raise amount, i.e. the maximum is
below the minimum, then the only legal raise is the maximum legal raise amount. This occurs when
a player makes an all-in raise.
The round of betting ends when a player calls, when a player folds, or when both players check in a
row. In the first round of betting, if the dealer calls right away, then the round does not end and their
opponent is given the opportunity to act: check (cost: 0) or raise (cost: 2+).
These are the standard Texas Hold’em betting rules; we encourage those unfamiliar with betting in
Hold’em to revisit the resources pkr .bot/poker-rules and pkr .bot/poker-video for a refresher.
Discarding
As discussed earlier, both players are dealt 3 pre-discard hole cards after the blinds are posted.
Pre-flop betting then occurs with these 3 cards in hand.
The flop will be 2 board cards and goes directly into the Discard round. The players sequentially
discard one of their 3 pre-discard hole cards, with the dealer’s opponent/out of position
discarding first. All discarded cards are public information. Thus, the dealer will see 3 cards on
the board (including their opponent’s discarded card) when they are deciding which of their cards
to discard. After both players have discarded, there will be 4 board cards.
Example of play:
A (dealer) and B (out of position)
A posts small blind: 1
B posts big blind: 2
A is dealt: 3 of Hearts, 4 of Hearts, J of Diamonds
B is dealt: K of Clubs, Q of Spades, J of Hearts
A calls
B checks
Flop is dealt with board cards: 7 of Hearts, 2 of Hearts
B is out of position and must discard first
B discards J of Hearts
Updated flop: 7 of Hearts, 2 of Hearts, J of Hearts
A must discard
-
Note the J of Hearts is visible to A while A decides what to discard
-
Also note A has a flush at this point because of the new J of Hearts board card
A discards J of Diamonds
Updated flop: 7 of Hearts, 2 of Hearts, J of Hearts, J of Diamonds
A’s post-discard hole cards: 3 of Hearts, 4 of Hearts
B’s post-discard hole cards: K of Clubs, Q of Spades
B checks
A bets 2
…
(Remainder proceeds as in typical Texas Hold’em, except with a 5th board card dealt during the turn
and a 6th board card dealt during the river)
Showdown
The winner of the round is determined by the standard Texas Hold’em hand scorings. Just as in
regular Texas Hold’Em, players may use any of the board cards in addition to their own hole cards to
make their best possible five-card hand. Player’s bankrolls are updated based off of the winning
player. In the case of a tie, players’ bankrolls do not change.