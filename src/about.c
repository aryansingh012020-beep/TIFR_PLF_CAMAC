#include <gtk/gtk.h>
#include <stdlib.h>
/*Function templates*/
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void WaitBox(gint XPos,gchar *Messg);

/*-----------------------------------------------------------------------------------------------*/
void DoHelp(GtkWidget *W,gpointer Unused)
{
system("nice -n 10 mozilla -height 300 -width 600 file:///home/lamps/help/help.htm&");
gtk_main_quit();
}
/*-----------------------------------------------------------------------------------------------*/
int main(int argc,char *argv[])
{
GtkWidget *Win,*VBox,*Label,*But,*HSep,*HBox,*VBox1;
GtkStyle *RedStyle,*GreenStyle,*BlueStyle;
static GdkColor C_Red    = {0,0xFFFF,0x0000,0x0000};
static GdkColor C_Green  = {0,0x0000,0x7777,0x0000};
static GdkColor C_Blue   = {0,0x0000,0x0000,0xAAAA};
gint i;

gtk_init(&argc,&argv);

RedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) RedStyle->fg[i]=RedStyle->text[i]=C_Red;
GreenStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) GreenStyle->fg[i]=GreenStyle->text[i]=C_Green;
BlueStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) BlueStyle->fg[i]=BlueStyle->text[i]=C_Blue;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(GTK_WIDGET(Win),415,280);
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_main_quit),
                          GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"About LAMPS");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);

VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox1,FALSE,FALSE,0); 
gtk_widget_set_usize(GTK_WIDGET(VBox1),300,88);
Label=gtk_label_new("The LAMPS program"); gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
Label=gtk_label_new("A.Chatterjee, Sudheer Singh, K.Ramachandran"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,BlueStyle);
Label=gtk_label_new("Nuclear Physics Division"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
Label=gtk_label_new("Bhabha Atomic Research Centre"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
Label=gtk_label_new("Mumbai 400 005, Indiia"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
Label=gtk_label_new("Email: ambar@tifr.res.in"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);

VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox1,FALSE,FALSE,2);
gtk_widget_set_usize(GTK_WIDGET(VBox1),300,74);
HSep=gtk_hseparator_new(); gtk_box_pack_start(GTK_BOX(VBox1),HSep,TRUE,FALSE,2);
Label=gtk_label_new("If the help files have been installed in"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,2);
SetStyleRecursively(Label,BlueStyle);
Label=gtk_label_new("/home/lamps/help"); gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,BlueStyle);
Label=gtk_label_new("You can click Help"); gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,BlueStyle);
Label=gtk_label_new("Otherwise see: http://www.tifr.res.in/~pell/lamps.html"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
HSep=gtk_hseparator_new(); gtk_box_pack_start(GTK_BOX(VBox1),HSep,TRUE,FALSE,2);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Help"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DoHelp),NULL); 
But=gtk_button_new_with_label("Exit"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_main_quit),NULL); 

gtk_widget_show_all(Win);
gtk_style_unref(RedStyle); gtk_style_unref(GreenStyle); gtk_style_unref(BlueStyle);
gtk_main();
return(0);
}
/*-----------------------------------------------------------------------------------------------*/
