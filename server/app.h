#ifndef CHESS_APP_H
#define CHESS_APP_H
#include "protocol.h"
#include "chess.h"
#include "storage.h"
#include <time.h>
#define MAX_CLIENTS 32
#define MAX_ROOMS 16
/* Single-threaded server owns all mutable clients/rooms. */
typedef struct {
    socket_t fd; Stream in,out; char name[25]; int room,invite,closing;
    time_t connected,invited;
} Client;
typedef struct {int active,players[2],game,ply; Chess chess;} Room;
extern Client clients[MAX_CLIENTS];
extern Room rooms[MAX_ROOMS];
extern Storage db;
void emit(int i,const cJSON *j);
void reply(int i,const char *type,const cJSON *req,const char *code);
void error(int i,const cJSON *req,const char *code);
void lobby(void);
void state(int r,const char *last,const char *request_id);
/* Database result must already be committed before announcing game over. */
void end_room(int r,const char *result,const char *reason);
void handle_game(int i,cJSON *req);
#endif
