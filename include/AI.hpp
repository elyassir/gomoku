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
    // Depth 3 is the safe ceiling without move ordering.
    // Alpha-beta only prunes well when the best moves come first — with random
    // ordering the pruning is modest and depth 4 still explodes in mid-game.
    // Phase 9 (move ordering + iterative deepening) will unlock depth 5+.
    static constexpr int SEARCH_DEPTH = 3;

    // Collect all empty cells within 2 cells of any existing stone.
    // No legality check — used inside the search tree where the double-three
    // rule check (isLegalMove) would be called millions of times and dominate
    // runtime. Legality is enforced only at the root before the final move is played.
    std::vector<Move> generateCandidates(const Game& game) const;

    // Crude placeholder heuristic used until Phase 8 replaces it.
    // Scores the board from Black's perspective:
    //   positive → Black advantaged, negative → White advantaged.
    int evaluate(const Game& game) const;

    // Minimax with alpha-beta pruning.
    // alpha: best score Black can already guarantee on the path to this node.
    // beta:  best score White can already guarantee on the path to this node.
    // A branch is pruned when alpha >= beta — the opponent would never allow
    // this path, so there is no point examining remaining siblings.
    int minimax(Game& game, int depth, bool is_maximizing, int alpha, int beta);
};
