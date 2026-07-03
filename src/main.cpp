#include <SFML/Graphics.hpp>
#include <cmath>     // std::round
#include <string>    // window title construction
#include <iostream>  // move log
#include <iomanip>   // std::setw
#include "../include/Game.hpp"
#include "../include/AI.hpp"

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int   WINDOW_SIZE = 700;
static constexpr int   MARGIN      = 40;
// CELL_SIZE: pixel distance between adjacent intersections.
// Divide by (BOARD_SIZE - 1) because 19 lines form 18 gaps, not 19.
static constexpr float CELL_SIZE = (WINDOW_SIZE - 2.f * MARGIN) / (BOARD_SIZE - 1);

// Stone radius is slightly less than half a cell so adjacent stones don't overlap.
static constexpr float STONE_RADIUS = CELL_SIZE * 0.44f;

// ── Coordinate helpers ────────────────────────────────────────────────────────

// Board → screen: returns the pixel centre of intersection (row, col).
// Every drawing and hit-test function uses this so the math lives in one place.
static sf::Vector2f boardToScreen(int row, int col) {
    return {
        MARGIN + col * CELL_SIZE,   // X grows left→right  (col index)
        MARGIN + row * CELL_SIZE    // Y grows top→bottom  (row index)
    };
}

// Screen → board: converts a mouse pixel (px, py) to the nearest intersection.
// Returns true if that intersection is inside the 19x19 grid and sets row/col.
// Returns false if the click was outside the valid board area.
//
// std::round() is the key: it snaps any pixel to the nearest intersection,
// so a player doesn't need to click pixel-perfectly on a line crossing.
static bool screenToBoard(int pixel_x, int pixel_y, int& row, int& col) {
    float col_f = (pixel_x - MARGIN) / CELL_SIZE;
    float row_f = (pixel_y - MARGIN) / CELL_SIZE;

    col = static_cast<int>(std::round(col_f));
    row = static_cast<int>(std::round(row_f));

    return (row >= 0 && row < BOARD_SIZE &&
            col >= 0 && col < BOARD_SIZE);
}

// ── Renderers ─────────────────────────────────────────────────────────────────

static void drawBoard(sf::RenderWindow& window) {
    // 1. Warm wood background — matches a traditional goban.
    sf::RectangleShape background({ (float)WINDOW_SIZE, (float)WINDOW_SIZE });
    background.setFillColor(sf::Color(220, 179, 92));
    window.draw(background);

    // 2. Grid lines — one horizontal + one vertical per row/column.
    const sf::Color line_color(80, 50, 20);
    const float     grid_span = WINDOW_SIZE - 2.f * MARGIN;

    for (int i = 0; i < BOARD_SIZE; ++i) {
        sf::RectangleShape horizontal_line({ grid_span, 1.f });
        horizontal_line.setPosition(MARGIN, MARGIN + i * CELL_SIZE);
        horizontal_line.setFillColor(line_color);
        window.draw(horizontal_line);

        sf::RectangleShape vertical_line({ 1.f, grid_span });
        vertical_line.setPosition(MARGIN + i * CELL_SIZE, MARGIN);
        vertical_line.setFillColor(line_color);
        window.draw(vertical_line);
    }

    // 3. Star points (hoshi) at the 9 standard positions on a 19x19 board.
    const int hoshi_indices[] = { 3, 9, 15 };
    for (int r : hoshi_indices) {
        for (int c : hoshi_indices) {
            sf::CircleShape dot(3.f);
            dot.setFillColor(line_color);
            dot.setOrigin(3.f, 3.f);
            dot.setPosition(boardToScreen(r, c));
            window.draw(dot);
        }
    }
}

// Draw every non-empty stone currently on the board.
// Black stones are near-black with a thin dark outline.
// White stones are near-white with a dark outline so they're visible on the wood background.
static void drawStones(sf::RenderWindow& window, const Board& board) {
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            Cell cell = board.get(row, col);
            if (cell == Cell::Empty)
                continue; // nothing to draw at this intersection

            sf::CircleShape stone(STONE_RADIUS);
            stone.setOrigin(STONE_RADIUS, STONE_RADIUS); // anchor at centre, not top-left
            stone.setPosition(boardToScreen(row, col));
            stone.setOutlineThickness(1.f);

            if (cell == Cell::Black) {
                stone.setFillColor(sf::Color(20, 20, 20));
                stone.setOutlineColor(sf::Color(0, 0, 0));
            } else { // White
                stone.setFillColor(sf::Color(245, 245, 245));
                stone.setOutlineColor(sf::Color(60, 60, 60));
            }

            window.draw(stone);
        }
    }
}

// ── Move logger ───────────────────────────────────────────────────────────────

// Columns A–S (19 letters), rows 1–19 from the top — standard Gomoku notation.
static std::string toNotation(int row, int col) {
    return std::string(1, static_cast<char>('A' + col)) + std::to_string(row + 1);
}

static void logMove(int move_num, const char* player, int row, int col,
                    int captures, double ai_ms) {
    // std::left is sticky in std::cout — reset to std::right after each use
    // so the move number stays right-aligned in subsequent calls.
    std::cout << "[" << std::right << std::setw(2) << move_num << "] "
              << std::left << std::setw(10) << player << std::right
              << toNotation(row, col);
    if (captures > 0)
        std::cout << "  captures:" << captures;
    if (ai_ms >= 0.0)
        std::cout << "  " << static_cast<int>(ai_ms) << "ms";
    std::cout << "\n";
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main() {
    sf::RenderWindow window(
        sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE),
        "Gomoku",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    Game   game;
    AI     ai;
    double last_ai_ms = -1.0;
    int    move_number = 0;

    std::cout << "=== Game Start ===\n";

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {

            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape)
                window.close();

            // 'R' restarts the game from any state (mid-game or after a win).
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::R) {
                game       = Game();
                last_ai_ms = -1.0;
                move_number = 0;
                std::cout << "=== Game Start ===\n";
            }

            // Left-click: only accepted on Black's turn — White is AI-controlled.
            // This prevents the human from accidentally placing White's stone
            // in the brief window between Black placing and the AI responding.
            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left &&
                game.currentPlayer() == Player::Black) {

                int row, col;
                bool on_board = screenToBoard(
                    event.mouseButton.x,
                    event.mouseButton.y,
                    row, col
                );

                if (on_board) {
                    int b_before = game.captureCount(Player::Black);
                    bool placed  = game.placeStone(row, col);
                    if (placed) {
                        int caps = game.captureCount(Player::Black) - b_before;
                        logMove(++move_number, "Black", row, col, caps, -1.0);
                        if (game.state() == GameState::BlackWins)
                            std::cout << "=== Black wins! ===\n";
                    }
                }
            }
        }

        // Update title and render BEFORE the AI thinks.
        // This way the human's stone appears on screen immediately after the click;
        // the window then freezes briefly during AI computation, and the AI stone
        // appears in the next frame. Without this ordering both stones would appear
        // together after the full AI delay, making the input feel broken.
        std::string title = "Gomoku — ";
        switch (game.state()) {
            case GameState::BlackWins: title += "Black wins!  (R to restart)"; break;
            case GameState::WhiteWins: title += "White wins!  (R to restart)"; break;
            case GameState::Ongoing:
                title += (game.currentPlayer() == Player::Black)
                             ? "Black's turn" : "White's turn (thinking...)";
                break;
        }
        title += "   [B captures: " + std::to_string(game.captureCount(Player::Black))
               + "  W captures: "  + std::to_string(game.captureCount(Player::White)) + "]";

        // Show AI think time once the AI has played at least once.
        if (last_ai_ms >= 0.0)
            title += "   AI: " + std::to_string(static_cast<int>(last_ai_ms)) + "ms";

        window.setTitle(title);

        window.clear();
        drawBoard(window);
        drawStones(window, game.board());
        window.display();

        // AI computes AFTER the frame is displayed so the human's stone is visible
        // before any freeze. The resulting AI stone will appear in the next frame.
        if (game.state() == GameState::Ongoing &&
            game.currentPlayer() == Player::White) {
            Move m = ai.bestMove(game, last_ai_ms);
            if (m.row >= 0) {
                int w_before  = game.captureCount(Player::White);
                MoveRecord rec = game.applyMove(m.row, m.col);
                int caps = game.captureCount(Player::White) - w_before;
                logMove(++move_number, "White (AI)", m.row, m.col, caps, last_ai_ms);
                if (game.state() == GameState::WhiteWins)
                    std::cout << "=== White wins! ===\n";
                (void)rec; // record not needed here; applyMove already applied
            }
        }
    }

    return 0;
}
