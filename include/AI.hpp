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
    // Find the best legal move for the current player using iterative deepening.
    // Searches depth 1, 2, 3, … until the 500ms budget is exhausted, then
    // returns the best move from the last *complete* depth.
    // Writes the actual think time in milliseconds to elapsed_ms.
    Move bestMove(Game& game, double& elapsed_ms);

private:
    // Hard wall on per-move think time.
    static constexpr int TIME_LIMIT_MS  = 500;
    // Iterative deepening will never exceed this depth.
    static constexpr int MAX_DEPTH      = 10;
    // Max candidates explored per node after ordering. Without a cap, ordering
    // evaluates ~50 candidates × 50^(d-1) nodes = ordering cost ≈ full search cost.
    // Capping at 15 reduces the per-node multiplier to 15^(d-1).
    static constexpr int MAX_CANDIDATES = 15;

    // Shared deadline set once at the start of bestMove and read by every minimax call.
    std::chrono::steady_clock::time_point _deadline;
    // Flipped to true the first time a node exceeds _deadline; unwinds the whole tree.
    bool _time_up = false;

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
