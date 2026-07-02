#include "AI.hpp"
#include <chrono>
#include <limits>
#include <algorithm>

// ── Candidate generation ──────────────────────────────────────────────────────

// Why no isLegalMove here?
// At depth 3 with ~50 candidates, there are ~50^2 = 2500 internal nodes, each
// calling this function. isLegalMove runs the full double-three check (~1000 ops
// per candidate). That's 2500 × 50 × 1000 = 125M extra ops for a rule that
// almost never fires. We pay that cost once at the root in bestMove instead,
// filtering the handful of candidates that would actually be played.
std::vector<Move> AI::generateCandidates(const Game& game) const {
    const Board& board = game.board();

    bool candidate[BOARD_SIZE][BOARD_SIZE] = {};
    bool any_stone = false;

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (board.get(r, c) == Cell::Empty) continue;
            any_stone = true;
            for (int dr = -2; dr <= 2; ++dr) {
                for (int dc = -2; dc <= 2; ++dc) {
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE)
                        candidate[nr][nc] = true;
                }
            }
        }
    }

    if (!any_stone)
        return {{BOARD_SIZE / 2, BOARD_SIZE / 2}};

    std::vector<Move> moves;
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (candidate[r][c] && board.get(r, c) == Cell::Empty)
                moves.push_back({r, c});
        }
    }
    return moves;
}

// ── Heuristic evaluation ──────────────────────────────────────────────────────

// Score a non-terminal board from Black's perspective (positive = Black winning).
//
// Scans each unbroken run of same-color stones in all 4 axes, classifying by
// length AND open ends. Completely blocked runs score 0 — they can never
// contribute to a win.
//
// Weight ladder — half_four must be nearly as large as open_four so the AI
// correctly blocks threats at depth 3. If half_four were small (5k), the leaf
// after "White ignores open three → Black makes open four → White blocks one end"
// would score roughly the same as "White blocked the three" → AI plays randomly.
// With half_four = 50k the leaf screams "catastrophic" and blocking is chosen.
//
//   open_four (100k) >> half_four (50k)
//     >> open_three (5k) >> half_three (500)
//       >> open_two (100) >> half_two (20)
int AI::evaluate(const Game& game) const {
    if (game.state() == GameState::BlackWins) return  1'000'000;
    if (game.state() == GameState::WhiteWins) return -1'000'000;

    const Board& board = game.board();

    // Each captured stone is progress toward the 10-stone capture win.
    int score = (game.captureCount(Player::Black) - game.captureCount(Player::White)) * 300;

    const int axes[4][2] = {{0,1},{1,0},{1,1},{1,-1}};

    // ── Pass 1: continuous sequences ─────────────────────────────────────────
    for (auto& axis : axes) {
        int dr = axis[0], dc = axis[1];
        for (int r = 0; r < BOARD_SIZE; ++r) {
            for (int c = 0; c < BOARD_SIZE; ++c) {
                Cell cell = board.get(r, c);
                if (cell == Cell::Empty) continue;

                // Only score the START of each run (avoid double-counting).
                int pr = r - dr, pc = c - dc;
                if (pr >= 0 && pr < BOARD_SIZE && pc >= 0 && pc < BOARD_SIZE
                    && board.get(pr, pc) == cell)
                    continue;

                // Measure run length.
                int len = 0;
                int nr = r, nc = c;
                while (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE
                       && board.get(nr, nc) == cell) {
                    ++len; nr += dr; nc += dc;
                }

                // A flank is open when the adjacent cell is empty (the run can
                // extend there). A board edge or opponent stone closes it.
                bool left_open  = (pr >= 0 && pr < BOARD_SIZE &&
                                   pc >= 0 && pc < BOARD_SIZE &&
                                   board.get(pr, pc) == Cell::Empty);
                bool right_open = (nr >= 0 && nr < BOARD_SIZE &&
                                   nc >= 0 && nc < BOARD_SIZE &&
                                   board.get(nr, nc) == Cell::Empty);

                int open_ends = (left_open ? 1 : 0) + (right_open ? 1 : 0);
                if (open_ends == 0) continue; // both ends blocked — dead sequence

                int weight = 0;
                if      (len >= 5) weight = 500'000;
                else if (len == 4) weight = (open_ends == 2) ? 100'000 : 50'000;
                else if (len == 3) weight = (open_ends == 2) ?   5'000 :    500;
                else if (len == 2) weight = (open_ends == 2) ?     100 :     20;

                if (cell == Cell::Black) score += weight;
                else                     score -= weight;
            }
        }
    }

    return score;
}

// ── Minimax with alpha-beta pruning ──────────────────────────────────────────

// Why alpha-beta is safe (returns the same move as plain minimax):
// At a maximising node, a score >= beta means the minimiser above already has
// a better option — this whole subtree would never be chosen, so we cut early.
// At a minimising node, a score <= alpha means the maximiser above already has
// a better option — same argument. Pruned branches cannot contain the true
// minimax value, so the final result is identical; we just visit fewer nodes.
int AI::minimax(Game& game, int depth, bool is_maximizing, int alpha, int beta) {
    if (game.state() != GameState::Ongoing || depth == 0)
        return evaluate(game);

    std::vector<Move> candidates = generateCandidates(game);
    if (candidates.empty())
        return 0;

    if (is_maximizing) {
        int best = std::numeric_limits<int>::min();
        for (const Move& m : candidates) {
            MoveRecord rec = game.applyMove(m.row, m.col);
            int s = minimax(game, depth - 1, false, alpha, beta);
            game.undoMove(rec);
            if (s > best) best = s;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break; // β-cutoff: minimiser won't allow this path
        }
        return best;
    } else {
        int best = std::numeric_limits<int>::max();
        for (const Move& m : candidates) {
            MoveRecord rec = game.applyMove(m.row, m.col);
            int s = minimax(game, depth - 1, true, alpha, beta);
            game.undoMove(rec);
            if (s < best) best = s;
            if (best < beta) beta = best;
            if (alpha >= beta) break; // α-cutoff: maximiser won't allow this path
        }
        return best;
    }
}

// ── Public interface ──────────────────────────────────────────────────────────

Move AI::bestMove(Game& game, double& elapsed_ms) {
    auto t_start = std::chrono::steady_clock::now();

    // Generate candidates fast (no legality check), then filter with isLegalMove.
    // This is the only place we pay the double-three cost — once per move, for
    // ~50 candidates, rather than millions of times inside the search tree.
    std::vector<Move> raw       = generateCandidates(game);
    std::vector<Move> candidates;
    candidates.reserve(raw.size());
    for (const Move& m : raw) {
        if (game.isLegalMove(m.row, m.col))
            candidates.push_back(m);
    }

    bool ai_maximizes = (game.currentPlayer() == Player::Black);
    Move best_move  = {-1, -1};
    int  best_score = ai_maximizes
                          ? std::numeric_limits<int>::min()
                          : std::numeric_limits<int>::max();

    int alpha = std::numeric_limits<int>::min();
    int beta  = std::numeric_limits<int>::max();

    for (const Move& m : candidates) {
        MoveRecord rec = game.applyMove(m.row, m.col);
        int score      = minimax(game, SEARCH_DEPTH - 1, !ai_maximizes, alpha, beta);
        game.undoMove(rec);

        bool is_better = ai_maximizes ? (score > best_score) : (score < best_score);
        if (is_better) {
            best_score = score;
            best_move  = m;
            if (ai_maximizes) alpha = best_score;
            else              beta  = best_score;
        }
    }

    auto t_end = std::chrono::steady_clock::now();
    elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    return best_move;
}
