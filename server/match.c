#include "app.h"
#include <stdio.h>
#include <string.h>

void state(int r,const char *last,const char *request_id){
    Room *room=&rooms[r];Chess *g=&room->chess;char board[65],fen[128];memcpy(board,g->board,64);board[64]=0;chess_fen(g,fen);
    cJSON *j=message("STATE");cJSON_AddNumberToObject(j,"room_id",room->game);cJSON_AddNumberToObject(j,"ply",room->ply);
    cJSON_AddStringToObject(j,"white",clients[room->players[0]].name);cJSON_AddStringToObject(j,"black",clients[room->players[1]].name);
    cJSON_AddStringToObject(j,"board",board);cJSON_AddStringToObject(j,"fen",fen);cJSON_AddStringToObject(j,"turn",g->turn?"black":"white");
    cJSON_AddBoolToObject(j,"check",chess_in_check(g,g->turn));cJSON_AddStringToObject(j,"last_move",last);
    if(request_id && *request_id)cJSON_AddStringToObject(j,"request_id",request_id);
    for(int side=0;side<2;side++)emit(room->players[side],j);
    cJSON_Delete(j);
}
void end_room(int r,const char *result,const char *reason){
    Room *room=&rooms[r];cJSON *j=message("GAME_OVER");cJSON_AddNumberToObject(j,"room_id",room->game);
    cJSON_AddStringToObject(j,"result",result);cJSON_AddStringToObject(j,"reason",reason);
    for(int side=0;side<2;side++){int i=room->players[side];emit(i,j);clients[i].room=-1;}
    cJSON_Delete(j);room->active=0;lobby();
}
void handle_game(int i,cJSON *req){
    Client *c=&clients[i];
    const char *type=json_string(req,"type"),*id=json_string(req,"request_id");
    {
        int r=c->room;if(r<0){error(i,req,"NOT_IN_GAME");return;}
        Room *room=&rooms[r];const cJSON *rid=cJSON_GetObjectItemCaseSensitive(req,"room_id");
        if(!cJSON_IsNumber(rid) || rid->valuedouble!=room->game){error(i,req,"BAD_ROOM");return;}
        if(!strcmp(type,"RESIGN")){
            const char *res=room->players[0]==i?"0-1":"1-0";
            if(!storage_finish(&db,room->game,res,"resign")){error(i,req,"STORAGE_ERROR");return;}
            reply(i,"OK",req,NULL);end_room(r,res,"resign");return;
        }
        if(room->players[room->chess.turn]!=i){error(i,req,"NOT_YOUR_TURN");return;}
        const char *uci=json_string(req,"uci");Chess next=room->chess;char why[100];
        if(!chess_move(&next,uci,why,sizeof(why))){
            cJSON *j=message("ERROR");cJSON_AddStringToObject(j,"code","ILLEGAL_MOVE");cJSON_AddStringToObject(j,"detail",why);cJSON_AddStringToObject(j,"request_id",id);emit(i,j);cJSON_Delete(j);return;
        }
        const char *res=NULL,*reason=NULL;
        if(!chess_has_moves(&next)){int check=chess_in_check(&next,next.turn);res=check?(next.turn?"1-0":"0-1"):"1/2-1/2";reason=check?"checkmate":"stalemate";}
        char fen[128];chess_fen(&next,fen);
        if(!storage_move(&db,room->game,room->ply+1,uci,fen,res,reason)){error(i,req,"STORAGE_ERROR");return;}
        room->chess=next;room->ply++;state(r,uci,id);if(res)end_room(r,res,reason);return;
    }
}
