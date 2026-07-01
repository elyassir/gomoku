#pragma once
#include "Board.hpp"

// Which player is acting or has won.
// Black always moves first in Gomoku.
enum class Player { Black, White };

// The three possible states of a game session.
// Every rule check (alignment, captures) will set this to a win state when triggered.
enum class GameState { Ongoing, BlackWins, WhiteWins };

// Game owns the complete session state: the board, whose turn it is, and whether
// the game is over. All move logic passes through here so the rest of the code
// never touches the Board directly — it asks Game to do it.
//
// We introduce this class in Phase 1 even though it's simple, because every
// future phase (captures, win detection, AI) will add to it rather than
// rewriting main.cpp.
class Game {
public:
    // Start a fresh game: empty board, Black to move, state = Ongoing.
    Game();

    // Attempt to place a stone for the current player at (row, col).
    // Returns true  → move was legal, stone placed, turn advanced.
    // Returns false → move was illegal (out of bounds, occupied, game over).
    // Phase 4 (double-three) and Phase 5 (endgame captures) will add more
    // rejection reasons here without changing the signature.
    bool placeStone(int row, int col);

    // Accessors — const so callers can't mutate state accidentally.
    Player        currentPlayer() const;
    GameState     state()         const;
    const Board&  board()         const;

private:
    Board     _board;
    Player    _current_player;
    GameState _state;

    // Flip Black→White or White→Black.
    // A private helper so togglePlayer() is the single place that changes turns.
    void togglePlayer();

    // Count consecutive stones of type `stone` starting from (row, col)
    // and stepping by (delta_row, delta_col) until the edge or a different cell.
    // Does NOT count the origin cell itself — callers add 1 for the placed stone.
    // Reused in Phase 8 (heuristic) to scan the same 4 axes.
    int countInDirection(int row, int col, int delta_row, int delta_col, Cell stone) const;

    // After a stone is placed at (row, col), scan all 4 axes through it.
    // Only checks lines that pass through the placed stone — not the whole board.
    // Sets _state to BlackWins or WhiteWins if any axis reaches 5+.
    void checkAlignment(int row, int col, Cell stone);
};
