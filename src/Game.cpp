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

    Cell stone = (_current_player == Player::Black) ? Cell::Black : Cell::White;

    // Guard 4: double-three rule.
    // A move is illegal if it simultaneously creates two or more free-threes
    // through the placed stone on distinct axes.
    // Exception: a move that also captures at least one pair is always legal,
    // even if it creates a double-three (explicitly stated in the rules appendix).
    if (!wouldCapture(row, col, stone)) {
        // Temporarily place the stone so hasFreeThreeOnAxis can see it in context —
        // the patterns require the placed stone to already be on the board.
        _board.set(row, col, stone);
        bool is_double_three = wouldCreateDoubleThree(row, col, stone);
        _board.set(row, col, Cell::Empty); // restore before applyMove places it for real

        if (is_double_three)
            return std::nullopt;
    }

    // All checks passed — applyMove handles placement, captures, win detection, turn.
    return applyMove(row, col);
}

// ── AI-facing interface ───────────────────────────────────────────────────────

// Checks all the same rules as placeStone() but does NOT commit the move.
// The double-three check requires temporarily placing the stone — we place it,
// query wouldCreateDoubleThree, then immediately remove it. This is safe because
// the search is single-threaded and we restore the board before returning.
bool Game::isLegalMove(int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) return false;
    if (_board.get(row, col) != Cell::Empty) return false;
    if (_state != GameState::Ongoing) return false;

    Cell stone = (_current_player == Player::Black) ? Cell::Black : Cell::White;

    // Moves that capture are always legal even if they form a double-three.
    if (!wouldCapture(row, col, stone)) {
        _board.set(row, col, stone);
        bool dbl = wouldCreateDoubleThree(row, col, stone);
        _board.set(row, col, Cell::Empty);
        if (dbl) return false;
    }
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

// Scan 4 axes through the placed stone.
// An axis declares a win only if it has 5+ consecutive same-color stones AND
// the opponent cannot immediately break that line by capturing a pair from it.
//
// Why the breakability check?
//   The rules say a 5-in-a-row is only a real win when the opponent has no
//   capture available on that line. If they do, the game continues — they get
//   their turn to execute the break. If that breaking capture reaches 10 stones,
//   they win by capture instead (checkCaptureWin handles that on their turn).
//
// We check all 4 axes before returning so that a player with simultaneous
// alignments on multiple axes wins if ANY of them is unbreakable.
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

        if (total >= 5 && !isAlignmentBreakable(row, col, stone, dr, dc)) {
            _state = (_current_player == Player::Black)
                         ? GameState::BlackWins
                         : GameState::WhiteWins;
            return; // one unbreakable alignment is enough
        }
    }
}

// Check every stone in the winning line to see if the opponent can capture it.
// A stone in the line is capturable when an adjacent same-color stone exists
// and the flanking cells form a valid Me Opp Opp Me opportunity for the opponent.
//
// The two cases for a pair (S1, S2) being captured by the opponent:
//   Case A: [opponent_existing] S1 S2 [empty]      → opponent plays at the empty end
//   Case B: [empty]             S1 S2 [opponent_existing] → opponent plays at the empty end
//
// We check all 8 directions from each line stone so both pair orderings and all
// axes are covered.
bool Game::isAlignmentBreakable(int row, int col, Cell stone, int dr, int dc) const {
    Cell opponent = (stone == Cell::Black) ? Cell::White : Cell::Black;

    // Walk the full extent of the winning line along this axis.
    int fwd = countInDirection(row, col,  dr,  dc, stone);
    int bwd = countInDirection(row, col, -dr, -dc, stone);

    for (int i = -bwd; i <= fwd; ++i) {
        int r = row + i * dr;
        int c = col + i * dc;

        // Try every direction as the pair axis (S1 at (r,c), S2 one step ahead).
        const int dirs[8][2] = {
            { 0, 1}, { 0,-1}, { 1, 0}, {-1, 0},
            { 1, 1}, {-1,-1}, { 1,-1}, {-1, 1}
        };

        for (auto& d : dirs) {
            int d_r = d[0], d_c = d[1];

            int r2 = r + d_r,     c2 = c + d_c;     // S2 (pair partner)
            int rf = r + 2 * d_r, cf = c + 2 * d_c; // far  flank position
            int rn = r - d_r,     cn = c - d_c;      // near flank position

            // All three surrounding cells must be on the board.
            if (r2 < 0 || r2 >= BOARD_SIZE || c2 < 0 || c2 >= BOARD_SIZE) continue;
            if (rf < 0 || rf >= BOARD_SIZE || cf < 0 || cf >= BOARD_SIZE) continue;
            if (rn < 0 || rn >= BOARD_SIZE || cn < 0 || cn >= BOARD_SIZE) continue;

            // S2 must be the same color (forming the capturable pair with S1).
            if (_board.get(r2, c2) != stone) continue;

            // Case A: opponent already at near flank, empty at far flank.
            // Opponent plays at far to complete: [opp] S1 S2 [opp_new].
            if (_board.get(rn, cn) == opponent && _board.get(rf, cf) == Cell::Empty)
                return true;

            // Case B: empty at near flank, opponent already at far flank.
            // Opponent plays at near to complete: [opp_new] S1 S2 [opp].
            if (_board.get(rn, cn) == Cell::Empty && _board.get(rf, cf) == opponent)
                return true;
        }
    }
    return false;
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

// ── Double-three helpers (Phase 4) ────────────────────────────────────────────

// Check whether placing `stone` at (row, col) would trigger any capture.
// The stone is NOT yet on the board when this is called — but the capture
// pattern only reads cells BEYOND (row, col), so this is correct.
// The placed stone is always the starting "Me" in Me Opp Opp Me, and we
// check offsets +1, +2, +3 outward in each direction.
bool Game::wouldCapture(int row, int col, Cell stone) const {
    Cell opponent = (stone == Cell::Black) ? Cell::White : Cell::Black;

    const int directions[8][2] = {
        { 0,  1}, { 0, -1},
        { 1,  0}, {-1,  0},
        { 1,  1}, {-1, -1},
        { 1, -1}, {-1,  1}
    };

    for (auto& dir : directions) {
        int dr = dir[0], dc = dir[1];
        int r1 = row + dr,     c1 = col + dc;
        int r2 = row + 2 * dr, c2 = col + 2 * dc;
        int r3 = row + 3 * dr, c3 = col + 3 * dc;

        if (r1 < 0 || r1 >= BOARD_SIZE || c1 < 0 || c1 >= BOARD_SIZE) continue;
        if (r2 < 0 || r2 >= BOARD_SIZE || c2 < 0 || c2 >= BOARD_SIZE) continue;
        if (r3 < 0 || r3 >= BOARD_SIZE || c3 < 0 || c3 >= BOARD_SIZE) continue;

        if (_board.get(r1, c1) == opponent &&
            _board.get(r2, c2) == opponent &&
            _board.get(r3, c3) == stone)
            return true;
    }
    return false;
}

// Check if the stone at (row, col) participates in a free-three on this axis.
// Called AFTER the stone is placed on the board.
//
// Strategy: slide a 6-cell window along the axis and test all 4 free-three
// patterns inside it. The placed stone (at offset 0) must be at a STONE
// position within the window — not at the gap or a flank.
//
// The 4 patterns within a 6-cell window [0..5] (flanks at [0] and [5]):
//   Pattern A:  _ _ X X X _    stones={2,3,4}  gap=1
//   Pattern B:  _ X _ X X _    stones={1,3,4}  gap=2
//   Pattern C:  _ X X _ X _    stones={1,2,4}  gap=3
//   Pattern D:  _ X X X _ _    stones={1,2,3}  gap=4
bool Game::hasFreeThreeOnAxis(int row, int col, int dr, int dc, Cell stone) const {
    // Read the cell type at axis offset k from (row, col).
    // Returns: 1 = mine, 0 = empty, -1 = opponent or out of bounds.
    auto cellType = [&](int k) -> int {
        int r = row + k * dr;
        int c = col + k * dc;
        if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) return -1;
        Cell cell = _board.get(r, c);
        if (cell == stone)       return  1;
        if (cell == Cell::Empty) return  0;
        return -1;
    };

    // Each pattern: which 3 window positions must be MINE, and which is the gap.
    struct Pattern {
        int stone_positions[3]; // window positions that must hold our stone
        int gap_position;       // window position that must be empty (completing move)
    };
    const Pattern patterns[4] = {
        {{2, 3, 4}, 1},  // _ _ X X X _
        {{1, 3, 4}, 2},  // _ X _ X X _
        {{1, 2, 4}, 3},  // _ X X _ X _
        {{1, 2, 3}, 4},  // _ X X X _ _
    };

    // The placed stone is at axis offset 0. In a 6-cell window it can occupy
    // window positions 1, 2, 3, or 4 (never a flank). The window start offset
    // is therefore -1, -2, -3, or -4 relative to the placed stone.
    for (int window_start = -1; window_start >= -4; --window_start) {
        int placed_in_window = -window_start; // 1, 2, 3, or 4

        for (const Pattern& pat : patterns) {
            // Skip if the placed stone lands on the gap, not a stone position.
            bool placed_is_stone_position =
                (placed_in_window == pat.stone_positions[0] ||
                 placed_in_window == pat.stone_positions[1] ||
                 placed_in_window == pat.stone_positions[2]);
            if (!placed_is_stone_position) continue;

            // Both flanks (window[0] and window[5]) must be empty — they are the
            // open ends that make this an "open" four after the gap is filled.
            if (cellType(window_start + 0) != 0) continue;
            if (cellType(window_start + 5) != 0) continue;

            // The gap must be empty (that's where the completing stone would go).
            if (cellType(window_start + pat.gap_position) != 0) continue;

            // All 3 stone positions must hold our stone.
            if (cellType(window_start + pat.stone_positions[0]) != 1) continue;
            if (cellType(window_start + pat.stone_positions[1]) != 1) continue;
            if (cellType(window_start + pat.stone_positions[2]) != 1) continue;

            return true; // found a free-three on this axis
        }
    }
    return false;
}

// Count how many distinct axes have a free-three through (row, col).
// If 2 or more do, the move that placed this stone is a double-three and illegal.
// Must be called with the stone already on the board at (row, col).
bool Game::wouldCreateDoubleThree(int row, int col, Cell stone) const {
    const int axes[4][2] = {
        {0,  1},  // horizontal
        {1,  0},  // vertical
        {1,  1},  // diagonal ↘
        {1, -1}   // anti-diagonal ↙
    };

    int free_three_count = 0;

    for (auto& axis : axes) {
        if (hasFreeThreeOnAxis(row, col, axis[0], axis[1], stone))
            ++free_three_count;
    }

    // Two or more free-three axes through the same stone = double-three = illegal.
    return free_three_count >= 2;
}
