# Gomoku — 42 Project

A fully rule-correct Gomoku game with a strong AI opponent, built in C++ with SFML.

---

## Build

Requires SFML 2.x installed at `~/goinfre/sfml/` (standard 42 cluster layout).

```bash
make        # build
make re     # clean rebuild
make clean  # remove object files
make fclean # remove objects + binary
```

Produces the executable `Gomoku`.

---

## Run

```bash
./Gomoku
```

A **startup lobby** appears before the game begins. Choose:

| Key | Action |
|-----|--------|
| `B` | Play as **Black** (you move first) |
| `W` | Play as **White** (AI moves first) |

Then choose AI difficulty:

| Key | Difficulty | Time limit | Search depth |
|-----|-----------|-----------|-------------|
| `1` | Easy | 150 ms | 3 |
| `2` | Pro | 500 ms | 10 |

---

## In-game controls

| Key / Input | Action |
|-------------|--------|
| **Left click** | Place a stone on the board |
| `H` | Toggle AI hint — highlights the suggested move with a green ring |
| `D` | Toggle debug view — shows the AI's top-5 candidates with minimax scores |
| `Z` | **Undo** — in vs-AI mode undoes both the human move and the AI response; in hotseat undoes one move |
| `P` | **Print history** — reprints the full move log to the terminal |
| `Tab` | Toggle **hotseat mode** — both players become human-controlled (Tab again to return to vs-AI) |
| `R` | Restart the game (keeps color and difficulty settings) |
| `Escape` | Quit |

---

## HUD

The panel below the board shows:

- **Turn status** — whose turn it is, or `"[Color] thinking..."` while the AI computes
- **Capture counts** — stones captured by each player (`pairs × 2`)
- **AI timer** — milliseconds and depth reached after each AI move (gold, top-right)
- **Hint status** — `"H = hint"` when available, or `"Hint: XN"` when active (bottom-right)
- **Hotseat label** — `"Hotseat   Tab: vs AI"` in blue when hotseat mode is active

---

## Rules implemented

### Win conditions

- **Alignment** — 5 or more consecutive stones in a row, column, or diagonal. A 5-in-a-row is only a win if the opponent cannot immediately break it by capturing a pair from within that line.
- **Capture** — capturing 10 opponent stones (5 pairs) wins, and this can override an opponent's 5-in-a-row.

### Capture

A capture fires when a player completes the pattern **`Me Opp Opp Me`** in any of the 8 directions. The two opponent stones are removed. Moving into a flanked position (`Opp Me Opp`) is **safe** — captures are only triggered by the player who *completes* the flank.

### Double-three rule

A move is illegal if it simultaneously creates two or more **free-threes** — a free-three being three stones that can become an open four (`_ X X X X _`) in exactly one move. **Exception:** a move that also captures a pair is always legal even if it creates a double-three.

---

## AI

The AI uses **minimax with alpha-beta pruning** and **iterative deepening**:

1. **Candidate generation** — only empty cells within 2 intersections of an existing stone are considered (~30–50 per position). This shrinks the branching factor from 361 to a manageable range.
2. **Iterative deepening** — the search runs at depth 1, 2, 3, … until the time budget runs out. The best move from the last *complete* depth is returned; partial results from the timed-out depth are discarded.
3. **Alpha-beta pruning** — branches that cannot influence the final result are skipped. With best-first ordering this approaches `O(b^(d/2))` nodes instead of `O(b^d)`.
4. **Move ordering** — before each recursive call, candidates are scored with a depth-0 evaluate and sorted best-first. This makes alpha-beta fire on the first sibling and prune aggressively. Ordering is skipped at depth 1 (overhead > benefit at that ply) and capped at 15 candidates per node.
5. **Heuristic evaluation** — non-terminal boards are scored from Black's perspective by scanning all 4 axes for unbroken runs and classifying them by length and open ends:

   | Pattern | Score |
   |---------|-------|
   | 5 in a row | ±500 000 |
   | Open four (`_ XXXX _`) | ±100 000 |
   | Half-four (one end blocked) | ±50 000 |
   | Open three | ±5 000 |
   | Half-three | ±500 |
   | Open two | ±100 |
   | Half-two | ±20 |
   | Capture progress | ±300 per stone |

   A **tempo bonus** applies at leaf nodes: if the current player already has an open four, they will execute it next turn regardless of the opponent — the position is scored as near-win (±900 000) to prevent the search from incorrectly "cancelling" threats by building a counter-threat.

6. **Crash-proofing** — all `applyMove`/`undoMove` pairs in the search tree are wrapped in a `ScopedMove` RAII guard, so `undoMove` is always called even if `std::bad_alloc` fires inside the search. A top-level `try/catch` in `main` prints a clean `[Fatal]` message and exits cleanly instead of crashing.

---

## Project structure

```
Gomoku/
├── Makefile
├── include/
│   ├── Board.hpp      — 19×19 grid, Cell enum (Empty/Black/White)
│   ├── Game.hpp       — full game state, make/unmake interface, all rules
│   └── AI.hpp         — iterative deepening minimax with configurable time/depth
└── src/
    ├── Board.cpp
    ├── Game.cpp       — placeStone, applyMove, undoMove, captures, alignment, double-three
    ├── AI.cpp         — generateCandidates, evaluate, orderMoves, minimax, bestMove
    └── main.cpp       — SFML window, lobby, HUD, hint, debug view, undo stack, history
```

---

## Bonus features

| Feature | How to use |
|---------|------------|
| Choose color | Startup lobby — `B` or `W` before the game begins |
| AI difficulty | Startup lobby — `1` Easy or `2` Pro |
| Play as White | Pick `W`; the AI (Black) plays the first move automatically |
| Undo | `Z` — undoes human + AI move pair in vs-AI mode, one move in hotseat |
| Move history | `P` — reprints the full numbered move log to the terminal |
| Hotseat mode | `Tab` — both players human-controlled, H hint works for either side |
| AI hint | `H` — asks the AI for a move suggestion at any point on your turn |
| Debug view | `D` — shows AI's top-5 candidates with minimax scores on the board |
