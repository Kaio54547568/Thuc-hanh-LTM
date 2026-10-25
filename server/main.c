#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];
Storage db;
static volatile sig_atomic_t running=1;
static void stop_server(int signum){(void)signum;running=0;}
void emit(int i,const cJSON *j){if(stream_queue(&clients[i].out,j))clients[i].closing=1;}
void reply(int i,const char *type,const cJSON *req,const char *code){
    cJSON *j=message(type);const char *id=json_string(req,"request_id");
    if(*id)cJSON_AddStringToObject(j,"request_id",id);
    if(code)cJSON_AddStringToObject(j,"code",code);
    emit(i,j);cJSON_Delete(j);
}
void error(int i,const cJSON *req,const char *code){reply(i,"ERROR",req,code);}
void lobby(void){
    cJSON *j=message("LOBBY"),*list=cJSON_AddArrayToObject(j,"players");
    for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].fd!=NET_INVALID && clients[i].name[0] && !clients[i].closing){
        cJSON *p=cJSON_CreateObject();cJSON_AddStringToObject(p,"name",clients[i].name);
        cJSON_AddStringToObject(p,"status",clients[i].room<0?"ready":"playing");cJSON_AddItemToArray(list,p);
    }
    for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].fd!=NET_INVALID && clients[i].name[0])emit(i,j);
    cJSON_Delete(j);
}
static void clear_invites(int i,const char *reason){
    for(int k=0;k<MAX_CLIENTS;k++)if(clients[k].fd!=NET_INVALID && (k==i || clients[k].invite==i) && clients[k].invite>=0){
        int other=clients[k].invite;cJSON *j=message("INVITE_CLOSED");cJSON_AddStringToObject(j,"reason",reason);
        emit(k,j);if(clients[other].fd!=NET_INVALID)emit(other,j);cJSON_Delete(j);clients[k].invite=-1;
    }
}
static void disconnect_client(int i){
    int r=clients[i].room;
    if(r>=0 && rooms[r].active){
        const char *res=rooms[r].players[0]==i?"0-1":"1-0";
        if(!storage_finish(&db,rooms[r].game,res,"disconnect"))fprintf(stderr,"Failed to persist disconnect result\n");
        end_room(r,res,"disconnect");
    }
    clear_invites(i,"disconnect");
    fprintf(stderr,"Disconnected: %s\n",clients[i].name);net_close(clients[i].fd);memset(&clients[i],0,sizeof(Client));
    clients[i].fd=NET_INVALID;clients[i].room=clients[i].invite=-1;lobby();
}
static int valid_name(const char *s){
    size_t n=strlen(s);if(!n||n>24)return 0;
    for(size_t k=0;k<n;k++)if(!((s[k]>='a'&&s[k]<='z')||(s[k]>='A'&&s[k]<='Z')||(s[k]>='0'&&s[k]<='9')||s[k]=='_'))return 0;
    return 1;
}
static int has_invite(int i){
    if(clients[i].invite>=0)return 1;
    for(int k=0;k<MAX_CLIENTS;k++)if(clients[k].fd!=NET_INVALID && clients[k].invite==i)return 1;
    return 0;
}
static void handle(int i,cJSON *req){
    Client *c=&clients[i];const char *type=json_string(req,"type"),*id=json_string(req,"request_id");
    if(!*id || strlen(id)>64){error(i,req,"BAD_REQUEST_ID");return;}
    if(!strcmp(type,"HELLO")){
        if(c->name[0]){error(i,req,"ALREADY_JOINED");return;}
        const char *name=json_string(req,"name");if(!valid_name(name)){error(i,req,"BAD_NAME");return;}
        for(int k=0;k<MAX_CLIENTS;k++)if(clients[k].fd!=NET_INVALID && !strcmp(name,clients[k].name)){error(i,req,"NAME_TAKEN");return;}
        strcpy(c->name,name);reply(i,"WELCOME",req,NULL);lobby();return;
    }
    if(!c->name[0]){error(i,req,"HELLO_REQUIRED");return;}
    if(!strcmp(type,"PING")){reply(i,"PONG",req,NULL);return;}
    if(!strcmp(type,"LIST")){lobby();return;}
    if(!strcmp(type,"CHALLENGE")){
        if(c->room>=0 || has_invite(i)){error(i,req,"BUSY");return;}
        int target=-1;const char *name=json_string(req,"target");
        for(int k=0;k<MAX_CLIENTS;k++)if(clients[k].fd!=NET_INVALID && !clients[k].closing && !strcmp(clients[k].name,name))target=k;
        if(target<0 || target==i){error(i,req,"BAD_TARGET");return;}
        if(clients[target].room>=0 || has_invite(target)){error(i,req,"TARGET_BUSY");return;}
        clients[target].invite=i;clients[target].invited=time(NULL);
        cJSON *j=message("INVITE");cJSON_AddStringToObject(j,"from",c->name);cJSON_AddNumberToObject(j,"expires_in",30);emit(target,j);cJSON_Delete(j);
        reply(i,"CHALLENGE_SENT",req,NULL);return;
    }
    if(!strcmp(type,"ACCEPT") || !strcmp(type,"DECLINE")){
        int other=c->invite;if(other<0 || clients[other].fd==NET_INVALID){error(i,req,"NO_INVITE");return;}
        if(time(NULL)-c->invited>=30){clear_invites(i,"expired");error(i,req,"NO_INVITE");return;}
        if(!strcmp(type,"DECLINE")){clear_invites(i,"declined");reply(i,"OK",req,NULL);return;}
        int r=-1;for(int k=0;k<MAX_ROOMS;k++)if(!rooms[k].active){r=k;break;}
        if(r<0){error(i,req,"SERVER_FULL");return;}
        Room room={0};room.players[0]=other;room.players[1]=i;chess_init(&room.chess);char fen[128];chess_fen(&room.chess,fen);
        room.game=storage_start(&db,clients[other].name,c->name,fen);if(!room.game){error(i,req,"STORAGE_ERROR");return;}
        room.active=1;rooms[r]=room;c->invite=-1;c->room=clients[other].room=r;
        reply(i,"OK",req,NULL);state(r,"",id);lobby();return;
    }
    if(!strcmp(type,"MOVE") || !strcmp(type,"RESIGN")){handle_game(i,req);return;}
    error(i,req,"UNKNOWN_COMMAND");
}
int main(int argc,char **argv){
    const char *host=argc>1?argv[1]:"127.0.0.1",*path=argc>3?argv[3]:"chess.db";char *end=NULL;
    long port=argc>2?strtol(argv[2],&end,10):5555;
    if(port<1||port>65535||(end&&*end)){fprintf(stderr,"Usage: server [IPv4] [port] [database]\n");return 1;}
    if(net_init() || !storage_open(&db,path)){fprintf(stderr,"Network/database init failed\n");return 1;}
    socket_t listener=net_listen(host,(unsigned short)port);if(listener==NET_INVALID){fprintf(stderr,"Listen failed\n");storage_close(&db);return 1;}
    for(int i=0;i<MAX_CLIENTS;i++){clients[i].fd=NET_INVALID;clients[i].room=clients[i].invite=-1;}
    signal(SIGINT,stop_server);signal(SIGTERM,stop_server);
    printf("Chess milestone server %s:%ld (guest mode, max %d clients)\n",host,port,MAX_CLIENTS);fflush(stdout);
    while(running){
        net_pollfd fds[MAX_CLIENTS+1];memset(fds,0,sizeof(fds));fds[0].fd=listener;fds[0].events=POLLIN;
        for(int i=0;i<MAX_CLIENTS;i++){fds[i+1].fd=clients[i].fd;fds[i+1].events=POLLIN|(clients[i].out.size?POLLOUT:0);}
        if(net_poll(fds,MAX_CLIENTS+1,100)<0){if(net_would_block())continue;break;}
        if(fds[0].revents&POLLIN){
            socket_t fd=accept(listener,NULL,NULL);
            if(fd!=NET_INVALID){int slot=-1;for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].fd==NET_INVALID){slot=i;break;}
                if(slot<0 || net_nonblock(fd))net_close(fd);else{clients[slot].fd=fd;clients[slot].connected=time(NULL);}
            }
        }
        for(int i=0;i<MAX_CLIENTS;i++){
            Client *c=&clients[i];if(c->fd==NET_INVALID)continue;
            short ev=fds[i+1].fd==c->fd?fds[i+1].revents:0;
            if(ev&(POLLERR|POLLHUP|POLLNVAL))c->closing=1;
            if(!c->closing && (ev&POLLIN)){
                unsigned char buf[4096];int n=net_recv(c->fd,buf,sizeof(buf));
                if(n==0 || (n<0 && !net_would_block()))c->closing=1;
                if(n>0){
                    if(stream_feed(&c->in,buf,(size_t)n))c->closing=1;
                    else {cJSON *req=NULL;int rc;while(!c->closing && (rc=stream_next(&c->in,&req))==1){handle(i,req);cJSON_Delete(req);}
                        if(!c->closing && rc<0)c->closing=1;
                    }
                }
            }
            if(!c->closing && stream_flush(&c->out,c->fd))c->closing=1;
            if(!c->name[0] && time(NULL)-c->connected>=15)c->closing=1;
            if(c->invite>=0 && time(NULL)-c->invited>=30)clear_invites(i,"expired");
            if(c->closing)disconnect_client(i);
        }
    }
    /* Graceful shutdown records unfinished games as aborted, not player losses. */
    for(int r=0;r<MAX_ROOMS;r++)if(rooms[r].active)storage_finish(&db,rooms[r].game,"aborted","server_shutdown");
    for(int i=0;i<MAX_CLIENTS;i++)net_close(clients[i].fd);
    net_close(listener);storage_close(&db);return 0;
}
