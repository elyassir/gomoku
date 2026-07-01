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

    // ── Phase 2 will add win detection here (alignment check). ──
    // ── Phase 3 will add capture processing here.              ──

    // All checks passed — advance the turn.
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
