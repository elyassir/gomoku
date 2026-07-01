#include "Game.hpp"

// Member-initialiser list is used so each field is constructed directly
// rather than default-constructed and then assigned — cleaner and safer.
Game::Game()
    : _board(),
      _current_player(Player::Black),   // Black moves first — Gomoku convention
      _state(GameState::Ongoing)
{}

bool Game::placeStone(int row, int col) {
    // Guard 1: coordinates must be inside the 19x19 grid.
    // This also catches the "click outside the board" case from the UI layer.
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE)
        return false;

    // Guard 2: the cell must be empty.
    // A stone already there means someone tried to click an occupied intersection.
    if (_board.get(row, col) != Cell::Empty)
        return false;

    // Guard 3: no moves are legal once the game is over.
    if (_state != GameState::Ongoing)
        return false;

    // Convert the current player enum to the Cell type the Board stores.
    Cell stone = (_current_player == Player::Black) ? Cell::Black : Cell::White;
    _board.set(row, col, stone);

    // ── Phase 3 will add capture processing here. ──

    // Check whether the placed stone completes an alignment of 5 or more.
    // We do this BEFORE toggling the turn so _current_player still identifies
    // the player who just moved when we set the win state.
    checkAlignment(row, col, stone);

    // Only advance the turn if the game is still ongoing.
    // If checkAlignment just set a win state, we freeze here.
    if (_state == GameState::Ongoing)
        togglePlayer();
    return true;
}

Player Game::currentPlayer() const {
    return _current_player;
}

GameState Game::state() const {
    return _state;
}

const Board& Game::board() const {
    return _board;
}

// Ternary toggle: Black→White, White→Black.
// Every turn change goes through this function so there's never a stray
// assignment to _current_player elsewhere in the codebase.
void Game::togglePlayer() {
    _current_player =
        (_current_player == Player::Black) ? Player::White : Player::Black;
}

// Walk from (row, col) in direction (delta_row, delta_col), counting cells
// that hold `stone`. Stop at the board edge or a different cell type.
// The origin cell is not counted — the caller adds 1 for the placed stone.
int Game::countInDirection(int row, int col, int delta_row, int delta_col, Cell stone) const {
    int count = 0;
    int current_row = row + delta_row;
    int current_col = col + delta_col;

    while (current_row >= 0 && current_row < BOARD_SIZE &&
           current_col >= 0 && current_col < BOARD_SIZE &&
           _board.get(current_row, current_col) == stone) {
        ++count;
        current_row += delta_row;
        current_col += delta_col;
    }
    return count;
}

// Scan 4 axes through (row, col): horizontal, vertical, and both diagonals.
// For each axis we look in both directions and sum the counts.
// The total including the placed stone is: 1 + forward_count + backward_count.
// If any axis reaches 5 or more, the current player wins.
//
// Why only the stone just played?
//   A winning line must include the last stone placed — if 5 existed before
//   this move, the previous move would have triggered the win. So scanning
//   only through the new stone is both correct and far faster than a full
//   board rescan. This same argument applies to captures in Phase 3.
void Game::checkAlignment(int row, int col, Cell stone) {
    // The 4 axes expressed as (delta_row, delta_col) for the forward direction.
    // The backward direction is simply the negation of each pair.
    const int directions[4][2] = {
        {0, 1},   // horizontal  →
        {1, 0},   // vertical    ↓
        {1, 1},   // diagonal    ↘
        {1, -1}   // anti-diagonal ↙
    };

    for (auto& dir : directions) {
        int delta_row = dir[0];
        int delta_col = dir[1];

        // Count in both directions along this axis, then add 1 for the placed stone.
        int total = 1
            + countInDirection(row, col,  delta_row,  delta_col, stone)  // forward
            + countInDirection(row, col, -delta_row, -delta_col, stone); // backward

        if (total >= 5) {
            // Note: Phase 5 will make this conditional — a 5-alignment can be
            // nullified if the opponent can immediately capture a stone from that
            // line. For now, 5-in-a-row is an instant win.
            _state = (_current_player == Player::Black)
                         ? GameState::BlackWins
                         : GameState::WhiteWins;
            return; // one winning axis is enough; no need to check the others
        }
    }
}
