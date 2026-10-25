#ifndef CHESS_RULES_H
#define CHESS_RULES_H
/* board[0] = a8; uppercase = white, lowercase = black, '.' = empty. */
typedef struct {
    char board[64]; int turn; /* 0 white, 1 black */
    unsigned castling; /* WK=1 WQ=2 BK=4 BQ=8 */
    int ep, halfmove, fullmove;
} Chess;
void chess_init(Chess *g);
int chess_square(const char *s);
int chess_in_check(const Chess *g, int side);
/* Full legal move incl. king safety, castling, en passant, promotion. */
int chess_move(Chess *g, const char *uci, char *error, unsigned error_size);
int chess_has_moves(const Chess *g);
void chess_fen(const Chess *g, char out[128]);
#endif
