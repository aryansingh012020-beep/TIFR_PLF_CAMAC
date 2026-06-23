#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lamps.h"

#define CANVAS_TITLE_HEIGHT        22
#define AXIS_TICK_HEIGHT           6
#define AXIS_LABELS_HEIGHT         8
#define SPACE_BELOW_XAXIS          32
#define AXIS_LABELS_WIDTH          55
#define AXIS_TICK_WIDTH            6
#define CANVAS_RIGHT_MARGIN_WIDTH  20
#define RES_FOR_THICK_BOX          500

#define FONT_HEIGHT 6
#define FONT_WIDTH 6

#define ORIGIN_X     (AXIS_LABELS_WIDTH + AXIS_TICK_WIDTH)
#define ORIGIN_Y(h)  (h -  SPACE_BELOW_XAXIS - AXIS_LABELS_HEIGHT - AXIS_TICK_HEIGHT)
#define LEN_X(w)     (w - AXIS_LABELS_WIDTH - AXIS_TICK_WIDTH - CANVAS_RIGHT_MARGIN_WIDTH)
#define LEN_Y(h)     (h - SPACE_BELOW_XAXIS - AXIS_LABELS_HEIGHT - AXIS_TICK_HEIGHT - CANVAS_TITLE_HEIGHT)

/*External Global variables*/
extern enum ProgramState ProgramState;
extern struct Setup Setup;
extern guint16 *Twod16;
extern guint32 *Twod32,*Proj;
extern gint Off2d[MAX_2D],OffProj[MAX_2D];
extern gint Screen;
extern gint FrozenWin;
extern gint ScreenSpecNo[SCREEN_TOT][MAX_SCREENS],ScreenSpecType[SCREEN_TOT][MAX_SCREENS]; 
extern GtkWidget *SpecWin[SCREEN_TOT],*Canvas[SCREEN_TOT];
extern GtkObject *HScrlAdj[SCREEN_TOT],*VScrlAdj[SCREEN_TOT];             //Global adjustmnets for horiz. and vert. Scroll
extern gfloat XSlope[SCREEN_TOT],CountsSlope[SCREEN_TOT];        //These slope arrays are used both by plot1.c and plot2.c
extern gint CanvasW[SCREEN_TOT],CanvasH[SCREEN_TOT];                                    //Used by both plot1.c and plot2.c
extern glong D2PenColours[32][3];                                                               //RGB Colours for 2d plots
extern GdkFont *Font;
extern gint ThemeChanged;
extern struct WinProperties Prop[SCREEN_TOT];
extern struct Calibration Calibration[MAX_TOTAL_PAR];
extern gint BananaWinOpen,BananaDrawMode,BananaShowing,BananaWinNo,BananaLineType,BananaMulti;
extern struct BGate BGate;
extern struct BGate MultiBGate[7];                                        //We allow 7 extra banana gates to be displayed
extern gchar MultiBGateNames[7][LONG_TEXT_FIELD];                                 //File names for the extra banana gates
extern gboolean MultiBGateFlags[7];                                                   //Flags to indicate bgate is active
extern GtkWidget **MultiBGateButtons;                                  //Buttons holding file names of multi banana gates
extern struct FileSelectType *FileX;
extern gchar BanDir[MAX_DIR_STRLEN],SpecDir[MAX_DIR_STRLEN];                                           //Directory prefs
extern gboolean CommonZoom,RealScroll,RealScroll2;
extern GdkGC *Gc;                                                                 //Graphics context for 1d and 2d plots
extern GdkColor Colour2D[10];                               //0=Background,1=Axis,2=Title,3=Box,4=Permanent Box,5=Banana
extern GdkColor ColourMB[7];                                                        // Colours for multiple banana gates

/*Global variables for this file only*/
struct BoxType Box[SCREEN_TOT];
GdkPixmap *PixMap[SCREEN_TOT];
gint ReDraw2d,GlobalTemp2,GlobalTemp3;
gfloat YSlope[SCREEN_TOT];
static GdkGC *Pen[32] = {NULL};
gint BGateX[MAX_BAN_POINTS],BGateY[MAX_BAN_POINTS];
gboolean BanMoved,BanScaled,BanRotated;                                                         //Adjust Banana Gate flags
GtkWidget **MulW;                                                                    //Used for Adjust Banana Gate Widgets 
GtkWidget **ColUse;                                                                               //Used by ColourSelector 
struct WidgetInt *U2;                                                                          //Needed by ReadWrite2dFile

/*Function Templates*/
void WinClosed(GtkWidget *W,GtkWidget *Win);
void HScroll2(GtkAdjustment *HAdj,gpointer Data);
void VScroll2(GtkAdjustment *HAdj,gpointer Data);
void Display2d(GtkWidget *W,gint WinNo);
void YAxis(GtkWidget *W,gint WinNo);
void CanvasTitle2(GtkWidget *W,gint WinNo);
GdkGC *GetPen(gint i);
void Plot2(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH);
void DrawBox2(GtkWidget *W,gint WinNo);
gint CloseSpecWin(GtkWidget *W,GdkEvent *Event,gpointer Data);
void Refresh(GtkWidget *W,gpointer Data);
void HideBox(GtkWidget *W,gpointer Data);
gint CanvasPopup(GtkWidget *W,GdkEvent *Event);
gdouble Energy(gint PNo,gdouble Ch);
void XAxis(GtkWidget *W,gint WinNo);
void Plot1(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH);
void Attention(gint XPos,gchar *Messg);
void ZeroTwod(gint SpecNo);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void Update2d(gint WinNo);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void Draw_Maximise_Button(GtkWidget *W,gint WinNo);
void Plain2d(GtkWidget *W,gpointer Data);
void PreparePS2(GtkWidget *W,gint WinNo);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void Write2d32(gchar *FName,gint i,gboolean InThread);
void Write2d16(gchar *FName,gint i,gboolean InThread);
void RadwareMatrixRead(gchar *FName,gint i); void RadwareMatrixWrite(gchar *FName,gint i);
void SavePrefs(void);
/*-------------------------------------------------------------------------------------------------------------------*/
void FindBounds(gint *U,gint N,gint *Lo,gint *Hi)                  //A general routine to find min and max of an array
{
gint i;

for (i=0,*Lo=*Hi=U[0];i<N;i++) { if (U[i]<*Lo) *Lo=U[i]; if (U[i]>*Hi) *Hi=U[i]; }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Update2d(gint WinNo)
{
gint SNo,MaxChan;

SNo=ScreenSpecNo[WinNo][Screen]; 
MaxChan=Setup.Twod.XChan[SNo];

gdk_gc_set_rgb_fg_color(Gc,&Colour2D[0]);
gdk_draw_rectangle(PixMap[WinNo],Gc,TRUE,0,0,CanvasW[WinNo],CanvasH[WinNo]);
gdk_gc_set_rgb_fg_color(Gc,&Colour2D[2]);
CanvasTitle2(Canvas[WinNo],WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour2D[1]);
XAxis(Canvas[WinNo],WinNo); YAxis(Canvas[WinNo],WinNo);
Draw_Maximise_Button(Canvas[WinNo],WinNo); 
Display2d(Canvas[WinNo],WinNo);
gdk_draw_pixmap(Canvas[WinNo]->window,Gc,PixMap[WinNo],0,0,0,0,CanvasW[WinNo],CanvasH[WinNo]);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void HScroll2(GtkAdjustment *HAdj,gpointer Data)
{
gint WinNo,SNo,D,MaxChan,i,TNo;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen]; 
MaxChan=Setup.Twod.XChan[SNo];
D=Prop[WinNo].Two.XHi-Prop[WinNo].Two.XLo;
Prop[WinNo].Two.XLo=HAdj->value;
Prop[WinNo].Two.XHi=MIN(Prop[WinNo].Two.XLo+D,MaxChan);

gdk_gc_set_rgb_fg_color(Gc,&Colour2D[0]);
gdk_draw_rectangle(PixMap[WinNo],Gc,TRUE,0,0,CanvasW[WinNo],CanvasH[WinNo]);
gdk_gc_set_rgb_fg_color(Gc,&Colour2D[2]);
CanvasTitle2(Canvas[WinNo],WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour2D[1]);
XAxis(Canvas[WinNo],WinNo); YAxis(Canvas[WinNo],WinNo);
Display2d(Canvas[WinNo],WinNo);
Prop[WinNo].Two.BoxDrawn=0;
gdk_draw_pixmap(Canvas[WinNo]->window,Gc,PixMap[WinNo],0,0,0,0,CanvasW[WinNo],CanvasH[WinNo]);
if (CommonZoom && RealScroll)
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       TNo=ScreenSpecNo[i][Screen];
       if ( (ScreenSpecType[i][Screen]==2) && (i != WinNo) &&                                             //2d spectra only
            (Setup.Twod.XChan[TNo]==Setup.Twod.XChan[SNo]) && (Setup.Twod.YChan[TNo]==Setup.Twod.YChan[SNo])  //same resl
           && (Prop[i].Open) )                                                                           //and open windows
          {
          Prop[i].Two.XLo=Prop[WinNo].Two.XLo; Prop[i].Two.XHi=Prop[WinNo].Two.XHi;
          Update2d(i);
          }
       }
   }
RealScroll=TRUE;
}
/*-------------------------------------------------------------------------------------------------------------------*/
void VScroll2(GtkAdjustment *VAdj,gpointer Data)
{
gint WinNo,SNo,D,MaxChan,i,TNo;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen]; 
MaxChan=Setup.Twod.YChan[SNo];
D=Prop[WinNo].Two.YHi-Prop[WinNo].Two.YLo;
Prop[WinNo].Two.YLo=MaxChan-2*D-VAdj->value;
Prop[WinNo].Two.YHi=MIN(Prop[WinNo].Two.YLo+D,MaxChan);

if (ReDraw2d)                     //After a Zoom there is both a hscroll and a vscroll and we have to avoid 2 updates
   {
   gdk_gc_set_rgb_fg_color(Gc,&Colour2D[0]);
   gdk_draw_rectangle(PixMap[WinNo],Gc,TRUE,0,0,CanvasW[WinNo],CanvasH[WinNo]);
   gdk_gc_set_rgb_fg_color(Gc,&Colour2D[2]);
   CanvasTitle2(Canvas[WinNo],WinNo);
   gdk_gc_set_rgb_fg_color(Gc,&Colour2D[1]);
   XAxis(Canvas[WinNo],WinNo); YAxis(Canvas[WinNo],WinNo);
   Display2d(Canvas[WinNo],WinNo);
   Prop[WinNo].Two.BoxDrawn=0;
   gdk_draw_pixmap(Canvas[WinNo]->window,Gc,PixMap[WinNo],0,0,0,0,CanvasW[WinNo],CanvasH[WinNo]);
   }
if (CommonZoom && RealScroll)
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       TNo=ScreenSpecNo[i][Screen];
       if ( (ScreenSpecType[i][Screen]==2) && (i != WinNo) &&                                             //2d spectra only
            (Setup.Twod.XChan[TNo]==Setup.Twod.XChan[SNo]) && (Setup.Twod.YChan[TNo]==Setup.Twod.YChan[SNo])  //same resl
           && (Prop[i].Open) )                                                                           //and open windows
          {
          Prop[i].Two.YLo=Prop[WinNo].Two.YLo; Prop[i].Two.YHi=Prop[WinNo].Two.YHi;
          Update2d(i);
          }
       }
   }
RealScroll=TRUE;
}
/*-------------------------------------------------------------------------------------------------------------------*/
gint Hue(guint32 Counts,gint WinNo)
{
gdouble Colour;

switch (Prop[WinNo].Two.TScale)
   {
   case Log:  Colour=(log((double)Counts)-log((double)Prop[WinNo].Two.CountsLo))*CountsSlope[WinNo];
              break;
   case Sqrt: Colour=(sqrt((double)Counts)-sqrt((double)Prop[WinNo].Two.CountsLo))*CountsSlope[WinNo];
              break;
   case Lin:  Colour=(Counts-Prop[WinNo].Two.CountsLo)*CountsSlope[WinNo];
   }
return (((gint)Colour) % 32);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void AutoScale2(gint WinNo)
{
gint SNo,ChX,ChY;
guint32 Index,Counts;

SNo=ScreenSpecNo[WinNo][Screen];
Prop[WinNo].Two.CountsHi=4.0;
for (ChX=Prop[WinNo].Two.XLo;ChX<Prop[WinNo].Two.XHi;ChX++)
    {
    for (ChY=Prop[WinNo].Two.YLo;ChY<Prop[WinNo].Two.YHi;ChY++)
        {
        Index=ChX+Setup.Twod.XChan[SNo]*ChY;
        Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
        if (Prop[WinNo].Two.CountsHi<Counts) Prop[WinNo].Two.CountsHi=Counts;
        }
    }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Display2d(GtkWidget *W,gint WinNo)
{
gint OriginX,OriginY,ChX,ChY,XPix,YPix,SNo,LenX,LenY,Res,XPix1,XPix2,YPix1,YPix2,X1,Y1,X2,Y2,i,j;
guint32 Index,Counts;

gdk_gc_set_line_attributes(Gc,0,GDK_LINE_SOLID,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
SNo=ScreenSpecNo[WinNo][Screen]; OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]);
if (Prop[WinNo].Two.CountsAuto) AutoScale2(WinNo); 
switch (Prop[WinNo].Two.TScale)
   {
   case Log:  CountsSlope[WinNo]=31.0/(log((gdouble)Prop[WinNo].Two.CountsHi)-log((gdouble)Prop[WinNo].Two.CountsLo));
              break;
   case Sqrt: CountsSlope[WinNo]=31.0/(sqrt((gdouble)Prop[WinNo].Two.CountsHi)-sqrt((gdouble)Prop[WinNo].Two.CountsLo));
              break;
   case Lin:  CountsSlope[WinNo]=31.0/(gfloat)(Prop[WinNo].Two.CountsHi-Prop[WinNo].Two.CountsLo);
   }
LenX=LEN_X(CanvasW[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
Res=0; if (XSlope[WinNo]>1.0) Res++; if (YSlope[WinNo]>1.0) Res+=2;
switch (Res)                                                            //4 different plot methods depending upon Res
   {
   case 0:                                               //Screen resolution is low for both x and y. Make a dot plot
      for (XPix=0;XPix<LenX;++XPix)
          { 
          ChX=(gint)XPix/XSlope[WinNo]+Prop[WinNo].Two.XLo;
          for (YPix=0;YPix<LenY;++YPix)
              {
              ChY=(gint)YPix/YSlope[WinNo]+Prop[WinNo].Two.YLo;
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=Prop[WinNo].Two.CountsLo) 
                 gdk_draw_point(PixMap[WinNo],Pen[Hue(Counts,WinNo)],OriginX+XPix,OriginY-YPix);
              }
          }
      break;
   case 1:                                     //Screen resolution is high along x but low along y. Draw horiz. lines
      for (YPix=0;YPix<LenY;++YPix)
          {
          ChY=(gint)YPix/YSlope[WinNo]+Prop[WinNo].Two.YLo;
          for (ChX=Prop[WinNo].Two.XLo;ChX<Prop[WinNo].Two.XHi;++ChX)
              { 
              XPix2=XSlope[WinNo]*(ChX-Prop[WinNo].Two.XLo+0.5); XPix1=MAX(XPix2-XSlope[WinNo],0);
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=Prop[WinNo].Two.CountsLo) 
                 gdk_draw_line(PixMap[WinNo],Pen[Hue(Counts,WinNo)],OriginX+XPix1,OriginY-YPix,OriginX+XPix2,OriginY-YPix);
              }
          }
      break;
   case 2:                                      //Screen resolution is low along x but high along y. Draw vert. lines
      for (XPix=0;XPix<LenX;++XPix)
          {
          ChX=(gint)XPix/XSlope[WinNo]+Prop[WinNo].Two.XLo;
          for (ChY=Prop[WinNo].Two.YLo;ChY<Prop[WinNo].Two.YHi;ChY++)
              { 
              YPix2=YSlope[WinNo]*(ChY-Prop[WinNo].Two.YLo+0.5); YPix1=MAX(YPix2-YSlope[WinNo],0);
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=Prop[WinNo].Two.CountsLo) 
                 gdk_draw_line(PixMap[WinNo],Pen[Hue(Counts,WinNo)],OriginX+XPix,OriginY-YPix1,OriginX+XPix,OriginY-YPix2);
              }
          }
      break;
   case 3:                                              //Screen resolution is high for both x and y. Draw rectangles
      for (ChX=Prop[WinNo].Two.XLo;ChX<Prop[WinNo].Two.XHi;ChX++)
          {
          XPix2=XSlope[WinNo]*(ChX-Prop[WinNo].Two.XLo+0.5); XPix1=MAX(XPix2-XSlope[WinNo],0);
          for (ChY=Prop[WinNo].Two.YLo;ChY<Prop[WinNo].Two.YHi;ChY++)
              { 
              YPix2=YSlope[WinNo]*(ChY-Prop[WinNo].Two.YLo+0.5); YPix1=MAX(YPix2-YSlope[WinNo],0);
              Index=ChX+Setup.Twod.XChan[SNo]*ChY;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              if (Counts>=Prop[WinNo].Two.CountsLo) 
                 gdk_draw_rectangle(PixMap[WinNo],Pen[Hue(Counts,WinNo)],TRUE,OriginX+XPix1,OriginY-YPix2,
                                               XPix2-XPix1,YPix2-YPix1);
              }
          }
   }
if (Prop[WinNo].Two.BoxDrawn)                                                                             //Show the box
   { //Recalculate pixel values since the user might have resized the window
   X1=OriginX+XSlope[WinNo]*(Prop[WinNo].Two.BoxX1Ch-Prop[WinNo].Two.XLo+0.5);
   X2=OriginX+XSlope[WinNo]*(Prop[WinNo].Two.BoxX2Ch-Prop[WinNo].Two.XLo+0.5);
   Y1=OriginY-YSlope[WinNo]*(Prop[WinNo].Two.BoxY1Ch-Prop[WinNo].Two.YLo+0.5);
   Y2=OriginY-YSlope[WinNo]*(Prop[WinNo].Two.BoxY2Ch-Prop[WinNo].Two.YLo+0.5);
   gdk_gc_set_rgb_fg_color(Gc,&Colour2D[4]);
   if ((CanvasW[WinNo]+CanvasH[WinNo])>RES_FOR_THICK_BOX)                                             //Set line width etc
      gdk_gc_set_line_attributes(Gc,3,GDK_LINE_SOLID,GDK_CAP_PROJECTING,GDK_JOIN_BEVEL); 
   gdk_draw_line(PixMap[WinNo],Gc,X1,Y1,X2,Y1);
   gdk_draw_line(PixMap[WinNo],Gc,X2,Y1,X2,Y2);
   gdk_draw_line(PixMap[WinNo],Gc,X1,Y2,X2,Y2);
   gdk_draw_line(PixMap[WinNo],Gc,X1,Y1,X1,Y2);
   if ((CanvasW[WinNo]+CanvasH[WinNo])>RES_FOR_THICK_BOX)                                              //Restore width etc
      gdk_gc_set_line_attributes(Gc,0,GDK_LINE_SOLID,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
   }
if (BananaShowing && (BananaWinNo==WinNo) && (BGate.N>2) )                                                  //Show BGate
   {
   gdk_gc_set_rgb_fg_color(Gc,&Colour2D[5]);
   switch (BananaLineType)
      {
      case 0: gdk_gc_set_line_attributes(Gc,3,GDK_LINE_ON_OFF_DASH,GDK_CAP_PROJECTING,GDK_JOIN_BEVEL);
              break;
      case 1: gdk_gc_set_line_attributes(Gc,3,GDK_LINE_SOLID,GDK_CAP_PROJECTING,GDK_JOIN_BEVEL);
              break;
      case 2: gdk_gc_set_line_attributes(Gc,0,GDK_LINE_ON_OFF_DASH,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
              break;
      case 3: gdk_gc_set_line_attributes(Gc,0,GDK_LINE_SOLID,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
              break;
      }
   for (i=1;i<BGate.N;++i) 
       {
       XPix1=OriginX+XSlope[WinNo]*((gfloat)BGate.X[i-1]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
       YPix1=OriginY-YSlope[WinNo]*((gfloat)BGate.Y[i-1]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
       XPix2=OriginX+XSlope[WinNo]*((gfloat)BGate.X[i]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
       YPix2=OriginY-YSlope[WinNo]*((gfloat)BGate.Y[i]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
       gdk_draw_line(PixMap[WinNo],Gc,XPix1,YPix1,XPix2,YPix2);
       }
   XPix1=OriginX+XSlope[WinNo]*((gfloat)BGate.X[0]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
   YPix1=OriginY-YSlope[WinNo]*((gfloat)BGate.Y[0]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
   gdk_draw_line(PixMap[WinNo],Gc,XPix2,YPix2,XPix1,YPix1);
   gdk_gc_set_line_attributes(Gc,0,GDK_LINE_SOLID,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);                  //Restore width etc
   }
if (BananaMulti && (BananaWinNo==WinNo))                                                             //Show Multi BGates
   {
   for (j=0;j<7;++j)                                                    //Loop over the 7 possible multiple banana gates
       {
       if (MultiBGateFlags[j])
          {
          gdk_gc_set_rgb_fg_color(Gc,&ColourMB[j]);
          for (i=1;i<MultiBGate[j].N;++i)
              {
              XPix1=OriginX+XSlope[WinNo]*((gfloat)MultiBGate[j].X[i-1]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
              YPix1=OriginY-YSlope[WinNo]*((gfloat)MultiBGate[j].Y[i-1]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
              XPix2=OriginX+XSlope[WinNo]*((gfloat)MultiBGate[j].X[i]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
              YPix2=OriginY-YSlope[WinNo]*((gfloat)MultiBGate[j].Y[i]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
              gdk_draw_line(PixMap[WinNo],Gc,XPix1,YPix1,XPix2,YPix2);
              }
          XPix1=OriginX+XSlope[WinNo]*((gfloat)MultiBGate[j].X[0]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
          YPix1=OriginY-YSlope[WinNo]*((gfloat)MultiBGate[j].Y[0]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
          gdk_draw_line(PixMap[WinNo],Gc,XPix2,YPix2,XPix1,YPix1);
          }
       }
   }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void YAxis(GtkWidget *W,gint WinNo)
{
gint OriginX,OriginY,LenX,LenY,i,YVal,DYVal,StrLen,LabelRightX,SNo,MaxChan,PNo,R;
gfloat Dy,y;
gdouble AxVal;
gchar Str[24];

SNo=ScreenSpecNo[WinNo][Screen]; MaxChan=Setup.Twod.YChan[SNo];
PNo=Setup.Twod.YPar[SNo]; 
if (PNo>0) R=Setup.Parameter.Chan[PNo-1]/MaxChan;                                         //Avoid PNo=0 (Hit parameter)
OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); LenX=LEN_X(CanvasW[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
LabelRightX=OriginX-AXIS_TICK_WIDTH-4; Dy=0.25*(gfloat)LenY;
gdk_draw_line(PixMap[WinNo],Gc,OriginX-1,OriginY,OriginX-1,OriginY-LenY);
DYVal=0.25*(gfloat)(Prop[WinNo].Two.YHi-Prop[WinNo].Two.YLo)+0.5;
YSlope[WinNo]=(gfloat)LenY/(gfloat)(Prop[WinNo].Two.YHi-Prop[WinNo].Two.YLo);
for (i=0,y=(gfloat)OriginY,YVal=Prop[WinNo].Two.YLo;i<5;i++,y-=Dy,YVal+=DYVal)
    {
    YVal=MIN(YVal,MaxChan);
    gdk_draw_line(PixMap[WinNo],Gc,OriginX-AXIS_TICK_WIDTH,(gint)(y+0.5),OriginX-2,(gint)(y+0.5));
    if (PNo>0)                                                                             //Avoid PNo=0 (Hit parameter)
       {
       AxVal=Energy(PNo,(gdouble)(YVal*R));
       if (AxVal>4 && AxVal<99999) sprintf(Str,"%.0f",AxVal); else sprintf(Str,"%.3g",AxVal);
       }
    else sprintf(Str,"%d",YVal); 
    StrLen=strlen(Str);
    gdk_draw_string(PixMap[WinNo],Font,Gc,LabelRightX-StrLen*FONT_WIDTH,(gint)(y+0.5)+FONT_HEIGHT/2,Str);
    }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void CreateTitle2(gchar *Title,gchar *XTitle,gchar *YTitle,gint WinNo)
{
gchar Str1[22];
gint SNo;

SNo=ScreenSpecNo[WinNo][Screen];
AbbreviateFileName(Str1,Setup.Files.Twod[SNo],18);
if (Setup.Twod.XPar[SNo]<=Setup.Parameter.NPar) sprintf(XTitle,Setup.Parameter.Name[Setup.Twod.XPar[SNo]-1]);
else  sprintf(XTitle,Setup.Pseudo.Name[Setup.Twod.XPar[SNo]-Setup.Parameter.NPar-1]);
if (Setup.Twod.YPar[SNo]<=Setup.Parameter.NPar) sprintf(YTitle,Setup.Parameter.Name[Setup.Twod.YPar[SNo]-1]);
else  sprintf(YTitle,Setup.Pseudo.Name[Setup.Twod.YPar[SNo]-Setup.Parameter.NPar-1]);
if (Setup.Twod.NYPar[SNo]>1) strcat(YTitle,"[*]");
if (Setup.Twod.NXPar[SNo]>1) strcat(XTitle,"[*]");
sprintf(Title,"X:%s Y:%s %s",XTitle,YTitle,Str1);
if (Setup.Twod.CDdet[SNo].Enabled) sprintf(Title,"CD Detector");
}
/*-------------------------------------------------------------------------------------------------------------------*/
void CanvasTitle2(GtkWidget *W,gint WinNo)
{
gchar Title[2*MAX_TEXT_FIELD+40],XTitle[MAX_TEXT_FIELD],YTitle[MAX_TEXT_FIELD];

CreateTitle2(Title,XTitle,YTitle,WinNo);
gdk_draw_string(PixMap[WinNo],Font,W->style->fg_gc[GTK_STATE_NORMAL],FONT_WIDTH,FONT_HEIGHT+4,Title);
}
/*-------------------------------------------------------------------------------------------------------------------*/
GdkGC *GetPen(gint i)
{
GdkGC *Gc;
GdkColor C;

if (!PixMap[0]) return NULL;                        //If PixMap[0] is not yet allocated gdk_gc_new(PixMap[0]) will fail
C.red=D2PenColours[i][0]; C.green=D2PenColours[i][1]; C.blue=D2PenColours[i][2];
gdk_color_alloc(gdk_colormap_get_system(),&C);
Gc=gdk_gc_new(PixMap[0]);
//gdk_gc_set_foreground(Gc,&C);
gdk_gc_set_rgb_fg_color(Gc,&C);
return Gc;
}
/*-------------------------------------------------------------------------------------------------------------------*/
gint CanvasConfig2(GtkWidget *W,GdkEventConfigure *Event,gpointer Data)
{
gint WinNo,i;

WinNo=GPOINTER_TO_INT(Data);
if (PixMap[WinNo]) gdk_pixmap_unref(PixMap[WinNo]);
CanvasW[WinNo]=W->allocation.width; CanvasH[WinNo]=W->allocation.height;
PixMap[WinNo]=gdk_pixmap_new(W->window,CanvasW[WinNo],CanvasH[WinNo],-1);
if (Gc==NULL) Gc=gdk_gc_new(PixMap[WinNo]);
gdk_gc_set_rgb_fg_color(Gc,&Colour2D[0]);
gdk_draw_rectangle(PixMap[WinNo],Gc,TRUE,0,0,W->allocation.width,W->allocation.height);
gdk_gc_set_rgb_fg_color(Gc,&Colour2D[2]);
CanvasTitle2(W,WinNo);
gdk_gc_set_rgb_fg_color(Gc,&Colour2D[1]);
XAxis(W,WinNo); YAxis(W,WinNo); Draw_Maximise_Button(W,WinNo);
if (ThemeChanged) { for (i=0;i<32;i++) Pen[i]=GetPen(i); ThemeChanged=FALSE; }
Display2d(W,WinNo);

return TRUE;
}
/*-------------------------------------------------------------------------------------------------------------------*/
gint CanvasExpose2(GtkWidget *W,GdkEventExpose *Event,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
gdk_draw_pixmap(W->window,W->style->fg_gc[GTK_WIDGET_STATE(W)],PixMap[WinNo],Event->area.x,Event->area.y,Event->area.x,
                Event->area.y,Event->area.width,Event->area.height);
/* Note that in draw_pixmap the colours specified in the second argument are ignored, colours
 * stored in the pixmap are applied so one could have even written: W->state->black_gc */
return FALSE;
}
/*-------------------------------------------------------------------------------------------------------------------*/
void DrawBox2(GtkWidget *W,gint WinNo)
{
GdkRectangle UpdateRect;
gint OriginY,LenY,SNo,XPNo,YPNo,RX,RY,Ch;
gchar Str[80];
guint32 Counts,Index;

SNo=ScreenSpecNo[WinNo][Screen];
XPNo=Setup.Twod.XPar[SNo]; YPNo=Setup.Twod.YPar[SNo];
RX=Setup.Parameter.Chan[XPNo-1]/Setup.Twod.XChan[SNo]; //Corrected on 10Nov06
RY=Setup.Parameter.Chan[YPNo-1]/Setup.Twod.YChan[SNo]; //Corrected on 10Nov06
OriginY=ORIGIN_Y(CanvasH[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);

/*First we need to erase Prev box from the pixmap used as a backing store*/
UpdateRect.x=MIN(Box[WinNo].X1,Box[WinNo].X2P); UpdateRect.y=Box[WinNo].Y2P; 
UpdateRect.width=abs(Box[WinNo].X1-Box[WinNo].X2P); UpdateRect.height=1;
gtk_widget_draw(W,&UpdateRect);
UpdateRect.x=MIN(Box[WinNo].X1,Box[WinNo].X2P); UpdateRect.y=Box[WinNo].Y1; 
UpdateRect.width=abs(Box[WinNo].X1-Box[WinNo].X2P); UpdateRect.height=1;
gtk_widget_draw(W,&UpdateRect);
UpdateRect.x=Box[WinNo].X1; UpdateRect.y=MIN(Box[WinNo].Y1,Box[WinNo].Y2P); 
UpdateRect.width=1; UpdateRect.height=abs(Box[WinNo].Y1-Box[WinNo].Y2P)+1;
gtk_widget_draw(W,&UpdateRect);
UpdateRect.x=Box[WinNo].X2P; UpdateRect.y=MIN(Box[WinNo].Y1,Box[WinNo].Y2P); 
UpdateRect.width=1; UpdateRect.height=abs(Box[WinNo].Y1-Box[WinNo].Y2P)+1;
gtk_widget_draw(W,&UpdateRect);

/*Also we need to erase both dialog lines from the backing pixmap */
UpdateRect.x=0; UpdateRect.y=CanvasH[WinNo]-2*(FONT_HEIGHT+7); UpdateRect.width=CanvasW[WinNo]; UpdateRect.height=2*(FONT_HEIGHT+7);
gtk_widget_draw(W,&UpdateRect);

/*Then we need to draw current box*/
gdk_draw_line(W->window,W->style->fg_gc[GTK_STATE_NORMAL],Box[WinNo].X1,Box[WinNo].Y2,
              Box[WinNo].X2,Box[WinNo].Y2);
gdk_draw_line(W->window,W->style->fg_gc[GTK_STATE_NORMAL],Box[WinNo].X1,Box[WinNo].Y1,
              Box[WinNo].X2,Box[WinNo].Y1);
gdk_draw_line(W->window,W->style->fg_gc[GTK_STATE_NORMAL],Box[WinNo].X1,Box[WinNo].Y1,
              Box[WinNo].X1,Box[WinNo].Y2);
gdk_draw_line(W->window,W->style->fg_gc[GTK_STATE_NORMAL],Box[WinNo].X2,Box[WinNo].Y1,
              Box[WinNo].X2,Box[WinNo].Y2);

Prop[WinNo].Two.BoxX2Ch=XSlope[WinNo]>0 ? (gint)(0.5+(gfloat)Prop[WinNo].Two.XLo+(gfloat)((Box[WinNo].X2-ORIGIN_X)/XSlope[WinNo])) 
                               : Setup.Twod.XChan[SNo];
Prop[WinNo].Two.BoxX2Ch=MIN(Prop[WinNo].Two.BoxX2Ch,Setup.Twod.XChan[SNo]-1);
Prop[WinNo].Two.BoxY2Ch=YSlope[WinNo]>0 ? (gint)(0.5+(gfloat)Prop[WinNo].Two.YLo+(gfloat)((OriginY-Box[WinNo].Y2)/YSlope[WinNo])) 
                               : Setup.Twod.YChan[SNo];
Prop[WinNo].Two.BoxY2Ch=MIN(Prop[WinNo].Two.BoxY2Ch,Setup.Twod.YChan[SNo]-1);
Index=Prop[WinNo].Two.BoxX2Ch+Setup.Twod.XChan[SNo]*Prop[WinNo].Two.BoxY2Ch;
Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
Ch=Prop[WinNo].Two.BoxX2Ch*RX;
sprintf(Str,"X=%d (%f %s)",Prop[WinNo].Two.BoxX2Ch,Energy(XPNo,(gdouble)Ch),Calibration[XPNo-1].Units);
gdk_draw_string(W->window,Font,W->style->fg_gc[GTK_STATE_NORMAL],0,CanvasH[WinNo]-FONT_HEIGHT-9,Str);              //Write dialog line
Ch=Prop[WinNo].Two.BoxY2Ch*RY;
sprintf(Str,"Y=%d (%f %s) Counts=%d",Prop[WinNo].Two.BoxY2Ch,Energy(YPNo,(gdouble)Ch),Calibration[YPNo-1].Units,Counts);
gdk_draw_string(W->window,Font,W->style->fg_gc[GTK_STATE_NORMAL],0,CanvasH[WinNo]-2,Str);                          //Write dialog line
Prop[WinNo].Two.BoxDrawn=1;
}
/*-------------------------------------------------------------------------------------------------------------------*/
gint CanvasEvent2(GtkWidget *W,GdkEventAny *Event,gpointer Data)
/* Here we handle Type GDK_BUTTON_PRESS=4, GDK_MOTION_NOTIFY=3, GDK_BUTTON_RELEASE=7 
 * for other Types we ignore the Event by returning FALSE */
{
GdkEventType Type;
GdkRectangle UpdateRect;
gint OriginX,OriginY,LenX,LenY,WinNo,BanX,BanY,SNo,XSize,YSize;
static gint PrevX,PrevY;

WinNo=GPOINTER_TO_INT(Data);
OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); LenX=LEN_X(CanvasW[WinNo]); LenY=LEN_Y(CanvasH[WinNo]);
SNo=ScreenSpecNo[WinNo][Screen];
Type=Event->type;
switch (Type)
   {
   case GDK_BUTTON_RELEASE:
        FrozenWin=-1;                                                                          //Free the window for update
        return TRUE;
        break;
   case GDK_BUTTON_PRESS:
        switch (((GdkEventButton *)(Event))->button)
           {
           case 1:                                                      //Left button pressed. Draw box or add Banana point
              if ( (((GdkEventMotion*)Event)->y < CANVAS_TITLE_HEIGHT) &&
                   (((GdkEventMotion*)Event)->x > CanvasW[WinNo]-CANVAS_RIGHT_MARGIN_WIDTH) ) 
                                                                                        //UR corner: maximise/normal window
                 {
                 gdk_window_get_size(SpecWin[WinNo]->window,&XSize,&YSize);
                 if ((XSize>=MONITOR_XRES-X_BORDER-16) && (YSize>=MONITOR_YRES-TOP_SPACE-BOT_SPACE-Y_BORDER-16))
                                                                                                              //Normal size
                    Plot2(WinNo,Prop[WinNo].X,Prop[WinNo].Y,Prop[WinNo].W,Prop[WinNo].H);
                 else Plot2(WinNo,0,TOP_SPACE,MONITOR_XRES-X_BORDER-1,MONITOR_YRES-TOP_SPACE-BOT_SPACE-Y_BORDER-1);
                 return TRUE;
                 }

              if (BananaDrawMode && (BananaWinNo==WinNo) )
                 {
                 BGate.N++;
                 BanX=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
                 BanY=CLAMP(((GdkEventMotion *)(Event))->y,OriginY-LenY,OriginY);
		 if (BGate.N==1) 
                    gdk_draw_arc(W->window,W->style->fg_gc[GTK_STATE_NORMAL],FALSE,BanX-5,BanY-5,10,10,0,360*64);
                 else 
                    gdk_draw_line(W->window,W->style->fg_gc[GTK_STATE_NORMAL],PrevX,PrevY,BanX,BanY);
                 PrevX=BanX; PrevY=BanY;
                 BGate.X[BGate.N-1]=((BanX-OriginX)/XSlope[WinNo]+Prop[WinNo].Two.XLo)*8192.0/Setup.Twod.XChan[SNo]+0.5;
                 BGate.Y[BGate.N-1]=((OriginY-BanY)/YSlope[WinNo]+Prop[WinNo].Two.YLo)*8192.0/Setup.Twod.YChan[SNo]+0.5;
                 return TRUE;
                 }
              if (Prop[WinNo].Two.BoxDrawn == 1)                                                         //Erase prev box
                 {
                 UpdateRect.x=MIN(Box[WinNo].X1,Box[WinNo].X2); UpdateRect.y=Box[WinNo].Y2; 
                 UpdateRect.width=abs(Box[WinNo].X1-Box[WinNo].X2);
                 UpdateRect.height=1; gtk_widget_draw(W,&UpdateRect);
                 UpdateRect.x=MIN(Box[WinNo].X1,Box[WinNo].X2); UpdateRect.y=Box[WinNo].Y1;
                 UpdateRect.width=abs(Box[WinNo].X1-Box[WinNo].X2);
                 UpdateRect.height=1; gtk_widget_draw(W,&UpdateRect);
                 UpdateRect.x=Box[WinNo].X1; UpdateRect.y=MIN(Box[WinNo].Y1,Box[WinNo].Y2); UpdateRect.width=1;
                 UpdateRect.height=abs(Box[WinNo].Y1-Box[WinNo].Y2)+1; gtk_widget_draw(W,&UpdateRect);
                 UpdateRect.x=Box[WinNo].X2; UpdateRect.y=MIN(Box[WinNo].Y1,Box[WinNo].Y2); UpdateRect.width=1;
                 UpdateRect.height=abs(Box[WinNo].Y1-Box[WinNo].Y2)+1; gtk_widget_draw(W,&UpdateRect);
                 UpdateRect.x=0; UpdateRect.y=CanvasH[WinNo]-2*(FONT_HEIGHT+7); 
                 UpdateRect.width=CanvasW[WinNo]; UpdateRect.height=2*(FONT_HEIGHT+7);
                 gtk_widget_draw(W,&UpdateRect);                                                 //Erase both dialog lines
                 }
           Box[WinNo].X1=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
           Box[WinNo].Y1=CLAMP(((GdkEventMotion *)(Event))->y,OriginY-LenY,OriginY);
           Box[WinNo].X2=Box[WinNo].X1; Box[WinNo].Y2=Box[WinNo].Y1; Prop[WinNo].Two.BoxDrawn=0;
           Prop[WinNo].Two.BoxX1Ch=XSlope[WinNo]>0 ? 
                            (gint)(0.5+(gfloat)Prop[WinNo].Two.XLo+(gfloat)((Box[WinNo].X1-OriginX)/XSlope[WinNo])) : 0;
           Prop[WinNo].Two.BoxY1Ch=YSlope[WinNo]>0 ? 
                            (gint)(0.5+(gfloat)Prop[WinNo].Two.YLo+(gfloat)((OriginY-Box[WinNo].Y1)/YSlope[WinNo])) : 0;
           return TRUE;
           case 2:  return TRUE;                                                    //Middle button press. Not used (yet)
           case 3:  return FALSE;                                            //Right button press. Thats for canvas popup
           }
        break;
   case GDK_MOTION_NOTIFY:
        FrozenWin=WinNo;                                        //Prevent update of this window if the mouse is in motion
        if (BananaDrawMode && (BananaWinNo==WinNo) ) return TRUE;                   //No dragging while in BananaDrawMode
        Box[WinNo].X2P=Box[WinNo].X2; Box[WinNo].Y2P=Box[WinNo].Y2;
        Box[WinNo].X2=CLAMP(((GdkEventMotion *)(Event))->x,OriginX,OriginX+LenX);
        Box[WinNo].Y2=CLAMP(((GdkEventMotion *)(Event))->y,OriginY-LenY,OriginY);
        if (PixMap[WinNo] != NULL) 
           { 
           gdk_gc_set_rgb_fg_color(Gc,&Colour2D[3]);
           DrawBox2(W,WinNo); 
           }
        return TRUE;
        break;
   default: return FALSE;
 }
return FALSE;
}
/*-------------------------------------------------------------------------------------------------------------------*/
gint TestBGate(gint X,gint Y,struct BGate BGate)
{
/* The algorithm is at http://muldoon.cipic.ucdavis.edu/~okreylos/TAship/Spring2000/PointInPolygon.html */
gint i,C,B2x,B2y,Sx;

if ( (X>BGate.XMax) || (X<BGate.XMin) || (Y>BGate.YMax) || (Y<BGate.YMin) ) return FALSE;
for (i=0,C=0;i<BGate.N;i++)                                                                //C is the intersection count
    {
    B2y=(i==BGate.N-1) ? BGate.Y[0] : BGate.Y[i+1];
    if (!( ((BGate.Y[i]<Y) && (B2y<Y)) || ((BGate.Y[i]>=Y) && (B2y>=Y)) ))
       {
       B2x=(i==BGate.N-1) ? BGate.X[0] : BGate.X[i+1];
       Sx=BGate.X[i]+(Y-BGate.Y[i])*(B2x-BGate.X[i])/(B2y-BGate.Y[i]);         //Intersection point:
                                                                               //Div by zero cannot occur because of if 
       if (Sx==X) return 1;  //Modification 18 April 2003. Points on edges are considered as inside without further ado
       if (Sx>=X) C++;                                                                   //Increment intersection count
       }
    }
return (C%2);                                              //Point is inside the polygon iff Intersection Count is odd
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Volume(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo,Temp,X,X1,X2,Y,Y1,Y2;
GdkRectangle UpdateRect;
gdouble Vol;                                                   //Here gdouble is better than guint32 to prevent overflows
guint32 Counts,Index;
gchar Str[80];
gfloat XF,YF;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
UpdateRect.x=0; UpdateRect.y=CanvasH[WinNo]-2*(FONT_HEIGHT+7);
UpdateRect.width=CanvasW[WinNo]; UpdateRect.height=2*(FONT_HEIGHT+7);
gtk_widget_draw(Canvas[WinNo],&UpdateRect);                                                     //Erase both dialog lines

if (BananaShowing && (WinNo==BananaWinNo))
   {
   XF=Setup.Twod.XChan[SNo]/8192.0; YF=Setup.Twod.YChan[SNo]/8192.0;
   X1=BGate.XMin*XF+0.5; X2=BGate.XMax*XF+0.5; Y1=BGate.YMin*YF+0.5; Y2=BGate.YMax*YF+0.5;
   }
else
   {
   if (Prop[WinNo].Two.BoxDrawn == 1)
      {
      if (Prop[WinNo].Two.BoxX2Ch<Prop[WinNo].Two.BoxX1Ch) 
         { Temp=Prop[WinNo].Two.BoxX1Ch; Prop[WinNo].Two.BoxX1Ch=Prop[WinNo].Two.BoxX2Ch; Prop[WinNo].Two.BoxX2Ch=Temp; }
      if (Prop[WinNo].Two.BoxY2Ch<Prop[WinNo].Two.BoxY1Ch) 
         { Temp=Prop[WinNo].Two.BoxY1Ch; Prop[WinNo].Two.BoxY1Ch=Prop[WinNo].Two.BoxY2Ch; Prop[WinNo].Two.BoxY2Ch=Temp; }
      X1=Prop[WinNo].Two.BoxX1Ch; X2=Prop[WinNo].Two.BoxX2Ch; Y1=Prop[WinNo].Two.BoxY1Ch; Y2=Prop[WinNo].Two.BoxY2Ch;
      }
   else { X1=Prop[WinNo].Two.XLo; X2=Prop[WinNo].Two.XHi; Y1=Prop[WinNo].Two.YLo; Y2=Prop[WinNo].Two.YHi; }
   }

X1=CLAMP(X1,0,Setup.Twod.XChan[SNo]-1); X2=CLAMP(X2,X1,Setup.Twod.XChan[SNo]-1);
Y1=CLAMP(Y1,0,Setup.Twod.YChan[SNo]-1); Y2=CLAMP(Y2,Y1,Setup.Twod.YChan[SNo]-1);
Vol=0.0;
for (Y=Y1;Y<=Y2;Y++)
    {
    Index=Setup.Twod.XChan[SNo]*Y;
    for (X=X1;X<=X2;X++)
        {
        if (BananaShowing && (WinNo==BananaWinNo))
           {
           if (TestBGate((gint)(X/XF+0.5),(gint)(Y/YF+0.5),BGate))
              {
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index+X] : Twod32[Off2d[SNo]+Index+X];
              Vol+=(gdouble)(Counts);
              }
           }
        else
           {
           Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index+X] : Twod32[Off2d[SNo]+Index+X];
           Vol+=(gdouble)(Counts);
           }
        }
    }
sprintf(Str,"X: %d, %d  Y: %d, %d",X1,X2,Y1,Y2);                                                     //Write dialog line
gdk_draw_string(Canvas[WinNo]->window,Font,Canvas[WinNo]->style->black_gc,0,CanvasH[WinNo]-FONT_HEIGHT-9,Str);
if (BananaShowing && (WinNo==BananaWinNo)) sprintf(Str,"Banana Vol=%.0f",Vol); else sprintf(Str,"Box Vol=%.0f",Vol);
gdk_draw_string(Canvas[WinNo]->window,Font,Canvas[WinNo]->style->black_gc,0,CanvasH[WinNo]-2,Str);   //Write dialog line
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Zoom2(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo,i,Temp,NoChange,TNo;
gfloat Delta;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
if (Prop[WinNo].Two.BoxX2Ch<Prop[WinNo].Two.BoxX1Ch) 
   { Temp=Prop[WinNo].Two.BoxX1Ch; Prop[WinNo].Two.BoxX1Ch=Prop[WinNo].Two.BoxX2Ch; Prop[WinNo].Two.BoxX2Ch=Temp; }
if (Prop[WinNo].Two.BoxY2Ch<Prop[WinNo].Two.BoxY1Ch) 
   { Temp=Prop[WinNo].Two.BoxY1Ch; Prop[WinNo].Two.BoxY1Ch=Prop[WinNo].Two.BoxY2Ch; Prop[WinNo].Two.BoxY2Ch=Temp; }
Prop[WinNo].Two.BoxX1Ch=CLAMP(Prop[WinNo].Two.BoxX1Ch,0,Setup.Twod.XChan[SNo]-4);
Prop[WinNo].Two.BoxX2Ch=CLAMP(Prop[WinNo].Two.BoxX2Ch,Prop[WinNo].Two.BoxX1Ch+4,Setup.Twod.XChan[SNo]);
Prop[WinNo].Two.BoxY1Ch=CLAMP(Prop[WinNo].Two.BoxY1Ch,0,Setup.Twod.YChan[SNo]-4);
Prop[WinNo].Two.BoxY2Ch=CLAMP(Prop[WinNo].Two.BoxY2Ch,Prop[WinNo].Two.BoxY1Ch+4,Setup.Twod.YChan[SNo]);
if (Prop[WinNo].Two.XLo==Prop[WinNo].Two.BoxX1Ch) NoChange=1; else NoChange=0;
Prop[WinNo].Two.XLo=Prop[WinNo].Two.BoxX1Ch; Prop[WinNo].Two.XHi=Prop[WinNo].Two.BoxX2Ch; 
Prop[WinNo].Two.YLo=Prop[WinNo].Two.BoxY1Ch; Prop[WinNo].Two.YHi=Prop[WinNo].Two.BoxY2Ch; 
for (i=ZOOM_LEVELS-1;i>0;i--) 
    {
    Prop[WinNo].Two.ZoomXLo[i]=Prop[WinNo].Two.ZoomXLo[i-1]; Prop[WinNo].Two.ZoomXHi[i]=Prop[WinNo].Two.ZoomXHi[i-1]; 
    Prop[WinNo].Two.ZoomYLo[i]=Prop[WinNo].Two.ZoomYLo[i-1]; Prop[WinNo].Two.ZoomYHi[i]=Prop[WinNo].Two.ZoomYHi[i-1]; 
    }
Prop[WinNo].Two.ZoomXLo[0]=Prop[WinNo].Two.XLo; Prop[WinNo].Two.ZoomXHi[0]=Prop[WinNo].Two.XHi;
Prop[WinNo].Two.ZoomYLo[0]=Prop[WinNo].Two.YLo; Prop[WinNo].Two.ZoomYHi[0]=Prop[WinNo].Two.YHi;
GTK_ADJUSTMENT(HScrlAdj[WinNo])->page_size=(gfloat)(Prop[WinNo].Two.XHi-Prop[WinNo].Two.XLo);
GTK_ADJUSTMENT(HScrlAdj[WinNo])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[WinNo])->upper=(gfloat)(Setup.Twod.XChan[SNo]);
Delta=(gfloat)(Prop[WinNo].Two.YHi-Prop[WinNo].Two.YLo);
GTK_ADJUSTMENT(VScrlAdj[WinNo])->page_size=Delta;
GTK_ADJUSTMENT(VScrlAdj[WinNo])->lower=-Delta; GTK_ADJUSTMENT(VScrlAdj[WinNo])->upper=(gfloat)(Setup.Twod.YChan[SNo])-Delta;
Prop[WinNo].Two.BoxDrawn=0;

if (CommonZoom)                                           //For CommonZoom copy the settings and update all display windows
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       TNo=ScreenSpecNo[i][Screen];
       if ( (ScreenSpecType[i][Screen]==2) && (i != WinNo) &&                                             //2d spectra only
            (Setup.Twod.XChan[TNo]==Setup.Twod.XChan[SNo]) && (Setup.Twod.YChan[TNo]==Setup.Twod.YChan[SNo])  //same resl
           && (Prop[i].Open) )                                                                           //and open windows
          {
          Prop[i].Two.XLo=Prop[WinNo].Two.XLo; Prop[i].Two.XHi=Prop[WinNo].Two.XHi;
          Prop[i].Two.YLo=Prop[WinNo].Two.YLo; Prop[i].Two.YHi=Prop[WinNo].Two.YHi;
          GTK_ADJUSTMENT(HScrlAdj[i])->page_size=(gfloat)(Prop[i].Two.XHi-Prop[i].Two.XLo);
          GTK_ADJUSTMENT(HScrlAdj[i])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[i])->upper=(gfloat)(Setup.Twod.XChan[TNo]);
          Delta=(gfloat)(Prop[i].Two.YHi-Prop[i].Two.YLo);
          GTK_ADJUSTMENT(VScrlAdj[i])->page_size=Delta;
          GTK_ADJUSTMENT(VScrlAdj[i])->lower=-Delta; GTK_ADJUSTMENT(VScrlAdj[i])->upper=(gfloat)(Setup.Twod.YChan[TNo])-Delta;
          Prop[i].Two.BoxDrawn=0; RealScroll2=FALSE;
          ReDraw2d=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(VScrlAdj[i]),Setup.Twod.YChan[TNo]-2.0*Delta-Prop[i].Two.YLo);
          ReDraw2d=TRUE;  gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[i]),(gfloat)Prop[i].Two.XLo+0.1);
          }
       }
   }
RealScroll2=FALSE; 

ReDraw2d=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(VScrlAdj[WinNo]),Setup.Twod.YChan[SNo]-2.0*Delta-Prop[WinNo].Two.YLo);
ReDraw2d=TRUE;  gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[WinNo]),(gfloat)Prop[WinNo].Two.XLo+0.1);
if (NoChange) gtk_signal_emit_by_name(GTK_OBJECT(HScrlAdj[WinNo]),"value_changed");       //Makes sure to update canvas
}
/*-------------------------------------------------------------------------------------------------------------------*/
void UnZoom2(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo,i,TNo;
gfloat Delta;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
for (i=0;i<ZOOM_LEVELS-1;i++) 
    { 
    Prop[WinNo].Two.ZoomXLo[i]=Prop[WinNo].Two.ZoomXLo[i+1]; Prop[WinNo].Two.ZoomXHi[i]=Prop[WinNo].Two.ZoomXHi[i+1]; 
    Prop[WinNo].Two.ZoomYLo[i]=Prop[WinNo].Two.ZoomYLo[i+1]; Prop[WinNo].Two.ZoomYHi[i]=Prop[WinNo].Two.ZoomYHi[i+1]; 
    }
Prop[WinNo].Two.ZoomXLo[ZOOM_LEVELS-1]=Prop[WinNo].Two.ZoomXLo[0]; 
Prop[WinNo].Two.ZoomXHi[ZOOM_LEVELS-1]=Prop[WinNo].Two.ZoomXHi[0];
Prop[WinNo].Two.ZoomYLo[ZOOM_LEVELS-1]=Prop[WinNo].Two.ZoomYLo[0]; 
Prop[WinNo].Two.ZoomYHi[ZOOM_LEVELS-1]=Prop[WinNo].Two.ZoomYHi[0];
Prop[WinNo].Two.XLo=Prop[WinNo].Two.ZoomXLo[0]; Prop[WinNo].Two.XHi=Prop[WinNo].Two.ZoomXHi[0];
Prop[WinNo].Two.YLo=Prop[WinNo].Two.ZoomYLo[0]; Prop[WinNo].Two.YHi=Prop[WinNo].Two.ZoomYHi[0];
if (Prop[WinNo].Two.CountsAuto) AutoScale2(WinNo);
GTK_ADJUSTMENT(HScrlAdj[WinNo])->page_size=(gfloat)(Prop[WinNo].Two.XHi-Prop[WinNo].Two.XLo);
GTK_ADJUSTMENT(HScrlAdj[WinNo])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[WinNo])->upper=(gfloat)(Setup.Oned.Chan[SNo]);
Delta=(gfloat)(Prop[WinNo].Two.YHi-Prop[WinNo].Two.YLo);
GTK_ADJUSTMENT(VScrlAdj[WinNo])->page_size=Delta;
GTK_ADJUSTMENT(VScrlAdj[WinNo])->lower=-Delta; GTK_ADJUSTMENT(VScrlAdj[WinNo])->upper=(gfloat)(Setup.Twod.YChan[SNo])-Delta;
Prop[WinNo].Two.BoxDrawn=0;

if (CommonZoom)                                           //For CommonZoom copy the settings and update all display windows
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       TNo=ScreenSpecNo[i][Screen];
       if ( (ScreenSpecType[i][Screen]==2) && (i != WinNo) &&                                             //2d spectra only
            (Setup.Twod.XChan[TNo]==Setup.Twod.XChan[SNo]) && (Setup.Twod.YChan[TNo]==Setup.Twod.YChan[SNo])  //same resl
           && (Prop[i].Open) )                                                                           //and open windows
          {
          Prop[i].Two.XLo=Prop[WinNo].Two.XLo; Prop[i].Two.XHi=Prop[WinNo].Two.XHi;
          Prop[i].Two.YLo=Prop[WinNo].Two.YLo; Prop[i].Two.YHi=Prop[WinNo].Two.YHi;
          GTK_ADJUSTMENT(HScrlAdj[i])->page_size=(gfloat)(Prop[i].Two.XHi-Prop[i].Two.XLo);
          GTK_ADJUSTMENT(HScrlAdj[i])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[i])->upper=(gfloat)(Setup.Twod.XChan[TNo]);
          Delta=(gfloat)(Prop[i].Two.YHi-Prop[i].Two.YLo);
          GTK_ADJUSTMENT(VScrlAdj[i])->page_size=Delta;
          GTK_ADJUSTMENT(VScrlAdj[i])->lower=-Delta; GTK_ADJUSTMENT(VScrlAdj[i])->upper=(gfloat)(Setup.Twod.YChan[TNo])-Delta;
          Prop[i].Two.BoxDrawn=0; RealScroll2=FALSE;
          ReDraw2d=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(VScrlAdj[i]),Setup.Twod.YChan[TNo]-2.0*Delta-Prop[i].Two.YLo);
          ReDraw2d=TRUE;  gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[i]),(gfloat)Prop[i].Two.XLo+0.1);
          }
       }
   }
RealScroll2=FALSE;

ReDraw2d=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(VScrlAdj[WinNo]),Setup.Twod.YChan[SNo]-2.0*Delta-Prop[WinNo].Two.YLo);
ReDraw2d=TRUE;  gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[WinNo]),(gfloat)Prop[WinNo].Two.XLo+0.1);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Full2(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo,TNo,i;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
Prop[WinNo].Two.XLo=0; Prop[WinNo].Two.XHi=Setup.Twod.XChan[SNo]; 
Prop[WinNo].Two.YLo=0; Prop[WinNo].Two.YHi=Setup.Twod.YChan[SNo]; 
if (Prop[WinNo].Two.CountsAuto) AutoScale2(WinNo);
GTK_ADJUSTMENT(HScrlAdj[WinNo])->page_size=Setup.Twod.XChan[SNo];
GTK_ADJUSTMENT(HScrlAdj[WinNo])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[WinNo])->upper=(gfloat)(Setup.Twod.XChan[SNo]);
GTK_ADJUSTMENT(VScrlAdj[WinNo])->page_size=Setup.Twod.YChan[SNo];
GTK_ADJUSTMENT(VScrlAdj[WinNo])->lower=-Setup.Twod.YChan[SNo]; 
GTK_ADJUSTMENT(VScrlAdj[WinNo])->upper=0.0;
Prop[WinNo].Two.BoxDrawn=0;

if (CommonZoom)                                           //For CommonZoom copy the settings and update all display windows
   {
   for (i=0;i<SCREEN_TOT;++i)
       {
       TNo=ScreenSpecNo[i][Screen];
       if ( (ScreenSpecType[i][Screen]==2) && (i != WinNo) &&                                             //2d spectra only
            (Setup.Twod.XChan[TNo]==Setup.Twod.XChan[SNo]) && (Setup.Twod.YChan[TNo]==Setup.Twod.YChan[SNo])  //same resl
           && (Prop[i].Open) )                                                                           //and open windows
          {
          Prop[i].Two.XLo=0; Prop[i].Two.XHi=Setup.Twod.XChan[TNo];
          Prop[i].Two.YLo=0; Prop[i].Two.YHi=Setup.Twod.YChan[TNo];
          GTK_ADJUSTMENT(HScrlAdj[i])->page_size=Setup.Twod.XChan[TNo];
          GTK_ADJUSTMENT(HScrlAdj[i])->lower=0.0; GTK_ADJUSTMENT(HScrlAdj[i])->upper=(gfloat)(Setup.Twod.XChan[TNo]);
          GTK_ADJUSTMENT(VScrlAdj[i])->page_size=Setup.Twod.YChan[TNo];
          GTK_ADJUSTMENT(VScrlAdj[i])->lower=-Setup.Twod.YChan[TNo];
          GTK_ADJUSTMENT(VScrlAdj[i])->upper=0.0;
          Prop[i].Two.BoxDrawn=0; RealScroll2=FALSE;
          ReDraw2d=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(VScrlAdj[i]),-Setup.Twod.YChan[TNo]+0.05);
          ReDraw2d=TRUE;  gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[i]),0.05);
          }
       }
   }
RealScroll2=FALSE;

ReDraw2d=FALSE; gtk_adjustment_set_value(GTK_ADJUSTMENT(VScrlAdj[WinNo]),-Setup.Twod.YChan[SNo]+0.05);
ReDraw2d=TRUE;  gtk_adjustment_set_value(GTK_ADJUSTMENT(HScrlAdj[WinNo]),0.05);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ComputeXProjection(gint WinNo)
{
gint SNo,X,Y,Temp,Y1,Y2;
guint32 Index,Counts;
gfloat XF,YF;

SNo=ScreenSpecNo[WinNo][Screen];
if (BananaShowing && (WinNo==BananaWinNo) )                                                                  //Banana gate
   {
   XF=Setup.Twod.XChan[SNo]/8192.0; YF=Setup.Twod.YChan[SNo]/8192.0;
   Prop[WinNo].One.XLo=BGate.XMin*XF+0.5; Prop[WinNo].One.XHi=BGate.XMax*XF+0.5;
                    Y1=BGate.YMin*YF+0.5;                  Y2=BGate.YMax*YF+0.5;
   }
else
   {
   if (!Prop[WinNo].Two.BoxDrawn)                                                                                 //No box
      {
      Prop[WinNo].One.XLo=0; Prop[WinNo].One.XHi=Setup.Twod.XChan[SNo]; 
                       Y1=0;                  Y2=Setup.Twod.YChan[SNo];
      }
   else                                                                                                  //Rectangular box
      {
      if (Prop[WinNo].Two.BoxX2Ch<Prop[WinNo].Two.BoxX1Ch)
         { Temp=Prop[WinNo].Two.BoxX1Ch; Prop[WinNo].Two.BoxX1Ch=Prop[WinNo].Two.BoxX2Ch; Prop[WinNo].Two.BoxX2Ch=Temp; }
      if (Prop[WinNo].Two.BoxY2Ch<Prop[WinNo].Two.BoxY1Ch) 
         { Temp=Prop[WinNo].Two.BoxY1Ch; Prop[WinNo].Two.BoxY1Ch=Prop[WinNo].Two.BoxY2Ch; Prop[WinNo].Two.BoxY2Ch=Temp; }
      Prop[WinNo].One.XLo=Prop[WinNo].Two.BoxX1Ch; Prop[WinNo].One.XHi=Prop[WinNo].Two.BoxX2Ch;
                       Y1=Prop[WinNo].Two.BoxY1Ch;                  Y2=Prop[WinNo].Two.BoxY2Ch; 
      }
   }
Prop[WinNo].One.XLo=CLAMP(Prop[WinNo].One.XLo,0,Setup.Twod.XChan[SNo]-2);
Prop[WinNo].One.XHi=MIN(Prop[WinNo].One.XHi,Setup.Twod.XChan[SNo]-1);
Y1=CLAMP(Y1,0,Setup.Twod.YChan[SNo]-2); Y2=MIN(Y2,Setup.Twod.YChan[SNo]-1);
for (X=0;X<MAX(Setup.Twod.XChan[SNo],Setup.Twod.YChan[SNo]);X++) Proj[OffProj[SNo]+X]=0;
                                                                                 //For safety zero also the unused channels
for (X=Prop[WinNo].One.XLo;X<=Prop[WinNo].One.XHi;X++)
    {
    for (Y=Y1;Y<=Y2;Y++) 
        {
        if (BananaShowing && (WinNo==BananaWinNo) )                                                           //Banana gate
           { 
           if (TestBGate((gint)(X/XF+0.5),(gint)(Y/YF+0.5),BGate)) 
              {
              Index=X+Setup.Twod.XChan[SNo]*Y;
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
              Proj[OffProj[SNo]+X]+=Counts;
              } 
           }
        else                      
           { 
           Index=X+Setup.Twod.XChan[SNo]*Y;
           Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index] : Twod32[Off2d[SNo]+Index];
           Proj[OffProj[SNo]+X]+=Counts; 
           }
        }
    }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void XProj(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
ScreenSpecType[WinNo][Screen]=3;                                 //ScreenSpecType =1(1d), =2(2d), =3(XProj), =4(YProj)
ComputeXProjection(WinNo);
Plot1(WinNo,0,0,0,0);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ComputeYProjection(gint WinNo)
{
gint SNo,X,Y,Temp,X1,X2;
guint32 Index,Counts;
gfloat XF,YF;

SNo=ScreenSpecNo[WinNo][Screen];
if (BananaShowing && (WinNo==BananaWinNo) )                                                              //Banana gate
   {
   XF=Setup.Twod.XChan[SNo]/8192.0; YF=Setup.Twod.YChan[SNo]/8192.0;
   Prop[WinNo].One.XLo=BGate.YMin*YF+0.5; Prop[WinNo].One.XHi=BGate.YMax*YF+0.5;
                    X1=BGate.XMin*XF+0.5;                  X2=BGate.XMax*XF+0.5;
   }
else
   {
   if (!Prop[WinNo].Two.BoxDrawn)                                                                                                 //No box
      {
      Prop[WinNo].One.XLo=0; Prop[WinNo].One.XHi=Setup.Twod.YChan[SNo];
                       X1=0;                  X2=Setup.Twod.XChan[SNo];
      }
   else                                                                                              //Rectangular box
      {
      if (Prop[WinNo].Two.BoxX2Ch<Prop[WinNo].Two.BoxX1Ch)
         { Temp=Prop[WinNo].Two.BoxX1Ch; Prop[WinNo].Two.BoxX1Ch=Prop[WinNo].Two.BoxX2Ch; Prop[WinNo].Two.BoxX2Ch=Temp; }
      if (Prop[WinNo].Two.BoxY2Ch<Prop[WinNo].Two.BoxY1Ch) 
         { Temp=Prop[WinNo].Two.BoxY1Ch; Prop[WinNo].Two.BoxY1Ch=Prop[WinNo].Two.BoxY2Ch; Prop[WinNo].Two.BoxY2Ch=Temp; }
      Prop[WinNo].One.XLo=Prop[WinNo].Two.BoxY1Ch; Prop[WinNo].One.XHi=Prop[WinNo].Two.BoxY2Ch;
                       X1=Prop[WinNo].Two.BoxX1Ch;                  X2=Prop[WinNo].Two.BoxX2Ch; 
      }
   }
Prop[WinNo].One.XLo=CLAMP(Prop[WinNo].One.XLo,0,Setup.Twod.YChan[SNo]-2);
Prop[WinNo].One.XHi=MIN(Prop[WinNo].One.XHi,Setup.Twod.YChan[SNo]-1);
X1=CLAMP(X1,0,Setup.Twod.XChan[SNo]-2); X2=MIN(X2,Setup.Twod.XChan[SNo]-1);
for (Y=0;Y<MAX(Setup.Twod.XChan[SNo],Setup.Twod.YChan[SNo]);Y++) Proj[OffProj[SNo]+Y]=0;    
                                                                                //For safety zero also the unused channels
for (Y=Prop[WinNo].One.XLo;Y<=Prop[WinNo].One.XHi;Y++)
    {
    Index=Setup.Twod.XChan[SNo]*Y;
    for (X=X1;X<=X2;X++) 
        {
        if (BananaShowing && (WinNo==BananaWinNo) )                                                          //Banana gate
           { 
           if (TestBGate((gint)(X/XF+0.5),(gint)(Y/YF+0.5),BGate)) 
              {
              Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index+X] : Twod32[Off2d[SNo]+Index+X];
              Proj[OffProj[SNo]+Y]+=Counts;
              } 
           }
        else                      
           { 
           Counts=Setup.Twod.WSz==1 ? (guint32)Twod16[Off2d[SNo]+Index+X] : Twod32[Off2d[SNo]+Index+X];
           Proj[OffProj[SNo]+Y]+=Counts; 
           }
        }
    }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void YProj(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
ScreenSpecType[WinNo][Screen]=4;                                 //ScreenSpecType =1(1d), =2(2d), =3(XProj), =4(YProj)
ComputeYProjection(WinNo);
Plot1(WinNo,0,0,0,0);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BananaWinDestroyed(GtkWidget *W,gpointer Data)
{ 
gint XPix1,YPix1,XPix2,YPix2,WinNo,OriginX,OriginY,SNo;

WinNo=GPOINTER_TO_INT(Data); OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); SNo=ScreenSpecNo[WinNo][Screen];

if (BananaDrawMode && (BGate.N>2) )
   {
   XPix1=OriginX+XSlope[WinNo]*((gfloat)BGate.X[BGate.N-1]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
   YPix1=OriginY-YSlope[WinNo]*((gfloat)BGate.Y[BGate.N-1]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
   XPix2=OriginX+XSlope[WinNo]*((gfloat)BGate.X[0]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
   YPix2=OriginY-YSlope[WinNo]*((gfloat)BGate.Y[0]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
   gdk_draw_line(Canvas[WinNo]->window,Canvas[WinNo]->style->fg_gc[GTK_STATE_NORMAL],XPix1,YPix1,XPix2,YPix2);
   FindBounds(BGate.X,BGate.N,&BGate.XMin,&BGate.XMax);
   FindBounds(BGate.Y,BGate.N,&BGate.YMin,&BGate.YMax);
   BananaShowing=1;
   }
BananaWinOpen=FALSE; BananaDrawMode=FALSE; BanMoved=BanScaled=BanRotated=FALSE;
g_free(MulW);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanHReSizeCallBack(GtkWidget *W,gpointer Data)
{
gint i,SNo,XSum;
gdouble F,Shift;

SNo=ScreenSpecNo[BananaWinNo][Screen];
if (!BanMoved) { for (i=0;i<BGate.N;i++) { BGateX[i]=BGate.X[i]; BGateY[i]=BGate.Y[i]; } BanMoved=TRUE; }
F=atof(gtk_entry_get_text(GTK_ENTRY(W)));
for (i=0,XSum=0;i<BGate.N;i++) XSum+=BGateX[i]; Shift=XSum*(F-1.0)/BGate.N;
for (i=0;i<BGate.N;i++) BGate.X[i]=F*BGateX[i]-Shift;
BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(BananaWinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanVReSizeCallBack(GtkWidget *W,gpointer Data)
{
gint i,SNo,YSum;
gdouble F,Shift;

SNo=ScreenSpecNo[BananaWinNo][Screen]; 
if (!BanMoved) { for (i=0;i<BGate.N;i++) { BGateX[i]=BGate.X[i]; BGateY[i]=BGate.Y[i]; } BanMoved=TRUE; }
F=atof(gtk_entry_get_text(GTK_ENTRY(W)));
for (i=0,YSum=0;i<BGate.N;i++) YSum+=BGateY[i]; Shift=YSum*(F-1.0)/BGate.N;
for (i=0;i<BGate.N;i++) BGate.Y[i]=F*BGateY[i]-Shift;
BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(BananaWinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanRotateCallBack(GtkWidget *W,gpointer Data)
{
gint i,SNo;
gdouble R,x,y,xp,yp,X0,Y0,r,t,tp;

SNo=ScreenSpecNo[BananaWinNo][Screen];
if (!BanRotated) { for (i=0;i<BGate.N;i++) { BGateX[i]=BGate.X[i]; BGateY[i]=BGate.Y[i]; } BanRotated=TRUE; }
R=atof(gtk_entry_get_text(GTK_ENTRY(W)))*M_PI/180.00;
for (i=0,X0=Y0=0;i<BGate.N;i++) { X0+=BGateX[i]; Y0+=BGateY[i]; } X0=X0/BGate.N; Y0=Y0/BGate.N;        //Centre of gravity
for (i=0;i<BGate.N;i++)
    {
    x=BGateX[i]-X0; y=BGateY[i]-Y0;                                                                         //Shift origin
    r=hypot(x,y); t=atan2(y,x); tp=t-R;                                                     //Convert to polar coordinates
    xp=r*cos(tp); yp=r*sin(tp);                                                       //Convert to rectangular coordinates
    BGate.X[i]=xp+X0; BGate.Y[i]=yp+Y0;                                                               //Restore the origin
    }
BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(BananaWinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanRestore(GtkWidget *W,gpointer Data)
{
gint i,SNo;

SNo=ScreenSpecNo[BananaWinNo][Screen];
for (i=0;i<BGate.N;i++) { BGate.X[i]=BGateX[i]; BGate.Y[i]=BGateY[i]; }
gtk_spin_button_set_value(GTK_SPIN_BUTTON(MulW[0]),0.0); gtk_spin_button_set_value(GTK_SPIN_BUTTON(MulW[1]),0.0);
gtk_entry_set_text(GTK_ENTRY(MulW[2]),"1.000"); gtk_entry_set_text(GTK_ENTRY(MulW[3]),"1.000");
gtk_entry_set_text(GTK_ENTRY(MulW[4]),"0.000");
BanMoved=BanScaled=BanRotated=FALSE; BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(BananaWinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanVMoveCallBack(GtkWidget *W,GtkSpinButton *Spin)
{
gint i,SNo,YF,D;

SNo=ScreenSpecNo[BananaWinNo][Screen]; YF=8192/Setup.Twod.YChan[SNo];
if (!BanMoved) { for (i=0;i<BGate.N;i++) { BGateX[i]=BGate.X[i]; BGateY[i]=BGate.Y[i]; } BanMoved=TRUE; }
D=gtk_spin_button_get_value_as_int(Spin);
for (i=0;i<BGate.N;i++) BGate.Y[i]=BGateY[i]+D*YF;
BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(BananaWinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanHMoveCallBack(GtkWidget *W,GtkSpinButton *Spin)
{
gint i,SNo,XF,D;

SNo=ScreenSpecNo[BananaWinNo][Screen]; XF=8192/Setup.Twod.XChan[SNo];
if (!BanMoved) { for (i=0;i<BGate.N;i++) { BGateX[i]=BGate.X[i]; BGateY[i]=BGate.Y[i]; } BanMoved=TRUE; }
D=gtk_spin_button_get_value_as_int(Spin);
for (i=0;i<BGate.N;i++) BGate.X[i]=BGateX[i]+D*XF;
BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(BananaWinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanAccept(GtkWidget *W,gpointer Data)
{
gint i,SNo;

SNo=ScreenSpecNo[BananaWinNo][Screen];
for (i=0;i<BGate.N;i++) { BGateX[i]=BGate.X[i]; BGateY[i]=BGate.Y[i]; }
gtk_spin_button_set_value(GTK_SPIN_BUTTON(MulW[0]),0.0); gtk_spin_button_set_value(GTK_SPIN_BUTTON(MulW[1]),0.0);
gtk_entry_set_text(GTK_ENTRY(MulW[2]),"1.000"); gtk_entry_set_text(GTK_ENTRY(MulW[3]),"1.000");
gtk_entry_set_text(GTK_ENTRY(MulW[4]),"0.000");
BanMoved=BanScaled=BanRotated=FALSE; BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(BananaWinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BananaDrawCallBack(GtkWidget *W,gpointer Data)
{
static GdkColor BlueC    = {0,0xFFFF,0x0000,0x0000};
static GdkColor WhiteC  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *Style;
gint i,XPix1,YPix1,XPix2,YPix2,WinNo,OriginX,OriginY,SNo;

WinNo=GPOINTER_TO_INT(Data); OriginX=ORIGIN_X; OriginY=ORIGIN_Y(CanvasH[WinNo]); SNo=ScreenSpecNo[WinNo][Screen];

if (!BananaDrawMode) 
   {
   Style=gtk_style_copy(gtk_widget_get_default_style());
   for (i=0;i<5;i++) { Style->fg[i]=Style->text[i]=WhiteC; Style->bg[i]=BlueC; }
   gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"End Draw");
   SetStyleRecursively(W,Style);
   BGate.N=0; 
   BananaDrawMode=TRUE;
   gdk_window_set_cursor(SpecWin[WinNo]->window,gdk_cursor_new(GDK_PENCIL));
   }
else
   {
   if (BGate.N>2)
      {
      XPix1=OriginX+XSlope[WinNo]*((gfloat)BGate.X[BGate.N-1]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
      YPix1=OriginY-YSlope[WinNo]*((gfloat)BGate.Y[BGate.N-1]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
      XPix2=OriginX+XSlope[WinNo]*((gfloat)BGate.X[0]*Setup.Twod.XChan[SNo]/8192.0-Prop[WinNo].Two.XLo);
      YPix2=OriginY-YSlope[WinNo]*((gfloat)BGate.Y[0]*Setup.Twod.YChan[SNo]/8192.0-Prop[WinNo].Two.YLo);
      gdk_draw_line(Canvas[WinNo]->window,Canvas[WinNo]->style->fg_gc[GTK_STATE_NORMAL],XPix1,YPix1,XPix2,YPix2);
      FindBounds(BGate.X,BGate.N,&BGate.XMin,&BGate.XMax);
      FindBounds(BGate.Y,BGate.N,&BGate.YMin,&BGate.YMax);
      BananaShowing=1; BanMoved=BanScaled=BanRotated=FALSE;
      }
   Style=gtk_style_copy(gtk_widget_get_default_style());
   gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Draw");
   SetStyleRecursively(W,Style);
   gdk_window_set_cursor(SpecWin[WinNo]->window,gdk_cursor_new(GDK_LEFT_PTR));
   BananaDrawMode=FALSE;
   }
gtk_style_unref(Style);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BananaHideCallBack(GtkWidget *W,gpointer Data)
{
gint WinNo;

if (BananaDrawMode) { Attention(0,"Error: BGate is still open!\nClick End Draw first"); return; }
WinNo=GPOINTER_TO_INT(Data); 
BananaShowing=FALSE; Refresh(W,GINT_TO_POINTER(WinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BananaShowCallBack(GtkWidget *W,gpointer Data)
{
gint WinNo;

if (BananaDrawMode) { Attention(0,"Error: BGate is still open!\nClick End Draw first"); return; }
WinNo=GPOINTER_TO_INT(Data); 
BananaShowing=TRUE; Refresh(W,GINT_TO_POINTER(WinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BanLineStyle(GtkWidget *W,gpointer Data)
{ BananaLineType=GPOINTER_TO_INT(Data); }
/*-------------------------------------------------------------------------------------------------------------------*/
void BananaLineCallBack(GtkWidget *W,gpointer Data)
{
gint WinNo;
GtkWidget *Win,*VBox,*HBox,*But,*Label;

if (BananaDrawMode) { Attention(0,"Error: BGate is still open!\nClick End Draw first"); return; }
WinNo=GPOINTER_TO_INT(Data); 

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_title(GTK_WINDOW(Win),"Ban Line Style");
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[WinNo]));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_set_usize(GTK_WIDGET(Win),167,85); gtk_widget_set_uposition(GTK_WIDGET(Win),300,300);
VBox=gtk_vbox_new(TRUE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);

Label=gtk_label_new("Select a line style"); gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label ("Thick Dashed"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanLineStyle),GINT_TO_POINTER(0));
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_with_label ("Thick Solid"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanLineStyle),GINT_TO_POINTER(1));
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label ("Thin Dashed"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanLineStyle),GINT_TO_POINTER(2));
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_with_label ("Thin Solid"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanLineStyle),GINT_TO_POINTER(3));
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ValidateBanana(void)                                //Makes sure that the elements of the BGate are within limits
{

BGate.XPar=CLAMP(BGate.XPar,1,MAX_PAR); BGate.YPar=CLAMP(BGate.YPar,1,MAX_PAR);
BGate.N=CLAMP(BGate.N,3,MAX_BAN_POINTS);

/* I think there should be no problem with banana points negative or greater than 8192. 
 * This can be after a move, rotate etc. CLAMP will cause distortions. So we comment this line:
for (i=0;i<BGate.N;i++) { BGate.X[i]=CLAMP(BGate.X[i],0,8191); BGate.Y[i]=CLAMP(BGate.Y[i],0,8191); }
 * The above line is commented 8 Aug 2002 */

FindBounds(BGate.X,BGate.N,&BGate.XMin,&BGate.XMax);
FindBounds(BGate.Y,BGate.N,&BGate.YMin,&BGate.YMax);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ReadBan(GtkWidget *W,gpointer Unused)
{
FILE *Fp;
gchar Str[MAX_FNAME_LENGTH+40],FName[MAX_FNAME_LENGTH];
gint i,j;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(BanDir,FileX->Path);                                                                            //Preserve path
g_free(FileX);

if ((Fp=fopen(FName,"r")) == NULL) { sprintf(Str,"Error opening %s",FName); Attention(300,Str); return; }
for (i=0;i<MAX_BAN_POINTS;i++) BGate.X[i]=BGate.Y[i]=0;
fgets(Str,120,Fp);                                                                                    //Skip title line
//g_print("%s\n",Str);
fscanf(Fp,"%s %d",Str,&BGate.XPar); fscanf(Fp,"%s %d",Str,&BGate.YPar); fscanf(Fp,"%s %d",Str,&BGate.N);
//g_print("XPar=%d YPar=%d N=%d\n",BGate.XPar,BGate.YPar,BGate.N);
for (i=0;i<BGate.N;i++) 
    { 
    fscanf(Fp,"%s %d %d %d",Str,&j,&BGate.X[i],&BGate.Y[i]); 
    //g_print("i=%d j=%d X=%d Y=%d\n",i,j,BGate.X[i],BGate.Y[i]); 
    }
fclose(Fp);
ValidateBanana(); BanMoved=BanScaled=BanRotated=FALSE;
}
/*-------------------------------------------------------------------------------------------------------------------*/
void SaveBan(GtkWidget *W,gpointer Unused)
{
FILE *Fp;
gchar Str[MAX_FNAME_LENGTH+40],FName[MAX_FNAME_LENGTH];
gint i;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(BanDir,FileX->Path);                                                                            //Preserve path
g_free(FileX);

if ((Fp=fopen(FName,"w")) == NULL) { sprintf(Str,"Error opening %s",FName); Attention(300,Str); return; }
fprintf(Fp,"LAMPS BANANA GATE FILE: %s\n",FName);
fprintf(Fp,"XPar= %d\n",BGate.XPar); fprintf(Fp,"YPar= %d\n",BGate.YPar); fprintf(Fp,"N= %d\n",BGate.N);
for (i=0;i<BGate.N;i++) fprintf(Fp,"i,X,Y= %d %d %d\n",i,BGate.X[i],BGate.Y[i]);
fprintf(Fp,"END OF FILE\n");
fclose(Fp);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void MultiBan(GtkWidget *W,gpointer Unused)
{
FILE *Fp;
gchar Str[MAX_FNAME_LENGTH+40];
gint i,j,k;

k=GlobalTemp3; //g_print("MultiBan...GlobalTemp3=%d\n",k);
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(MultiBGateNames[k],"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(BanDir,FileX->Path);                                                                            //Preserve path
g_free(FileX);

AbbreviateFileName(Str,MultiBGateNames[k],18);
gtk_label_set_text(GTK_LABEL(GTK_BIN(MultiBGateButtons[k])->child),Str);                    //Change text inside button
if ((Fp=fopen(MultiBGateNames[k],"r")) == NULL) 
   { sprintf(Str,"Error opening %s",MultiBGateNames[k]); Attention(300,Str); return; }
for (i=0;i<MAX_BAN_POINTS;i++) MultiBGate[k].X[i]=MultiBGate[k].Y[i]=0;
fgets(Str,120,Fp);                                                                                    //Skip title line
fscanf(Fp,"%s %d",Str,&MultiBGate[k].XPar); fscanf(Fp,"%s %d",Str,&MultiBGate[k].YPar); 
fscanf(Fp,"%s %d",Str,&MultiBGate[k].N);
for (i=0;i<MultiBGate[k].N;i++) fscanf(Fp,"%s %d %d %d",Str,&j,&MultiBGate[k].X[i],&MultiBGate[k].Y[i]);
fclose(Fp);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void LoadBananaMulti(GtkWidget *W,gpointer Data)
{ 
if (BananaDrawMode) { Attention(0,"Error: BGate is still open!\nClick End Draw first"); return; }
GlobalTemp3=GPOINTER_TO_INT(Data); 
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Read Banana Gate File",NULL,304,195,TRUE,BanDir,".ban",FALSE,&MultiBan,FALSE);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void LoadBanana(GtkWidget *W,gpointer Unused)
{
if (BananaDrawMode) { Attention(0,"Error: BGate is still open!\nClick End Draw first"); return; }
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Read Banana Gate File",NULL,514,195,TRUE,BanDir,".ban",FALSE,&ReadBan,FALSE);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void SaveBanana(GtkWidget *W,gpointer Unused)
{
if (BananaDrawMode) { Attention(0,"Error: BGate is still open!\nClick End Draw first"); return; }
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Write Banana Gate File",NULL,514,195,FALSE,BanDir,".ban",FALSE,&SaveBan,FALSE);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void CloseBananaMulti(GtkWidget *W,GtkWidget *Win)
{
if (BananaMulti) { BananaMulti=FALSE; gtk_widget_destroy(Win); g_free(MultiBGateButtons); }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void MultiBGateActive(GtkWidget *W,gpointer Data)
{
MultiBGateFlags[GPOINTER_TO_INT(Data)]=1-MultiBGateFlags[GPOINTER_TO_INT(Data)];
Update2d(BananaWinNo);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void BananaMultiCallBack(GtkWidget *W,gpointer Data)
{
gchar Str[20];
gint WinNo,i,j;
GtkWidget *Win,*VBox,*Table,*HBox,*But,*CheckBut;
static gchar *Titles[3]= {"No","File Name","On"};
gint ColWidth[3]={30,130,27};
GtkStyle *TitlesStyle,*BStyle[7];
static GdkColor Black  = {0,0x0000,0x0000,0x0000};
static GdkColor White  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor Bg[7] = { {0,0xAAAA,0x0000,0x0000},{0,0x0000,0x7777,0x0000},{0,0x0000,0x0000,0x8888},
                          {0,0x8888,0x6666,0x0000},{0,0x8888,0x0000,0x8888},{0,0x0000,0x8888,0x8888},
                          {0,0x6666,0x6666,0x6666} };
                          
if (BananaMulti) return;                                                                    //Multi window already open

TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;++i) { TitlesStyle->fg[i]=TitlesStyle->text[i]=White; TitlesStyle->bg[i]=Black; }

for (j=0;j<7;++j)
   {
   BStyle[j]=gtk_style_copy(gtk_widget_get_default_style());
   for (i=0;i<5;++i) { BStyle[j]->fg[i]=BStyle[j]->text[i]=White; BStyle[j]->bg[i]=Bg[j]; }
   }

WinNo=GPOINTER_TO_INT(Data);
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); BananaMulti=TRUE; gtk_grab_add(Win);                            //Make it modal
gtk_widget_set_uposition(GTK_WIDGET(Win),814,220);
gtk_window_set_title(GTK_WINDOW(Win),"Multi BGates");
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[WinNo]));                             //Window visibility
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(CloseBananaMulti),GTK_OBJECT(Win));

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Table=gtk_table_new(1,3,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_box_pack_start(GTK_BOX(HBox),Table,FALSE,FALSE,0);
for (i=0;i<3;i++)
    {
    But=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(But,TitlesStyle);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Table=gtk_table_new(7,3,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_box_pack_start(GTK_BOX(HBox),Table,FALSE,FALSE,0);
MultiBGateButtons=g_new(GtkWidget *,7);
for (i=0;i<7;++i)
    {
    sprintf(Str,"%d",i+1); But=gtk_button_new_with_label(Str); SetStyleRecursively(But,BStyle[i]);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    AbbreviateFileName(Str,MultiBGateNames[i],18);
    MultiBGateButtons[i]=gtk_button_new_with_label(Str); SetStyleRecursively(MultiBGateButtons[i],BStyle[i]);
    gtk_widget_set_size_request(GTK_WIDGET(MultiBGateButtons[i]),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(Table),MultiBGateButtons[i],1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(MultiBGateButtons[i]),"clicked",GTK_SIGNAL_FUNC(LoadBananaMulti),GINT_TO_POINTER(i));
    CheckBut=gtk_check_button_new();
    if (MultiBGateFlags[i]) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckBut),TRUE);
                       else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckBut),FALSE);
    gtk_widget_set_size_request(GTK_WIDGET(CheckBut),ColWidth[2],-1);
    gtk_table_attach(GTK_TABLE(Table),CheckBut,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(CheckBut),"toggled",GTK_SIGNAL_FUNC(MultiBGateActive),GINT_TO_POINTER(i));
    }

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Done"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CloseBananaMulti),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
gtk_style_unref(TitlesStyle); for (i=0;i<7;++i) gtk_style_unref(BStyle[i]);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Banana(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo;
GtkWidget *Win,*HBox,*VBox,*Label,*But,*Frame,*VBox2,*HBox1,*HBox2,*HBox3;
GtkAdjustment *Adj;

if (BananaWinOpen) { Attention(0,"Another Banana window already open!"); return; }

MulW=g_new(GtkWidget *,5);

BananaWinOpen=TRUE; BanMoved=BanScaled=BanRotated=FALSE; BananaMulti=FALSE;
WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];

BGate.XPar=Setup.Twod.XPar[SNo]; BGate.YPar=Setup.Twod.YPar[SNo];   //Caution: values will be overwritten by LoadBanana
BananaShowing=FALSE;                            //This may still leave an old bgate on the screen till the next refresh
BananaWinNo=WinNo;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_size_request(GTK_WIDGET(Win),410,-1);
gtk_widget_set_uposition(GTK_WIDGET(Win),602,0);
gtk_window_set_title(GTK_WINDOW(Win),"Banana Gate");
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[WinNo]));                             //Window visibility
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(BananaWinDestroyed),GINT_TO_POINTER(WinNo));
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
But=gtk_button_new_with_label("Load"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(LoadBanana),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Save"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveBanana),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Draw"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BananaDrawCallBack),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Show"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BananaShowCallBack),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Hide"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BananaHideCallBack),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Line"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BananaLineCallBack),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Multi"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BananaMultiCallBack),GINT_TO_POINTER(WinNo));

Frame=gtk_frame_new("Adjust Banana Gate"); gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
gtk_container_set_border_width(GTK_CONTAINER(Frame),5);
VBox2=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox2);
gtk_container_set_border_width(GTK_CONTAINER(VBox2),5);

HBox1=gtk_hbox_new(TRUE,5); gtk_box_pack_start(GTK_BOX(VBox2),HBox1,TRUE,TRUE,0);
Label=gtk_label_new("Move H"); gtk_box_pack_start(GTK_BOX(HBox1),Label,TRUE,TRUE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new(0.0,-8192.0,8192.0,1.0,100.0,0.0));
MulW[0]=gtk_spin_button_new(Adj,1.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(MulW[0]),FALSE);
gtk_widget_set_size_request(GTK_WIDGET(MulW[0]),80,-1); gtk_box_pack_start(GTK_BOX(HBox1),MulW[0],TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(BanHMoveCallBack),MulW[0]);
Label=gtk_label_new("Move V"); gtk_box_pack_start(GTK_BOX(HBox1),Label,TRUE,TRUE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new(0.0,-8192.0,8192.0,1.0,100.0,0.0));
MulW[1]=gtk_spin_button_new(Adj,1.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(MulW[1]),FALSE);
gtk_widget_set_size_request(GTK_WIDGET(MulW[1]),80,-1); gtk_box_pack_start(GTK_BOX(HBox1),MulW[1],TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(BanVMoveCallBack),MulW[1]);
But=gtk_button_new_with_label("Accept"); gtk_box_pack_start(GTK_BOX(HBox1),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanAccept),NULL);
But=gtk_button_new_with_label("Restore"); gtk_box_pack_start(GTK_BOX(HBox1),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanRestore),NULL);

HBox2=gtk_hbox_new(TRUE,5); gtk_box_pack_start(GTK_BOX(VBox2),HBox2,TRUE,TRUE,0);
Label=gtk_label_new("Scale H"); gtk_box_pack_start(GTK_BOX(HBox2),Label,TRUE,TRUE,0);
MulW[2]=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox2),MulW[2],FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(MulW[2]),60,-1);
gtk_entry_set_text(GTK_ENTRY(MulW[2]),"1.000"); 
gtk_signal_connect(GTK_OBJECT(MulW[2]),"changed",GTK_SIGNAL_FUNC(BanHReSizeCallBack),GINT_TO_POINTER(WinNo));
Label=gtk_label_new("Scale V"); gtk_box_pack_start(GTK_BOX(HBox2),Label,TRUE,TRUE,0);
MulW[3]=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox2),MulW[3],FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(MulW[3]),60,-1);
gtk_entry_set_text(GTK_ENTRY(MulW[3]),"1.000");
gtk_signal_connect(GTK_OBJECT(MulW[3]),"changed",GTK_SIGNAL_FUNC(BanVReSizeCallBack),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Accept"); gtk_box_pack_start(GTK_BOX(HBox2),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanAccept),NULL);
But=gtk_button_new_with_label("Restore"); gtk_box_pack_start(GTK_BOX(HBox2),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanRestore),NULL);

HBox3=gtk_hbox_new(TRUE,5); gtk_box_pack_start(GTK_BOX(VBox2),HBox3,TRUE,TRUE,0);
Label=gtk_label_new("Rotate"); gtk_box_pack_start(GTK_BOX(HBox3),Label,TRUE,TRUE,0);
MulW[4]=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox3),MulW[4],FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(MulW[4]),60,-1);
gtk_entry_set_text(GTK_ENTRY(MulW[4]),"0.000");
gtk_signal_connect(GTK_OBJECT(MulW[4]),"changed",GTK_SIGNAL_FUNC(BanRotateCallBack),GINT_TO_POINTER(WinNo));
But=gtk_button_new_with_label("Accept"); gtk_box_pack_start(GTK_BOX(HBox3),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanAccept),NULL);
But=gtk_button_new_with_label("Restore"); gtk_box_pack_start(GTK_BOX(HBox3),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BanRestore),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Done"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
//----------------------------------------------------------------------------------------------------------------------
void Zero2dSpec(GtkWidget *W,gpointer Data)
{
gint WinNo,SNo;

WinNo=GPOINTER_TO_INT(Data); SNo=ScreenSpecNo[WinNo][Screen];
ZeroTwod(SNo);
Refresh(W,GINT_TO_POINTER(WinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void WriteRadwareMatrixS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];
gint WinNo,SNo;

WinNo=U2->N; SNo=ScreenSpecNo[WinNo][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SpecDir,FileX->Path); SavePrefs();                                                              //Preserve path
g_free(FileX);

RadwareMatrixWrite(FName,SNo);
if (GTK_IS_WIDGET(U2->W)) gtk_widget_destroy(U2->W);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ReadRadwareMatrixS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];
gint WinNo,SNo;

WinNo=U2->N; SNo=ScreenSpecNo[WinNo][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SpecDir,FileX->Path); SavePrefs();                                                              //Preserve path
g_free(FileX);

RadwareMatrixRead(FName,SNo);
Refresh(W,GINT_TO_POINTER(WinNo));
if (GTK_IS_WIDGET(U2->W)) gtk_widget_destroy(U2->W);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Write2dS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];
gint WinNo,SNo;

WinNo=U2->N; SNo=ScreenSpecNo[WinNo][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SpecDir,FileX->Path); SavePrefs();                                                              //Preserve path
g_free(FileX);

if (Setup.Twod.WSz == 2) Write2d32(FName,SNo,FALSE);
else                     Write2d16(FName,SNo,FALSE);

if (GTK_IS_WIDGET(U2->W)) gtk_widget_destroy(U2->W);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Read2dS(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH],Str[80];
gint WinNo,SNo;
FILE *Fp;
gint Y,P1,P2;
guint16 XSize,YSize,NZer,NDat;
size_t ChansRead;

WinNo=U2->N; SNo=ScreenSpecNo[WinNo][Screen];
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SpecDir,FileX->Path); SavePrefs();                                                              //Preserve path
g_free(FileX);

ZeroTwod(SNo); 
if ((Fp=fopen(FName,"r"))==NULL) { Attention(0,"Error accessing file!"); return; }
fread(&XSize,2,1,Fp); fread(&YSize,2,1,Fp);
if ( (XSize != Setup.Twod.XChan[SNo]) || (YSize != Setup.Twod.YChan[SNo]) )
   {
   sprintf(Str,"2d Spec File is (%dx%d) instead of (%dx%d)!\n",XSize,YSize,Setup.Twod.XChan[SNo],Setup.Twod.YChan[SNo]);
   Attention(200,Str); fclose(Fp); return;
   }
else
   {
   for (Y=0,P2=0;Y<YSize;Y++)
       {
       P1=0;
       do
          {
          fread(&NZer,2,1,Fp); fread(&NDat,2,1,Fp); P1+=NZer;
          if (Setup.Twod.WSz == 1) ChansRead=fread(&Twod16[Off2d[SNo]+P2+P1],2,NDat,Fp);
          else                     ChansRead=fread(&Twod32[Off2d[SNo]+P2+P1],4,NDat,Fp);
          if (ChansRead<NDat)
             { sprintf(Str,"2d Spec not read correctly (wrong word size?)"); Attention(250,Str); return; }
          P1+=NDat;
          } while (P1<XSize);
       P2+=XSize;
       }
   }
fclose(Fp);
Refresh(W,GINT_TO_POINTER(WinNo));
if (GTK_IS_WIDGET(U2->W)) gtk_widget_destroy(U2->W);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void WriteRadwareMatrix(GtkWidget *W,gpointer Unused)
{
gint WinNo,SNo;

WinNo=U2->N; SNo=ScreenSpecNo[WinNo][Screen];
if ( (Setup.Twod.WSz != 2) || (Setup.Twod.XChan[SNo] != 4096) || (Setup.Twod.YChan[SNo] != 4096) )
     { Attention(0,"Sorry! Radware .spn needs 4k x 4k DoubleWord"); return; }

FileX=g_new(struct FileSelectType,1);
FileOpenNew("Write Radware Matrix",U2->W,394,TOP_SPACE,FALSE,SpecDir,".spn",FALSE,&WriteRadwareMatrixS,FALSE);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ReadRadwareMatrix(GtkWidget *W,gpointer Unused)
{
gint WinNo,SNo;

WinNo=U2->N; SNo=ScreenSpecNo[WinNo][Screen];
if ( (Setup.Twod.WSz != 2) || (Setup.Twod.XChan[SNo] != 4096) || (Setup.Twod.YChan[SNo] != 4096) )
     { Attention(0,"Sorry! You need 4k x 4k DoubleWord to read Radware .spn files"); return; }

FileX=g_new(struct FileSelectType,1);
FileOpenNew("Read Radware Matrix",U2->W,394,TOP_SPACE,TRUE,SpecDir,".spn",FALSE,&ReadRadwareMatrixS,FALSE);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Write2dFile(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Write 2d File",U2->W,394,TOP_SPACE,FALSE,SpecDir,".z2d",FALSE,&Write2dS,FALSE);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Read2dFile(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 2d File",U2->W,394,TOP_SPACE,TRUE,SpecDir,".z2d",FALSE,&Read2dS,FALSE);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void ReadWrite2dDestroyed(GtkWidget *W,gpointer Unused)
{ g_free(U2); }                                                              //Free the memory allocated in ReadWrite2dFile
/*-----------------------------------------------------------------------------------------------------------------------*/
void ReadWrite2dFile(GtkWidget *W,gpointer Data)
{
GtkWidget *But,*VBox,*HBox,*Label,*Frame,*VBox1;
static GdkColor FrameBg  = {0,0xFFFF,0x0000,0x0000};
static GdkColor FrameFg  = {0,0xFFFF,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;

FrameStyle=gtk_style_copy(gtk_widget_get_default_style());                              //Copy default style to this style
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }              //Set colours
                                                                                                                             
U2=g_new(struct WidgetInt,1);                                                   //Reserve memory for the variables required
                                                                                                                             
U2->N=GPOINTER_TO_INT(Data);
U2->W=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(U2->W),"Read/Write 2d");
gtk_container_set_border_width(GTK_CONTAINER(U2->W),10);
gtk_widget_set_uposition(U2->W,100,TOP_SPACE);
gtk_window_set_transient_for(GTK_WINDOW(U2->W),GTK_WINDOW(SpecWin[U2->N]));                             //Window visibility
gtk_signal_connect(GTK_OBJECT(U2->W),"destroy",GTK_SIGNAL_FUNC(ReadWrite2dDestroyed),U2->W);
                                                                                                                             
VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(U2->W),VBox);
Label=gtk_label_new("Read File will overwrite the displayed spectrum");
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Write File will save the spectrum"); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Label=gtk_label_new("Output Word Size corresponds to that in Setup"); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_with_label("Read\nFile"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Read2dFile),NULL);
But=gtk_button_new_with_label("Write\nFile"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Write2dFile),NULL);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(U2->W));

Frame=gtk_frame_new("RADWARE MATRIX (.spn)"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox1=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Frame),VBox1);
gtk_container_set_border_width(GTK_CONTAINER(VBox1),5);
Label=gtk_label_new("Radware .spn files are 4096x4096 double word");
gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
Label=gtk_label_new("Ensure you have a compatible setup");
gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,2);
But=gtk_button_new_with_label("Read"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReadRadwareMatrix),NULL);
But=gtk_button_new_with_label("Write"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WriteRadwareMatrix),NULL);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(U2->W));

gtk_widget_show_all(U2->W);
gtk_style_unref(FrameStyle);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void MaxCountsCallBack(GtkWidget *W,gpointer Data)
{ 
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].Two.CountsHi=atof(gtk_entry_get_text(GTK_ENTRY(W))); 
}
/*-------------------------------------------------------------------------------------------------------------------*/
void MinCountsCallBack(GtkWidget *W,gpointer Data) 
{ 
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].Two.CountsLo=atof(gtk_entry_get_text(GTK_ENTRY(W))); 
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Toggle2dAuto(GtkWidget *CheckButton,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
if (GTK_TOGGLE_BUTTON(CheckButton)->active) Prop[WinNo].Two.CountsAuto=TRUE; else Prop[WinNo].Two.CountsAuto=FALSE; 
}
/*-------------------------------------------------------------------------------------------------------------------*/
void TScaleLog(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
if (GTK_TOGGLE_BUTTON(W)->active) Prop[WinNo].Two.TScale=0;
if (!Prop[WinNo].Two.CountsAuto) { Prop[WinNo].Two.CountsHi=4.0e9; gtk_entry_set_text(GTK_ENTRY(ColUse[32]),"4.00e+09"); }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void TScaleSqrt(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
if (GTK_TOGGLE_BUTTON(W)->active) Prop[WinNo].Two.TScale=1;
if (!Prop[WinNo].Two.CountsAuto) { Prop[WinNo].Two.CountsHi=1024.0; gtk_entry_set_text(GTK_ENTRY(ColUse[32]),"1024"); }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void TScaleLin(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
if (GTK_TOGGLE_BUTTON(W)->active) Prop[WinNo].Two.TScale=2;
if (!Prop[WinNo].Two.CountsAuto) { Prop[WinNo].Two.CountsHi=32.0; gtk_entry_set_text(GTK_ENTRY(ColUse[32]),"32"); }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Apply2dScale(GtkWidget *W,GtkWidget *HiText)
{
gint WinNo,i;
gchar Str[20];

WinNo=GlobalTemp2;
Prop[WinNo].Two.CountsHi=MAX(Prop[WinNo].Two.CountsHi,4.0);
if (Prop[WinNo].Two.TScale==0) Prop[WinNo].Two.CountsLo=CLAMP(Prop[WinNo].Two.CountsLo,1.0,Prop[WinNo].Two.CountsHi-1);
                          else Prop[WinNo].Two.CountsLo=CLAMP(Prop[WinNo].Two.CountsLo,0.0,Prop[WinNo].Two.CountsHi-1);
for (i=0;i<32;i++) Pen[i]=GetPen(i);
Refresh(W,GINT_TO_POINTER(WinNo));
if (Prop[WinNo].Two.CountsHi>9.0e6) sprintf(Str,"%.2e",Prop[WinNo].Two.CountsHi); else sprintf(Str,"%.0f",Prop[WinNo].Two.CountsHi);
gtk_entry_set_text(GTK_ENTRY(HiText),Str);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void DefaultHi(GtkWidget *W,GtkWidget *HiText)
{
gint WinNo;
gchar Str[20];

WinNo=GlobalTemp2;
Prop[WinNo].Two.CountsHi=4.0e9;
sprintf(Str,"%.2e",Prop[WinNo].Two.CountsHi);
gtk_entry_set_text(GTK_ENTRY(HiText),Str);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void DefaultLo(GtkWidget *W,GtkWidget *LoText)
{
gint WinNo;
gchar Str[20];

WinNo=GlobalTemp2;
Prop[WinNo].Two.CountsLo=1.0;
sprintf(Str,"%.0f",Prop[WinNo].Two.CountsLo);
gtk_entry_set_text(GTK_ENTRY(LoText),Str);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void DefaultAuto(GtkWidget *W,GtkWidget *CheckAuto)
{
gint WinNo;

WinNo=GlobalTemp2;
Prop[WinNo].Two.CountsAuto=FALSE;
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckAuto),Prop[WinNo].Two.CountsAuto);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void DefaultTScale(GtkWidget *W,GtkWidget *RadioTScale0)
{
gint WinNo,i,j;
GtkStyle *BStyle;
GdkColor BColour,FColour;

WinNo=GlobalTemp2;
Prop[WinNo].Two.TScale=0;
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RadioTScale0),TRUE);
Plain2d(NULL,NULL); for (i=0;i<32;++i) Pen[i]=GetPen(i);

for (i=0;i<32;++i)
    {
    BStyle=gtk_style_copy(gtk_widget_get_default_style());
    FColour.pixel=0L; FColour.red=0xFFFF-D2PenColours[i][0];
    FColour.green=0xFFFF-D2PenColours[i][1]; FColour.blue=0xFFFF-D2PenColours[i][2];
    BColour.pixel=0L; BColour.red=D2PenColours[i][0]; BColour.green=D2PenColours[i][1]; BColour.blue=D2PenColours[i][2];
    for (j=0;j<5;++j) { BStyle->fg[j]=BStyle->text[j]=FColour; BStyle->bg[j]=BColour; }
    SetStyleRecursively(ColUse[i],BStyle);
    gtk_style_unref(BStyle);
    }
Refresh(W,GINT_TO_POINTER(WinNo));
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Scale2dPlotDestroyed(GtkWidget *W,gpointer Data)
{
gint WinNo;

WinNo=GPOINTER_TO_INT(Data);
Prop[WinNo].Two.CountsHi=MAX(Prop[WinNo].Two.CountsHi,4.0);
if (Prop[WinNo].Two.TScale==0) Prop[WinNo].Two.CountsLo=CLAMP(Prop[WinNo].Two.CountsLo,1.0,Prop[WinNo].Two.CountsHi-1);
                          else Prop[WinNo].Two.CountsLo=CLAMP(Prop[WinNo].Two.CountsLo,0.0,Prop[WinNo].Two.CountsHi-1);
Refresh(W,GINT_TO_POINTER(WinNo));
g_free(ColUse);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ChangeColour(GtkWidget *W,gpointer Data)
{
gint i,j;
GdkColor BColour,FColour;
GtkStyle *BStyle;
gdouble Colour[4];

i=GPOINTER_TO_INT(Data);
gtk_color_selection_get_color(GTK_COLOR_SELECTION(W),Colour);
BStyle=gtk_style_copy(gtk_widget_get_default_style());
D2PenColours[i][0]=Colour[0]*0xFFFF; D2PenColours[i][1]=Colour[1]*0xFFFF; D2PenColours[i][2]=Colour[2]*0xFFFF;
FColour.pixel=0L; FColour.red=0xFFFF-D2PenColours[i][0];
FColour.green=0xFFFF-D2PenColours[i][1]; FColour.blue=0xFFFF-D2PenColours[i][2];
BColour.pixel=0L; BColour.red=D2PenColours[i][0]; BColour.green=D2PenColours[i][1]; BColour.blue=D2PenColours[i][2];
for (j=0;j<5;++j) { BStyle->fg[j]=BStyle->text[j]=FColour; BStyle->bg[j]=BColour; }
SetStyleRecursively(ColUse[i],BStyle);
gtk_style_unref(BStyle);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void ColourSelector(GtkWidget *But,gpointer Data)
{
GtkWidget *Win;

Win=gtk_color_selection_dialog_new("Chose a colour"); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[GlobalTemp2]));
gtk_widget_set_uposition(GTK_WIDGET(Win),270,168); 
gtk_signal_connect_object(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(Win)->cancel_button),"clicked",
                          GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_signal_connect_object(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(Win)->ok_button),"clicked",
                          GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(Win)->colorsel),"color_changed",GTK_SIGNAL_FUNC(ChangeColour),Data);
gtk_widget_show_all(Win);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Scale2dPlot(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*VBox,*VBox2,*HBox,*Label,*But,*LoText,*CheckAuto,*RadioTScale0,*RadioTScale1,*RadioTScale2;
gint WinNo,i,j;
gchar Str[20];
GdkColor BColour,FColour;
GtkStyle *BStyle;

ColUse=g_new(GtkWidget *,33);
WinNo=GPOINTER_TO_INT(Data); GlobalTemp2=WinNo;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                         //Create a modal window
gtk_window_set_title(GTK_WINDOW(Win),"2d Colours");                                                          //Window Title
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(Scale2dPlotDestroyed),Data);
gtk_widget_set_usize(GTK_WIDGET(Win),347,140); gtk_widget_set_uposition(GTK_WIDGET(Win),320,0);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(SpecWin[WinNo]));
VBox=gtk_vbox_new(TRUE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(VBox),HBox);
Label=gtk_label_new("Max Counts"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
ColUse[32]=gtk_entry_new(); gtk_widget_set_usize(GTK_WIDGET(ColUse[32]),80,0);
gtk_box_pack_start(GTK_BOX(HBox),ColUse[32],FALSE,FALSE,0);
if (Prop[WinNo].Two.CountsHi>9.0e6) sprintf(Str,"%.2e",Prop[WinNo].Two.CountsHi); 
else                                sprintf(Str,"%.0f",Prop[WinNo].Two.CountsHi);
gtk_entry_set_text(GTK_ENTRY(ColUse[32]),Str);
gtk_signal_connect(GTK_OBJECT(ColUse[32]),"changed",GTK_SIGNAL_FUNC(MaxCountsCallBack),Data);

Label=gtk_label_new("Min Counts"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
LoText=gtk_entry_new(); gtk_widget_set_usize(GTK_WIDGET(LoText),80,0);
gtk_box_pack_start(GTK_BOX(HBox),LoText,FALSE,FALSE,0);                                         //Define and place TextEntry
if (Prop[WinNo].Two.CountsLo>9.0e6) sprintf(Str,"%.2e",Prop[WinNo].Two.CountsLo); 
else                                sprintf(Str,"%.0f",Prop[WinNo].Two.CountsLo);
gtk_entry_set_text(GTK_ENTRY(LoText),Str);                                                                   //Default text
gtk_signal_connect(GTK_OBJECT(LoText),"changed",GTK_SIGNAL_FUNC(MinCountsCallBack),Data);                //Connect callback

HBox=gtk_hbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(VBox),HBox);
CheckAuto=gtk_check_button_new_with_label("Auto"); 
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckAuto),Prop[WinNo].Two.CountsAuto);
gtk_box_pack_start(GTK_BOX(HBox),CheckAuto,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(CheckAuto),"toggled",GTK_SIGNAL_FUNC(Toggle2dAuto),Data);

RadioTScale0=gtk_radio_button_new_with_label(NULL,"Log");
gtk_signal_connect(GTK_OBJECT(RadioTScale0),"toggled",GTK_SIGNAL_FUNC(TScaleLog),Data);
if (Prop[WinNo].Two.TScale==0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RadioTScale0),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),RadioTScale0,TRUE,FALSE,0);

RadioTScale1=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(RadioTScale0)),"Sqrt");
gtk_signal_connect(GTK_OBJECT(RadioTScale1),"toggled",GTK_SIGNAL_FUNC(TScaleSqrt),Data);
if (Prop[WinNo].Two.TScale==1) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RadioTScale1),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),RadioTScale1,TRUE,FALSE,0);

RadioTScale2=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(RadioTScale1)),"Lin");
gtk_signal_connect(GTK_OBJECT(RadioTScale2),"toggled",GTK_SIGNAL_FUNC(TScaleLin),Data);
if (Prop[WinNo].Two.TScale==2) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RadioTScale2),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),RadioTScale2,TRUE,FALSE,0);

VBox2=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox2,FALSE,FALSE,0); 
for (i=0;i<32;++i) 
    { 
    if ((i==0) || (i==16)) { HBox=gtk_hbox_new(FALSE,1); gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,1); }
    sprintf(Str,"%d",i+1); ColUse[i]=gtk_button_new_with_label(Str);
    BStyle=gtk_style_copy(gtk_widget_get_default_style());
    FColour.pixel=0L; FColour.red=0xFFFF-D2PenColours[i][0]; 
    FColour.green=0xFFFF-D2PenColours[i][1]; FColour.blue=0xFFFF-D2PenColours[i][2];
    BColour.pixel=0L; BColour.red=D2PenColours[i][0]; BColour.green=D2PenColours[i][1]; BColour.blue=D2PenColours[i][2];
    for (j=0;j<5;++j) { BStyle->fg[j]=BStyle->text[j]=FColour; BStyle->bg[j]=BColour; } 
    SetStyleRecursively(ColUse[i],BStyle);
    gtk_widget_set_usize(GTK_WIDGET(ColUse[i]),20,16); gtk_box_pack_start(GTK_BOX(HBox),ColUse[i],FALSE,FALSE,0); 
    gtk_signal_connect(GTK_OBJECT(ColUse[i]),"clicked",GTK_SIGNAL_FUNC(ColourSelector),GINT_TO_POINTER(i));
    gtk_style_unref(BStyle);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Defaults"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DefaultHi),ColUse[32]);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DefaultLo),LoText);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DefaultAuto),CheckAuto);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DefaultTScale),RadioTScale0);
But=gtk_button_new_with_label("Apply"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Apply2dScale),ColUse[32]);
But=gtk_button_new_with_label("Ok"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void Plot2(gint WinNo,gint WindowX,gint WindowY,gint WindowW,gint WindowH)
{
GtkWidget *VBox,*HBox,*Menu,*MenuItem,*HScrlBar,*VScrlBar,*SubMenu1,*SubMenuItem;
gint i,SNo,WinW,WinH,WinX,WinY;
gchar Str[80];
gfloat Temp;

PixMap[WinNo]=NULL; SNo=ScreenSpecNo[WinNo][Screen];
ReDraw2d=TRUE;
if (ScreenSpecType[WinNo][Screen]==2)                                                                         //2d plot
   {
   Prop[WinNo].Two.XLo=0; Prop[WinNo].Two.XHi=Setup.Twod.XChan[SNo]; 
   Prop[WinNo].Two.YLo=0; Prop[WinNo].Two.YHi=Setup.Twod.YChan[SNo]; 
   Prop[WinNo].Two.CountsLo=1.0; Prop[WinNo].Two.CountsHi=4.0e9; 
   Prop[WinNo].Two.CountsAuto=0; Prop[WinNo].Two.TScale=0; 
   Prop[WinNo].Two.BoxX1Ch=0; Prop[WinNo].Two.BoxX2Ch=Setup.Twod.XChan[SNo]-1;
   Prop[WinNo].Two.BoxY1Ch=0; Prop[WinNo].Two.BoxY2Ch=Setup.Twod.YChan[SNo]-1;
   Prop[WinNo].Two.BoxDrawn=0;
   for (i=0;i<ZOOM_LEVELS;i++) 
       { 
       Prop[WinNo].Two.ZoomXLo[i]=0; Prop[WinNo].Two.ZoomXHi[i]=Setup.Twod.XChan[SNo]; 
       Prop[WinNo].Two.ZoomYLo[i]=0; Prop[WinNo].Two.ZoomYHi[i]=Setup.Twod.YChan[SNo]; 
       }
   if (Prop[WinNo].Open) gtk_widget_destroy(SpecWin[WinNo]);
   SpecWin[WinNo]=gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_widget_set_usize(GTK_WIDGET(SpecWin[WinNo]),WindowW,WindowH);
   gtk_widget_set_uposition(GTK_WIDGET(SpecWin[WinNo]),WindowX,WindowY);
   gtk_window_set_policy(GTK_WINDOW(SpecWin[WinNo]),TRUE,TRUE,FALSE);
   }
else                                                                                     //Coming back after a projection 
   {
   if (SpecWin[WinNo]->window != NULL)                             //Coming back after a projection to an existing window 
      {
      WinW=MIN((gint)SpecWin[WinNo]->allocation.width,1015); 
      WinH=MIN((gint)SpecWin[WinNo]->allocation.height,556);
      gdk_window_get_root_origin(SpecWin[WinNo]->window,&WinX,&WinY); if (WinW==1015) { WinX=0; WinY=TOP_SPACE; }
      gtk_widget_destroy(SpecWin[WinNo]); SpecWin[WinNo]=gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_usize(GTK_WIDGET(SpecWin[WinNo]),WinW,WinH);
      gtk_widget_set_uposition(GTK_WIDGET(SpecWin[WinNo]),WinX,WinY);
      gtk_window_set_policy(GTK_WINDOW(SpecWin[WinNo]),TRUE,TRUE,FALSE);
      }
   else            //In case of "Screen N" to a screen where a projection was already made, we effect a return to 2d plot
      {
      SpecWin[WinNo]=gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_usize(GTK_WIDGET(SpecWin[WinNo]),WindowW,WindowH);
      gtk_widget_set_uposition(GTK_WIDGET(SpecWin[WinNo]),WindowX,WindowY);
      gtk_window_set_policy(GTK_WINDOW(SpecWin[WinNo]),TRUE,TRUE,FALSE);
      }
   ScreenSpecType[WinNo][Screen]=2;                                                             //Now its back to 2d plot
   }
sprintf(Str,"2d#%d %s",SNo+1,Setup.Twod.Name[SNo]); gtk_window_set_title(GTK_WINDOW(SpecWin[WinNo]),Str);
Prop[WinNo].Open=1;
gtk_signal_connect(GTK_OBJECT(SpecWin[WinNo]),"delete_event",GTK_SIGNAL_FUNC(CloseSpecWin),GINT_TO_POINTER(WinNo));
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(SpecWin[WinNo]),VBox);
HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox),HBox);

Temp=(gfloat)Setup.Twod.YChan[SNo];
VScrlAdj[WinNo]=gtk_adjustment_new(0.0,-Temp,0.0,0.01*Temp,0.05*Temp,Temp);
gtk_signal_connect(GTK_OBJECT(VScrlAdj[WinNo]),"value_changed",GTK_SIGNAL_FUNC(VScroll2),GINT_TO_POINTER(WinNo));
VScrlBar=gtk_vscrollbar_new(GTK_ADJUSTMENT(VScrlAdj[WinNo]));
gtk_range_set_update_policy(GTK_RANGE(VScrlBar),GTK_UPDATE_DISCONTINUOUS);
gtk_box_pack_start(GTK_BOX(HBox),VScrlBar,FALSE,FALSE,0);

Canvas[WinNo]=gtk_drawing_area_new();
gtk_box_pack_start(GTK_BOX(HBox),Canvas[WinNo],TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(Canvas[WinNo]),"expose_event",GTK_SIGNAL_FUNC(CanvasExpose2),GINT_TO_POINTER(WinNo));
gtk_signal_connect(GTK_OBJECT(Canvas[WinNo]),"configure_event",GTK_SIGNAL_FUNC(CanvasConfig2),GINT_TO_POINTER(WinNo));
gtk_signal_connect(GTK_OBJECT(Canvas[WinNo]),"event",GTK_SIGNAL_FUNC(CanvasEvent2),GINT_TO_POINTER(WinNo));

Temp=(gfloat)Setup.Twod.XChan[SNo];
HScrlAdj[WinNo]=gtk_adjustment_new(0.0,0.0,Temp,0.01*Temp,0.05*Temp,Temp);
gtk_signal_connect(GTK_OBJECT(HScrlAdj[WinNo]),"value_changed",GTK_SIGNAL_FUNC(HScroll2),GINT_TO_POINTER(WinNo));
HScrlBar=gtk_hscrollbar_new(GTK_ADJUSTMENT(HScrlAdj[WinNo]));
gtk_range_set_update_policy(GTK_RANGE(HScrlBar),GTK_UPDATE_DISCONTINUOUS);
gtk_box_pack_start(GTK_BOX(VBox),HScrlBar,FALSE,FALSE,0);

Menu=gtk_menu_new(); SubMenu1=gtk_menu_new();
MenuItem=gtk_menu_item_new_with_label("Zoom"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Zoom2),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("UnZoom"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(UnZoom2),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Full"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Full2),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Colours"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Scale2dPlot),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("X-Proj"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(XProj),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Y-Proj"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(YProj),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Refresh"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Refresh),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Hide Box"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(HideBox),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Volume"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Volume),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("BGate"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(Banana),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Zero Spec"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_menu_item_set_submenu(GTK_MENU_ITEM(MenuItem),SubMenu1);
SubMenuItem=gtk_menu_item_new_with_label("Sure?"); gtk_menu_append(GTK_MENU(SubMenu1),SubMenuItem); 
gtk_widget_show(SubMenuItem);
gtk_signal_connect(GTK_OBJECT(SubMenuItem),"activate",GTK_SIGNAL_FUNC(Zero2dSpec),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("Read/Write"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(ReadWrite2dFile),GINT_TO_POINTER(WinNo));
MenuItem=gtk_menu_item_new_with_label("PS/EPS/PDF"); gtk_menu_append(GTK_MENU(Menu),MenuItem); gtk_widget_show(MenuItem);
gtk_signal_connect(GTK_OBJECT(MenuItem),"activate",GTK_SIGNAL_FUNC(PreparePS2),GINT_TO_POINTER(WinNo));

gtk_signal_connect_object(GTK_OBJECT(Canvas[WinNo]),"button_press_event",GTK_SIGNAL_FUNC(CanvasPopup),GTK_OBJECT(Menu));
gtk_widget_set_events(Canvas[WinNo],GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK);

gtk_widget_show_all(SpecWin[WinNo]);
}
/*-------------------------------------------------------------------------------------------------------------------*/
