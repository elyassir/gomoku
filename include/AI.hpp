#pragma once
#include "Game.hpp"
#include <vector>
#include <chrono>

// The move the AI selects: just the board intersection to play.
struct Move {
    int row, col;
};

class AI {
public:
    // time_ms: hard wall on per-move think time (default 500ms).
    // max_depth: iterative deepening never exceeds this (default 10).
    // Easy difficulty: AI(150, 3).  Pro: AI(500, 10).
    explicit AI(int time_ms = 500, int max_depth = 10)
        : _time_limit_ms(time_ms), _max_depth(max_depth) {}

    // Find the best legal move for the current player using iterative deepening.
    // Searches depth 1, 2, 3, … until the budget is exhausted, then returns the
    // best move from the last *complete* depth.
    // Writes the actual think time (ms) to elapsed_ms and the depth reached to reached_depth.
    Move bestMove(Game& game, double& elapsed_ms, int& reached_depth);

    // Root candidates from the last complete depth, sorted best-first.
    // Populated by bestMove so the UI can show the AI's top moves and scores.
    const std::vector<std::pair<int,Move>>& debugMoves() const { return _debug_moves; }

private:
    // Per-move time budget and search depth ceiling — set by constructor.
    int _time_limit_ms;
    int _max_depth;

    // Max candidates explored per node after ordering. Without a cap, ordering
    // evaluates ~50 candidates × 50^(d-1) nodes = ordering cost ≈ full search cost.
    // Capping at 15 reduces the per-node multiplier to 15^(d-1).
    static constexpr int MAX_CANDIDATES = 15;

    // Shared deadline set once at the start of bestMove and read by every minimax call.
    std::chrono::steady_clock::time_point _deadline;
    // Flipped to true the first time a node exceeds _deadline; unwinds the whole tree.
    bool _time_up = false;
    // Root candidates from the last complete depth — updated by bestMove.
    std::vector<std::pair<int,Move>> _debug_moves;

    // Collect all empty cells within 2 cells of any existing stone.
    // No legality check — used inside the search tree where the double-three
    // rule check (isLegalMove) would be called millions of times and dominate
    // runtime. Legality is enforced only at the root before the final move is played.
    std::vector<Move> generateCandidates(const Game& game) const;

    // Score the board from Black's perspective (positive = Black winning).
    int evaluate(const Game& game) const;

    // Score candidates with a depth-0 evaluate and return them sorted best-first.
    // want_high=true for the maximiser, false for the minimiser.
    std::vector<std::pair<int,Move>> orderMoves(
        Game& game, const std::vector<Move>& moves, bool want_high);

    // Minimax with alpha-beta pruning.
    // Returns 0 immediately (and sets _time_up) if the deadline has passed —
    // the caller must discard any result produced after _time_up is set.
    int minimax(Game& game, int depth, bool is_maximizing, int alpha, int beta);
};
