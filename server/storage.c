#include "storage.h"
#include <stdio.h>
static int exec(Storage *s,const char *sql) {
    char *error=NULL;int rc=sqlite3_exec(s->db,sql,NULL,NULL,&error);
    if(rc!=SQLITE_OK) {fprintf(stderr,"SQLite: %s\n",error?error:"error");sqlite3_free(error);return 0;}return 1;
}
int storage_open(Storage *s,const char *path) {
    s->db=NULL;if(sqlite3_open(path,&s->db)!=SQLITE_OK){storage_close(s);return 0;}
    sqlite3_busy_timeout(s->db,1000);
    /* One server writer: rollback journal also works on older WSL filesystems. */
    if(!exec(s,"PRAGMA foreign_keys=ON; PRAGMA journal_mode=DELETE;"
      "CREATE TABLE IF NOT EXISTS games(id INTEGER PRIMARY KEY,white TEXT NOT NULL,black TEXT NOT NULL,"
      "initial_fen TEXT NOT NULL,current_fen TEXT NOT NULL,result TEXT NOT NULL DEFAULT '*',reason TEXT NOT NULL DEFAULT '',"
      "started_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,ended_at TEXT);"
      "CREATE TABLE IF NOT EXISTS moves(game_id INTEGER NOT NULL REFERENCES games(id),ply INTEGER NOT NULL,"
      "uci TEXT NOT NULL,fen TEXT NOT NULL,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,PRIMARY KEY(game_id,ply));"
      "UPDATE games SET result='aborted',reason='server_restart',ended_at=CURRENT_TIMESTAMP WHERE result='*';")) {
        storage_close(s);return 0;
    }return 1;
}
void storage_close(Storage *s){if(s->db)sqlite3_close(s->db);s->db=NULL;}
int storage_start(Storage *s,const char *w,const char *b,const char *fen){
    sqlite3_stmt *q=NULL;int ok=0;
    if(sqlite3_prepare_v2(s->db,"INSERT INTO games(white,black,initial_fen,current_fen) VALUES(?,?,?,?)",-1,&q,NULL)==SQLITE_OK){
        sqlite3_bind_text(q,1,w,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,2,b,-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(q,3,fen,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,4,fen,-1,SQLITE_TRANSIENT);
        if(sqlite3_step(q)==SQLITE_DONE)ok=(int)sqlite3_last_insert_rowid(s->db);
    }sqlite3_finalize(q);return ok;
}
int storage_finish(Storage *s,int game,const char *result,const char *reason){
    sqlite3_stmt *q=NULL;int ok=0;
    if(sqlite3_prepare_v2(s->db,"UPDATE games SET result=?,reason=?,ended_at=CURRENT_TIMESTAMP WHERE id=? AND result='*'",-1,&q,NULL)==SQLITE_OK){
        sqlite3_bind_text(q,1,result,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,2,reason,-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,3,game);
        ok=sqlite3_step(q)==SQLITE_DONE && sqlite3_changes(s->db)==1;
    }sqlite3_finalize(q);return ok;
}
int storage_move(Storage *s,int game,int ply,const char *uci,const char *fen,const char *result,const char *reason){
    if(!exec(s,"BEGIN IMMEDIATE"))return 0;
    sqlite3_stmt *q=NULL;int ok=0;
    if(sqlite3_prepare_v2(s->db,"INSERT INTO moves(game_id,ply,uci,fen) VALUES(?,?,?,?)",-1,&q,NULL)==SQLITE_OK){
        sqlite3_bind_int(q,1,game);sqlite3_bind_int(q,2,ply);sqlite3_bind_text(q,3,uci,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,4,fen,-1,SQLITE_TRANSIENT);
        ok=sqlite3_step(q)==SQLITE_DONE;
    }sqlite3_finalize(q);q=NULL;
    if(ok && sqlite3_prepare_v2(s->db,"UPDATE games SET current_fen=? WHERE id=? AND result='*'",-1,&q,NULL)==SQLITE_OK){
        sqlite3_bind_text(q,1,fen,-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,2,game);ok=sqlite3_step(q)==SQLITE_DONE && sqlite3_changes(s->db)==1;
    }else ok=0;
    sqlite3_finalize(q);
    if(ok && result)ok=storage_finish(s,game,result,reason);
    if(ok && exec(s,"COMMIT"))return 1;
    exec(s,"ROLLBACK");return 0;
}
