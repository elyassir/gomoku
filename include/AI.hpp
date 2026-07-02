#pragma once
#include "Game.hpp"
#include <vector>

// The move the AI selects: just the board intersection to play.
struct Move {
    int row, col;
};

class AI {
public:
    // Find the best legal move for the current player using plain minimax.
    // Writes the think time in milliseconds to elapsed_ms so the UI can display it.
    // Returns {-1, -1} if no legal moves exist (shouldn't happen in a live game).
    Move bestMove(Game& game, double& elapsed_ms);

private:
    // Depth 4 is viable with move ordering: ordering shrinks the tree from O(b^d)
    // toward O(b^(d/2)), making depth 4 cheaper than depth 3 without ordering.
    static constexpr int SEARCH_DEPTH    = 4;
    // Max candidates explored per node after ordering. Without a cap, ordering
    // evaluates ~50 candidates × 50^(d-1) nodes = ordering cost ≈ full search cost.
    // Capping at 15 reduces the per-node multiplier to 15^(d-1), making the
    // ordering overhead negligible while keeping the quality candidates.
    static constexpr int MAX_CANDIDATES  = 15;

    // Collect all empty cells within 2 cells of any existing stone.
    // No legality check — used inside the search tree where the double-three
    // rule check (isLegalMove) would be called millions of times and dominate
    // runtime. Legality is enforced only at the root before the final move is played.
    std::vector<Move> generateCandidates(const Game& game) const;

    // Crude placeholder heuristic used until Phase 8 replaces it.
    // Scores the board from Black's perspective:
    //   positive → Black advantaged, negative → White advantaged.
    int evaluate(const Game& game) const;

    // Score candidates with a depth-0 evaluate and return them sorted best-first.
    // want_high=true for the maximiser, false for the minimiser.
    std::vector<std::pair<int,Move>> orderMoves(
        Game& game, const std::vector<Move>& moves, bool want_high);

    // Minimax with alpha-beta pruning.
    // alpha: best score Black can already guarantee on the path to this node.
    // beta:  best score White can already guarantee on the path to this node.
    // A branch is pruned when alpha >= beta — the opponent would never allow
    // this path, so there is no point examining remaining siblings.
    int minimax(Game& game, int depth, bool is_maximizing, int alpha, int beta);
};
