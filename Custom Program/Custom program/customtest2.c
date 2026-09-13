#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------ CONFIG ---------------------------------- */
#define TILE        64
#define BOARD_W     8
#define WIN_SIZE    (TILE * BOARD_W)
#define ASSET_DIR   "assets/"

/* --------------------------- CHESS ENGINE -------------------------------- */
#define EMPTY '.'
#define IS_WHITE(p) ((p) >= 'A' && (p) <= 'Z')
#define IS_BLACK(p) ((p) >= 'a' && (p) <= 'z')
#define OPP_COLOR(c) ((c) == 'w' ? 'b' : 'w')

typedef char Piece;

typedef struct {
    Piece board[BOARD_W][BOARD_W];
    char  turn;
    int wk_moved, wra_moved, wrh_moved;
    int bk_moved, bra_moved, brh_moved;
    int en_passant_r, en_passant_c;
    int fullmove_number;
} GameState;

/* ------------------------ TEXTURE & ASSETS ------------------------------ */

Texture2D piece_textures[12];
const char *piece_files[12] = {
    "w_pawn.png", "w_knight.png", "w_bishop.png", "w_rook.png",
    "w_queen.png", "w_king.png", "b_pawn.png", "b_knight.png",
    "b_bishop.png", "b_rook.png", "b_queen.png", "b_king.png"
};

int piece_to_texture_index(char p) {
    switch (p) {
        case 'P': return 0;
        case 'N': return 1;
        case 'B': return 2;
        case 'R': return 3;
        case 'Q': return 4;
        case 'K': return 5;
        case 'p': return 6;
        case 'n': return 7;
        case 'b': return 8;
        case 'r': return 9;
        case 'q': return 10;
        case 'k': return 11;
    }
    return -1;
}

void load_piece_textures() {
    for (int i = 0; i < 12; ++i) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", ASSET_DIR, piece_files[i]);
        piece_textures[i] = LoadTexture(filepath);
    }
}

void unload_piece_textures() {
    for (int i = 0; i < 12; ++i) {
        UnloadTexture(piece_textures[i]);
    }
}

/* --------------------------- BOARD & MOVES ------------------------------- */

// Check if position is inside board boundaries
static int in_bounds(int r, int c) {
    return r >= 0 && r < BOARD_W && c >= 0 && c < BOARD_W;
}

// Initialize chess starting position and state
static void init_board(GameState *g) {
    static const char *start[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQKBNR"
    };
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            g->board[r][c] = start[r][c];

    g->turn = 'w';
    g->wk_moved = g->wra_moved = g->wrh_moved = 0;
    g->bk_moved = g->bra_moved = g->brh_moved = 0;
    g->en_passant_r = g->en_passant_c = -1;
    g->fullmove_number = 1;
}

// Check if a move is legal (basic rules only, no check check here)
static int is_legal_move(GameState *g, int sr, int sc, int dr, int dc, char promo) {
    Piece p = g->board[sr][sc];
    int dir = IS_WHITE(p) ? -1 : 1;
    int delta_r = dr - sr, delta_c = dc - sc;
    Piece dst = g->board[dr][dc];

    if (IS_WHITE(p) && g->turn != 'w') return 0;
    if (IS_BLACK(p) && g->turn != 'b') return 0;
    if (!in_bounds(dr, dc)) return 0;
    if ((IS_WHITE(p) && IS_WHITE(dst)) || (IS_BLACK(p) && IS_BLACK(dst))) return 0;

    switch (tolower(p)) {
        case 'p': // Pawn moves
            if (delta_c == 0 && dst == EMPTY) {
                if (delta_r == dir) return 1;
                if ((sr == 1 && dir == 1 || sr == 6 && dir == -1) && delta_r == 2 * dir && g->board[sr + dir][sc] == EMPTY)
                    return 1;
            } else if (abs(delta_c) == 1 && delta_r == dir) {
                if ((IS_WHITE(p) && IS_BLACK(dst)) || (IS_BLACK(p) && IS_WHITE(dst))) return 1;
                if (dr == g->en_passant_r && dc == g->en_passant_c) return 1;
            }
            return 0;

        case 'r': // Rook moves (straight line)
            if (delta_r != 0 && delta_c != 0) return 0;
            for (int i = 1; i < abs(delta_r + delta_c); ++i) {
                int r = sr + (delta_r ? i * (delta_r / abs(delta_r)) : 0);
                int c = sc + (delta_c ? i * (delta_c / abs(delta_c)) : 0);
                if (g->board[r][c] != EMPTY) return 0;
            }
            return 1;

        case 'n': // Knight moves
            return (abs(delta_r) == 2 && abs(delta_c) == 1) || (abs(delta_r) == 1 && abs(delta_c) == 2);

        case 'b': // Bishop moves (diagonal)
            if (abs(delta_r) != abs(delta_c)) return 0;
            for (int i = 1; i < abs(delta_r); ++i) {
                int r = sr + i * (delta_r / abs(delta_r));
                int c = sc + i * (delta_c / abs(delta_c));
                if (g->board[r][c] != EMPTY) return 0;
            }
            return 1;

        case 'q': // Queen moves (rook + bishop)
            if (delta_r == 0 || delta_c == 0) {
                int step_r = (delta_r == 0) ? 0 : delta_r / abs(delta_r);
                int step_c = (delta_c == 0) ? 0 : delta_c / abs(delta_c);
                for (int i = 1; i < abs(delta_r + delta_c); ++i) {
                    int r = sr + step_r * i;
                    int c = sc + step_c * i;
                    if (g->board[r][c] != EMPTY) return 0;
                }
                return 1;
            } else if (abs(delta_r) == abs(delta_c)) {
                int step_r = delta_r / abs(delta_r);
                int step_c = delta_c / abs(delta_c);
                for (int i = 1; i < abs(delta_r); ++i) {
                    int r = sr + step_r * i;
                    int c = sc + step_c * i;
                    if (g->board[r][c] != EMPTY) return 0;
                }
                return 1;
            }
            return 0;

        case 'k': // King moves
            if (abs(delta_r) <= 1 && abs(delta_c) <= 1) return 1;

            // Castling conditions simplified
            if (IS_WHITE(p) && sr == 7 && sc == 4 && dr == 7 && (dc == 2 || dc == 6)) {
                if (dc == 6 && !g->wk_moved && !g->wrh_moved &&
                    g->board[7][5] == EMPTY && g->board[7][6] == EMPTY) return 1;
                if (dc == 2 && !g->wk_moved && !g->wra_moved &&
                    g->board[7][3] == EMPTY && g->board[7][2] == EMPTY && g->board[7][1] == EMPTY) return 1;
            }
            if (IS_BLACK(p) && sr == 0 && sc == 4 && dr == 0 && (dc == 2 || dc == 6)) {
                if (dc == 6 && !g->bk_moved && !g->brh_moved &&
                    g->board[0][5] == EMPTY && g->board[0][6] == EMPTY) return 1;
                if (dc == 2 && !g->bk_moved && !g->bra_moved &&
                    g->board[0][3] == EMPTY && g->board[0][2] == EMPTY && g->board[0][1] == EMPTY) return 1;
            }
            return 0;
    }
    return 0;
}

// Updates board to perform the move and updates flags
void make_move(GameState *g, int sr, int sc, int dr, int dc, char promo) {
    Piece p = g->board[sr][sc];

    // Handle castling move: move rook too
    if (tolower(p) == 'k' && abs(dc - sc) == 2) {
        if (dc == 6) {
            // Kingside castling
            g->board[dr][5] = g->board[dr][7];
            g->board[dr][7] = EMPTY;
        } else if (dc == 2) {
            // Queenside castling
            g->board[dr][3] = g->board[dr][0];
            g->board[dr][0] = EMPTY;
        }
    }

    // En passant capture
    if (tolower(p) == 'p' && dc != sc && g->board[dr][dc] == EMPTY) {
        if (IS_WHITE(p))
            g->board[dr + 1][dc] = EMPTY;
        else
            g->board[dr - 1][dc] = EMPTY;
    }

    // Move piece
    g->board[dr][dc] = p;
    g->board[sr][sc] = EMPTY;

    // Promotion
    if (promo) g->board[dr][dc] = promo;

    // Update castling rights
    if (p == 'K') g->wk_moved = 1;
    if (p == 'R') {
        if (sr == 7 && sc == 0) g->wra_moved = 1;
        if (sr == 7 && sc == 7) g->wrh_moved = 1;
    }
    if (p == 'k') g->bk_moved = 1;
    if (p == 'r') {
        if (sr == 0 && sc == 0) g->bra_moved = 1;
        if (sr == 0 && sc == 7) g->brh_moved = 1;
    }

    // Update en passant square
    if (tolower(p) == 'p' && abs(dr - sr) == 2) {
        g->en_passant_r = (dr + sr) / 2;
        g->en_passant_c = sc;
    } else {
        g->en_passant_r = -1;
        g->en_passant_c = -1;
    }

    // Update turn
    g->turn = OPP_COLOR(g->turn);

    // Fullmove number increment on Black's turn
    if (g->turn == 'w') g->fullmove_number++;
}

// Check if given position (r,c) is under attack by opponent pieces
int is_square_attacked(GameState *g, int r, int c, char by_color) {
    // Scan board for opponent pieces, check if any legal move attacks (r,c)
    for (int rr = 0; rr < 8; rr++) {
        for (int cc = 0; cc < 8; cc++) {
            Piece p = g->board[rr][cc];
            if (p == EMPTY) continue;
            if ((by_color == 'w' && !IS_WHITE(p)) || (by_color == 'b' && !IS_BLACK(p))) continue;

            // If this piece can move to (r,c), it's an attack
            if (is_legal_move(g, rr, cc, r, c, 0)) return 1;
        }
    }
    return 0;
}

// Find king position for given color
void find_king(GameState *g, char color, int *kr, int *kc) {
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            if (color == 'w' && g->board[r][c] == 'K') {
                *kr = r; *kc = c;
                return;
            }
            if (color == 'b' && g->board[r][c] == 'k') {
                *kr = r; *kc = c;
                return;
            }
        }
}

// Check if current player is in check
int in_check(GameState *g, char color) {
    int kr, kc;
    find_king(g, color, &kr, &kc);
    return is_square_attacked(g, kr, kc, OPP_COLOR(color));
}

/* --------------------------- DRAWING ------------------------------------- */

// Draw chessboard squares with alternating colors
void draw_board() {
    Color light = (Color){240, 217, 181, 255};
    Color dark  = (Color){181, 136, 99, 255};

    for (int r = 0; r < BOARD_W; ++r) {
        for (int c = 0; c < BOARD_W; ++c) {
            Color col = ((r + c) % 2 == 0) ? light : dark;
            DrawRectangle(c * TILE, r * TILE, TILE, TILE, col);
        }
    }
}

// Draw pieces with texture images
void draw_pieces(GameState *g, int selected_r, int selected_c) {
    for (int r = 0; r < BOARD_W; ++r) {
        for (int c = 0; c < BOARD_W; ++c) {
            Piece p = g->board[r][c];
            if (p == EMPTY) continue;

            int tex_idx = piece_to_texture_index(p);
            if (tex_idx == -1) continue;

            // Highlight selected square
            if (r == selected_r && c == selected_c) {
                DrawRectangle(c * TILE, r * TILE, TILE, TILE, (Color){255, 255, 0, 120});
            }

            DrawTexturePro(piece_textures[tex_idx],
                           (Rectangle){0, 0, piece_textures[tex_idx].width, piece_textures[tex_idx].height},
                           (Rectangle){c * TILE, r * TILE, TILE, TILE},
                           (Vector2){0, 0}, 0, WHITE);
        }
    }
}

/* --------------------------- MAIN ---------------------------------------- */

int main() {
    InitWindow(WIN_SIZE, WIN_SIZE, "Raylib Chess");
    SetTargetFPS(60);

    GameState game;
    init_board(&game);

    load_piece_textures();

    int selected_r = -1, selected_c = -1;
    int dragging = 0;
    int mouse_r = -1, mouse_c = -1;

    while (!WindowShouldClose()) {
        // Mouse coordinates in board coords
        int mx = GetMouseX() / TILE;
        int my = GetMouseY() / TILE;

        // Handle mouse clicks for selecting and moving pieces
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (selected_r == -1) {
                // Select a piece of the correct turn
                if (in_bounds(my, mx)) {
                    Piece p = game.board[my][mx];
                    if (p != EMPTY && ((game.turn == 'w' && IS_WHITE(p)) || (game.turn == 'b' && IS_BLACK(p)))) {
                        selected_r = my;
                        selected_c = mx;
                        dragging = 1;
                    }
                }
            } else {
                // Try to move selected piece to clicked square
                if (in_bounds(my, mx)) {
                    if (is_legal_move(&game, selected_r, selected_c, my, mx, 0)) {
                        // Make move if it doesn't leave player in check
                        GameState temp = game;
                        make_move(&temp, selected_r, selected_c, my, mx, 0);
                        if (!in_check(&temp, game.turn)) {
                            make_move(&game, selected_r, selected_c, my, mx, 0);
                        }
                    }
                }
                selected_r = -1;
                selected_c = -1;
                dragging = 0;
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw_board();
        draw_pieces(&game, selected_r, selected_c);

        if (dragging && selected_r != -1 && selected_c != -1) {
            // Draw dragged piece under mouse
            Piece p = game.board[selected_r][selected_c];
            int tex_idx = piece_to_texture_index(p);
            if (tex_idx != -1) {
                Vector2 pos = {(float)GetMouseX() - TILE / 2, (float)GetMouseY() - TILE / 2};
                DrawTexturePro(piece_textures[tex_idx],
                               (Rectangle){0, 0, piece_textures[tex_idx].width, piece_textures[tex_idx].height},
                               (Rectangle){pos.x, pos.y, TILE, TILE},
                               (Vector2){0, 0}, 0, WHITE);
            }
        }

        // Show whose turn it is
        DrawText(game.turn == 'w' ? "White to move" : "Black to move", 5, WIN_SIZE - 20, 20, BLACK);

        // Show check status
        if (in_check(&game, game.turn)) {
            DrawText("Check!", WIN_SIZE / 2 - 30, WIN_SIZE - 20, 20, RED);
        }

        EndDrawing();
    }

    unload_piece_textures();
    CloseWindow();
    return 0;
}
