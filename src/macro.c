#define GTK_ENABLE_BROKEN
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include "lamps.h"

/*Function Templates*/
void Attention(gint XPos,gchar *Messg);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void WinClosed(GtkWidget *W,GtkWidget *Win);

/*External Global variables*/
extern struct Setup Setup;
extern enum ProgramState ProgramState;
extern GtkWidget *EdWin;                   //This is global only in setup.c. Modal windows transient with respect to this
extern gint Off1d[MAX_1D];
extern guint16 *Oned16;
extern guint32 *Oned32;
extern gulong ScalerCurr[MAX_SCALER];                                             //Scaler values from the current buffer
extern gfloat MacroCurr[MAX_MACRO],MacroPrev[MAX_MACRO],MacroDiff[MAX_MACRO];                 //Computed values of macros
extern gboolean MacroOutBox;                                                   //Availability of window to display macros

/*Global variables in this file only*/
gint MacroNo;                                                                                       //Current macro number
GtkWidget **OutBox;                                                                           //Text box for macro display
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroNameChanged(GtkWidget *W,gpointer Data)
{ strcpy(Setup.Macro.Name[GPOINTER_TO_INT(Data)],gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroTypeChanged(GtkWidget *W,gpointer Data)
{
gint i;
const gchar *Str;
                                                                                                                             
i=GPOINTER_TO_INT(Data);
Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Area"))       { Setup.Macro.Type[i]=0; return; }
if (!strcmp(Str,"Integral"))   { Setup.Macro.Type[i]=1; return; }
if (!strcmp(Str,"Rectangle"))  { Setup.Macro.Type[i]=2; return; }
if (!strcmp(Str,"Banana"))     { Setup.Macro.Type[i]=3; return; }
if (!strcmp(Str,"Ratio"))      { Setup.Macro.Type[i]=4; return; }
if (!strcmp(Str,"Product"))    { Setup.Macro.Type[i]=5; return; }
if (!strcmp(Str,"Sum"))        { Setup.Macro.Type[i]=6; return; }
if (!strcmp(Str,"Sqrt"))       { Setup.Macro.Type[i]=7; return; }
if (!strcmp(Str,"Scaler"))     { Setup.Macro.Type[i]=8; return; }
if (!strcmp(Str,"User"))       { Setup.Macro.Type[i]=9; return; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroDspChanged(GtkWidget *W,gpointer Data)
{
gint i;
const gchar *Str;
                                                                                                                             
i=GPOINTER_TO_INT(Data);
Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Yes")) Setup.Macro.Display[i]=TRUE; else Setup.Macro.Display[i]=FALSE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroSpecNoChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.SpecNo[MacroNo]=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroXMinChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.XMin[MacroNo]=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroXMaxChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.XMax[MacroNo]=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroArea(i)
{
GtkWidget *Win,*Label,*Spin,*VBox,*HBox,*But;
GtkAdjustment *Adj;
gchar Str[80];

MacroNo=i;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Macro"); gtk_widget_set_uposition(GTK_WIDGET(Win),462,60);
gtk_widget_set_usize(GTK_WIDGET(Win),300,140);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Define Macro No: %d\nMacro Type: Area",i+1); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("1d Spec No:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.SpecNo[i],1.0,(gfloat)Setup.Oned.N,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroSpecNoChanged),(gpointer)Spin);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("From Channel:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.XMin[i],0.0,16384.0,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroXMinChanged),(gpointer)Spin);

Label=gtk_label_new("To Channel:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.XMax[i],0.0,16384.0,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroXMaxChanged),(gpointer)Spin);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroIntegral(i)
{
GtkWidget *Win,*Label,*Spin,*VBox,*HBox,*But;
GtkAdjustment *Adj;
gchar Str[80];

MacroNo=i;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Macro"); gtk_widget_set_uposition(GTK_WIDGET(Win),462,60);
gtk_widget_set_usize(GTK_WIDGET(Win),300,140);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Define Macro No: %d\nMacro Type: Integral",i+1); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("1d Spec No:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.SpecNo[i],1.0,(gfloat)Setup.Oned.N,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroSpecNoChanged),(gpointer)Spin);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("From Channel:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.XMin[i],0.0,16384.0,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroXMinChanged),(gpointer)Spin);

Label=gtk_label_new("To Channel:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.XMax[i],0.0,16384.0,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroXMaxChanged),(gpointer)Spin);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroRectangle(i)
{
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroBanana(i)
{
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroK1Changed(GtkWidget *W,gpointer Data)
{
gint i;
                                                                                                                             
i=GPOINTER_TO_INT(Data);
Setup.Macro.K1[i]=atof(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroK2Changed(GtkWidget *W,gpointer Data)
{
gint i;
                                                                                                                             
i=GPOINTER_TO_INT(Data);
Setup.Macro.K2[i]=atof(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroIndex1Changed(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.Index1[MacroNo]=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroIndex2Changed(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.Index2[MacroNo]=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroScalerNoChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.ScalerNo[MacroNo]=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroRatio(i)
{
GtkWidget *Win,*Label,*Spin,*VBox,*HBox,*But,*Entry;
GtkAdjustment *Adj;
gchar Str[80];

MacroNo=i;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Macro"); gtk_widget_set_uposition(GTK_WIDGET(Win),462,60);
gtk_widget_set_usize(GTK_WIDGET(Win),320,160);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Define Macro No: %d\nMacro Type: Ratio",i+1); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Ratio=K*(Macro1/Macro2)");
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Constant Multiplier, K"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(Entry,160,22);
sprintf(Str,"%g",Setup.Macro.K1[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(MacroK1Changed),GINT_TO_POINTER(i));
gtk_box_pack_start(GTK_BOX(HBox),Entry,TRUE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No. for Macro1:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.Index1[i],1.0,(gfloat)MAX_MACRO,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroIndex1Changed),(gpointer)Spin);

Label=gtk_label_new("No. for Macro2:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.Index2[i],1.0,(gfloat)MAX_MACRO,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroIndex2Changed),(gpointer)Spin);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroProduct(i)
{
GtkWidget *Win,*Label,*Spin,*VBox,*HBox,*But,*Entry;
GtkAdjustment *Adj;
gchar Str[80];

MacroNo=i;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Macro"); gtk_widget_set_uposition(GTK_WIDGET(Win),462,60);
gtk_widget_set_usize(GTK_WIDGET(Win),320,160);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Define Macro No: %d\nMacro Type: Product",i+1); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Product=K*Macro1*Macro2");
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Constant Multiplier, K"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(Entry,160,22);
sprintf(Str,"%g",Setup.Macro.K1[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(MacroK1Changed),GINT_TO_POINTER(i));
gtk_box_pack_start(GTK_BOX(HBox),Entry,TRUE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No. for Macro1:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.Index1[i],1.0,(gfloat)MAX_MACRO,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroIndex1Changed),(gpointer)Spin);

Label=gtk_label_new("No. for Macro2:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.Index2[i],1.0,(gfloat)MAX_MACRO,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroIndex2Changed),(gpointer)Spin);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroSum(i)
{
GtkWidget *Win,*Label,*Spin,*VBox,*HBox,*But,*Entry;
GtkAdjustment *Adj;
gchar Str[80];

MacroNo=i;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Macro"); gtk_widget_set_uposition(GTK_WIDGET(Win),462,60);
gtk_widget_set_usize(GTK_WIDGET(Win),320,160);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Define Macro No: %d\nMacro Type: Sum",i+1); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Sum=K*(Macro1+Macro2)");
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Constant Multiplier, K"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(Entry,160,22);
sprintf(Str,"%g",Setup.Macro.K1[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(MacroK1Changed),GINT_TO_POINTER(i));
gtk_box_pack_start(GTK_BOX(HBox),Entry,TRUE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No. for Macro1:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.Index1[i],1.0,(gfloat)MAX_MACRO,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroIndex1Changed),(gpointer)Spin);

Label=gtk_label_new("No. for Macro2:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.Index2[i],1.0,(gfloat)MAX_MACRO,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroIndex2Changed),(gpointer)Spin);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroSqrt(i)
{
GtkWidget *Win,*Label,*Spin,*VBox,*HBox,*But,*Entry;
GtkAdjustment *Adj;
gchar Str[80];

MacroNo=i;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Macro"); gtk_widget_set_uposition(GTK_WIDGET(Win),462,60);
gtk_widget_set_usize(GTK_WIDGET(Win),320,160);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Define Macro No: %d\nMacro Type: Sqrt",i+1); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Sqrt=K*sqrt(Macro1)");
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Constant Multiplier, K"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(Entry,160,22);
sprintf(Str,"%g",Setup.Macro.K1[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(MacroK1Changed),GINT_TO_POINTER(i));
gtk_box_pack_start(GTK_BOX(HBox),Entry,TRUE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No. for Macro1:"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.Index1[i],1.0,(gfloat)MAX_MACRO,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroIndex1Changed),(gpointer)Spin);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroScaler(i)
{
GtkWidget *Win,*Label,*Spin,*VBox,*HBox,*But;
GtkAdjustment *Adj;
gchar Str[80];

MacroNo=i;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Macro"); gtk_widget_set_uposition(GTK_WIDGET(Win),462,60);
gtk_widget_set_usize(GTK_WIDGET(Win),320,120);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Define Macro No: %d\nMacro Type: Scaler",i+1); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Scaler Number"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.ScalerNo[i],1.0,(gfloat)MAX_SCALER,1.0,5.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),FALSE);
gtk_widget_set_usize(Spin,60,22); gtk_box_pack_start(GTK_BOX(HBox),Spin,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroScalerNoChanged),(gpointer)Spin);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacroUser(i)
{
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineMacro(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);

switch (Setup.Macro.Type[i])
   {
   case MacroArea:      DefineMacroArea(i);      break;
   case MacroIntegral:  DefineMacroIntegral(i);  break;
   case MacroRectangle: DefineMacroRectangle(i); break;
   case MacroBanana:    DefineMacroBanana(i);    break;
   case MacroRatio:     DefineMacroRatio(i);     break;
   case MacroProduct:   DefineMacroProduct(i);   break;
   case MacroSum:       DefineMacroSum(i);       break;
   case MacroSqrt:      DefineMacroSqrt(i);      break;
   case MacroScaler:    DefineMacroScaler(i);    break;
   case MacroUser:      DefineMacroUser(i);   
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroRefreshRateChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.RefreshRate=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void SetMacro(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*Table,*ScrollW,*VBox,*Label,*But,*HBox,*Entry,*Combo,*Spin;
GtkAdjustment *Adj;
static gchar Heading[5][25]= {"Macro No","Name","Type","Details","Display"};
static GdkColor HeadingBg  = {0,0x0000,0xA000,0xA000};
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor LabelFg    = {0,0xE000,0x0000,0x0000};
GtkStyle *HeadingStyle,*LabelStyle;
gint i,j;
gchar Str[80];
GList *GList;
static gchar MacroName[10][15]={"Area","Integral","Rectangle","Banana","Ratio","Product","Sum",
                                "Sqrt","Scaler","User"};

if (!Setup.Macro.N) return; 
HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
LabelStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++)
    {
    HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg;
    LabelStyle->fg[i]=LabelStyle->text[i]=LabelFg;
    }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Set Macros"); gtk_widget_set_uposition(GTK_WIDGET(Win),0,60);
gtk_widget_set_size_request(GTK_WIDGET(Win),-1,400);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
Label=gtk_label_new("Enter Macro definitions");
SetStyleRecursively(Label,LabelStyle);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
ScrollW=gtk_scrolled_window_new(NULL,NULL); gtk_container_set_border_width(GTK_CONTAINER(ScrollW),5);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);
Table=gtk_table_new(Setup.Macro.N+1,5,FALSE);
for (i=0;i<5;i++)
    {
    But=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(But,HeadingStyle);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }
for (i=0;i<Setup.Macro.N;i++)
    {
    sprintf(Str,"%2d",i+1); But=gtk_button_new_with_label(Str);
    gtk_table_attach(GTK_TABLE(Table),But,0,1,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,160,-1);
    sprintf(Str,"%s",Setup.Macro.Name[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(MacroNameChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),Entry,1,2,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    Combo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Combo),100,-1);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),FALSE);
    GList=NULL;
    for (j=0;j<10;++j) GList=g_list_append(GList,MacroName[j]);
    gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
    strcpy(Str,MacroName[Setup.Macro.Type[i]]);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(MacroTypeChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),Combo,2,3,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    But=gtk_button_new_with_label("Define");
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DefineMacro),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),But,3,4,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    Combo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Combo),40,-1);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),FALSE);
    GList=NULL;
    GList=g_list_append(GList,"Yes"); GList=g_list_append(GList,"No");
    gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
    if (Setup.Macro.Display[i]) strcpy(Str,"Yes"); else strcpy(Str,"No");
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(MacroDspChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),Combo,4,5,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);
    }

gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Macro Refresh Rate");
SetStyleRecursively(Label,LabelStyle);
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Macro.RefreshRate,1.0,8192.0,31.0,255.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_widget_set_size_request(GTK_WIDGET(Spin),60,-1);
gtk_box_pack_start(GTK_BOX(HBox),Spin,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroRefreshRateChanged),Spin);
Label=gtk_label_new("buffers");
SetStyleRecursively(Label,LabelStyle);
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win); g_list_free(GList);
gtk_style_unref(HeadingStyle); gtk_style_unref(LabelStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroNChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Macro.N=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroSetup(GtkWidget *VBox)
{
GtkWidget *HBox,*Spin,*Label,*But,*Frame,*VBox0;
GtkAdjustment *Adj;
static GdkColor FrameBg  = {0,0x7777,0x7777,0x7777};
static GdkColor FrameFg  = {0,0xDDDD,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;
                                                                                                                             
FrameStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }
                                                                                                                             
Frame=gtk_frame_new("MACRO SETTINGS"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.5);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox0=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Frame),VBox0);
gtk_container_set_border_width(GTK_CONTAINER(VBox0),5);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox0),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Number of Macros"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
Adj=(GtkAdjustment *)gtk_adjustment_new(Setup.Macro.N,0,MAX_MACRO,1,5,0);
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),Spin,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(MacroNChanged),(gpointer)Spin);
But=gtk_button_new_with_label("Define"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SetMacro),NULL);
                                                                                                                             
gtk_style_unref(FrameStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CloseMacroDisplay(GtkWidget *W,gpointer Data)
{ g_free(OutBox); MacroOutBox=FALSE; }
/*----------------------------------------------------------------------------------------------------------------------*/
void Macros(GtkWidget *W,gpointer Data)                                                          //Displays defined macros
{
static GdkColor Red   =  {0,0xffff,0x0000,0x0000};
static GdkColor Green =  {0,0x0000,0x7777,0x0000};
static GdkColor Black =  {0,0x0000,0x0000,0x0000};
GtkWidget *Win,*HBox,*VBox,*SBar,*Header;
gint i;

if (MacroOutBox) return;                                                                       //Macro output already open
if (!Setup.Macro.N) { Attention(0,"No macros defined"); return; }

OutBox=g_new(GtkWidget *,1);                                             //Allocate memory for widget, must be freed later
MacroOutBox=TRUE;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(CloseMacroDisplay),NULL);
gtk_window_set_title(GTK_WINDOW(Win),"Macro Display"); gtk_widget_set_uposition(GTK_WIDGET(Win),100,TOP_SPACE);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
Header=gtk_text_new(NULL,NULL); gtk_widget_set_size_request(GTK_WIDGET(Header),800,60);
gtk_text_set_editable(GTK_TEXT(Header),FALSE); gtk_text_set_word_wrap(GTK_TEXT(Header),FALSE);
gtk_box_pack_start(GTK_BOX(VBox),Header,FALSE,FALSE,0);
gtk_text_insert(GTK_TEXT(Header),NULL,&Red,NULL,"The columns are as follows.",-1);
gtk_text_insert(GTK_TEXT(Header),NULL,&Green,NULL," Differential values D() are in green\n",-1);
gtk_text_insert(GTK_TEXT(Header),NULL,&Black,NULL,"Buffer No, ",-1);
for (i=0;i<Setup.Macro.N;++i) 
    {
    gtk_text_insert(GTK_TEXT(Header),NULL,&Red,NULL,Setup.Macro.Name[i],-1);
    gtk_text_insert(GTK_TEXT(Header),NULL,&Green,NULL,", D(",-1);
    gtk_text_insert(GTK_TEXT(Header),NULL,&Green,NULL,Setup.Macro.Name[i],-1);
    if (i<Setup.Macro.N-1) gtk_text_insert(GTK_TEXT(Header),NULL,&Green,NULL,"), ",-1);
    else                   gtk_text_insert(GTK_TEXT(Header),NULL,&Green,NULL,").",-1);
    }
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
OutBox[0]=gtk_text_new(NULL,NULL); gtk_widget_set_size_request(GTK_WIDGET(OutBox[0]),800,400);
gtk_text_set_editable(GTK_TEXT(OutBox[0]),FALSE); gtk_text_set_word_wrap(GTK_TEXT(OutBox[0]),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),OutBox[0],FALSE,FALSE,0);
SBar=gtk_vscrollbar_new(GTK_TEXT(OutBox[0])->vadj); gtk_box_pack_start(GTK_BOX(HBox),SBar,FALSE,FALSE,0);

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroGetArea(gint MNo)                                                    //Very similar to ComputeArea in plot1.c
{
guint32 LSum,RSum,Counts;
gint SNo,Ch,i;
gfloat LBkg,RBkg,SlopeBkg,Bkg;
                                                                                                                             
SNo=Setup.Macro.SpecNo[MNo]-1;
for (i=-1,LSum=0;i<2;i++)
    {
    Ch=MAX(Setup.Macro.XMin[MNo]+i,0);
    LSum+=Setup.Oned.WSz==1 ? (guint32)Oned16[Off1d[SNo]+Ch] : (guint32)Oned32[Off1d[SNo]+Ch];
    }
for (i=-1,RSum=0;i<2;i++)
    {
    Ch=MIN(Setup.Macro.XMax[MNo]+i,Setup.Oned.Chan[SNo]-1);
    RSum+=Setup.Oned.WSz==1 ? (guint32)Oned16[Off1d[SNo]+Ch] : (guint32)Oned32[Off1d[SNo]+Ch];
    }
LBkg=(gfloat)LSum/3.0; RBkg=(gfloat)RSum/3.0;
if (Setup.Macro.XMax[MNo]>Setup.Macro.XMin[MNo])
    SlopeBkg=(RBkg-LBkg)/(gdouble)(Setup.Macro.XMax[MNo]-Setup.Macro.XMin[MNo]);            //A redundant precaution here! 
else SlopeBkg=0.0;
for (Ch=Setup.Macro.XMin[MNo],MacroCurr[MNo]=0.0;Ch<=Setup.Macro.XMax[MNo];++Ch)
    {
    Counts=Setup.Oned.WSz==1 ? (guint32)Oned16[Off1d[SNo]+Ch] : (guint32)Oned32[Off1d[SNo]+Ch];
    Bkg=LBkg+SlopeBkg*(gdouble)(Ch-Setup.Macro.XMin[MNo]);
    MacroCurr[MNo]+=(gfloat)Counts-Bkg;
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroGetIntegral(gint MNo)                                                //Very similar to ComputeArea in plot1.c
{
gint SNo,Ch;
                                                                                                                             
SNo=Setup.Macro.SpecNo[MNo]-1;
for (Ch=Setup.Macro.XMin[MNo],MacroCurr[MNo]=0.0;Ch<=Setup.Macro.XMax[MNo];++Ch)
    MacroCurr[MNo]+=Setup.Oned.WSz==1 ? (gfloat)Oned16[Off1d[SNo]+Ch] : (gfloat)Oned32[Off1d[SNo]+Ch];
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroGetRatio(gint MNo)
{ 
gint i1,i2;

i1=Setup.Macro.Index1[MNo]-1;
i2=Setup.Macro.Index2[MNo]-1;
if (MacroCurr[i2]>1.0e-18) MacroCurr[MNo]=Setup.Macro.K1[MNo]*MacroCurr[i1]/MacroCurr[i2];
else                       MacroCurr[MNo]=1.0e20;
if (MacroDiff[i2]>1.0e-18) MacroDiff[MNo]=Setup.Macro.K1[MNo]*MacroDiff[i1]/MacroDiff[i2];
else                       MacroDiff[MNo]=1.0e20;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroGetProduct(gint MNo)
{ 
gint i1,i2;

i1=Setup.Macro.Index1[MNo]-1;
i2=Setup.Macro.Index2[MNo]-1;
MacroCurr[MNo]=Setup.Macro.K1[MNo]*MacroCurr[i1]*MacroCurr[i2]; 
MacroDiff[MNo]=Setup.Macro.K1[MNo]*MacroDiff[i1]*MacroDiff[i2]; 
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroGetSum(gint MNo)
{
gint i1,i2;

i1=Setup.Macro.Index1[MNo]-1;
i2=Setup.Macro.Index2[MNo]-1;
MacroCurr[MNo]=Setup.Macro.K1[MNo]*MacroCurr[i1]+Setup.Macro.K2[MNo]*MacroCurr[i2];
MacroDiff[MNo]=Setup.Macro.K1[MNo]*MacroDiff[i1]+Setup.Macro.K2[MNo]*MacroDiff[i2];
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroGetSqrt(gint MNo)
{
gdouble x;

x=(gdouble)MacroCurr[Setup.Macro.Index1[MNo]-1];
if (x>0.0) MacroCurr[MNo]=Setup.Macro.K1[MNo]*sqrt(x); else MacroCurr[MNo]=0.0;
x=(gdouble)MacroDiff[Setup.Macro.Index1[MNo]-1];
if (x>0.0) MacroDiff[MNo]=Setup.Macro.K1[MNo]*sqrt(x); else MacroDiff[MNo]=0.0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MacroGetScaler(gint MNo)
{ MacroCurr[MNo]=ScalerCurr[Setup.Macro.ScalerNo[MNo]-1]; }
/*----------------------------------------------------------------------------------------------------------------------*/
void ComputeMacros(glong BuffersAcquired,FILE *MacroFp)
{
static GdkColor Red   =  {0,0xffff,0x0000,0x0000};
static GdkColor Green =  {0,0x0000,0x7777,0x0000};
static GdkColor Black =  {0,0x0000,0x0000,0x0000};
gint i;
gchar Str[80];

for (i=0;i<Setup.Macro.N;++i)
    {
    switch (Setup.Macro.Type[i])
       {
       case MacroArea:      MacroPrev[i]=MacroCurr[i]; MacroGetArea(i); 
                            MacroDiff[i]=MacroCurr[i]-MacroPrev[i]; break;               //Need prev. and diff. values also
       case MacroIntegral:  MacroPrev[i]=MacroCurr[i]; MacroGetIntegral(i); 
                            MacroDiff[i]=MacroCurr[i]-MacroPrev[i]; break;               //Need prev. and diff. values also
       case MacroRectangle: break;                                                       //Need prev. and diff. values also
       case MacroBanana:    break;                                                       //Need prev. and diff. values also
       case MacroRatio:     MacroGetRatio(i);     break;              //Do not compute prev. and diff. values for this case
       case MacroProduct:   MacroGetProduct(i);   break;              //Do not compute prev. and diff. values for this case
       case MacroSum:       MacroGetSum(i);       break;              //Do not compute prev. and diff. values for this case
       case MacroSqrt:      MacroGetSqrt(i);      break;              //Do not compute prev. and diff. values for this case
       case MacroScaler:    MacroPrev[i]=MacroCurr[i]; MacroGetScaler(i);
                            MacroDiff[i]=MacroCurr[i]-MacroPrev[i]; break;               //Need prev. and diff. values also
       case MacroUser:      break;                                                                        //Not implemented 
       }
    }
fprintf(MacroFp,"%8ld ",BuffersAcquired);
for (i=0;i<Setup.Macro.N;++i) fprintf(MacroFp,"%10.4g %10.4g",MacroCurr[i],MacroDiff[i]);
fprintf(MacroFp,"\n"); fflush(MacroFp);
if (MacroOutBox)
   {
   gdk_threads_enter();
   sprintf(Str,"%8ld",BuffersAcquired); gtk_text_insert(GTK_TEXT(OutBox[0]),NULL,&Black,NULL,Str,-1);
   for (i=0;i<Setup.Macro.N;++i) 
       {
       if (Setup.Macro.Display[i]) 
          {
          sprintf(Str,"%10.4g ",MacroCurr[i]); gtk_text_insert(GTK_TEXT(OutBox[0]),NULL,&Red  ,NULL,Str,-1);
          sprintf(Str,"%10.4g ",MacroDiff[i]); gtk_text_insert(GTK_TEXT(OutBox[0]),NULL,&Green,NULL,Str,-1);
          }
       }
   gtk_text_insert(GTK_TEXT(OutBox[0]),NULL,&Black,NULL,"\n",-1);
   gtk_text_insert(GTK_TEXT(OutBox[0]),NULL,&Black,NULL,"------------------------------------------",-1);
   gtk_text_insert(GTK_TEXT(OutBox[0]),NULL,&Black,NULL,"------------------------------------------",-1);
   gtk_text_insert(GTK_TEXT(OutBox[0]),NULL,&Black,NULL,"------------------------------------------\n",-1);
   gdk_threads_leave();
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
