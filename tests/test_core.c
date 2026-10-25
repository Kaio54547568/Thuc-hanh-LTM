#include "chess.h"
#include "protocol.h"
#include "storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(expr) do { checks++; if(!(expr)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);exit(1);} } while(0)
static int move(Chess *g,const char *s){char why[100];return chess_move(g,s,why,sizeof(why));}
static void empty(Chess *g){memset(g->board,'.',64);g->turn=0;g->castling=0;g->ep=-1;g->halfmove=0;g->fullmove=1;}
static void put(Chess *g,const char *square,char piece){g->board[chess_square(square)]=piece;}
static unsigned long perft(const Chess *g,int depth){
    if(!depth)return 1;
    unsigned long count=0;
    for(int a=0;a<64;a++)for(int b=0;b<64;b++){
        char uci[6]={(char)('a'+a%8),(char)('8'-a/8),(char)('a'+b%8),(char)('8'-b/8),0,0};
        int promotion=(g->board[a]=='P'||g->board[a]=='p')&&(b/8==0||b/8==7);
        for(int p=0;p<(promotion?4:1);p++){Chess next=*g;uci[4]=promotion?"qrbn"[p]:0;if(move(&next,uci))count+=perft(&next,depth-1);}
    }return count;
}
static void rules(void){
    Chess g;chess_init(&g);char fen[128];chess_fen(&g,fen);
    CHECK(!strcmp(fen,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    CHECK(perft(&g,1)==20);CHECK(perft(&g,2)==400);CHECK(perft(&g,3)==8902);
    Chess original=g;CHECK(!move(&g,"e2e5"));CHECK(!memcmp(&g,&original,sizeof(g)));
    CHECK(!move(&g,"a7a6"));CHECK(!move(&g,"a1a8"));CHECK(!move(&g,"e2e4q"));CHECK(!move(&g,"xx"));
    CHECK(move(&g,"e2e4"));CHECK(!move(&g,"d2d4"));CHECK(move(&g,"a7a6"));CHECK(move(&g,"e4e5"));CHECK(move(&g,"d7d5"));
    CHECK(move(&g,"e5d6"));CHECK(g.board[chess_square("d5")]=='.');CHECK(g.board[chess_square("d6")]=='P');
    chess_init(&g);CHECK(move(&g,"f2f3"));CHECK(move(&g,"e7e5"));CHECK(move(&g,"g2g4"));CHECK(move(&g,"d8h4"));
    CHECK(chess_in_check(&g,0));CHECK(!chess_has_moves(&g));
    empty(&g);put(&g,"e1",'K');put(&g,"h1",'R');put(&g,"e8",'k');g.castling=1;
    CHECK(move(&g,"e1g1"));CHECK(g.board[chess_square("f1")]=='R');CHECK(g.board[chess_square("h1")]=='.');CHECK(g.castling==0);
    empty(&g);put(&g,"e1",'K');put(&g,"h1",'R');put(&g,"e8",'k');put(&g,"f8",'r');g.castling=1;CHECK(!move(&g,"e1g1"));
    empty(&g);put(&g,"e1",'K');put(&g,"e2",'R');put(&g,"a8",'k');put(&g,"e8",'r');CHECK(!move(&g,"e2f2"));
    empty(&g);put(&g,"a1",'K');put(&g,"h8",'k');put(&g,"e7",'P');CHECK(!move(&g,"e7e8"));CHECK(move(&g,"e7e8n"));CHECK(g.board[4]=='N');
    /* En passant exposing a rook attack on own king is illegal. */
    empty(&g);put(&g,"e5",'K');put(&g,"f5",'P');put(&g,"g5",'p');put(&g,"h5",'r');put(&g,"a8",'k');g.ep=chess_square("g6");CHECK(!move(&g,"f5g6"));
    empty(&g);put(&g,"c6",'K');put(&g,"b6",'Q');put(&g,"a8",'k');g.turn=1;CHECK(!chess_in_check(&g,1));CHECK(!chess_has_moves(&g));
    /* Kings cannot touch or be captured. */
    empty(&g);put(&g,"e1",'K');put(&g,"e3",'k');CHECK(!move(&g,"e1e2"));
    empty(&g);put(&g,"a1",'K');put(&g,"e1",'R');put(&g,"e8",'k');CHECK(!move(&g,"e1e8"));
}
static void framing(void){
    Stream s={0},wire={0};cJSON *j=message("PING"),*parsed=NULL;cJSON_AddStringToObject(j,"request_id","1");
    CHECK(stream_queue(&wire,j)==0);cJSON_Delete(j);
    for(size_t i=0;i<wire.size;i++){CHECK(stream_feed(&s,wire.data+i,1)==0);int rc=stream_next(&s,&parsed);CHECK(rc==(i==wire.size-1?1:0));}
    CHECK(!strcmp(json_string(parsed,"type"),"PING"));cJSON_Delete(parsed);CHECK(s.size==0);
    CHECK(stream_feed(&s,wire.data,wire.size)==0);CHECK(stream_feed(&s,wire.data,wire.size)==0);
    CHECK(stream_next(&s,&parsed)==1);cJSON_Delete(parsed);CHECK(stream_next(&s,&parsed)==1);cJSON_Delete(parsed);CHECK(s.size==0);
    uint32_t bad=htonl(FRAME_MAX+1);stream_feed(&s,&bad,4);CHECK(stream_next(&s,&parsed)==-1);
    s.size=0;bad=htonl(2);stream_feed(&s,&bad,4);stream_feed(&s,"[]",2);CHECK(stream_next(&s,&parsed)==-1);
    s.size=0;bad=htonl(4);stream_feed(&s,&bad,4);stream_feed(&s,"{}\0x",4);CHECK(stream_next(&s,&parsed)==-1);
    s.size=0;CHECK(stream_feed(&s,"",STREAM_CAP+1)==-1);
}
static void storage(void){
    Storage s;CHECK(storage_open(&s,":memory:"));int id=storage_start(&s,"Loc","Viet","start");CHECK(id>0);
    CHECK(storage_move(&s,id,1,"e2e4","after_e4",NULL,NULL));
    CHECK(!storage_move(&s,id,1,"d2d4","bad",NULL,NULL));
    sqlite3_stmt *q=NULL;CHECK(sqlite3_prepare_v2(s.db,"SELECT current_fen FROM games WHERE id=1",-1,&q,NULL)==SQLITE_OK);
    CHECK(sqlite3_step(q)==SQLITE_ROW);CHECK(!strcmp((const char *)sqlite3_column_text(q,0),"after_e4"));sqlite3_finalize(q);
    CHECK(storage_move(&s,id,2,"e7e5","after_e5","1-0","checkmate"));CHECK(!storage_finish(&s,id,"0-1","resign"));
    CHECK(!storage_move(&s,id,3,"g1f3","invalid",NULL,NULL));
    CHECK(sqlite3_prepare_v2(s.db,"SELECT count(*) FROM moves",-1,&q,NULL)==SQLITE_OK);CHECK(sqlite3_step(q)==SQLITE_ROW);CHECK(sqlite3_column_int(q,0)==2);sqlite3_finalize(q);storage_close(&s);
}
int main(void){rules();framing();storage();printf("PASS %d checks (rules + initial perft depth 3 + framing + SQLite rollback)\n",checks);return 0;}
