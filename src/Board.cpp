#include "Board.hpp"

// Delegate construction to reset() so there is exactly one place
// that defines what "empty board" means.
Board::Board() {
    reset();
}

Cell Board::get(int row, int col) const {
    return _grid[row][col];
}

void Board::set(int row, int col, Cell cell) {
    _grid[row][col] = cell;
}

// Fill every cell with Empty.
// std::array::fill is a single call per row — clearer than a nested loop.
void Board::reset() {
    for (auto& row : _grid)
        row.fill(Cell::Empty);
}
