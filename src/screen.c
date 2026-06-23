#include <gtk/gtk.h>
#include "lamps.h"

//screen.c:: This file contains the screen related callbacks from main.c

/*External Global Variables*/
extern gint Screen,ThemeChanged;
extern gint ScreenSpecType[SCREEN_TOT][MAX_SCREENS];
extern struct Setup Setup;
extern struct WinProperties Prop[SCREEN_TOT];
extern gint TopOfset;                                           //Accounts for space occupied by window manager at the top

/*Function templates*/
void CloseAllSpecWindows(GtkWidget *W,gpointer Data);
void Plot1(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH);
void Plot2(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH);
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void DisplayScreen(GtkWidget *W,GtkWidget *Win)
{
gint NScreen,WinNo,WindowX,WindowY,WindowW,WindowH;

if (Win) gtk_widget_destroy(Win);                                                      //Destroys the user interaction window of ScreenN
NScreen=1+(Setup.Oned.N+Setup.Twod.N-1)/SCREEN_TOT;
Screen=CLAMP(Screen,0,NScreen-1);
CloseAllSpecWindows(NULL,NULL);                                                                            //Get rid of existing windows
/*Now we put up all the windows for the current screen. Note that ScreenSpecType=0 means that there is no window*/
WindowW=CANVAS_MIN_WIDTH; WindowH=CANVAS_MIN_HEIGHT; WindowX=0; WindowY=TOP_SPACE+TopOfset; ThemeChanged=TRUE;
for (WinNo=0;WinNo<SCREEN_TOT;WinNo++)
    {
    Prop[WinNo].X=WindowX; Prop[WinNo].Y=WindowY; Prop[WinNo].W=WindowW; Prop[WinNo].H=WindowH;
    switch (ScreenSpecType[WinNo][Screen])
       {
       case 1: Plot1(WinNo,WindowX,WindowY,WindowW,WindowH); break;
       case 2: case 3: case 4: Plot2(WinNo,WindowX,WindowY,WindowW,WindowH);                    //In case of projections we go back to 2d
       }
    WindowX+=WindowW+X_BORDER; if (!((WinNo+1)%SCREEN_COLS)) { WindowX=0; WindowY+=WindowH+Y_BORDER; }
    }
}
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void Screen1(GtkWidget *W,gpointer Data)
{ Screen=0; DisplayScreen(NULL,NULL); }
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void Screen2(GtkWidget *W,gpointer Data)
{ Screen=1; DisplayScreen(NULL,NULL); }
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void Screen3(GtkWidget *W,gpointer Data)
{ Screen=2; DisplayScreen(NULL,NULL); }
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void Screen4(GtkWidget *W,gpointer Data)
{ Screen=3; DisplayScreen(NULL,NULL); }
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void NChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Screen=gtk_spin_button_get_value_as_int(Spin)-1; }
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void ScreenN(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*HBox,*NSpin,*Label,*But;
GtkAdjustment *Adj;
gint NScreen;

NScreen=1+(Setup.Oned.N+Setup.Twod.N-1)/SCREEN_TOT;
Win=gtk_dialog_new(); gtk_grab_add(Win);
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Screen N");
gtk_widget_set_uposition(GTK_WIDGET(Win),415,280); gtk_widget_set_usize(GTK_WIDGET(Win),300,100);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,TRUE,FALSE,5);
Label=gtk_label_new("Screen Number"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(Screen+1,1,NScreen,1,5,0);
NSpin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(NSpin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(NSpin),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),NSpin,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(NChanged),(gpointer)NSpin);

But=gtk_button_new_with_label("Ok");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DisplayScreen),Win);
gtk_widget_show_all(Win);
}
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void NextScreen(GtkWidget *W,gpointer Data)
{
gint NScreen;

NScreen=1+(Setup.Oned.N+Setup.Twod.N-1)/SCREEN_TOT;
Screen=MIN(NScreen-1,Screen+1); DisplayScreen(NULL,NULL);
}
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void PrevScreen(GtkWidget *W,gpointer Data)
{ Screen=MAX(0,Screen-1); DisplayScreen(NULL,NULL); }
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void LastScreen(GtkWidget *W,gpointer Data)
{
gint NScreen;

NScreen=1+(Setup.Oned.N+Setup.Twod.N-1)/SCREEN_TOT;
Screen=NScreen-1; DisplayScreen(NULL,NULL);
}
/*--------------------------------------------------------------------------------------------------------------------------------------*/

