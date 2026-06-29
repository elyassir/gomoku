#pragma once
#include <array>

// The board is always 19x19 — fixed by Gomoku rules, never changes at runtime.
static constexpr int BOARD_SIZE = 19;

// Every cell on the board is exactly one of these three states.
// Using an enum class (scoped) prevents accidental int comparisons.
enum class Cell {
    Empty,
    Black,
    White
};

// Board owns the raw game state: who has a stone where.
// It knows nothing about rules, rendering, or whose turn it is —
// those concerns live elsewhere so each piece stays testable on its own.
class Board {
public:
    // Zero-initialises the grid to all Empty on construction.
    Board();

    // Read a single cell. Row and col are 0-indexed from the top-left.
    Cell        get(int row, int col) const;

    // Write a single cell. Used by every move-making path: human clicks, AI moves, undo.
    void        set(int row, int col, Cell cell);

    // Wipe the board back to all Empty (used for new game / restart).
    void        reset();

private:
    // A 2D array indexed [row][col].
    // Outer index = row (Y), inner = col (X) — consistent with how we scan lines later.
    std::array<std::array<Cell, BOARD_SIZE>, BOARD_SIZE> _grid;
};
