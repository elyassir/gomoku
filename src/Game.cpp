#include "../include/Game.hpp"

// Member-initialiser list constructs each field directly.
Game::Game()
    : _board(),
      _current_player(Player::Black),
      _state(GameState::Ongoing),
      _black_captures(0),
      _white_captures(0)
{}

// ── Human-facing move entry point ────────────────────────────────────────────

std::optional<MoveRecord> Game::placeStone(int row, int col) {
    // Guard 1: coordinates must be inside the 19x19 grid.
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE)
        return std::nullopt;

    // Guard 2: the cell must be empty.
    if (_board.get(row, col) != Cell::Empty)
        return std::nullopt;

    // Guard 3: no moves after the game is decided.
    if (_state != GameState::Ongoing)
        return std::nullopt;

    // All checks passed — applyMove handles placement, captures, win detection, turn.
    return applyMove(row, col);
}

// ── AI-facing interface ───────────────────────────────────────────────────────

// Returns true if placing at (row, col) is legal: in bounds, empty, game ongoing.
bool Game::isLegalMove(int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) return false;
    if (_board.get(row, col) != Cell::Empty) return false;
    if (_state != GameState::Ongoing) return false;
    return true;
}

// ── AI-facing make / unmake ───────────────────────────────────────────────────

// applyMove does the full work of a move: place stone, process captures,
// check win conditions, advance turn. It returns a MoveRecord so the caller
// (the AI search) can pass it straight to undoMove() later.
MoveRecord Game::applyMove(int row, int col) {
    // Snapshot the state BEFORE this move so undoMove can restore it exactly.
    MoveRecord record;
    record.row                   = row;
    record.col                   = col;
    record.stone                 = (_current_player == Player::Black) ? Cell::Black : Cell::White;
    record.state_before          = _state;
    record.black_captures_before = _black_captures;
    record.white_captures_before = _white_captures;
    record.num_captures          = 0;

    // Place the stone.
    _board.set(row, col, record.stone);

    // Check for captures triggered by this stone BEFORE checking alignment.
    // Order matters: a capture can free cells that would otherwise block a 5-in-a-row,
    // and Phase 5 will let captures break an opponent's alignment win.
    processCaptures(row, col, record.stone, record);

    // Check whether the placed stone (or a capture win) ends the game.
    checkCaptureWin();
    if (_state == GameState::Ongoing)
        checkAlignment(row, col, record.stone);

    // Advance turn only if the game is still going.
    if (_state == GameState::Ongoing)
        togglePlayer();

    return record;
}

// Reverse every effect of a move: remove the placed stone, restore all captured
// stones, restore counters, restore game state, and give the turn back.
void Game::undoMove(const MoveRecord& record) {
    // Restore the placed stone to empty.
    _board.set(record.row, record.col, Cell::Empty);

    // Put every captured stone back on the board.
    for (int i = 0; i < record.num_captures; ++i) {
        const CapturedPair& pair = record.captures[i];
        // The captured stones belonged to the opponent of whoever placed this stone.
        Cell opponent = (record.stone == Cell::Black) ? Cell::White : Cell::Black;
        _board.set(pair.row1, pair.col1, opponent);
        _board.set(pair.row2, pair.col2, opponent);
    }

    // Restore capture counters and game state to exactly what they were before.
    _black_captures = record.black_captures_before;
    _white_captures = record.white_captures_before;

    // applyMove only calls togglePlayer() when _state == Ongoing AFTER the move.
    // _state at this point still holds the post-move value (not yet restored),
    // so we read it now to determine whether a toggle must be reversed.
    // Reading state_before instead would be wrong: a move that ENDS the game has
    // state_before == Ongoing but did NOT toggle — the two conditions differ
    // precisely in that case, causing _current_player corruption in the search.
    bool did_toggle = (_state == GameState::Ongoing);

    _state = record.state_before;

    if (did_toggle)
        togglePlayer();
}

// ── Accessors ─────────────────────────────────────────────────────────────────

Player Game::currentPlayer() const {
    return _current_player;
}

GameState Game::state() const {
    return _state;
}

const Board& Game::board() const {
    return _board;
}

int Game::captureCount(Player player) const {
    return (player == Player::Black) ? _black_captures : _white_captures;
}

// ── Internal helpers ──────────────────────────────────────────────────────────

void Game::togglePlayer() {
    _current_player =
        (_current_player == Player::Black) ? Player::White : Player::Black;
}

// Walk from (row, col) in direction (delta_row, delta_col), counting cells
// that hold `stone`. Stop at the board edge or a different cell type.
// The origin cell is NOT counted — callers add 1 for the placed stone.
int Game::countInDirection(int row, int col, int delta_row, int delta_col, Cell stone) const {
    int count = 0;
    int r = row + delta_row;
    int c = col + delta_col;

    while (r >= 0 && r < BOARD_SIZE &&
           c >= 0 && c < BOARD_SIZE &&
           _board.get(r, c) == stone) {
        ++count;
        r += delta_row;
        c += delta_col;
    }
    return count;
}

// Scan 4 axes through the placed stone; 5 or more consecutive same-color stones wins.
void Game::checkAlignment(int row, int col, Cell stone) {
    const int axes[4][2] = {
        {0,  1},  // horizontal →
        {1,  0},  // vertical   ↓
        {1,  1},  // diagonal   ↘
        {1, -1}   // anti-diag  ↙
    };

    for (auto& axis : axes) {
        int dr = axis[0];
        int dc = axis[1];

        int total = 1
            + countInDirection(row, col,  dr,  dc, stone)
            + countInDirection(row, col, -dr, -dc, stone);

        if (total >= 5) {
            _state = (_current_player == Player::Black)
                         ? GameState::BlackWins
                         : GameState::WhiteWins;
            return;
        }
    }
}

// Scan all 8 directions from the placed stone looking for Me Opp Opp Me.
// The placed stone is always the first "Me" in the pattern; we look outward
// from it. This means we ONLY detect captures triggered by the mover —
// moving into a flanked position never fires captures on the mover (safe rule).
//
// For each direction we check:
//   cell[+1] == opponent?
//   cell[+2] == opponent?
//   cell[+3] == mover?
// If all three match, we remove cells +1 and +2, record them, and update
// the capture counter.
void Game::processCaptures(int row, int col, Cell stone, MoveRecord& record) {
    Cell opponent = (stone == Cell::Black) ? Cell::White : Cell::Black;

    // All 8 directions: the placed stone is the starting flank.
    const int directions[8][2] = {
        { 0,  1}, { 0, -1},  // horizontal
        { 1,  0}, {-1,  0},  // vertical
        { 1,  1}, {-1, -1},  // diagonal ↘/↖
        { 1, -1}, {-1,  1}   // anti-diagonal ↙/↗
    };

    for (auto& dir : directions) {
        int dr = dir[0];
        int dc = dir[1];

        // The three cells we need to inspect beyond the placed stone.
        int r1 = row + dr,     c1 = col + dc;     // Opp (first)
        int r2 = row + 2 * dr, c2 = col + 2 * dc; // Opp (second)
        int r3 = row + 3 * dr, c3 = col + 3 * dc; // Me  (far flank)

        // Bounds check all three cells before reading them.
        if (r1 < 0 || r1 >= BOARD_SIZE || c1 < 0 || c1 >= BOARD_SIZE) continue;
        if (r2 < 0 || r2 >= BOARD_SIZE || c2 < 0 || c2 >= BOARD_SIZE) continue;
        if (r3 < 0 || r3 >= BOARD_SIZE || c3 < 0 || c3 >= BOARD_SIZE) continue;

        // Pattern: Me(placed) Opp Opp Me — capture the two Opp stones.
        if (_board.get(r1, c1) == opponent &&
            _board.get(r2, c2) == opponent &&
            _board.get(r3, c3) == stone) {

            // Remove both captured stones from the board.
            _board.set(r1, c1, Cell::Empty);
            _board.set(r2, c2, Cell::Empty);

            // Record the pair so undoMove can put them back.
            record.captures[record.num_captures++] = { r1, c1, r2, c2 };

            // Update the counter for the mover (+2 because a pair = 2 stones).
            if (stone == Cell::Black)
                _black_captures += 2;
            else
                _white_captures += 2;
        }
    }
}

// A player wins by capture when they have removed 10 or more opponent stones
// (i.e., 5 or more pairs). Check both players each time — theoretically only
// the mover can reach 10 on this move, but checking both keeps this correct
// under any future rule changes.
void Game::checkCaptureWin() {
    if (_black_captures >= 10)
        _state = GameState::BlackWins;
    else if (_white_captures >= 10)
        _state = GameState::WhiteWins;
}

