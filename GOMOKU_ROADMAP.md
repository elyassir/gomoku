# Gomoku — Project Roadmap

A step-by-step build plan for the 42 Gomoku project, designed so you **understand every piece before moving on**. We don't write the AI until the game is fully playable and rule-correct.

---

## How we'll work together (the protocol)

This is the loop we repeat for every single step. We do **not** skip ahead.

1. **I announce the step.** Its goal, which files/functions it touches, and the *one* concept it teaches.
2. **I produce the change.** Either the code directly, or a tight prompt you paste into Claude Code / Cursor — your choice.
3. **I show what changed and why.** A plain-language walkthrough of the diff: what's new, what moved, what each part does.
4. **You confirm it clicks.** You build/run it, and either restate the idea back to me or ask questions. If anything is fuzzy, we stay here.
5. **Only then do we advance.** No moving on with a black box behind us.

The Code Quality Mandate: Readable & Documented
To ensure you can easily defend this project to a grader, we will never write "clever" but unreadable code. Every single function and complex block we write will include inline comments explaining why it does what it does.

Self-documenting layout: Clear variable names (has_vertical_alignment) instead of cryptic shorthand (v_al).

Strategic commenting: Documenting the intent of algorithms (like bit manipulations, pruning choices, or line scans) so you can read any file cold during defense and know exactly what's happening.

> Why this matters for *you specifically*: the defense requires you to **thoroughly explain minimax and your heuristic to someone who knows nothing about them**. The subject literally says if you can't explain it, you get zero points for it. So "understand before advancing" isn't optional polish — it's the grading criterion.

Steps stay small. A "phase" below often contains several steps. We never dump a whole phase at once.

---

## Before step 1: two decisions

**Language.** Performance is the whole game here (depth 10, <0.5s). Pick a fast compiled language:

- **C++** — my default recommendation. You already know it, it fits the 42 toolchain (Makefile, etc.), and GUI libs like **raylib** or **SFML** make the window + timer trivial. raylib especially is a few lines to get a board on screen.
- **Rust** — strong alternative. The subject demands *"should not crash in any circumstances, even out of memory"*; Rust's safety eliminates whole classes of crash bugs for free. GUI via **macroquad** or **ggez**. You've used Rust before, so it's viable.

Either is fine. Tell me your pick when we start step 1 and the roadmap adapts (GUI library, Makefile shape).

**Make/unmake, not copy.** Decide now: the AI will explore *millions* of board states. We will **never copy the board** to try a move. We'll *apply* a move and *undo* it (restoring any captured stones). Building this in from Phase 3 saves a painful rewrite later. I'll flag it when we get there.

---

## The shape of the project (why this order)

```
RULES FIRST  →  AI SECOND  →  SPEED THIRD  →  POLISH/DEFENSE LAST

Phase 0   Scaffolding + empty board on screen
Phase 1   Place stones, take turns (hotseat, no rules)
Phase 2   Win by aligning 5+
Phase 3   Captures (+ the capture-win)
Phase 4   No double-three rule
Phase 5   Endgame-capture win nuances
        ── game is now fully rule-correct & playable ──
Phase 6   Minimax skeleton (weak but legal AI)
Phase 7   Alpha-beta pruning
Phase 8   The heuristic (the hardest part)
Phase 9   Speed: reach depth 10 in <0.5s
Phase 10  Required UI: timer, hints, debug view
Phase 11  Defense prep + crash-proofing
Bonus     Swap/Swap2/Pro start rules, etc.
```

You get something *playable by humans* at the end of Phase 5. That's the morale checkpoint — everything after is making the computer good at a game that already works.

---

## Phase 0 — Scaffolding

**Goal:** `make` produces an executable named `Gomoku` that opens a window showing an empty 19×19 board.

- Git repo + folder structure.
- **Makefile** with rules `$(NAME)`, `all`, `clean`, `fclean`, `re`. It **must not relink** (the subject checks this).
- Board data structure: a 19×19 grid where each cell is `Empty / Black / White`.
- Open a window, draw the grid lines and intersections.

**Checkpoint:** `make` → run → an empty goban appears. `make` again does nothing (no relink).

**Teaches:** project skeleton, your render loop, how the board maps to screen pixels.

---

## Phase 1 — Place stones & turns (hotseat, no rules)

**Goal:** two humans click to drop alternating stones.

- Map mouse click → board coordinate.
- Reject clicks on occupied cells / off-board.
- Track whose turn it is; alternate after each valid move.
- Draw stones.

**Checkpoint:** you and a friend can fill the board with alternating stones.

**Teaches:** input → board-state → render, the core game loop. No rules yet on purpose.

---

## Phase 2 — Win by alignment

**Goal:** aligning **5 or more** stones ends the game.

- After each move, scan the **4 directions** (horizontal, vertical, two diagonals) through the placed stone.
- Count consecutive same-color stones; 5+ → that player wins.
- Show a win state.

> Only check lines passing *through the stone just played* — no need to rescan the whole board. This habit matters later for speed.

**Checkpoint:** five in a row (any direction) declares a winner.

**Teaches:** directional line scanning — you'll reuse this exact idea in captures, free-threes, and the heuristic.

---

## Phase 3 — Captures

**Goal:** flanking a pair removes it; capturing 10 stones (5 pairs) wins.

- After a move, in each of the 8 directions, check the pattern **`Me, Opp, Opp, Me`** → remove those two opponent stones.
- Per-player capture counter; reaching 10 captured = win.
- **Subtlety from the appendix:** moving *into* a spot that looks flanked does **not** get you captured. Capture only triggers from the move of the *flanking* player completing `Me,Opp,Opp,Me`. The example (Red plays between two Blues safely) must work correctly.
- **Build make/unmake here:** a move records which stones (if any) it captured, so undo can put them back.

**Checkpoint:** captures fire correctly; the "safe to move between two enemies" case works; 10 captures wins.

**Teaches:** board mutation + reversible moves (the foundation the whole AI stands on).

---

## Phase 4 — No double-three

**Goal:** forbid any move that creates **two free-threes at once**.

This is the fiddliest rule. Pace it as its own steps.

- Define a **free-three**: three stones that, if unblocked, can become an *open four* (four in a row with **both** ends open) in one move. This includes broken shapes like `_XX_X_`, not just `_XXX_`.
- After a candidate move, count how many distinct directions yield a free-three. **Two or more → illegal**, reject the move.
- **Important exception (appendix):** it **is** legal to create a double-three *by capturing a pair*. Handle that case.

**Checkpoint:** the appendix's example position rejects the forbidden move, and the "blue stone at b" variant allows it.

**Teaches:** pattern recognition over lines — directly transferable to the heuristic.

---

## Phase 5 — Endgame-capture nuances

**Goal:** make the win conditions precisely match the subject.

- A 5-alignment **only wins if the opponent cannot break it by capturing a pair** out of that line. So "5 in a row" is now a *conditional* win — check for a breaking capture first.
- If a player has already lost **4 pairs** and the opponent can capture one more, the opponent **wins by capture** (this can override an alignment).

**Checkpoint:** a 5-in-a-row that's capturable does *not* immediately win; the 4-pairs-lost edge case resolves correctly.

**Teaches:** how the two win conditions interact — exactly the kind of edge case graders probe.

> 🎉 **Milestone:** the game is now fully rule-correct and playable human-vs-human. Take a breath. Everything next is about the AI.

---

## Phase 6 — Minimax skeleton

**Goal:** a legal (if weak) computer opponent.

- **Candidate move generation — the single most important decision in the whole project.** The board has 361 cells; searching all of them to depth 10 is `361^10` ≈ impossible. Instead, only consider empty cells **adjacent (within 1–2 cells) to an existing stone.** This shrinks the branching factor enormously and is what makes depth 10 even thinkable.
- Implement plain **minimax**: maximizing player (AI) vs minimizing player (you), to a small fixed depth (say 3–4 for now).
- A **placeholder heuristic** (e.g. just count alignments crudely) — we improve it in Phase 8.
- Wire it as the AI player; AI plays legal moves.

**Checkpoint:** AI responds with legal moves, even if it plays badly. Show the move timer (we'll formalize it in Phase 10).

**Teaches:** the minimax recursion and *why narrowing candidate moves is non-negotiable.* This is the algorithm you must explain at defense — we'll go slow.

---

## Phase 7 — Alpha-beta pruning

**Goal:** same decisions, dramatically fewer nodes explored.

- Add **alpha-beta** to the minimax recursion: stop exploring branches that can't beat what you've already found.
- Verify it returns the *same move* as plain minimax (correctness check) but visits far fewer nodes.

**Checkpoint:** identical move choice to Phase 6, measurably faster / deeper reachable.

**Teaches:** pruning — and you'll be able to explain *why* it's safe (doesn't change the result).

---

## Phase 8 — The heuristic (the hard part)

**Goal:** an evaluation function that scores a non-terminal board well, fast.

The subject calls this *"actually the hardest part"* and it's right. Iterative by nature.

- Score a position by recognizing **patterns** in all 4 directions: open/closed twos, threes, fours, fives, plus capture count and **capture vulnerability** (pairs of yours that the opponent could flank).
- Assign weights (a five = win, an open four ≈ winning, an open three is strong, etc.) and **tune them by playing against it.**
- Keep it *cheap* — it runs at every leaf of a huge tree.

**Checkpoint:** the AI starts making moves that look genuinely sensible — blocks your threats, builds its own.

**Teaches:** the second thing you must explain thoroughly at defense. We'll make sure you can justify every weight.

---

## Phase 9 — Speed: depth 10 in under 0.5s

**Goal:** hit the hard validation bar. This is its own engineering phase; budget real time for it.

Layer these in, measuring after each:

- **Move ordering** — try the most promising moves first so alpha-beta prunes more. Biggest single win.
- **Iterative deepening** — search depth 1, 2, 3… reusing results; lets you stop when time's up and feeds move ordering.
- **Transposition table (Zobrist hashing)** — cache evaluations of positions you've seen before via a different move order.
- **Incremental evaluation** — update the score from the last move instead of rescanning the whole board at every node.
- **Tighten the candidate set** — consider only the top-K most promising moves, not every neighbor.
- *(Optional, advanced)* **bitboards** for very fast pattern matching.

**Checkpoint:** average move time < 0.5s while searching ≥ 10 plies. **This is a hard gate** — without it you can't fully validate.

**Teaches:** real game-AI engineering. Each optimization is independently explainable.

---

## Phase 10 — Required UI & features

**Goal:** every mandatory interface requirement, ticked off.

- **Move timer showing AI think time** — *mandatory.* The subject says: **no timer, no validation.** Do not skip.
- **Move-suggestion / hint** feature for the hotseat (human-vs-human) mode — easy now that the AI exists; just run the AI on the current human's position.
- **Debug / reasoning view** — the subject strongly recommends a way to inspect the AI's thinking (top candidate moves + scores). Hugely helpful for tuning *and* for explaining yourself at defense.
- Capture counters, turn indicator, win screen, restart.

**Checkpoint:** timer visible, hints work, you can watch the AI "think."

---

## Phase 11 — Defense prep & crash-proofing

**Goal:** bulletproof and explainable.

- **Never crash — even on out-of-memory.** A crash = grade 0. Handle allocation failures, bad input, board edges gracefully.
- Re-test every rule edge case (the capture/alignment interactions, double-three + capture exception).
- Rehearse the two explanations out loud: **minimax** and **your heuristic**, to an imaginary grader who's never heard of either.

**Checkpoint:** you can run it under pressure and explain every line.

---

## Bonus (only if mandatory is *perfect*)

The subject is blunt: bonuses count **only if the mandatory part is flawless**. Don't touch these until then.

- Selectable **opening rules**: Standard, **Pro**, **Swap**, **Swap2**.
- Configurable rule toggles at game start.
- Up to 5 bonuses are considered — pick ones that are genuinely interesting (e.g. difficulty levels, opening book).

---

## Defense survival notes (keep these in view the whole time)

- The grade hinges on **explaining minimax and the heuristic**, not just on them working.
- **Timer is mandatory.** **No relink** in the Makefile. **No crashes, ever.**
- Depth ≥ 10 and < 0.5s average are **hard gates** for full marks.
- A 5-alignment isn't an automatic win — captures can break it.

---

*When you're ready, tell me your language choice and we start at Phase 0, Step 1. I'll go one small step at a time, show you exactly what changed, and we won't advance until it makes sense to you.*
