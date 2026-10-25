/* Exercise real GTK widgets/callbacks against a real server in a virtual display. */
#include <stdio.h>
#define main chess_client_entry
#include "../client/main.c"
#undef main
static const char *test_name,*test_port;
static int phase,moves,success;
static gint64 deadline,finished_at;
static int square_index(const char *s){return ('8'-s[1])*8+s[0]-'a';}
static void click_move(const char *uci){
    int a=square_index(uci),b=square_index(uci+2);
    if(app.my_side){a=63-a;b=63-b;}
    g_signal_emit_by_name(app.squares[a],"clicked");
    g_signal_emit_by_name(app.squares[b],"clicked");
}
static int capture(void){
    int width=gtk_widget_get_width(app.window),height=gtk_widget_get_height(app.window);
    GdkPaintable *paintable=gtk_widget_paintable_new(app.window);GtkSnapshot *snapshot=gtk_snapshot_new();
    gdk_paintable_snapshot(paintable,GDK_SNAPSHOT(snapshot),width,height);
    GskRenderNode *node=gtk_snapshot_free_to_node(snapshot);g_object_unref(paintable);
    if(!node)return 0;
    GskRenderer *renderer=gtk_native_get_renderer(GTK_NATIVE(app.window));
    GdkTexture *texture=gsk_renderer_render_texture(renderer,node,NULL);gsk_render_node_unref(node);
    char path[128];g_snprintf(path,sizeof(path),"build/gui-%s.png",test_name);
    int ok=gdk_texture_save_to_png(texture,path);g_object_unref(texture);return ok;
}
static gboolean tick(gpointer data){
    GtkApplication *application=data;gint64 now=g_get_monotonic_time();
    if(now>deadline){g_printerr("GUI timeout %s: %s\n",test_name,gtk_label_get_text(GTK_LABEL(app.status)));g_application_quit(G_APPLICATION(application));return G_SOURCE_REMOVE;}
    int white=!strcmp(test_name,"Loc");
    if((phase==2 || phase==3) && !app.playing && moves>0){
        const char *result=gtk_label_get_text(GTK_LABEL(app.result));
        if(!strstr(result,"0-1") || !strstr(result,"checkmate")){g_printerr("Wrong result %s\n",result);g_application_quit(G_APPLICATION(application));return G_SOURCE_REMOVE;}
        if(gtk_widget_get_sensitive(app.squares[0])||gtk_widget_get_sensitive(app.resign)){g_printerr("Board not locked\n");g_application_quit(G_APPLICATION(application));return G_SOURCE_REMOVE;}
        finished_at=now;phase=4;
    }
    if(phase==0 && app.connected){
        if(white){
            int count=0;for(GtkWidget *c=gtk_widget_get_first_child(app.players);c;c=gtk_widget_get_next_sibling(c))count++;
            if(count>=2){gtk_editable_set_text(GTK_EDITABLE(app.target),"Viet");g_signal_emit_by_name(app.challenge,"clicked");phase=1;}
        }else if(gtk_widget_get_sensitive(app.accept)){g_signal_emit_by_name(app.accept,"clicked");phase=1;}
    }else if(phase==1 && app.playing)phase=2;
    else if(phase==2){
        if(app.playing && !strcmp(app.turn,white?"white":"black")){
            /* Wait for STATE confirming each previous move before clicking again. */
            if(moves==0){click_move(white?"f2f3":"e7e5");moves=1;phase=3;}
            else if(moves==1){click_move(white?"g2g4":"d8h4");moves=2;phase=3;}
        }
    }else if(phase==3){
        if(strcmp(app.turn,white?"white":"black"))phase=2;
    }else if(phase==4 && now-finished_at>500000){
        success=capture();g_print("%s GUI %s: invitation, board clicks, real TCP, checkmate, locked board, screenshot\n",success?"PASS":"FAIL",test_name);
        g_application_quit(G_APPLICATION(application));return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}
static void test_activate(GtkApplication *application,gpointer data){
    (void)data;activate(application,NULL);gtk_editable_set_text(GTK_EDITABLE(app.name),test_name);gtk_editable_set_text(GTK_EDITABLE(app.port),test_port);
    g_signal_emit_by_name(app.connect,"clicked");g_timeout_add(100,tick,application);
}
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"test-gui Loc|Viet port\n");return 1;}
    test_name=argv[1];test_port=argv[2];deadline=g_get_monotonic_time()+20000000;
    if(net_init())return 1;
    app.incoming=g_async_queue_new();app.outgoing=g_async_queue_new();
    GtkApplication *application=gtk_application_new("vn.networklab.chess.test",G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application,"activate",G_CALLBACK(test_activate),NULL);
    char *args[]={argv[0],NULL};g_application_run(G_APPLICATION(application),1,args);
    app.closing=TRUE;g_atomic_int_set(&app.stop,1);if(app.worker)g_thread_join(app.worker);
    cJSON *j;while((j=g_async_queue_try_pop(app.incoming)))cJSON_Delete(j);while((j=g_async_queue_try_pop(app.outgoing)))cJSON_Delete(j);
    g_async_queue_unref(app.incoming);g_async_queue_unref(app.outgoing);g_object_unref(application);return success?0:1;
}
