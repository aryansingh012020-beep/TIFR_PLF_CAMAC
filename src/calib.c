#define GTK_ENABLE_BROKEN
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lamps.h"

/*External Globals*/
extern struct Calibration Calibration[MAX_TOTAL_PAR];
extern struct Setup Setup;
extern struct FileSelectType *FileX;
extern gchar CalDir[MAX_DIR_STRLEN];                                                             //Directory prefs
/*Function Templates*/
void ReadCalib(GtkWidget *W,gpointer Data); void SaveCalib(GtkWidget *W,gpointer Data);
void EditCalib(GtkWidget *W,gpointer Data);
void NoCalib(GtkWidget *W,gpointer Unused);
void FNameCalibDialog(gint Action); void WinClosed(GtkWidget *W,GtkWidget *Win);
void Attention(gint XPos,gchar *Messg);
void ParseTextToStr(gchar *TextBuf,gint From,gchar *OutText,gint *ToHere);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void SavePrefs(void);
void ReadCalibrationFile(gchar *FName,gboolean DspErr);
void SaveCalibrationFile(gchar *FName);
/*Globals*/
GtkWidget *CalibText;
/*----------------------------------------------------------------------------------------------------------------*/
void NoCalib(GtkWidget *W,gpointer Unused)
//Calibration as ADC Channel Number
{
gint i;

for (i=0;i<MAX_TOTAL_PAR;++i)
   {
   sprintf(Calibration[i].Units,"adc");
   Calibration[i].P[0]=0.0; Calibration[i].P[1]=1.0;
   Calibration[i].P[2]=0.0; Calibration[i].P[3]=0.0;
   }
}
/*----------------------------------------------------------------------------------------------------------------*/
void ReadCalibrationFile(gchar *FName,gboolean DspErr)
{
FILE *Fp;
gchar Str0[20],Str1[20],Str2[20],Str3[20],Skip[80];
gint i,j;

if ((Fp=fopen(FName,"r"))==NULL) { if (DspErr) Attention(0,"Could not read calibration file"); return; }
for (i=0;i<4;++i) fgets(Skip,79,Fp);                                                                  //Skip 4 lines
for (i=0;i<MAX_TOTAL_PAR;++i)
   {
   if (fscanf(Fp,"%d %s %s %s %s %s",&j,Str0,Str1,Str2,Str3,Calibration[i].Units)==EOF) break;
   Calibration[i].P[0]=atof(Str0); Calibration[i].P[1]=atof(Str1);
   Calibration[i].P[2]=atof(Str2); Calibration[i].P[3]=atof(Str3);
   }
fclose(Fp);
}
/*----------------------------------------------------------------------------------------------------------------*/
void ReadC(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(CalDir,FileX->Path); SavePrefs();                                                           //Preserve path
g_free(FileX);
ReadCalibrationFile(FName,TRUE);
SaveCalibrationFile(".lamps_cal");                                                 //File read when lamps re-starts
}
/*----------------------------------------------------------------------------------------------------------------*/
void SaveCalibrationFile(gchar *FName)
{
FILE *Fp;
gint i;

if ((Fp=fopen(FName,"w"))==NULL) { Attention(0,"Could not write calibration file"); return; }
fprintf(Fp,"LAMPS Calibration File\n");
fprintf(Fp,"E=P[0]+P[1]x+P[2]x**2+P[3]*sqrt(x)\n\n");
fprintf(Fp," Para     Coeff P[0]   Coeff P[1]    Coeff P[2]    Coeff P[3]  Units\n");
for (i=0;i<(Setup.Parameter.NPar+Setup.Pseudo.N);i++)
    fprintf(Fp,"%5d  %+12.5e  %+12.5e  %+12.5e  %+12.5e  %s\n",
            i+1,Calibration[i].P[0],Calibration[i].P[1],Calibration[i].P[2],
            Calibration[i].P[3],Calibration[i].Units);
fclose(Fp);
}
/*----------------------------------------------------------------------------------------------------------------*/
void SaveC(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(CalDir,FileX->Path); SavePrefs();                                                             //Preserve path
g_free(FileX);
SaveCalibrationFile(FName);
}
/*----------------------------------------------------------------------------------------------------------------*/
void ReadCalib(GtkWidget *W,gpointer Data)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Read Calibration File",NULL,300,TOP_SPACE,TRUE,CalDir,".cal",FALSE,&ReadC,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------*/
void SaveCalib(GtkWidget *W,gpointer Data)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Save Calibration File",NULL,300,TOP_SPACE,FALSE,CalDir,".cal",FALSE,&SaveC,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------*/
void EditCalibDone(GtkWidget *W,GtkWidget *Win)
{
gchar *TextBuf,Str[LONG_TEXT_FIELD];
gint i,From,ToHere;

TextBuf=gtk_editable_get_chars(GTK_EDITABLE(CalibText),0,-1);
for (i=0,From=0;i<Setup.Parameter.NPar+Setup.Pseudo.N;i++)
    {
    ParseTextToStr(TextBuf,From,Str,&ToHere); From=ToHere;        //First is Parameter Number: skip
    ParseTextToStr(TextBuf,From,Str,&ToHere); From=ToHere; 
    Calibration[i].P[0]=atof(Str);                                        //Second quantity is P[0]
    ParseTextToStr(TextBuf,From,Str,&ToHere); From=ToHere; 
    Calibration[i].P[1]=atof(Str);                                         //Third quantity is P[1]
    ParseTextToStr(TextBuf,From,Str,&ToHere); From=ToHere; 
    Calibration[i].P[2]=atof(Str);                                        //Fourth quantity is P[2]
    ParseTextToStr(TextBuf,From,Str,&ToHere); From=ToHere; 
    Calibration[i].P[3]=atof(Str);                                         //Fifth quantity is P[3]
    ParseTextToStr(TextBuf,From,Calibration[i].Units,&ToHere); From=ToHere;        //Sixth is Units
    }
g_free(TextBuf);
gtk_grab_remove(GTK_WIDGET(Win));                                                      //Take away the modal property
gtk_widget_destroy(GTK_WIDGET(Win));                                                                 //Destroy window
}
/*----------------------------------------------------------------------------------------------------------------*/
gint DeleteWin(GtkWidget *W,GdkEvent *Event,gpointer Data)  
{ return TRUE; }                                                 //Prevents closing a window from the window manager
/*----------------------------------------------------------------------------------------------------------------*/
void EditCalib(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*HBox,*VBox,*But,*SBar,*Label,*VBox2;
static GdkColor Red   = {0,0xffff,0x0000,0x0000};
gint i;
gchar Str[120];

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_grab_add(Win);                                  //Define a modal window
gtk_signal_connect(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(DeleteWin),NULL); //Ensure you cant exit this way
gtk_window_set_title(GTK_WINDOW(Win),"Edit Calibration");
gtk_widget_set_uposition(GTK_WIDGET(Win),300,51);
gtk_window_set_policy(GTK_WINDOW(Win),FALSE,FALSE,TRUE);                                     //Dont allow re-sizing
gtk_container_set_border_width(GTK_CONTAINER(Win),5);                                              //Breathing room

VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);                    //VBox for entire window

VBox2=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),VBox2);
Label=gtk_label_new("LAMPS Calibration File");
gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
Label=gtk_label_new("E=P[0]+P[1]x+P[2]x**2+P[3]*sqrt(x)");
gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
Label=gtk_label_new("This is a free format file"); 
gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox2),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Para"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label(" Coeff P[0] ");
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label(" Coeff P[1] ");
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Coeff P[2]");
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label(" Coeff P[3] ");
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Units"); 
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,0);
gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);                          //Scrollable text box
CalibText=gtk_text_new(NULL,NULL);
gtk_text_set_editable(GTK_TEXT(CalibText),TRUE); gtk_widget_set_usize(GTK_WIDGET(CalibText),460,120);
for (i=0;i<Setup.Parameter.NPar+Setup.Pseudo.N;i++)
    {
    sprintf(Str,"%5d  %+12.5e  %+12.5e  %+12.5e  %+12.5e  %s\n",i+1,Calibration[i].P[0],
            Calibration[i].P[1],Calibration[i].P[2],Calibration[i].P[3],Calibration[i].Units);
    gtk_text_insert(GTK_TEXT(CalibText),NULL,&Red,NULL,Str,-1);
    }
gtk_box_pack_start(GTK_BOX(HBox),CalibText,FALSE,FALSE,0);
SBar=gtk_vscrollbar_new(GTK_TEXT(CalibText)->vadj);
gtk_box_pack_start(GTK_BOX(HBox),SBar,FALSE,FALSE,0);

VBox2=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),VBox2);
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox2),HBox,FALSE,FALSE,0);
But=gtk_button_new_from_stock(GTK_STOCK_OK); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(EditCalibDone),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*-----------------------------------------------------------------------------------------------*/
