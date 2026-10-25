#ifndef CHESS_STORAGE_H
#define CHESS_STORAGE_H
#include <sqlite3.h>
typedef struct { sqlite3 *db; } Storage;
int storage_open(Storage *s, const char *path);
void storage_close(Storage *s);
int storage_start(Storage *s, const char *white, const char *black, const char *fen);
/* Atomically append a move and update the game, including optional result. */
int storage_move(Storage *s,int game,int ply,const char *uci,const char *fen,const char *result,const char *reason);
int storage_finish(Storage *s,int game,const char *result,const char *reason);
#endif
