#include "chess.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int side(char p) { return p == '.' ? -1 : (islower((unsigned char)p) ? 1 : 0); }
void chess_init(Chess *g) {
    memcpy(g->board, "rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR", 64);
    g->turn=0; g->castling=15; g->ep=-1; g->halfmove=0; g->fullmove=1;
}
int chess_square(const char *s) {
    if (!s || s[0]<'a' || s[0]>'h' || s[1]<'1' || s[1]>'8') return -1;
    return ('8'-s[1])*8+s[0]-'a';
}
static int clear_path(const Chess *g, int a, int b) {
    int r=a/8,c=a%8, dr=(b/8>r)-(b/8<r), dc=(b%8>c)-(b%8<c);
    r+=dr; c+=dc;
    while (r*8+c!=b) { if (g->board[r*8+c]!='.') return 0; r+=dr; c+=dc; }
    return 1;
}
static int attacked(const Chess *g, int target, int by) {
    for (int a=0;a<64;a++) {
        char p=g->board[a]; if (side(p)!=by) continue;
        int dr=target/8-a/8,dc=target%8-a%8, ar=abs(dr),ac=abs(dc);
        switch (tolower((unsigned char)p)) {
        case 'p': if (dr==(by?1:-1) && ac==1) return 1; break;
        case 'n': if (ar*ac==2) return 1; break;
        case 'k': if (ar<=1 && ac<=1 && ar+ac) return 1; break;
        case 'b': if (ar && ar==ac && clear_path(g,a,target)) return 1; break;
        case 'r': if ((dr==0 || dc==0) && a!=target && clear_path(g,a,target)) return 1; break;
        case 'q': if (a!=target && (ar==ac || !dr || !dc) && clear_path(g,a,target)) return 1; break;
        }
    }
    return 0;
}
int chess_in_check(const Chess *g, int who) {
    char king=who?'k':'K';
    for(int i=0;i<64;i++) if(g->board[i]==king) return attacked(g,i,1-who);
    return 1;
}
static int fail(char *e, unsigned n, const char *s) { if(e && n) snprintf(e,n,"%s",s); return 0; }
static int apply(Chess *g, int a,int b,char promotion,char *e,unsigned n) {
    if(a<0||b<0||a==b) return fail(e,n,"Invalid coordinates");
    char p=g->board[a],captured=g->board[b],kind=(char)tolower((unsigned char)p);
    int who=g->turn,dr=b/8-a/8,dc=b%8-a%8,ar=abs(dr),ac=abs(dc),dir=who?1:-1;
    int capture_ep=0,castle=0,legal=0;
    if(side(p)!=who) return fail(e,n,"Not your piece or turn");
    if(side(captured)==who || tolower((unsigned char)captured)=='k') return fail(e,n,"Invalid destination");
    switch(kind) {
    case 'p':
        if(dc==0 && captured=='.' && (dr==dir || (dr==2*dir && a/8==(who?1:6) && g->board[a+8*dir]=='.'))) legal=1;
        if(ac==1 && dr==dir && side(captured)==1-who) legal=1;
        if(ac==1 && dr==dir && b==g->ep && captured=='.' && g->board[b-8*dir]==(who?'P':'p')) {legal=1;capture_ep=1;}
        if(b/8==(who?7:0)) { if(!strchr("qrbn",promotion) || !promotion) return fail(e,n,"Promotion requires q/r/b/n"); }
        else if(promotion) return fail(e,n,"Unexpected promotion");
        break;
    case 'n': legal=ar*ac==2; break;
    case 'b': legal=ar==ac && clear_path(g,a,b); break;
    case 'r': legal=(!dr || !dc) && clear_path(g,a,b); break;
    case 'q': legal=(ar==ac || !dr || !dc) && clear_path(g,a,b); break;
    case 'k':
        legal=ar<=1 && ac<=1;
        if(a==(who?4:60) && dr==0 && ac==2) {
            int right=who?(dc>0?4:8):(dc>0?1:2), rook=dc>0?a+3:a-4, step=dc>0?1:-1;
            if((g->castling & (unsigned)right) && g->board[rook]==(who?'r':'R') && clear_path(g,a,rook) && !chess_in_check(g,who)) {
                Chess transit=*g; transit.board[a]='.'; transit.board[a+step]=p;
                if(!attacked(&transit,a+step,1-who)) {legal=1;castle=1;}
            }
        }
        break;
    }
    if(kind!='p' && promotion) return fail(e,n,"Unexpected promotion");
    if(!legal) return fail(e,n,"Illegal piece movement");
    Chess next=*g; next.board[a]='.';next.board[b]=p;
    if(capture_ep) next.board[b-8*dir]='.';
    if(castle) {int rook=dc>0?a+3:a-4;next.board[rook]='.';next.board[a+(dc>0?1:-1)]=who?'r':'R';}
    if(kind=='p' && promotion) next.board[b]=who?promotion:(char)toupper((unsigned char)promotion);
    if(chess_in_check(&next,who)) return fail(e,n,"Move leaves king in check");
    if(kind=='k') next.castling &= who?~12u:~3u;
    const int squares[4]={63,56,7,0};
    for(int i=0;i<4;i++) if(a==squares[i]||b==squares[i]) next.castling &= ~(1u<<i);
    next.ep=kind=='p' && ar==2?(a+b)/2:-1;
    next.halfmove=(kind=='p'||captured!='.'||capture_ep)?0:g->halfmove+1;
    next.fullmove=g->fullmove+who;next.turn=1-who;*g=next;
    return 1;
}
int chess_move(Chess *g,const char *uci,char *e,unsigned n) {
    if(!uci || (strlen(uci)!=4 && strlen(uci)!=5)) return fail(e,n,"Use UCI, e.g. e2e4 or e7e8q");
    return apply(g,chess_square(uci),chess_square(uci+2),uci[4],e,n);
}
int chess_has_moves(const Chess *g) {
    for(int a=0;a<64;a++) if(side(g->board[a])==g->turn)
        for(int b=0;b<64;b++) { Chess copy=*g;
            char promo=tolower((unsigned char)g->board[a])=='p' && (b/8==0||b/8==7)?'q':0;
            if(apply(&copy,a,b,promo,NULL,0)) return 1;
        }
    return 0;
}
void chess_fen(const Chess *g,char out[128]) {
    char *p=out;
    for(int r=0;r<8;r++) {int empty=0;
        for(int c=0;c<8;c++) {char v=g->board[r*8+c];if(v=='.') empty++;else {if(empty)*p++=(char)('0'+empty);empty=0;*p++=v;}}
        if(empty)*p++=(char)('0'+empty);
        if(r!=7)*p++='/';
    }
    *p++=' ';*p++=g->turn?'b':'w';*p++=' ';
    if(!g->castling)*p++='-';else for(int i=0;i<4;i++) if(g->castling&(1u<<i))*p++="KQkq"[i];
    *p++=' ';if(g->ep<0)*p++='-';else {*p++=(char)('a'+g->ep%8);*p++=(char)('8'-g->ep/8);}
    snprintf(p,32," %d %d",g->halfmove,g->fullmove);
}
