#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "lamps.h"

/* Global variables */
extern struct Setup Setup;
extern gulong ScalerCurr[MAX_SCALER],ScalerPrev[MAX_SCALER];           //Scaler values from the current and previous buffers
extern gint ScalerWinOpen;
extern GtkWidget **ScalerTotal,**ScalerRate,**ScalerDRate;
/* Function Templates */
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void Attention(gint XPos,gchar *Messg);
/*-----------------------------------------------------------------------------------------------------------------------*/
void ScalerWinDestroyed(GtkWidget *W,gpointer Unused)
{
ScalerWinOpen=0;
g_free(ScalerTotal); g_free(ScalerRate); g_free(ScalerDRate);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Scalers(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*VBox,*Scrl,*Table,*HBox,*But;
static GdkColor HeadingBg  = {0,0x0000,0x0000,0x0000};
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};
static gchar Heading[5][25] = {"No","Name","Total","Rate (Hz)","Diff. Rate"};
gint ColWidth[5]={24,120,120,80,80};
gchar Str[40];
GtkStyle *HeadingStyle;
gint i;

if (ScalerWinOpen) return;
if (!Setup.Scaler.NSc) { Attention(0,"No scalers defined"); return; }
HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }

ScalerWinOpen=1; ScalerTotal=g_new(GtkWidget *,Setup.Scaler.NSc); 
ScalerRate=g_new(GtkWidget *,Setup.Scaler.NSc); ScalerDRate=g_new(GtkWidget *,Setup.Scaler.NSc);

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(ScalerWinDestroyed),NULL);
gtk_window_set_title(GTK_WINDOW(Win),"Scalers"); gtk_widget_set_uposition(GTK_WIDGET(Win),275,TOP_SPACE);
gtk_widget_set_size_request(GTK_WIDGET(Win),460,435); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                                     

Scrl=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Scrl),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(GTK_WIDGET(Scrl),380,27);
gtk_box_pack_start(GTK_BOX(VBox),Scrl,FALSE,FALSE,0);

Table=gtk_table_new(1,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Scrl),Table);
for (i=0;i<5;++i)
    {
    But=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(But,HeadingStyle);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

Scrl=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Scrl),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(GTK_WIDGET(Scrl),380,360);
gtk_box_pack_start(GTK_BOX(VBox),Scrl,FALSE,FALSE,0);

Table=gtk_table_new(Setup.Scaler.NSc,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Scrl),Table);
for (i=0;i<Setup.Scaler.NSc;++i)
    {
    sprintf(Str,"%d",i+1); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    But=gtk_button_new_with_label(Setup.Scaler.Name[i]);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(Table),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    sprintf(Str,"%ld",ScalerCurr[i]); ScalerTotal[i]=gtk_button_new_with_label(Str);
    gtk_label_set_justify(GTK_LABEL(GTK_BIN(ScalerTotal[i])->child),GTK_JUSTIFY_RIGHT);
    gtk_widget_set_size_request(GTK_WIDGET(ScalerTotal[i]),ColWidth[2],-1);
    gtk_table_attach(GTK_TABLE(Table),ScalerTotal[i],2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ScalerRate[i]=gtk_button_new_with_label("Pl. wait");
    gtk_widget_set_size_request(GTK_WIDGET(ScalerRate[i]),ColWidth[3],-1);
    gtk_table_attach(GTK_TABLE(Table),ScalerRate[i],3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ScalerDRate[i]=gtk_button_new_with_label("Pl. wait");
    gtk_widget_set_size_request(GTK_WIDGET(ScalerDRate[i]),ColWidth[4],-1);
    gtk_table_attach(GTK_TABLE(Table),ScalerDRate[i],4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_from_stock(GTK_STOCK_OK); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
gtk_style_unref(HeadingStyle);
}
/*------------------------------------------------------------------------------------------------------------------------*/
