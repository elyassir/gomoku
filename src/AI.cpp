#include "../include/AI.hpp"
#include <chrono>
#include <iostream>
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

    // Track each side's strongest individual sequence for the tempo check below.
    int black_max = 0, white_max = 0;

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

                if (cell == Cell::Black) {
                    score += weight;
                    if (weight > black_max) black_max = weight;
                } else {
                    score -= weight;
                    if (weight > white_max) white_max = weight;
                }
            }
        }
    }

    // Tempo: at a leaf the current player's open four is a guaranteed win next move
    // (the opponent can only block one end at a time). Without this, the search can
    // "cancel" an opponent's open four by building its own, netting to 0 — but the
    // opponent goes first and wins before we ever get our open four off the ground.
    if (game.currentPlayer() == Player::Black && black_max >= 100'000)
        return  900'000;
    if (game.currentPlayer() == Player::White && white_max >= 100'000)
        return -900'000;

    return score;
}

// ── Move ordering ─────────────────────────────────────────────────────────────

// Score each candidate with a depth-0 evaluate (apply → score → undo), sort
// best-first for the given side, and return at most MAX_CANDIDATES entries.
//
// Why ordering helps: alpha-beta prunes once alpha >= beta. That fires only
// after a GOOD move tightens the window. With random order, good moves come
// last and almost no pruning happens. With best-first, the first candidate
// tightens the window immediately and siblings are cut after one evaluate call.
// This shrinks the tree from O(b^d) toward O(b^(d/2)).
//
// Why we cap at MAX_CANDIDATES: without a cap, the ordering evaluates ~50
// candidates per node. At depth=4 that is 50^3 = 125 000 extra evaluate calls —
// about as expensive as the whole unordered search. Limiting to 15 drops the
// per-node multiplier to 15^3 = 3 375, making the ordering overhead negligible
// while the pruning gain still dominates.
//
// Why we skip ordering at depth == 1: depth=1 nodes call evaluate() on leaves
// directly — there is nothing left to prune below them. The 50-eval ordering
// overhead per node exceeds the tiny pruning benefit at this last ply.
using SM = std::pair<int, Move>;  // (shallow_score, move) for sort

std::vector<SM> AI::orderMoves(
    Game& game, const std::vector<Move>& moves, bool want_high)
{
    std::vector<SM> out;
    out.reserve(moves.size());
    for (const Move& m : moves) {
        MoveRecord rec = game.applyMove(m.row, m.col);
        out.push_back({evaluate(game), m});
        game.undoMove(rec);
    }
    if (want_high)
        std::sort(out.begin(), out.end(),
            [](const SM& a, const SM& b){ return a.first > b.first; });
    else
        std::sort(out.begin(), out.end(),
            [](const SM& a, const SM& b){ return a.first < b.first; });
    if ((int)out.size() > MAX_CANDIDATES)
        out.resize(MAX_CANDIDATES);
    return out;
}

// ── Minimax with alpha-beta pruning ──────────────────────────────────────────

// Why alpha-beta is safe (returns the same move as plain minimax):
// At a maximising node, a score >= beta means the minimiser above already has
// a better option — this whole subtree would never be chosen, so we cut early.
// At a minimising node, a score <= alpha means the maximiser above already has
// a better option — same argument. Pruned branches cannot contain the true
// minimax value, so the final result is identical; we just visit fewer nodes.
//
// Time-abort: if the deadline is exceeded at any node, _time_up is set and the
// function returns 0 immediately. Every caller checks _time_up and propagates
// the abort upward so bestMove can discard the partial result and use the last
// complete depth instead.
int AI::minimax(Game& game, int depth, bool is_maximizing, int alpha, int beta) {
    // Check time before doing any work at this node.
    if (_time_up) return 0;
    if (std::chrono::steady_clock::now() >= _deadline) {
        _time_up = true;
        return 0;
    }

    if (game.state() != GameState::Ongoing || depth == 0)
        return evaluate(game);

    std::vector<Move> raw = generateCandidates(game);
    if (raw.empty())
        return 0;

    // At depth=1 (just before leaves) ordering costs more than it saves:
    // the 50-eval overhead per node exceeds the alpha-beta benefit at one ply.
    // Use unordered raw candidates there, capped to MAX_CANDIDATES.
    std::vector<SM> ordered;
    if (depth >= 2) {
        ordered = orderMoves(game, raw, is_maximizing);
    } else {
        int lim = std::min((int)raw.size(), MAX_CANDIDATES);
        ordered.reserve(lim);
        for (int i = 0; i < lim; ++i)
            ordered.push_back({0, raw[i]});
    }

    if (is_maximizing) {
        int best = std::numeric_limits<int>::min();
        for (const SM& sm : ordered) {
            MoveRecord rec = game.applyMove(sm.second.row, sm.second.col);
            int s = minimax(game, depth - 1, false, alpha, beta);
            game.undoMove(rec);
            if (_time_up) return 0; // result is garbage — abort immediately
            if (s > best) best = s;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break; // β-cutoff: minimiser won't allow this path
        }
        return best;
    } else {
        int best = std::numeric_limits<int>::max();
        for (const SM& sm : ordered) {
            MoveRecord rec = game.applyMove(sm.second.row, sm.second.col);
            int s = minimax(game, depth - 1, true, alpha, beta);
            game.undoMove(rec);
            if (_time_up) return 0; // result is garbage — abort immediately
            if (s < best) best = s;
            if (best < beta) beta = best;
            if (alpha >= beta) break; // α-cutoff: maximiser won't allow this path
        }
        return best;
    }
}

// ── Public interface ──────────────────────────────────────────────────────────

// Iterative deepening: run minimax at depth 1, 2, 3, … until the 500ms budget
// runs out, then return the best move from the last *complete* depth.
//
// Why iterative deepening is not wasteful: each shallower search informs move
// ordering for the next (via orderMoves). At depth d the ordering is based on
// depth-(d-1) results, making alpha-beta prune far more aggressively than a
// cold start at depth d. The total cost of depths 1..d is roughly equal to the
// cost of depth d alone, so the "wasted" shallower passes are negligible.
Move AI::bestMove(Game& game, double& elapsed_ms, int& reached_depth) {
    auto t_start = std::chrono::steady_clock::now();
    _deadline    = t_start + std::chrono::milliseconds(TIME_LIMIT_MS);

    // Generate candidates fast (no legality check), then filter with isLegalMove.
    // This is the only place we pay the double-three cost — once per move, for
    // ~50 candidates, rather than millions of times inside the search tree.
    std::vector<Move> raw = generateCandidates(game);
    std::vector<Move> candidates;
    candidates.reserve(raw.size());
    for (const Move& m : raw) {
        if (game.isLegalMove(m.row, m.col))
            candidates.push_back(m);
    }

    if (candidates.empty())
        return {-1, -1};

    bool ai_maximizes = (game.currentPlayer() == Player::Black);
    Move best_move    = candidates[0]; // fallback: first legal move
    reached_depth     = 0;

    for (int depth = 1; depth <= MAX_DEPTH; ++depth) {
        _time_up = false;

        int  best_score = ai_maximizes
                              ? std::numeric_limits<int>::min()
                              : std::numeric_limits<int>::max();
        int  alpha      = std::numeric_limits<int>::min();
        int  beta       = std::numeric_limits<int>::max();
        Move candidate  = {-1, -1};

        // Order root candidates best-first; this uses depth-0 evals but the
        // result from the previous iteration already lives in the score that
        // orderMoves reads, so ordering improves with each deeper pass.
        std::vector<SM> ordered = orderMoves(game, candidates, ai_maximizes);

        std::vector<SM> this_scores; // root scores for this depth
        this_scores.reserve(ordered.size());

        for (const SM& sm : ordered) {
            if (_time_up) break;

            const Move& m   = sm.second;
            MoveRecord  rec = game.applyMove(m.row, m.col);
            int score       = minimax(game, depth - 1, !ai_maximizes, alpha, beta);
            game.undoMove(rec);

            if (_time_up) break; // partial result — discard

            this_scores.push_back({score, m});

            bool is_better = ai_maximizes ? (score > best_score) : (score < best_score);
            if (is_better) {
                best_score = score;
                candidate  = m;
                if (ai_maximizes) alpha = best_score;
                else              beta  = best_score;
            }
        }

        // Only commit a result if the full depth completed without a timeout.
        if (!_time_up && candidate.row != -1) {
            best_move     = candidate;
            reached_depth = depth;
            // Sort best-first and expose for the debug view.
            if (ai_maximizes)
                std::sort(this_scores.begin(), this_scores.end(),
                    [](const SM& a, const SM& b){ return a.first > b.first; });
            else
                std::sort(this_scores.begin(), this_scores.end(),
                    [](const SM& a, const SM& b){ return a.first < b.first; });
            _debug_moves = std::move(this_scores);
        }

        if (_time_up) break;
    }

    auto t_end = std::chrono::steady_clock::now();
    elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    std::cout << "[AI] depth=" << reached_depth
              << "  time=" << static_cast<int>(elapsed_ms) << "ms\n";

    return best_move;
}
