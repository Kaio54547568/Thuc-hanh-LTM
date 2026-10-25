/* Loc: GTK owns widgets; the worker owns the socket. Only JSON crosses queues. */
#include <gtk/gtk.h>
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
    GtkWidget *window,*host,*port,*name,*connect,*status,*players,*target,*challenge;
    GtkWidget *invite,*accept,*decline,*info,*result,*resign,*promotion,*squares[64];
    GAsyncQueue *incoming,*outgoing;GThread *worker;
    gint stop;gboolean closing,connected,playing;int room,selected,my_side;unsigned request;
    char username[25],board[65],turn[8];
} App;
static App app;
typedef struct {char host[64],name[25];unsigned short port;} Connection;
static void set_status(const char *s){gtk_label_set_text(GTK_LABEL(app.status),s);}
static void enqueue_event(cJSON *j){
    if(g_async_queue_length(app.incoming)<128)g_async_queue_push(app.incoming,j);
    else if(!strcmp(json_string(j,"type"),"DISCONNECTED")){
        cJSON *old=g_async_queue_try_pop(app.incoming);cJSON_Delete(old);g_async_queue_push(app.incoming,j);
    }else {cJSON_Delete(j);g_atomic_int_set(&app.stop,1);}
}
static void local_event(const char *type,const char *detail){cJSON *j=message(type);cJSON_AddStringToObject(j,"detail",detail);enqueue_event(j);}
static gpointer network_worker(gpointer data){
    Connection *config=data;socket_t fd=net_connect(config->host,config->port,3000);
    if(fd==NET_INVALID){local_event("DISCONNECTED","Connection failed. Check numeric IPv4, port and server.");g_free(config);return NULL;}
    Stream in={0},out={0};cJSON *hello=message("HELLO");cJSON_AddStringToObject(hello,"request_id","hello");cJSON_AddStringToObject(hello,"name",config->name);
    stream_queue(&out,hello);cJSON_Delete(hello);g_free(config);
    while(!g_atomic_int_get(&app.stop)){
        cJSON *j;while((j=g_async_queue_try_pop(app.outgoing))!=NULL){int rc=stream_queue(&out,j);cJSON_Delete(j);if(rc)goto done;}
        net_pollfd p={0};p.fd=fd;p.events=POLLIN|(out.size?POLLOUT:0);
        if(net_poll(&p,1,30)<0){if(net_would_block())continue;break;}
        if(p.revents&(POLLERR|POLLHUP|POLLNVAL))break;
        if(p.revents&POLLIN){unsigned char bytes[4096];int n=net_recv(fd,bytes,sizeof(bytes));
            if(!n || (n<0&&!net_would_block()))break;
            if(n>0){if(stream_feed(&in,bytes,(size_t)n))break;
                int rc;while((rc=stream_next(&in,&j))==1)enqueue_event(j);if(rc<0)break;
            }
        }
        if(stream_flush(&out,fd))break;
    }
done:
    net_close(fd);local_event("DISCONNECTED","Disconnected. Active games cannot be resumed in this milestone.");return NULL;
}
static cJSON *command(const char *type){
    cJSON *j=message(type);char id[32];g_snprintf(id,sizeof(id),"gui-%u",++app.request);cJSON_AddStringToObject(j,"request_id",id);return j;
}
static void send_command(cJSON *j){
    if(!app.connected){cJSON_Delete(j);set_status("Connect and join first.");return;}
    if(g_async_queue_length(app.outgoing)>=128){cJSON_Delete(j);set_status("Too many pending commands.");return;}
    g_async_queue_push(app.outgoing,j);
}
static const char *piece_symbol(char p){
    switch(p){case 'K':return "♔";case 'Q':return "♕";case 'R':return "♖";case 'B':return "♗";case 'N':return "♘";case 'P':return "♙";
    case 'k':return "♚";case 'q':return "♛";case 'r':return "♜";case 'b':return "♝";case 'n':return "♞";case 'p':return "♟";default:return "";}
}
static void render_board(void){
    for(int view=0;view<64;view++){
        int square=app.my_side==1?63-view:view;GtkWidget *button=app.squares[view];
        gtk_button_set_label(GTK_BUTTON(button),piece_symbol(app.board[square]));
        char tooltip[16];g_snprintf(tooltip,sizeof(tooltip),"%c%c",'a'+square%8,'8'-square/8);gtk_widget_set_tooltip_text(button,tooltip);
        if(square==app.selected)gtk_widget_add_css_class(button,"selected");else gtk_widget_remove_css_class(button,"selected");
        gtk_widget_set_sensitive(button,app.connected&&app.playing);
    }
}
static void on_square(GtkButton *button,gpointer user){
    (void)button;int view=GPOINTER_TO_INT(user),square=app.my_side==1?63-view:view;
    if(!app.playing || !app.connected)return;
    if(strcmp(app.turn,app.my_side?"black":"white")){set_status("Wait for your opponent.");return;}
    char p=app.board[square];int own=app.my_side?(p>='a'&&p<='z'):(p>='A'&&p<='Z');
    if(own){app.selected=square;render_board();return;}
    if(app.selected<0){set_status("Select one of your pieces first.");return;}
    char uci[6]={(char)('a'+app.selected%8),(char)('8'-app.selected/8),(char)('a'+square%8),(char)('8'-square/8),0,0};
    char moving=app.board[app.selected];
    if((moving=='P'||moving=='p')&&(square/8==0||square/8==7)){
        guint choice=gtk_drop_down_get_selected(GTK_DROP_DOWN(app.promotion));uci[4]="qrbn"[choice<4?choice:0];
    }
    cJSON *j=command("MOVE");cJSON_AddNumberToObject(j,"room_id",app.room);cJSON_AddStringToObject(j,"uci",uci);send_command(j);
    app.selected=-1;render_board();set_status("Move sent; waiting for server validation.");
}
static void on_challenge(GtkButton *button,gpointer data){
    (void)button;(void)data;cJSON *j=command("CHALLENGE");
    cJSON_AddStringToObject(j,"target",gtk_editable_get_text(GTK_EDITABLE(app.target)));send_command(j);
}
static void on_invite(GtkButton *button,gpointer data){
    (void)button;send_command(command((const char *)data));gtk_widget_set_sensitive(app.accept,FALSE);gtk_widget_set_sensitive(app.decline,FALSE);
}
static void resign_confirm(GtkButton *button,gpointer dialog){
    (void)button;if(app.playing){cJSON *j=command("RESIGN");cJSON_AddNumberToObject(j,"room_id",app.room);send_command(j);}
    gtk_window_destroy(GTK_WINDOW(dialog));
}
static void resign_cancel(GtkButton *button,gpointer dialog){
    (void)button;gtk_window_destroy(GTK_WINDOW(dialog));
}
static void on_resign(GtkButton *button,gpointer data){
    (void)button;(void)data;
    GtkWidget *dialog=gtk_window_new();gtk_window_set_title(GTK_WINDOW(dialog),"Confirm resignation");
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(app.window));gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog),TRUE);
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,16);gtk_widget_set_margin_top(box,24);gtk_widget_set_margin_bottom(box,24);gtk_widget_set_margin_start(box,24);gtk_widget_set_margin_end(box,24);
    gtk_window_set_child(GTK_WINDOW(dialog),box);gtk_box_append(GTK_BOX(box),gtk_label_new("Resign this game?"));
    GtkWidget *yes=gtk_button_new_with_label("Resign"),*no=gtk_button_new_with_label("Cancel");gtk_box_append(GTK_BOX(box),no);gtk_box_append(GTK_BOX(box),yes);
    g_signal_connect(yes,"clicked",G_CALLBACK(resign_confirm),dialog);g_signal_connect(no,"clicked",G_CALLBACK(resign_cancel),dialog);gtk_window_present(GTK_WINDOW(dialog));
}
static void on_connect(GtkButton *button,gpointer data){
    (void)button;(void)data;if(app.worker)return;
    const char *name=gtk_editable_get_text(GTK_EDITABLE(app.name)),*port=gtk_editable_get_text(GTK_EDITABLE(app.port));char *end=NULL;
    long value=strtol(port,&end,10);
    if(!*name || strlen(name)>24 || value<1 || value>65535 || *end){set_status("Name: 1-24 ASCII letters/digits/_. Port: 1-65535.");return;}
    Connection *c=g_new0(Connection,1);g_strlcpy(c->host,gtk_editable_get_text(GTK_EDITABLE(app.host)),sizeof(c->host));g_strlcpy(c->name,name,sizeof(c->name));c->port=(unsigned short)value;
    g_strlcpy(app.username,name,sizeof(app.username));g_atomic_int_set(&app.stop,0);gtk_widget_set_sensitive(app.connect,FALSE);
    gtk_widget_set_sensitive(app.name,FALSE);set_status("Connecting...");app.worker=g_thread_new("chess-network",network_worker,c);
}
static gboolean process_events(gpointer data){
    (void)data;if(app.closing)return G_SOURCE_REMOVE;
    cJSON *j;
    /* Bound work per tick so a busy server cannot starve GTK rendering. */
    for(int batch=0;batch<32 && (j=g_async_queue_try_pop(app.incoming))!=NULL;batch++){
        const char *type=json_string(j,"type");
        if(!strcmp(type,"WELCOME")){app.connected=TRUE;set_status("Joined as guest. Challenge an available player.");gtk_widget_set_sensitive(app.challenge,TRUE);}
        else if(!strcmp(type,"LOBBY")){
            GtkWidget *child;while((child=gtk_widget_get_first_child(app.players)))gtk_list_box_remove(GTK_LIST_BOX(app.players),child);
            const cJSON *p,*players=cJSON_GetObjectItemCaseSensitive(j,"players");
            cJSON_ArrayForEach(p,players){char line[96];g_snprintf(line,sizeof(line),"%s  ·  %s",json_string(p,"name"),json_string(p,"status"));
                GtkWidget *label=gtk_label_new(line);gtk_label_set_xalign(GTK_LABEL(label),0);gtk_widget_set_margin_top(label,8);gtk_widget_set_margin_bottom(label,8);gtk_list_box_append(GTK_LIST_BOX(app.players),label);}
        }else if(!strcmp(type,"INVITE")){
            char text[128];g_snprintf(text,sizeof(text),"%s challenges you (30s)",json_string(j,"from"));gtk_label_set_text(GTK_LABEL(app.invite),text);
            gtk_widget_set_sensitive(app.accept,TRUE);gtk_widget_set_sensitive(app.decline,TRUE);
        }else if(!strcmp(type,"INVITE_CLOSED")){
            gtk_label_set_text(GTK_LABEL(app.invite),json_string(j,"reason"));gtk_widget_set_sensitive(app.accept,FALSE);gtk_widget_set_sensitive(app.decline,FALSE);
        }else if(!strcmp(type,"STATE")){
            const char *board=json_string(j,"board");const cJSON *rid=cJSON_GetObjectItemCaseSensitive(j,"room_id");
            if(strlen(board)==64 && cJSON_IsNumber(rid)){
                app.room=rid->valueint;app.playing=TRUE;app.my_side=!strcmp(app.username,json_string(j,"white"))?0:1;
                memcpy(app.board,board,65);g_strlcpy(app.turn,json_string(j,"turn"),sizeof(app.turn));app.selected=-1;
                char info[256];const cJSON *check=cJSON_GetObjectItemCaseSensitive(j,"check");
                g_snprintf(info,sizeof(info),"%s vs %s\nYou: %s · Turn: %s%s\nLast move: %s",json_string(j,"white"),json_string(j,"black"),app.my_side?"Black":"White",app.turn,cJSON_IsTrue(check)?" · CHECK":"",json_string(j,"last_move"));
                gtk_label_set_text(GTK_LABEL(app.info),info);gtk_label_set_text(GTK_LABEL(app.result),"");gtk_label_set_text(GTK_LABEL(app.invite),"Game started");
                gtk_widget_set_sensitive(app.accept,FALSE);gtk_widget_set_sensitive(app.decline,FALSE);gtk_widget_set_sensitive(app.challenge,FALSE);gtk_widget_set_sensitive(app.resign,TRUE);render_board();set_status("Board confirmed by server.");
            }
        }else if(!strcmp(type,"GAME_OVER")){
            char text[128];g_snprintf(text,sizeof(text),"Result %s · %s",json_string(j,"result"),json_string(j,"reason"));gtk_label_set_text(GTK_LABEL(app.result),text);
            app.playing=FALSE;app.selected=-1;gtk_label_set_text(GTK_LABEL(app.invite),"Game finished");gtk_widget_set_sensitive(app.resign,FALSE);gtk_widget_set_sensitive(app.challenge,TRUE);render_board();set_status("Game finished. You can challenge again.");
        }else if(!strcmp(type,"ERROR")){
            char text[256];g_snprintf(text,sizeof(text),"%s %s",json_string(j,"code"),json_string(j,"detail"));set_status(text);
            if(!app.connected)g_atomic_int_set(&app.stop,1);
        }else if(!strcmp(type,"CHALLENGE_SENT"))set_status("Challenge sent. Waiting for acceptance (30s).");
        else if(!strcmp(type,"DISCONNECTED")){
            app.connected=app.playing=FALSE;app.selected=-1;set_status(json_string(j,"detail"));
            if(app.worker){g_thread_join(app.worker);app.worker=NULL;}
            cJSON *pending;while((pending=g_async_queue_try_pop(app.outgoing)))cJSON_Delete(pending);
            gtk_widget_set_sensitive(app.connect,TRUE);gtk_widget_set_sensitive(app.name,TRUE);gtk_widget_set_sensitive(app.challenge,FALSE);
            gtk_widget_set_sensitive(app.accept,FALSE);gtk_widget_set_sensitive(app.decline,FALSE);gtk_widget_set_sensitive(app.resign,FALSE);render_board();
        }
        cJSON_Delete(j);
    }return G_SOURCE_CONTINUE;
}
static gboolean on_close(GtkWindow *window,gpointer data){(void)window;(void)data;app.closing=TRUE;g_atomic_int_set(&app.stop,1);return FALSE;}
static GtkWidget *label(const char *s){GtkWidget *w=gtk_label_new(s);gtk_label_set_xalign(GTK_LABEL(w),0);gtk_label_set_wrap(GTK_LABEL(w),TRUE);return w;}
static GtkWidget *entry(const char *text,int width){GtkWidget *e=gtk_entry_new();gtk_editable_set_text(GTK_EDITABLE(e),text);gtk_widget_set_size_request(e,width,-1);return e;}
static void activate(GtkApplication *application,gpointer data){
    (void)data;app.window=gtk_application_window_new(application);gtk_window_set_title(GTK_WINDOW(app.window),"Chess Network · 30% milestone");gtk_window_set_default_size(GTK_WINDOW(app.window),1080,800);
    g_signal_connect(app.window,"close-request",G_CALLBACK(on_close),NULL);
    GtkCssProvider *css=gtk_css_provider_new();
    const char *stylesheet=
        "window{background:#161b25;color:#edf1f7;} .title{font-size:26px;font-weight:bold;}"
        "entry{border-radius:8px;} button{border-radius:8px;padding:8px;} .primary{background:#6c5ce7;color:white;} .primary:disabled{background:#414252;color:#aeb6c4;}"
        ".square{font-size:40px;border-radius:0;padding:0;min-width:48px;min-height:48px;border:0;color:#171c26;}"
        ".light{background:#e4dfce;} .dark{background:#819786;} .selected{background:#e6bf58;}"
        ".panel{background:#232b3a;border-radius:12px;padding:16px;} list{background:transparent;color:#edf1f7;}";
#if GTK_CHECK_VERSION(4,12,0)
    gtk_css_provider_load_from_string(css,stylesheet);
#else
    gtk_css_provider_load_from_data(css,stylesheet,-1);
#endif
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(app.window),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);g_object_unref(css);
    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,16);gtk_widget_set_margin_start(root,24);gtk_widget_set_margin_end(root,24);gtk_widget_set_margin_top(root,20);gtk_widget_set_margin_bottom(root,20);gtk_window_set_child(GTK_WINDOW(app.window),root);
    GtkWidget *title=label("CHESS / NETWORK LAB");gtk_widget_add_css_class(title,"title");gtk_box_append(GTK_BOX(root),title);
    GtkWidget *connection=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_append(GTK_BOX(root),connection);
    app.host=entry("127.0.0.1",140);app.port=entry("5555",70);app.name=entry("Loc",110);app.connect=gtk_button_new_with_label("Connect as guest");gtk_widget_add_css_class(app.connect,"primary");
    gtk_box_append(GTK_BOX(connection),label("IPv4"));gtk_box_append(GTK_BOX(connection),app.host);gtk_box_append(GTK_BOX(connection),label("Port"));gtk_box_append(GTK_BOX(connection),app.port);gtk_box_append(GTK_BOX(connection),label("Name"));gtk_box_append(GTK_BOX(connection),app.name);gtk_box_append(GTK_BOX(connection),app.connect);
    g_signal_connect(app.connect,"clicked",G_CALLBACK(on_connect),NULL);
    GtkWidget *content=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,24);gtk_widget_set_vexpand(content,TRUE);gtk_box_append(GTK_BOX(root),content);
    GtkWidget *left=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);gtk_widget_set_hexpand(left,TRUE);gtk_box_append(GTK_BOX(content),left);
    app.info=label("Connect two clients with different names.\nClick a piece, then its destination. Hover for coordinates.");gtk_box_append(GTK_BOX(left),app.info);
    GtkWidget *aspect=gtk_aspect_frame_new(0.5f,0.5f,1.0f,FALSE);gtk_widget_set_vexpand(aspect,TRUE);gtk_widget_set_hexpand(aspect,TRUE);gtk_box_append(GTK_BOX(left),aspect);
    GtkWidget *grid=gtk_grid_new();gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);gtk_aspect_frame_set_child(GTK_ASPECT_FRAME(aspect),grid);
    for(int i=0;i<64;i++){GtkWidget *b=gtk_button_new();app.squares[i]=b;gtk_widget_add_css_class(b,"square");gtk_widget_add_css_class(b,(i/8+i%8)%2?"dark":"light");gtk_widget_set_hexpand(b,TRUE);gtk_widget_set_vexpand(b,TRUE);gtk_grid_attach(GTK_GRID(grid),b,i%8,i/8,1,1);g_signal_connect(b,"clicked",G_CALLBACK(on_square),GINT_TO_POINTER(i));}
    GtkWidget *controls=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_append(GTK_BOX(left),controls);gtk_box_append(GTK_BOX(controls),gtk_label_new("Promote to:"));
    const char *choices[]={"Queen","Rook","Bishop","Knight",NULL};app.promotion=gtk_drop_down_new_from_strings(choices);gtk_box_append(GTK_BOX(controls),app.promotion);
    app.resign=gtk_button_new_with_label("Resign");gtk_widget_set_sensitive(app.resign,FALSE);gtk_box_append(GTK_BOX(controls),app.resign);g_signal_connect(app.resign,"clicked",G_CALLBACK(on_resign),NULL);
    app.result=label("");gtk_box_append(GTK_BOX(left),app.result);
    GtkWidget *right=gtk_box_new(GTK_ORIENTATION_VERTICAL,12);gtk_widget_set_size_request(right,260,-1);gtk_widget_add_css_class(right,"panel");gtk_box_append(GTK_BOX(content),right);
    gtk_box_append(GTK_BOX(right),label("ONLINE PLAYERS"));app.players=gtk_list_box_new();gtk_list_box_set_selection_mode(GTK_LIST_BOX(app.players),GTK_SELECTION_NONE);
    GtkWidget *scroll=gtk_scrolled_window_new();gtk_widget_set_vexpand(scroll,TRUE);gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),app.players);gtk_box_append(GTK_BOX(right),scroll);
    app.target=entry("",180);gtk_entry_set_placeholder_text(GTK_ENTRY(app.target),"Opponent's exact name");gtk_box_append(GTK_BOX(right),app.target);
    app.challenge=gtk_button_new_with_label("Challenge");gtk_widget_add_css_class(app.challenge,"primary");gtk_widget_set_sensitive(app.challenge,FALSE);gtk_box_append(GTK_BOX(right),app.challenge);g_signal_connect(app.challenge,"clicked",G_CALLBACK(on_challenge),NULL);
    app.invite=label("No invitation");gtk_box_append(GTK_BOX(right),app.invite);
    app.accept=gtk_button_new_with_label("Accept");app.decline=gtk_button_new_with_label("Decline");gtk_widget_set_sensitive(app.accept,FALSE);gtk_widget_set_sensitive(app.decline,FALSE);
    gtk_box_append(GTK_BOX(right),app.accept);gtk_box_append(GTK_BOX(right),app.decline);g_signal_connect(app.accept,"clicked",G_CALLBACK(on_invite),"ACCEPT");g_signal_connect(app.decline,"clicked",G_CALLBACK(on_invite),"DECLINE");
    gtk_box_append(GTK_BOX(right),label("Milestone: guest play + legal moves + saved games.\nAccounts, Elo and replay UI come next."));
    app.status=label("Not connected");gtk_box_append(GTK_BOX(root),app.status);
    memcpy(app.board,"rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR",65);app.selected=-1;render_board();
    g_timeout_add(30,process_events,NULL);gtk_window_present(GTK_WINDOW(app.window));
}
int main(int argc,char **argv){
    if(net_init())return 1;
    app.incoming=g_async_queue_new();app.outgoing=g_async_queue_new();
    GtkApplication *application=gtk_application_new("vn.networklab.chess",G_APPLICATION_NON_UNIQUE);g_signal_connect(application,"activate",G_CALLBACK(activate),NULL);
    int status=g_application_run(G_APPLICATION(application),argc,argv);g_atomic_int_set(&app.stop,1);if(app.worker)g_thread_join(app.worker);
    cJSON *j;while((j=g_async_queue_try_pop(app.incoming)))cJSON_Delete(j);while((j=g_async_queue_try_pop(app.outgoing)))cJSON_Delete(j);
    g_async_queue_unref(app.incoming);g_async_queue_unref(app.outgoing);g_object_unref(application);return status;
}
