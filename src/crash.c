#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc,char *argv[])
{
GtkWidget *Win,*VBox,*Label,*But,*HSep,*HBox,*VBox1;

gtk_init(&argc,&argv);
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(GTK_WIDGET(Win),415,280);
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_main_quit),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Crash");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);

VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox1,FALSE,FALSE,0); 
gtk_widget_set_usize(GTK_WIDGET(VBox1),300,88);
Label=gtk_label_new("LAMPS has crashed with an irrecoverable error"); gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
if (argc>1) { Label=gtk_label_new(argv[1]); gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0); }
if (argc>2) { Label=gtk_label_new(argv[2]); gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0); }

HSep=gtk_hseparator_new(); gtk_box_pack_start(GTK_BOX(VBox1),HSep,TRUE,FALSE,2);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Okay"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_main_quit),NULL); 

gtk_widget_show_all(Win);
gtk_main();
return(0);
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
