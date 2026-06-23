#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "lamps.h"
//Global variables in this file
GtkWidget **DirWidget;
//External globals
extern gint TopOfset;
extern gchar SetupDir[MAX_DIR_STRLEN],ListFDir[MAX_DIR_STRLEN],BanDir[MAX_DIR_STRLEN],SpecDir[MAX_DIR_STRLEN],
       BatDir[MAX_DIR_STRLEN],CalDir[MAX_DIR_STRLEN],LogDir[MAX_DIR_STRLEN],MacroDir[MAX_DIR_STRLEN];
//Function templates
void SavePrefs(void);                                                                                        //In main.c
void Attention(gint XPos,gchar *Messg);
//----------------------------------------------------------------------------------------------------------------------
void SetPrefs(GtkWidget *W,gpointer Unused)
{
gtk_entry_set_text(GTK_ENTRY(DirWidget[0]),SetupDir);
gtk_entry_set_text(GTK_ENTRY(DirWidget[1]),ListFDir);
gtk_entry_set_text(GTK_ENTRY(DirWidget[2]),BanDir);
gtk_entry_set_text(GTK_ENTRY(DirWidget[3]),SpecDir);
gtk_entry_set_text(GTK_ENTRY(DirWidget[4]),BatDir);
gtk_entry_set_text(GTK_ENTRY(DirWidget[5]),CalDir);
gtk_entry_set_text(GTK_ENTRY(DirWidget[6]),LogDir);
gtk_entry_set_text(GTK_ENTRY(DirWidget[7]),MacroDir);
}
//----------------------------------------------------------------------------------------------------------------------
void DefaultPrefs(GtkWidget *W,gpointer Unused) 
{ 
gchar Pwd[MAX_FNAME_LENGTH-20],Str[120];

if (!getcwd(Pwd,MAX_FNAME_LENGTH-20))
   {
   sprintf(Str,"nice -n 10 crash \"Working directory has too long a path name\"&");
   system(Str); exit(1);
   }
//Set to standard directories (if they dont exist, they will be created in SaveExitPrefs)
sprintf(SetupDir,"%s/set",Pwd); sprintf(ListFDir,"%s/zls",Pwd); sprintf(BanDir,  "%s/ban",Pwd);
sprintf(SpecDir, "%s/spec",Pwd); sprintf(BatDir,  "%s/bat",Pwd); sprintf(CalDir,  "%s/cal",Pwd);
sprintf(LogDir,  "%s/log",Pwd); sprintf(MacroDir,"%s/mac",Pwd);
}
//----------------------------------------------------------------------------------------------------------------------
void AcceptPrefs()
{
strcpy(SetupDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[0])));
strcpy(ListFDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[1])));
strcpy(BanDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[2])));
strcpy(SpecDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[3])));
strcpy(BatDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[4])));
strcpy(CalDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[5])));
strcpy(LogDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[6])));
strcpy(MacroDir,gtk_entry_get_text(GTK_ENTRY(DirWidget[7])));
}
//----------------------------------------------------------------------------------------------------------------------
void PrefsWinDestroy(GtkWidget *W,gpointer Unused) 
{
struct stat StatBuf;

if (stat(".lamps_prefs",&StatBuf)) { AcceptPrefs(); SavePrefs(); Attention(0,"Directory preferences saved"); }
CreateDir();                                     //Create directories as per user preferences if they dont exist already
g_free(DirWidget);
}
//----------------------------------------------------------------------------------------------------------------------
void SaveExitPrefs(GtkWidget *W,GtkWidget *Win)
{ AcceptPrefs(); SavePrefs(); gtk_widget_destroy(Win); }
//----------------------------------------------------------------------------------------------------------------------
void CancelPrefs(GtkWidget *W,GtkWidget *Win)
{
struct stat StatBuf;

if (stat(".lamps_prefs",&StatBuf)) 
   Attention(0,"Cancel is not permitted\nPreferences must be saved!\nPlease click 'Save and Exit'"); 
else gtk_widget_destroy(Win);
}
//----------------------------------------------------------------------------------------------------------------------
gboolean DeletePrefs(GtkWidget *W,GdkEvent *Event,GtkWidget *Win)
{
struct stat StatBuf;

if (stat(".lamps_prefs",&StatBuf)) 
   {
   Attention(0,"Cancel is not permitted\nPreferences must be saved!\nPlease click 'Save and Exit'"); 
   return TRUE; 
   }
else 
   { gtk_widget_destroy(Win); return FALSE; }
}
//----------------------------------------------------------------------------------------------------------------------
void Prefs(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Table,*HBox,*VBox,*Label,*But;
gint i;
static GdkColor RedBg  = {0,0x0000,0x0000,0x0000};
static GdkColor RedFg  = {0,0x9999,0x0000,0x0000};
GtkStyle *RedStyle;

DirWidget=g_new(GtkWidget *,8);
RedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { RedStyle->fg[i]=RedStyle->text[i]=RedFg; RedStyle->bg[i]=RedBg; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
g_signal_connect(GTK_OBJECT(Win),"destroy",G_CALLBACK(PrefsWinDestroy),NULL);
g_signal_connect(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(DeletePrefs),Win);
gtk_widget_set_uposition(GTK_WIDGET(Win),10,TOP_SPACE+TopOfset+100);
gtk_window_set_title(GTK_WINDOW(Win),"LAMPS Preferences");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
Label=gtk_label_new("PLEASE SET UP YOUR DIRECTORY PREFERENCES"); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
Table=gtk_table_new(8,2,FALSE); gtk_box_pack_start(GTK_BOX(VBox),Table,FALSE,FALSE,0);

Label=gtk_label_new("Setup Files Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,0,1,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[0]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[0]),SetupDir);
gtk_widget_set_size_request(DirWidget[0],MAX_DIR_STRLEN,-1); 
gtk_table_attach(GTK_TABLE(Table),DirWidget[0],1,2,0,1,GTK_FILL,GTK_SHRINK,0,0);

Label=gtk_label_new("List Data Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,1,2,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[1]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[1]),ListFDir);
gtk_widget_set_size_request(DirWidget[1],MAX_DIR_STRLEN,-1);
gtk_table_attach(GTK_TABLE(Table),DirWidget[1],1,2,1,2,GTK_FILL,GTK_SHRINK,0,0);

Label=gtk_label_new("Banana Files Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,2,3,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[2]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[2]),BanDir);
gtk_widget_set_size_request(DirWidget[2],MAX_DIR_STRLEN,-1);
gtk_table_attach(GTK_TABLE(Table),DirWidget[2],1,2,2,3,GTK_FILL,GTK_SHRINK,0,0);

Label=gtk_label_new("Spectra Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,3,4,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[3]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[3]),SpecDir);
gtk_widget_set_size_request(DirWidget[3],MAX_DIR_STRLEN,-1);
gtk_table_attach(GTK_TABLE(Table),DirWidget[3],1,2,3,4,GTK_FILL,GTK_SHRINK,0,0);

Label=gtk_label_new("Batch Files Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,4,5,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[4]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[4]),BatDir);
gtk_widget_set_size_request(DirWidget[4],MAX_DIR_STRLEN,-1);
gtk_table_attach(GTK_TABLE(Table),DirWidget[4],1,2,4,5,GTK_FILL,GTK_SHRINK,0,0);

Label=gtk_label_new("Calibration Files Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,5,6,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[5]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[5]),CalDir);
gtk_widget_set_size_request(DirWidget[5],MAX_DIR_STRLEN,-1);
gtk_table_attach(GTK_TABLE(Table),DirWidget[5],1,2,5,6,GTK_FILL,GTK_SHRINK,0,0);

Label=gtk_label_new("Log Files Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,6,7,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[6]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[6]),LogDir);
gtk_widget_set_size_request(DirWidget[6],MAX_DIR_STRLEN,-1);
gtk_table_attach(GTK_TABLE(Table),DirWidget[6],1,2,6,7,GTK_FILL,GTK_SHRINK,0,0);

Label=gtk_label_new("Macro Files Directory:"); gtk_table_attach(GTK_TABLE(Table),Label,0,1,7,8,GTK_FILL,GTK_SHRINK,0,0);
DirWidget[7]=gtk_entry_new_with_max_length(MAX_DIR_STRLEN); gtk_entry_set_text(GTK_ENTRY(DirWidget[7]),MacroDir);
gtk_widget_set_size_request(DirWidget[7],MAX_DIR_STRLEN,-1);
gtk_table_attach(GTK_TABLE(Table),DirWidget[7],1,2,7,8,GTK_FILL,GTK_SHRINK,0,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,20);
But=gtk_button_new_with_label("Select Defaults"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(DefaultPrefs),NULL);
But=gtk_button_new_with_label("Revert"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(SetPrefs),NULL);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(CancelPrefs),Win);
But=gtk_button_new_with_label("Save and Exit"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(SaveExitPrefs),Win);
gtk_widget_show_all(Win);
gtk_style_unref(RedStyle);
}
//---------------------------------------------------------------------------------------------------------------------
