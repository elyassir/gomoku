#include <SFML/Graphics.hpp>
#include <cmath>   // std::round
#include <string>  // window title construction
#include "Game.hpp"

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

// ── Entry point ───────────────────────────────────────────────────────────────
int main() {
    sf::RenderWindow window(
        sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE),
        "Gomoku",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    Game game; // owns the board, the turn, and the game state

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {

            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape)
                window.close();

            // Left-click: attempt to place a stone at the nearest intersection.
            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left) {

                int row, col;
                bool on_board = screenToBoard(
                    event.mouseButton.x,
                    event.mouseButton.y,
                    row, col
                );

                if (on_board) {
                    // placeStone enforces: in-bounds, cell empty, game ongoing.
                    // It returns false silently — no crash, no feedback yet.
                    // Phase 10 will add a visual rejection indicator.
                    game.placeStone(row, col);
                }
            }
        }

        // Update window title each frame to reflect whose turn it is.
        // Cheap operation — a board game runs at 60 fps so this never costs anything.
        std::string title = "Gomoku — ";
        title += (game.currentPlayer() == Player::Black) ? "Black's turn" : "White's turn";
        window.setTitle(title);

        window.clear();
        drawBoard(window);
        drawStones(window, game.board());
        window.display();
    }

    return 0;
}
