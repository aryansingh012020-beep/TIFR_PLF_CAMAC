#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "lamps.h"

/*External Global variables*/
extern GdkFont *Font;
extern GdkColor ColourOv[MAX_OVERLAP];                                                             //Colours for overlap
extern GdkColor ColourMB[7];                                                         //Colours for multiple banana gates
extern struct Setup Setup;
extern struct PsBGated PsBGated[MAX_PSEUDO];                                                  //Banana gated pseudo type
extern gint AreaWinOpen,OvWinOpen,FitWinOpen,CalibWinOpen,BananaWinOpen,BananaDrawMode,BananaShowing,BananaWinNo,
       BananaLineType,BananaMulti,PkSearchOpen,ScalerWinOpen;
extern gint ThemeChanged;
extern struct FitType Fit;                                                                           //The Fit parameters
extern struct Calibration Calibration[MAX_TOTAL_PAR];                                     //Calibration of each parameter
extern struct BGate BGate;                                                                            //BGate held in RAM
extern enum AcqSignal AcqSignal;                                                                 //Stop, Pause and Resume
extern gchar SStop[40];                                              //The stop time of time of this, or the previous run
extern gboolean BatchRunning;                                                               //Status of batch acquisition
extern gboolean CommonZoom,RealScroll,RealScroll2;                             //For common zoom and scroll of 1d spectra
extern struct BGate MultiBGate[7];                                        //We allow 7 extra banana gates to be displayed
extern gchar MultiBGateNames[7][LONG_TEXT_FIELD];                                 //File names for the extra banana gates
extern gboolean MultiBGateFlags[7];                                                   //Flags to indicate bgate is active
extern gfloat MacroCurr[MAX_MACRO],MacroPrev[MAX_MACRO],MacroDiff[MAX_MACRO];                 //Computed values of macros
extern gboolean LogOutBox,MacroOutBox;                                                    //To prevent multiple instances
extern gchar SetupDir[MAX_DIR_STRLEN],ListFDir[MAX_DIR_STRLEN],BanDir[MAX_DIR_STRLEN],SpecDir[MAX_DIR_STRLEN],
       BatDir[MAX_DIR_STRLEN],CalDir[MAX_DIR_STRLEN],LogDir[MAX_DIR_STRLEN],MacroDir[MAX_DIR_STRLEN];         //Dir prefs
extern gint Theme1d,Theme2d;                                                              //Theme settings. -1 for random

/*Function Templates*/
void Rose1d(GtkWidget *W,gpointer Data); void Sky1d(GtkWidget *W,gpointer Data); 
void Cucumber1d(GtkWidget *W,gpointer Data);
void Sandy1d(GtkWidget *W,gpointer Data); void Plain1d(GtkWidget *W,gpointer Data);
void Sunset2d(GtkWidget *W,gpointer Data); void Daylight2d(GtkWidget *W,gpointer Data); 
void Midnight2d(GtkWidget *W,gpointer Data);
void Plain2d(GtkWidget *W,gpointer Data);
void CheckSetup(void); void ZeroOned(gint SNo); void ZeroTwod(gint SNo);
void InitializeColours(void);
gint Read(gchar *ErrMessg,gint Opt);
void ReadCalibrationFile(gchar *FName,gboolean DspErr);
void LoadBatchFile(gchar *FName);
void CreateDir(void);
//----------------------------------------------------------------------------------------------------------------------
void InitializePrefs(void)
{
FILE *Fp;
gchar Skip[30],PrefDir[MAX_FNAME_LENGTH];

DefaultPrefs(); CreateDir();
if ((Fp=fopen(".lamps_prefs","r")))
   {
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(SetupDir,PrefDir);
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(ListFDir,PrefDir);
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(BanDir,PrefDir);
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(SpecDir,PrefDir);
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(BatDir,PrefDir);
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(CalDir,PrefDir);
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(LogDir,PrefDir);
   if (fscanf(Fp,"%s %s\n",Skip,PrefDir) == 2) strcpy(MacroDir,PrefDir);
   if (fscanf(Fp,"%s %d\n",Skip,&Theme1d) != 2) Theme1d=-1;                 //-1 for Random theme, see InitializeColours
   if (fscanf(Fp,"%s %d\n",Skip,&Theme2d) != 2) Theme2d=-1;                 //-1 for Random theme, see InitializeColours
   fclose(Fp);
   }
else Prefs(NULL,NULL);
CreateDir();                                                                                        //Create directories
InitializeColours();
}
//----------------------------------------------------------------------------------------------------------------------
void InitializeSearchedPeaks(void)
{
gint i,j;

for (j=0;j<MAX_1D;j++) for (i=0;i<MAX_PEAKS;i++) Setup.Oned.Peaks[i][j]=0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitializeSpecFileNames(void)
{
gint i;

for (i=0;i<MAX_1D;i++) strcpy(Setup.Files.Oned[i],""); 
for (i=0;i<MAX_2D;i++) strcpy(Setup.Files.Twod[i],""); 
//The code that reads file names from lamps.lst has been moved to ReadOneByOne() in iospec.c
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitializeColours(void)
{
gint Index,R,Hue,Intensity,Red,Green,Blue,C;
gchar Str[12];

if (Theme1d!=-1) R=CLAMP(Theme1d,0,4);
else             R=time(NULL) % 5;                                                 //Use current time to select 1d theme
switch (R)
   {
   case 0: Rose1d(NULL,NULL);     break;
   case 1: Sky1d(NULL,NULL);      break;
   case 2: Cucumber1d(NULL,NULL); break;
   case 3: Plain1d(NULL,NULL);    break;
   case 4: default: Sandy1d(NULL,NULL);
   }
if (Theme2d!=-1) R=CLAMP(Theme2d,0,3);
else             R=time(NULL) % 4;                                                 //Use current time to select 2d theme
switch (R)
   {
   case 0: Sunset2d(NULL,NULL);   break;
   case 1: Daylight2d(NULL,NULL); break;
   case 2: Plain2d(NULL,NULL);    break;
   case 3: default: Midnight2d(NULL,NULL);
   }

for (C=0;C<MAX_OVERLAP;++C)                          //Set colours for 1d overlap. Works for all values of MAX_OVERLAP
    {
    Hue=C%7; Intensity=255.0-230.0*(gfloat)C/(gfloat)MAX_OVERLAP;
    switch (Hue)
       {
       case 0: Red=Green=Blue=255-Intensity; break;     //Black
       case 1: Red=Green=0; Blue=Intensity;  break;     //Blue
       case 2: Red=Blue=0; Green=Intensity;  break;     //Green
       case 3: Red=0; Green=Blue=Intensity;  break;     //Cyan
       case 4: Green=Blue=0; Red=Intensity;  break;     //Red
       case 5: Red=Blue=Intensity; Green=0;  break;     //Magenta
       case 6: Red=Green=Intensity; Blue=0;             //Brown
       }
    ColourOv[C].pixel=0; ColourOv[C].red=Red<<8; ColourOv[C].green=Green<<8; ColourOv[C].blue=Blue<<8;
    }

ColourMB[0].pixel=0; ColourMB[0].red=0xFF00; ColourMB[0].green=0x0000; ColourMB[0].blue=0x0000;
ColourMB[1].pixel=0; ColourMB[1].red=0x0000; ColourMB[1].green=0x7700; ColourMB[1].blue=0x0000;
ColourMB[2].pixel=0; ColourMB[2].red=0x0000; ColourMB[2].green=0x0000; ColourMB[2].blue=0x8800;
ColourMB[3].pixel=0; ColourMB[3].red=0xAA00; ColourMB[3].green=0x5500; ColourMB[3].blue=0x0000;
ColourMB[4].pixel=0; ColourMB[4].red=0x8800; ColourMB[4].green=0x0000; ColourMB[4].blue=0x8800;
ColourMB[5].pixel=0; ColourMB[5].red=0x0000; ColourMB[5].green=0x8800; ColourMB[5].blue=0x8800;
ColourMB[6].pixel=0; ColourMB[6].red=0x3300; ColourMB[6].green=0x3300; ColourMB[6].blue=0x3300;
}
/*--------------------------------------------------------------------------------------------------------------------*/
void InitializeFont(void)
{
if ( (Font=gdk_font_load("-misc-fixed-*-*-*--*-100-*-*-*-*-*-*")) == NULL)
    { g_print("Fatal error: Could not load font...exiting...\n"); exit(1); }
}
/*---------------------------------------------------------------------------------------------------------------------*/
void DefaultSetup(void)                                                         //Makes a default setup with safe values
{
gint i,j,GateLim;
FILE *Fp;
gchar ErrMessg[200];

if ((Fp=fopen("exogam.list","r"))) { GateLim=16383; fclose(Fp); } else GateLim=8191;   //For Exogam, GateHi limits=16383 

Setup.Parameter.IgnoreCamac=FALSE;
strcpy(Setup.FName,""); Setup.Modified=FALSE;                                                   //No file name initially

Setup.Offline.NFiles=0; Setup.Offline.BufRefresh=100; Setup.Offline.Delay=0;                    //Offline setup defaults
for (i=0;i<MAX_OFFLINE_FILES;++i)
    {
    Setup.Offline.Zero[i]=FALSE; strcpy(Setup.Offline.ListFName[i],""); strcpy(Setup.Offline.SetFName[i],"Current"); 
    Setup.Offline.SaveSpec[i]=FALSE; strcpy(Setup.Offline.SpecFName[i],"None"); Setup.Offline.BufSkip[i]=0; 
    strcpy(Setup.Offline.BufRead[i],"All");
    }
Setup.Offline.Zero[0]=TRUE;

Setup.ReWrite.NFiles=0; Setup.ReWrite.OutFormat='N';                                      //ReWrite files setup defaults
for (i=0;i<MAX_TOTAL_PAR;++i) Setup.ReWrite.Select[i]=FALSE;
for (i=0;i<MAX_REWRITE_FILES;++i)
    { 
    strcpy(Setup.ReWrite.ListFName[i],""); Setup.ReWrite.BufSkip[i]=0; strcpy(Setup.ReWrite.BufRead[i],"All"); 
    strcpy(Setup.ReWrite.OutFName[i],"myfile.zls");
    }

Setup.BatAcq.NBat=7;                                                                           //BatchAcq setup defaults
for (i=0;i<MAX_BAT;++i)
    {
    sprintf(Setup.BatAcq.RunName[i],"run%03d",i+1); 
    strcpy(Setup.BatAcq.Start[i],"00:00:00");
    strcpy(Setup.BatAcq.Stop[i],"Duration 00:01:00");
    strcpy(Setup.BatAcq.SetFName[i],"Current");
    }

Setup.ListMode.ListOn=0; Setup.ListMode.Compr=1; Setup.ListMode.Rate=2; 
Setup.ListMode.NoOfLGates=0;                                                                  //ListMode setup defaults
for (i=0;i<MAX_LGATES;i++)                                                      //Gated List Mode-LGates default values
    { Setup.ListMode.LGates[i].Para=1; Setup.ListMode.LGates[i].Lo=0; Setup.ListMode.LGates[i].Hi=8192; }
Setup.Hardware.NCrates=1;                                                                        //One crate by default 
Setup.Hardware.CamacMode=0;                                            //Normal Mode (not Q-Stop Block Mode) by default
Setup.Hardware.UseGtSup=FALSE; Setup.Hardware.GtSupStn=23;                           //Defaults for CM90 gate supressor
Setup.Hardware.AutoStopOn=FALSE; Setup.Hardware.AutoStop=60;                                              //No AutoStop

for (i=0;i<2*MAX_CAMAC_STNS;i++)                                         //First initialise modules etc at all stations
    {
    Setup.Hardware.Modules[i]=0; Setup.Hardware.Properties[i].AdcLam=Disabled;
    Setup.Hardware.Properties[i].AdcMode=NotRelevant;
    Setup.Hardware.Properties[i].AdcLLD=50;                         //A safe default for both CM60 and AD413 needed here
    for (j=0;j<8;++j) Setup.Hardware.Properties[i].SilenaOffset[j]=127;                     //Used only by Silena 4418/Q
    for (j=0;j<16;++j) Setup.Hardware.Properties[i].LowerThreshold[j]=1;      //Used for Silena 4418/Q and Phillips 71xx
    for (j=0;j<16;++j) Setup.Hardware.Properties[i].UpperThreshold[j]=4094;   //Used for Silena 4418/Q and Phillips 71xx
    Setup.Hardware.Properties[i].AdcFCode=0;                                        //FCode=0 is common for many modules
    Setup.Hardware.Properties[i].AdcGain=8192;                                                        //Default ADC gain
    strcpy(Setup.Hardware.Paras[i],"");                                                                  //Empty strings
    strcpy(Setup.Hardware.ParaNames[i],"");                                                       //Default names: blank
    Setup.Hardware.ZSupLLD[i]=1; Setup.Hardware.ZSupULD[i]=8190;                            //Safe defaults for all ADCs
    }
Setup.Hardware.Modules[9]=Setup.Hardware.Modules[12]=1;                              //Ortec AD413s at Stn 10,13 Crate 1
Setup.Hardware.Properties[9].AdcLam=Enabled; Setup.Hardware.Properties[12].AdcLam=Enabled;         //LAM enabled on both
Setup.Hardware.Properties[9].AdcMode=Singles; Setup.Hardware.Properties[12].AdcMode=Singles;//Singles mode for both ADCs
Setup.Hardware.Properties[9].AdcLLD=50; Setup.Hardware.Properties[12].AdcLLD=50;    //This will give 100mV for all slots
Setup.Hardware.Properties[9].AdcFCode=2; Setup.Hardware.Properties[12].AdcFCode=2;     //Function Code=2 for Ortec AD413
Setup.Hardware.Properties[9].AdcGain=8192; Setup.Hardware.Properties[12].AdcGain=8192;                       //ADC Gains
strcpy(Setup.Hardware.Paras[9],"1-4"); strcpy(Setup.Hardware.SubAddr[ 9],"0-3");         //Params 1-4 at Crate 1, Stn 10
strcpy(Setup.Hardware.Paras[12],"5-8"); strcpy(Setup.Hardware.SubAddr[12],"0-3");        //Params 5-8 at Crate 2, Stn 10
strcpy(Setup.Hardware.ParaNames[9],"Para01-04"); strcpy(Setup.Hardware.ParaNames[12],"Para05-08");     //Parameter names
Setup.Hardware.ZSupLLD[9]=1; Setup.Hardware.ZSupLLD[12]=1;                     //LLD values for Para01-04 and Paras05-08
Setup.Hardware.ZSupULD[9]=8190; Setup.Hardware.ZSupULD[12]=8190;               //ULD values for Para01-04 and Paras05-08
Setup.Oned.N=4; Setup.Twod.N=1; Setup.Oned.WSz=2; Setup.Twod.WSz=1;
Setup.Spectra.Safety=1; Setup.Spectra.SafetyTime=1;                                        //Defaults for Spectra.Safety
for (i=0;i<MAX_1D;++i)                                                 //Initialize settings for all possible 1d spectra
    { 
    sprintf(Setup.Oned.Name[i],"Oned_%d",i+1);
    Setup.Oned.Par[i]=i+1; Setup.Oned.Chan[i]=1024;
    Setup.Oned.Gate1[i].NGates=0; Setup.Oned.Gate2[i].NGates=0;
    Setup.Oned.Gate1[i].Cond=And; Setup.Oned.Gate2[i].Cond=And;
    Setup.Oned.NPar[i]=1;                                                                 //Default: not Vector spectrum
    for (j=0;j<MAX_GATES_1D;j++)
        {
        Setup.Oned.Gate1[i].Gate1d[j].Para=1; Setup.Oned.Gate1[i].Gate1d[j].Lo=0; 
        Setup.Oned.Gate1[i].Gate1d[j].Hi=GateLim;
        strcpy(Setup.Oned.Gate2[i].Gate2d[j],"");
        } 
    }
for (i=0;i<MAX_2D;i++)                                                 //Initialize settings for all possible 2d spectra
    { 
    sprintf(Setup.Twod.Name[i],"Twod_%d",i+1);
    Setup.Twod.XPar[i]=1; Setup.Twod.YPar[i]=2; Setup.Twod.XChan[i]=512; Setup.Twod.YChan[i]=512;
    Setup.Twod.Gate1[i].NGates=0; Setup.Twod.Gate2[i].NGates=0;
    Setup.Twod.Gate1[i].Cond=And; Setup.Twod.Gate2[i].Cond=And;
    Setup.Twod.NXPar[i]=1; Setup.Twod.NYPar[i]=1;                                  //Default: not X-Vector, not Y-Vector
    for (j=0;j<MAX_GATES_2D;j++)
        {
        Setup.Twod.Gate1[i].Gate1d[j].Para=1; Setup.Twod.Gate1[i].Gate1d[j].Lo=0; Setup.Twod.Gate1[i].Gate1d[j].Hi=GateLim;
        strcpy(Setup.Twod.Gate2[i].Gate2d[j],"");
        } 
    }
Setup.Pseudo.N=0;                                                                   //Defaults for PseudoParameter Setup
for (i=0;i<MAX_PSEUDO;++i)
    {
    sprintf(Setup.Pseudo.Name[i],"Pseudo_%d",i+1);
    Setup.Pseudo.P1[i]=1; Setup.Pseudo.P2[i]=1; Setup.Pseudo.Size[i]=8192; Setup.Pseudo.Type[i]=Sum;
    Setup.Pseudo.K1[i]=1.0; Setup.Pseudo.K2[i]=1.0; Setup.Pseudo.K3[i]=1.0;
    Setup.Pseudo.O1[i]=0.0; Setup.Pseudo.O2[i]=0.0; Setup.Pseudo.O3[i]=0.0;
    Setup.Pseudo.Power[i]=1.65; Setup.Pseudo.L1[i]=1; Setup.Pseudo.L2[i]=1;
    Setup.Pseudo.ArrayN[i]=4; 
    for (j=0;j<MAX_ARRAY;++j)
        {
        Setup.Pseudo.ArrayPar[j][i]=1; Setup.Pseudo.ArrayLLD[j][i]=0.0; Setup.Pseudo.ArrayOffset[j][i]=0.0;
        Setup.Pseudo.ArraySlope[j][i]=1.0; Setup.Pseudo.ArrayQuad[j][i]=0.0;
        }
    strcpy(PsBGated[i].Name,"");                  //Initialize name of banana gate file for all the pseudo parameters
    }
Setup.Parameter.NPar=0;                                         //Allow CheckSetup to compute this from Setup.Hardware
for (i=0;i<MAX_PAR;++i)
    {
    Setup.Parameter.N[i]=10; Setup.Parameter.A[i]=0; Setup.Parameter.F[i]=0; 
    Setup.Parameter.Chan[i]=8192; Setup.Parameter.LLD[i]=1; Setup.Parameter.ULD[i]=8191;
    sprintf(Setup.Parameter.Name[i],"Para %d",i+1);
    }
    
Setup.Scaler.NSc=0; Setup.Scaler.NSc1=0;
for (i=0;i<MAX_SCALER;++i)
    { 
    sprintf(Setup.Scaler.Name[i],"Scaler%02d",i+1); 
    Setup.Scaler.C[i]=1; Setup.Scaler.N[i]=1; Setup.Scaler.A[i]=i; Setup.Scaler.F[i]=0; 
    }

Setup.PLogSetup.On=TRUE; Setup.PLogSetup.BufCount=100;                            //Defaults for periodic log settings

Setup.Macro.N=0;                                                                                  //Defaults for Macro
Setup.Macro.RefreshRate=100;
LogOutBox=MacroOutBox=FALSE;
for (i=0;i<MAX_MACRO;++i)
    {
    sprintf(Setup.Macro.Name[i],"Macro%02d",i+1); Setup.Macro.Type[i]=MacroArea;
    Setup.Macro.Display[i]=TRUE; Setup.Macro.SpecNo[i]=1; Setup.Macro.XMin[i]=10; Setup.Macro.XMax[i]=100;
    Setup.Macro.YMin[i]=10; Setup.Macro.YMax[i]=100; Setup.Macro.K1[i]=1.0; Setup.Macro.K2[i]=1.0; 
    Setup.Macro.ScalerNo[i]=1; Setup.Macro.Index1[i]=1; Setup.Macro.Index2[i]=1;
    MacroCurr[i]=MacroPrev[i]=MacroDiff[i]=0.0;
    }
Read(ErrMessg,1);                                                                           //Read .lamps_set if it exists
CheckSetup(); ZeroOned(-1); ZeroTwod(-1);
ReadCalibrationFile(".lamps_cal",FALSE);             //Was saved automatically whenever user did read or edit calibration
LoadBatchFile(".lamps_bat");                                       //Was saved automatically whenever user began analysis
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitializeFlags(void)
{
gint i;

AreaWinOpen=FALSE; OvWinOpen=FALSE; FitWinOpen=FALSE; CalibWinOpen=FALSE; BananaWinOpen=FALSE; BananaDrawMode=FALSE;
BananaShowing=FALSE; BananaWinNo=0; BananaMulti=FALSE;
ThemeChanged=TRUE; Setup.Spectra.NoZero=FALSE; AcqSignal=Stop; PkSearchOpen=FALSE;
BananaLineType=0; ScalerWinOpen=0; BatchRunning=FALSE;
strcpy(SStop,"Stop:        ");
for (i=0;i<7;++i) MultiBGateFlags[i]=FALSE;                                           //Flags to indicate bgate is active
CommonZoom=FALSE; RealScroll=TRUE; RealScroll2=TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitializeFitParas(void)
{
gint i;

Fit.NPts=0;
Fit.P[0]=0.0; Fit.F[0]=Variable;                                                                         //Left Background
Fit.P[1]=0.0; Fit.F[1]=Variable;                                                                        //Right Background
Fit.P[2]=0.0; Fit.F[2]=Fixed;                                                                            //Quad Background
Fit.TailOpt=None; Fit.ShapeOpt=Same;                                                      //Default Tail and Shape Options
Fit.StatsOpt=Normal;
Fit.NPeaks=0; 
for (i=0;i<MAX_PEAKS;i++) 
    {
    Fit.P[6*i+3]=0.0; Fit.F[6*i+3]=Variable;                                                               //Peak position
    Fit.P[6*i+4]=0.0; if (i==0) Fit.F[6*i+4]=Variable; else Fit.F[6*i+4]=Constrained;                               //FWHM
    Fit.P[6*i+5]=0.0; Fit.F[6*i+5]=NotApplicable;                                                              //Asymmetry
    Fit.P[6*i+6]=0.0; Fit.F[6*i+6]=NotApplicable;                                                                  //LTail
    Fit.P[6*i+7]=0.0; Fit.F[6*i+7]=NotApplicable;                                                                  //RTail
    Fit.P[6*i+8]=0.0; Fit.F[6*i+8]=Variable;                                                                      //Height
    }
Fit.MouseContext=AddPeaks;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitializeCalibParas(void)
{
gint i;

for (i=0;i<MAX_TOTAL_PAR;i++)
    {
    Calibration[i].P[0]=0.0; Calibration[i].P[1]=1.0; Calibration[i].P[2]=0.0; Calibration[i].P[3]=0.0;
    strcpy(Calibration[i].Units,"KeV");
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitializeBGate(void)                                                              //Make default banana gates in RAM
{
gint i;

BGate.XPar=BGate.YPar=1; BGate.N=3;
BGate.X[0]=3000; BGate.Y[0]=3000; BGate.X[1]=4000; BGate.Y[1]=2000; BGate.X[2]=3000; BGate.Y[2]=1000;
BGate.XMin=3000; BGate.XMax=4000; BGate.YMin=1000; BGate.YMax=3000;

for (i=0;i<7;++i)
    {
    strcpy(MultiBGateNames[i],"Browse");
    MultiBGate[i].XPar=MultiBGate[i].YPar=1; MultiBGate[i].N=4;
    MultiBGate[i].X[0]=2000+200*i; MultiBGate[i].Y[0]=7000; MultiBGate[i].X[1]=4400+200*i; MultiBGate[i].Y[1]=5000; 
    MultiBGate[i].X[2]=2500+200*i; MultiBGate[i].Y[2]=800; MultiBGate[i].X[3]=700+200*i; MultiBGate[i].Y[3]=4200;
    MultiBGate[i].XMin=700; MultiBGate[i].XMax=4400+200*i; MultiBGate[i].YMin=800; MultiBGate[i].YMax=7000;
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/

