#define GTK_ENABLE_BROKEN
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "lamps.h"

#define CANVAS_TITLE_HEIGHT        22
#define AXIS_TICK_HEIGHT           6
#define AXIS_LABELS_HEIGHT         8
#define SPACE_BELOW_XAXIS          32
#define AXIS_LABELS_WIDTH          55
#define AXIS_TICK_WIDTH            6
#define CANVAS_RIGHT_MARGIN_WIDTH  20

#define FONT_HEIGHT 6
#define FONT_WIDTH 6

#define ORIGIN_X     (AXIS_LABELS_WIDTH + AXIS_TICK_WIDTH)
#define ORIGIN_Y(h)  (h -  SPACE_BELOW_XAXIS - AXIS_LABELS_HEIGHT - AXIS_TICK_HEIGHT)
#define LEN_X(w)     (w - AXIS_LABELS_WIDTH - AXIS_TICK_WIDTH - CANVAS_RIGHT_MARGIN_WIDTH)
#define LEN_Y(h)     (h - SPACE_BELOW_XAXIS - AXIS_LABELS_HEIGHT - AXIS_TICK_HEIGHT - CANVAS_TITLE_HEIGHT)

#define TSL2         1.665109222  //Value of 2.0*sqrt(ln2)
#define GX(x)        exp(-x*x)    //Gauss function

//External Global variables
extern enum ProgramState ProgramState;
extern struct Setup Setup;
extern guint16 *Oned16;
extern guint32 *Oned32,*Proj;
extern gint Off1d[MAX_1D],OffProj[MAX_2D];
extern gint Screen;
extern gint FrozenWin;
extern gint ScreenSpecNo[SCREEN_TOT][MAX_SCREENS],ScreenSpecType[SCREEN_TOT][MAX_SCREENS]; 
extern GtkWidget *SpecWin[SCREEN_TOT],*Canvas[SCREEN_TOT];                                     //Global spectrum widgets
extern GtkObject *HScrlAdj[SCREEN_TOT],*VScrlAdj[SCREEN_TOT];           //Global adjustmnets for horiz. and vert. Scroll
extern gfloat XSlope[SCREEN_TOT],CountsSlope[SCREEN_TOT];      //These slope arrays are used both by plot1.c and plot2.c
extern gint CanvasW[SCREEN_TOT],CanvasH[SCREEN_TOT];                                  //Used by both plot1.c and plot2.c
extern GdkFont *Font;
extern gint AreaMonitor,AreaWinOpen,OvWinOpen,FitWinOpen,CalibWinOpen,AssistWinOpen,ThemeChanged,PkSearchOpen;
extern struct WinProperties Prop[SCREEN_TOT];
extern struct FitType Fit;                                                                          //The Fit parameters
extern struct Calibration Calibration[MAX_TOTAL_PAR];
extern gint BananaDrawMode,BananaShowing,BananaWinNo;
extern struct FileSelectType *FileX;
extern gboolean CommonZoom,RealScroll;
extern gchar SpecDir[MAX_DIR_STRLEN];                                                                  //Directory prefs
extern GdkGC *Gc;                                                                 //Graphics context for 1d and 2d plots
extern GdkColor Colour1D[10]; 
                         //0=Backg,1=Plot,2=FitBkg,3=Axis,4=Title,5=Cursors,6=Pk Markers,7=Fit ROI,8=Fit,9=Perm. Cursors
GdkColor ColourOv[MAX_OVERLAP];                                                                    //Colours for overlap
extern gint TopOfset;                                         //Accounts for space occupied by window manager at the top

//Global variables for this file only
struct BoxType Box[SCREEN_TOT];
GdkPixmap *PixMap[SCREEN_TOT];
gint LcRc[2],GlobalTemp;
struct WidgetInt *AreaWidget;                                                  //Holds the area display window and WinNo
GtkWidget *FitWin;
static GdkGC *Pen[32] = {NULL};
struct CalibData CalibData;
struct PeakSearchUtil {GtkWidget *W1; GtkWidget *W2;};
struct PeakSearchUtil *PeakSearchUtil;
gboolean AutoCalibOn,SemiAutoCalibOn;
struct ForOverlap {gint WinNo;gint Row;GtkWidget *Spec[MAX_OVERLAP];GtkWidget *HShft[MAX_OVERLAP];
                   GtkWidget *VShft[MAX_OVERLAP];};
struct ForOverlap *ForOverlap;                                                          //Needed by the Overlap Tool widget
struct WidgetInt *U1;                                                                           //Needed by ReadWrite1dFile
struct Assist { GtkWidget *Win; GtkWidget *Peak1; GtkWidget *Peak2; gint PkNo; };                   //Needed by AssistCalib
struct Assist *Assist;                                                                              //Needed by AssistCalib
struct ManScl { GtkWidget *X_Min; GtkWidget *X_Max; GtkWidget *Y_Min; GtkWidget *Y_Max;};
struct ManScl *ManScl;

/*Function Templates*/
void WinClosed(GtkWidget *W,GtkWidget *Win);                                      //This function is to be found in setup.c
void Scroll(GtkAdjustment *HAdj,gpointer Data);
void Display1d(GtkWidget *W,gint WinNo);
guint32 Get1dCounts(gint WinNo,gint Ch,gboolean Overlap,gint OvSpec);
gint GetMaxChan(gint WinNo);
void Plot1(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH);
GdkGC *GetPen(gint i);
void SaveCalib(GtkWidget *W,gpointer Data);
gint MatInv(gint N,gdouble *C);
gint DoFit(gint NPts,gint NPara,gdouble (*Func)(),gint Option,gdouble *X,gdouble *Y,gdouble *P,
           gdouble *Err,enum FitFlags *Flag,gchar *Messg);
void *FitThread(gpointer Data);
void Attention(gint XPos,gchar *Messg);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void Plot2(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH);
void ZeroOned(gint SNo);
void Update1d(gint WinNo);
void Update2d(gint WinNo);
void ComputeXProjection(gint WinNo); void ComputeYProjection(gint WinNo);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void AddCalibPoint(gint WinNo,gint CalibX,gint Opt,gdouble ChNo,gdouble Energy);
gfloat LinBkg(gint Ch);
gdouble BiGauss(gint i,gdouble *P,gdouble *X);
void PreparePS1(GtkWidget *W,gpointer Data);
void CreateTitle1(gchar *Str,gint WinNo);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void radr16_(gchar *,gint *,guint16 *,gint *,gint *); void radr32_(gchar *,gint *,guint32 *,gint *,gint *);
void radw16_(gchar *,gint *,guint16 *,gint *,gchar *,gint *); void radw32_(gchar *,gint *,guint32 *,gint *,gchar *,gint *);
void SavePrefs(void);
void DeleteCalibPoint(GtkWidget *W,gpointer Data);
/*---------------------------------------------------------------------------------------------------------------------*/
void CalibTerms(GtkWidget *W,gpointer Data)
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Linear")) CalibData.Terms=2;
else if (!strcmp(Str,"Quadratic")) CalibData.Terms=3;
else if (!strcmp(Str,"Quad+Sqrt")) CalibData.Terms=4;
else CalibData.Terms=2;
}
/*---------------------------------------------------------------------------------------------------------------------*/
void CalibUnits(GtkWidget *W,gpointer Data)
{ 
strncpy(Calibration[CalibData.WinNo].Units,gtk_entry_get_text(GTK_ENTRY(W)),29); 
Calibration[CalibData.WinNo].Units[29]='\0'; 
}
/*------------------------------------------------------------------------------------------------------------------*/
void CalibWinDestroyed(GtkWidget *W,GtkWidget *CalibWin)
{ 
CalibWinOpen=FALSE; 
if (AssistWinOpen) gtk_widget_destroy(Assist->Win);
}
/*--------------------------------------------------------------------------------------------------------------------*/
gdouble CalibFunc(gint i,gdouble *P,gdouble *X)
{ return P[0]+P[1]*X[i]+P[2]*X[i]*X[i]+P[3]*sqrt(X[i]); }
/*--------------------------------------------------------------------------------------------------------------------*/
void DoCalibration(GtkWidget *W,gpointer Data)
{
gdouble P[4],Err[4],X[MAX_CALIB_DATA],Y[MAX_CALIB_DATA],C[16];
enum FitFlags Flag[4];
gchar Messg[40],Str[120];
gint i,j,WinNo,SNo,Par,T,N;
GList *Node;

WinNo=CalibData.WinNo; SNo=ScreenSpecNo[WinNo][Screen];
Par=Setup.Oned.Par[SNo];
for (i=0;i<CalibData.NPts;i++) { X[i]=CalibData.Ch[i]; Y[i]=CalibData.En[i]; }
if (CalibData.Terms==CalibData.NPts)                                    //In this case use matrix inversion instead of fit
   {
   for (i=0;i<4;i++) P[i]=0.0;
   N=CLAMP(CalibData.Terms,2,4);
   for (i=0;i<N;i++) { T=i*N; C[T]=1.0; C[T+1]=X[i]; if (N>2) C[T+2]=X[i]*X[i]; if (N>3) C[T+3]=sqrt(X[i]); }
   if (MatInv(N,C)) strcpy(Messg,"Matrix invert failed"); else strcpy(Messg,"Exact fit");
   for (i=0;i<N;i++) { T=i*N; for (j=0;j<N;j++) P[i]+=C[T+j]*Y[j]; }
   }
else                                                                                                    //otherwise do fit
   {
   for (i=0;i<4;i++) { P[i]=1.0; Flag[i]=Variable; }                                                           //All cases
   if (CalibData.Terms==2) { P[2]=P[3]=0.0; Flag[2]=Flag[3]=Fixed; }                                         //Linear case
   if (CalibData.Terms==3) { P[2]=1.0e-05; P[3]=0.0; Flag[3]=Fixed; }                                     //Quadratic case
   if (CalibData.Terms==4) P[2]=P[3]=1.0e-05;                                                        //Quadratic+Sqrt case
   DoFit(CalibData.NPts,4,CalibFunc,1,X,Y,P,Err,Flag,Messg);
   }
sprintf(Str,"P[0]=%g P[1]=%g P[2]=%g P[3]=%g\n%s",P[0],P[1],P[2],P[3],Messg);
gtk_label_set_text(GTK_LABEL(CalibData.Output),Str); gtk_widget_show(CalibData.Output);

Node=g_list_last(GTK_TABLE(CalibData.Table)->children);
for (j=0;j<3;j++) Node=g_list_previous(Node); 
for (i=0;i<CalibData.NPts;i++)
    {
    sprintf(Str,"%f",CalibFunc(i,P,X)); 
    gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);
    for (j=0;j<5;j++) Node=g_list_previous(Node); 
    }
for (i=0;i<4;i++) Calibration[Par-1].P[i]=P[i];
}
/*--------------------------------------------------------------------------------------------------------------------*/
void CalibrationPeaks(gint WinNo,gint NPk,gint NTPk,gdouble *Pk,gdouble *St)
//First do a peak search as in ProceedPeakSearch. 
//Pick up NPk2 peaks with the largest values of significance of the second derivative
//From these, select NPk peaks with the largest value of PeakCounts
//Use QuickFit to get the peak positions precisely
{
gdouble PSq,JSq,Thresh,C[100],CSum,Dd,Sd,Cj,Counts,Ss,SsPrev,Sig[MAX_PEAKS],Fwhm;
gint SNo,j,JMax,i,N,Del,Peaks[MAX_PEAKS],Lc,PeakCh,Ch,MaxChan,k,NPk2;
gboolean P;
guint32 PeakCounts;

SNo=ScreenSpecNo[WinNo][Screen];
Fwhm=6.0; Del=5; PSq=Fwhm/2.335; PSq=PSq*PSq; Thresh=4.0;                  //Somewhat arbitrary values of Fwhm and Thresh!

/*First calculate C[j]*/
JMax=99; C[0]=100.0; for (j=1;j<100;j++) C[j]=0.0;
for (j=1;j<100;j++)
    {
    JSq=j*j;
    C[j]=100.0*(PSq-JSq)*exp(-0.5*JSq/PSq)/PSq;
    if ( (-C[j]>1.0e-06) && (-C[j]<1.0) ) { JMax=j; break; }
    }
if (JMax>97) { Attention(0,"Peak Search: FWHM too large to proceed"); return; }
CSum=C[0]; for (j=1;j<JMax;j++) CSum+=2.0*C[j]; C[1]-=0.5*CSum;

/* Now calculate the significance of the second difference at each channel number and see if it exceeds Thresh */
N=0; SsPrev=0; for (i=0;i<MAX_PEAKS;i++) { Peaks[i]=0; Sig[i]=0.0; }                                                  //Initialize
for (i=JMax+5;i<Setup.Oned.Chan[SNo]-JMax-5;i++)                                                 //Avoid the edges of the spectrum
    {
    for (j=-JMax,Dd=0.0,Sd=0.0;j<=JMax;j++) { Cj=C[ABS(j)]; Counts=Get1dCounts(WinNo,i+j,FALSE,0); Dd+=Cj*Counts; Sd+=Cj*Cj*Counts; }
    if (Sd>0.0) Ss=Dd/sqrt(Sd); else Ss=0.0;
    if (Ss > Thresh)
       {
       P=FALSE; if (N) { if (i-Peaks[N-1]<Del) P=TRUE; }                                            //Already a prior peak nearby ?
       if (P) { if (Ss>SsPrev) { Peaks[N-1]=i; Sig[N-1]=Ss; SsPrev=Ss; } }                       //Yes, already a prior peak nearby
       else { Peaks[N]=i; Sig[N]=Ss; N++; SsPrev=Ss; }                                                       //No prior peak nearby
       }
    if (N==MAX_PEAKS-2) break;                                                                                     //Limit reached!
    }
if (N<NPk) { Attention(0,"Auto Calibration failed\nExpected peaks were not found"); return; }

/*Okay N peaks are located, now we pick up the NTPk most significant ones*/
NPk2=MIN(N,NTPk);                                                                                //We may not have got NTPk peaks
for (j=0;j<NPk2;j++) St[j]=0.0;
for (i=0;i<N;i++) 
    for (j=0;j<NPk2;j++) 
        if (Sig[i]>St[j]) { for (k=NPk2-1;k>j;k--) { Pk[k]=Pk[k-1]; St[k]=St[k-1]; } Pk[j]=Peaks[i]; St[j]=Sig[i]; break; }

/*From these we now select NPk peaks having the highest value of PeakCounts*/
for (i=0;i<NPk2;i++) { Peaks[i]=(gint)Pk[i]; Sig[i]=(gdouble)Get1dCounts(WinNo,Peaks[i],FALSE,0); }
for (j=0;j<NPk;j++) St[j]=0.0;
for (i=0;i<NPk2;i++) 
    for (j=0;j<NPk;j++) 
        if (Sig[i]>St[j]) { for (k=NPk-1;k>j;k--) { Pk[k]=Pk[k-1]; St[k]=St[k-1]; } Pk[j]=Peaks[i]; St[j]=Sig[i]; break; }

/*Now we use QuickFit to get the peak positions precisely*/
MaxChan=GetMaxChan(WinNo); Fit.WinNo=WinNo; Fit.NPts=25; Fit.NPeaks=1; Fit.TailOpt=None;
for (j=0;j<NPk;j++)
    {
    Lc=MAX(0,Pk[j]-12); Fit.Lc=Lc;
    for (i=0;i<Fit.NPts;i++) Fit.Data[i]=(gdouble)Get1dCounts(WinNo,Lc+i,FALSE,0);                                      //Copy data values
    Fit.P[0]=Fit.Data[0]; Fit.F[0]=Variable;                                                                             //Left Background
    Fit.P[1]=Fit.Data[Fit.NPts-1]; Fit.F[1]=Variable;                                                                   //Right Background
    for (i=0,Fit.P[3]=Lc,Fit.P[8]=0.0;i<Fit.NPts;i++)                                           //Locate maximum counts in selected region
        if ( (Fit.Data[i]>Fit.P[8]) && (i+Lc>5) ) { Fit.P[8]=Fit.Data[i]; Fit.P[3]=i+Lc; }                          //Avoid 'noise' region
    Fit.P[8]=MAX(0.0,Fit.P[8]-LinBkg(Fit.P[3]));                                   //Subtract background for proper peak height estimation
    Fit.F[3]=Variable; Fit.F[8]=Variable;
    Fit.P[5]=0.0; Fit.F[5]=Fixed;                                                                                              //Asymmetry
    PeakCounts=(guint32)Fit.P[8]; PeakCh=Fit.P[3];
    for (Ch=PeakCh;Ch>MAX(0,PeakCh-10);Ch--)                                              //Descend to the left to find the LHS half-width
        if ((Fit.Data[Ch-Fit.Lc]-LinBkg(Ch))<PeakCounts/2) break;
    Fit.P[4]=PeakCh-Ch;
    for (Ch=PeakCh;Ch<MIN(PeakCh+10,MaxChan-1);Ch++)                                     //Descend to the right to find the RHS half-width
        if ((Fit.Data[Ch-Fit.Lc]-LinBkg(Ch))<PeakCounts/2) break;
    Fit.P[4]+=(Ch-PeakCh); Fit.F[4]=Variable;                                                                                       //FWHM
    DoFit(Fit.NPts,6*Fit.NPeaks+3,BiGauss,0,NULL,Fit.Data,Fit.P,Fit.Err,Fit.F,Fit.Messg);
    Pk[j]=Fit.P[3];                                                                                //Thats the peak position
    }
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Co60Calib(GtkWidget *W,gpointer Data)                                                    //Calibration with Co60 source
{
gint WinNo;
gdouble Pk[4],St[4];                                                    //To understand the dimension see CalibrationPeaks()

if (AutoCalibOn) { Attention(0,"Auto calibration already done!"); return; }
WinNo=CalibData.WinNo; AutoCalibOn=TRUE;
CalibData.Terms=2; strcpy(Calibration[CalibData.WinNo].Units,"KeV");                                      //Override options

CalibrationPeaks(WinNo,2,4,Pk,St);                                             //Locate the 2 strongest peaks among (upto) 4
CalibData.NPts=0; AddCalibPoint(WinNo,0,1,Pk[0],1173.2); AddCalibPoint(WinNo,0,1,Pk[1],1332.5);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void AssistWinDestroyed(GtkWidget *W,gpointer Unused)
{
g_free(Assist);                                                                          //Free the memory allocated in Eu152AssistCalib
AssistWinOpen=FALSE;
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void AcceptEu152Assist(GtkWidget *W,gpointer Data)
{
gint WinNo,i,j,J;
gdouble Pk[20],St[20];                                                               //To understand the dimension see CalibrationPeaks()
gint Pk1,Pk2;
gdouble Slope,Offset,Temp,Diff;
const gdouble Eg[7]={121.782,244.692,344.275,778.903,964.131,1112.116,1408.011};

WinNo=CalibData.WinNo;
strcpy(Calibration[CalibData.WinNo].Units,"KeV");                                        //Override units option but not CalibData.Terms

Pk1=atoi(gtk_entry_get_text(GTK_ENTRY(Assist->Peak1)));
Pk2=atoi(gtk_entry_get_text(GTK_ENTRY(Assist->Peak2))); Pk2=MAX(Pk2,Pk1+1);                                              //A precaution
Slope=(344.275-244.692)/(Pk2-Pk1); Offset=244.692-Slope*Pk1;
if ( (Slope<1.0e-06) || (Slope>1.0e+06) ){  Attention(20,"Assisted calibration failed.\nNot a Eu152 spectrum?"); return; }

CalibrationPeaks(WinNo,12,20,Pk,St);                   //Locate the 12 strongest peaks among (upto) 20. Peaks are ordered by peak counts

do                                                                                                          //Arrange in ascending order
   { 
   for (i=1,j=0;i<12;i++) if (Pk[i]<Pk[i-1]) { Temp=Pk[i]; Pk[i]=Pk[i-1]; Pk[i-1]=Temp; j=1; } 
   } while (j);

for (i=0,CalibData.NPts=0;i<7;++i)                                     //For each calibration point, find nearest peak
    {
    for (j=0,J=0,Temp=1.0e6;j<12;++j)
        {
        Diff=fabs(Eg[i]-Slope*Pk[j]-Offset);
        if (Diff<Temp) { J=j; Temp=Diff; }
        }
    AddCalibPoint(WinNo,0,1,Pk[J],Eg[i]);
    }
gtk_widget_destroy(Assist->Win);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void Eu152AssistCalib(GtkWidget *W,gpointer Data)                         //Semi Automatic Calibration with Eu152 source
{
GtkWidget *VBox,*Label,*HBox,*But;

if (AutoCalibOn) { Attention(0,"For assisted calibration, re-start Calibration"); return; }
if (SemiAutoCalibOn) { Attention(0,"Assisted calibration already done!"); return; }
SemiAutoCalibOn=TRUE; AssistWinOpen=TRUE;

//Let user click near 121.7 KeV and 344.2 KeV peaks. Calculate approximate calibration from this.
Assist=g_new(struct Assist,1); Assist->PkNo=0;
Assist->Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(Assist->Win),"Assisted Calibration");
gtk_widget_set_uposition(Assist->Win,544,397);
gtk_window_set_transient_for(GTK_WINDOW(Assist->Win),GTK_WINDOW(SpecWin[CalibData.WinNo]));        //Window visibility
gtk_signal_connect(GTK_OBJECT(Assist->Win),"destroy",GTK_SIGNAL_FUNC(AssistWinDestroyed),Assist->Win);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Assist->Win),VBox);
Label=gtk_label_new("Click middle button in spectrum window\nnear 244.6 and 344.2 KeV peaks"); 
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Ch# of 244.6 KeV"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0); 
Assist->Peak1=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(Assist->Peak1,80,22);
gtk_entry_set_text(GTK_ENTRY(Assist->Peak1),"114"); gtk_box_pack_start(GTK_BOX(HBox),Assist->Peak1,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Ch# of 344.2 KeV"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0); 
Assist->Peak2=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(Assist->Peak2,80,22);
gtk_entry_set_text(GTK_ENTRY(Assist->Peak2),"195"); gtk_box_pack_start(GTK_BOX(HBox),Assist->Peak2,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Accept"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AcceptEu152Assist),Assist->Win);
gtk_widget_show_all(Assist->Win);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void Eu152Calib(GtkWidget *W,gpointer Data)                                              //Calibration with Eu152 source
{
gint WinNo,i,j,J;
gdouble Pk[20],St[20];                                               //To understand the dimension see CalibrationPeaks()
gdouble Slope,Offset,Temp,Diff,BestOffset,BestSlope;
const gdouble Eg[7]={121.782,244.692,344.275,778.903,964.131,1112.116,1408.011};

if (AutoCalibOn) { Attention(0,"Auto calibration already done!"); return; }
WinNo=CalibData.WinNo; AutoCalibOn=TRUE;
strcpy(Calibration[CalibData.WinNo].Units,"KeV");                         //Override units option but not CalibData.Terms

CalibrationPeaks(WinNo,12,20,Pk,St);    //Locate the 12 strongest peaks among (upto) 20. Peaks are ordered by peak counts

do                                                  //The peaks are in order of peak counts, rearrange in ascending order
   { 
   for (i=1,j=0;i<12;i++) if (Pk[i]<Pk[i-1]) { Temp=Pk[i]; Pk[i]=Pk[i-1]; Pk[i-1]=Temp; j=1; } 
   } while (j);

/* Algorithm for EXOGAM but should work in general too
 * We work with 244 and 344 KeV lines (121 could be attenuated).
 * We try successively Pk[0],Pk[1]; Pk[0],Pk[2] then Pk[1],Pk[2];Pk[1],Pk[3] ... to match 244 and 344 KeV lines
 * And select that which gives the smallest abs(Offset) 
 * Unfortunately for EXOGAM_20MeV spectra the offset is actually very large and the algorithm fails
 * In this case use the Assist calibration method  
 */ 
for (i=0,BestOffset=1.0e5;i<5;++i)
    {
    Temp=Pk[i+1]-Pk[i]; if (Temp==0.0) Temp=1.0e-3; Slope=(344.275-244.692)/Temp; Offset=244.692-Slope*Pk[i];
    if (fabs(Offset)<fabs(BestOffset)) { BestOffset=Offset; BestSlope=Slope; }
    Temp=Pk[i+2]-Pk[i]; if (Temp==0.0) Temp=1.0e-3; Slope=(344.275-244.692)/Temp; Offset=244.692-Slope*Pk[i];
    if (fabs(Offset)<fabs(BestOffset)) { BestOffset=Offset; BestSlope=Slope; }
    }

for (i=0,CalibData.NPts=0;i<7;++i)                                     //For each calibration point, find nearest peak
    {
    for (j=0,J=0,Temp=1.0e6;j<12;++j)
        {
        Diff=fabs(Eg[i]-BestSlope*Pk[j]-BestOffset);
        if (Diff<Temp) { J=j; Temp=Diff; }
        }
    AddCalibPoint(WinNo,0,1,Pk[J],Eg[i]);
    }
}
/*--------------------------------------------------------------------------------------------------------------------*/
void Calibrate(GtkWidget *W,gpointer Data)
{
gint i,WinNo,MaxChan,SNo,SType,Par,R;
static GdkColor GreenC  = {0,0x0000,0x5555,0x0000};
static GdkColor WhiteC  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor RedC    = {0,0xFFFF,0x0000,0x0000};
GtkStyle *Style1;
GtkStyle *Style2;
GtkWidget *CalibWin,*LBox,*Combo,*HBox,*Label,*But,*VBox2,*RBox,*Box,*Frame;
GList *GList;
gchar Str[120];

if (CalibWinOpen||AreaWinOpen||OvWinOpen||FitWinOpen||PkSearchOpen) 
   { Attention(0,"Another context window open\nPlease close it first"); return; }

WinNo=GPOINTER_TO_INT(Data);
SNo=ScreenSpecNo[WinNo][Screen]; SType=ScreenSpecType[WinNo][Screen];
switch (SType)
   {
   case 1: Par=Setup.Oned.Par[SNo]; break;  //1d spectrum
   case 3: Par=Setup.Twod.XPar[SNo]; break; //XProj of 2d
   case 4: Par=Setup.Twod.YPar[SNo];        //YProj of 2d
   }
if (Par==0) { Attention(0,"Cant calibrate Hit Pattern spectrum"); return; }

AutoCalibOn=FALSE;                                              //This flag prevents multiple invokation of AutoCalibration
SemiAutoCalibOn=FALSE;                                     //This flag prevents multiple invokation of Assisted Calibration

Style1=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { Style1->fg[i]=Style1->text[i]=WhiteC; Style1->bg[i]=GreenC; }
Style2=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { Style2->fg[i]=Style2->text[i]=RedC; Style2->bg[i]=GreenC; }

MaxChan=GetMaxChan(WinNo); CalibData.WinNo=WinNo;
CalibWinOpen=TRUE; CalibData.NPts=0;
CalibWin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(CalibWin),"Calibration Window"); gtk_widget_set_size_request(CalibWin,-1,355);
gtk_widget_set_uposition(CalibWin,0,397);
gtk_window_set_transient_for(GTK_WINDOW(CalibWin),GTK_WINDOW(SpecWin[WinNo]));                          //Window visibility
gtk_signal_connect(GTK_OBJECT(CalibWin),"destroy",GTK_SIGNAL_FUNC(CalibWinDestroyed),CalibWin);
gtk_container_set_border_width(GTK_CONTAINER(CalibWin),5);

Box=gtk_hbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(CalibWin),Box);
LBox=gtk_vbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(Box),LBox,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(LBox),HBox,FALSE,FALSE,0);
R=Setup.Parameter.Chan[Par-1]/MaxChan;
switch (SType)
   {
   case 1: sprintf(Str,"Calib. 1d Spec# %d (%d chans) Para# %d (%d chans) Factor=%d",SNo+1,MaxChan,Par,Setup.Parameter.Chan[Par-1],R);
           break;                                                                                             //1d spectrum
   case 3: sprintf(Str,"Calib. XProj. (%d chans) Para# %d (%d chans) Factor=%d",MaxChan,Par,Setup.Parameter.Chan[Par-1],R);
           break;                                                                                             //XProj of 2d
   case 4: sprintf(Str,"Calib. YProj. (%d chans) Para# %d (%d chans) Factor=%d",MaxChan,Par,Setup.Parameter.Chan[Par-1],R);
                                                                                                                              //YProj of 2d
   }
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(LBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Type:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Combo=gtk_combo_new(); gtk_widget_set_size_request(Combo,125,-1);
gtk_box_pack_start(GTK_BOX(HBox),Combo,FALSE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Linear"); GList=g_list_append(GList,"Quadratic"); GList=g_list_append(GList,"Quad+Sqrt");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Quadratic"); CalibData.Terms=3;
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(CalibTerms),NULL);

Label=gtk_label_new("Units:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Combo=gtk_combo_new(); gtk_widget_set_size_request(Combo,80,-1);
gtk_box_pack_start(GTK_BOX(HBox),Combo,FALSE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"MeV"); GList=g_list_append(GList,"KeV"); GList=g_list_append(GList,"eV");
GList=g_list_append(GList,"ps"); GList=g_list_append(GList,"ns"); GList=g_list_append(GList,"us"); GList=g_list_append(GList,"ms");
GList=g_list_append(GList,"cm"); GList=g_list_append(GList,"m"); GList=g_list_append(GList,"Km");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"KeV"); strcpy(Calibration[CalibData.WinNo].Units,"KeV");
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(CalibUnits),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(LBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Use mouse middle button on spectrum window to add calibration points"); 
SetStyleRecursively(Label,Style2);
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);

VBox2=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(LBox),VBox2,TRUE,TRUE,0);
HBox=gtk_hbox_new(FALSE,1); gtk_box_pack_start(GTK_BOX(VBox2),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("No"); SetStyleRecursively(But,Style1); gtk_widget_set_size_request(But,26,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Spec Ch"); SetStyleRecursively(But,Style1); gtk_widget_set_size_request(But,93,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Value"); SetStyleRecursively(But,Style1); gtk_widget_set_size_request(But,95,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Fit Val"); SetStyleRecursively(But,Style1); gtk_widget_set_size_request(But,95,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);

CalibData.ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(CalibData.ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox2),CalibData.ScrlW,TRUE,TRUE,0);
CalibData.Table=gtk_table_new(0,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(CalibData.ScrlW),CalibData.Table); 

CalibData.Output=gtk_label_new("Click (Re) Do Calibration\nTo start calibration"); 
SetStyleRecursively(CalibData.Output,Style2);
gtk_box_pack_start(GTK_BOX(LBox),CalibData.Output,FALSE,FALSE,0);

RBox=gtk_vbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(Box),RBox,FALSE,FALSE,0);
Frame=gtk_frame_new("Auto Calibration"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
SetStyleRecursively(Frame,Style2); gtk_box_pack_start(GTK_BOX(RBox),Frame,FALSE,FALSE,0);
VBox2=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Frame),VBox2);
gtk_container_set_border_width(GTK_CONTAINER(VBox2),10);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Co60"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Co60Calib),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Assist"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Eu152"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Eu152Calib),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Assist"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Eu152AssistCalib),GINT_TO_POINTER(WinNo));

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Cs137"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
But=gtk_button_new_with_label("Assist"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Na24"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
But=gtk_button_new_with_label("Assist"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Am-Pu"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
But=gtk_button_new_with_label("Assist"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(LBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("(Re) Do\nCalibration"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DoCalibration),NULL);
But=gtk_button_new_with_label("Save\nCalibration"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveCalib),NULL);
But=gtk_button_new_with_label("Dismiss"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),CalibWin);

gtk_widget_show_all(CalibWin);
gtk_style_unref(Style1); gtk_style_unref(Style2); g_list_free(GList);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
gdouble Energy(gint PNo,gdouble Ch)
/*Calculates calibrated Energy at a given absolute channel number (i.e without the scaling applied in a spectrum definition)
  For convenience in use along with peak fit, centroid etc, Ch is made gdouble rather than gint                             */
{
return Calibration[PNo-1].P[0]+Calibration[PNo-1].P[1]*Ch+Calibration[PNo-1].P[2]*Ch*Ch+Calibration[PNo-1].P[3]*sqrt(Ch);
} 
/*-----------------------------------------------------------------------------------------------------------------------*/
gdouble PeakFunc(gint i,gdouble *P,gint N)                                                 //Gives the value of the Nth peak at channel i
{
gint J;
gdouble Pos,kL,kR,kT;

J=6*N; Pos=P[J+3]-Fit.Lc;
kL=TSL2/(P[J+4]+P[J+5]); kR=TSL2/(P[J+4]-P[J+5]);
if (((Fit.TailOpt==Left)||(Fit.TailOpt==Both)) && (i<(Pos-P[J+6])))
   {
   kT=-2.0*kL*kL*P[J+6];
   return P[J+8]*GX(kL*P[J+6])*exp(kT*(Pos-P[J+6]-i));
   }
else if (((Fit.TailOpt==Right)||(Fit.TailOpt==Both)) && (i>(Pos+P[J+7])))
   {
   kT=2.0*kR*kR*P[J+7];
   return P[J+8]*GX(kR*P[J+7])*exp(kT*(Pos+P[J+7]-i));   
   }
else
  if (i<P[J+3]) return P[J+8]*GX(kL*(i-Pos));
  else          return P[J+8]*GX(kR*(i-Pos));
}
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
gdouble Background(gint i,gdouble *P,gdouble *X)                                              //Background in peakfit (used only for drawing)
{
gdouble Temp,Bkg;

Temp=i+Fit.Lc; Bkg=P[0]+(P[1]-P[0])*i/(Fit.NPts-1.0)+P[2]*Temp*Temp;
return Bkg;
}
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
gdouble BiGauss(gint i,gdouble *P,gdouble *X)                                                             //The Tailed-BiGaussian function
// Here i is the data point number=ChNo-Fit.Lc. The variable X is unused as in this case X is just i itself
// As the fit program is quite general we include the variable X also
// P[0]=Bgk left, P[1]=Bgk right, P[1]=Bkg quad,
// P[6N+3]=Pos, P[6N+4]=FWHM, P[6N+5]=Asymmetry, P[6N+6]=L Tail, P[6N+7]=R Tail, P[6N+8]=Height
{
gint N,J;
gdouble F,Bkg,kL,kR,kT,Pos,Temp;

Temp=i+Fit.Lc; Bkg=P[0]+(P[1]-P[0])*i/(Fit.NPts-1.0)+P[2]*Temp*Temp;                                                           //Background
for (N=0,F=0.0;N<Fit.NPeaks;N++)                                                                                          //Loop over peaks
    {
    J=6*N; Pos=P[J+3]-Fit.Lc;                                                                        //There are 6 parameters for each peak
    kL=TSL2/(P[J+4]+P[J+5]); kR=TSL2/(P[J+4]-P[J+5]);
    if (((Fit.TailOpt==Left)||(Fit.TailOpt==Both)) && (i<(Pos-P[J+6])))                                                       //The LH Tail
       {
       kT=-2.0*kL*kL*P[J+6];
       F+=P[J+8]*GX(kL*P[J+6])*exp(kT*(Pos-P[J+6]-i));
       }
    else if (((Fit.TailOpt==Right)||(Fit.TailOpt==Both)) && (i>(Pos+P[J+7])))                                                 //The RH Tail
       {
       kT=2.0*kR*kR*P[J+7];
       F+=P[J+8]*GX(kR*P[J+7])*exp(kT*(Pos+P[J+7]-i));
       }
    else
       if (i<P[J+3]) F+=P[J+8]*GX(kL*(i-Pos));
       else          F+=P[J+8]*GX(kR*(i-Pos));
    }
return (Bkg+F);
}
/*---------------------------------------------------------------------------------------------------------------------*/
guint32 Get1dCounts(gint WinNo,gint Ch,gboolean Overlap,gint OvSpec)      
//For 1d & projection Overlap=FALSE, for overlapped spectrum Overlap=TRUE and we must provide the OvSpec number
{
gint SNo;
guint32 Counts;

if (!Overlap) SNo=ScreenSpecNo[WinNo][Screen]; else SNo=OvSpec;
if (ScreenSpecType[WinNo][Screen] == 1)                                                                     /* 1d spec*/
   {
   if ( (Ch<0) || (Ch>=Setup.Oned.Chan[SNo]) ) return 0;                 /*This happens for shifted overlapped spectra*/
   Counts=Setup.Oned.WSz==1 ? (gfloat)Oned16[Off1d[SNo]+Ch] : (gfloat)Oned32[Off1d[SNo]+Ch];
   }
else                                                                                                  /* 2d projection*/
   Counts=(gfloat)Proj[OffProj[SNo]+Ch];
return Counts;
}
/*--------------------------------------------------------------------------------------------------------------------*/
gint GetMaxChan(WinNo)
{
gint SNo,SType;

SNo=ScreenSpecNo[WinNo][Screen]; SType=ScreenSpecType[WinNo][Screen];
switch (SType)
   {
   case 1:  return Setup.Oned.Chan[SNo];               /*1d Spectra*/
   case 3:  return Setup.Twod.XChan[SNo];              /*2d X projection*/
   case 4:  return Setup.Twod.YChan[SNo];              /*2d Y projection*/
   default: return 4;                                  /*Should never come here!*/ 
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DrawBox(GtkWidget *W,gint WinNo)
{
GdkRectangle UpdateRect;
gint OriginY,LenY,MaxChan,PNo,R,Ix;
gchar Str[40];
guint32 Counts;
gdouble Ch;

switch (ScreenSpecType[WinNo][Screen])
   {
   case 1: PNo=Setup.Oned.Par[ScreenSpecNo[WinNo][Screen]]; break;  //1d Spectra
   case 3: PNo=Setup.Twod.XPar[ScreenSpecNo[WinNo][Screen]]; break; //2d X-Projection
   case 4: PNo=Setup.Twod.YPar[ScreenSpecNo[WinNo][Screen]]; break; //2d Y-Projection
   }

MaxChan=GetMaxChan(WinNo); if (PNo>0) R=Setup.Parameter.Chan[PNo-1]/MaxChan; else R=1;
OriginY=ORIGIN_Y(CanvasH[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);

/*First we need to erase Prev box from the pixmap used as a backing store*/
UpdateRect.x=Box[WinNo].X1; UpdateRect.y=OriginY-LenY;
UpdateRect.width=1; UpdateRect.height=LenY+1;
gtk_widget_draw(W,&UpdateRect);
UpdateRect.x=Box[WinNo].X2P; UpdateRect.y=OriginY-LenY;
UpdateRect.width=1; UpdateRect.height=LenY+1;
gtk_widget_draw(W,&UpdateRect);

/*Also we need to erase the rectangle where the dialog 2nd line appears from the same backing pixmap */
UpdateRect.x=0; UpdateRect.y=CanvasH[WinNo]-FONT_HEIGHT-7; UpdateRect.width=CanvasW[WinNo]; UpdateRect.height=FONT_HEIGHT+7;
gtk_widget_draw(W,&UpdateRect);

/*Then we need to draw current box*/
gdk_draw_line(W->window,W->style->black_gc,Box[WinNo].X1,OriginY,Box[WinNo].X1,OriginY-LenY);
gdk_draw_line(W->window,W->style->black_gc,Box[WinNo].X2,OriginY,Box[WinNo].X2,OriginY-LenY);
Prop[WinNo].One.BoxX2Ch=XSlope[WinNo]>0 ? (gint)(0.5+(gfloat)Prop[WinNo].One.XLo+(gfloat)((Box[WinNo].X2-ORIGIN_X)/XSlope[WinNo])) 
                               : Setup.Oned.Chan[ScreenSpecNo[WinNo][Screen]];
Prop[WinNo].One.BoxX2Ch=MIN(Prop[WinNo].One.BoxX2Ch,MaxChan-1);
Counts=Get1dCounts(WinNo,Prop[WinNo].One.BoxX1Ch,FALSE,0);
Ch=Prop[WinNo].One.BoxX1Ch*R;
if (PNo>0) sprintf(Str,"%d (%f %s) %d",Prop[WinNo].One.BoxX1Ch,Energy(PNo,Ch),Calibration[PNo-1].Units,Counts);
else       
   {
   Ix=Prop[WinNo].One.BoxX1Ch; 
   if (Ix<Setup.Parameter.NPar) sprintf(Str,"%s Hits=%d",Setup.Parameter.Name[Ix],Counts); else strcpy(Str,"");
   }
gdk_draw_string(W->window,Font,W->style->black_gc,0,CanvasH[WinNo]-FONT_HEIGHT-9,Str);                //Write dialog line
Counts=Get1dCounts(WinNo,Prop[WinNo].One.BoxX2Ch,FALSE,0);
Ch=Prop[WinNo].One.BoxX2Ch*R;
if (PNo>0) sprintf(Str,"%d (%f %s) %d",Prop[WinNo].One.BoxX2Ch,Energy(PNo,Ch),Calibration[PNo-1].Units,Counts);
else 
   {
   Ix=Prop[WinNo].One.BoxX2Ch; 
   if (Ix<Setup.Parameter.NPar) sprintf(Str,"%s Hits=%d",Setup.Parameter.Name[Ix],Counts); else strcpy(Str,"");
   }
gdk_draw_string(W->window,Font,W->style->black_gc,0,CanvasH[WinNo]-2,Str);                            //Write dialog line
Prop[WinNo].One.BoxDrawn=1;
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void PChanged(GtkWidget *W,gpointer Data)
{ 
gint i;

i=GPOINTER_TO_INT(Data);                                                                                         //i is the index of Fit.P
Fit.P[i]=atof(gtk_entry_get_text(GTK_ENTRY(W))); 
}
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void FToggle(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);                                                                                         //i is the index of Fit.F
if (Fit.F[i]==Variable) { Fit.F[i]=Fixed;    gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Fixed"); }
else                    { Fit.F[i]=Variable; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Variable"); }
}
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void FClick(GtkWidget *W,gpointer Data)
{
gint i,ParaType,PeakNo;

i=GPOINTER_TO_INT(Data);
ParaType=(i-3)%6; PeakNo=(i-3)/6;                                                           /*There are 3 Bkg paras and 6 paras per peak*/
switch (ParaType)
   {
   case 0: case 5:                               /*Posn and Height: In both cases the Flag can take on only 2 values, Fixed and Variable*/
           if (Fit.F[i]==Variable) { Fit.F[i]=Fixed;    gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"F"); }
           else                    { Fit.F[i]=Variable; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"V"); }
           break;
   case 1: case 2: case 3: case 4:                                                                         /*FWHM, Asym, LTail and RTail*/
           if (Fit.ShapeOpt==ShapeCal) { Fit.F[i]=Fixed; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"F"); }
           else
              {
              if (PeakNo==0)  /*The first peak: here only Variable and Fixed are possible*/
                 {
                 if (Fit.F[i]==Variable) { Fit.F[i]=Fixed;    gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"F"); }
                 else                    { Fit.F[i]=Variable; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"V"); }
                 }
              else
                 {
                 if (Fit.ShapeOpt==Same) { Fit.F[i]=Constrained; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"C"); }
                 else 
                    {
                    if (Fit.F[i]==Variable) { Fit.F[i]=Fixed;    gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"F"); }
                    else                    { Fit.F[i]=Variable; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"V"); }
                    }
                 }
              }
           break;  
   }
/*We have also to worry about TailOpt. This can override LTail and RTail*/
if ( (ParaType==3) && ((Fit.TailOpt==None) || (Fit.TailOpt==Right)) )                                                 /*Switch off LTail*/
   { Fit.F[i]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"N"); }
if ( (ParaType==4) && ((Fit.TailOpt==None) || (Fit.TailOpt==Left)) )                                                  /*Switch off RTail*/
   { Fit.F[i]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"N"); }
}
//----------------------------------------------------------------------------------------------------------------------
gfloat LinBkg(gint Ch)
//Computes a linear estimate of the background for initial peak width estimate in CanvasEvent1 below
//This function is not intended for any other use
{
//It has already been ensured in FitWidget that Fit.NPts>=6
return MAX(0,Fit.P[0]+(Fit.P[1]-Fit.P[0])*(Ch-Fit.Lc)/(Fit.NPts-1));
}
//----------------------------------------------------------------------------------------------------------------------
void AddPeak(gint WinNo,gint PeakX)
{
gint i,PeakCh,Ch,MaxChan,OriginY,YPix;
guint32 PeakCounts;
GtkWidget *Label,*PosnEntry,*PosnBut,*FWHMEntry,*FWHMBut,*AsymEntry,*AsymBut,*LTailEntry,*LTailBut,*RTailEntry,
          *RTailBut,*HeightEntry,*HeightBut;
gchar Str[40];

MaxChan=GetMaxChan(WinNo);
if (!FitWinOpen || (Fit.WinNo!=WinNo) || (Fit.NPeaks>=MAX_PEAKS)) return;       //Ensure click is in relevant window etc
PeakCh=XSlope[WinNo]>0 ? (gint)(0.5+(gfloat)Prop[WinNo].One.XLo+(gfloat)((PeakX-ORIGIN_X)/XSlope[WinNo])) : 0; 
if ( (PeakCh<=Fit.Lc) || (PeakCh>=Fit.Lc+Fit.NPts+1) ) return;           //Ensure the user has clicked between Lc and Rc
PeakX=XSlope[WinNo]*(0.5+(gfloat)(PeakCh-Prop[WinNo].One.XLo))+(gfloat)ORIGIN_X;                     //Recalculate PeakX

gdk_gc_set_rgb_fg_color(Gc,&Colour1D[6]);                                               //Begin code to draw peak marker
OriginY=ORIGIN_Y(CanvasH[WinNo]);
if (Prop[WinNo].One.CountsLog)
   YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                  MAX(0.0,log((gdouble)Fit.Data[PeakCh-Fit.Lc])-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
else 
   YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                  MAX(0,(guint32)Fit.Data[PeakCh-Fit.Lc]-Prop[WinNo].One.CountsLo));
gdk_draw_line(PixMap[WinNo],Gc,PeakX,YPix+1,PeakX,OriginY);
gdk_draw_pixmap(Canvas[WinNo]->window,Gc,PixMap[WinNo],PeakX,YPix+1,
                PeakX,YPix+1,1,OriginY-YPix);                                          //Update only the required pixels

Fit.NPeaks++;
gtk_table_resize(GTK_TABLE(Fit.Table2),Fit.NPeaks,13);
i=Fit.NPeaks-1;
sprintf(Str,"%02d",i+1); Label=gtk_label_new(Str);                                                   //Peak number label
gtk_table_attach(GTK_TABLE(Fit.Table2),Label,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Fit.P[6*i+3]=(gfloat)PeakCh;                                                                             //Peak Position
PosnEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(PosnEntry,80,-1);
sprintf(Str,"%.3f",Fit.P[6*i+3]); gtk_entry_set_text(GTK_ENTRY(PosnEntry),Str);
gtk_signal_connect(GTK_OBJECT(PosnEntry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+3));
gtk_table_attach(GTK_TABLE(Fit.Table2),PosnEntry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

PosnBut=gtk_button_new_with_label("V"); Fit.F[6*i+3]=Variable;                                      //Peak Position Flag
gtk_signal_connect(GTK_OBJECT(PosnBut),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+3));
gtk_table_attach(GTK_TABLE(Fit.Table2),PosnBut,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

FWHMEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(FWHMEntry,80,-1);            //FWHM
PeakCounts=(guint32)MAX(0.0,Fit.Data[PeakCh-Fit.Lc]-LinBkg(PeakCh));
for (Ch=PeakCh;Ch>MAX(0,PeakCh-10);Ch--)                                //Descend to the left to find the LHS half-width
    if ((Fit.Data[Ch-Fit.Lc]-LinBkg(Ch))<PeakCounts/2) break;
Fit.P[6*i+4]=PeakCh-Ch;
for (Ch=PeakCh;Ch<MIN(PeakCh+10,MaxChan-1);Ch++)                       //Descend to the right to find the RHS half-width
    if ((Fit.Data[Ch-Fit.Lc]-LinBkg(Ch))<PeakCounts/2) break;
Fit.P[6*i+4]+=(Ch-PeakCh);
Fit.P[6*i+4]=MAX(3.0,Fit.P[6*i+4]);                                                 //Clamp to smallest meaningful value
gtk_signal_connect(GTK_OBJECT(FWHMEntry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+4));
gtk_table_attach(GTK_TABLE(Fit.Table2),FWHMEntry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

FWHMBut=gtk_button_new_with_label(".");                                                                      //FWHM Flag
gtk_signal_connect(GTK_OBJECT(FWHMBut),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+4));
gtk_table_attach(GTK_TABLE(Fit.Table2),FWHMBut,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

AsymEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(AsymEntry,80,-1);            //Asym
Fit.P[6*i+5]=0.0;
sprintf(Str,"%.3f",Fit.P[6*i+5]); gtk_entry_set_text(GTK_ENTRY(AsymEntry),Str);
gtk_signal_connect(GTK_OBJECT(AsymEntry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+5));
gtk_table_attach(GTK_TABLE(Fit.Table2),AsymEntry,5,6,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

AsymBut=gtk_button_new_with_label("F"); Fit.F[6*i+5]=Fixed;                                                  //Asym Flag
gtk_signal_connect(GTK_OBJECT(AsymBut),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+5));
gtk_table_attach(GTK_TABLE(Fit.Table2),AsymBut,6,7,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

LTailEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(LTailEntry,80,-1);        //L.Tail
Fit.P[6*i+6]=10.0; 
sprintf(Str,"%.3f",Fit.P[6*i+6]); gtk_entry_set_text(GTK_ENTRY(LTailEntry),Str);
gtk_signal_connect(GTK_OBJECT(LTailEntry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+6));
gtk_table_attach(GTK_TABLE(Fit.Table2),LTailEntry,7,8,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

LTailBut=gtk_button_new_with_label(".");                                                                   //L.Tail Flag
gtk_signal_connect(GTK_OBJECT(LTailBut),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+6));
gtk_table_attach(GTK_TABLE(Fit.Table2),LTailBut,8,9,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

RTailEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(RTailEntry,80,-1);        //R.Tail
Fit.P[6*i+7]=10.0;
sprintf(Str,"%.3f",Fit.P[6*i+7]); gtk_entry_set_text(GTK_ENTRY(RTailEntry),Str);
gtk_signal_connect(GTK_OBJECT(RTailEntry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+7));
gtk_table_attach(GTK_TABLE(Fit.Table2),RTailEntry,9,10,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

RTailBut=gtk_button_new_with_label(".");                                                                   //R.Tail Flag
gtk_signal_connect(GTK_OBJECT(RTailBut),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+7));
gtk_table_attach(GTK_TABLE(Fit.Table2),RTailBut,10,11,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

if (Fit.ShapeOpt==ShapeCal)                                              //Get FWHM, LTail, RTail from Shape Calibration
   { 
   Fit.P[6*i+4]=1.0; /*Actually: Get FWHM from shape calibration*/                                          //FWHM value
   sprintf(Str,"%.3f",Fit.P[6*i+4]); gtk_entry_set_text(GTK_ENTRY(FWHMEntry),Str);
   Fit.F[6*i+4]=Fixed; gtk_label_set_text(GTK_LABEL(GTK_BIN(FWHMBut)->child),"F");                           //FWHM flag
   Fit.P[6*i+6]=11.0;  /*Actually: Get LTail from shape calibration*/                                      //LTail value
   sprintf(Str,"%.3f",Fit.P[6*i+6]); gtk_entry_set_text(GTK_ENTRY(LTailEntry),Str);
   if ( (Fit.TailOpt==Left) || (Fit.TailOpt==Both) )                                                        //Ltail flag
      { Fit.F[6*i+6]=Fixed;         gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"F"); }
   else
      { Fit.F[6*i+6]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"N"); }
   Fit.P[6*i+7]=11.0;  /*Actually: Get RTail from shape calibration*/                                      //RTail value
   sprintf(Str,"%.3f",Fit.P[6*i+7]); gtk_entry_set_text(GTK_ENTRY(RTailEntry),Str);
   if ( (Fit.TailOpt==Right) || (Fit.TailOpt==Both) ) 
      { Fit.F[6*i+7]=Fixed;         gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"F"); }          //RTail flag
   else
      { Fit.F[6*i+7]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"N"); }
   } 
else                                                                             //Shapes are not from Shape Calibration
   {
   if (i==0)                                                                                            //The first peak
      { 
      sprintf(Str,"%.3f",Fit.P[6*i+4]); gtk_entry_set_text(GTK_ENTRY(FWHMEntry),Str);                       //FWHM value
      Fit.F[6*i+4]=Variable; gtk_label_set_text(GTK_LABEL(GTK_BIN(FWHMBut)->child),"V");                     //FWHM flag
      sprintf(Str,"%.3f",Fit.P[6*i+6]); gtk_entry_set_text(GTK_ENTRY(LTailEntry),Str);                     //LTail value
      if (( Fit.TailOpt==Left) || (Fit.TailOpt==Both) )                                                     //LTail flag
         { Fit.F[6*i+6]=Variable;      gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"V"); }
      else                                          
         { Fit.F[6*i+6]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"N"); }
      sprintf(Str,"%.3f",Fit.P[6*i+7]); gtk_entry_set_text(GTK_ENTRY(RTailEntry),Str);                     //RTail value
      if ( (Fit.TailOpt==Right) || (Fit.TailOpt==Both) )                                                    //RTail flag
         { Fit.F[6*i+7]=Variable;      gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"V"); }
      else                                          
         { Fit.F[6*i+7]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"N"); }
      }
   else                                                                                  //All peaks after the first one
      {
      if (Fit.ShapeOpt==Same)                                                            //Shapes set same for all peaks
         { 
         Fit.P[6*i+4]=Fit.P[4];                                                                             //FWHM value
         sprintf(Str,"%.3f",Fit.P[6*i+4]); gtk_entry_set_text(GTK_ENTRY(FWHMEntry),Str);
         Fit.F[6*i+4]=Constrained; gtk_label_set_text(GTK_LABEL(GTK_BIN(FWHMBut)->child),"C");               //FWHM flag
         Fit.P[6*i+5]=Fit.P[5];                                                                             //Asym value
         sprintf(Str,"%.3f",Fit.P[6*i+5]); gtk_entry_set_text(GTK_ENTRY(AsymEntry),Str);
         Fit.F[6*i+5]=Constrained; gtk_label_set_text(GTK_LABEL(GTK_BIN(AsymBut)->child),"C");               //FWHM flag
         Fit.P[6*i+6]=Fit.P[6];                                                                            //LTail value
         sprintf(Str,"%.3f",Fit.P[6*i+6]); gtk_entry_set_text(GTK_ENTRY(LTailEntry),Str);
         if ( (Fit.TailOpt==Left) || (Fit.TailOpt==Both) )                                                  //LTail flag
            { Fit.F[6*i+6]=Constrained;   gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"C"); }
         else                                          
            { Fit.F[6*i+6]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"N"); }
         Fit.P[6*i+7]=Fit.P[7];                                                                            //RTail value
         sprintf(Str,"%.3f",Fit.P[6*i+7]); gtk_entry_set_text(GTK_ENTRY(RTailEntry),Str);
         if ( (Fit.TailOpt==Right) || (Fit.TailOpt==Both) )                                                 //RTail flag
            { Fit.F[6*i+7]=Constrained;   gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"C"); }
         else                                          
            { Fit.F[6*i+7]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"N"); }
         }
      else                                                                                      //Shapes are independent
         { 
         sprintf(Str,"%.3f",Fit.P[6*i+4]); gtk_entry_set_text(GTK_ENTRY(FWHMEntry),Str);                    //FWHM value
         Fit.F[6*i+4]=Variable; gtk_label_set_text(GTK_LABEL(GTK_BIN(FWHMBut)->child),"V");                  //FWHM flag
         sprintf(Str,"%.3f",Fit.P[6*i+6]); gtk_entry_set_text(GTK_ENTRY(LTailEntry),Str);                  //LTail value
         if ((Fit.TailOpt==Left)||(Fit.TailOpt==Both)) 
            { Fit.F[6*i+6]=Variable;      gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"V"); }    //LTail flag
         else                                          
            { Fit.F[6*i+6]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(LTailBut)->child),"N"); }
         sprintf(Str,"%.3f",Fit.P[6*i+7]); gtk_entry_set_text(GTK_ENTRY(RTailEntry),Str);                  //RTail value
         if ( (Fit.TailOpt==Right) || (Fit.TailOpt==Both) )                                                 //RTail flag
            { Fit.F[6*i+7]=Variable;      gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"V"); }
         else                                          
            { Fit.F[6*i+7]=NotApplicable; gtk_label_set_text(GTK_LABEL(GTK_BIN(RTailBut)->child),"N"); }
         }
      }
   }

HeightEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(HeightEntry,80,-1);      //Height
Fit.P[6*i+8]=(gfloat)PeakCounts;
sprintf(Str,"%.3f",Fit.P[6*i+8]); gtk_entry_set_text(GTK_ENTRY(HeightEntry),Str);
gtk_signal_connect(GTK_OBJECT(HeightEntry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+8));
gtk_table_attach(GTK_TABLE(Fit.Table2),HeightEntry,11,12,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

HeightBut=gtk_button_new_with_label("V");  Fit.F[6*i+8]=Variable;                                          //Height Flag
gtk_signal_connect(GTK_OBJECT(HeightBut),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+8));
gtk_table_attach(GTK_TABLE(Fit.Table2),HeightBut,12,13,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
gtk_widget_show_all(Fit.Table2);
}
//----------------------------------------------------------------------------------------------------------------------
void FlagStr(gint I,gchar *Str)
{
switch (Fit.F[I])
   {
   case Variable:      strcpy(Str,"V"); break;
   case Fixed:         strcpy(Str,"F"); break;
   case Constrained:   strcpy(Str,"C"); break;
   case NotApplicable: strcpy(Str,"N"); break;
   }
}
//----------------------------------------------------------------------------------------------------------------------
void PopulateFitTable()
{
gchar Str[40];
gint i;
GtkWidget *Label,*Entry,*But;

for (i=0;i<Fit.NPeaks;++i)                                        //Note that if Fit.NPeaks=0 the entire loop is skipped
    {
    sprintf(Str,"%02d",i+1); Label=gtk_label_new(Str); 
    gtk_table_attach(GTK_TABLE(Fit.Table2),Label,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);       //Peak Position
    sprintf(Str,"%.3f",Fit.P[6*i+3]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+3));
    gtk_table_attach(GTK_TABLE(Fit.Table2),Entry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    FlagStr(6*i+3,Str); But=gtk_button_new_with_label(Str);                                         //Peak Position Flag
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+3));
    gtk_table_attach(GTK_TABLE(Fit.Table2),But,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);                //FWHM
    sprintf(Str,"%.3f",Fit.P[6*i+4]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+4));
    gtk_table_attach(GTK_TABLE(Fit.Table2),Entry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    FlagStr(6*i+4,Str); But=gtk_button_new_with_label(Str);                                                  //FWHM Flag
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+4));
    gtk_table_attach(GTK_TABLE(Fit.Table2),But,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);                //Asym
    sprintf(Str,"%.3f",Fit.P[6*i+5]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+5));
    gtk_table_attach(GTK_TABLE(Fit.Table2),Entry,5,6,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    FlagStr(6*i+5,Str); But=gtk_button_new_with_label(Str);                                                  //Asym Flag
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+5));
    gtk_table_attach(GTK_TABLE(Fit.Table2),But,6,7,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);              //L.Tail
    sprintf(Str,"%.3f",Fit.P[6*i+6]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+6));
    gtk_table_attach(GTK_TABLE(Fit.Table2),Entry,7,8,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    FlagStr(6*i+6,Str); But=gtk_button_new_with_label(Str);                                                //L.Tail Flag
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+6));
    gtk_table_attach(GTK_TABLE(Fit.Table2),But,8,9,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);              //R.Tail
    sprintf(Str,"%.3f",Fit.P[6*i+7]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+7));
    gtk_table_attach(GTK_TABLE(Fit.Table2),Entry,9,10,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    FlagStr(6*i+7,Str); But=gtk_button_new_with_label(Str);                                                //R.Tail Flag
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+7));
    gtk_table_attach(GTK_TABLE(Fit.Table2),But,10,11,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    /* Insert code for shape calibration case. See AddPeak for details */

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);              //Height
    sprintf(Str,"%.3f",Fit.P[6*i+8]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(6*i+8));
    gtk_table_attach(GTK_TABLE(Fit.Table2),Entry,11,12,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    FlagStr(6*i+8,Str); But=gtk_button_new_with_label(Str);                                                //Height Flag
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FClick),GINT_TO_POINTER(6*i+8));
    gtk_table_attach(GTK_TABLE(Fit.Table2),But,12,13,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }
}
//----------------------------------------------------------------------------------------------------------------------
void DelPeak(gint WinNo,gint PeakX)
{
gint MaxChan,Ch,i,j,Dist,LoDist,PkNo;

MaxChan=GetMaxChan(WinNo);
if (!FitWinOpen || (Fit.WinNo!=WinNo) || (Fit.NPeaks==0)) return;               //Ensure click is in relevant window etc
Ch=XSlope[WinNo]>0 ? (gint)(0.5+(gfloat)Prop[WinNo].One.XLo+(gfloat)((PeakX-ORIGIN_X)/XSlope[WinNo])) : 0; 
if ( (Ch<=Fit.Lc) || (Ch>=Fit.Lc+Fit.NPts+1) ) return;                   //Ensure the user has clicked between Lc and Rc
PkNo=0;
for (i=0,LoDist=MaxChan;i<Fit.NPeaks;i++)                               //Run thru all the peaks to find the nearest one
    {
    Dist=ABS(Ch-(gint)Fit.P[6*i+3]);
    if (Dist<LoDist) { LoDist=Dist; PkNo=i; }
    }
Fit.NPeaks--;
for (i=PkNo;i<Fit.NPeaks;++i)
    { 
    Fit.P[6*i+3]=Fit.P[6*(i+1)+3]; Fit.P[6*i+4]=Fit.P[6*(i+1)+4]; Fit.P[6*i+5]=Fit.P[6*(i+1)+5]; 
    Fit.P[6*i+6]=Fit.P[6*(i+1)+6]; Fit.P[6*i+7]=Fit.P[6*(i+1)+7]; Fit.P[6*i+8]=Fit.P[6*(i+1)+8];
    for (j=3;j<9;++j) { Fit.F[6*i+j]=Fit.F[6*(i+1)+j]; if (!i && (Fit.F[6*i+j]==Constrained)) Fit.F[6*i+j]=Variable; }
    //We copy the flags from the next row, but on the first row we must change Constrained to Variable
    }
if (GTK_IS_WIDGET(Fit.Table2)) gtk_widget_destroy(Fit.Table2);
Fit.Table2=gtk_table_new(Fit.NPeaks,13,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Fit.ScrlW),Fit.Table2); 
PopulateFitTable();
gtk_widget_show_all(Fit.ScrlW);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void CalibDataChanged(GtkWidget *W,gpointer Data)
{
gint Index,Row,Col;
gdouble SpecCh;

Index=GPOINTER_TO_INT(Data); Row=Index/5; Col=Index%5;

switch (Col)
   {
   case 1: //SpecCh changed
           SpecCh=atof(gtk_entry_get_text(GTK_ENTRY(W)));
           CalibData.Ch[Row]=SpecCh*GlobalTemp;
           break;
   case 2: //EnChanged
           CalibData.En[Row]=atof(gtk_entry_get_text(GTK_ENTRY(W)));
   }
}
/*--------------------------------------------------------------------------------------------------------------------*/
void AddAssistPoint(gint WinNo,gint CalibX)
{
gint MaxChan,SNo,Par;
gdouble SpecCh;
gchar Str[40];

MaxChan=GetMaxChan(WinNo);
if (!AssistWinOpen || (CalibData.WinNo!=WinNo)) return;                                         //Ensure click is in relevant window etc
if ( (CalibX<ORIGIN_X) || (CalibX>ORIGIN_X+LEN_X(CanvasW[WinNo])) ) return;                              //Ensure click is inside limits
SNo=ScreenSpecNo[WinNo][Screen]; Par=Setup.Oned.Par[SNo];
SpecCh=XSlope[WinNo]>0 ? (gint)(0.5+(gfloat)Prop[WinNo].One.XLo+(gfloat)((CalibX-ORIGIN_X)/XSlope[WinNo])) : 0;
sprintf(Str,"%d",(gint)SpecCh);
if (Assist->PkNo==0)  gtk_entry_set_text(GTK_ENTRY(Assist->Peak1),Str); 
else                 gtk_entry_set_text(GTK_ENTRY(Assist->Peak2),Str);
Assist->PkNo++; if (Assist->PkNo>1) Assist->PkNo=0;
}
//----------------------------------------------------------------------------------------------------------------------
void MakeCalibRow(gint i)
{
gdouble SpecCh;
gchar Str[40];
GtkWidget *Label,*Entry,*But;

sprintf(Str,"%02d",i+1); Label=gtk_label_new(Str);
gtk_table_attach(GTK_TABLE(CalibData.Table),Label,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

SpecCh=(gfloat)CalibData.Ch[i]/GlobalTemp;
Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,95,-1);
sprintf(Str,"%.3f",SpecCh); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(CalibDataChanged),GINT_TO_POINTER(5*i+1));
gtk_table_attach(GTK_TABLE(CalibData.Table),Entry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,95,-1);
sprintf(Str,"%.3f",CalibData.En[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(CalibDataChanged),GINT_TO_POINTER(5*i+2));
gtk_table_attach(GTK_TABLE(CalibData.Table),Entry,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,95,-1);
gtk_entry_set_text(GTK_ENTRY(Entry),"0");
gtk_table_attach(GTK_TABLE(CalibData.Table),Entry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

But=gtk_button_new_with_label("Delete");
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DeleteCalibPoint),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(CalibData.Table),But,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
}
//----------------------------------------------------------------------------------------------------------------------
void DeleteCalibPoint(GtkWidget *W,gpointer Data)
{
gint i,Row;

Row=GPOINTER_TO_INT(Data);
CalibData.NPts=MAX(0,CalibData.NPts-1);
for (i=Row;i<CalibData.NPts;++i) { CalibData.Ch[i]=CalibData.Ch[i+1]/GlobalTemp; CalibData.En[i]=CalibData.En[i+1]; }
if (GTK_IS_WIDGET(CalibData.Table)) gtk_widget_destroy(CalibData.Table);
CalibData.Table=gtk_table_new(CalibData.NPts,5,FALSE);
for (i=0;i<CalibData.NPts;++i) MakeCalibRow(i);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(CalibData.ScrlW),CalibData.Table);
gtk_widget_show_all(CalibData.ScrlW);
}
//----------------------------------------------------------------------------------------------------------------------
void AddCalibPoint(gint WinNo,gint CalibX,gint Opt,gdouble ChNo,gdouble Energy)
//When Opt=0 (normal) CalibX is input, when Opt=1 (auto calibration for souurce) ChNo is input 
{
gint MaxChan,i,SNo,Par;
gdouble SpecCh;
gchar Str[40];

MaxChan=GetMaxChan(WinNo);
if (Opt==0)
   {
   if (!CalibWinOpen || (CalibData.WinNo!=WinNo) || (CalibData.NPts>=MAX_CALIB_DATA)) return;      //Ensure click is in relevant window etc
   if ( (CalibX<ORIGIN_X) || (CalibX>ORIGIN_X+LEN_X(CanvasW[WinNo])) ) return;                              //Ensure click is inside limits
   }
SNo=ScreenSpecNo[WinNo][Screen]; Par=Setup.Oned.Par[SNo];

if (Opt==0) SpecCh=XSlope[WinNo]>0 ? (gint)(0.5+(gfloat)Prop[WinNo].One.XLo+(gfloat)((CalibX-ORIGIN_X)/XSlope[WinNo])) : 0;
else        SpecCh=ChNo;

GlobalTemp=Setup.Parameter.Chan[Par-1]/MaxChan;
CalibData.Ch[CalibData.NPts]=SpecCh*GlobalTemp;

i=CalibData.NPts; CalibData.NPts++; 
gtk_table_resize(GTK_TABLE(CalibData.Table),CalibData.NPts,5);
CalibData.En[i]=Energy;
MakeCalibRow(i);
gtk_widget_show_all(CalibData.Table);
}
/*------------------------------------------------------------------------------------------------------------------------*/
gint CanvasEvent1(GtkWidget *W,GdkEventAny *Event,gpointer Data)
/* Here we handle Type GDK_BUTTON_PRESS=4, GDK_MOTION_NOTIFY=3, GDK_BUTTON_RELEASE=7
 * for other Types we ignore the Event by returning FALSE */
{
GdkEventType Type;
GdkRectangle UpdateRect;
gint OriginX,OriginY,LenX,LenY,WinNo,MaxChan,BoxY1,BoxY2,BoxY2P,PeakX,CalibX,XSize,YSize;

WinNo=GPOINTER_TO_INT(Data); MaxChan=GetMaxChan(WinNo);
OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); LenX=LEN_X(CanvasW[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
Type=Event->type;
switch (Type)
   {
   case GDK_BUTTON_RELEASE:
        FrozenWin=-1;                                                                           //Free the window for update
        return TRUE;
        break;
   case GDK_BUTTON_PRESS:
        switch (((GdkEventButton *)(Event))->button)
           {
           case 1:                                                                       //Left button pressed. Draw markers
              if ( (((GdkEventMotion*)Event)->y < CANVAS_TITLE_HEIGHT) && 
                   (((GdkEventMotion*)Event)->x > CanvasW[WinNo]-CANVAS_RIGHT_MARGIN_WIDTH) )   //UR: maximise/normal window
                 {
                 gdk_window_get_size(SpecWin[WinNo]->window,&XSize,&YSize);
                 if ((XSize>=MONITOR_XRES-X_BORDER-16) && (YSize>=MONITOR_YRES-TOP_SPACE-BOT_SPACE-Y_BORDER-16)) //Normal window size
                    Plot1(WinNo,Prop[WinNo].X,Prop[WinNo].Y,Prop[WinNo].W,Prop[WinNo].H);
                 else Plot1(WinNo,0,TOP_SPACE+TopOfset,MONITOR_XRES-X_BORDER-1,MONITOR_YRES-TOP_SPACE-BOT_SPACE-Y_BORDER-1);//Maximise
                 return TRUE;
                 }
              if (Prop[WinNo].One.BoxDrawn == 1)                                                            //Erase prev box
                 {
                 UpdateRect.x=Box[WinNo].X1; UpdateRect.y=OriginY-LenY; UpdateRect.width=1;
                 UpdateRect.height=LenY+1; gtk_widget_draw(W,&UpdateRect);
                 UpdateRect.x=Box[WinNo].X2; UpdateRect.y=OriginY-LenY; UpdateRect.width=1;
                 UpdateRect.height=LenY+1; gtk_widget_draw(W,&UpdateRect);
                 UpdateRect.x=0; UpdateRect.y=CanvasH[WinNo]-2*(FONT_HEIGHT+7); 
                 UpdateRect.width=CanvasW[WinNo]; UpdateRect.height=2*(FONT_HEIGHT+7);
                 gtk_widget_draw(W,&UpdateRect);                                                   //Erase both dialog lines
                 }
              Box[WinNo].X1=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
              BoxY1=CLAMP(((GdkEventMotion *)(Event))->y,OriginY-LenY,OriginY);
              Box[WinNo].X2=Box[WinNo].X1; BoxY2=BoxY1; Prop[WinNo].One.BoxDrawn=0;
              Prop[WinNo].One.BoxX1Ch=XSlope[WinNo]>0 ? 
                                      (gint)(0.5+(gfloat)Prop[WinNo].One.XLo+(gfloat)((Box[WinNo].X1-ORIGIN_X)/XSlope[WinNo])) : 0;
              Prop[WinNo].One.BoxX1Ch=MIN(Prop[WinNo].One.BoxX1Ch,MaxChan-1);
              return TRUE;
              break;
           case 2:           //Middle button pressed. We use this to Add or Del peaks for Fit also to add Calibration points
              if (FitWinOpen)
                 {
                 PeakX=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
                 switch (Fit.MouseContext)
                    {
                    case AddPeaks: AddPeak(WinNo,PeakX); break;                                     //Mouse set to add peaks
                    case DelPeaks: DelPeak(WinNo,PeakX); break;                                  //Mouse set to delete peaks
                    }
                 }
              if (CalibWinOpen)
                 {
                 if (!AssistWinOpen)
                    {
                    CalibX=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
                    AddCalibPoint(WinNo,CalibX,0,0.0,0.0);
                    }
                 else
                    {
                    CalibX=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
                    AddAssistPoint(WinNo,CalibX);
                    }
                 }
              return TRUE;
              break;
           case 3:  return FALSE;                                               //Right button press. Thats for canvas popup
           }
        break;
   case GDK_MOTION_NOTIFY:
        FrozenWin=WinNo;                                           //Prevent update of this window if the mouse is in motion
        Box[WinNo].X2P=Box[WinNo].X2; BoxY2P=BoxY2;
        Box[WinNo].X2=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
        BoxY2=CLAMP(((GdkEventMotion *)(Event))->y,OriginY-LenY,OriginY);
        if (PixMap[WinNo] != NULL) 
           { 
           gdk_gc_set_rgb_fg_color(Gc,&Colour1D[5]);
           DrawBox(W,WinNo); 
           }
        return TRUE;
        break;
   default: return FALSE;
   }
return FALSE;
}
/*------------------------------------------------------------------------------------------------------------------------*/
gint CanvasPopup(GtkWidget *W,GdkEvent *Event)
{
GtkMenu *Menu;
GdkEventButton *EventButton;

/* Note that we can reach here with a double click also.  We could use this for some other functionality  *
 * For double click we have to test if (Event->type == GDK_2BUTTON_PRESS)                                 */

Menu=GTK_MENU(W);
if (Event->type == GDK_BUTTON_PRESS)
     {
     EventButton=(GdkEventButton *)Event;
     if (EventButton->button == 3)
        {
        gtk_menu_popup(Menu,NULL,NULL,NULL,NULL,EventButton->button,EventButton->time);
        return TRUE;
        }
     }
return FALSE;
}
/*------------------------------------------------------------------------------------------------------------------------*/
void CreateTitle1(gchar *Str,gint WinNo)
{
gchar FileName[22],ParaName[60];
gint SNo,Par,NPar;
                                                                                                                             
SNo=ScreenSpecNo[WinNo][Screen];
AbbreviateFileName(FileName,Setup.Files.Oned[SNo],18);
switch (ScreenSpecType[WinNo][Screen])
   {
   case 1: Par=Setup.Oned.Par[SNo]; NPar=Setup.Oned.NPar[SNo]; break;   //1d Spectra
   case 3: Par=Setup.Twod.XPar[SNo]; NPar=Setup.Twod.NXPar[SNo]; break; //2d X projection
   case 4: Par=Setup.Twod.YPar[SNo]; NPar=Setup.Twod.NYPar[SNo];        //2d Y projection
   }
if (Par==0) { sprintf(Str,"Hit Pattern %s",FileName); return; }
if (Par<=Setup.Parameter.NPar) strcpy(ParaName,Setup.Parameter.Name[Par-1]);
else strcpy(ParaName,Setup.Pseudo.Name[Par-1-Setup.Parameter.NPar]);
if (NPar==1) sprintf(Str,"%s %s",ParaName,FileName);
else         sprintf(Str,"%sVec %s",ParaName,FileName);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void CanvasTitle1(GtkWidget *W,gint WinNo)
{
gchar Str[80];

CreateTitle1(Str,WinNo);
gdk_draw_string(PixMap[WinNo],Font,Gc,FONT_WIDTH,FONT_HEIGHT+4,Str);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void XAxis(GtkWidget *W,gint WinNo)
{
gint OriginX,OriginY,LenX,LenY,i,XVal,DXVal,StrLen,LabelBottomY,MaxChan,SType,SNo,XLo,XHi,PNo,R;
gfloat Dx,x;
gdouble AxVal;
gchar Str[24];

SType=ScreenSpecType[WinNo][Screen]; SNo=ScreenSpecNo[WinNo][Screen];

switch (SType)
   {
   case 1: XLo=Prop[WinNo].One.XLo; XHi=Prop[WinNo].One.XHi;                      //Oned display
           MaxChan=GetMaxChan(WinNo); PNo=Setup.Oned.Par[SNo]; break;
   case 2: XLo=Prop[WinNo].Two.XLo; XHi=Prop[WinNo].Two.XHi; 
           MaxChan=Setup.Twod.XChan[SNo]; PNo=Setup.Twod.XPar[SNo]; break;        //Twod display
   case 3: XLo=Prop[WinNo].One.XLo; XHi=Prop[WinNo].One.XHi;                      //2d X projection
           MaxChan=GetMaxChan(WinNo); PNo=Setup.Twod.XPar[SNo]; break;
   case 4: XLo=Prop[WinNo].One.XLo; XHi=Prop[WinNo].One.XHi;                      //2d Y projection
           MaxChan=GetMaxChan(WinNo); PNo=Setup.Twod.YPar[SNo]; break;
   }

if (PNo>0) R=Setup.Parameter.Chan[PNo-1]/MaxChan;                                             //Avoid PNo=0 (Hit parameter)
OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); LenX=LEN_X(CanvasW[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
LabelBottomY=OriginY+AXIS_TICK_HEIGHT+AXIS_LABELS_HEIGHT+4; Dx=0.25*(gfloat)(LEN_X(CanvasW[WinNo]));
gdk_draw_line(PixMap[WinNo],Gc,OriginX,OriginY+1,OriginX+LenX,OriginY+1);
DXVal=0.25*(gfloat)(XHi-XLo)+0.5;
XSlope[WinNo]=(gfloat)LenX/(gfloat)(XHi-XLo);
for (i=0,x=(gfloat)OriginX,XVal=XLo;i<5;i++,x+=Dx,XVal+=DXVal)
    {
    XVal=MIN(XVal,MaxChan);
    gdk_draw_line(PixMap[WinNo],Gc,(gint)(x+0.5),OriginY+2,
                  (gint)(x+0.5),OriginY+AXIS_TICK_HEIGHT);
    if (PNo>0)                                                                                //Avoid PNo=0 (Hit parameter)
       {
       AxVal=Energy(PNo,(gdouble)(XVal*R));
       if (AxVal>4 && AxVal<99999) sprintf(Str,"%.0f",AxVal); else sprintf(Str,"%.3g",AxVal);
       } 
    else sprintf(Str,"%d",XVal); 
    StrLen=0.5*(float)strlen(Str);
    gdk_draw_string(PixMap[WinNo],Font,Gc,
                    (gint)(x+0.5)-(StrLen+1)*FONT_WIDTH,LabelBottomY,Str);
    }
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void CountsAxis(GtkWidget *W,gint WinNo)
{
gint OriginX,OriginY,LenX,LenY,LabelRightX,StrLen,i,EFormat;
guint32 DYVal,YVal;
gfloat Dy,y;
gdouble LogYLo,LogYHi,LogDYVal,LogYVal;
gchar Str[8];

Prop[WinNo].One.CountsHi=MAX(Prop[WinNo].One.CountsLo+4,Prop[WinNo].One.CountsHi);
OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); LenX=LEN_X(CanvasW[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
LabelRightX=OriginX-AXIS_TICK_WIDTH-4; Dy=0.25*(gfloat)LenY;
gdk_draw_line(PixMap[WinNo],Gc,OriginX-1,OriginY,OriginX-1,OriginY-LenY);
if (Prop[WinNo].One.CountsLog)
   {
   LogYLo=log((double)MAX(1,Prop[WinNo].One.CountsLo)); LogYHi=log((double)MAX(Prop[WinNo].One.CountsLo+50,Prop[WinNo].One.CountsHi));
   LogDYVal=0.25*(LogYHi-LogYLo); CountsSlope[WinNo]=(gfloat)LenY/(LogYHi-LogYLo);
   LogYVal=LogYLo; YVal=(guint32)exp(LogYVal);
   }
else
   { 
   DYVal=0.25*(gfloat)(Prop[WinNo].One.CountsHi-Prop[WinNo].One.CountsLo)+0.5; 
   CountsSlope[WinNo]=(gfloat)LenY/(gfloat)(Prop[WinNo].One.CountsHi-Prop[WinNo].One.CountsLo);
   YVal=Prop[WinNo].One.CountsLo;
   }
if (Prop[WinNo].One.CountsHi>9.0e6) EFormat=1; else EFormat=0;
for (i=0,y=(gfloat)OriginY;i<5;i++,y-=Dy)
    {
    gdk_draw_line(PixMap[WinNo],Gc,OriginX-AXIS_TICK_WIDTH,
                  (gint)(y+0.5),OriginX-2,(gint)(y+0.5));
    if (EFormat) sprintf(Str,"%8.2e",(gdouble)YVal); else sprintf(Str,"%u",YVal);
    StrLen=strlen(Str);
    gdk_draw_string(PixMap[WinNo],Font,Gc,LabelRightX-StrLen*FONT_WIDTH,
                    (gint)(y+0.5)+FONT_HEIGHT/2,Str);
    if (Prop[WinNo].One.CountsLog) { LogYVal+=LogDYVal; YVal=(guint32)(exp(LogYVal)); } else YVal+=DYVal;
    }
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void Overlap1d(GtkWidget *W,gint WinNo)
{
gint i,OriginX,OriginY,ChX,XPix,YPix,XPrev,YPrev,LenY;
gfloat Counts;

if (Prop[WinNo].One.OverlapOff) return;                                         //A quick return if all overlaps are off
for (i=0;i<MIN(Setup.Oned.N,MAX_OVERLAP);++i)
    {
    if (Prop[WinNo].One.Overlap[i])
       {
       gdk_gc_set_rgb_fg_color(Gc,&ColourOv[i]);
       
       OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
       Counts=(gfloat)Get1dCounts(WinNo,Prop[WinNo].One.XLo-Prop[WinNo].One.OvHShift[i],TRUE,Prop[WinNo].One.OvSpec[i])
             +(gfloat)Prop[WinNo].One.OvVShift[i];
       XPrev=XSlope[WinNo]*0.5+(gfloat)OriginX; 
       if (Prop[WinNo].One.CountsLog) 
           YPrev=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                 MAX(0.0,log((gdouble)Counts)-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
       else YPrev=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*MAX(0,Counts-Prop[WinNo].One.CountsLo));
       gdk_draw_line(PixMap[WinNo],Gc,OriginX,YPrev,XPrev,YPrev);
       for (ChX=Prop[WinNo].One.XLo+1;ChX<Prop[WinNo].One.XHi;ChX++)
           {
           XPix=XSlope[WinNo]*(0.5+(gfloat)(ChX-Prop[WinNo].One.XLo))+(gfloat)OriginX;
           Counts=(gfloat)Get1dCounts(WinNo,ChX-Prop[WinNo].One.OvHShift[i],TRUE,Prop[WinNo].One.OvSpec[i])
                 +(gfloat)Prop[WinNo].One.OvVShift[i];
           if (Prop[WinNo].One.CountsLog) 
               YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                    MAX(0.0,log((gdouble)Counts)-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
           else YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*MAX(0,Counts-Prop[WinNo].One.CountsLo));
           gdk_draw_line(PixMap[WinNo],Gc,XPrev,YPrev,XPrev,YPix);
           gdk_draw_line(PixMap[WinNo],Gc,XPrev,YPix,XPix,YPix);
           XPrev=XPix; YPrev=YPix;
           }
       }
    }
}
/*---------------------------------------------------------------------------------------------------------------------*/
void Draw_Maximise_Button(GtkWidget *W,gint WinNo)
{
gdk_draw_rectangle(PixMap[WinNo],W->style->black_gc,0,CanvasW[WinNo]-CANVAS_RIGHT_MARGIN_WIDTH+6,2,
                   CANVAS_RIGHT_MARGIN_WIDTH-10,CANVAS_TITLE_HEIGHT-12);
gdk_draw_line(PixMap[WinNo],W->style->black_gc,CanvasW[WinNo]-CANVAS_RIGHT_MARGIN_WIDTH+6,3, 
              CanvasW[WinNo]-4,3);
gdk_draw_line(PixMap[WinNo],W->style->black_gc,CanvasW[WinNo]-CANVAS_RIGHT_MARGIN_WIDTH+6,4, 
              CanvasW[WinNo]-4,4);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Display1d(GtkWidget *W,gint WinNo)
{
gint OriginX,OriginY,ChX,XPix,YPix,XPrev,YPrev,Pk,LenX,LenY,FitRegionX1,FitRegionX2,XEnd,YEnd,i,SNo;
gfloat Counts;
GdkRectangle Rect;

gdk_gc_set_rgb_fg_color(Gc,&Colour1D[1]);
gdk_gc_set_line_attributes(Gc,0,GDK_LINE_SOLID,GDK_CAP_NOT_LAST,GDK_JOIN_MITER); //Style

OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
Counts=(gfloat)Get1dCounts(WinNo,Prop[WinNo].One.XLo,FALSE,0);
XPrev=XSlope[WinNo]*0.5+(gfloat)OriginX; 
if (Prop[WinNo].One.CountsLog) 
     YPrev=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
               MAX(0.0,log((gdouble)Counts)-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
else YPrev=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*MAX(0,Counts-Prop[WinNo].One.CountsLo));
gdk_draw_line(PixMap[WinNo],Gc,OriginX,YPrev,XPrev,YPrev);
for (ChX=Prop[WinNo].One.XLo+1;ChX<Prop[WinNo].One.XHi;ChX++)
    {
    XPix=XSlope[WinNo]*(0.5+(gfloat)(ChX-Prop[WinNo].One.XLo))+(gfloat)OriginX;
    Counts=(gfloat)Get1dCounts(WinNo,ChX,FALSE,0);
    if (Prop[WinNo].One.CountsLog) 
         YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                  MAX(0.0,log((gdouble)Counts)-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
    else YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*MAX(0,Counts-Prop[WinNo].One.CountsLo));
    gdk_draw_line(PixMap[WinNo],Gc,XPrev,YPrev,XPrev,YPix);
    gdk_draw_line(PixMap[WinNo],Gc,XPrev,YPix,XPix,YPix);
    XPrev=XPix; YPrev=YPix;
    }

if (Prop[WinNo].One.BoxDrawn)                                          //Added 20 July 2002. Preserves cursors after update
   {                                //I would prefer soft cursors on Canvas[WinNo]->window but dont know why it doesnt work
   /*Recalculate pixel values since the user might have resized the window*/
   Box[WinNo].X1=ORIGIN_X+(Prop[WinNo].One.BoxX1Ch-Prop[WinNo].One.XLo)*XSlope[WinNo];
   Box[WinNo].X2=ORIGIN_X+(Prop[WinNo].One.BoxX2Ch-Prop[WinNo].One.XLo)*XSlope[WinNo];
   gdk_gc_set_rgb_fg_color(Gc,&Colour1D[9]);
   gdk_draw_line(PixMap[WinNo],Gc,Box[WinNo].X1,OriginY,Box[WinNo].X1,OriginY-LenY);
   gdk_draw_line(PixMap[WinNo],Gc,Box[WinNo].X2,OriginY,Box[WinNo].X2,OriginY-LenY);
   }

if ( (Prop[WinNo].One.ShowPeaks)                                                              //Ensure context: Peak Search
     && (!FitWinOpen || (Fit.WinNo!=WinNo)) )
   {
   SNo=ScreenSpecNo[WinNo][Screen]; gdk_gc_set_rgb_fg_color(Gc,&Colour1D[6]);
   gdk_gc_set_line_attributes(Gc,0,GDK_LINE_ON_OFF_DASH,GDK_CAP_NOT_LAST,
                              GDK_JOIN_MITER);                                                                     //Dashed 
   for (i=0;i<Setup.Oned.NPeaks[SNo];i++)
       {
       if ( (Setup.Oned.Peaks[i][SNo]>Prop[WinNo].One.XLo) && (Setup.Oned.Peaks[i][SNo]<Prop[WinNo].One.XHi) )
          {
          XPix=XSlope[WinNo]*(0.5+(gfloat)(Setup.Oned.Peaks[i][SNo]-Prop[WinNo].One.XLo))+(gfloat)OriginX;
          gdk_draw_line(PixMap[WinNo],Gc,XPix,OriginY-LenY,XPix,OriginY);
          }
       }
   gdk_gc_set_line_attributes(Gc,0,GDK_LINE_SOLID,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);                             //Restore
   }

if (!FitWinOpen || (Fit.WinNo!=WinNo)) return;                                                    //Ensure context: Peak Fit
if (Fit.Busy) return;                                                                                   //Fit is in progress
Rect.x=0; Rect.y=CanvasH[WinNo]-2*(FONT_HEIGHT+7);
Rect.width=CanvasW[WinNo]; Rect.height=2*(FONT_HEIGHT+7);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[7]);                                                         //Draw Fit Region rectangle

FitRegionX1=XSlope[WinNo]*((gfloat)(Fit.Lc-Prop[WinNo].One.XLo))+(gfloat)OriginX;
FitRegionX2=XSlope[WinNo]*((gfloat)((Fit.Lc+Fit.NPts-1)-Prop[WinNo].One.XLo))+(gfloat)OriginX;
LenX=LEN_X(CanvasW[WinNo]); XEnd=OriginX+LenX; YEnd=OriginY-LenY;
if ((FitRegionX1>OriginX) && (FitRegionX1<XEnd)) 
   gdk_draw_line(PixMap[WinNo],Gc,FitRegionX1,YEnd,FitRegionX1,OriginY);
if ((FitRegionX2>OriginX) && (FitRegionX2<XEnd)) 
   gdk_draw_line(PixMap[WinNo],Gc,FitRegionX2,YEnd,FitRegionX2,OriginY);
if ( !((FitRegionX1>XEnd) || (FitRegionX2<OriginX)) )
   {
   gdk_draw_line(PixMap[WinNo],Gc,MAX(OriginX,FitRegionX1),OriginY,MIN(XEnd,FitRegionX2),
                 OriginY);
   gdk_draw_line(PixMap[WinNo],Gc,MAX(OriginX,FitRegionX1),YEnd,MIN(XEnd,FitRegionX2),
                 YEnd);
   }
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[6]);                                                             //Draw peak marker
for (Pk=0;Pk<Fit.NPeaks;Pk++)
    {
    ChX=(gint)(Fit.P[6*Pk+3]+0.5);
    if ( (ChX>Prop[WinNo].One.XLo) && (ChX<Prop[WinNo].One.XHi) )
       {
       XPix=XSlope[WinNo]*(0.5+(gfloat)(ChX-Prop[WinNo].One.XLo))+(gfloat)OriginX;
       if (Prop[WinNo].One.CountsLog)
       YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                  MAX(0.0,log(Fit.Data[ChX-Fit.Lc])-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
       else YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                     MAX(0,(guint32)Fit.Data[ChX-Fit.Lc]-Prop[WinNo].One.CountsLo));
       gdk_draw_line(PixMap[WinNo],Gc,XPix,YPix+1,XPix,OriginY);
       }
    }
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[8]);                                            //Display the fit by a dashed curve
gdk_gc_set_line_attributes(Gc,0,GDK_LINE_ON_OFF_DASH,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);                          //Dashed 
for (ChX=Fit.Lc;ChX<Fit.Lc+Fit.NPts;ChX++)
    {
    XPix=XSlope[WinNo]*(0.5+(gfloat)(ChX-Prop[WinNo].One.XLo))+(gfloat)OriginX;
    Counts=BiGauss(ChX-Fit.Lc,Fit.P,NULL);
    if (Prop[WinNo].One.CountsLog)
         YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                  MAX(0.0,log((gdouble)Counts)-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
    else YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*MAX(0,Counts-Prop[WinNo].One.CountsLo));
    if ( (ChX>Fit.Lc) && (XPrev>=OriginX) && (XPix<=XEnd) )                                 //Display only within the region
       gdk_draw_line(PixMap[WinNo],Gc,XPrev,YPrev,XPix,YPix);
    XPrev=XPix; YPrev=YPix;
    }
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[2]);                                      //Display the background by a dashed line
gdk_gc_set_line_attributes(Gc,0,GDK_LINE_ON_OFF_DASH,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);                          //Dashed 
for (ChX=Fit.Lc;ChX<Fit.Lc+Fit.NPts;ChX++)
    {
    XPix=XSlope[WinNo]*(0.5+(gfloat)(ChX-Prop[WinNo].One.XLo))+(gfloat)OriginX;
    Counts=Background(ChX-Fit.Lc,Fit.P,NULL);
    if (Prop[WinNo].One.CountsLog)
         YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*
                  MAX(0.0,log((gdouble)Counts)-log((gdouble)MAX(1,Prop[WinNo].One.CountsLo))));
    else YPix=MAX(CANVAS_TITLE_HEIGHT,(gfloat)OriginY-CountsSlope[WinNo]*MAX(0,Counts-Prop[WinNo].One.CountsLo));
    if ( (ChX>Fit.Lc) && (XPrev>=OriginX) && (XPix<=XEnd) )                             //Display only within the region
       gdk_draw_line(PixMap[WinNo],Gc,XPrev,YPrev,XPix,YPix);
    XPrev=XPix; YPrev=YPix;
    }

gdk_gc_set_line_attributes(Gc,0,GDK_LINE_SOLID,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);                               //Restore
}
//----------------------------------------------------------------------------------------------------------------------
void AutoScale1(gint WinNo)
{
guint32 YMax,Counts;
gint i,X1,X2,MaxChan,LoLim;

MaxChan=GetMaxChan(WinNo);
LoLim=MAX(0.004*MaxChan,1); if (MaxChan<257) LoLim=1;
YMax=0; X1=MAX(Prop[WinNo].One.XLo,LoLim); X2=MIN(Prop[WinNo].One.XHi,MaxChan-2);
for (i=X1;i<X2;i++) 
    {
    Counts=Get1dCounts(WinNo,i,FALSE,0);
    if (Counts>YMax) YMax=Counts;
    }
YMax=1.15*YMax; YMax=4*(YMax/4); YMax=MAX(4,YMax);
Prop[WinNo].One.CountsHi=(gfloat)YMax; Prop[WinNo].One.CountsLo=0;
}
//----------------------------------------------------------------------------------------------------------------------
gint CanvasConfig1(GtkWidget *W,GdkEventConfigure *Event,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
if (PixMap[WinNo]) gdk_pixmap_unref(PixMap[WinNo]);
CanvasW[WinNo]=W->allocation.width; CanvasH[WinNo]=W->allocation.height;
PixMap[WinNo]=gdk_pixmap_new(W->window,CanvasW[WinNo],CanvasH[WinNo],-1);
if (Gc==NULL) Gc=gdk_gc_new(PixMap[WinNo]); 
if (Prop[WinNo].One.CountsAuto) AutoScale1(WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[0]);
gdk_draw_rectangle(PixMap[WinNo],Gc,TRUE,0,0,W->allocation.width,W->allocation.height);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[4]);
CanvasTitle1(W,WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[3]);
XAxis(W,WinNo); 
CountsAxis(W,WinNo); 
Draw_Maximise_Button(W,WinNo);
Display1d(W,WinNo); 
Overlap1d(W,WinNo);
return TRUE;
}
//----------------------------------------------------------------------------------------------------------------------
gint CanvasExpose1(GtkWidget *W,GdkEventExpose *Event,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
gdk_draw_pixmap(W->window,Gc,PixMap[WinNo],Event->area.x,Event->area.y,Event->area.x,
                Event->area.y,Event->area.width,Event->area.height);
/* Note that in draw_pixmap the colours specified in the second argument are ignored, colours
 * stored in the pixmap are applied so one could have even written: W->state->black_gc */
return FALSE;
}
//----------------------------------------------------------------------------------------------------------------------
void Zoom(GtkWidget *W,gpointer Data)
{
gint WinNo,i,Temp,MaxChan;
gboolean NoChange;

WinNo=GPOINTER_TO_INT(Data); MaxChan=GetMaxChan(WinNo);
if (Prop[WinNo].One.BoxX2Ch<Prop[WinNo].One.BoxX1Ch) 
   { Temp=Prop[WinNo].One.BoxX1Ch; Prop[WinNo].One.BoxX1Ch=Prop[WinNo].One.BoxX2Ch; Prop[WinNo].One.BoxX2Ch=Temp; }
Prop[WinNo].One.BoxX1Ch=CLAMP(Prop[WinNo].One.BoxX1Ch,0,MaxChan-4);
Prop[WinNo].One.BoxX2Ch=CLAMP(Prop[WinNo].One.BoxX2Ch,Prop[WinNo].One.BoxX1Ch+4,MaxChan);
if (Prop[WinNo].One.XLo==Prop[WinNo].One.BoxX1Ch) NoChange=TRUE; else NoChange=FALSE;
Prop[WinNo].One.XLo=Prop[WinNo].One.BoxX1Ch; Prop[WinNo].One.XHi=Prop[WinNo].One.BoxX2Ch; 
for (i=ZOOM_LEVELS-1;i>0;i--) 
    { Prop[WinNo].One.ZoomXLo[i]=Prop[WinNo].One.ZoomXLo[i-1]; Prop[WinNo].One.ZoomXHi[i]=Prop[WinNo].One.ZoomXHi[i-1]; }
Prop[WinNo].One.ZoomXLo[0]=Prop[WinNo].One.XLo; Prop[WinNo].One.ZoomXHi[0]=Prop[WinNo].One.XHi;
GTK_ADJUSTMENT(HScrlAdj[WinNo])->page_size=(gfloat)(Prop[WinNo].One.XHi-Prop[WinNo].One.XLo);
GTK_ADJUSTMENT(HScrlAdj[WinNo])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[WinNo])->upper=(gfloat)MaxChan;
Prop[WinNo].One.BoxDrawn=0;
if (CommonZoom)                                           //For CommonZoom copy the settings and update all display windows
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       if ( (ScreenSpecType[i][Screen]==1) && (i != WinNo) &&  //1d spectra only ... not projections (do we need that too?)
            (GetMaxChan(i)==MaxChan) && (Prop[i].Open) )             //Only spectra of the same resolution and open windows
          {
          Prop[i].One.XLo=Prop[WinNo].One.XLo; Prop[i].One.XHi=Prop[WinNo].One.XHi;
          GTK_ADJUSTMENT(HScrlAdj[i])->page_size=(gfloat)(Prop[i].One.XHi-Prop[i].One.XLo);
          GTK_ADJUSTMENT(HScrlAdj[i])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[i])->upper=(gfloat)MaxChan;
          Prop[i].One.BoxDrawn=0; RealScroll=FALSE;
          gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[i]),(gfloat)Prop[i].One.XLo+0.1);
          }
       }
   }
RealScroll=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[WinNo]),(gfloat)Prop[WinNo].One.XLo+0.1);
if (NoChange) gtk_signal_emit_by_name(GTK_OBJECT(HScrlAdj[WinNo]),"value_changed");                  //Ensure canvas update
}
//----------------------------------------------------------------------------------------------------------------------
void UnZoom(GtkWidget *W,gpointer Data)
{
gint WinNo,i,MaxChan;

WinNo=GPOINTER_TO_INT(Data); MaxChan=GetMaxChan(WinNo);
for (i=0;i<ZOOM_LEVELS-1;i++) 
    { Prop[WinNo].One.ZoomXLo[i]=Prop[WinNo].One.ZoomXLo[i+1]; Prop[WinNo].One.ZoomXHi[i]=Prop[WinNo].One.ZoomXHi[i+1]; }
Prop[WinNo].One.ZoomXLo[ZOOM_LEVELS-1]=Prop[WinNo].One.ZoomXLo[0]; 
Prop[WinNo].One.ZoomXHi[ZOOM_LEVELS-1]=Prop[WinNo].One.ZoomXHi[0];
Prop[WinNo].One.XLo=Prop[WinNo].One.ZoomXLo[0]; Prop[WinNo].One.XHi=Prop[WinNo].One.ZoomXHi[0];
GTK_ADJUSTMENT(HScrlAdj[WinNo])->page_size=(gfloat)(Prop[WinNo].One.XHi-Prop[WinNo].One.XLo);
GTK_ADJUSTMENT(HScrlAdj[WinNo])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[WinNo])->upper=(gfloat)MaxChan;
Prop[WinNo].One.BoxDrawn=0;
if (CommonZoom)                                           //For CommonZoom copy the settings and update all display windows
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       if ( (ScreenSpecType[i][Screen]==1) && (i != WinNo) &&  //1d spectra only ... not projections (do we need that too?)
            (GetMaxChan(i)==MaxChan) && (Prop[i].Open) )             //Only spectra of the same resolution and open windows
          {
          Prop[i].One.XLo=Prop[WinNo].One.XLo; Prop[i].One.XHi=Prop[WinNo].One.XHi;
          GTK_ADJUSTMENT(HScrlAdj[i])->page_size=(gfloat)(Prop[i].One.XHi-Prop[i].One.XLo);
          GTK_ADJUSTMENT(HScrlAdj[i])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[i])->upper=(gfloat)MaxChan;
          Prop[i].One.BoxDrawn=0; RealScroll=FALSE;
          gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[i]),(gfloat)Prop[i].One.XLo+0.1);
          }
       }
   }
RealScroll=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[WinNo]),(gfloat)Prop[WinNo].One.XLo+0.1);
}
//----------------------------------------------------------------------------------------------------------------------
void Full(GtkWidget *W,gpointer Data)
{
gint WinNo,MaxChan,i;

RealScroll=FALSE;
WinNo=GPOINTER_TO_INT(Data); MaxChan=GetMaxChan(WinNo);
Prop[WinNo].One.XLo=0; Prop[WinNo].One.XHi=MaxChan; 
GTK_ADJUSTMENT(HScrlAdj[WinNo])->page_size=MaxChan;
GTK_ADJUSTMENT(HScrlAdj[WinNo])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[WinNo])->upper=(gfloat)MaxChan;
Prop[WinNo].One.BoxDrawn=0;
if (CommonZoom)                                           //For CommonZoom copy the settings and update all display windows
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       if ( (ScreenSpecType[i][Screen]==1) && (i != WinNo) &&  //1d spectra only ... not projections (do we need that too?)
            (GetMaxChan(i)==MaxChan) && (Prop[i].Open) )             //Only spectra of the same resolution and open windows
          {
          Prop[i].One.XLo=Prop[WinNo].One.XLo; Prop[i].One.XHi=Prop[WinNo].One.XHi;
          GTK_ADJUSTMENT(HScrlAdj[i])->page_size=(gfloat)(Prop[i].One.XHi-Prop[i].One.XLo);
          GTK_ADJUSTMENT(HScrlAdj[i])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[i])->upper=(gfloat)MaxChan;
          Prop[i].One.BoxDrawn=0; RealScroll=FALSE;
          gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[i]),0.05);
          }
       }
   }
RealScroll=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[WinNo]),0.05);
}
//----------------------------------------------------------------------------------------------------------------------
void RefreshAll(void)
{
gint i;

if (!PixMap[0]) return;                                                                   //No display was ever opened!
if (ThemeChanged) { for (i=0;i<32;i++) Pen[i]=GetPen(i); ThemeChanged=FALSE; }
for (i=0;i<SCREEN_TOT;i++) 
    if (Prop[i].Open && (i!=FrozenWin) && !(BananaDrawMode && (i==BananaWinNo)))
       {
       switch (ScreenSpecType[i][Screen])
          {
          case 1: Update1d(i); break;
          case 2: Update2d(i); break;
          case 3: ComputeXProjection(i); Update1d(i); break;
          case 4: ComputeYProjection(i); Update1d(i);
          } 
       }
}
//----------------------------------------------------------------------------------------------------------------------
void RefreshAllOned(void)
{
gint i;

if (!PixMap[0]) return;                                                                   //No display was ever opened!
for (i=0;i<SCREEN_TOT;i++)
     if (Prop[i].Open && ScreenSpecType[i][Screen] == 1) Update1d(i);
}
//----------------------------------------------------------------------------------------------------------------------
void RefreshAllTwod(void)
{
gint i;

if (!PixMap[0]) return;                                                                   //No display was ever opened!
for (i=0;i<SCREEN_TOT;++i)
     if (Prop[i].Open && ScreenSpecType[i][Screen] == 2) Update2d(i);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void Refresh(GtkWidget *W,gpointer Data)
{
gint WinNo,i;

WinNo=GPOINTER_TO_INT(Data);
if (ThemeChanged) { for (i=0;i<32;++i) Pen[i]=GetPen(i); ThemeChanged=FALSE; }
switch (ScreenSpecType[WinNo][Screen])
   {
   case 1: case 3: case 4: Update1d(WinNo); break;
   case 2: Update2d(WinNo); 
   }
}
/*--------------------------------------------------------------------------------------------------------------------*/
void HideBox(GtkWidget *W,gpointer Data)             //This is similar to Refresh except that we switch off Two.BoxDrawn
{                                                   //Note that this is to be called from 2d plot, wont work for 1d plot
gint WinNo,i;

WinNo=GPOINTER_TO_INT(Data);
if (ThemeChanged) { for (i=0;i<32;++i) Pen[i]=GetPen(i); ThemeChanged=FALSE; }
Prop[WinNo].Two.BoxDrawn=0;                                                                           //Turn off the box
if (ScreenSpecType[WinNo][Screen] == 2) Update2d(WinNo);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void AreaFromChanged(GtkWidget *W,gpointer Data)
{
LcRc[0]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*--------------------------------------------------------------------------------------------------------------------*/
void AreaToChanged(GtkWidget *W,gpointer Data)
{
LcRc[1]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*--------------------------------------------------------------------------------------------------------------------*/
void AreaWinDestroyed(GtkWidget *W,GtkWidget *AreaWin)
{ g_free(AreaWidget); AreaMonitor=FALSE; AreaWinOpen=FALSE; }
/*--------------------------------------------------------------------------------------------------------------------*/
void ComputeArea(gint WinNo,gint Lc,gint Rc,guint32 *Integ,gdouble *Area,gdouble *Cent1,gdouble *Cent2)
{
guint32 LSum,RSum,Temp;
gint ChNo,i,MaxChan;
gdouble LBkg,RBkg,SlopeBkg,Bkg;

MaxChan=GetMaxChan(WinNo);
for (i=-1,LSum=0;i<2;i++)
    { 
    ChNo=MAX(Lc+i,0); 
    LSum+=Get1dCounts(WinNo,ChNo,FALSE,0);
    }
for (i=-1,RSum=0;i<2;i++)
    { 
    ChNo=MIN(Rc+i,MaxChan-1);
    RSum+=Get1dCounts(WinNo,ChNo,FALSE,0);
    }
LBkg=(gdouble)LSum/3.0; RBkg=(gdouble)RSum/3.0;
if (Rc != Lc) SlopeBkg=(RBkg-LBkg)/(gdouble)(Rc-Lc); else SlopeBkg=0;                    //A redundant precaution here!
*Integ=0; *Area=*Cent1=*Cent2=0.0;
for (ChNo=Lc;ChNo<=Rc;ChNo++)
    {
    Temp=Get1dCounts(WinNo,ChNo,FALSE,0);
    (*Integ)+=Temp;
    (*Cent1)+=(gdouble)ChNo*(gdouble)Temp;
    Bkg=LBkg+SlopeBkg*(gdouble)(ChNo-Lc);
    (*Area)+=(gdouble)Temp-Bkg;
    (*Cent2)+=(gdouble)ChNo*((gdouble)Temp-Bkg);
    }
if (*Area>0.0) (*Cent2)=(*Cent2)/(*Area); else (*Cent2)=0.0;
if (*Integ>0) (*Cent1)=(*Cent1)/(float)(*Integ); else (*Cent1)=0.0;
}
/*--------------------------------------------------------------------------------------------------------------------*/
void DisplayArea(int ChangedLimits)
{
gint WinNo,Lc,Rc,Temp,MaxChan,SNo,Par,R;
GtkWidget *AreaWin,*Label,*Table;
static GtkWidget *LabelLcY,*LabelRcY,*LabelInteg,*LabelArea,*LabelCent1,*LabelCent2,*TextEntryLc,*TextEntryRc,
                 *LabelLcU,*LabelRcU,*LabelCent1U,*LabelCent2U;
gchar Str[30],StrLcY[30],StrRcY[30],StrInteg[30],StrArea[30],StrCent1[30],StrCent2[30];
guint32 Integ;
gdouble Area,Cent1,Cent2;

WinNo=AreaWidget->N; AreaWin=AreaWidget->W; MaxChan=GetMaxChan(WinNo);
SNo=ScreenSpecNo[WinNo][Screen];
Par=Setup.Oned.Par[SNo];
R=Setup.Parameter.Chan[Par-1]/MaxChan;

if (ChangedLimits) { Lc=CLAMP(LcRc[0],0,MaxChan-2); Rc=CLAMP(LcRc[1],Lc+1,MaxChan-1); }
else
   {
   if (Prop[WinNo].One.BoxDrawn == 1) 
      {
      if (Prop[WinNo].One.BoxX2Ch<Prop[WinNo].One.BoxX1Ch) 
         { Temp=Prop[WinNo].One.BoxX1Ch; Prop[WinNo].One.BoxX1Ch=Prop[WinNo].One.BoxX2Ch; Prop[WinNo].One.BoxX2Ch=Temp; }
      Lc=CLAMP(Prop[WinNo].One.BoxX1Ch,0,MaxChan-2); Rc=CLAMP(Prop[WinNo].One.BoxX2Ch,Lc+1,MaxChan-1); 
      }
   else { Lc=CLAMP(Prop[WinNo].One.XLo,0,MaxChan-2); Rc=CLAMP(Prop[WinNo].One.XHi,Lc+1,MaxChan-1); }
   }

sprintf(StrLcY,"%u",Get1dCounts(WinNo,Lc,FALSE,0)); sprintf(StrRcY,"%u",Get1dCounts(WinNo,Rc,FALSE,0));
ComputeArea(WinNo,Lc,Rc,&Integ,&Area,&Cent1,&Cent2);
sprintf(StrInteg,"%d",Integ); sprintf(StrArea,"%.1f",Area); 
sprintf(StrCent1,"%.1f",Cent1); sprintf(StrCent2,"%.1f",Cent2); 

if (!ChangedLimits) 
   { 
   Table=gtk_table_new(6,4,TRUE); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(AreaWin)->vbox),Table,TRUE,TRUE,0); 
   Label=gtk_label_new("    From:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,0,1,0,1);
   sprintf(Str,"%d",Lc); TextEntryLc=gtk_entry_new_with_max_length(6); gtk_entry_set_text(GTK_ENTRY(TextEntryLc),Str);
   LcRc[0]=Lc;
   gtk_signal_connect(GTK_OBJECT(TextEntryLc),"changed",GTK_SIGNAL_FUNC(AreaFromChanged),NULL);
   gtk_table_attach_defaults(GTK_TABLE(Table),TextEntryLc,1,2,0,1);
   Label=gtk_label_new("      To:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,2,3,0,1);
   sprintf(Str,"%d",Rc); TextEntryRc=gtk_entry_new_with_max_length(6); gtk_entry_set_text(GTK_ENTRY(TextEntryRc),Str);
   LcRc[1]=Rc;
   gtk_signal_connect(GTK_OBJECT(TextEntryRc),"changed",GTK_SIGNAL_FUNC(AreaToChanged),NULL);
   gtk_table_attach_defaults(GTK_TABLE(Table),TextEntryRc,3,4,0,1);
   sprintf(Str,"(%.3f %s)",Energy(Par,(gdouble)(Lc*R)),Calibration[Par-1].Units);
   LabelLcU=gtk_label_new(Str); gtk_table_attach_defaults(GTK_TABLE(Table),LabelLcU,1,2,1,2);
   sprintf(Str,"(%.3f %s)",Energy(Par,(gdouble)(Rc*R)),Calibration[Par-1].Units);
   LabelRcU=gtk_label_new(Str); gtk_table_attach_defaults(GTK_TABLE(Table),LabelRcU,3,4,1,2);
   Label=gtk_label_new("  Counts:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,0,1,2,3);
   Label=gtk_label_new("  Counts:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,2,3,2,3);
   Label=gtk_label_new("Integral:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,0,1,3,4);
   Label=gtk_label_new("    Area:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,2,3,3,4);
   Label=gtk_label_new("Raw Centroid:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,0,1,4,5);
   Label=gtk_label_new("Centroid:"); gtk_table_attach_defaults(GTK_TABLE(Table),Label,2,3,4,5);
   LabelLcY=gtk_label_new(StrLcY); gtk_table_attach_defaults(GTK_TABLE(Table),LabelLcY,1,2,2,3);
   LabelRcY=gtk_label_new(StrRcY); gtk_table_attach_defaults(GTK_TABLE(Table),LabelRcY,3,4,2,3);
   LabelInteg=gtk_label_new(StrInteg); gtk_table_attach_defaults(GTK_TABLE(Table),LabelInteg,1,2,3,4);
   LabelArea=gtk_label_new(StrArea); gtk_table_attach_defaults(GTK_TABLE(Table),LabelArea,3,4,3,4);
   LabelCent1=gtk_label_new(StrCent1); gtk_table_attach_defaults(GTK_TABLE(Table),LabelCent1,1,2,4,5);
   LabelCent2=gtk_label_new(StrCent2); gtk_table_attach_defaults(GTK_TABLE(Table),LabelCent2,3,4,4,5);
   sprintf(Str,"(%.3f %s)",Energy(Par,Cent1*R),Calibration[Par-1].Units);
   LabelCent1U=gtk_label_new(Str); gtk_table_attach_defaults(GTK_TABLE(Table),LabelCent1U,1,2,5,6);
   sprintf(Str,"(%.3f %s)",Energy(Par,Cent2*R),Calibration[Par-1].Units);
   LabelCent2U=gtk_label_new(Str); gtk_table_attach_defaults(GTK_TABLE(Table),LabelCent2U,3,4,5,6);
   }
else
   {
   sprintf(Str,"%d",Lc); gtk_entry_set_text(GTK_ENTRY(TextEntryLc),Str);
   sprintf(Str,"%d",Rc); gtk_entry_set_text(GTK_ENTRY(TextEntryRc),Str);
   gtk_label_set_text(GTK_LABEL(LabelLcY),StrLcY); gtk_label_set_text(GTK_LABEL(LabelRcY),StrRcY);
   sprintf(Str,"(%.3f %s)",Energy(Par,(gdouble)(Lc*R)),Calibration[Par-1].Units);
   gtk_label_set_text(GTK_LABEL(LabelLcU),Str); 
   sprintf(Str,"(%.3f %s)",Energy(Par,(gdouble)(Rc*R)),Calibration[Par-1].Units);
   gtk_label_set_text(GTK_LABEL(LabelRcU),Str);
   gtk_label_set_text(GTK_LABEL(LabelInteg),StrInteg); gtk_label_set_text(GTK_LABEL(LabelArea),StrArea);
   gtk_label_set_text(GTK_LABEL(LabelCent1),StrCent1); gtk_label_set_text(GTK_LABEL(LabelCent2),StrCent2);
   sprintf(Str,"(%.3f %s)",Energy(Par,Cent1*R),Calibration[Par-1].Units);
   gtk_label_set_text(GTK_LABEL(LabelCent1U),Str); 
   sprintf(Str,"(%.3f %s)",Energy(Par,Cent2*R),Calibration[Par-1].Units);
   gtk_label_set_text(GTK_LABEL(LabelCent2U),Str);
   }
}
/*------------------------------------------------------------------------------------------------------------------------*/
void UpdateArea(GtkWidget *W,GtkWidget *AreaWin)
{
DisplayArea(1);
gtk_widget_show_all(AreaWin);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void AreaMonitoring(GtkWidget *Mon,gpointer Unused)                                                    //Callback from Area
{
if (GTK_TOGGLE_BUTTON(Mon)->active) { AreaMonitor=TRUE;  gtk_label_set_text(GTK_LABEL(GTK_BIN(Mon)->child),"Monitor On "); }
else                                { AreaMonitor=FALSE; gtk_label_set_text(GTK_LABEL(GTK_BIN(Mon)->child),"Monitor Off"); }
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Area(GtkWidget *W,gpointer Data)
{
gint WinNo;
GtkWidget *AreaWin,*But,*Mon;

if (CalibWinOpen||AreaWinOpen||OvWinOpen||FitWinOpen||PkSearchOpen) 
   { Attention(0,"Another context window open\nPlease close it first"); return; }
WinNo=GPOINTER_TO_INT(Data);
if (Setup.Oned.Par[ScreenSpecNo[WinNo][Screen]]==0) { Attention(0,"Area function not permitted for Hit Pattern"); return; }
AreaWinOpen=TRUE; AreaMonitor=FALSE;
AreaWidget=g_new(struct WidgetInt,1);                           //Create this WidgetInt, to be destroyed in DismissAreaWin

AreaWin=gtk_dialog_new(); gtk_window_set_title(GTK_WINDOW(AreaWin),"Area");
gtk_signal_connect(GTK_OBJECT(AreaWin),"destroy",GTK_SIGNAL_FUNC(AreaWinDestroyed),AreaWin);
gtk_widget_set_uposition(GTK_WIDGET(AreaWin),240,240); gtk_widget_set_usize(GTK_WIDGET(AreaWin),430,150);
Mon=gtk_check_button_new_with_label("Monitor Off"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(AreaWin)->action_area),Mon,TRUE,TRUE,0);
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Mon),FALSE);
gtk_signal_connect(GTK_OBJECT(Mon),"toggled",GTK_SIGNAL_FUNC(AreaMonitoring),NULL);

But=gtk_button_new_with_label("Update"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(AreaWin)->action_area),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(UpdateArea),AreaWin);
But=gtk_button_new_with_label("Dismiss"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(AreaWin)->action_area),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),AreaWin);
AreaWidget->W=AreaWin; AreaWidget->N=WinNo;
DisplayArea(0);
gtk_widget_show_all(AreaWin);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Scroll(GtkAdjustment *HAdj,gpointer Data)
{
gint WinNo,D,MaxChan,i;

WinNo=GPOINTER_TO_INT(Data); MaxChan=GetMaxChan(WinNo);
D=Prop[WinNo].One.XHi-Prop[WinNo].One.XLo;
Prop[WinNo].One.XLo=HAdj->value;
Prop[WinNo].One.XHi=MIN(Prop[WinNo].One.XLo+D,MaxChan);
if (Prop[WinNo].One.CountsAuto) AutoScale1(WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[0]);
gdk_draw_rectangle(PixMap[WinNo],Gc,TRUE,0,0,CanvasW[WinNo],CanvasH[WinNo]);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[4]);
CanvasTitle1(Canvas[WinNo],WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[3]);
XAxis(Canvas[WinNo],WinNo); CountsAxis(Canvas[WinNo],WinNo);
Draw_Maximise_Button(Canvas[WinNo],WinNo);
Display1d(Canvas[WinNo],WinNo); Overlap1d(Canvas[WinNo],WinNo);
Prop[WinNo].One.BoxDrawn=0;
gdk_draw_pixmap(Canvas[WinNo]->window,Gc,PixMap[WinNo],0,0,0,0,
                CanvasW[WinNo],CanvasH[WinNo]);
if (CommonZoom && RealScroll)
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       if ( (ScreenSpecType[i][Screen]==1) && (i != WinNo) &&  //1d spectra only ... not projections (do we need that too?)
            (GetMaxChan(i)==MaxChan) && (Prop[i].Open) )             //Only spectra of the same resolution and open windows
          {
          Prop[i].One.XLo=Prop[WinNo].One.XLo; Prop[i].One.XHi=Prop[WinNo].One.XHi;
          Update1d(i);
          }
       }
   }
RealScroll=TRUE;
}
//----------------------------------------------------------------------------------------------------------------------
void Update1d(gint WinNo)
{
gint MaxChan;

MaxChan=GetMaxChan(WinNo);
if (Prop[WinNo].One.CountsAuto) AutoScale1(WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[0]);                                    //Replaces gtk_widget_modify_style for gtk2
gdk_draw_rectangle(PixMap[WinNo],Gc,TRUE,0,0,CanvasW[WinNo],CanvasH[WinNo]);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[4]);
CanvasTitle1(Canvas[WinNo],WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour1D[3]);
XAxis(Canvas[WinNo],WinNo); CountsAxis(Canvas[WinNo],WinNo);
Draw_Maximise_Button(Canvas[WinNo],WinNo); Display1d(Canvas[WinNo],WinNo); Overlap1d(Canvas[WinNo],WinNo);
gdk_draw_pixmap(Canvas[WinNo]->window,Gc,PixMap[WinNo],0,0,0,0,
                CanvasW[WinNo],CanvasH[WinNo]);
if (AreaWinOpen && AreaMonitor && (AreaWidget->N==WinNo)) UpdateArea(NULL,AreaWidget->W);              //Area Montioring
}
//----------------------------------------------------------------------------------------------------------------------
void IncVScale(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].One.CountsAuto=0; Prop[WinNo].One.CountsHi=2.0*MIN(2.0e9,Prop[WinNo].One.CountsHi);
Prop[WinNo].One.CountsHi=MAX(Prop[WinNo].One.CountsHi,Prop[WinNo].One.CountsLo+4.0);
Update1d(WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void DecVScale(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].One.CountsAuto=0; Prop[WinNo].One.CountsHi=Prop[WinNo].One.CountsHi/2.0;
Prop[WinNo].One.CountsHi=MAX(Prop[WinNo].One.CountsHi,Prop[WinNo].One.CountsLo+4.0);
Update1d(WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void LogVScale(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].One.CountsLog=1-Prop[WinNo].One.CountsLog;
Update1d(WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void AutoVScale(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].One.CountsAuto=1;
Update1d(WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void ApplyManScale(GtkWidget *W,gpointer Data)
{
gint WinNo,X1,X2,Temp;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].One.CountsLo=MAX(0.0,(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(ManScl->Y_Min))));
Prop[WinNo].One.CountsHi=MIN(4.0e9,(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(ManScl->Y_Max))));
X1=atoi(gtk_entry_get_text(GTK_ENTRY(ManScl->X_Min))); X2=atoi(gtk_entry_get_text(GTK_ENTRY(ManScl->X_Max)));
if (X1>X2) { Temp=X1; X1=X2; X2=Temp; }
Temp=GetMaxChan(WinNo);
Prop[WinNo].One.XLo=CLAMP(X1,0,Temp-4); Prop[WinNo].One.XHi=CLAMP(X2,Prop[WinNo].One.XLo+4,Temp);
Update1d(WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void OkManScale(GtkWidget *W,gpointer Data)
{ g_free(ManScl); gtk_widget_destroy(W); }
/*------------------------------------------------------------------------------------------------------------------------------------*/
void ManScale(GtkWidget *W,gpointer Data)                                                             //Mannual entry VScale and HScale
{
GtkWidget *Win,*VBox,*HBox,*Label,*But;
gint WinNo;
gchar Str[20];

ManScl=g_new(struct ManScl,1);
WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].One.CountsAuto=0;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                                    /*Create a modal window*/
gtk_window_set_title(GTK_WINDOW(Win),"Scale Adjustment");                                                              /*Window Title*/
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_set_uposition(GTK_WIDGET(Win),300,300);                                                                     //Window Position
gtk_widget_set_usize(GTK_WIDGET(Win),215,150);                                                                             //Window Size
gtk_container_set_border_width(GTK_CONTAINER(Win),5);                                                                   //Breathing room
VBox=gtk_vbox_new(TRUE,2); gtk_container_add(GTK_CONTAINER(Win),VBox);

HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),HBox);
Label=gtk_label_new(" YMin"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,2);
ManScl->Y_Min=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),ManScl->Y_Min,FALSE,FALSE,0);
if (Prop[WinNo].One.CountsLo>9.0e6) sprintf(Str,"%.2e",Prop[WinNo].One.CountsLo); else sprintf(Str,"%.0f",Prop[WinNo].One.CountsLo);
gtk_entry_set_text(GTK_ENTRY(ManScl->Y_Min),Str);

HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),HBox);
Label=gtk_label_new("YMax"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,2);
ManScl->Y_Max=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),ManScl->Y_Max,FALSE,FALSE,0);
if (Prop[WinNo].One.CountsHi>9.0e6) sprintf(Str,"%.2e",Prop[WinNo].One.CountsHi); else sprintf(Str,"%.0f",Prop[WinNo].One.CountsHi);
gtk_entry_set_text(GTK_ENTRY(ManScl->Y_Max),Str);

HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),HBox);
Label=gtk_label_new(" XMin"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,2);
ManScl->X_Min=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),ManScl->X_Min,FALSE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].One.XLo); gtk_entry_set_text(GTK_ENTRY(ManScl->X_Min),Str);

HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),HBox);
Label=gtk_label_new("XMax"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,2);
ManScl->X_Max=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),ManScl->X_Max,FALSE,FALSE,0);
sprintf(Str,"%d",Prop[WinNo].One.XHi); gtk_entry_set_text(GTK_ENTRY(ManScl->X_Max),Str);                                      /*Default text*/

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label ("Apply"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ApplyManScale),Data);
But=gtk_button_new_with_label ("Ok"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ApplyManScale),Data);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OkManScale),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*-------------------------------------------------------------------------------------------------------------------------*/
void BackTo2d(GtkWidget *W,gpointer Data)                                             //Returns to 2d plot after a projection
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Plot2(WinNo,0,0,0,0);
}
/*-------------------------------------------------------------------------------------------------------------------------*/
void Tile(GtkWidget *W,gpointer Data)                                                              //Called from main_menu.c
{
gint WinNo,WindowX,WindowY,WindowW,WindowH,OpWin,NWin,ScreenRows,ScreenCols,NCols,i,Extra,Row;
gint Cols[SCREEN_ROWS];

for (WinNo=0,NWin=0;WinNo<SCREEN_TOT;WinNo++) if (Prop[WinNo].Open) NWin++;                    //NWin=Number of open windows
if (!NWin) return;                                                                                   //No windows to display
ScreenRows=(gint)sqrt((gdouble)NWin); WindowH=((MONITOR_YRES-TOP_SPACE-BOT_SPACE)/ScreenRows-Y_BORDER-1);
NCols=NWin/ScreenRows; Extra=NWin%ScreenRows;
Cols[0]=NCols; if (Extra) { Cols[0]++; Extra--; }
for (i=1;i<ScreenRows;i++) { Cols[i]=Cols[i-1]+NCols; if (Extra) { Cols[i]++; Extra--; } }
Extra=NWin%ScreenRows; OpWin=0; Row=0; WindowX=0; WindowY=TOP_SPACE+TopOfset;
for (WinNo=0;WinNo<SCREEN_TOT;WinNo++)
    {
    if (Prop[WinNo].Open)
       {
       ScreenCols=NCols; if (Extra) ScreenCols++;
       WindowW=(MONITOR_XRES/ScreenCols)-X_BORDER-1;
       Prop[WinNo].X=WindowX; Prop[WinNo].Y=WindowY; Prop[WinNo].W=WindowW; Prop[WinNo].H=WindowH;
       switch (ScreenSpecType[WinNo][Screen])
          {
          case 1: case 3: case 4: Plot1(WinNo,WindowX,WindowY,WindowW,WindowH); break;       //1d plot or projection from 2d
          case 2: Plot2(WinNo,WindowX,WindowY,WindowW,WindowH);                                                    //2d plot
          }
       OpWin++; WindowX+=WindowW+X_BORDER;
       if (OpWin==Cols[Row]) { Row++; Extra=MAX(Extra-1,0); WindowX=0; WindowY+=(WindowH+Y_BORDER); }
       }
    }
}
/*------------------------------------------------------------------------------------------------------------------------*/
void MiniTile(GtkWidget *W,gpointer Data)                                                         //Called from main_menu.c
{
gint WinNo,WindowX,WindowY,WindowW,WindowH,OpWin;

WindowW=CANVAS_MIN_WIDTH; WindowH=CANVAS_MIN_HEIGHT; WindowX=0; WindowY=TOP_SPACE+TopOfset;
for (WinNo=0,OpWin=0;WinNo<SCREEN_TOT;WinNo++)
    {
    if (Prop[WinNo].Open)
       {
       Prop[WinNo].X=WindowX; Prop[WinNo].Y=WindowY; Prop[WinNo].W=WindowW; Prop[WinNo].H=WindowH;
       switch (ScreenSpecType[WinNo][Screen])
          {
          case 1: case 3: case 4: Plot1(WinNo,WindowX,WindowY,WindowW,WindowH); break;       //1d plot or projection from 2d
          case 2: Plot2(WinNo,WindowX,WindowY,WindowW,WindowH);                                                    //2d plot
          }
       OpWin++; WindowX+=WindowW+X_BORDER;
       if (!(OpWin%SCREEN_COLS)) { WindowX=0; WindowY+=WindowH+Y_BORDER; }
       }
    }
}
/*-------------------------------------------------------------------------------------------------------------------------*/
void CloseAllSpecWindows(GtkWidget *W,gpointer Unused)                              //Called from main_menu.c also readfile.c
{
gint WinNo;

for (WinNo=0;WinNo<SCREEN_TOT;WinNo++)
    if (Prop[WinNo].Open && !((FitWinOpen||PkSearchOpen) && Fit.WinNo==WinNo) )      //Dont destroy if PeakFit/Search active 
       { gtk_widget_destroy(SpecWin[WinNo]); Prop[WinNo].Open=0; }
}
/*---------------------------------------------------------------------------------------------------------------------*/
gint CloseSpecWin(GtkWidget *W,GdkEvent *Event,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
if ((FitWinOpen||PkSearchOpen) && Fit.WinNo==WinNo) 
   { Attention(0,"Cant close\nPlease close Peak Fit/Search window first"); return TRUE; }
if (OvWinOpen && ForOverlap->WinNo==WinNo)
   { Attention(0,"Cant close\nPlease close Overlap Tool first"); return TRUE; }
Prop[WinNo].Open=0; return FALSE;
}
/*---------------------------------------------------------------------------------------------------------------------*/
void FitWinDestroyed(GtkWidget *W,gpointer Data)
{ 
FitWinOpen=FALSE;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void QuickFit(GtkWidget *W,gpointer Data)
{
gint i,WinNo,MaxChan,Temp,Lc,Rc,PeakCh,Ch;
GtkWidget *Label,*But,*VBox,*HBox;
gchar Str[256];
gint SNo,Par,R,ChNo;
gchar Name[MAX_TEXT_FIELD],Units[8];
gdouble Area,CalPos,CalWidth,EPos,EHt,EWidth,EArea;
guint32 PeakCounts;
void InitializeFitParas(void);

if (CalibWinOpen||AreaWinOpen||OvWinOpen||FitWinOpen||PkSearchOpen) 
   { Attention(0,"Another context window open\nPlease close it first"); return; }

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
Par=Setup.Oned.Par[SNo]; 
if (Par==0) { Attention(0,"Quick Fit no permitted for Hit Pattern spectrum"); return; }
strcpy(Name,Setup.Parameter.Name[Par-1]);
MaxChan=GetMaxChan(WinNo); R=Setup.Parameter.Chan[Par-1]/MaxChan; 
strncpy(Units,Calibration[Par-1].Units,6); Units[7]='\0';

if (Prop[WinNo].One.BoxDrawn == 1)
   {
   if (Prop[WinNo].One.BoxX2Ch<Prop[WinNo].One.BoxX1Ch)
      { Temp=Prop[WinNo].One.BoxX1Ch; Prop[WinNo].One.BoxX1Ch=Prop[WinNo].One.BoxX2Ch; Prop[WinNo].One.BoxX2Ch=Temp; }
   Lc=CLAMP(Prop[WinNo].One.BoxX1Ch,0,MaxChan-7); Rc=CLAMP(Prop[WinNo].One.BoxX2Ch,Lc+1,MaxChan-1);
   }
else { Lc=CLAMP(Prop[WinNo].One.XLo,0,MaxChan-7); Rc=CLAMP(Prop[WinNo].One.XHi,Lc+1,MaxChan-1); }
if ((Rc-Lc)<5) { Attention(0,"The selected region has less than 6 points!"); return; }

InitializeFitParas();
memset(Fit.Err, 0, sizeof(Fit.Err));

Fit.WinNo=WinNo; Fit.Lc=Lc; Fit.NPts=Rc-Lc+1; Fit.NPeaks=1; Fit.TailOpt=None;

for (i=0;i<Fit.NPts;i++) Fit.Data[i]=(gdouble)Get1dCounts(WinNo,Lc+i,FALSE,0);                                          //Copy data values
Fit.P[0]=Fit.Data[0]; Fit.F[0]=Variable;                                                                                 //Left Background
Fit.P[1]=Fit.Data[Fit.NPts-1]; Fit.F[1]=Variable;                                                                       //Right Background
for (i=0,Fit.P[3]=Lc,Fit.P[8]=0.0;i<Fit.NPts;i++)                                               //Locate maximum counts in selected region
    if ( (Fit.Data[i]>Fit.P[8]) && (i+Lc>5) ) { Fit.P[8]=Fit.Data[i]; Fit.P[3]=i+Lc; }                              //Avoid 'noise' region
Fit.P[8]=MAX(0.0,Fit.P[8]-LinBkg(Fit.P[3]));                                       //Subtract background for proper peak height estimation
Fit.F[3]=Variable; Fit.F[8]=Variable;
Fit.P[5]=0.0; Fit.F[5]=Fixed;                                                                                                   //Asymmetry

PeakCounts=(guint32)Fit.P[8]; PeakCh=Fit.P[3];
for (Ch=PeakCh;Ch>MAX(Lc,PeakCh-10);Ch--)                                              /*Descend to the left to find the LHS half-width*/
    if ((Fit.Data[Ch-Fit.Lc]-LinBkg(Ch))<PeakCounts/2) break;
Fit.P[4]=PeakCh-Ch;
for (Ch=PeakCh;Ch<MIN(Rc,PeakCh+10);Ch++)                                     /*Descend to the right to find the RHS half-width*/
    if ((Fit.Data[Ch-Fit.Lc]-LinBkg(Ch))<PeakCounts/2) break;
Fit.P[4]+=(Ch-PeakCh); 
Fit.P[4]=MAX(3.0,Fit.P[4]); // Clamp FWHM to prevent division by zero / nan
Fit.F[4]=Variable;                                                                                            //FWHM

if (DoFit(Fit.NPts,6*Fit.NPeaks+3,BiGauss,0,NULL,Fit.Data,Fit.P,Fit.Err,Fit.F,Fit.Messg) != 0)
   {
   Attention(0,Fit.Messg);
   return;
   }

FitWinOpen=TRUE;
Refresh(NULL,GINT_TO_POINTER(Fit.WinNo));

FitWin=gtk_dialog_new(); gtk_grab_add(FitWin);                                                                       //Create modal window
gtk_window_set_title(GTK_WINDOW(FitWin),"Quick Single Peak Fit");
gtk_signal_connect(GTK_OBJECT(FitWin),"destroy",GTK_SIGNAL_FUNC(FitWinDestroyed),FitWin);
gtk_container_set_border_width(GTK_CONTAINER(FitWin),5);
VBox=gtk_vbox_new(FALSE,2); gtk_container_add(GTK_CONTAINER(GTK_DIALOG(FitWin)->action_area),VBox);
Label=gtk_label_new("Quick Fit finds and fits a single peak in the selected region");
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
HBox=gtk_hbox_new(TRUE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Dismiss"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),FitWin);

CalWidth=Energy(Par,(Fit.P[3]+0.5*Fit.P[4])*R)-Energy(Par,(Fit.P[3]-0.5*Fit.P[4])*R); CalPos=Energy(Par,Fit.P[3]*R);
EPos=Fit.Err[3]/Fit.P[3]; EHt=Fit.Err[8]/Fit.P[8]; EWidth=Fit.Err[4]/Fit.P[4]; EArea=sqrt(EHt*EHt+EWidth*EWidth);
for (ChNo=Lc,Area=0;ChNo<=Rc;ChNo++) Area+=PeakFunc(ChNo-Lc,Fit.P,0);

sprintf(Str,"Position: %.3f +- %.3f (%.3f +- %.3f) %s",Fit.P[3],Fit.Err[3],CalPos,EPos*CalPos,Units); 
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(FitWin)->vbox),Label,FALSE,FALSE,0);

sprintf(Str,"FWHM: %.3f +- %.3f (%.3f +- %.3f) %s",Fit.P[4],Fit.Err[4],CalWidth,EWidth*CalWidth,Units); 
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(FitWin)->vbox),Label,FALSE,FALSE,0);

sprintf(Str,"Area: %.1f +- %.1f",Area,EArea*Area); 
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(FitWin)->vbox),Label,FALSE,FALSE,0);

gtk_widget_show_all(FitWin);
}
//----------------------------------------------------------------------------------------------------------------------
void TOptChanged(GtkWidget *W,gpointer Unused)                                                     //Tail Option changed
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"None")) Fit.TailOpt=None;
else if (!strcmp(Str,"Left")) Fit.TailOpt=Left;
else if (!strcmp(Str,"Right")) Fit.TailOpt=Right;
else if (!strcmp(Str,"Both")) Fit.TailOpt=Both;
else Fit.TailOpt=None;                                                  //In case the user types junk into the Combo box
}
//----------------------------------------------------------------------------------------------------------------------
void SOptChanged(GtkWidget *W,GtkWidget *But)                                                     //Shape Option changed
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Same for all")) Fit.ShapeOpt=Same;
else if (!strcmp(Str,"Independent")) Fit.ShapeOpt=Indep;
else if (!strcmp(Str,"From Shape Calib")) Fit.ShapeOpt=ShapeCal;
else Fit.ShapeOpt=Same;                                                 //In case the user types junk into the Combo box
if (Fit.ShapeOpt==ShapeCal) gtk_widget_set_sensitive(But,TRUE); else gtk_widget_set_sensitive(But,FALSE);
}
//----------------------------------------------------------------------------------------------------------------------
void StatsOptChanged(GtkWidget *W,gpointer Unused)                                           //Statistics Option changed
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Low Statistics")) Fit.StatsOpt=LowStat;
else Fit.StatsOpt=Normal;
}
//----------------------------------------------------------------------------------------------------------------------
void DeleteAllPeaks(GtkWidget *W,gpointer Data)
{
Fit.NPeaks=0;
if (GTK_IS_WIDGET(Fit.Table2)) gtk_widget_destroy(Fit.Table2);
Fit.Table2=gtk_table_new(Fit.NPeaks,13,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Fit.ScrlW),Fit.Table2);
gtk_widget_show_all(Fit.ScrlW);
Refresh(NULL,GINT_TO_POINTER(Fit.WinNo));   //May as well refresh the whole display rather than erase all the peak markers
}
//----------------------------------------------------------------------------------------------------------------------
void MouseContext(GtkWidget *W,gpointer Data)                                                  /*Toggle Fit.MouseContext*/
{ Fit.MouseContext=1-Fit.MouseContext; }
//----------------------------------------------------------------------------------------------------------------------
void StartFit(GtkWidget *W,gpointer IterLabel)
{
pthread_t FitJob;

if (Fit.Busy) { Attention(0,"Already running!"); return; }
Fit.Busy=TRUE; pthread_create(&FitJob,NULL,FitThread,IterLabel);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ShowFit(GtkWidget *W,gpointer Data)
{
static GdkColor Red   =  {0,0xffff,0x0000,0x0000};
static GdkColor Blue  =  {0,0x0000,0x0000,0xffff};
GtkWidget *Win,*VBox,*Text,*SBar,*HBox,*But;
gchar Str[300];
gint i,j,WinNo,SNo,Par,Lc,Rc,R,MaxChan,ChNo;
gchar Name[MAX_TEXT_FIELD],Units[8];
gdouble Area,CalPos,CalWidth,EPos,EHt,EWidth,EArea;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
Par=Setup.Oned.Par[SNo]; strcpy(Name,Setup.Parameter.Name[Par-1]);
MaxChan=GetMaxChan(WinNo); R=Setup.Parameter.Chan[Par-1]/MaxChan; Lc=Fit.Lc; Rc=Fit.Lc+Fit.NPts-1;
strncpy(Units,Calibration[Par-1].Units,6); Units[7]='\0';

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Fit Results"); gtk_widget_set_usize(GTK_WIDGET(Win),650,200);
VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
Text=gtk_text_new(NULL,NULL); gtk_widget_set_usize(GTK_WIDGET(Text),630,150);
gtk_box_pack_start(GTK_BOX(HBox),Text,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(Text),TRUE); gtk_text_set_editable(GTK_TEXT(Text),FALSE);
SBar=gtk_vscrollbar_new(GTK_TEXT(Text)->vadj);
gtk_box_pack_start(GTK_BOX(HBox),SBar,FALSE,FALSE,0);

sprintf(Str,"Spec. No: %d (%s)\n",SNo,Name); gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,Str,-1);
sprintf(Str,"From: %4d (%4.2f %7s) To: %4d (%4.2f %7s)\n",Lc,Energy(Par,(gdouble)(Lc*R)),Units,Rc,Energy(Par,(gdouble)(Rc*R)),Units);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,Str,-1);
sprintf(Str,"  No            Position                           FWHM                        Asym      LTail      RTail         Area\n");
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,Str,-1);
for (i=0;i<Fit.NPeaks;i++)
    {
    j=6*i;
    CalWidth=Energy(Par,(Fit.P[j+3]+0.5*Fit.P[j+4])*R)-Energy(Par,(Fit.P[j+3]-0.5*Fit.P[j+4])*R); CalPos=Energy(Par,Fit.P[j+3]*R);
    EPos=Fit.Err[j+3]/Fit.P[j+3]; EHt=Fit.Err[j+8]/Fit.P[j+8]; EWidth=Fit.Err[j+4]/Fit.P[j+4]; EArea=sqrt(EHt*EHt+EWidth*EWidth);
    for (ChNo=Lc,Area=0;ChNo<=Rc;ChNo++) Area+=PeakFunc(ChNo-Lc,Fit.P,i);
    sprintf(Str,"%4d   %4.2f   (%4.2f %7s)     %3.3f   (%3.3f %7s)     %3.3f    %3.3f    %3.3f %8.1f\n",
               i+1,Fit.P[j+3],CalPos,Units,Fit.P[j+4],CalWidth,Units,Fit.P[j+5],Fit.P[j+6],Fit.P[j+7],Area);
    gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,Str,-1);
    sprintf(Str,"Errors:+-%.3f  (+-%.3f %7s)    +-%.3f (+-%.3f %7s)    +-%.3f   +-%.3f   +-%.3f       +-%.3f\n",
               Fit.Err[j+3],EPos*CalPos,Units,Fit.Err[j+4],EWidth*CalWidth,Units,Fit.Err[j+5],Fit.Err[j+6],Fit.Err[j+7],EArea*Area);
    gtk_text_insert(GTK_TEXT(Text),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,Fit.Messg,-1);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Close"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*--------------------------------------------------------------------------------------------------------------------------------------*/
void SaveFit(GtkWidget *W,gpointer Data)
{
FILE *Fp;
time_t Time;
gint i,j,WinNo,SNo,Par,Lc,Rc,R,MaxChan,ChNo;
gchar Name[MAX_TEXT_FIELD],Units[8];
gchar TailOpts[4][8]  = {"None","Left","Right","Both"};
gchar ShapeOpts[3][10] = {"Same","Indep","ShapeCal"};
gdouble Area,CalPos,CalWidth,EPos,EHt,EWidth,EArea;

Time=time(NULL); WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
Par=Setup.Oned.Par[SNo]; strcpy(Name,Setup.Parameter.Name[Par-1]);
MaxChan=GetMaxChan(WinNo); R=Setup.Parameter.Chan[Par-1]/MaxChan; Lc=Fit.Lc; Rc=Fit.Lc+Fit.NPts-1;
strncpy(Units,Calibration[Par-1].Units,6); Units[7]='\0';
if ((Fp=fopen("fit.out","a")) != NULL)
   {
   fprintf(Fp,"Spec. No: %d (%s) %s",SNo+1,Name,ctime(&Time));
   fprintf(Fp,"From: %4d (%4.2f %7s) To: %4d (%4.2f %7s)\n",Lc,Energy(Par,(gdouble)(Lc*R)),Units,Rc,Energy(Par,(gdouble)(Rc*R)),Units);
   fprintf(Fp,"Tail Option: %s  Shape Option: %s\n",TailOpts[Fit.TailOpt],ShapeOpts[Fit.ShapeOpt]);
   fprintf(Fp,"Background: Left=%.3f +-%.3f",Fit.P[0],Fit.Err[0]);
   fprintf(Fp,"  Right=%.3f +-%.3f",Fit.P[1],Fit.Err[1]);
   fprintf(Fp,"  Quad=%.3f +-%.3f\n",Fit.P[2],Fit.Err[2]);
   fprintf(Fp,"  No          Position                    FWHM                      Asym     LTail     RTail         Area\n");
   for (i=0;i<Fit.NPeaks;i++)
       {
       j=6*i; 
       CalWidth=Energy(Par,(Fit.P[j+3]+0.5*Fit.P[j+4])*R)-Energy(Par,(Fit.P[j+3]-0.5*Fit.P[j+4])*R); CalPos=Energy(Par,Fit.P[j+3]*R);
       EPos=Fit.Err[j+3]/Fit.P[j+3]; EHt=Fit.Err[j+8]/Fit.P[j+8]; EWidth=Fit.Err[j+4]/Fit.P[j+4]; EArea=sqrt(EHt*EHt+EWidth*EWidth);
       for (ChNo=Lc,Area=0;ChNo<=Rc;ChNo++) Area+=PeakFunc(ChNo-Lc,Fit.P,i);
       fprintf(Fp,"%4d   %4.2f   (%4.2f %7s)     %3.3f   (%3.3f %7s)     %3.3f    %3.3f    %3.3f %8.1f\n",
               i+1,Fit.P[j+3],CalPos,Units,Fit.P[j+4],CalWidth,Units,Fit.P[j+5],Fit.P[j+6],Fit.P[j+7],Area);
       fprintf(Fp,"Errors:+-%.3f  (+-%.3f %7s)    +-%.3f (+-%.3f %7s)    +-%.3f   +-%.3f   +-%.3f       +-%.3f\n",
               Fit.Err[j+3],EPos*CalPos,Units,Fit.Err[j+4],EWidth*CalWidth,Units,Fit.Err[j+5],Fit.Err[j+6],Fit.Err[j+7],EArea*Area);
       }
   fprintf(Fp,"%s\n",Fit.Messg);
   fprintf(Fp,"------------------------------------------------------------------------------------------------------------\n");
   fclose(Fp);
   }
}
/*--------------------------------------------------------------------------------------------------------------------*/
void DismissFitWin(GtkWidget *W,GtkWidget *Win)
{ if (!Fit.Busy) gtk_widget_destroy(Win); }
//----------------------------------------------------------------------------------------------------------------------
void ReadShapeCalib(GtkWidget *W,gpointer Unused)
{
Attention(0,"Shape Calib not implemented\n");
}
//----------------------------------------------------------------------------------------------------------------------
void FitWidget(GtkWidget *W,gpointer Data)
{
gint i,WinNo,Lc,Rc,MaxChan,Temp,SNo,PeakX;
GtkWidget *VBox,*HBox,*HBox1,*HeadBut,*Label,*Entry,*Combo,*But,*Frame,*VBox2,*IterLabel,*StartBut;
gchar Str[40];
static gchar BkHead[3][25]={"Background Left","Background Right","Background Quad"};
static GdkColor RedC    = {0,0xFFFF,0x0000,0x0000};
static GdkColor WhiteC  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor BlackC  = {0,0x0000,0x0000,0x0000};
GtkStyle *Style1,*Style2,*Style3;
GList *TGList,*SGList,*MGList;

if (CalibWinOpen||AreaWinOpen||OvWinOpen||FitWinOpen||PkSearchOpen) 
   { Attention(0,"Another context window open\nPlease close it first"); return; }

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
if (Setup.Oned.Par[SNo]==0) { Attention(0,"Peak Fit not permitted for Hit Pattern spectrum"); return; }

Style1=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { Style1->fg[i]=Style1->text[i]=WhiteC; Style1->bg[i]=RedC; }
Style2=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { Style2->fg[i]=Style2->text[i]=WhiteC; Style2->bg[i]=BlackC; }
Style3=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { Style3->fg[i]=Style3->text[i]=RedC; Style3->bg[i]=WhiteC; }

MaxChan=GetMaxChan(WinNo);
if (Prop[WinNo].One.BoxDrawn == 1) 
   {
   if (Prop[WinNo].One.BoxX2Ch<Prop[WinNo].One.BoxX1Ch) 
      { Temp=Prop[WinNo].One.BoxX1Ch; Prop[WinNo].One.BoxX1Ch=Prop[WinNo].One.BoxX2Ch; Prop[WinNo].One.BoxX2Ch=Temp; }
   Lc=CLAMP(Prop[WinNo].One.BoxX1Ch,0,MaxChan-7); Rc=CLAMP(Prop[WinNo].One.BoxX2Ch,Lc+1,MaxChan-1); 
   }
else { Lc=CLAMP(Prop[WinNo].One.XLo,0,MaxChan-7); Rc=CLAMP(Prop[WinNo].One.XHi,Lc+1,MaxChan-1); }
if ((Rc-Lc)<5) { Attention(0,"The selected region has less than 6 points!"); return; }

FitWinOpen=TRUE; Fit.WinNo=WinNo; Fit.Lc=Lc; Fit.NPts=Rc-Lc+1;
for (i=0;i<Fit.NPts;i++) Fit.Data[i]=(gdouble)Get1dCounts(WinNo,Lc+i,FALSE,0);                        //Copy data values
Fit.P[0]=Fit.Data[0]; Fit.F[0]=Variable;                                                               //Left Background
if (Lc==0) Fit.P[0]=Fit.Data[1];                                                                 //To avoid channel zero
Fit.P[1]=Fit.Data[Fit.NPts-1]; Fit.F[1]=Variable;                                                     //Right Background
Fit.NPeaks=0;                       //To start with, get rid of all peaks, then (see below) get from Peak Search results

FitWin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(FitWin),"Fit Interaction Window");
gtk_widget_set_size_request(FitWin,-1,370);
gtk_window_set_transient_for(GTK_WINDOW(FitWin),GTK_WINDOW(SpecWin[WinNo]));                         //Window visibility
gtk_signal_connect(GTK_OBJECT(FitWin),"destroy",GTK_SIGNAL_FUNC(FitWinDestroyed),FitWin);
gtk_container_set_border_width(GTK_CONTAINER(FitWin),5);
VBox=gtk_vbox_new(FALSE,2); gtk_container_add(GTK_CONTAINER(FitWin),VBox); 

Fit.MouseContext=AddPeaks;
HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Frame=gtk_frame_new("Selected Region"); gtk_box_pack_start(GTK_BOX(HBox),Frame,FALSE,FALSE,0);
HBox1=gtk_hbox_new(TRUE,0); gtk_container_add(GTK_CONTAINER(Frame),HBox1); 
gtk_container_set_border_width(GTK_CONTAINER(HBox1),5);
sprintf(Str,"From %d To %d",Lc,Rc); Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox1),Label,FALSE,FALSE,0);

Label=gtk_label_new("Background:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Fit.Table1=gtk_table_new(2,6,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Fit.Table1),1); gtk_table_set_col_spacings(GTK_TABLE(Fit.Table1),1);
for (i=0;i<3;i++)                                                                             //Headings for each column
    {
    HeadBut=gtk_button_new_with_label(BkHead[i]); SetStyleRecursively(HeadBut,Style1);
    gtk_table_attach(GTK_TABLE(Fit.Table1),HeadBut,2*i,2*i+2,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }
Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);                //Bkg Left
sprintf(Str,"%.3f",Fit.P[0]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(0));
gtk_table_attach(GTK_TABLE(Fit.Table1),Entry,0,1,1,2,GTK_FILL,GTK_SHRINK,0,0);
if (Fit.F[0]==Variable) strcpy(Str,"Variable"); else strcpy(Str,"Fixed");
But=gtk_toggle_button_new_with_label(Str);
gtk_signal_connect(GTK_OBJECT(But),"toggled",GTK_SIGNAL_FUNC(FToggle),GINT_TO_POINTER(0));
gtk_table_attach(GTK_TABLE(Fit.Table1),But,1,2,1,2,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);               //Bkg Right
sprintf(Str,"%.3f",Fit.P[1]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(1));
gtk_table_attach(GTK_TABLE(Fit.Table1),Entry,2,3,1,2,GTK_FILL,GTK_SHRINK,0,0);
if (Fit.F[1]==Variable) strcpy(Str,"Variable"); else strcpy(Str,"Fixed");
But=gtk_toggle_button_new_with_label(Str);
gtk_signal_connect(GTK_OBJECT(But),"toggled",GTK_SIGNAL_FUNC(FToggle),GINT_TO_POINTER(1));
gtk_table_attach(GTK_TABLE(Fit.Table1),But,3,4,1,2,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,80,-1);                //Bkg Quad
sprintf(Str,"%.3f",Fit.P[2]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PChanged),GINT_TO_POINTER(2));
gtk_table_attach(GTK_TABLE(Fit.Table1),Entry,4,5,1,2,GTK_FILL,GTK_SHRINK,0,0);
if (Fit.F[2]==Variable) strcpy(Str,"Variable"); else strcpy(Str,"Fixed");
But=gtk_toggle_button_new_with_label(Str);
gtk_signal_connect(GTK_OBJECT(But),"toggled",GTK_SIGNAL_FUNC(FToggle),GINT_TO_POINTER(2));
gtk_table_attach(GTK_TABLE(Fit.Table1),But,5,6,1,2,GTK_FILL,GTK_SHRINK,0,0);

gtk_box_pack_start(GTK_BOX(HBox),Fit.Table1,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);                             //TailOpt
Label=gtk_label_new("Tail:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Combo=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBox),Combo,FALSE,FALSE,0); TGList=NULL;
gtk_widget_set_size_request(Combo,70,-1);
TGList=g_list_append(TGList,"None");  TGList=g_list_append(TGList,"Left"); 
TGList=g_list_append(TGList,"Right"); TGList=g_list_append(TGList,"Both");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),TGList);
switch (Fit.TailOpt)
   {
   case None:  strcpy(Str,"None");  break;
   case Left:  strcpy(Str,"Left");  break;
   case Right: strcpy(Str,"Right"); break;
   case Both:  strcpy(Str,"Both");  break;
   default:    strcpy(Str,"None"); 
   }
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(TOptChanged),NULL);

Label=gtk_label_new("Shape:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);                         //ShapeOpt
Combo=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBox),Combo,FALSE,FALSE,0); SGList=NULL;
gtk_widget_set_size_request(Combo,148,-1);
SGList=g_list_append(SGList,"Same for all");  SGList=g_list_append(SGList,"Independent"); 
SGList=g_list_append(SGList,"From Shape Calib");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),SGList);
switch (Fit.TailOpt)
   {
   case Same:     strcpy(Str,"Same for all");  break;
   case Indep:    strcpy(Str,"Independent");  break;
   case ShapeCal: strcpy(Str,"From Shape Calib"); break;
   default:       strcpy(Str,"Same for all"); 
   }
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);
//The signal for this appears after the definition of the next button

But=gtk_button_new_with_label("Read Shape\nCalib File");
if (Fit.TailOpt) gtk_widget_set_sensitive(But,TRUE); else gtk_widget_set_sensitive(But,FALSE);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReadShapeCalib),NULL);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,10);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(SOptChanged),But);

But=gtk_button_new_with_label("Peak Parameters given below\nV=Variable, F=Fixed, C=Constrained, N=n/a");
SetStyleRecursively(But,Style2);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);

VBox2=gtk_vbox_new(FALSE,0); gtk_container_set_border_width(GTK_CONTAINER(VBox2),10);
gtk_box_pack_start(GTK_BOX(VBox),VBox2,TRUE,TRUE,0);
HBox=gtk_hbox_new(FALSE,1);
gtk_box_pack_start(GTK_BOX(VBox2),HBox,FALSE,FALSE,0);

HeadBut=gtk_button_new_with_label("No"); SetStyleRecursively(HeadBut,Style1); gtk_widget_set_size_request(HeadBut,26,-1);
gtk_box_pack_start(GTK_BOX(HBox),HeadBut,FALSE,FALSE,0);
HeadBut=gtk_button_new_with_label("Position"); SetStyleRecursively(HeadBut,Style1); gtk_widget_set_size_request(HeadBut,93,-1);
gtk_box_pack_start(GTK_BOX(HBox),HeadBut,FALSE,FALSE,0);
HeadBut=gtk_button_new_with_label("FWHM"); SetStyleRecursively(HeadBut,Style1); gtk_widget_set_usize(HeadBut,95,22);
gtk_box_pack_start(GTK_BOX(HBox),HeadBut,FALSE,FALSE,0);
HeadBut=gtk_button_new_with_label("Asymmetry"); SetStyleRecursively(HeadBut,Style1); gtk_widget_set_size_request(HeadBut,96,-1);
gtk_box_pack_start(GTK_BOX(HBox),HeadBut,FALSE,FALSE,0);
HeadBut=gtk_button_new_with_label("Left Tail"); SetStyleRecursively(HeadBut,Style1); gtk_widget_set_size_request(HeadBut,95,-1);
gtk_box_pack_start(GTK_BOX(HBox),HeadBut,FALSE,FALSE,0);
HeadBut=gtk_button_new_with_label("Right Tail"); SetStyleRecursively(HeadBut,Style1); gtk_widget_set_size_request(HeadBut,96,-1);
gtk_box_pack_start(GTK_BOX(HBox),HeadBut,FALSE,FALSE,0);
HeadBut=gtk_button_new_with_label("Height"); SetStyleRecursively(HeadBut,Style1); gtk_widget_set_size_request(HeadBut,96,-1);
gtk_box_pack_start(GTK_BOX(HBox),HeadBut,FALSE,FALSE,0);

Fit.ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Fit.ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox2),Fit.ScrlW,TRUE,TRUE,0);
Fit.Table2=gtk_table_new(Fit.NPeaks,13,FALSE);
PopulateFitTable();
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Fit.ScrlW),Fit.Table2); 

for (i=0;i<Setup.Oned.NPeaks[SNo];i++)              //Get peaks (if any) within (Lc,Rc) from those stored by Peak Search
    {
    if ((Setup.Oned.Peaks[i][SNo]>Lc) && (Setup.Oned.Peaks[i][SNo]<Rc))
       {
       PeakX=ORIGIN_X+XSlope[WinNo]*(Setup.Oned.Peaks[i][SNo]-Prop[WinNo].One.XLo);
       AddPeak(WinNo,PeakX);
       }
    }

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0); 
Frame=gtk_frame_new("Set mouse middle button to:"); gtk_box_pack_start(GTK_BOX(HBox),Frame,FALSE,FALSE,0);
HBox1=gtk_hbox_new(TRUE,0); gtk_container_add(GTK_CONTAINER(Frame),HBox1); 
But=gtk_radio_button_new_with_label(NULL,"Add Peak");
gtk_signal_connect(GTK_OBJECT(But),"toggled",GTK_SIGNAL_FUNC(MouseContext),NULL);
gtk_box_pack_start(GTK_BOX(HBox1),But,TRUE,TRUE,0);
But=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(But)),"Delete Peak");
gtk_box_pack_start(GTK_BOX(HBox1),But,TRUE,TRUE,0);
But=gtk_button_new_with_label("Delete All\nPeaks"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DeleteAllPeaks),NULL);
But=gtk_button_new_with_label("Try"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Refresh),GINT_TO_POINTER(Fit.WinNo));
StartBut=gtk_button_new_with_label("(Re)Start\nFit"); gtk_box_pack_start(GTK_BOX(HBox),StartBut,TRUE,TRUE,0); 
But=gtk_button_new_with_label("Show\nOutput"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ShowFit),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Save\nOutput"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveFit),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Dismiss"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DismissFitWin),FitWin);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,4);
Label=gtk_label_new("Modified Chisq:"); 
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Combo=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBox),Combo,FALSE,FALSE,0); MGList=NULL;
gtk_widget_set_size_request(Combo,148,-1);
MGList=g_list_append(MGList,"Normal");  MGList=g_list_append(MGList,"Low Statistics");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),MGList);
switch (Fit.StatsOpt)
   {
   case Normal:   strcpy(Str,"Normal");  break;
   case LowStat:  strcpy(Str,"Low Statistics");  break;
   default:       strcpy(Str,"Normal");
   }
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(StatsOptChanged),NULL);

IterLabel=gtk_label_new("Output will be displayed here"); SetStyleRecursively(IterLabel,Style3);
gtk_box_pack_start(GTK_BOX(HBox),IterLabel,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(StartBut),"clicked",GTK_SIGNAL_FUNC(StartFit),GTK_WIDGET(IterLabel));

gtk_widget_show_all(FitWin);
gtk_style_unref(Style1); gtk_style_unref(Style2); gtk_style_unref(Style3);
g_list_free(TGList); g_list_free(SGList); g_list_free(MGList);
}
/*------------------------------------------------------------------------------------------------------------------*/
void OverlapSpecNo(GtkWidget *W,gpointer Data)
{ 
gint Row;

Row=GPOINTER_TO_INT(Data);
Prop[ForOverlap->WinNo].One.OvSpec[Row]=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ForOverlap->Spec[Row]))-1; 
Update1d(ForOverlap->WinNo);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void OverlapHShift(GtkWidget *W,gpointer Data)
{ 
gint Row;

Row=GPOINTER_TO_INT(Data);
Prop[ForOverlap->WinNo].One.OvHShift[Row]=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ForOverlap->HShft[Row])); 
Update1d(ForOverlap->WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void OverlapVShift(GtkWidget *W,gpointer Data)
{ 
gint Row;

Row=GPOINTER_TO_INT(Data);
Prop[ForOverlap->WinNo].One.OvVShift[Row]=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ForOverlap->VShft[Row])); 
Update1d(ForOverlap->WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void ToggleOverlap(GtkWidget *W,gpointer Data)
{
gint Row,i;

Row=GPOINTER_TO_INT(Data);
if (GTK_TOGGLE_BUTTON(W)->active)
   { Prop[ForOverlap->WinNo].One.Overlap[Row]=TRUE; Prop[ForOverlap->WinNo].One.OverlapOff=FALSE; }
else
   {
   Prop[ForOverlap->WinNo].One.Overlap[Row]=FALSE; Prop[ForOverlap->WinNo].One.OverlapOff=TRUE;
   for (i=0;i<MIN(Setup.Oned.N,MAX_OVERLAP);i++) 
       if (Prop[ForOverlap->WinNo].One.Overlap[i]) { Prop[ForOverlap->WinNo].One.OverlapOff=FALSE; break; }
   }
Update1d(ForOverlap->WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void OverlapWinDestroyed(GtkWidget *W,GtkWidget *OvWin)
{ 
OvWinOpen=FALSE; 
g_free(ForOverlap);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Overlap(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo,i;
gfloat MaxHShift,MaxVShift;
GtkWidget *OvWin,*VBox,*HBox,*But,*ScrollW,*CheckBut,*Table;
const GdkColor BlueC   = {0,0x0000,0x0000,0x5555};
const GdkColor WhiteC  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkAdjustment *Adj;
GtkStyle *Style;

if (CalibWinOpen||AreaWinOpen||OvWinOpen||FitWinOpen||PkSearchOpen)
   { Attention(0,"Another context window open\nPlease close it first"); return; }
OvWinOpen=TRUE;

Style=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { Style->fg[i]=Style->text[i]=WhiteC; Style->bg[i]=BlueC; }

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
MaxHShift=0.5*GetMaxChan(WinNo); MaxVShift=0.5*(Prop[WinNo].One.CountsHi-Prop[WinNo].One.CountsLo);

OvWin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(OvWin),"Overlap Tool");
gtk_widget_set_size_request(GTK_WIDGET(OvWin),210,300);
gtk_window_set_transient_for(GTK_WINDOW(OvWin),GTK_WINDOW(SpecWin[WinNo]));                           //Window visibility
gtk_signal_connect(GTK_OBJECT(OvWin),"destroy",GTK_SIGNAL_FUNC(OverlapWinDestroyed),OvWin);

VBox=gtk_vbox_new(FALSE,6); gtk_container_add(GTK_CONTAINER(OvWin),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),6);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Spec#"); SetStyleRecursively(But,Style); gtk_widget_set_size_request(But,44,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("HShift"); SetStyleRecursively(But,Style); gtk_widget_set_size_request(But,44,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("VShift"); SetStyleRecursively(But,Style); gtk_widget_set_size_request(But,44,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("On"); SetStyleRecursively(But,Style); gtk_widget_set_size_request(But,30,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);
Table=gtk_table_new(MAX_OVERLAP,4,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),1); gtk_table_set_col_spacings(GTK_TABLE(Table),5);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);

ForOverlap=g_new(struct ForOverlap,1);
ForOverlap->WinNo=WinNo;
for (i=0;i<MIN(Setup.Oned.N,MAX_OVERLAP);i++)
    {
    Adj=(GtkAdjustment *)gtk_adjustment_new((gfloat)Prop[WinNo].One.OvSpec[i]+1,1.0,Setup.Oned.N,1.0,2.0,0.0);
    ForOverlap->Spec[i]=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(ForOverlap->Spec[i]),TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(ForOverlap->Spec[i]),44,-1);
    gtk_table_attach(GTK_TABLE(Table),ForOverlap->Spec[i],0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(OverlapSpecNo),GINT_TO_POINTER(i)); 

    Adj=(GtkAdjustment *)gtk_adjustment_new((gfloat)Prop[WinNo].One.OvHShift[i],-MaxHShift,MaxHShift,1.0,0.1*MaxHShift,0.0);
    ForOverlap->HShft[i]=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(ForOverlap->HShft[i]),TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(ForOverlap->HShft[i]),44,-1);
    gtk_table_attach(GTK_TABLE(Table),ForOverlap->HShft[i],1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(OverlapHShift),GINT_TO_POINTER(i)); 

    Adj=(GtkAdjustment *)gtk_adjustment_new((gfloat)Prop[WinNo].One.OvVShift[i],-MaxVShift,MaxVShift,0.02*MaxVShift,0.1*MaxVShift,0.0);
    ForOverlap->VShft[i]=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(ForOverlap->VShft[i]),FALSE);
    gtk_widget_set_size_request(GTK_WIDGET(ForOverlap->VShft[i]),44,-1);
    gtk_table_attach(GTK_TABLE(Table),ForOverlap->VShft[i],2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(OverlapVShift),GINT_TO_POINTER(i)); 

    CheckBut=gtk_check_button_new();
    gtk_table_attach(GTK_TABLE(Table),CheckBut,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0); 
    if (Prop[WinNo].One.Overlap[i]) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckBut),TRUE);
    gtk_signal_connect(GTK_OBJECT(CheckBut),"toggled",GTK_SIGNAL_FUNC(ToggleOverlap),GINT_TO_POINTER(i)); 
    }

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Dismiss"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),OvWin);

gtk_widget_show_all(OvWin);
gtk_style_unref(Style);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Zero1dSpec(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
if (ScreenSpecType[WinNo][Screen] !=1 ) return;                           //Only 1d spectra can be zeroed not projections
ZeroOned(SNo); Update1d(WinNo);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Read1dS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH],Str[80];
gint SNo;
FILE *Fp;
size_t ChansRead;

SNo=ScreenSpecNo[U1->N][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SpecDir,FileX->Path); SavePrefs();                                                                  //Preserve path
g_free(FileX);
ZeroOned(SNo);
if ((Fp=fopen(FName,"r"))==NULL) { Attention(0,"Error accessing file!"); return; }
if (Setup.Oned.WSz == 1) ChansRead=fread(&Oned16[Off1d[SNo]],2,Setup.Oned.Chan[SNo],Fp);
else                     ChansRead=fread(&Oned32[Off1d[SNo]],4,Setup.Oned.Chan[SNo],Fp);
if (ChansRead<Setup.Oned.Chan[SNo])
   { sprintf(Str,"1d Spec read only %d channels\nout of %d!",ChansRead,Setup.Oned.Chan[SNo]); Attention(-200,Str); }
fclose(Fp);
Update1d(U1->N);
if (GTK_IS_WIDGET(U1->W)) gtk_widget_destroy(U1->W);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void ReadRadwareSpecS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH],Str[80];
gint SNo,L;
gint ChansRead;

SNo=ScreenSpecNo[U1->N][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile); L=strlen(FName);
strcpy(SpecDir,FileX->Path); SavePrefs();                                                                  //Preserve path
g_free(FileX);
ZeroOned(SNo);
if (Setup.Oned.WSz == 1) radr16_(FName,&L,&Oned16[Off1d[SNo]],&Setup.Oned.Chan[SNo],&ChansRead);
else                     radr32_(FName,&L,&Oned32[Off1d[SNo]],&Setup.Oned.Chan[SNo],&ChansRead);
if (ChansRead<Setup.Oned.Chan[SNo])
   { sprintf(Str,"1d Spec read only %d channels\nout of %d!",ChansRead,Setup.Oned.Chan[SNo]); Attention(-200,Str); }
Update1d(U1->N);
if (GTK_IS_WIDGET(U1->W)) gtk_widget_destroy(U1->W);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Read1dFile(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 1d File",U1->W,394,TOP_SPACE+TopOfset,TRUE,SpecDir,".z1d",FALSE,&Read1dS,FALSE);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void ReadRadwareSpec(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select Radware Spectrum",U1->W,394,TOP_SPACE+TopOfset,TRUE,SpecDir,".spe",FALSE,&ReadRadwareSpecS,FALSE);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void Write1dS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];
FILE *Out;
gint SNo,SType;

SNo=ScreenSpecNo[U1->N][Screen]; SType=ScreenSpecType[U1->N][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SpecDir,FileX->Path); SavePrefs();                                                                    //Preserve path
g_free(FileX);
if (!(Out=fopen(FName,"w"))) { Attention(0,"Failed to open Output File"); return; }
switch (SType)
   {
   case 1: if (Setup.Oned.WSz == 1) fwrite(&Oned16[Off1d[SNo]],2,Setup.Oned.Chan[SNo],Out);                    //1d Spectrum
           else                     fwrite(&Oned32[Off1d[SNo]],4,Setup.Oned.Chan[SNo],Out);
           break;
   case 3: fwrite(&Proj[OffProj[SNo]],4,Setup.Twod.XChan[SNo],Out); break;                                 //2d X projection
   case 4: fwrite(&Proj[OffProj[SNo]],4,Setup.Twod.XChan[SNo],Out);                                        //2d Y projection
   }
fclose(Out);
if (GTK_IS_WIDGET(U1->W)) gtk_widget_destroy(U1->W);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void WriteRadwareSpecS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH],NameSp[8];
gint SNo,SType,L,M;

SNo=ScreenSpecNo[U1->N][Screen]; SType=ScreenSpecType[U1->N][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile); L=strlen(FName);
M=MIN(8,strlen(FileX->TargetFile)); strncpy(NameSp,FileX->TargetFile,M);  //8 character name to be embedded in the .spe file
strcpy(SpecDir,FileX->Path); SavePrefs();                                                                    //Preserve path
g_free(FileX);
switch (SType)
   {
   case 1: if (Setup.Oned.WSz == 1) radw16_(FName,&L,&Oned16[Off1d[SNo]],&Setup.Oned.Chan[SNo],NameSp,&M);     //1d Spectrum
           else                     radw32_(FName,&L,&Oned32[Off1d[SNo]],&Setup.Oned.Chan[SNo],NameSp,&M);
           break;
   case 3: radw32_(FName,&L,&Proj[OffProj[SNo]],&Setup.Twod.XChan[SNo],NameSp,&M); break;                  //2d X projection
   case 4: radw32_(FName,&L,&Proj[OffProj[SNo]],&Setup.Twod.XChan[SNo],NameSp,&M);                         //2d Y projection
   }
if (GTK_IS_WIDGET(U1->W)) gtk_widget_destroy(U1->W);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Write1dFile(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Write 1d File",U1->W,394,TOP_SPACE+TopOfset,FALSE,SpecDir,".z1d",FALSE,&Write1dS,FALSE);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void WriteRadwareSpec(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Write Radware Spectrum",U1->W,394,TOP_SPACE+TopOfset,FALSE,SpecDir,".spe",FALSE,&WriteRadwareSpecS,FALSE);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void ReadWrite1dDestroyed(GtkWidget *W,gpointer Unused)
{ g_free(U1); }                                                              //Free the memory allocated in ReadWrite1dFile
/*-----------------------------------------------------------------------------------------------------------------------*/
void ReadWrite1dFile(GtkWidget *W,gpointer Data)
{
GtkWidget *But,*VBox,*HBox,*Label,*Frame,*VBox1;
static GdkColor FrameBg  = {0,0xFFFF,0x0000,0x0000};
static GdkColor FrameFg  = {0,0xFFFF,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;

FrameStyle=gtk_style_copy(gtk_widget_get_default_style());                              //Copy default style to this style
for (i=0;i<5;++i) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }              //Set colours

U1=g_new(struct WidgetInt,1);                                                   //Reserve memory for the variables required

U1->N=GPOINTER_TO_INT(Data);
U1->W=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(U1->W),"Read/Write 1d");
gtk_container_set_border_width(GTK_CONTAINER(U1->W),10);
gtk_widget_set_uposition(U1->W,100,TOP_SPACE+TopOfset);
gtk_window_set_transient_for(GTK_WINDOW(U1->W),GTK_WINDOW(SpecWin[U1->N]));                             //Window visibility
gtk_signal_connect(GTK_OBJECT(U1->W),"destroy",GTK_SIGNAL_FUNC(ReadWrite1dDestroyed),U1->W);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(U1->W),VBox);
Label=gtk_label_new("Read File will overwrite the displayed spectrum"); 
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0); 
Label=gtk_label_new("Write File will save the spectrum"); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Output Word Size corresponds to that in Setup"); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Projections are 32-bit"); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_with_label("Read\nFile"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Read1dFile),NULL);
But=gtk_button_new_with_label("Write\nFile"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Write1dFile),NULL);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(U1->W));

Frame=gtk_frame_new("RADWARE SPECTRUM (.spe)"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox1=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Frame),VBox1);
gtk_container_set_border_width(GTK_CONTAINER(VBox1),5);
Label=gtk_label_new("Radware stores fractional counts in REAL format");
gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
Label=gtk_label_new("So data conversion will take place");
gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,2);
But=gtk_button_new_with_label("Read"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReadRadwareSpec),NULL);
But=gtk_button_new_with_label("Write"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WriteRadwareSpec),NULL);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(U1->W));

gtk_widget_show_all(U1->W);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void HideSearchedPeaks(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
Prop[WinNo].One.ShowPeaks=FALSE;
Update1d(WinNo);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void ProceedPeakSearch(GtkWidget *W,gpointer Data)
{
const gchar *Str;
gdouble PSq,JSq,Thresh,C[100],CSum,Dd,Sd,Cj,Counts,Ss,SsPrev;
gint WinNo,SNo,j,JMax,i,N,Del;
gboolean P;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];

Str=gtk_entry_get_text(GTK_ENTRY(PeakSearchUtil->W1)); 
Setup.Oned.Fwhm[SNo]=MAX(1.2,atof(Str)); Del=(gint)(Setup.Oned.Fwhm[SNo]+0.5); PSq=Setup.Oned.Fwhm[SNo]/2.335; PSq=PSq*PSq;
Str=gtk_entry_get_text(GTK_ENTRY(PeakSearchUtil->W2)); Thresh=atof(Str);
/*First calculate C[j]*/
JMax=99; C[0]=100.0; for (j=1;j<100;j++) C[j]=0.0;
for (j=1;j<100;j++)
    {
    JSq=j*j;
    C[j]=100.0*(PSq-JSq)*exp(-0.5*JSq/PSq)/PSq;
    if ( (-C[j]>1.0e-06) && (-C[j]<1.0) ) { JMax=j; break; }
    }
if (JMax>97) { Attention(0,"Peak Search: FWHM too large to proceed"); return; }
CSum=C[0]; for (j=1;j<JMax;j++) CSum+=2.0*C[j]; C[1]-=0.5*CSum;
/* for (j=0;j<JMax;j++) g_print("j=%d C[j]=%f\n",j,C[j]); Verified values from NIM paper for p=0.5, 1.0, 2.0, 3.0 and 4.0 */
/* Now calculate the significance of the second difference at each channel number and see if it exceeds Thresh */
Setup.Oned.NPeaks[SNo]=0; for (i=0;i<MAX_PEAKS;i++) Setup.Oned.Peaks[i][SNo]=0;                //Get rid of previous peaks
N=0; SsPrev=0.0;
for (i=JMax+1;i<Setup.Oned.Chan[SNo]-JMax-1;i++)
    {
    for (j=-JMax,Dd=0.0,Sd=0.0;j<=JMax;j++) { Cj=C[ABS(j)]; Counts=Get1dCounts(WinNo,i+j,FALSE,0); Dd+=Cj*Counts; Sd+=Cj*Cj*Counts; }
    if (Sd>0.0) Ss=Dd/sqrt(Sd); else Ss=0.0;
    if (Ss > Thresh) 
       {
       P=FALSE; if (N) { if (i-Setup.Oned.Peaks[N-1][SNo]<Del) P=TRUE; }                            //Already a prior peak nearby ? 
       if (P) { if (Ss>SsPrev) { Setup.Oned.Peaks[N-1][SNo]=i; SsPrev=Ss; } }                    //Yes, already a prior peak nearby
       else { Setup.Oned.Peaks[N][SNo]=i; N++; SsPrev=Ss; }                                                  //No prior peak nearby
       }
    if (N==MAX_PEAKS-2) break;                                                                                     //Limit reached!
    }
Setup.Oned.NPeaks[SNo]=N; Prop[WinNo].One.ShowPeaks=TRUE;
Update1d(WinNo);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void PeakSearchDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (GTK_IS_WIDGET(Win)) gtk_widget_destroy(Win);
PkSearchOpen=FALSE;
g_free(PeakSearchUtil);                    
}
/*------------------------------------------------------------------------------------------------------------------------*/
void PeakSearch(GtkWidget *W,gpointer Data)
/* An improved version of the Mariscotti algorithm to be found in:                                *
 * Photopeak Method for the Computer Analysis of Gamma Ray Spectra from Semiconductor Detectors,  *
 * J.T.Routi and S.G.Prussin, Nucl. Instrum. Meth. 72 (1969) 125.                                 */
{
GtkWidget *Win,*HBox,*Label,*VBox,*But;
gint WinNo,SNo;
gchar Str[20];

if (CalibWinOpen||AreaWinOpen||OvWinOpen||FitWinOpen||PkSearchOpen) 
   { Attention(0,"Another context window open\nPlease close it first"); return; }
WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen]; 
if (Setup.Oned.Par[SNo]==0) { Attention(0,"Peak Search not permitted for Hit Pattern Spectrum"); return; }

PeakSearchUtil=g_new(struct PeakSearchUtil,1);
PkSearchOpen=TRUE;

Fit.WinNo=WinNo;                         //The same variable Fit.WinNo is re-used (Only one of Fit or Search can be active)
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_title(GTK_WINDOW(Win),"Peak Search");
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[WinNo]));                               //Window visibility
gtk_widget_set_usize(Win,280,80); gtk_widget_set_uposition(Win,320,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(PeakSearchDestroyed),Win);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),16);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("FWHM"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,4);
PeakSearchUtil->W1=gtk_entry_new_with_max_length(6); gtk_widget_set_usize(PeakSearchUtil->W1,80,22);
sprintf(Str,"%.1f",6.0); gtk_entry_set_text(GTK_ENTRY(PeakSearchUtil->W1),Str);
gtk_box_pack_start(GTK_BOX(HBox),PeakSearchUtil->W1,FALSE,FALSE,0);
Label=gtk_label_new("Level"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,8);
PeakSearchUtil->W2=gtk_entry_new_with_max_length(6); gtk_widget_set_usize(PeakSearchUtil->W2,80,22);
sprintf(Str,"%.1f",2.5); gtk_entry_set_text(GTK_ENTRY(PeakSearchUtil->W2),Str);
gtk_box_pack_start(GTK_BOX(HBox),PeakSearchUtil->W2,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_with_label("Hide Peaks"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(HideSearchedPeaks),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Locate Peaks"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedPeakSearch),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Dismiss"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Plot1(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH)
{
GtkWidget *VBox,*HBox,*Menu,*MenuItem,*HScrlBar,*VSclUp,*VSclDn,*VSclLog,*VSclAuto,*VSclMan,
          *VSclBox,*ButtonBox,*BBox,*Label,*SubMenu1,*SubMenuItem;
gint i,SNo,SType;
gchar Str[120];
gfloat MaxChan;
gint WinW,WinH,WinX,WinY;

PixMap[WinNo]=NULL; SNo=ScreenSpecNo[WinNo][Screen]; SType=ScreenSpecType[WinNo][Screen]; 
if (SType==1) { Prop[WinNo].One.XLo=0; Prop[WinNo].One.XHi=Setup.Oned.Chan[SNo]; } 
Prop[WinNo].One.CountsLo=0.0; Prop[WinNo].One.CountsHi=4.0; Prop[WinNo].One.CountsAuto=1; 
Prop[WinNo].One.CountsLog=FALSE; Prop[WinNo].One.BoxDrawn=0;
Prop[WinNo].One.BoxX1Ch=Prop[WinNo].One.XLo; Prop[WinNo].One.BoxX2Ch=Prop[WinNo].One.XHi;
for (i=0;i<ZOOM_LEVELS;i++) { Prop[WinNo].One.ZoomXLo[i]=Prop[WinNo].One.XLo; Prop[WinNo].One.ZoomXHi[i]=Prop[WinNo].One.XHi; }

if (SType==1)                                                                                       //1d plot of 1d spectrum
   {
   if (Prop[WinNo].Open) gtk_widget_destroy(SpecWin[WinNo]);
   SpecWin[WinNo]=gtk_window_new(GTK_WINDOW_TOPLEVEL);
   sprintf(Str,"1d#%d %s",SNo+1,Setup.Oned.Name[SNo]); gtk_window_set_title(GTK_WINDOW(SpecWin[WinNo]),Str);
   gtk_widget_set_size_request(SpecWin[WinNo],WindowW,WindowH);
   gtk_widget_set_uposition(GTK_WIDGET(SpecWin[WinNo]),WindowX,WindowY);
   gtk_window_set_policy(GTK_WINDOW(SpecWin[WinNo]),TRUE,TRUE,FALSE);
   }
else                                               //1d plot of 2d projection. Note: Stype=3 for XProj and Stype=4 for YProj
   {
   WinW=(gint)SpecWin[WinNo]->allocation.width; WinH=(gint)SpecWin[WinNo]->allocation.height;
   gdk_window_get_root_origin(SpecWin[WinNo]->window,&WinX,&WinY);
   gtk_widget_destroy(SpecWin[WinNo]); SpecWin[WinNo]=gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_policy(GTK_WINDOW(SpecWin[WinNo]),TRUE,TRUE,FALSE);
   if ( (WindowX==0) && (WindowY==0) && (WindowW==0) && (WindowH==0) )                    //Situation just after projection
      { 
      gtk_widget_set_usize(GTK_WIDGET(SpecWin[WinNo]),WinW,WinH); 
      gtk_widget_set_uposition(GTK_WIDGET(SpecWin[WinNo]),WinX,WinY); 
      }
   else                                                                 //Situation after Tile or Minitile after projection 
      { 
      gtk_widget_set_usize(GTK_WIDGET(SpecWin[WinNo]),WindowW,WindowH); 
      gtk_widget_set_uposition(GTK_WIDGET(SpecWin[WinNo]),WindowX,WindowY); 
      }
   if (BananaShowing && (WinNo==BananaWinNo) )
      { if (SType==3) sprintf(Str,"Ban XProj. of 2d#%d",SNo+1); else sprintf(Str,"Ban YProj. of 2d#%d",SNo+1); }
   else
      { if (SType==3) sprintf(Str,"XProj. of 2d Spec#%d",SNo+1); else sprintf(Str,"YProj. of 2d Spec#%d",SNo+1); }
   gtk_window_set_title(GTK_WINDOW(SpecWin[WinNo]),Str);
   }

Prop[WinNo].Open=1;
for (i=0;i<MAX_OVERLAP;++i)                                                       //Initialise variables related to overlap 
    { 
    Prop[WinNo].One.OvSpec[i]=MIN(i,Setup.Oned.N-1); 
    Prop[WinNo].One.OvHShift[i]=0; Prop[WinNo].One.OvVShift[i]=0; Prop[WinNo].One.Overlap[i]=FALSE; 
    }
Prop[WinNo].One.OverlapOff=TRUE;

gtk_signal_connect(GTK_OBJECT(SpecWin[WinNo]),"delete_event",GTK_SIGNAL_FUNC(CloseSpecWin),GINT_TO_POINTER(WinNo));
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(SpecWin[WinNo]),VBox);
HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),HBox);
VSclBox=gtk_vbox_new(FALSE,0);
gtk_box_pack_start(GTK_BOX(HBox),VSclBox,FALSE,FALSE,0);
ButtonBox=gtk_vbox_new(FALSE,0); 
gtk_box_pack_start(GTK_BOX(VSclBox),ButtonBox,FALSE,FALSE,0);
VSclUp=gtk_button_new();
gtk_signal_connect(GTK_OBJECT(VSclUp),"clicked",GTK_SIGNAL_FUNC(IncVScale),GINT_TO_POINTER(WinNo));
BBox=gtk_hbox_new(FALSE,0);
Label=gtk_label_new("+"); gtk_box_pack_start(GTK_BOX(BBox),Label,FALSE,FALSE,0);
gtk_container_add(GTK_CONTAINER(VSclUp),BBox); gtk_box_pack_start(GTK_BOX(ButtonBox),VSclUp,TRUE,TRUE,2);
VSclDn=gtk_button_new();
gtk_signal_connect(GTK_OBJECT(VSclDn),"clicked",GTK_SIGNAL_FUNC(DecVScale),GINT_TO_POINTER(WinNo));
BBox=gtk_hbox_new(FALSE,0);
Label=gtk_label_new("-"); gtk_box_pack_start(GTK_BOX(BBox),Label,FALSE,FALSE,0);
gtk_container_add(GTK_CONTAINER(VSclDn),BBox); gtk_box_pack_start(GTK_BOX(ButtonBox),VSclDn,TRUE,TRUE,2);
VSclAuto=gtk_button_new();
gtk_signal_connect(GTK_OBJECT(VSclAuto),"clicked",GTK_SIGNAL_FUNC(AutoVScale),GINT_TO_POINTER(WinNo));
BBox=gtk_hbox_new(FALSE,0);
Label=gtk_label_new("A"); gtk_box_pack_start(GTK_BOX(BBox),Label,FALSE,FALSE,0);
gtk_container_add(GTK_CONTAINER(VSclAuto),BBox); gtk_box_pack_start(GTK_BOX(ButtonBox),VSclAuto,TRUE,TRUE,2);
VSclLog=gtk_button_new();
gtk_signal_connect(GTK_OBJECT(VSclLog),"clicked",GTK_SIGNAL_FUNC(LogVScale),GINT_TO_POINTER(WinNo));
BBox=gtk_hbox_new(FALSE,0);
Label=gtk_label_new("L"); gtk_box_pack_start(GTK_BOX(BBox),Label,FALSE,FALSE,0);
gtk_container_add(GTK_CONTAINER(VSclLog),BBox); gtk_box_pack_start(GTK_BOX(ButtonBox),VSclLog,TRUE,TRUE,2);
VSclMan=gtk_button_new();
gtk_signal_connect(GTK_OBJECT(VSclMan),"clicked",GTK_SIGNAL_FUNC(ManScale),GINT_TO_POINTER(WinNo));
BBox=gtk_hbox_new(FALSE,0);
Label=gtk_label_new("M"); gtk_box_pack_start(GTK_BOX(BBox),Label,FALSE,FALSE,0);
gtk_container_add(GTK_CONTAINER(VSclMan),BBox); gtk_box_pack_start(GTK_BOX(ButtonBox),VSclMan,TRUE,TRUE,2);

Canvas[WinNo]=gtk_drawing_area_new();
gtk_box_pack_start(GTK_BOX(HBox),Canvas[WinNo],TRUE,TRUE,0);
g_signal_connect(G_OBJECT(Canvas[WinNo]),"expose_event",G_CALLBACK(CanvasExpose1),GINT_TO_POINTER(WinNo));
g_signal_connect(G_OBJECT(Canvas[WinNo]),"configure_event",G_CALLBACK(CanvasConfig1),GINT_TO_POINTER(WinNo));
gtk_signal_connect(GTK_OBJECT(Canvas[WinNo]),"event",GTK_SIGNAL_FUNC(CanvasEvent1),GINT_TO_POINTER(WinNo));

MaxChan=(gfloat)GetMaxChan(WinNo);
HScrlAdj[WinNo]=gtk_adjustment_new(0.0,0.0,MaxChan,0.01*MaxChan,0.05*MaxChan,
                                   (gfloat)(Prop[WinNo].One.XHi-Prop[WinNo].One.XLo));
gtk_signal_connect(GTK_OBJECT(HScrlAdj[WinNo]),"value_changed",GTK_SIGNAL_FUNC(Scroll),GINT_TO_POINTER(WinNo));
HScrlBar=gtk_hscrollbar_new(GTK_ADJUSTMENT(HScrlAdj[WinNo]));
gtk_box_pack_start(GTK_BOX(VBox),HScrlBar,FALSE,FALSE,0);

Menu=gtk_menu_new(); SubMenu1=gtk_menu_new();
MenuItem=gtk_menu_item_new_with_label("Zoom"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Zoom),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("UnZoom"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(UnZoom),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Full"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Full),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Refresh"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Refresh),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Area"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Area),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Calibrate"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Calibrate),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Overlap"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Overlap),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Quick Fit"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(QuickFit),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Peak Search"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(PeakSearch),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Peak Fit"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(FitWidget),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Zero Spec"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_menu_item_set_submenu(GTK_MENU_ITEM(MenuItem),SubMenu1);
SubMenuItem=gtk_menu_item_new_with_label("Sure?"); gtk_menu_append(GTK_MENU(SubMenu1),SubMenuItem); gtk_widget_show(SubMenuItem);
gtk_signal_connect(GTK_OBJECT(SubMenuItem),"activate",GTK_SIGNAL_FUNC(Zero1dSpec),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Read/Write"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(ReadWrite1dFile),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Back to 2d"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(BackTo2d),GINT_TO_POINTER(WinNo));
if (SType==1) gtk_widget_set_sensitive(MenuItem,FALSE);
MenuItem=gtk_menu_item_new_with_label("PS/EPS/PDF"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(PreparePS1),GINT_TO_POINTER(WinNo));
gtk_signal_connect_object(GTK_OBJECT(Canvas[WinNo]),"button_press_event",GTK_SIGNAL_FUNC(CanvasPopup),GTK_OBJECT(Menu));
gtk_widget_set_events(Canvas[WinNo],GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK);

gtk_widget_show_all(SpecWin[WinNo]);
}
/*------------------------------------------------------------------------------------------------------------------------*/
