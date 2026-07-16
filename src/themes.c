#include <gtk/gtk.h>
extern glong D2PenColours[32][3];                                                                              /*RGB Colours for 2d plots*/
extern gint ThemeChanged;
extern gint Theme1d,Theme2d;
extern GdkColor Colour1D[10];   //0=Backg,1=Plot,2=FitBkg,3=Axis,4=Title,5=Cursors,6=Pk Markers,7=Fit ROI,8=Fit,9=Perm. Cursors
extern GdkColor Colour2D[10];   //0=Background,1=Axis,2=Title,3=Box,4=Permanent Box,5=Banana

/* For 1d themes. We set the following colours:
   0=Background , 1=Plot , 2=FitBkg , 3=Axis , 4=Title , 5=Cursors, 6=Peak Markers, 7=Fit ROI, 8=Fit, 9=Permanent Cursors */
/*-----------------------------------------------------------------------------------------------------------------------*/
void Rose1d(GtkWidget *W,gpointer Data)
{
Theme1d=0;
Colour1D[0].pixel=0; Colour1D[0].red=0xFFFF; Colour1D[0].green=0xDADA; Colour1D[0].blue=0xB9B9; //Background
Colour1D[1].pixel=0; Colour1D[1].red=0xFFFF; Colour1D[1].green=0x6363; Colour1D[1].blue=0x4747; //Plot
Colour1D[2].pixel=0; Colour1D[2].red=0x0000; Colour1D[2].green=0xAAAA; Colour1D[2].blue=0xCCCC; //FitBkg
Colour1D[3].pixel=0; Colour1D[3].red=0xA5A5; Colour1D[3].green=0x2A2A; Colour1D[3].blue=0x2A2A; //Axis
Colour1D[4].pixel=0; Colour1D[4].red=0xFFFF; Colour1D[4].green=0x0000; Colour1D[4].blue=0x0000; //Title
Colour1D[5].pixel=0; Colour1D[5].red=0x7777; Colour1D[5].green=0x0000; Colour1D[5].blue=0xAAAA; //Cursors
Colour1D[6].pixel=0; Colour1D[6].red=0x0000; Colour1D[6].green=0xCCCC; Colour1D[6].blue=0x0000; //Peak Markers
Colour1D[7].pixel=0; Colour1D[7].red=0x0000; Colour1D[7].green=0x0000; Colour1D[7].blue=0x0000; //Fit ROI
Colour1D[8].pixel=0; Colour1D[8].red=0x0000; Colour1D[8].green=0xAAAA; Colour1D[8].blue=0xCCCC; //Fit  (Unused?)
Colour1D[9].pixel=0; Colour1D[9].red=0xFFFF; Colour1D[9].green=0xFFFF; Colour1D[9].blue=0xFFFF; //Permanent Cursors
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Sky1d(GtkWidget *W,gpointer Data)
{
Theme1d=1;
Colour1D[0].pixel=0; Colour1D[0].red=0xCCCC; Colour1D[0].green=0xDDDD; Colour1D[0].blue=0xFFFF; //Background
Colour1D[1].pixel=0; Colour1D[1].red=0xAAAA; Colour1D[1].green=0x7777; Colour1D[1].blue=0x7777; //Plot
Colour1D[2].pixel=0; Colour1D[2].red=0x0000; Colour1D[2].green=0x0000; Colour1D[2].blue=0xFFFF; //FitBkg
Colour1D[3].pixel=0; Colour1D[3].red=0x0000; Colour1D[3].green=0x0000; Colour1D[3].blue=0xFFFF; //Axis
Colour1D[4].pixel=0; Colour1D[4].red=0xFFFF; Colour1D[4].green=0x0000; Colour1D[4].blue=0x0000; //Title
Colour1D[5].pixel=0; Colour1D[5].red=0x0000; Colour1D[5].green=0xAAAA; Colour1D[5].blue=0xAAAA; //Cursors
Colour1D[6].pixel=0; Colour1D[6].red=0x6B6B; Colour1D[6].green=0x8E8E; Colour1D[6].blue=0x0023; //Peak Markers
Colour1D[7].pixel=0; Colour1D[7].red=0x0000; Colour1D[7].green=0x0000; Colour1D[7].blue=0x0000; //Fit ROI
Colour1D[8].pixel=0; Colour1D[8].red=0x6B6B; Colour1D[8].green=0x8E8E; Colour1D[8].blue=0x2323; //Fit  (Unused?)
Colour1D[9].pixel=0; Colour1D[9].red=0x0000; Colour1D[9].green=0x0000; Colour1D[9].blue=0x0000; //Permanent Cursors
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Cucumber1d(GtkWidget *W,gpointer Data)
{
Theme1d=2;
Colour1D[0].pixel=0; Colour1D[0].red=0xDDDD; Colour1D[0].green=0xFFFF; Colour1D[0].blue=0xDDDD; //Background
Colour1D[1].pixel=0; Colour1D[1].red=0x7777; Colour1D[1].green=0x0000; Colour1D[1].blue=0x0000; //Plot
Colour1D[2].pixel=0; Colour1D[2].red=0x0000; Colour1D[2].green=0x0000; Colour1D[2].blue=0xFFFF; //FitBkg
Colour1D[3].pixel=0; Colour1D[3].red=0x0000; Colour1D[3].green=0x4444; Colour1D[3].blue=0x0000; //Axis
Colour1D[4].pixel=0; Colour1D[4].red=0x0000; Colour1D[4].green=0x0000; Colour1D[4].blue=0x0000; //Title
Colour1D[5].pixel=0; Colour1D[5].red=0x6B6B; Colour1D[5].green=0x8E8E; Colour1D[5].blue=0x2323; //Cursors
Colour1D[6].pixel=0; Colour1D[6].red=0xFFFF; Colour1D[6].green=0x0000; Colour1D[6].blue=0x0000; //Peak Markers
Colour1D[7].pixel=0; Colour1D[7].red=0x0000; Colour1D[7].green=0x0000; Colour1D[7].blue=0x0000; //Fit ROI
Colour1D[8].pixel=0; Colour1D[8].red=0xFFFF; Colour1D[8].green=0x0000; Colour1D[8].blue=0x0000; //Fit  (Unused?)
Colour1D[9].pixel=0; Colour1D[9].red=0x0000; Colour1D[9].green=0x0000; Colour1D[9].blue=0xFFFF; //Permanent Cursors
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Sandy1d(GtkWidget *W,gpointer Data)
{
Theme1d=4;
Colour1D[0].pixel=0; Colour1D[0].red=0xFFFF; Colour1D[0].green=0xDDDD; Colour1D[0].blue=0x7777; //Background
Colour1D[1].pixel=0; Colour1D[1].red=0xFFFF; Colour1D[1].green=0x0000; Colour1D[1].blue=0x0000; //Plot
Colour1D[2].pixel=0; Colour1D[2].red=0x0000; Colour1D[2].green=0x0000; Colour1D[2].blue=0xFFFF; //FitBkg
Colour1D[3].pixel=0; Colour1D[3].red=0x3333; Colour1D[3].green=0x3333; Colour1D[3].blue=0x0000; //Axis
Colour1D[4].pixel=0; Colour1D[4].red=0xFFFF; Colour1D[4].green=0x0000; Colour1D[4].blue=0x0000; //Title
Colour1D[5].pixel=0; Colour1D[5].red=0x0000; Colour1D[5].green=0x0000; Colour1D[5].blue=0x7777; //Cursors
Colour1D[6].pixel=0; Colour1D[6].red=0x0000; Colour1D[6].green=0x0000; Colour1D[6].blue=0xCCCC; //Peak Markers
Colour1D[7].pixel=0; Colour1D[7].red=0x0000; Colour1D[7].green=0x0000; Colour1D[7].blue=0x0000; //Fit ROI
Colour1D[8].pixel=0; Colour1D[8].red=0x0000; Colour1D[8].green=0x0000; Colour1D[8].blue=0x0000; //Fit  (Unused?)
Colour1D[9].pixel=0; Colour1D[9].red=0x0000; Colour1D[9].green=0x4444; Colour1D[9].blue=0x0000; //Permanent Cursors
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Plain1d(GtkWidget *W,gpointer Data)
{
Theme1d=3;
Colour1D[0].pixel=0; Colour1D[0].red=0xFFFF; Colour1D[0].green=0xFFFF; Colour1D[0].blue=0xFFFF; //Background
Colour1D[1].pixel=0; Colour1D[1].red=0x0000; Colour1D[1].green=0x0000; Colour1D[1].blue=0x0000; //Plot
Colour1D[2].pixel=0; Colour1D[2].red=0x0000; Colour1D[2].green=0x0000; Colour1D[2].blue=0xFFFF; //FitBkg
Colour1D[3].pixel=0; Colour1D[3].red=0x0000; Colour1D[3].green=0x7777; Colour1D[3].blue=0x0000; //Axis
Colour1D[4].pixel=0; Colour1D[4].red=0xFFFF; Colour1D[4].green=0x0000; Colour1D[4].blue=0x0000; //Title
Colour1D[5].pixel=0; Colour1D[5].red=0x0000; Colour1D[5].green=0x0000; Colour1D[5].blue=0xFFFF; //Cursors
Colour1D[6].pixel=0; Colour1D[6].red=0xAAAA; Colour1D[6].green=0x0000; Colour1D[6].blue=0x4444; //Peak Markers
Colour1D[7].pixel=0; Colour1D[7].red=0x0000; Colour1D[7].green=0x0000; Colour1D[7].blue=0x0000; //Fit ROI
Colour1D[8].pixel=0; Colour1D[8].red=0xFF00; Colour1D[8].green=0x0000; Colour1D[8].blue=0x0000; //Fit  (Unused?)
Colour1D[9].pixel=0; Colour1D[9].red=0x0000; Colour1D[9].green=0x7777; Colour1D[9].blue=0x0000; //Permanent Cursors
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ComputeRGB(gint i,glong *R,glong *G,glong *B)
{
switch (i)
   {
   case  0: *R=0;     *G=0;     *B=65535; break;    //blue
   case  1: *R=0;     *G=65535; *B=0;     break;    //green
   case  2: *R=0;     *G=65535; *B=65535; break;    //cyan
   case  3: *R=65535; *G=0;     *B=0;     break;    //red
   case  4: *R=65535; *G=0;     *B=65535; break;    //magenta
   case  5: *R=42240; *G=10752; *B=10752; break;    //brown
   case  6: *R=54016; *G=54016; *B=54016; break;    //light gray
   case  7: *R=26880; *G=26880; *B=26880; break;    //dim gray
   case  8: *R=40960; *G=26880; *B=65535; break;    //light blue
   case  9: *R=40960; *G=65535; *B=40960; break;    //light green
   case 10: *R=40960; *G=65535; *B=65535; break;    //light cyan
   case 11: *R=65535; *G=40960; *B=40960; break;    //light red
   case 12: *R=65535; *G=40960; *B=65535; break;    //light magenta
   case 13: *R=65535; *G=65535; *B=0;     break;    //yellow
   case 14: *R=0;     *G=0;     *B=0;     break;    //black

   case 15: *R=10000; *G=10000; *B=10000; break;    //black variation
   case 16: *R=10000; *G=10000; *B=65535; break;    //blue variation
   case 17: *R=10000; *G=65535; *B=10000; break;    //green variation
   case 18: *R=10000; *G=65535; *B=65535; break;    //cyan variation
   case 19: *R=65535; *G=10000; *B=10000; break;    //red variation
   case 20: *R=65535; *G=10000; *B=65535; break;    //magenta variation
   case 21: *R=42240; *G=20752; *B=20752; break;    //brown variation
   case 22: *R=54016; *G=54016; *B=54016; break;    //light gray (no) variation
   case 23: *R=26880; *G=26880; *B=26880; break;    //dim gray (no) variation
   case 24: *R=20960; *G=26880; *B=65535; break;    //light blue variation
   case 25: *R=20960; *G=65535; *B=20960; break;    //light green variation
   case 26: *R=20960; *G=65535; *B=65535; break;    //light cyan variation
   case 27: *R=65535; *G=20960; *B=20960; break;    //light red variation
   case 28: *R=65535; *G=20960; *B=65535; break;    //light magenta variation
   case 29: *R=65535; *G=65535; *B=1000;  break;    //yellow variation

   case 30: *R=0;     *G=0;     *B=0;     break;    //black again
   case 31: *R=0;     *G=0;     *B=65535;           //blue again
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Sunset2d(GtkWidget *W,gpointer Data)
{
gint i;
glong R,G,B;

Theme2d=0;
Colour2D[0].pixel=0; Colour2D[0].red=0xFFFF; Colour2D[0].green=0xBBBB; Colour2D[0].blue=0x8888; //Background
Colour2D[1].pixel=0; Colour2D[1].red=0xFFFF; Colour2D[1].green=0x0000; Colour2D[1].blue=0x0000; //Axis
Colour2D[2].pixel=0; Colour2D[2].red=0xFFFF; Colour2D[2].green=0x0000; Colour2D[2].blue=0x0000; //Title
Colour2D[3].pixel=0; Colour2D[3].red=0xAAAA; Colour2D[3].green=0x5353; Colour2D[3].blue=0x4747; //Box
Colour2D[4].pixel=0; Colour2D[4].red=0x0000; Colour2D[4].green=0x0000; Colour2D[4].blue=0x0000; //Permanent Box
Colour2D[5].pixel=0; Colour2D[5].red=0xAAAA; Colour2D[5].green=0x5353; Colour2D[5].blue=0x4747; //Banana

for (i=0;i<32;++i)
    { 
    ComputeRGB(i,&R,&G,&B); 
    if ( (i==14) || (i==29) ) R=G=B=0;                                            //Replace yellow and variations by black
    D2PenColours[i][0]=R; D2PenColours[i][1]=G; D2PenColours[i][2]=B; 
    }
ThemeChanged=TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Daylight2d(GtkWidget *W,gpointer Data)
{
gint i;
glong R,G,B;

Theme2d=1;
Colour2D[0].pixel=0; Colour2D[0].red=0xFFFF; Colour2D[0].green=0xFFFF; Colour2D[0].blue=0xAAAA; //Background
Colour2D[1].pixel=0; Colour2D[1].red=0x0000; Colour2D[1].green=0x0000; Colour2D[1].blue=0xFFFF; //Axis
Colour2D[2].pixel=0; Colour2D[2].red=0xFFFF; Colour2D[2].green=0x0000; Colour2D[2].blue=0x0000; //Title
Colour2D[3].pixel=0; Colour2D[3].red=0x0000; Colour2D[3].green=0xAAAA; Colour2D[3].blue=0xAAAA; //Box
Colour2D[4].pixel=0; Colour2D[4].red=0x7777; Colour2D[4].green=0x0000; Colour2D[4].blue=0xAAAA; //Permanent Box
Colour2D[5].pixel=0; Colour2D[5].red=0x0000; Colour2D[5].green=0xAAAA; Colour2D[5].blue=0xAAAA; //Banana

for (i=0;i<32;++i) { ComputeRGB(i,&R,&G,&B); D2PenColours[i][0]=R; D2PenColours[i][1]=G; D2PenColours[i][2]=B; }

ThemeChanged=TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Midnight2d(GtkWidget *W,gpointer Data)
{
gint i;
glong R,G,B;

Theme2d=3;
Colour2D[0].pixel=0; Colour2D[0].red=0xAAAA; Colour2D[0].green=0xAAAA; Colour2D[0].blue=0xAAAA; //Background
Colour2D[1].pixel=0; Colour2D[1].red=0x0000; Colour2D[1].green=0x0000; Colour2D[1].blue=0x0000; //Axis
Colour2D[2].pixel=0; Colour2D[2].red=0x8888; Colour2D[2].green=0x0000; Colour2D[2].blue=0x0000; //Title
Colour2D[3].pixel=0; Colour2D[3].red=0xFFFF; Colour2D[3].green=0xFFFF; Colour2D[3].blue=0xFFFF; //Box
Colour2D[4].pixel=0; Colour2D[4].red=0x0000; Colour2D[4].green=0x0000; Colour2D[4].blue=0xFFFF; //Permanent Box
Colour2D[5].pixel=0; Colour2D[5].red=0xFFFF; Colour2D[5].green=0xFFFF; Colour2D[5].blue=0xFFFF; //Banana

for (i=0;i<32;++i) { ComputeRGB(i,&R,&G,&B); D2PenColours[i][0]=R; D2PenColours[i][1]=G; D2PenColours[i][2]=B; }

ThemeChanged=TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Plain2d(GtkWidget *W,gpointer Data)
{
gint i;
glong R,G,B;

Theme2d=2;
Colour2D[0].pixel=0; Colour2D[0].red=0xFFFF; Colour2D[0].green=0xFFFF; Colour2D[0].blue=0xFFFF; //Background
Colour2D[1].pixel=0; Colour2D[1].red=0xFFFF; Colour2D[1].green=0x0000; Colour2D[1].blue=0x0000; //Axis
Colour2D[2].pixel=0; Colour2D[2].red=0x0000; Colour2D[2].green=0x0000; Colour2D[2].blue=0xFFFF; //Title
Colour2D[3].pixel=0; Colour2D[3].red=0x0000; Colour2D[3].green=0x8888; Colour2D[3].blue=0x0000; //Box
Colour2D[4].pixel=0; Colour2D[4].red=0x0000; Colour2D[4].green=0x0000; Colour2D[4].blue=0xFFFF; //Permanent Box
Colour2D[5].pixel=0; Colour2D[5].red=0xFFFF; Colour2D[5].green=0x0000; Colour2D[5].blue=0x0000; //Banana

for (i=0;i<32;++i) { ComputeRGB(i,&R,&G,&B); D2PenColours[i][0]=R; D2PenColours[i][1]=G; D2PenColours[i][2]=B; }

ThemeChanged=TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
