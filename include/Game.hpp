#pragma once
#include "Board.hpp"
#include <array>

// Which player is acting or has won.
// Black always moves first in Gomoku.
enum class Player { Black, White };

// The three possible states of a game session.
enum class GameState { Ongoing, BlackWins, WhiteWins };

// ── Make / Unmake structures ──────────────────────────────────────────────────

// Two opponent stones removed by a single capture (Me Opp Opp Me pattern).
// Storing both cells separately avoids recomputing them during undo.
struct CapturedPair {
    int row1, col1;   // first opponent stone in the pair
    int row2, col2;   // second opponent stone in the pair
};

// Everything needed to fully reverse a move.
// The AI calls applyMove() on the way down the search tree and undoMove() on
// the way back up — this record is the only thing that makes that possible
// without copying the entire board state.
struct MoveRecord {
    int       row, col;                    // where the stone was placed
    Cell      stone;                       // which color was placed
    GameState state_before;               // game state before this move, restored by undo
    int       black_captures_before;      // capture counters before this move, restored by undo
    int       white_captures_before;
    std::array<CapturedPair, 8> captures; // pairs removed (max 8: one per direction)
    int       num_captures;               // how many entries in captures[] are valid
};

// ── Game class ────────────────────────────────────────────────────────────────

// Game owns the complete session state: the board, whose turn it is, capture
// counters, and whether the game is over.
class Game {
public:
    // Start a fresh game: empty board, Black to move, state = Ongoing.
    Game();

    // ── Human-facing interface ────────────────────────────────────────────────

    // Validate then apply a move for the current player at (row, col).
    // Returns true  → move was legal, stone placed, captures processed, turn advanced.
    // Returns false → move was illegal (out of bounds, occupied, game over).
    bool placeStone(int row, int col);

    // ── AI-facing interface ───────────────────────────────────────────────────

    // Apply a move with no validation — the AI only generates legal moves so
    // the guards in placeStone() would just add overhead in the search tree.
    // Returns a MoveRecord that can be passed to undoMove() to reverse everything.
    MoveRecord applyMove(int row, int col);

    // Fully reverse a move: restore the placed stone, all captured stones,
    // capture counters, whose turn it is, and the game state.
    // Called by the AI on the way back up the search tree.
    void undoMove(const MoveRecord& record);

    // ── Accessors ─────────────────────────────────────────────────────────────
    Player        currentPlayer()             const;
    GameState     state()                     const;
    const Board&  board()                     const;
    int           captureCount(Player player) const; // stones captured (pairs × 2)

private:
    Board     _board;
    Player    _current_player;
    GameState _state;
    int       _black_captures; // total opponent stones Black has captured
    int       _white_captures; // total opponent stones White has captured

    void togglePlayer();

    // Count consecutive `stone`-colored cells from (row,col) in one direction.
    // Does NOT count the origin — callers add 1 for the placed stone.
    int countInDirection(int row, int col, int delta_row, int delta_col, Cell stone) const;

    // Scan all 4 axes through the placed stone; set _state to a win if 5+ align.
    void checkAlignment(int row, int col, Cell stone);

    // Scan all 8 directions for the pattern Me Opp Opp Me.
    // Remove any matching pairs, update capture counters, and record pairs in `record`.
    // Only fires for the player who moved (captures are never triggered by the mover
    // placing a stone into a flanked position — see Phase 3 explanation).
    void processCaptures(int row, int col, Cell stone, MoveRecord& record);

    // Set _state to a capture win if either player has reached 10 captured stones.
    void checkCaptureWin();

    // ── Double-three helpers (Phase 4) ────────────────────────────────────────

    // Returns true if placing `stone` at (row, col) would immediately trigger
    // at least one capture in any direction (Me Opp Opp Me pattern).
    // Used to apply the capture exception to the double-three rule:
    // a move that captures is legal even if it simultaneously creates a double-three.
    bool wouldCapture(int row, int col, Cell stone) const;

    // Returns true if, with `stone` already placed at (row, col), that axis
    // contains a free-three through (row, col).
    //
    // A free-three is a set of exactly 3 same-color stones that can become an
    // OPEN FOUR — i.e., _ X X X X _ — in exactly one move (the gap cell).
    // The placed stone must be one of the 3 existing stones, not the gap.
    //
    // The 4 patterns (6-cell window where window[0] and window[5] are the open flanks):
    //   _ _ X X X _   (gap at window[1], stones at 2,3,4)
    //   _ X _ X X _   (gap at window[2], stones at 1,3,4)
    //   _ X X _ X _   (gap at window[3], stones at 1,2,4)
    //   _ X X X _ _   (gap at window[4], stones at 1,2,3)
    //
    // NOTE: this must be called with `stone` already on the board at (row, col)
    // so that the board state reflects the move being evaluated.
    bool hasFreeThreeOnAxis(int row, int col, int dr, int dc, Cell stone) const;

    // Returns true if the stone placed at (row, col) participates in a free-three
    // on 2 or more distinct axes simultaneously — the double-three condition.
    // Must be called with `stone` already on the board at (row, col).
    bool wouldCreateDoubleThree(int row, int col, Cell stone) const;
};
