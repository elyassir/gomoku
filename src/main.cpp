#include <SFML/Graphics.hpp>
#include "Board.hpp"

// ── Layout constants ──────────────────────────────────────────────────────────
// The board is a square window. MARGIN is the gap between the window edge and
// the first grid line, so stones on the border aren't clipped.
static constexpr int   WINDOW_SIZE = 700;
static constexpr int   MARGIN      = 40;

// CELL_SIZE is the pixel distance between adjacent intersections.
// We divide by (BOARD_SIZE - 1) because 19 lines create 18 gaps, not 19.
static constexpr float CELL_SIZE = (WINDOW_SIZE - 2.f * MARGIN) / (BOARD_SIZE - 1);

// ── Coordinate helper ─────────────────────────────────────────────────────────
// Converts a board position (row, col) to the pixel centre of that intersection.
// Every rendering and input function goes through this so the math lives in one place.
static sf::Vector2f boardToScreen(int row, int col) {
    return {
        MARGIN + col * CELL_SIZE,   // X grows left→right  (col)
        MARGIN + row * CELL_SIZE    // Y grows top→bottom  (row)
    };
}

// ── Board renderer ────────────────────────────────────────────────────────────
static void drawBoard(sf::RenderWindow& window) {
    // 1. Background — a warm wood tone that matches a traditional goban.
    sf::RectangleShape background({ (float)WINDOW_SIZE, (float)WINDOW_SIZE });
    background.setFillColor(sf::Color(220, 179, 92));
    window.draw(background);

    // 2. Grid lines — one horizontal and one vertical per board row/column.
    //    We draw them as thin 1-pixel rectangles rather than SFML lines
    //    because sf::RectangleShape renders crisply at all window sizes.
    const sf::Color line_color(80, 50, 20);
    const float     grid_span = WINDOW_SIZE - 2.f * MARGIN; // total pixel width of the grid

    for (int i = 0; i < BOARD_SIZE; ++i) {
        // Horizontal line: full width, at the i-th row's Y position.
        sf::RectangleShape horizontal_line({ grid_span, 1.f });
        horizontal_line.setPosition(MARGIN, MARGIN + i * CELL_SIZE);
        horizontal_line.setFillColor(line_color);
        window.draw(horizontal_line);

        // Vertical line: full height, at the i-th column's X position.
        sf::RectangleShape vertical_line({ 1.f, grid_span });
        vertical_line.setPosition(MARGIN + i * CELL_SIZE, MARGIN);
        vertical_line.setFillColor(line_color);
        window.draw(vertical_line);
    }

    // 3. Star points (hoshi) — the 9 reference dots on a 19x19 board.
    //    Their positions are fixed by convention at indices 3, 9 and 15.
    //    They help players orient themselves without counting lines.
    const int hoshi_indices[] = { 3, 9, 15 };
    for (int row : hoshi_indices) {
        for (int col : hoshi_indices) {
            sf::CircleShape dot(3.f);
            dot.setFillColor(line_color);
            dot.setOrigin(3.f, 3.f); // centre the dot on the intersection pixel
            dot.setPosition(boardToScreen(row, col));
            window.draw(dot);
        }
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main() {
    // Titlebar | Close gives a fixed-size window (no resize handle).
    // A resizable window would need us to recompute CELL_SIZE at runtime —
    // unnecessary complexity for now.
    sf::RenderWindow window(
        sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE),
        "Gomoku",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60); // cap CPU use; 60 fps is more than enough for a board game

    Board board; // the game state — empty at start

    // ── Main loop ─────────────────────────────────────────────────────────────
    // Standard SFML pattern: poll all pending events, then render one frame.
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            // Escape is a convenient dev shortcut; does not need to survive to shipping.
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape)
                window.close();
        }

        window.clear();
        drawBoard(window);
        window.display();
    }

    return 0;
}
