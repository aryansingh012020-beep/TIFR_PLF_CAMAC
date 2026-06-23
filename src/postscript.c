#include <gtk/gtk.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "lamps.h"

/*External global variables*/
extern guint16 *Oned16;
extern guint32 *Oned32,*Proj;
extern gint ScreenSpecNo[SCREEN_TOT][MAX_SCREENS];
extern GtkWidget *SpecWin[SCREEN_TOT];
extern gint Screen;
extern struct WinProperties Prop[SCREEN_TOT];
extern struct Setup Setup;
extern guint16 *Twod16;
extern guint32 *Twod32;
extern gint Off2d[MAX_2D];
extern glong D2PenColours[32][3];                                                               //RGB Colours for 2d plots

/*Local global variables*/
struct PSForm { 
               GtkWidget *XMinEntry; GtkWidget *XMaxEntry; GtkWidget *YMinEntry; GtkWidget *YMaxEntry; 
               GtkWidget *MaxCounts; GtkWidget *MinCounts; GtkWidget *Auto;              //These 3 fields only for 2d plots
               GtkWidget *Lin; GtkWidget *XLabel; GtkWidget *YLabel; GtkWidget *TLabel; 
               GtkWidget *XSizeEntry; GtkWidget *YSizeEntry;  GtkWidget *Landscape; GtkWidget *FileType;
               GtkWidget *FNameEntry; GtkWidget *PrintCommand; GtkWidget *OutToFile; gint WinNo;
               };
struct PSForm *PSForm;

/*Function templates*/
guint32 Get1dCounts(gint WinNo,gint Ch,gboolean Overlap,gint OvSpec);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void Attention(gint XPos,gchar *Messg);
void CreateTitle1(gchar *Str,gint WinNo);
void CreateTitle2(gchar *Title,gchar *XTitle,gchar *YTitle,gint WinNo);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);

/*------------------------------------------------------------------------------------------------------------------------*/
void MakePSFile1(gint XMin,gint XMax,gint YMin,gint YMax,gboolean Lin,const gchar *XLabel,const gchar *YLabel,
                 const gchar *TLabel,gint XSize,gint YSize,gboolean Landscape,gint FileType,
                 const gchar *FName,const gchar *PrintCommand, gboolean OutToFile)
//Based on code written by S.Mukhopadhyay (IUC-DAEF-CC)
{
gint NPts,Len,i,Ax[16384];
guint32 Ay[16384];
gfloat LeftX;
gdouble ScaleX,ScaleY,OffsetX,OffsetY,AxisStep,AxisOrigin,Temp,LTemp;
gchar Text[30],FileName[MAX_FNAME_LENGTH+5],Str[80];
FILE *Fp;

XMax=CLAMP(XMax,4,16383); XMin=CLAMP(XMin,0,XMax-4); YMax=MAX(YMax,4); YMin=CLAMP(YMin,0,YMax-4);

if (OutToFile) strcpy(FileName,FName); else strcpy(FileName,"/tmp/lamps_ps_file.ps");
if (!(Fp=fopen(FileName,"w"))) { sprintf(Str,"Open %s failed",Str); Attention(0,Str); return; }

if (FileType == 0)                                                                        //FileType=0(PS), 1(EPS), 2(PDF)
   {
   fprintf(Fp,"%%!PS-Adobe-1.0\n");                                                                         //For PS files
   if (Landscape) fprintf(Fp,"%%%%Orientation: Landscape\n"); else fprintf(Fp,"%%%%Orientation: Portrait\n");
   fprintf(Fp,"%%%%BoundingBox: 0 0 764 568\n%%%%EndComments\n%%%%EndProlog\n\n");
   }
else fprintf(Fp,"%%!PS-Adobe EPSF-3.0\n%%%%BoundingBox: 0 0 764 568\n\n");          //For EPS files, also PDF (for ps2pdf)

fprintf(Fp,"gsave\n");
fprintf(Fp,"%% /f {findfont exch scalefont setfont} bind def\n/l {lineto} bind def\n");
fprintf(Fp,"/m {newpath moveto} bind def\n/a {stroke} bind def\n/cp {closepath} bind def\n");
fprintf(Fp,"/g {gsave} bind def\n matrix currentmatrix /originmat exch def\n");
fprintf(Fp,"/umatrix {originmat matrix concatmatrix setmatrix} def\n");                   //These two lines scale the graph
fprintf(Fp,"[28.3465 0 0 28.3465 10.5 100.0] umatrix\n0.010000 setlinewidth\n");            //so all values are in cm units

NPts=XMax-XMin+1;
for (i=0;i<NPts;++i) { Ax[i]=XMin+i; Ay[i]=Get1dCounts(PSForm->WinNo,Ax[i],FALSE,0); if (!Lin && Ay[i]==0) Ay[i]=1; }

ScaleX=(gfloat)XSize/(XMax-XMin); OffsetX=2.0; OffsetY=2.0;
if (!Lin && YMin==0) YMin=1;
if (Lin) ScaleY=(gfloat)YSize/(YMax-YMin); else ScaleY=(gfloat)YSize/(log((gdouble)YMax)-log((gdouble)YMin));

if (Lin) fprintf(Fp,"%.3f %.3f m\n",ScaleX*(Ax[0]-XMin)+OffsetX,ScaleY*(Ay[0]-YMin)+OffsetY);
else     fprintf(Fp,"%.3f %.3f m\n",ScaleX*(Ax[0]-XMin)+OffsetX,ScaleY*(log((gdouble)Ay[0])-log((gdouble)YMin))+OffsetY);
for (i=1;i<NPts;++i)
    {
    fprintf(Fp,"%.3f ",(Ax[i]-XMin)*ScaleX+OffsetX);
    if (Lin) fprintf(Fp,"%.3f l\n",(Ay[i]-YMin)*ScaleY+OffsetY);
    else     fprintf(Fp,"%.3f l\n",(log((gdouble)Ay[i])-log((gdouble)YMin))*ScaleY+OffsetY);
    }
fprintf(Fp,"a\n"); fprintf(Fp,"0 setgray\n");
       
fprintf(Fp,"0.04 setlinewidth\n");                                                                //Draw the bounding box
fprintf(Fp,"%.3f ",OffsetX);
fprintf(Fp,"%.3f m\n",OffsetY);
fprintf(Fp,"%.3f ",(XMax-XMin)*ScaleX+OffsetX);
fprintf(Fp,"%.3f l\n",OffsetY);
fprintf(Fp,"%.3f ",(XMax-XMin)*ScaleX+OffsetX);
if (Lin) fprintf(Fp,"%.3f l\n",(YMax-YMin)*ScaleY+OffsetY);
else     fprintf(Fp,"%.3f l\n",(log((gdouble)YMax)-log((gdouble)YMin))*ScaleY+OffsetY);
fprintf(Fp,"%.3f ",OffsetX);
if (Lin) fprintf(Fp,"%.3f l\n",(YMax-YMin)*ScaleY+OffsetY);
else     fprintf(Fp,"%.3f l\n",(log((gdouble)YMax)-log((gdouble)YMin))*ScaleY+OffsetY);
fprintf(Fp,"cp a\n");
fprintf(Fp,"/Helvetica findfont\n");
fprintf(Fp,"0.3 scalefont setfont\n");

AxisStep=(XMax-XMin)/5.0;                                                                             //X-axis tick marks
Temp=log(AxisStep)/log(10.0); AxisStep /= pow(10.0,(floor(Temp)));
if (floor(AxisStep)==0.0) AxisStep=1.0; else if (floor(AxisStep)>6.5) AxisStep=10.0;
else if (floor(AxisStep)==6.0) AxisStep=5.0; else if (floor(AxisStep)==4.0) AxisStep=5.0;
else if (floor(AxisStep)==3.0) AxisStep=2.0; else AxisStep=floor(AxisStep);
AxisStep *= pow(10.0,(floor(Temp)));
AxisOrigin=(floor((gfloat)XMin/AxisStep)+1.0)*AxisStep;
for (Temp=AxisOrigin;Temp<=XMax;Temp+=AxisStep)
    {
    if ((fabs(Temp)<1.0e-6) && (AxisStep>1.0e-4)) Temp=0.0;
    sprintf(Text,"%g",Temp); Len=strlen(Text);
    fprintf(Fp,"%.3f ",(Temp-XMin)*ScaleX+OffsetX);
    fprintf(Fp,"%.3f m ",OffsetY);
    fprintf(Fp,"%.3f ",(Temp-XMin)*ScaleX+OffsetX);
    fprintf(Fp,"%.3f l a\n",OffsetY+0.2);
    fprintf(Fp,"%.3f ",(Temp-XMin)*ScaleX+OffsetX);
    if (Lin) fprintf(Fp,"%.3f m ",(YMax-YMin)*ScaleY+OffsetY);
    else     fprintf(Fp,"%.3f m ",(log((gfloat)YMax)-log((gfloat)YMin))*ScaleY+OffsetY);
    fprintf(Fp,"%.3f ",(Temp-XMin)*ScaleX+OffsetX);
    if (Lin) fprintf(Fp,"%.3f l a\n",(YMax-YMin)*ScaleY+OffsetY-0.2);
    else     fprintf(Fp,"%.3f l a\n",(log((gdouble)YMax)-log((gdouble)YMin))*ScaleY+OffsetY-0.2);
    fprintf(Fp,"%.3f ",(Temp-XMin)*ScaleX+OffsetX-0.09*Len);
    fprintf(Fp,"%.3f m ",OffsetY-0.5);
    fprintf(Fp,"(%g) show\n",Temp);
    }

if (Lin) AxisStep=(YMax-YMin)/5.0;                                                                      //Y-axis tick marks
else     AxisStep=(log((gdouble)YMax)-log((gdouble)YMin))/5.0;                         //Y-axis tick marks logarithmic case
Temp=log(AxisStep)/log(10.0);
AxisStep /= pow(10.0,(floor(Temp)));
if (floor(AxisStep)==0.0) AxisStep=1.0; else if (floor(AxisStep)>6.5) AxisStep=10.0;
else if (floor(AxisStep)==6.0) AxisStep=5.0; else if (floor(AxisStep)==4.0) AxisStep=5.0;
else if (floor(AxisStep)==3.0) AxisStep=2.0; else AxisStep = floor(AxisStep);
AxisStep *= pow(10.0,(floor(Temp)));
if (Lin) AxisOrigin=(floor((gfloat)YMin/AxisStep)+1.0)*AxisStep;
else     AxisOrigin=(floor(log((gdouble)YMin)/AxisStep)+1.0)*AxisStep;
for (Temp=AxisOrigin;Temp<=YMax;Temp+=AxisStep)
    {
    if (!Lin) LTemp=log10(Temp);
    if ( Lin || (LTemp==floor(LTemp)) ) 
       {
       if ((fabs(Temp)<1.0e-6) && (AxisStep>1.0e-4))  Temp=0.0;
       sprintf(Text,"%g",Temp); Len=strlen(Text);
       fprintf(Fp,"%.3f ",OffsetX);
       if (Lin) fprintf(Fp,"%.3f m ",(Temp-YMin)*ScaleY+OffsetY);
       else     fprintf(Fp,"%.3f m ",(log(Temp)-log((gdouble)YMin))*ScaleY+OffsetY);
       fprintf(Fp,"%.3f ",OffsetX+0.2);
       if (Lin) fprintf(Fp,"%.3f l a\n",(Temp-YMin)*ScaleY+OffsetY);
       else     fprintf(Fp,"%.3f l a\n",(log(Temp)-log((gdouble)YMin))*ScaleY+OffsetY);
       fprintf(Fp,"%.3f ",(XMax-XMin)*ScaleX+OffsetX);
       if (Lin) fprintf(Fp,"%.3f m ",(Temp-YMin)*ScaleY+OffsetY);
       else     fprintf(Fp,"%.3f m ",(log(Temp)-log((gdouble)YMin))*ScaleY+OffsetY);
       fprintf(Fp,"%.3f ",(XMax-XMin)*ScaleX+OffsetX-0.2);
       if (Lin) fprintf(Fp,"%.3f l a\n",(Temp-YMin)*ScaleY+OffsetY);
       else     fprintf(Fp,"%.3f l a\n",(log(Temp)-log((gdouble)YMin))*ScaleY+OffsetY);
       LeftX=OffsetX-0.17*Len-0.2;
       fprintf(Fp,"%.3f ",LeftX);
       if (Lin) fprintf(Fp,"%.3f m ",(Temp-YMin)*ScaleY+OffsetY-0.1);
       else     fprintf(Fp,"%.3f m ",(log(Temp)-log((gdouble)YMin))*ScaleY+OffsetY-0.1);
       fprintf(Fp,"(%g) show\n",Temp);
       }
    }
                 
fprintf(Fp,"/Helvetica findfont\n");                                                                         //X-axis label
fprintf(Fp,"0.5 scalefont setfont\n");
Len=strlen(XLabel);
fprintf(Fp,"%.3f ",((XMax-XMin)*0.5)*ScaleX+OffsetX-Len*0.15);
fprintf(Fp,"%.3f m ",OffsetY-1.5);
fprintf(Fp,"(%s) show\n",XLabel);
Len=strlen(YLabel);                                                                                          //Y-Axis label
fprintf(Fp,"%.3f ",LeftX-0.4);
if (Lin) fprintf(Fp,"%.3f m ",((YMax-YMin)*0.5)*ScaleY+OffsetY-Len*0.15);
else     fprintf(Fp,"%.3f m ",((log((gdouble)YMax)-log((gdouble)YMin))*0.5)*ScaleY+OffsetY-Len*0.15);
fprintf(Fp,"gsave\n");
fprintf(Fp,"90 rotate\n");
fprintf(Fp,"(%s) show\n",YLabel);
fprintf(Fp,"grestore\n");

fprintf(Fp,"/Helvetica findfont\n");                                                                         //Write title
fprintf(Fp,"0.7 scalefont setfont\n");
Len=strlen(TLabel);
fprintf(Fp,"%.3f ",((XMax-XMin)*0.5)*ScaleX+OffsetX-Len*0.23);
if (Lin) fprintf(Fp,"%.3f m ",(YMax-YMin)*ScaleY+OffsetY+1.5);
else     fprintf(Fp,"%.3f m ",(log((gdouble)YMax)-log((gdouble)YMin))*ScaleY+OffsetY+1.5);
fprintf(Fp,"(%s) show\n",TLabel);

fprintf(Fp,"showpage\ngrestore\n%%%%Trailer");
fclose(Fp);
if (!OutToFile) { sprintf(Str,"%s /tmp/lamps_ps_file.ps",PrintCommand); system(Str); }
if (FileType==2)                                                                              //Create PDF file
   {
   sprintf(Str,"mv %s /tmp/lamps_ps_file.eps",FName); system(Str);
   sprintf(Str,"ps2pdf /tmp/lamps_ps_file.eps %s",FName); system(Str);
   }
}
/*------------------------------------------------------------------------------------------------------------------------*/
void ProceedPS1(GtkWidget *W,GtkWidget *Win)
{
gint XMin,XMax,YMin,YMax,XSize,YSize,FileType;
const gchar *XLabel,*YLabel,*TLabel,*FName,*PrintCommand,*Str;
gboolean Lin,Landscape,OutToFile;

XMin=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->XMinEntry)));                           //Minimum channel number for plot
XMax=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->XMaxEntry)));                           //Maximum channel number for plot
YMin=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->YMinEntry)));                                   //Minimum counts for plot
YMax=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->YMaxEntry)));                                   //Maximum counts for plot
Lin=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(PSForm->Lin));            //TRUE for Linear plots, FALSE for Semi-log
XLabel=gtk_entry_get_text(GTK_ENTRY(PSForm->XLabel));                                                     //X axis label
YLabel=gtk_entry_get_text(GTK_ENTRY(PSForm->YLabel));                                                     //Y axis label
TLabel=gtk_entry_get_text(GTK_ENTRY(PSForm->TLabel));                                                       //Plot title
XSize=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->XSizeEntry)));                                          //X Size of plot
YSize=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->YSizeEntry)));                                          //Y Size of plot
Landscape=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(PSForm->Landscape));   //TRUE for Landscape, FALSE for Portrait
Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(PSForm->FileType)->entry));                  //Postscript, EPS or PDF Acrobat
if (!strcmp(Str,"Postscript")) FileType=0;
else if (!strcmp(Str,"EPS")) FileType=1;
else if (!strcmp(Str,"PDF Acrobat")) FileType=2;
FName=gtk_entry_get_text(GTK_ENTRY(PSForm->FNameEntry));                                                     //File name
PrintCommand=gtk_entry_get_text(GTK_ENTRY(PSForm->PrintCommand));                           //Print device e.g. /dev/lp0
OutToFile=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(PSForm->OutToFile)); //TRUE: output tofile, FALSE: direct print
MakePSFile1(XMin,XMax,YMin,YMax,Lin,XLabel,YLabel,TLabel,XSize,YSize,Landscape,FileType,FName,PrintCommand,OutToFile);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void PreparePSDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (GTK_IS_WIDGET(Win)) gtk_widget_destroy(Win);
g_free(PSForm);                                                                                    //Free global structure
}
/*------------------------------------------------------------------------------------------------------------------------*/
void FileTypeCallback(GtkWidget *W,gpointer Unused)
{
const gchar *FTypeStr,*Str;
gchar FNameStr[24];
                                                                                                                             
FTypeStr=gtk_entry_get_text(GTK_ENTRY(W));
Str=gtk_entry_get_text(GTK_ENTRY(PSForm->FNameEntry)); strcpy(FNameStr,Str);
if (!strlen(FNameStr)) strcpy(FNameStr,"myfile");
if (!strcmp(FTypeStr,"Postscript"))       strcpy(&FNameStr[strcspn(FNameStr,".")],".ps"); 
else if (!strcmp(FTypeStr,"EPS"))         strcpy(&FNameStr[strcspn(FNameStr,".")],".eps");
else if (!strcmp(FTypeStr,"PDF Acrobat")) strcpy(&FNameStr[strcspn(FNameStr,".")],".pdf");
gtk_entry_set_text(GTK_ENTRY(PSForm->FNameEntry),FNameStr);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void PreparePS1(GtkWidget *W,gint WinNo)
{
gint SNo,i;
GtkWidget *Win,*VBox,*Label,*HBox,*But,*Sep;
static GdkColor Black = {0,0x0000,0x0000,0x0000};
static GdkColor White = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor   Red = {0,0xFFFF,0x0000,0x0000};
static GdkColor  Blue = {0,0x0000,0x0000,0xFFFF};
static GdkColor Green = {0,0x0000,0x7777,0x0000};
GtkStyle *RedStyle,*GreenStyle,*BlueStyle,*BlackStyle;
gchar Str[80],Str1[22];
GList *GList;

RedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { RedStyle->fg[i]=RedStyle->text[i]=Red; RedStyle->bg[i]=White; }
GreenStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { GreenStyle->fg[i]=GreenStyle->text[i]=Green; GreenStyle->bg[i]=White; }
BlueStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { BlueStyle->fg[i]=BlueStyle->text[i]=Blue; BlueStyle->bg[i]=White; }
BlackStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { BlackStyle->fg[i]=BlackStyle->text[i]=Black; BlueStyle->bg[i]=White; }
                                                                                                                             
PSForm=g_new(struct PSForm,1);                                 //Create global structure to be freed in PreparePSDestroyed 

SNo=ScreenSpecNo[WinNo][Screen];
PSForm->WinNo=WinNo;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                  //Define Win and make it modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(PreparePSDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"1d Spectrum to PS/EPS/PDF");
gtk_window_set_position(GTK_WINDOW(Win),GTK_WIN_POS_CENTER);
gtk_widget_set_usize(GTK_WIDGET(Win),424,324); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[WinNo]));                              //Window visibility
                                                                                                                             
VBox=gtk_vbox_new(FALSE,8); gtk_container_add(GTK_CONTAINER(Win),VBox);
Label=gtk_label_new("Postscript Driver for 1d Spectra\nadapted from code sent by S.Mukhopadhyay (IUC-DAEF-CC)"); 
SetStyleRecursively(Label,BlueStyle);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Sep=gtk_hseparator_new(); SetStyleRecursively(Sep,BlackStyle); gtk_box_pack_start(GTK_BOX(VBox),Sep,FALSE,TRUE,0);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("XMin"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->XMinEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->XMinEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->XMinEntry,TRUE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].One.XLo); gtk_entry_set_text(GTK_ENTRY(PSForm->XMinEntry),Str);
Label=gtk_label_new("XMax"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->XMaxEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->XMaxEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->XMaxEntry,TRUE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].One.XHi); gtk_entry_set_text(GTK_ENTRY(PSForm->XMaxEntry),Str);
Label=gtk_label_new("YMin"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->YMinEntry=gtk_entry_new_with_max_length(6); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->YMinEntry),62,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->YMinEntry,TRUE,FALSE,0);
sprintf(Str,"%f.0",Prop[WinNo].One.CountsLo); gtk_entry_set_text(GTK_ENTRY(PSForm->YMinEntry),Str);
Label=gtk_label_new("YMax"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->YMaxEntry=gtk_entry_new_with_max_length(8); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->YMaxEntry),78,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->YMaxEntry,TRUE,FALSE,0);
sprintf(Str,"%f.0",Prop[WinNo].One.CountsHi); gtk_entry_set_text(GTK_ENTRY(PSForm->YMaxEntry),Str);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4);
PSForm->Lin=gtk_radio_button_new_with_label(NULL,"Linear Plot"); 
gtk_box_pack_start(GTK_BOX(HBox),PSForm->Lin,TRUE,FALSE,0);
But=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(PSForm->Lin)),"Semi-Log Plot");
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("X-axis Label"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->XLabel=gtk_entry_new_with_max_length(17); gtk_widget_set_usize(GTK_WIDGET(PSForm->XLabel),115,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->XLabel,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->XLabel),"Channel Number");
Label=gtk_label_new("Y-axis Label"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->YLabel=gtk_entry_new_with_max_length(17); gtk_widget_set_usize(GTK_WIDGET(PSForm->YLabel),115,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->YLabel,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->YLabel),"Counts");
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("Plot Title"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->TLabel=gtk_entry_new_with_max_length(60); gtk_widget_set_usize(GTK_WIDGET(PSForm->TLabel),250,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->TLabel,TRUE,FALSE,0);
CreateTitle1(Str,WinNo); gtk_entry_set_text(GTK_ENTRY(PSForm->TLabel),Str);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

Sep=gtk_hseparator_new(); SetStyleRecursively(Sep,BlackStyle); gtk_box_pack_start(GTK_BOX(VBox),Sep,FALSE,TRUE,0);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Plot X Size (cm)"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->XSizeEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->XSizeEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->XSizeEntry,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->XSizeEntry),"20");
Label=gtk_label_new("Plot Y Size (cm)"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->YSizeEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->YSizeEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->YSizeEntry,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->YSizeEntry),"10");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
PSForm->Landscape=gtk_radio_button_new_with_label(NULL,"Landscape"); 
gtk_box_pack_start(GTK_BOX(HBox),PSForm->Landscape,TRUE,FALSE,0);
But=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(PSForm->Landscape)),"Portrait");
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(But),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,20);
Label=gtk_label_new("Type"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,-30);
PSForm->FileType=gtk_combo_new(); gtk_widget_set_usize(GTK_WIDGET(PSForm->FileType),100,22);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(PSForm->FileType)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->FileType,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Postscript"); GList=g_list_append(GList,"EPS");
GList=g_list_append(GList,"PDF Acrobat");
gtk_combo_set_popdown_strings(GTK_COMBO(PSForm->FileType),GList);

HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("File Name"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
PSForm->FNameEntry=gtk_entry_new_with_max_length(18); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->FNameEntry),120,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->FNameEntry,TRUE,FALSE,0);
AbbreviateFileName(Str1,Setup.Files.Oned[SNo],18); 
if (strlen(Str1)==0) strcpy(Str1,"myfile.ps");
else strcpy(&Str1[strlen(Str1)-3],"ps");
gtk_entry_set_text(GTK_ENTRY(PSForm->FNameEntry),Str1);
Label=gtk_label_new("Print Command"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
PSForm->PrintCommand=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->PrintCommand),60,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->PrintCommand,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->PrintCommand),"lpr");
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

gtk_signal_connect(GTK_OBJECT(GTK_COMBO(PSForm->FileType)->entry),"changed",GTK_SIGNAL_FUNC(FileTypeCallback),NULL);
                                                        //Declare callback to FileType only after FileNameEntry is active
HBox=gtk_hbox_new(FALSE,4);
PSForm->OutToFile=gtk_radio_button_new_with_label(NULL,"Make ps/eps/pdf File");
gtk_box_pack_start(GTK_BOX(HBox),PSForm->OutToFile,TRUE,FALSE,0);
But=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(PSForm->OutToFile)),
                                    "Send to Postscript Printer");
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

Sep=gtk_hseparator_new(); SetStyleRecursively(Sep,BlackStyle); gtk_box_pack_start(GTK_BOX(VBox),Sep,FALSE,TRUE,0);

HBox=gtk_hbox_new(FALSE,4);
But=gtk_button_new_with_label("Proceed"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
SetStyleRecursively(But,GreenStyle);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedPS1),Win);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
SetStyleRecursively(But,GreenStyle);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void MakePSFile2(gint XMin,gint XMax,gint YMin,gint YMax,gboolean Auto,gfloat CountsHi,gboolean CountsLo,gint ScaleType,
                const gchar *XLabel,const gchar *YLabel,const gchar *TLabel,gint XSize,gint YSize,gboolean Landscape,
                gint FileType,const gchar *FName,const gchar *PrintCommand,gboolean OutToFile)
{
gchar FileName[MAX_FNAME_LENGTH+5],Str[80];
FILE *Fp;
gint i,X0,Y0,DX,DY,X,Y,XVal,DelX,YVal,DelY,SNo,ChX,ChY,Res,XPix,YPix,XPix1,XPix2,YPix1,YPix2,Hue;
guint32 Index,Counts;
gfloat CountsSlope,XSlope,YSlope;
gdouble Colour,Red,Blue,Green,Sum;

if (OutToFile) strcpy(FileName,FName); else strcpy(FileName,"/tmp/lamps_ps_file.ps");
if (!(Fp=fopen(FileName,"w"))) { sprintf(Str,"Open %s failed",Str); Attention(0,Str); return; }

X0=80; Y0=230; DX=DY=512;                                                                        //Origin and Axis lengths

if (FileType == 0)                                                                        //FileType=0(PS), 1(EPS), 2(PDF)
   {
   fprintf(Fp,"%%!PS-Adobe-1.0\n");                                                                        //For PS files
   if (Landscape) fprintf(Fp,"%%%%Orientation: Landscape\n"); else fprintf(Fp,"%%%%Orientation: Portrait\n");
   fprintf(Fp,"%%%%EndComments\n%%%%EndProlog\n\n");
   }
else fprintf(Fp,"%%!PS-Adobe EPSF-3.0\n%%%%BoundingBox: 6 150 700 790\n\n");        //For EPS files, also PDF (for ps2pdf)

fprintf(Fp,"/l {moveto lineto stroke} def  %%Subroutine drawline\n");                                         //Subroutine
fprintf(Fp,"/s {setrgbcolor} def           %%Subroutine setrgbcolor\n\n");                                    //Subroutine
fprintf(Fp,"newpath\n3 setlinewidth\n1 setlinecap\n\n");                                                   //Line settings
fprintf(Fp,"%%X Axis\n %d %d %d %d l\n\n",X0,Y0-3,X0+DX,Y0-3);                                                    //X-Axis
fprintf(Fp,"%%X Axis Ticks\n");
for (i=0,X=X0;i<=4;++i,X+=DX/4) fprintf(Fp," %d %d %d %d l\n",X,Y0-3,X,Y0-10);
fprintf(Fp,"\n%%Y Axis\n %d %d %d %d l\n\n",X0-3,Y0,X0-3,Y0+DY);                                                  //Y-Axis
fprintf(Fp,"%%Y Axis Ticks\n");
for (i=0,Y=Y0;i<=4;++i,Y+=DY/4) fprintf(Fp," %d %d %d %d l\n",X0-3,Y,X0-10,Y);
fprintf(Fp,"\n%%Title\n/Times-Roman findfont\n20 scalefont\nsetfont\nnewpath\n");                             //Main Title
fprintf(Fp,"%d %d moveto\n",X0+DX/2-6*strlen(TLabel),Y0+DY+10);
fprintf(Fp,"(%s) true charpath\n",TLabel);
fprintf(Fp,"0.5 setlinewidth\n%%0.4 setgray\nstroke\n\n");
fprintf(Fp,"%%X-Axis Title\n/Times-Roman findfont\n20 scalefont\nsetfont\nnewpath\n");                      //X-Axis Title
fprintf(Fp,"%d %d moveto\n",(gint)(X0+0.625*DX-5*strlen(XLabel)),Y0-50);
fprintf(Fp,"(%s) show\n\n",XLabel);
fprintf(Fp,"%%Y-Axis Title\n/Times-Roman findfont\n20 scalefont\nsetfont\nnewpath\n");                      //Y-Axis Title
fprintf(Fp," %d %d moveto\n",X0-50,(gint)(Y0+0.625*DY-5*strlen(YLabel)));
fprintf(Fp,"90 rotate\n(%s) show\n-90 rotate\n\n",YLabel);
fprintf(Fp,"%%X-Axis Tick Values\n/Times-Roman findfont\n20 scalefont\nsetfont\nnewpath\n");          //X-Axis Tick Values
DelX=(gint)(0.25*(XMax-XMin)+0.5);
for (i=0,X=X0-20,XVal=XMin;i<=4;++i,X+=DX/4,XVal+=DelX) fprintf(Fp," %d %d moveto\n(%4d) show\n",X,Y0-30,XVal);
fprintf(Fp,"\n%%Y-Axis Tick Values\n/Times-Roman findfont\n20 scalefont\nsetfont\nnewpath\n");        //Y-Axis Tick Values
DelY=(gint)(0.25*(YMax-YMin)+0.5);
for (i=0,Y=Y0-7,YVal=YMin;i<=4;++i,Y+=DY/4,YVal+=DelY) fprintf(Fp," %d %d moveto\n(%4d) show\n",X0-48,Y,YVal);

fprintf(Fp,"\n%%Coloured Spots\n0 setlinecap\n");                                           //Coloured Dots, Lines or Boxes
SNo=ScreenSpecNo[PSForm->WinNo][Screen]; 
if (PSForm->Auto)                                                                       //Find Maximum Counts in the region
   {
   for (ChX=XMin,CountsHi=4.0;ChX<XMax;++ChX)
       {
       for (ChY=YMin;ChY<YMax;++ChY)
           {
           Index=ChX+Setup.Twod.XChan[SNo]*ChY;
           Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
           if (CountsHi<Counts) CountsHi=Counts;
           }
       }
   }
switch (ScaleType)                                                                  //ScaleType =0(Log)  =1(Srqt)  =2(Lin)
   {
   case 0: CountsSlope=31.0/(log((gdouble)CountsHi)-log((gdouble)CountsLo)); break;
   case 1: CountsSlope=31.0/(sqrt((gdouble)CountsHi)-sqrt((gdouble)CountsLo)); break;
   case 2: CountsSlope=31.0/(gfloat)(CountsHi-CountsLo);
   }
XSlope=(gfloat)DX/(gfloat)(XMax-XMin); YSlope=(gfloat)DY/(gfloat)(YMax-YMin);

Res=0; if (XSlope>1.0) Res++; if (YSlope>1.0) Res+=2;

switch (Res)                                                                //4 different plot methods depending upon Res
   {
   case 0:                           //Plot resolution is lower than spectrum resolution for both x and y. Make a dot plot
      for (XPix=0;XPix<DX;++XPix)
          {
          ChX=(gint)XPix/XSlope+XMin;
          for (YPix=0;YPix<DY;++YPix)
              {
              ChY=(gint)YPix/YSlope+YMin;
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=CountsLo)
                 {
                 switch (ScaleType)
                    {
                    case 0: Colour=(log((double)Counts)-log((double)CountsLo))*CountsSlope; break;
                    case 1: Colour=(sqrt((double)Counts)-sqrt((double)CountsLo))*CountsSlope; break;
                    case 2: Colour=(Counts-CountsLo)*CountsSlope;
                    }
                 Hue=((gint)Colour) % 32; Red=(gdouble)D2PenColours[Hue][0]/0xFFFF; 
                 Green=(gdouble)D2PenColours[Hue][1]/0xFFFF; Blue=(gdouble)D2PenColours[Hue][2]/0xFFFF;
                 Sum=Red+Green+Blue; if (Sum==0.0) Sum=1.0;
                 fprintf(Fp,"%3.2f %3.2f %3.2f s\n",Red/Sum,Green/Sum,Blue/Sum);
                 fprintf(Fp,"%d %d %d %d l\n",X0+XPix,Y0+YPix,X0+XPix+1,Y0+YPix); //Actually a horiz. line of unit thickness
                 }
              }
          }
      break;
   case 1:                       //Plot resolution lower than spectrum in x, higher than spectrum in y. Draw horiz. lines
      for (YPix=0;YPix<DY;++YPix)
          {
          ChY=(gint)YPix/YSlope+YMin;
          for (ChX=XMin;ChX<XMax;++ChX)
              {
              XPix2=XSlope*(ChX-XMin+0.5); XPix1=MAX(XPix2-XSlope,0);
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=CountsLo)
                 {
                 switch (ScaleType)
                    {
                    case 0: Colour=(log((double)Counts)-log((double)CountsLo))*CountsSlope; break;
                    case 1: Colour=(sqrt((double)Counts)-sqrt((double)CountsLo))*CountsSlope; break;
                    case 2: Colour=(Counts-CountsLo)*CountsSlope;
                    }
                 Hue=((gint)Colour) % 32; Red=(gdouble)D2PenColours[Hue][0]/0xFFFF; 
                 Green=(gdouble)D2PenColours[Hue][1]/0xFFFF; Blue=(gdouble)D2PenColours[Hue][2]/0xFFFF;
                 Sum=Red+Green+Blue; if (Sum==0.0) Sum=1.0;
                 fprintf(Fp,"%3.2f %3.2f %3.2f s\n",Red/Sum,Green/Sum,Blue/Sum);
                 fprintf(Fp,"%d %d %d %d l\n",X0+XPix1,Y0+YPix,X0+XPix2,Y0+YPix);
                 }
              }
          }
      break;
   case 2:                       //Plot resolution higher than spectrum in x, lower than spectrum in x. Draw vert. lines
      for (XPix=0;XPix<DX;++XPix)
          {
          ChX=(gint)XPix/XSlope+XMin;
          for (ChY=YMin;ChY<YMax;++ChY)
              {
              YPix2=YSlope*(ChY-YMin+0.5); YPix1=MAX(YPix2-YSlope,0);
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=CountsLo)
                 {
                 switch (ScaleType)
                    {
                    case 0: Colour=(log((double)Counts)-log((double)CountsLo))*CountsSlope; break;
                    case 1: Colour=(sqrt((double)Counts)-sqrt((double)CountsLo))*CountsSlope; break;
                    case 2: Colour=(Counts-CountsLo)*CountsSlope;
                    }
                 Hue=((gint)Colour) % 32; Red=(gdouble)D2PenColours[Hue][0]/0xFFFF; 
                 Green=(gdouble)D2PenColours[Hue][1]/0xFFFF; Blue=(gdouble)D2PenColours[Hue][2]/0xFFFF;
                 Sum=Red+Green+Blue; if (Sum==0.0) Sum=1.0;
                 fprintf(Fp,"%3.2f %3.2f %3.2f s\n",Red/Sum,Green/Sum,Blue/Sum);
                 fprintf(Fp,"%d %d %d %d l\n",X0+XPix,Y0+YPix1,X0+XPix,Y0+YPix2);
                 }
              }
          }
      break;
   case 3:                                   //Plot resolution is higher than spectrum for both x and y. Draw rectangles
      fprintf(Fp,"%d setlinewidth\n",(gint)(YSlope+0.5));
      for (ChX=XMin;ChX<XMax;++ChX)
          {
          XPix2=XSlope*(ChX-XMin+0.5); XPix1=MAX(XPix2-XSlope,0);
          for (ChY=YMin;ChY<YMax;++ChY)
              {
              YPix1=YSlope*(ChY-YMin+0.5);
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=CountsLo)
                 //OriginX+XPix1,OriginY-YPix2,XPix2-XPix1,YPix2-YPix1
                 {
                 switch (ScaleType)
                    {
                    case 0: Colour=(log((double)Counts)-log((double)CountsLo))*CountsSlope; break;
                    case 1: Colour=(sqrt((double)Counts)-sqrt((double)CountsLo))*CountsSlope; break;
                    case 2: Colour=(Counts-CountsLo)*CountsSlope;
                    }
                 Hue=((gint)Colour) % 32; Red=(gdouble)D2PenColours[Hue][0]/0xFFFF; 
                 Green=(gdouble)D2PenColours[Hue][1]/0xFFFF; Blue=(gdouble)D2PenColours[Hue][2]/0xFFFF;
                 Sum=Red+Green+Blue; if (Sum==0.0) Sum=1.0;
                 fprintf(Fp,"%3.2f %3.2f %3.2f s\n",Red/Sum,Green/Sum,Blue/Sum);
                 fprintf(Fp,"%d %d %d %d l\n",X0+XPix1,Y0+YPix1,X0+XPix2,Y0+YPix1);
                  }
              }
          }
   }
fprintf(Fp,"\nshowpage\n%%%%Trailer");                                                        //End of PS stuff
fclose(Fp);
if (!OutToFile) { sprintf(Str,"%s /tmp/lamps_ps_file.ps",PrintCommand); system(Str); }
if (FileType==2)                                                                              //Create PDF file
   {
   sprintf(Str,"mv %s /tmp/lamps_ps_file.eps",FName); system(Str);
   sprintf(Str,"ps2pdf /tmp/lamps_ps_file.eps %s",FName); system(Str);
   }
}
/*------------------------------------------------------------------------------------------------------------------------*/
void ProceedPS2(GtkWidget *W,GtkWidget *Win)
{
gint XMin,XMax,YMin,YMax,XSize,YSize,ScaleType,FileType;
gfloat CountsLo,CountsHi;
const gchar *ColourScale,*XLabel,*YLabel,*TLabel,*FName,*PrintCommand,*Str;
gboolean Auto,Landscape,OutToFile;

XMin=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->XMinEntry)));                         //Minimum X-channel number for plot
XMax=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->XMaxEntry)));                         //Maximum X-channel number for plot
YMin=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->YMinEntry)));                         //Minimum Y-channel number for plot
YMax=atoi(gtk_entry_get_text(GTK_ENTRY(PSForm->YMaxEntry)));                         //Maximum Y-channel number for plot
Auto=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(PSForm->Auto));                                  //Auto Counts Scale
CountsHi=atof(gtk_entry_get_text(GTK_ENTRY(PSForm->MaxCounts)));                                        //Maximum Counts
CountsLo=atof(gtk_entry_get_text(GTK_ENTRY(PSForm->MinCounts)));                                        //Minimum Counts
ColourScale=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(PSForm->Lin)->entry));
if (!strncmp(ColourScale,"Log.",3)) ScaleType=0;                                             //ScaleType=0 ... Log Scale
else if (!strncmp(ColourScale,"Sqrt.",3)) ScaleType=1;                                      //ScaleType=1 ... Sqrt Scale
else ScaleType=2;                                                                         //ScaleType=2 ... Linear Scale 
CountsHi=MAX(CountsHi,4.0);                                                                                   //Validate
if (ScaleType==0) CountsLo=CLAMP(CountsLo,1.0,CountsHi-1); else CountsLo=CLAMP(CountsLo,0.0,CountsHi-1);      //Validate
XLabel=gtk_entry_get_text(GTK_ENTRY(PSForm->XLabel));                                                     //X axis label
YLabel=gtk_entry_get_text(GTK_ENTRY(PSForm->YLabel));                                                     //Y axis label
TLabel=gtk_entry_get_text(GTK_ENTRY(PSForm->TLabel));                                                       //Plot title
Landscape=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(PSForm->Landscape));   //TRUE for Landscape, FALSE for Portrait
Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(PSForm->FileType)->entry));                  //Postscript, EPS or PDF Acrobat
if (!strcmp(Str,"Postscript")) FileType=0;
else if (!strcmp(Str,"EPS")) FileType=1;
else if (!strcmp(Str,"PDF Acrobat")) FileType=2;
FName=gtk_entry_get_text(GTK_ENTRY(PSForm->FNameEntry));                                                     //File name
PrintCommand=gtk_entry_get_text(GTK_ENTRY(PSForm->PrintCommand));                           //Print device e.g. /dev/lp0
OutToFile=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(PSForm->OutToFile)); //TRUE: output tofile, FALSE: direct print
MakePSFile2(XMin,XMax,YMin,YMax,Auto,CountsHi,CountsLo,ScaleType,XLabel,YLabel,TLabel,XSize,YSize,Landscape,
            FileType,FName,PrintCommand,OutToFile);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void PreparePS2(GtkWidget *W,gint WinNo)
{
gint SNo,i;
GtkWidget *Win,*VBox,*VBox1,*Label,*HBox,*But,*Sep,*Frame;
static GdkColor    Black = {0,0x0000,0x0000,0x0000};
static GdkColor    White = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor      Red = {0,0xFFFF,0x0000,0x0000};
static GdkColor     Blue = {0,0x0000,0x0000,0xFFFF};
static GdkColor    Green = {0,0x0000,0x7777,0x0000};
static GdkColor FrameBg  = {0,0xFFFF,0x0000,0x0000};
static GdkColor FrameFg  = {0,0x8888,0x0000,0xFFFF};
GtkStyle *RedStyle,*GreenStyle,*BlueStyle,*BlackStyle,*FrameStyle;
gchar Str[80],Str1[22],Title[2*MAX_TEXT_FIELD+40],XTitle[MAX_TEXT_FIELD],YTitle[MAX_TEXT_FIELD];
GList *GList;

RedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { RedStyle->fg[i]=RedStyle->text[i]=Red; RedStyle->bg[i]=White; }
GreenStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { GreenStyle->fg[i]=GreenStyle->text[i]=Green; GreenStyle->bg[i]=White; }
BlueStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { BlueStyle->fg[i]=BlueStyle->text[i]=Blue; BlueStyle->bg[i]=White; }
BlackStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { BlackStyle->fg[i]=BlackStyle->text[i]=Black; BlueStyle->bg[i]=White; }
FrameStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }
                                                                                                                             
PSForm=g_new(struct PSForm,1);                                 //Create global structure to be freed in PreparePSDestroyed 
SNo=ScreenSpecNo[WinNo][Screen];
PSForm->WinNo=WinNo;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                  //Define Win and make it modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(PreparePSDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"2d Spectrum to PS/EPS/PDF");
gtk_window_set_position(GTK_WINDOW(Win),GTK_WIN_POS_CENTER);
gtk_widget_set_usize(GTK_WIDGET(Win),400,340); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[WinNo]));                              //Window visibility
                                                                                                                             
VBox=gtk_vbox_new(FALSE,8); gtk_container_add(GTK_CONTAINER(Win),VBox);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("XMin"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->XMinEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->XMinEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->XMinEntry,TRUE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].Two.XLo); gtk_entry_set_text(GTK_ENTRY(PSForm->XMinEntry),Str);
Label=gtk_label_new("XMax"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->XMaxEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->XMaxEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->XMaxEntry,TRUE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].Two.XHi); gtk_entry_set_text(GTK_ENTRY(PSForm->XMaxEntry),Str);
Label=gtk_label_new("YMin"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->YMinEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->YMinEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->YMinEntry,TRUE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].Two.YLo); gtk_entry_set_text(GTK_ENTRY(PSForm->YMinEntry),Str);
Label=gtk_label_new("YMax"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); SetStyleRecursively(Label,RedStyle);
PSForm->YMaxEntry=gtk_entry_new_with_max_length(5); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->YMaxEntry),54,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->YMaxEntry,TRUE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].Two.YHi); gtk_entry_set_text(GTK_ENTRY(PSForm->YMaxEntry),Str);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

Frame=gtk_frame_new("COLOUR SETTINGS"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox1=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Frame),VBox1);
gtk_container_set_border_width(GTK_CONTAINER(VBox1),4);

HBox=gtk_hbox_new(FALSE,4);
PSForm->Auto=gtk_check_button_new_with_label("Auto scaling for Max. Counts");
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(PSForm->Auto),Prop[WinNo].Two.CountsAuto);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->Auto,TRUE,FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("Max Counts"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
PSForm->MaxCounts=gtk_entry_new_with_max_length(11); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->MaxCounts),80,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->MaxCounts,FALSE,FALSE,0);
if (Prop[WinNo].Two.CountsHi>9.0e6) sprintf(Str,"%.2e",Prop[WinNo].Two.CountsHi);
else                                sprintf(Str,"%.0f",Prop[WinNo].Two.CountsHi);
gtk_entry_set_text(GTK_ENTRY(PSForm->MaxCounts),Str);
Label=gtk_label_new("Min Counts"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,40);
PSForm->MinCounts=gtk_entry_new_with_max_length(11); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->MinCounts),80,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->MinCounts,FALSE,FALSE,-20);
if (Prop[WinNo].Two.CountsLo>9.0e6) sprintf(Str,"%.2e",Prop[WinNo].Two.CountsLo);
else                                sprintf(Str,"%.0f",Prop[WinNo].Two.CountsLo);
gtk_entry_set_text(GTK_ENTRY(PSForm->MinCounts),Str);
gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("Please set colours from 2d menu - Colours");
gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4);
PSForm->Lin=gtk_combo_new(); gtk_widget_set_usize(GTK_WIDGET(PSForm->Lin),144,22);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(PSForm->Lin)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->Lin,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Log. Colour Scale"); GList=g_list_append(GList,"Sqrt. Colour Scale");
GList=g_list_append(GList,"Lin. Colour Scale");
gtk_combo_set_popdown_strings(GTK_COMBO(PSForm->Lin),GList);
gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);

CreateTitle2(Title,XTitle,YTitle,WinNo);
HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("X-axis Label"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->XLabel=gtk_entry_new_with_max_length(17); gtk_widget_set_usize(GTK_WIDGET(PSForm->XLabel),115,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->XLabel,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->XLabel),XTitle);
Label=gtk_label_new("Y-axis Label"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->YLabel=gtk_entry_new_with_max_length(17); gtk_widget_set_usize(GTK_WIDGET(PSForm->YLabel),115,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->YLabel,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->YLabel),YTitle);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("Plot Title"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
PSForm->TLabel=gtk_entry_new_with_max_length(60); gtk_widget_set_usize(GTK_WIDGET(PSForm->TLabel),250,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->TLabel,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->TLabel),Title); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

Sep=gtk_hseparator_new(); SetStyleRecursively(Sep,BlackStyle); gtk_box_pack_start(GTK_BOX(VBox),Sep,FALSE,TRUE,0);

HBox=gtk_hbox_new(FALSE,4);
PSForm->Landscape=gtk_radio_button_new_with_label(NULL,"Landscape"); 
gtk_box_pack_start(GTK_BOX(HBox),PSForm->Landscape,TRUE,FALSE,0);
But=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(PSForm->Landscape)),"Portrait");
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(But),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,20); 
Label=gtk_label_new("Type"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,-30);
PSForm->FileType=gtk_combo_new(); gtk_widget_set_usize(GTK_WIDGET(PSForm->FileType),100,22);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(PSForm->FileType)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->FileType,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Postscript"); GList=g_list_append(GList,"EPS");
GList=g_list_append(GList,"PDF Acrobat");
gtk_combo_set_popdown_strings(GTK_COMBO(PSForm->FileType),GList);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4);
Label=gtk_label_new("File Name"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
PSForm->FNameEntry=gtk_entry_new_with_max_length(18); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->FNameEntry),120,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->FNameEntry,TRUE,FALSE,0);
AbbreviateFileName(Str1,Setup.Files.Twod[SNo],18); 
if (strlen(Str1)==0) strcpy(Str1,"myfile.ps");
else strcpy(&Str1[strlen(Str1)-3],"ps");
gtk_entry_set_text(GTK_ENTRY(PSForm->FNameEntry),Str1);
Label=gtk_label_new("Print Command"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
PSForm->PrintCommand=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_widget_set_usize(GTK_WIDGET(PSForm->PrintCommand),60,18);
gtk_box_pack_start(GTK_BOX(HBox),PSForm->PrintCommand,TRUE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(PSForm->PrintCommand),"lpr"); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

gtk_signal_connect(GTK_OBJECT(GTK_COMBO(PSForm->FileType)->entry),"changed",GTK_SIGNAL_FUNC(FileTypeCallback),NULL);
                                                        //Declare callback to FileType only after FileNameEntry is active 
HBox=gtk_hbox_new(FALSE,4);
PSForm->OutToFile=gtk_radio_button_new_with_label(NULL,"Make ps/eps/pdf File");
gtk_box_pack_start(GTK_BOX(HBox),PSForm->OutToFile,TRUE,FALSE,0);
But=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(PSForm->OutToFile)),
                                    "Send to Postscript Printer");
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

Sep=gtk_hseparator_new(); SetStyleRecursively(Sep,BlackStyle); gtk_box_pack_start(GTK_BOX(VBox),Sep,FALSE,TRUE,0);

HBox=gtk_hbox_new(FALSE,4);
But=gtk_button_new_with_label("Proceed"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
SetStyleRecursively(But,GreenStyle);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedPS2),Win);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
SetStyleRecursively(But,GreenStyle);
gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*------------------------------------------------------------------------------------------------------------------*/
