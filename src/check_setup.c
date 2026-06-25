#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "lamps.h"

/* External Globals*/
extern enum ProgramState ProgramState;
extern struct Setup Setup;
extern struct PsBGated PsBGated[MAX_PSEUDO];                                                  //Banana gated pseudo type
extern guint16 *Oned16,*Twod16;
extern guint32 *Oned32,*Twod32,*Proj;
extern gint Off1d[MAX_1D],Off2d[MAX_2D],OffProj[MAX_2D];
extern gint ScreenSpecNo[SCREEN_TOT][MAX_SCREENS],ScreenSpecType[SCREEN_TOT][MAX_SCREENS];
       /*Info: each spectrum window of each screen*/
extern struct WinProperties Prop[SCREEN_TOT];
extern gchar SetupDir[MAX_DIR_STRLEN];                                                                  //Directory prefs

/*Globals in this file only*/
GtkWidget *ParaListW;
gboolean ParaListVisible=FALSE;

/*Function Templates*/
void ParaError(gint Stn); void SubAddrError(gint Stn); void NameError(gint Stn);
void ParsePar(gint Stn,gchar *LoPar,gchar *HiPar,gint NoHi,gint *LoP,gint *HiP);
void ParseSub(gint Stn,gchar *LoSub,gchar *HiSub,gint NoHiA,gint LoP,gint HiP);
void ParseName(gint Stn,gchar *LoName,gchar *HiName,gint NoHiN,gint LoP,gint HiP);
void CheckSetup(void); void ParameterSetup(void); 
void MakeIniClr1(gchar *FName); void MakeIniClr2(gchar *FName); 
void ParameterList(GtkWidget *W,gpointer Data); void ListModeSettings(GtkWidget *W,gpointer Data);
void OnedSpecSettings(GtkWidget *W,gpointer Data); 
void TwodSpecSettings(GtkWidget *W,gpointer Data); 
void PseudoSettings(GtkWidget *W,gpointer Data);
void ScalerSettings(GtkWidget *W,gpointer Data);
void AllocateMemory(void);
void AllocateScreenWins(void); void Test1dMem16(void); void Test1dMem32(void);
void Attention(gint XPos,gchar *Messg);
void CloseAllSpecWindows(GtkWidget *W,gpointer Unused);
void WinClosed(GtkWidget *W,GtkWidget *Win);
void FindBounds(gint *U,gint N,gint *Lo,gint *Hi);
gboolean ReadBananaFile(gchar *FName,struct BGate *BGate);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void Save(gint Opt);
/*---------------------------------------------------------------------------------------------------------------------*/
void Test1dMem16(void)
{
gint i,j,k;

g_print("Testing 1d Mem 16-bit\n");
for (i=0;i<Setup.Oned.N;++i)
    {
    g_print("Testing Spec# %d Offset=%d\n",i,Off1d[i]);
    for (j=0;j<Setup.Oned.Chan[i];j++)
        {
        k=Off1d[i]+j; Oned16[k]=k; 
        if (!(k%500)) g_print("Oned16[%d]=%d\n",k,Oned16[k]);
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Test1dMem32(void)
{
gint i,j,k;

g_print("Testing 1d Mem 32-bit\n");
for (i=0;i<Setup.Oned.N;++i)
    {
    g_print("Testing Spec# %d Offset=%d\n",i,Off1d[i]);
    for (j=0;j<Setup.Oned.Chan[i];j++)
        {
        k=Off1d[i]+j; Oned32[k]=k; 
        if (!(k%500)) g_print("Oned32[%d]=%d\n",k,Oned32[k]);
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void AllocateMemory(void)                                   //Allocate memory for 1d and 2d spectra
{
gint i;
size_t Mem1,Mem2,MemProj;

for (i=0,Mem1=0;i<Setup.Oned.N;++i) Mem1+=Setup.Oned.Chan[i];
for (i=0,Mem2=0;i<Setup.Twod.N;++i) Mem2+=Setup.Twod.XChan[i]*Setup.Twod.YChan[i];
if (Setup.Twod.WSz*Mem2 == G_MAXLONG) { Attention(0,"Memory size for 2D exceeded!"); ProgramState=Invalid; return; }
if (Setup.Oned.WSz == 1)
   {
   Oned32=g_renew(guint32,Oned32,0);                               //Free memory for 32-bit spectra
   Oned16=g_renew(guint16,Oned16,Mem1);
   if (Setup.Oned.N && (Oned16 == NULL)) 
      { Attention(0,"Not enough memory for 1d Spectra"); ProgramState=Invalid; }
   }
else
   {
   Oned16=g_renew(guint16,Oned16,0);                              //Free memory for 16-bit spectra
   Oned32=g_renew(guint32,Oned32,Mem1);
   if (Setup.Oned.N && (Oned32 == NULL))
      { Attention(0,"Not enough memory for 1d Spectra"); ProgramState=Invalid; }
   }
Off1d[0]=0; for (i=1;i<Setup.Oned.N;++i) Off1d[i]=Off1d[i-1]+Setup.Oned.Chan[i-1];
if (Setup.Twod.WSz == 1)
   {
   Twod32=g_renew(guint32,Twod32,0);                               //Free memory for 32-bit spectra
   Twod16=g_renew(guint16,Twod16,Mem2);
   if (Setup.Twod.N && (Twod16 == NULL)) 
      { Attention(0,"Not enough memory for 2d Spectra"); ProgramState=Invalid; }
   }
else
   {
   Twod16=g_renew(guint16,Twod16,0);                               //Free memory for 16-bit spectra
   Twod32=g_renew(guint32,Twod32,Mem2);
   if (Setup.Twod.N && (Twod32 == NULL)) 
      { Attention(0,"Not enough memory for 2d Spectra"); ProgramState=Invalid; }
   }
Off2d[0]=0; 
for (i=1;i<Setup.Twod.N;++i) Off2d[i]=Off2d[i-1]+Setup.Twod.XChan[i-1]*Setup.Twod.YChan[i-1];

/* Allocate memory for X and Y projections */
for (i=0,MemProj=0;i<Setup.Twod.N;++i) MemProj+=MAX(Setup.Twod.XChan[i],Setup.Twod.YChan[i]);

Proj=g_renew(guint32,Proj,MemProj);
if (Setup.Twod.N && (Proj == NULL)) 
   { Attention(0,"Not enough memory for projections"); ProgramState=Invalid; }
OffProj[0]=0; 
for (i=1;i<Setup.Twod.N;++i) 
    OffProj[i]=OffProj[i-1]+MAX(Setup.Twod.XChan[i-1],Setup.Twod.YChan[i-1]);

//if (Setup.Oned.WSz == 1) Test1dMem16(); else Test1dMem32();
}
//----------------------------------------------------------------------------------------------------------------------
gint Msb(gint N)                           //Find the position of the most significan bit of N (namely log(N) to base 2)
{
gint i;

for (i=0;i<16;++i) { N>>=1; if (!N) return i; }                                         //It will work only upto 16 bits
return 0;
}
//----------------------------------------------------------------------------------------------------------------------
void CalculateBitShifts()                                     //The bit shifts required while building 1d and 2d spectra
{
gint i,j;
gchar Str[120];
gboolean ErrMes;

for (i=0,ErrMes=FALSE;i<Setup.Oned.N;++i)
    {
    if (Setup.Oned.Par[i]>0)
       Setup.Oned.BitShift[i]=Msb(Setup.Parameter.Chan[Setup.Oned.Par[i]-1])-Msb(Setup.Oned.Chan[i]);
    else if (Setup.Oned.NPar[i]>1) { sprintf(Str,"1d Spec %d Vector spectrum\nPara=0 illegal!",i+1); Attention(-100,Str); }
    if (Setup.Oned.NPar[i]>1)                                     //Multi-update 1d vector spectrum
       for (j=1;j<Setup.Oned.NPar[i];++j)
           {
           if ((Setup.Parameter.Chan[Setup.Oned.Par[i]-1+j]!=Setup.Parameter.Chan[Setup.Oned.Par[i]-1]) && !ErrMes)
              { ErrMes=TRUE; sprintf(Str,"1d Spec %d Vector spectrum\nDiffering para size illegal!",i+1); Attention(100,Str); }
           }
    }
for (i=0,ErrMes=FALSE;i<Setup.Twod.N;++i)
    {
    Setup.Twod.XBitShift[i]=Msb(Setup.Parameter.Chan[Setup.Twod.XPar[i]-1])-Msb(Setup.Twod.XChan[i]);
    if (Setup.Twod.NXPar[i]>1)                                                    //Multi-update 2d spectrum, vector X-Axis 
       for (j=1;j<Setup.Twod.NXPar[i];++j)
           {
           if ((Setup.Parameter.Chan[Setup.Twod.XPar[i]-1+j]!=Setup.Parameter.Chan[Setup.Twod.XPar[i]-1]) && !ErrMes)
              { ErrMes=TRUE; sprintf(Str,"2d Spec %d Vector X-axis\nDiffering para size illegal!",i+1); Attention(-100,Str); }
           }
    Setup.Twod.YBitShift[i]=Msb(Setup.Parameter.Chan[Setup.Twod.YPar[i]-1])-Msb(Setup.Twod.YChan[i]);
    if (Setup.Twod.NYPar[i]>1)                                                    //Multi-update 2d spectrum, vector Y-Axis 
       for (j=1;j<Setup.Twod.NYPar[i];++j)
           {
           if ((Setup.Parameter.Chan[Setup.Twod.YPar[i]-1+j]!=Setup.Parameter.Chan[Setup.Twod.YPar[i]-1]) && !ErrMes)
              { ErrMes=TRUE; sprintf(Str,"2d Spec %d Vector Y-axis\nDiffering para size illegal!",i+1); Attention(100,Str); }
           }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void AllocateScreenWins(void)  
/*First allocate all the 1D spectra then all the 2D spectra*/
{
gint i,j,Win,Screen;

/* First we initialise ScreenSpecNo and ScreenSpecType. 
 * Note: ScreenSpecType=0 for no spectrum, =1 for 1d spectrum, =2 for 2d spectrum, 
         =3 for XProj, =4 for YProj*/
for (Screen=0;Screen<MAX_SCREENS;Screen++) 
for (Win=0;Win<SCREEN_TOT;Win++) ScreenSpecNo[Win][Screen]=ScreenSpecType[Win][Screen]=0;
j=0;
for (i=0;i<Setup.Oned.N;++i)
    {
    Screen=j/SCREEN_TOT;  Win=j%SCREEN_TOT;
    ScreenSpecNo[Win][Screen]=i; ScreenSpecType[Win][Screen]=1;
    j++;
    }
for (i=0;i<Setup.Twod.N;++i)
    {
    Screen=j/SCREEN_TOT; Win=j%SCREEN_TOT;
    ScreenSpecNo[Win][Screen]=i; ScreenSpecType[Win][Screen]=2;
    j++;
    }
CloseAllSpecWindows(NULL,NULL);                            //If any windows are open, we close them
for (i=0;i<SCREEN_TOT;++i)                                 //Initial properties for all the windows
    { 
    Prop[i].Open=0; Prop[i].InUse=0;                               //Closed, not locked by activity
    for (j=0;j<MAX_OVERLAP;j++)                           //Initialise variables related to overlap
        { 
        Prop[i].One.OvSpec[j]=0; Prop[i].One.OvHShift[j]=0; 
        Prop[i].One.OvVShift[j]=1; Prop[i].One.Overlap[j]=FALSE; 
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CheckPseudos(void)
{
gint i,j;
gchar Str[120];

for (i=0;i<Setup.Pseudo.N;++i)
    {
    for (j=0;j<MAX_TEXT_FIELD;++j) 
        if (Setup.Pseudo.Name[i][j]==' ') Setup.Pseudo.Name[i][j]='_';      //Replace blanks by _
    Setup.Pseudo.P1[i]=CLAMP(Setup.Pseudo.P1[i],1,Setup.Parameter.NPar+Setup.Pseudo.N);
    Setup.Pseudo.P2[i]=CLAMP(Setup.Pseudo.P2[i],1,Setup.Parameter.NPar+Setup.Pseudo.N);
    if (Setup.Pseudo.Type[i]==Multiplicity) 
       { 
       Setup.Pseudo.L1[i]=CLAMP(Setup.Pseudo.L1[i],1,Setup.Parameter.NPar+Setup.Pseudo.N);
       Setup.Pseudo.L2[i]=CLAMP(Setup.Pseudo.L2[i],1,Setup.Parameter.NPar+Setup.Pseudo.N);
       }
    if (Setup.Pseudo.Type[i]==BGated)
       {
       if (!ReadBananaFile(PsBGated[i].Name,&PsBGated[i].BGate))
          {
          sprintf(Str,"Pseudo Para #%d invalid banana file\n%s",i+1,PsBGated[i].Name);
          Attention(-200+8*i,Str);
          }
       }
    Setup.Parameter.Chan[Setup.Parameter.NPar+i]=Setup.Pseudo.Size[i];
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
gboolean ReadBananaFile(gchar *FName,struct BGate *BGate)
{
FILE *Fp;
gchar Str[MAX_FNAME_LENGTH+80];
gint i,j;

if ((Fp=fopen(FName,"r")) == NULL) return FALSE;
for (i=0;i<MAX_BAN_POINTS;++i) BGate->X[i]=BGate->Y[i]=0;
fgets(Str,120,Fp);                                                               //Skip title line
fscanf(Fp,"%s %d",Str,&(BGate->XPar)); fscanf(Fp,"%s %d",Str,&(BGate->YPar)); 
fscanf(Fp,"%s %d",Str,&(BGate->N));
for (i=0;i<BGate->N;++i) fscanf(Fp,"%s %d %d %d",Str,&j,&(BGate->X[i]),&(BGate->Y[i]));
fclose(Fp);

BGate->XPar=CLAMP(BGate->XPar,1,MAX_PAR); BGate->YPar=CLAMP(BGate->YPar,1,MAX_PAR);
BGate->N=CLAMP(BGate->N,3,MAX_BAN_POINTS);
FindBounds(BGate->X,BGate->N,&(BGate->XMin),&(BGate->XMax));
FindBounds(BGate->Y,BGate->N,&(BGate->YMin),&(BGate->YMax));
return TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CheckOned(void)                             //Clamp Oned sizes, read and validate banana files
{
gint SNo,Gate,j;
gboolean Suppress;
gchar ShortName[25],Messg[120];

for (SNo=0,Suppress=FALSE;SNo<Setup.Oned.N;++SNo)
    {
    for (j=0;j<MAX_TEXT_FIELD;++j) 
        if (Setup.Oned.Name[SNo][j]==' ') Setup.Oned.Name[SNo][j]='_';                              //Replace blanks by _
    Setup.Oned.Par[SNo]=CLAMP(Setup.Oned.Par[SNo],0,Setup.Parameter.NPar+Setup.Pseudo.N);          //Par=0 is hit pattern
    if (Setup.Oned.Par[SNo]>0) 
         Setup.Oned.Chan[SNo]=CLAMP(Setup.Oned.Chan[SNo],16,
                                    Setup.Parameter.Chan[Setup.Oned.Par[SNo]-1]);
    else Setup.Oned.Chan[SNo]=CLAMP(Setup.Oned.Chan[SNo],16,1024);
    for (Gate=0;Gate<Setup.Oned.Gate2[SNo].NGates;++Gate)
        {
        if (!ReadBananaFile(Setup.Oned.Gate2[SNo].Gate2d[Gate],&Setup.Oned.Gate2[SNo].BGates[Gate])) 
           {
           if (!Suppress) 
              {
              AbbreviateFileName(ShortName,Setup.Oned.Gate2[SNo].Gate2d[Gate],18);
              sprintf(Messg,"1dSpec #%d Gate #%d failed:\n%s\nFurther messages suppressed",SNo,Gate,ShortName);
              Attention(-200,Messg);
              }
           Suppress=TRUE;
           }
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
gboolean CheckDCO(gint SNo)                                          //Radware .spn matrices must be 4K x 4K x Double Word
{
gint i,j,XPar,YPar;

if (Setup.Twod.XChan[SNo]!=4096)                  return FALSE; 
if (Setup.Twod.YChan[SNo]!=4096)                  return FALSE;
if (Setup.Twod.WSz!=2)                            return FALSE;

for (i=0;i<Setup.Twod.NXPar[SNo];++i)                                 //For DCO matrices, X and Y vectors must not overlap
    {
    XPar=Setup.Twod.XPar[SNo]+i-1;
    for (j=0;j<Setup.Twod.NYPar[SNo];++j)
        {
        YPar=Setup.Twod.YPar[SNo]+j-1;
        if (XPar==YPar) return FALSE;
        }
    }
Setup.Twod.MatrixType[SNo]=DCO; return TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
gboolean CheckGammaGamma(gint SNo)                                   //Radware .spn matrices must be 4K x 4K x Double Word
{
if (Setup.Twod.NXPar[SNo]!=Setup.Twod.NYPar[SNo]) return FALSE;//For GammaGamma matrices, X and Y vectors must be the same
if (Setup.Twod.XPar[SNo]!=Setup.Twod.YPar[SNo])   return FALSE;
if (Setup.Twod.XChan[SNo]!=4096)                  return FALSE;
if (Setup.Twod.YChan[SNo]!=4096)                  return FALSE;
if (Setup.Twod.WSz!=2)                            return FALSE;
Setup.Twod.MatrixType[SNo]=GammaGamma; return TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CheckTwod(void)                                                    //Clamp Twod sizes, read and validate banana files
{
gint SNo,Gate,j;
gboolean Suppress;
gchar ShortName[25],Messg[120];

for (SNo=0,Suppress=FALSE;SNo<Setup.Twod.N;++SNo)
    {
    for (j=0;j<MAX_TEXT_FIELD;++j) 
        if (Setup.Twod.Name[SNo][j]==' ') Setup.Twod.Name[SNo][j]='_';                              //Replace blanks by _
    Setup.Twod.XPar[SNo]=CLAMP(Setup.Twod.XPar[SNo],0,Setup.Parameter.NPar+Setup.Pseudo.N);        //Par=0 is hit pattern
    Setup.Twod.YPar[SNo]=CLAMP(Setup.Twod.YPar[SNo],0,Setup.Parameter.NPar+Setup.Pseudo.N);        //Par=0 is hit pattern
    Setup.Twod.XChan[SNo]=CLAMP(Setup.Twod.XChan[SNo],16,Setup.Parameter.Chan[Setup.Twod.XPar[SNo]-1]);
    Setup.Twod.YChan[SNo]=CLAMP(Setup.Twod.YChan[SNo],16,Setup.Parameter.Chan[Setup.Twod.YPar[SNo]-1]);
    Setup.Twod.MatrixType[SNo]=Other;
    if (Setup.Twod.NXPar[SNo]>1 || Setup.Twod.NYPar[SNo]>1)                              //Check GammaGamma, DCO or Other
       {
       if (CheckGammaGamma(SNo)) sprintf(Messg,"2dSpec #%d Gamma-Gamma Matrix will be saved as RADWARE .spn",SNo+1);
       else if (CheckDCO(SNo))   sprintf(Messg,"2dSpec #%d DCO Matrix will be saved as RADWARE .spn",SNo+1);
       else                      sprintf(Messg,"2dSpec #%d not a standard Gamma-Gamma or DCO Matrix\nFor Radware spn you need 4kx4k Double Word",SNo+1);
       Attention(50*SNo,Messg);
       }
    for (Gate=0;Gate<Setup.Twod.Gate2[SNo].NGates;++Gate)
        {
        if (!ReadBananaFile(Setup.Twod.Gate2[SNo].Gate2d[Gate],&Setup.Twod.Gate2[SNo].BGates[Gate]))
           {
           if (!Suppress)
              {
              AbbreviateFileName(ShortName,Setup.Twod.Gate2[SNo].Gate2d[Gate],18);
              sprintf(Messg,"2dSpec #%d Gate #%d failed:\n%s\nFurther messages suppressed",SNo,Gate,ShortName);
              Attention(200,Messg);
              }
           Suppress=TRUE;
           }
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CheckExogam()
{
FILE *Fp;
gint i,Skip;
gchar Str[125],Option[30];

if ((Fp=fopen("exogam.list","r")) == NULL) 
   { Attention(0,"Severe error: exogam.list not found"); return; }
i=0;
while (1)
   {
   if (fgets(Str,120,Fp)==NULL) break;
   Option[0]='\0';
   sscanf(Str,"%s %d %d %d %s",Setup.Parameter.Name[i],&Skip,&Setup.Parameter.ExogamID[i],
          &Setup.Parameter.Chan[i],Option);
   if (Str[0]=='#') continue;
   if (!strcmp(Option,"ACCEPT_ALL"))            Setup.Parameter.ExoOpt[i]=AcceptAll;
   else if (!strcmp(Option,"REJECT_CS_EVENTS")) Setup.Parameter.ExoOpt[i]=RejectCsEvents;
   else if (!strcmp(Option,"REJECT_PU_EVENTS")) Setup.Parameter.ExoOpt[i]=RejectPuEvents;
   else if (!strcmp(Option,"REJECT_CS_PU"))     Setup.Parameter.ExoOpt[i]=RejectCsPu;
   else if (!strcmp(Option,"ONLY_CS_EVENTS"))   Setup.Parameter.ExoOpt[i]=OnlyCsEvents;
   else if (!strcmp(Option,"ONLY_PU_EVENTS"))   Setup.Parameter.ExoOpt[i]=OnlyPuEvents;
   else                                         Setup.Parameter.ExoOpt[i]=AcceptAll;
   /*g_print("%d Name=%s ID=%d Res=%d Option=%d\n",i,Setup.Parameter.Name[i],Setup.Parameter.ExogamID[i],Setup.Parameter.Chan[i],
            Setup.Parameter.ExoOpt[i]);*/
   if (i==Setup.Parameter.NPar-1) break;
   ++i;
   }
fclose(Fp);
if (i<Setup.Parameter.NPar-1) 
   { 
   sprintf(Str,"Error: exogam.list doesnt list all the %d parameters",Setup.Parameter.NPar);
   Attention(0,Str); 
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CheckScaler()
{
gint i,j;

Setup.Scaler.Master=-1;
for (i=0;i<Setup.Scaler.NSc;++i) 
    { 
    Setup.Scaler.C[i]=CLAMP(Setup.Scaler.C[i],1,2);
    for (j=0;j<MAX_TEXT_FIELD;++j) 
        if (Setup.Scaler.Name[i][j]==' ') Setup.Scaler.Name[i][j]='_'; }        //Replace blanks _
for (i=0;i<Setup.Scaler.NSc;++i)
    if (!strncasecmp(Setup.Scaler.Name[i],"master",5)) { Setup.Scaler.Master=i; break; }
if (Setup.Hardware.NCrates==1) Setup.Scaler.NSc1=Setup.Scaler.NSc;
   else Setup.Scaler.NSc1=CLAMP(Setup.Scaler.NSc1,0,Setup.Scaler.NSc);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void CheckPLog()
{ Setup.PLogSetup.BufCount=CLAMP(Setup.PLogSetup.BufCount,1,8192); }
/*---------------------------------------------------------------------------------------------------------------------*/
void CheckMacros()
{
gint i;

Setup.Macro.N=CLAMP(Setup.Macro.N,0,MAX_MACRO);
Setup.Macro.RefreshRate=CLAMP(Setup.Macro.RefreshRate,1,8192);
for (i=0;i<MAX_MACRO;++i)
    {
    Setup.Macro.Type[i]=CLAMP(Setup.Macro.Type[i],0,9);
    if ( (Setup.Macro.Type[i]==MacroArea) || (Setup.Macro.Type[i]==MacroIntegral) )
       {
       Setup.Macro.SpecNo[i]=CLAMP(Setup.Macro.SpecNo[i],1,Setup.Oned.N);
       Setup.Macro.XMin[i]=CLAMP(Setup.Macro.XMin[i],0,Setup.Oned.Chan[Setup.Macro.SpecNo[i]-1]-2);
       Setup.Macro.XMax[i]=CLAMP(Setup.Macro.XMax[i],Setup.Macro.XMin[i]+1,Setup.Oned.Chan[Setup.Macro.SpecNo[i]-1]-1);
       }
    if ( (Setup.Macro.Type[i]==MacroRectangle) || (Setup.Macro.Type[i]==MacroBanana) )
       {
       Setup.Macro.SpecNo[i]=CLAMP(Setup.Macro.SpecNo[i],1,Setup.Twod.N);
       Setup.Macro.XMin[i]=CLAMP(Setup.Macro.XMin[i],0,Setup.Twod.XChan[Setup.Macro.SpecNo[i]-1]-2);
       Setup.Macro.XMax[i]=CLAMP(Setup.Macro.XMax[i],Setup.Macro.XMin[i]+1,Setup.Twod.XChan[Setup.Macro.SpecNo[i]-1]-1);
       Setup.Macro.YMin[i]=CLAMP(Setup.Macro.YMin[i],0,Setup.Twod.YChan[Setup.Macro.SpecNo[i]-1]-2);
       Setup.Macro.YMax[i]=CLAMP(Setup.Macro.YMax[i],Setup.Macro.YMin[i]+1,Setup.Twod.YChan[Setup.Macro.SpecNo[i]-1]-1);
       }
    if (Setup.Macro.N)
       {
       if (Setup.Scaler.NSc) Setup.Macro.ScalerNo[i]=CLAMP(Setup.Macro.ScalerNo[i],1,Setup.Scaler.NSc);
       Setup.Macro.Index1[i]=CLAMP(Setup.Macro.Index1[i],1,Setup.Macro.N);
       Setup.Macro.Index2[i]=CLAMP(Setup.Macro.Index2[i],1,Setup.Macro.N);
       }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void DecodeSetup()                           //For sparse read we store the Stn values (0-23) for each non Empty module
{
gint Stn,i,A;
//Note: ModuleNames are not needed here except for diagnostic printing purposes
static gchar ModuleNames[13][25]=                                                                        //13 Module types
       { "Empty","Ortec 413 ADC","Ortec 811 ADC","LeCroy ADC/QDC","BARC CM60","Phillips 71xx",
         "CAEN TDC","Silena 4418/Q","BiRa Bit Register","BARC CM88","Sync. Scaler","Unknown","LeCroy MTDC"};

Setup.Decode.Modules=0;
for (Stn=0;Stn<MAX_CAMAC_STNS;++Stn)
    {
    for (i=0;i<16;++i) Setup.Decode.ParNo[Stn][i]=MAX_PAR-1;  //We set ParNo to MAX_PAR-1 at unused subaddresses
    if (Setup.Hardware.Modules[Stn]) 
       {
       Setup.Decode.Module[Setup.Decode.Modules++]=Stn; //For all non empty modules
       for (i=0,A=Setup.Hardware.LoSubStn[Stn];i<Setup.Hardware.NParStn[Stn];++i)  //Here we assume that subaddress ranges do not have holes!! (but can start at a value >0)
           Setup.Decode.ParNo[Stn][A++]=Setup.Hardware.LoParStn[Stn]+i;
       }
    }
if (Setup.Print.Yes)     //Diagnostic print
   {
   for (i=0;i<Setup.Decode.Modules;++i)
       {
       Stn=Setup.Decode.Module[i];
       g_print("Stn+1=%d %s Par=",Stn+1,ModuleNames[Setup.Hardware.Modules[Stn]]);
       for (A=0;A<16;++A) g_print(" %d",Setup.Decode.ParNo[Stn][A]);
       g_print("\n");
       }
   }                    //End diagnostic print
}
//---------------------------------------------------------------------------------------------------------------------
void CheckSetup(void)                           
//Go through the values of setup parameters in memory and make corrections, report errors
{
if (!Setup.Parameter.IgnoreCamac)
   {
   ParameterSetup();                                              //Go through Hardware Setup and create Parameter Setup
   MakeIniClr1("memory.set");                                                   //Create memory.ini and memory.clr files
   MakeIniClr2("memory.set");                                                   //Create memory.ini and memory.clr files
   /*if (Setup.Hardware.CamacMode)*/ DecodeSetup(); //For sparse read we need to collect some information. The Simultor needs this in any case
   }
if (Setup.ListMode.Compr==3) CheckExogam();                                        //Exogam format: read file exogam.list
CheckPseudos();                                                                        //Clamp parameters in pseudo setup
CheckMacros();                                                                          //Clamp parameters in macro setup
CalculateBitShifts();                                            //The bit shifts required for building 1d and 2d spectra
CheckOned();
CheckTwod();
CheckScaler();                                                                       //Locate the scaler labelled Masters
CheckPLog();                                                        //Check settings for the periodic log saftety feature
AllocateMemory();                                                                 //Allocate memory for 1d and 2d spectra
AllocateScreenWins();                                                         //Assign spectra to windows for each screen
ParameterList(NULL,NULL);
Save(1);                                           //Setup file saved to .lamps_set so that lamps can read it on re-start
}
/*---------------------------------------------------------------------------------------------------------------------*/
void ParaError(gint Stn)
{ 
gchar Str[120];

ProgramState=Invalid;
sprintf(Str,"Error in Hardware Setup Stn No %d\nPara=%s\nThe setup is now invalid",
        Stn+1,Setup.Hardware.Paras[Stn]);
Attention(0,Str);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void SubAddrError(gint Stn)
{ 
gchar Str[120];

ProgramState=Invalid;
sprintf(Str,"Error in Hardware Setup Stn No %d\nSubAddr=%s\nThe Setup is now invalid",
        Stn+1,Setup.Hardware.SubAddr[Stn]);
Attention(0,Str);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void NameError(gint Stn)
{ 
gchar Str[120];

ProgramState=Invalid;
sprintf(Str,"Error in Hardware Setup Stn No %d\nName=%s\nThe Setup is now invalid",
        Stn+1,Setup.Hardware.ParaNames[Stn]);
Attention(0,Str);
}
/*---------------------------------------------------------------------------------------------------------------------*/
gint ValidateAdcGain(Gain)                                    //Makes sure the gain is a power of 2
{
gint i,Val;

Gain=CLAMP(Gain,16,65536);                             //Dont expect less than 16 and of course cant exceed 64K
for (i=0,Val=1;i<17;++i) { Gain>>=1; if (!Gain) return Val; Val<<=1; }
return 8192;                                                         //Code doesnt ever reach here
}
/*---------------------------------------------------------------------------------------------------------------------*/
void ParsePar(gint Stn,gchar *LoPar,gchar *HiPar,gint NoHi,gint *LoP,gint *HiP)
{
int i;

*LoP=CLAMP(atoi(LoPar),1,MAX_PAR);
if (!NoHi) *HiP=CLAMP(atoi(HiPar),1,MAX_PAR); else *HiP=*LoP;
Setup.Parameter.NPar=0;
for (i=*LoP-1;i<=*HiP-1;++i)
    {
    Setup.Parameter.NPar=MAX(i+1,Setup.Parameter.NPar);
    Setup.Parameter.N[i]=Stn+1;
    Setup.Parameter.F[i]=CLAMP(Setup.Hardware.Properties[Stn].AdcFCode,0,31);             //F=0-31
    Setup.Parameter.Chan[i]=ValidateAdcGain(Setup.Hardware.Properties[Stn].AdcGain); 
    Setup.Parameter.LLD[i]=CLAMP(Setup.Hardware.ZSupLLD[Stn],1,Setup.Parameter.Chan[i]-1); 
    Setup.Parameter.ULD[i]=CLAMP(Setup.Hardware.ZSupULD[Stn],1,Setup.Parameter.Chan[i]-1);
    }
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ParseSub(gint Stn,gchar *LoSub,gchar *HiSub,gint NoHiA,gint LoP,gint HiP)
{
int i,LoA,A;

/*Notice that HiSub is not required at all!*/
LoA=CLAMP(atoi(LoSub),0,15);                        //In the CAMAC standard A is in the range 0-15
for (i=LoP-1,A=LoA;i<=HiP-1;++i,A++) Setup.Parameter.A[i]=A;
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ParseName(gint Stn,gchar *LoName,gchar *HiName,gint NoHiN,gint LoP,gint HiP)
{
gint i,D,Numeric;
gchar AlphaName[LONG_TEXT_FIELD];

/*Again notice that HiName is not required at all!*/
if (!strlen(LoName)) return; //If field is blank do nothing, init.c would have setup default names 
  // Calcultae D=position of numeric field. Eg: LoName="Clover1E01" then D=8 i.e position of "01"
for (i=strlen(LoName)-1;i>=0;i--) if (!isdigit((gint)LoName[i])) { D=i+1; break; }
D=CLAMP(D,0,MAX_TEXT_FIELD-5);                          //Allow 4 places for digit field + 1 safety
Numeric=CLAMP(atoi(&LoName[D]),0,9999-HiP+LoP);       //Ensure Numeric will not exceed 4 characters
strncpy(AlphaName,LoName,D); AlphaName[D]='\0';
for (i=LoP-1;i<=HiP-1;++i,Numeric++) 
    {
    if (Numeric) sprintf(Setup.Parameter.Name[i],"%s%d",AlphaName,Numeric); 
    else         sprintf(Setup.Parameter.Name[i],"%s",AlphaName); 
                //No numeric field if zero. Probably the user didnt put it
    }
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ParameterSetup(void)                                         //Go through Hardware Setup and create Parameter Setup
{
gint Stn,L,LA,LN,i,Pos,PosA,PosN,Lo,LoA,LoN,NoHi,NoHiA,NoHiN,NoMore,Start,StartA,StartN,LoP,HiP,NPar1,NPar2;
gchar c,LoPar[8],HiPar[8],LoSub[8],HiSub[8],LoName[45],HiName[45];

Setup.Hardware.NCrates=CLAMP(Setup.Hardware.NCrates,1,2);                                                //Extra precaution
Setup.Hardware.CamacMode=CLAMP(Setup.Hardware.CamacMode,0,1);                                            //Extra precaution
for (Stn=0;Stn<Setup.Hardware.NCrates*MAX_CAMAC_STNS;Stn++)
    {
    Setup.Hardware.LoParStn[Stn]=0; Setup.Hardware.NParStn[Stn]=0; Setup.Hardware.LoSubStn[Stn]=0;
    if (Setup.Hardware.Modules[Stn]==10) 
       { Setup.Hardware.ZSupLLD[Stn]=1; Setup.Hardware.ZSupULD[Stn]=65535; }                                 //Sync. Scaler
    if (strlen(Setup.Hardware.Paras[Stn]))
       {
       NoMore=0; Start=0; StartA=0; StartN=0;
       do
          {
          //------Decode Para-----
          L=strlen(&Setup.Hardware.Paras[Stn][Start]); 
          for (i=0;i<=L;++i) 
              if ((Setup.Hardware.Paras[Stn][Start+i]==';') || (Setup.Hardware.Paras[Stn][Start+i]==',')) { L=i; break; }
          for (i=0,Lo=1,Pos=0;i<=L;++i)
              {
              c=Setup.Hardware.Paras[Stn][Start+i]; 
              switch (c)
                {
                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                          if (Lo) LoPar[Pos]=c; else HiPar[Pos]=c;
                          Pos++;
                          if (Pos>5) { ParaError(Stn); return; }
                          break;
                case '-': if (Lo) { LoPar[Pos]='\0'; Lo=0; Pos=0; } else { ParaError(Stn); return; }
                          break;
                case '\0': NoMore=1;
                case ';': case ',':
                          if (Lo) { LoPar[Pos]='\0'; NoHi=1; Lo=0; }
                          else    { HiPar[Pos]='\0'; NoHi=0; Lo=1; }
                          if (!strlen(LoPar)) { ParaError(Stn); return; }
                          if ( (!NoHi) && (!strlen(HiPar)) ) { ParaError(Stn); return; }
                          ParsePar(Stn,LoPar,HiPar,NoHi,&LoP,&HiP); Pos=0; Start+=i+1;
                          break;
                default: ParaError(Stn); return;
                }             //End of switch (c)
              }               //End of for (i=
          //-----End Decode Para----
          Setup.Hardware.LoParStn[Stn]=atoi(LoPar)-1; Setup.Hardware.NParStn[Stn]=atoi(HiPar)-atoi(LoPar)+1;  //Stored to use in QStop mode
          //-----Decode SubAddr----
          LA=strlen(&Setup.Hardware.SubAddr[Stn][StartA]);
          for (i=0;i<=LA;++i) 
              if ( (Setup.Hardware.SubAddr[Stn][StartA+i]==';') || (Setup.Hardware.SubAddr[Stn][StartA+i]==',') ) 
                 { LA=i; break; }
          if (!LA) { SubAddrError(Stn); return; }
          for (i=0,LoA=1,PosA=0;i<=LA;++i)
              {
              c=Setup.Hardware.SubAddr[Stn][StartA+i];
              switch (c)
                 {
                 case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                           if (LoA) LoSub[PosA]=c; else HiSub[PosA]=c;
                           PosA++;
                           if (PosA>3) { SubAddrError(Stn); return; }
                           break;
                 case '-': if (LoA) { LoSub[PosA]='\0'; LoA=0; PosA=0; } else { SubAddrError(Stn); return; }
                           break;
                 case '\0': case ';': case ',':
                           if (LoA) { LoSub[PosA]='\0'; NoHiA=1; LoA=0; }
                           else     { HiSub[PosA]='\0'; NoHiA=0; LoA=1; }
                           if (!strlen(LoSub)) { SubAddrError(Stn); return; }
                           if ( (!NoHiA) && (!strlen(HiSub)) ) { SubAddrError(Stn); return; }
                           ParseSub(Stn,LoSub,HiSub,NoHiA,LoP,HiP); PosA=0; StartA+=i+1;
                           break;
                 default: SubAddrError(Stn); return;
                 }
              }  
          //-----End Decode SubAddr-----
          Setup.Hardware.LoSubStn[Stn]=atoi(LoSub);   //Stored to use in QStop mode
          //-----Decode ParaNames-------
          LN=strlen(&Setup.Hardware.ParaNames[Stn][StartN]);
          for (i=0;i<=LN;++i)
              if ( (Setup.Hardware.ParaNames[Stn][StartN+i]==';') || (Setup.Hardware.ParaNames[Stn][StartN+i]==',') ) 
                 { LN=i; break; }
          if (LN)
             {
             for (i=0,LoN=1,PosN=0;i<=LN;++i)
                 {
                 c=Setup.Hardware.ParaNames[Stn][StartN+i]; 
                 switch (c)
                    {
                    case '-': if (LoN) { LoName[PosN]='\0'; LoN=0; PosN=0; } else { NameError(Stn); return; }
                           break;
                    case '\0': case ';': case ',':
                           if (LoN) { LoName[PosN]='\0'; NoHiN=1; LoN=0; }
                           else     { HiName[PosN]='\0'; NoHiN=0; LoN=1; }
                           if (!strlen(LoName)) { NameError(Stn); return; }
                           if ( (!NoHiN) && (!strlen(HiName)) ) { NameError(Stn); return; }
                           ParseName(Stn,LoName,HiName,NoHiN,LoP,HiP); PosN=0; StartN+=i+1;
                           break;
                    default: if (LoN) LoName[PosN]=c; else HiName[PosN]=c;
                             PosN++;
                             if (PosN>LONG_TEXT_FIELD+10) { NameError(Stn); return; }
                    }
                 }
             }
          //-----End Decode ParaNames-------
          }while (!NoMore);   //End of do
       }                      //End of if (strlen
    }                         //End of for (Stn=
Setup.ListMode.Rate=CLAMP(Setup.ListMode.Rate,0,4);
for (i=0,NPar1=NPar2=0;i<Setup.Parameter.NPar;++i) { if (Setup.Parameter.N[i]>MAX_CAMAC_STNS) ++NPar2; else ++NPar1; }
}
/*---------------------------------------------------------------------------------------------------------------------*/
void ParaListClosed(GtkWidget *W,gpointer Data)
{ ParaListVisible=FALSE; }
/*---------------------------------------------------------------------------------------------------------------------*/
void ParameterList(GtkWidget *W,gpointer Data)
{
gint i;
GtkWidget *HBox,*VBox,*ScrollW,*CList;
static gchar *Titles[3] = {"Para","Name","Chan"};
gchar *Text[9],Col0[10],Col1[MAX_TEXT_FIELD],Col2[10];

if (ParaListVisible) gtk_widget_destroy(ParaListW);
ParaListW=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_signal_connect(GTK_OBJECT(ParaListW),"destroy",GTK_SIGNAL_FUNC(ParaListClosed),ParaListW);
gtk_window_set_title(GTK_WINDOW(ParaListW),"Parameter List"); gtk_widget_set_uposition(GTK_WIDGET(ParaListW),MONITOR_XRES-1,0);
gtk_widget_set_usize(GTK_WIDGET(ParaListW),250,690); gtk_container_set_border_width(GTK_CONTAINER(ParaListW),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(ParaListW),VBox);
HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);
CList=gtk_clist_new_with_titles(3,Titles);
gtk_clist_set_column_width(GTK_CLIST(CList),1,120);
gtk_container_add(GTK_CONTAINER(ScrollW),CList);
for (i=0;i<Setup.Parameter.NPar;++i)
    {
    sprintf(Col0,"%d",i+1);                     Text[0]=Col0;
    sprintf(Col1,"%s",Setup.Parameter.Name[i]); Text[1]=Col1;
    sprintf(Col2,"%d",Setup.Parameter.Chan[i]); Text[2]=Col2;
    gtk_clist_append((GtkCList *)CList,Text);
    }
for (i=0;i<Setup.Pseudo.N;++i)
    {
    sprintf(Col0,"%d",Setup.Parameter.NPar+i+1); Text[0]=Col0;
    sprintf(Col1,"%s",Setup.Pseudo.Name[i]);     Text[1]=Col1;
    sprintf(Col2,"%d",Setup.Pseudo.Size[i]);     Text[2]=Col2;
    gtk_clist_append((GtkCList *)CList,Text);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
ParaListVisible=TRUE; gtk_widget_show_all(ParaListW);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ListModeSettings(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*Label,*VBox,*HBox,*CList,*But;
static gchar *Titles[4] = {"No","Para No.","Lo Limit","Hi Limit"};
gchar Str[80],*Text[4],Col0[6],Col1[MAX_TEXT_FIELD],Col2[MAX_TEXT_FIELD],Col3[MAX_TEXT_FIELD];
gint i;

if (ProgramState == DoingSetup) { Attention(0,"Cant view the Parameter List\nin the current Program State"); return; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                                        //Make the window modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"List Mode Settings"); gtk_widget_set_uposition(GTK_WIDGET(Win),127,TOP_SPACE);
gtk_widget_set_usize(GTK_WIDGET(Win),250,73); gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
if (Setup.ListMode.ListOn) sprintf(Str,"List Mode On"); else sprintf(Str,"List Mode Off");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,TRUE,0);
sprintf(Str,"Data Rate(0=Highest ... 4=Slow): %d",Setup.ListMode.Rate);
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,TRUE,0);
if (Setup.ListMode.ListOn)
   {
   gtk_widget_set_usize(GTK_WIDGET(Win),250,120);
   switch (Setup.ListMode.Compr)
          {
          case 1: sprintf(Str,"File Format: Normal (zls scheme)"); break;
          case 2: sprintf(Str,"File Format: Advanced (gls scheme)"); break;
          case 0: sprintf(Str,"File Format: Candle (nsc scheme)"); 
          }
   Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,TRUE,0);
   if (Setup.ListMode.NoOfLGates)
      {
      gtk_widget_set_usize(GTK_WIDGET(Win),262,312);
      CList=gtk_clist_new_with_titles(4,Titles);
      for (i=0;i<4;++i) gtk_clist_set_column_width(GTK_CLIST(CList),i,60);                                   //Same width for all columns
      gtk_clist_set_column_width(GTK_CLIST(CList),0,20);                                                       //Except 1st column narrow
      gtk_box_pack_start(GTK_BOX(VBox),CList,FALSE,FALSE,0);
      for (i=0;i<Setup.ListMode.NoOfLGates;++i)
          {
          sprintf(Col0,"%d",i+1);                            Text[0]=Col0;
          sprintf(Col1,"%5d",Setup.ListMode.LGates[i].Para); Text[1]=Col1;
          sprintf(Col2,"%5d",Setup.ListMode.LGates[i].Lo);   Text[2]=Col2;
          sprintf(Col3,"%5d",Setup.ListMode.LGates[i].Hi);   Text[3]=Col3;
          gtk_clist_append((GtkCList *)CList,Text);
          }
      }
   }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedSpecSettings(GtkWidget *W,gpointer Data) 
{
GtkWidget *Win,*VBox,*HBox,*Label,*CList,*But,*ScrW;
static gchar *Titles[5] = {"No","Para","Size","1d Gates","2d Gates"};
gchar Str[80],*Text[5],Col0[6],Col1[MAX_TEXT_FIELD],Col2[MAX_TEXT_FIELD],Col3[MAX_TEXT_FIELD],Col4[MAX_TEXT_FIELD];
gint i;

if (ProgramState == DoingSetup) { Attention(0,"Cant view the Parameter List\nin the current Program State"); return; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                                        //Make the window modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Spectra Settings"); gtk_widget_set_uposition(GTK_WIDGET(Win),127,TOP_SPACE);
gtk_widget_set_usize(GTK_WIDGET(Win),340,227); gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
if (Setup.Oned.WSz==1) sprintf(Str,"No of 1d Spectra (16-bit)=%d",Setup.Oned.N); 
else                   sprintf(Str,"No of 1d Spectra (32-bit)=%d",Setup.Oned.N);
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

ScrW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrW,TRUE,TRUE,0);
CList=gtk_clist_new_with_titles(5,Titles);
for (i=0;i<5;++i) gtk_clist_set_column_width(GTK_CLIST(CList),i,60);                                   //Same width for all columns
gtk_clist_set_column_width(GTK_CLIST(CList),0,20);                                                       //Except 1st column narrow
gtk_container_add(GTK_CONTAINER(ScrW),CList);
for (i=0;i<Setup.Oned.N;++i)
    {
    sprintf(Col0,"%d",i+1);                            Text[0]=Col0;
    sprintf(Col1,"%5d",Setup.Oned.Par[i]);             Text[1]=Col1;
    sprintf(Col2,"%5d",Setup.Oned.Chan[i]);            Text[2]=Col2;
    sprintf(Col3,"%5d",Setup.Oned.Gate1[i].NGates);    Text[3]=Col3;
    sprintf(Col4,"%5d",Setup.Oned.Gate2[i].NGates);    Text[4]=Col4;
    gtk_clist_append((GtkCList *)CList,Text);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodSpecSettings(GtkWidget *W,gpointer Data) 
{
GtkWidget *Win,*VBox,*HBox,*Label,*CList,*But,*ScrW;
static gchar *Titles[6] = {"No","X-Para","Y-Para","Size","1d Gates","2d Gates"};
gchar Str[80],*Text[6],Col0[6],Col1[MAX_TEXT_FIELD],Col2[MAX_TEXT_FIELD],Col3[MAX_TEXT_FIELD],Col4[MAX_TEXT_FIELD],Col5[MAX_TEXT_FIELD];
gint i;

if (ProgramState == DoingSetup) { Attention(0,"Cant view the Parameter List\nin the current Program State"); return; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                                        //Make the window modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Spectra Settings"); gtk_widget_set_uposition(GTK_WIDGET(Win),127,TOP_SPACE);
gtk_widget_set_usize(GTK_WIDGET(Win),400,227); gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
if (Setup.Oned.WSz==1) sprintf(Str,"No of 2d Spectra (16-bit)=%d",Setup.Twod.N); 
else                   sprintf(Str,"No of 2d Spectra (32-bit)=%d",Setup.Twod.N);
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

ScrW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrW,TRUE,TRUE,0);
CList=gtk_clist_new_with_titles(6,Titles);
for (i=0;i<6;++i) gtk_clist_set_column_width(GTK_CLIST(CList),i,60);                                   //Same width for all columns
gtk_clist_set_column_width(GTK_CLIST(CList),0,20);                                                       //Except 1st column narrow
gtk_container_add(GTK_CONTAINER(ScrW),CList);
for (i=0;i<Setup.Twod.N;++i)
    {
    sprintf(Col0,"%d",i+1);                                          Text[0]=Col0;
    sprintf(Col1,"%5d",Setup.Twod.XPar[i]);                          Text[1]=Col1;
    sprintf(Col2,"%5d",Setup.Twod.YPar[i]);                          Text[2]=Col2;
    sprintf(Col3,"%4dx%4d",Setup.Twod.XChan[i],Setup.Twod.YChan[i]); Text[3]=Col3;
    sprintf(Col4,"%5d",Setup.Twod.Gate1[i].NGates);                  Text[4]=Col4;
    sprintf(Col5,"%5d",Setup.Twod.Gate2[i].NGates);                  Text[5]=Col5;
    gtk_clist_append((GtkCList *)CList,Text);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PseudoSettings(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*VBox,*HBox,*Label,*CList,*But,*ScrW;
static gchar *Titles[5] = {"No","Para 1","Para 2","Size","Type"};
gchar Str[80],*Text[5],Col0[6],Col1[MAX_TEXT_FIELD],Col2[MAX_TEXT_FIELD],Col3[MAX_TEXT_FIELD],Col4[MAX_TEXT_FIELD];
gint i;

if (ProgramState == DoingSetup) { Attention(0,"Cant view the Parameter List\nin the current Program State"); return; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                                        //Make the window modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Pseudo Parameters"); gtk_widget_set_uposition(GTK_WIDGET(Win),127,TOP_SPACE);
gtk_widget_set_usize(GTK_WIDGET(Win),350,227); gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"No of Pseudo Parameters=%d",Setup.Pseudo.N); 
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

ScrW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrW,TRUE,TRUE,0);
CList=gtk_clist_new_with_titles(5,Titles);
for (i=0;i<5;++i) gtk_clist_set_column_width(GTK_CLIST(CList),i,60);                                   //Same width for all columns
gtk_clist_set_column_width(GTK_CLIST(CList),0,20);                                                       //Except 1st column narrow
gtk_container_add(GTK_CONTAINER(ScrW),CList);
for (i=0;i<Setup.Pseudo.N;++i)
    {
    sprintf(Col0,"%d",i+1);                   Text[0]=Col0;
    sprintf(Col1,"%5d",Setup.Pseudo.P1[i]);   Text[1]=Col1;
    sprintf(Col2,"%5d",Setup.Pseudo.P2[i]);   Text[2]=Col2;
    sprintf(Col3,"%5d",Setup.Pseudo.Size[i]); Text[3]=Col3;
    switch (Setup.Pseudo.Type[i])
           {
           case Sum:          sprintf(Col4,"Sum");          break;
           case Product:      sprintf(Col4,"Product");      break;
           case Ratio:        sprintf(Col4,"Ratio");        break;
           case Position:     sprintf(Col4,"Position");     break;
           case PI:           sprintf(Col4,"PI");           break;
           case Multiplicity: sprintf(Col4,"Multiplicity"); break;
           case User:         sprintf(Col4,"User");         break;
           case Array:        sprintf(Col4,"Array");        break;
           case BGated:       sprintf(Col4,"BGated");
           }
    Text[4]=Col4;
    gtk_clist_append((GtkCList *)CList,Text);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ScalerSettings(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*VBox,*HBox,*Label,*But;
gchar Str[120];
gint i;

if (ProgramState == DoingSetup) { Attention(0,"Cant view the Parameter List\nin the current Program State"); return; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Scaler Settings"); gtk_widget_set_uposition(GTK_WIDGET(Win),127,TOP_SPACE);
gtk_widget_set_usize(GTK_WIDGET(Win),250,150); gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
sprintf(Str,"Number of Scalers=%d",Setup.Scaler.NSc); Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);
for (i=0;i<Setup.Scaler.NSc;++i)
    {
    HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
    sprintf(Str,"%d %-30s C=%d N=%d A=%d F=%d",i+1,Setup.Scaler.Name[i],Setup.Scaler.C[i],Setup.Scaler.N[i],
            Setup.Scaler.A[i],Setup.Scaler.F[i]);
    Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MakeIniClr1(gchar *FName)                         //Creates .ini and .clr files as FName1.ini, FName1.clr for Crate 1
{
FILE *Fp;
gchar Str[120],IniName[MAX_FNAME_LENGTH],ClrName[MAX_FNAME_LENGTH];
gint i,j,CLam,SLam;
gboolean LLDWarning,PhillipsLam,SilenaExists,AD413Exists;

if (index(FName,'.') == NULL) { sprintf(Str,"ERROR\nNo dot in %s",FName); Attention(0,Str); }
sprintf(IniName,"%s/%s",SetupDir,FName); sprintf(ClrName,"%s/%s",SetupDir,FName);
strcpy(rindex(IniName,'.'),"1.ini"); strcpy(rindex(ClrName,'.'),"1.clr");

if ((Fp=fopen(IniName,"w")) == NULL) 
   { 
   sprintf(Str,"nice -n 10 crash \"Could not create %s\" \"Could be a file permissions problem\"&",IniName);
   system(Str); exit(1);
   }  
CLam=SLam=0;                                             //Count Stns with Lam in coinc and singles mode to issue warnings
LLDWarning=FALSE;                                                       //If LLD is too low (for AD413) we issue a warning

fprintf(Fp,"CLRI   ;Clear Inhibit\n"); fprintf(Fp,"SETZ   ;Initialize Crate\n");            //Two commands at the begining

/*Insert commands for AD413 ADCs*/
AD413Exists=FALSE; for (i=0;i<MAX_CAMAC_STNS;++i) if (Setup.Hardware.Modules[i] == 1) AD413Exists=TRUE;
if (AD413Exists)
   {
   fprintf(Fp,"SETI\n"); /* fprintf(Fp,"CLRC\n");                       Decided to do individual module clears 22-12-04 */
   for (i=0;i<MAX_CAMAC_STNS;++i)
       {
       if (Setup.Hardware.Modules[i] == 1)  /*Ortec 413 ADC*/
          {
          fprintf(Fp,";--------------------------Commands for AD413--------------------------------\n");
          fprintf(Fp,"NAF : %d 0 9         ;Clear the ADC\n",i+1);
          if (Setup.Hardware.Properties[i].AdcMode == Singles)                                              //Singles mode
             { strcpy(Str,"DATA = 62208         ;62208 for Singles Mode,Lam enabled\n"); ++SLam; }
                                                                                //Singles mode. Here we assume LAM=Enabled
          else if (Setup.Hardware.Properties[i].AdcLam == Enabled)                         //Coincidence mode, LAM enabled
             { strcpy(Str,"DATA = 58112         ;58112 for Coinc mode Lam enabled\n"); ++CLam; }
          else
             strcpy(Str,"DATA = 41728         ;41728 for Coinc mode Lam disabled\n");     //Coincidence mode, LAM disabled
          fprintf(Fp,Str); fprintf(Fp,"NAF : %d 0 16        ;Control Register 1\n",i+1);
          if (Setup.Hardware.Properties[i].AdcMode == Singles)                     //For singles mode, disable master gate
             fprintf(Fp,"DATA = 16            ;B5=1 to disable master gate\n");
          else                                                     
             fprintf(Fp,"DATA = 0             ;All gates enabled\n");             //For coincidence mode, enable all gates
          fprintf(Fp,"NAF : %d 1 16        ;Control Register 2\n",i+1);
          for (j=0;j<4;++j)                                                                     //For SubAddresses 0,1,2,3
              {
              fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].AdcLLD);
              fprintf(Fp,"NAF : %d %d 17        ;Set LLD of Ch%d (Default is 36*2=72mV)\n",i+1,j,j);
              if (Setup.Hardware.Properties[i].AdcLLD<30) LLDWarning=TRUE;
              }
          }
       }
   fprintf(Fp,";---------------------------------------------------------------------------\n");
   fprintf(Fp,"CLRI\n");
   }

/*Insert commands for Phillips 71xx*/
for (i=0,PhillipsLam=FALSE;i<MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 5)                                                                   //Phillips 71xx
       {
       if (Setup.Hardware.Properties[i].AdcMode == Test)                                     //Phillips 71xx in test mode
          {
          PhillipsLam=TRUE;
          fprintf(Fp,"NAF : %d 0 26          ;Enable Phillips TDC LAM\n",i+1);
          fprintf(Fp,"NAF : %d 0 26         ;Enable Phillips 71xx LAM\n",i+1);
          fprintf(Fp,"NAF : %d 3 11          ;Reset Hit Register and Data Memory\n",i+1);
          fprintf(Fp,"NAF : %d 1 25          ;Test 1/3\n",i+1);
          }
       else
          {
          if (Setup.Hardware.CamacMode)  //Setup Pedestal and Thresholds only in the Q-Stop case
             {
             fprintf(Fp,"NAF : %d 0 17          ;Select Pedestal for next F20 operation\n",i+1);
             for (j=0;j<16;++j) 
                 {
                 fprintf(Fp,"DATA = 0              ;Pedestal value\n");
                 fprintf(Fp,"NAF : %d %d 20          ;Set pedestal\n",i+1,j);
                 }
             fprintf(Fp,"NAF : %d 1 17          ;Select Lower Threshold for next F20 operation\n",i+1);
             for (j=0;j<16;++j) 
                 {
                 fprintf(Fp,"DATA = %d              ;LT value\n",Setup.Hardware.Properties[i].LowerThreshold[j]);
                 fprintf(Fp,"NAF : %d %d 20          ;Set LT\n",i+1,j);
                 }
             fprintf(Fp,"NAF : %d 2 17          ;Select Upper Threshold for next F20 operation\n",i+1);
             for (j=0;j<16;++j)
                 {
                 fprintf(Fp,"DATA = %d           ;UT value\n",Setup.Hardware.Properties[i].UpperThreshold[j]);
                 fprintf(Fp,"NAF : %d %d 20          ;Set UT\n",i+1,j);
                 }
             fprintf(Fp,"DATA = 7               ;Note: 2^2+2^1+1=7\n");
             fprintf(Fp,"NAF : %d 0 19          ;Enable Pedestal, Upper and Lower Thresholds\n",i+1);
             }
          if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
             {
             PhillipsLam=TRUE; 
             fprintf(Fp,"NAF : %d 0 26         ;Enable Phillips 71xx LAM\n",i+1); ++CLam; 
             }
          fprintf(Fp,"NAF : %d 3 11          ;Reset Hit Register and Data Memory\n",i+1);
          }
       }
    }

/*Insert conmmands for LeCroy 3377 Multi-hit TDC*/
for (i=0;i<MAX_CAMAC_STNS;++i)
    {
       if (Setup.Hardware.Modules[i] == 12)  /*LeCroy Multi-hit TDC*/
          {
          fprintf(Fp,";---------------------Commands for LeCroy 3377 Multi-hit TDC-----------------\n");
          fprintf(Fp,"NAF : %d 0 9        ;Clears all data and Lam\n",i+1);
          fprintf(Fp,"NAF : %d 0 30       ;Begin the programming sequence\n",i+1);
          fprintf(Fp,"NAF : %d 0 23       ;Select EPROM mode 3: Common start double word\n",i+1);
          fprintf(Fp,"NAF : %d 0 25       ;Begin Xilinx programming sequence\n",i+1);
          fprintf(Fp,"DLY = 1000          ;Wait for 1 sec (200 ms is required)\n");
          fprintf(Fp,"NAF : %d 0 13       ;Test Xlinx programming done\n",i+1);
          fprintf(Fp,"NAF : %d 0 9        ;Clears all data and Lam\n",i+1);
          fprintf(Fp,"DATA = 0            ;IDCode=0, rising edge, CAMAC, single buffer\n");
          fprintf(Fp,"NAF : %d 0 17       ;Control Register 0\n",i+1);
          fprintf(Fp,"DATA = 0            ;Data for control register 1\n");
          fprintf(Fp,"NAF : %d 1 17       ;Control Register 1\n",i+1);
          fprintf(Fp,"DATA = 1            ;Hits\n");
          fprintf(Fp,"NAF : %d 2 17       ;Control Register 2\n",i+1);
          fprintf(Fp,"DATA = 0            ;Request delay setting=0\n");
          fprintf(Fp,"NAF : %d 3 17       ;Control Register 3\n",i+1);
          fprintf(Fp,"DATA = 600          ;Timeout=600*50 ns=30 us (Effective full scale)\n");
          fprintf(Fp,"NAF : %d 4 17       ;Control Register 4\n",i+1);
          fprintf(Fp,"DATA = 0            ;No test mode\n");
          fprintf(Fp,"NAF : %d 5 17       ;Control Register 5\n",i+1);
          if (Setup.Hardware.Properties[i].AdcLam == Enabled)
             {
             CLam++;
             fprintf(Fp,"NAF : %d 0 26       ;Enable LAM\n",i+1);
             }
          fprintf(Fp,"NAF : %d 1 26       ;Enable Acquisition mode\n",i+1);
          }
    }

/*Insert commands for BARC CM60*/
for (i=0;i<MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 4)  /*BARC CM60*/
       {
       for (j=0;j<4;j++)
           {
           fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].AdcLLD);
           fprintf(Fp,"NAF : %d %d 17        ;Set LLD of Ch%d\n",i+1,j,j);
           }
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 12 26          ;Enable CM60 LAM\n",i+1); ++CLam; }
       }
    }

/*Insert commands for ORTEC 811*/
for (i=0;i<MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 2)  /*Ortec AD811*/
       {
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 12 26          ;Enable AD811 LAM\n",i+1); ++CLam; }
       }
    }

/*Insert commands for LeCroy ADC*/
for (i=0;i<MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 3)  /*LeCroy ADC or QDC*/
       {
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 0 26           ;Enable LeCrory ADC/QDC LAM\n",i+1); ++CLam; }
       }
    }

/*Insert commands for CAEN TDC*/
for (i=0;i<MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 6)  /*CAEN TDC*/
       {
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 0 26           ;Enable CAEN TDC LAM\n",i+1); ++CLam; }
       }
    }

/*Insert commands for Silena 4418/Q units*/
SilenaExists=FALSE; for (i=0;i<MAX_CAMAC_STNS;++i) if (Setup.Hardware.Modules[i] == 7) SilenaExists=TRUE;
if (SilenaExists)
   {
   fprintf(Fp,"SETI\n"); 
   if (!PhillipsLam) fprintf(Fp,"CLRC\n");
   else fprintf(Fp,";Unfortunately CLRC cannot be done as PhillipsLam present\n");
   for (i=0;i<MAX_CAMAC_STNS;++i)
       {
       if (Setup.Hardware.Modules[i] == 7)  //Silena 4418/Q
          {
          fprintf(Fp,";--------------------------Silena 4418/Q-------------------------------------\n");
          fprintf(Fp,"DATA = %d\n",MIN(255,Setup.Hardware.Properties[i].AdcLLD));
          fprintf(Fp,"NAF : %d 9 20         ;Set common threshold value (N.B. maximum is 255)\n",i+1); 
          if (Setup.Hardware.Properties[i].AdcLam == Enabled)
             { strcpy(Str,"DATA = 18944         ;18944 for Lam enabled\n"); ++CLam; }
          else
             strcpy(Str,"DATA = 2560          ;2560  for Lam disabled\n");
          fprintf(Fp,Str);
          fprintf(Fp,"NAF : %d 14 20         ;Status Register\n",i+1); 
          for (j=0;j<8;++j)
              {
              fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].SilenaOffset[j]);
              fprintf(Fp,"NAF : %d %d 20    ;Set Silena offset\n",i+1,j);
              }
          }
       }
   fprintf(Fp,";---------------------------------------------------------------------------\n");
   fprintf(Fp,"CLRI\n");
   }
/*Insert commands for BiRa 2351 Bit Registers*/
for (i=0;i<MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 8) //BiRa bit register
       {
       fprintf(Fp,";--------------------------BiRa 2351 Bit Register----------------------------\n");
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 0 26         ;Enable LAM\n",i+1); ++CLam; }
       else                                                
          fprintf(Fp,";No commands needed\n");
       }
    }
/*Insert commands for BARC CM88 ADCs*/
for (i=0;i<MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 9) //BARC CM88 ADC
       {
       fprintf(Fp,";--------------------------Commands for BARC CM88----------------------------\n");
/*This ADC has 4 modes: 0=Coincidence, 1=Coincidence with individual gates enabled
                        2=Singles      3=Singles with individual gates enabled     
  In modes 0 and 1 master gate is required and in modes 2 and 3 there is no master gate */
       if (Setup.Hardware.Properties[i].AdcMode == Coincidence) strcpy(Str,"DATA = 9     ;Coin. no individual gates\n");
       if (Setup.Hardware.Properties[i].AdcMode == CoinGate)    strcpy(Str,"DATA = 25    ;Coin. + individual gates\n");
       if (Setup.Hardware.Properties[i].AdcMode == Singles)     strcpy(Str,"DATA = 12    ;Singles no individual gates\n");
       if (Setup.Hardware.Properties[i].AdcMode == SingGate)    strcpy(Str,"DATA = 8     ;Sing. + individual gates\n");
       fprintf(Fp,Str);
       fprintf(Fp,"NAF : %d 0 16\n",i+1);
       fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].AdcLLD);
       fprintf(Fp,"NAF : %d 0 17        ;Set LLD(mV)=2*number\n",i+1);
       if (Setup.Hardware.Properties[i].AdcLLD<30) LLDWarning=TRUE;
       fprintf(Fp,"DATA = 254\n");
       fprintf(Fp,"NAF : %d 1 17        ;Set ULD (0-255) to maximum\n",i+1);
       if (Setup.Hardware.Properties[i].AdcLam == Enabled)
          { 
          fprintf(Fp,"NAF : %d 12 26         ;Enable LAM\n",i+1); 
          if ( (Setup.Hardware.Properties[i].AdcMode == Coincidence) || 
               (Setup.Hardware.Properties[i].AdcMode == CoinGate) ) ++CLam; 
          else ++SLam; 
          } 
       }
    }

fprintf(Fp,";---------------------------------------------------------------------------\n");
if (Setup.Hardware.NCrates==2) fprintf(Fp,"NAF : 23 0 9 ;Operate CM90 Sync. Module\n");
if (Setup.Hardware.NCrates==1 && !PhillipsLam && !Setup.Hardware.CamacMode) fprintf(Fp,"CLRC                  ;For first LAM\n");
fprintf(Fp,"END\n");
fprintf(Fp,";---------------------------------------------------------------------------\n");
fprintf(Fp,";The folowing keywords are used in these files:\n");
fprintf(Fp,";SETZ (Camac Z), CLRC (Camac C), SETI (Set Inhibit), CLRI (Clear Inhibit),\n");
fprintf(Fp,";DATA = <decimal number>, NAF : <N> <A> <F> (Camac NAF command)\n");
fprintf(Fp,";DLY = <decimal number=Delay in milliseconds>\n");
fprintf(Fp,";The END at the end is not compulsory\n");
fprintf(Fp,";Comments can be put after a semicolon\n");
fprintf(Fp,";---------------------------------------------------------------------------\n");
fclose(Fp);

/*Issue warnings*/
if (CLam>1) 
   { 
   sprintf(Str,"WARNING!! Crate#1 has %d LAMs in coinc. mode\nNormal is 1 LAM per crate",CLam); 
   Attention(-170,Str); 
   }
if ( (CLam==0) && (SLam==0) )
   { 
   sprintf(Str,"WARNING!! Crate#1 has no LAM enabled"); 
   Attention(-170,Str); 
   }
if (LLDWarning)
   { 
   sprintf(Str,"WARNING!! Such low values of LLD\n may jam AD413 ADCs"); 
   Attention(-270,Str); 
   }

if ((Fp=fopen(ClrName,"w")) == NULL) 
   { 
   sprintf(Str,"nice -n 10 crash \"Could not create %s\" \"Could be a file permissions problem\"&",ClrName);
   system(Str); exit(1);
   }  

if (Setup.Hardware.NCrates==1)
   {
   if (!Setup.Scaler.NSc)                      //If there are no scalers we can do a crate clear plus some needed commands
      {
      if (!PhillipsLam) fprintf(Fp,"CLRC\n");                 //The CLRC gives problems when Phillips 71xx has LAM enabled 
      for (i=0;i<MAX_CAMAC_STNS;++i)
          {
          if (Setup.Hardware.Modules[i] == 5)                                                              //Phillips 71xx
             {
             if (Setup.Hardware.Properties[i].AdcMode == 1)                                   //Phillips 71xx in test mode
                {
                fprintf(Fp,"NAF : %d 3 11          ;Reset Hit Register and Data Memory\n",i+1);
                fprintf(Fp,"NAF : %d 1 25          ;Test 1/3\n",i+1);
                }
             else
                {
                fprintf(Fp,"NAF : %d 3 11           ;Reset Hit Register and Data Memory\n",i+1);
                if (Setup.Hardware.Properties[i].AdcLam == Enabled)
                   fprintf(Fp,"NAF : %d 0 26          ;Enable Phillips 71xx LAM\n",i+1);
                                                                             //Lam is lost by CLRC we have to re-enable it
                }
             }
          if ( (Setup.Hardware.Modules[i] == 6) && (Setup.Hardware.Properties[i].AdcLam == Enabled) )           //CAEN TDC
             fprintf(Fp,"NAF : %d 0 26           ;Enable CAEN TDC LAM\n",i+1);                    //Re-enable CAEN TDC LAM
          }
      }
   else                                                    //When scalers are present we must clear all modules one-by-one
      {
      for (i=0;i<MAX_CAMAC_STNS;++i)                                    //First clear all modules where LAM is not enabled
          {
          if (Setup.Hardware.Properties[i].AdcLam == Disabled)
             {
             switch (Setup.Hardware.Modules[i])
                    {
                    case 1: fprintf(Fp,"NAF : %d 0 9            ;Clear Ortec AD413\n",i+1);                    //Ortec 413
                            break;
                    case 2: fprintf(Fp,"NAF : %d 12 11          ;Clear Ortec AD811\n",i+1);                    //Ortec 811
                            break;
                    case 3: fprintf(Fp,"NAF : %d 0 9            ;Clear LeCroy ADC/QDC\n",i+1);                    //LeCroy
                            break;
                    case 4: fprintf(Fp,"NAF : %d 12 11          ;Clear BARC CM60\n",i+1);                      //BARC CM60
                            break;
                    case 5:                                                                                 //Phillips71xx
                            if (!Setup.Hardware.CamacMode)       //In Q-Stop mode Phillips 71xx are cleared during readout
                                          fprintf(Fp,"NAF : %d 3 11           ;Reset Hit Register and Data Memory\n",i+1);
                            if (Setup.Hardware.Properties[i].AdcMode == 1)                     //Phillips TDC in test mode
                               fprintf(Fp,"NAF : %d 1 25          ;Test 1/3\n",i+1);
                            break;
                    case 6: fprintf(Fp,"NAF : %d 0 9            ;Clear CAEN TDC\n",i+1);                        //CAEN TDC
                            break;
                    case 7: fprintf(Fp,"NAF : %d 0 9            ;Clear Silena 4418/Q\n",i+1);              //Silena 4418/Q
                            break;
                    case 9: fprintf(Fp,"NAF : %d 12 9           ;Clear BARC CM88\n",i+1);                      //BARC CM88
                            break;
                    case 10://Insert a command here to clear an unknown module                                    //Unknown
                            break;
                    }
             }
          }
      for (i=0;i<MAX_CAMAC_STNS;++i)//Now clear modules where LAM is enabled, normally there will be only one such station
          {
          if (Setup.Hardware.Properties[i].AdcLam == Enabled)
             {
             switch (Setup.Hardware.Modules[i])
                 {
                 case 1: fprintf(Fp,"NAF : %d 0 9            ;Clear Ortec AD413\n",i+1);                       //Ortec 413
                         break;
                 case 2: fprintf(Fp,"NAF : %d 12 11          ;Clear Ortec AD811\n",i+1);                       //Ortec 811
                         break;
                 case 3: fprintf(Fp,"NAF : %d 0 9            ;Clear LeCroy ADC/QDC\n",i+1);                       //LeCroy
                         break;
                 case 4: fprintf(Fp,"NAF : %d 12 11          ;Clear BARC CM60\n",i+1);                         //BARC CM60
                         break;
                 case 5:                                                                                   //Phillips 71xx
                         if (!Setup.Hardware.CamacMode)          //In Q-Stop mode Phillips 71xx are cleared during readout
                                          fprintf(Fp,"NAF : %d 3 11           ;Reset Hit Register and Data Memory\n",i+1);
                         if (Setup.Hardware.Properties[i].AdcMode == 1)                       //Phillips 71xx in test mode
                            fprintf(Fp,"NAF : %d 1 25          ;Test 1/3\n",i+1);
                         break;
                 case 6: fprintf(Fp,"NAF : %d 0 9            ;Clear CAEN TDC\n",i+1);                           //CAEN TDC
                         fprintf(Fp,"NAF : %d 0 9            ;Enable LAM of CAEN TDC\n",i+1);              //Re-enable LAM
                         break;
                 case 7: fprintf(Fp,"NAF : %d 0 9            ;Clear Silena 4418/Q\n",i+1);                 //Silena 4418/Q
                         break;
                 case 9: fprintf(Fp,"NAF : %d 12 9           ;Clear BARC CM88\n",i+1);                         //BARC CM88
                         break;
                 case 10://Insert a comand here to clear an unknown module                                       //Unknown
                         break;
                 }
             }
          }
      if (Setup.Hardware.UseGtSup)                                                     //BARC CM90 Gate suppressor (if used)
         fprintf(Fp,"NAF : %d 0 9           ;Operate BARC CM90 gate suppressor\n",Setup.Hardware.GtSupStn);
      }
   }
else                            //2 Crate case. Here the clear is done through front panel with signal generated from CM90
   {
   fprintf(Fp,"NAF : 23 0 9 ;CM90 Synchronisation Module\n");
   for (i=0;i<MAX_CAMAC_STNS;++i)                          //Only for modules which dont have a front panel clear facility
       {
       switch (Setup.Hardware.Modules[i])
              {
              case 4: fprintf(Fp,"NAF : %d 12 11          ;Clear BARC CM60\n",i+1);      //BARC CM60: no front panel clear
                      break;
              }
       }
   }
fclose(Fp);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MakeIniClr2(gchar *FName)                         //Creates .ini and .clr files as FName2.ini, FName2.clr for Crate 2
{
FILE *Fp;
gchar Str[120],IniName[MAX_FNAME_LENGTH],ClrName[MAX_FNAME_LENGTH];
gint i,j,CLam,SLam;
gboolean LLDWarning,SilenaExists,AD413Exists;

if (index(FName,'.') == NULL) { sprintf(Str,"ERROR\nNo dot in %s",FName); Attention(0,Str); }
sprintf(IniName,"%s/%s",SetupDir,FName); sprintf(ClrName,"%s/%s",SetupDir,FName);
strcpy(rindex(IniName,'.'),"2.ini"); strcpy(rindex(ClrName,'.'),"2.clr");

if ((Fp=fopen(IniName,"w")) == NULL) 
   { 
   sprintf(Str,"nice -n 10 crash \"Could not create %s\" \"Could be a file permissions problem\"&",IniName);
   system(Str); exit(1);
   }  
CLam=SLam=0;                                             //Count Stns with Lam in coinc and singles mode to issue warnings
LLDWarning=FALSE;                                                       //If LLD is too low (for AD413) we issue a warning

fprintf(Fp,"CLRI   ;Clear Inhibit\n"); fprintf(Fp,"SETZ   ;Initialize Crate\n");            //Two commands at the begining

/*Insert commands for AD413 ADCs*/
AD413Exists=FALSE; for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i) if (Setup.Hardware.Modules[i] == 1) AD413Exists=TRUE;
if (AD413Exists)
   {
   fprintf(Fp,"SETI\n"); /* fprintf(Fp,"CLRC\n");                       Decided to do individual module clears 22-12-04 */
   for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
       {
       if (Setup.Hardware.Modules[i] == 1)  /*Ortec 413 ADC*/
          {
          fprintf(Fp,";--------------------------Commands for AD413--------------------------------\n");
          fprintf(Fp,"NAF : %d 0 9         ;Clear the ADC\n",i+1-MAX_CAMAC_STNS);
          if (Setup.Hardware.Properties[i].AdcMode == 1)                                                    //Singles mode
             { strcpy(Str,"DATA = 62208         ;62208 for Singles Mode,Lam enabled\n"); ++SLam; }
                                                                                //Singles mode. Here we assume LAM=Enabled
          else if (Setup.Hardware.Properties[i].AdcLam == Enabled)                              //Coinc. mode, LAM enabled
             { strcpy(Str,"DATA = 58112         ;58112 for Coinc mode Lam enabled\n"); ++CLam; }
          else
             strcpy(Str,"DATA = 41728         ;41728 for Coinc mode Lam disabled\n");          //Coinc. mode, LAM disabled
          fprintf(Fp,Str); fprintf(Fp,"NAF : %d 0 16        ;Control Register 1\n",i+1-MAX_CAMAC_STNS);
          if (Setup.Hardware.Properties[i].AdcMode == 1) 
             fprintf(Fp,"DATA = 16            ;B5=1 to disable master gate\n");    //For singles mode, disable master gate
          else                                                     
             fprintf(Fp,"DATA = 0             ;All gates enabled\n");             //For coincidence mode, enable all gates
          fprintf(Fp,"NAF : %d 1 16        ;Control Register 2\n",i+1-MAX_CAMAC_STNS);
          for (j=0;j<4;j++)                                                                     //For SubAddresses 0,1,2,3
              {
              fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].AdcLLD);
              fprintf(Fp,"NAF : %d %d 17        ;Set LLD of Ch%d (Default is 36*2=72mV)\n",i+1-MAX_CAMAC_STNS,j,j);
              if (Setup.Hardware.Properties[i].AdcLLD<30) LLDWarning=TRUE;
              }
          }
       }
   fprintf(Fp,";---------------------------------------------------------------------------\n");
   fprintf(Fp,"CLRI\n");
   }

/*Insert commands for Phillips 71xx*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 5)                                                                   //Phillips 71xx
       {
       if (Setup.Hardware.Properties[i].AdcMode == 1)                                                  //TDC in test mode
          {
          fprintf(Fp,"NAF : %d 0 26         ;Enable Phillips 71xx LAM\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"NAF : %d 3 11          ;Reset Hit Register and Data Memory\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"NAF : %d 1 25          ;Test 1/3\n",i+1-MAX_CAMAC_STNS);
          }
       else
          {
          if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
             { fprintf(Fp,"NAF : %d 0 26         ;Enable Phillips 71xx LAM\n",i+1-MAX_CAMAC_STNS); ++CLam; }
          fprintf(Fp,"NAF : %d 3 11          ;Reset Hit Register and Data Memory\n",i+1-MAX_CAMAC_STNS);
          }
       }
    }

/*Insert conmmands for LeCroy 3377 Multi-hit TDC*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
       if (Setup.Hardware.Modules[i] == 12)  /*LeCroy Multi-hit TDC*/
          {
          fprintf(Fp,";---------------------Commands for LeCroy 3377 Multi-hit TDC-----------------\n");
          fprintf(Fp,"NAF : %d 0 9        ;Clears all data and Lam\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"NAF : %d 0 30       ;Begin the programming sequence\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"NAF : %d 0 23       ;Select EPROM mode 3: Common start double word\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"NAF : %d 0 25       ;Begin Xilinx programming sequence\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"DLY = 1000          ;Wait for 1 sec (200 ms is required)\n");
          fprintf(Fp,"NAF : %d 0 13       ;Test Xlinx programming done\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"NAF : %d 0 9        ;Clears all data and Lam\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"DATA = 0            ;IDCode=0, rising edge, CAMAC, single buffer\n");
          fprintf(Fp,"NAF : %d 0 17       ;Control Register 0\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"DATA = 0            ;Data for control register 1\n");
          fprintf(Fp,"NAF : %d 1 17       ;Control Register 1\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"DATA = 1            ;Hits\n");
          fprintf(Fp,"NAF : %d 2 17       ;Control Register 2\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"DATA = 0            ;Request delay setting=0\n");
          fprintf(Fp,"NAF : %d 3 17       ;Control Register 3\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"DATA = 600          ;Timeout=600*50 ns=30 us (Effective full scale)\n");
          fprintf(Fp,"NAF : %d 4 17       ;Control Register 4\n",i+1-MAX_CAMAC_STNS);
          fprintf(Fp,"DATA = 0            ;No test mode\n");
          fprintf(Fp,"NAF : %d 5 17       ;Control Register 5\n",i+1-MAX_CAMAC_STNS);
          if (Setup.Hardware.Properties[i].AdcLam == Enabled)
             {
             CLam++;
             fprintf(Fp,"NAF : %d 0 26       ;Enable LAM\n",i+1-MAX_CAMAC_STNS);
             }
          fprintf(Fp,"NAF : %d 1 26       ;Enable Acquisition mode\n",i+1-MAX_CAMAC_STNS);
          }
    }

/*Insert commands for BARC CM60*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 4)  /*BARC CM60*/
       {
       for (j=0;j<4;j++)
           {
           fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].AdcLLD);
           fprintf(Fp,"NAF : %d %d 17        ;Set LLD of Ch%d\n",i+1-MAX_CAMAC_STNS,j,j);
           }
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 12 26          ;Enable CM60 LAM\n",i+1-MAX_CAMAC_STNS); ++CLam; }
       }
    }

/*Insert commands for ORTEC 811*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 2)  /*Ortec AD811*/
       {
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 12 26          ;Enable AD811 LAM\n",i+1-MAX_CAMAC_STNS); ++CLam; }
       }
    }

/*Insert commands for LeCroy ADC*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 3)  /*LeCroy ADC or QDC*/
       {
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 0 26           ;Enable LeCrory ADC/QDC LAM\n",i+1-MAX_CAMAC_STNS); ++CLam; }
       }
    }

/*Insert commands for CAEN TDC*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 6)  /*CAEN TDC*/
       {
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 0 26           ;Enable CAEN TDC LAM\n",i+1-MAX_CAMAC_STNS); ++CLam; }
       }
    }

/*Insert commands for Silena 4418/Q units*/
SilenaExists=FALSE; for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i) if (Setup.Hardware.Modules[i] == 7) SilenaExists=TRUE;
if (SilenaExists)
   {
   fprintf(Fp,"SETI\n"); fprintf(Fp,"CLRC\n");
   for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
       {
       if (Setup.Hardware.Modules[i] == 7)  //Silena 4418/Q
          {
          fprintf(Fp,";--------------------------Silena 4418/Q-------------------------------------\n");
          fprintf(Fp,"DATA = %d\n",MIN(255,Setup.Hardware.Properties[i].AdcLLD));
          fprintf(Fp,"NAF : %d 9 20         ;Set common threshold value (N.B. maximum is 255)\n",i+1-MAX_CAMAC_STNS); 
          if (Setup.Hardware.Properties[i].AdcLam == Enabled)
             { strcpy(Str,"DATA = 18944         ;18944 for Lam enabled\n"); ++CLam; }
          else
             strcpy(Str,"DATA = 2560          ;2560  for Lam disabled\n");
          fprintf(Fp,Str);
          fprintf(Fp,"NAF : %d 14 20         ;Status Register\n",i+1-MAX_CAMAC_STNS); 
          for (j=0;j<8;++j)
              {
              fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].SilenaOffset[j]);
              fprintf(Fp,"NAF : %d %d 20    ;Set Silena offset\n",i+1-MAX_CAMAC_STNS,j);
              }
          }
       }
   fprintf(Fp,";---------------------------------------------------------------------------\n");
   fprintf(Fp,"CLRI\n");
   }
/*Insert commands for BiRa 2351 Bit Registers*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 8) //BiRa bit register
       {
       fprintf(Fp,";--------------------------Silena 4418/Q-------------------------------------\n");
       if (Setup.Hardware.Properties[i].AdcLam == Enabled) 
          { fprintf(Fp,"NAF : %d 0 26         ;Enable LAM\n",i+1-MAX_CAMAC_STNS); ++CLam; }
       else                                                
          fprintf(Fp,";No commands needed\n");
       }
    }
/*Insert commands for BARC CM88 ADCs*/
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)
    {
    if (Setup.Hardware.Modules[i] == 9) //BARC CM88 ADC
       {
       fprintf(Fp,";--------------------------Commands for BARC CM88----------------------------\n");
/*This ADC has 4 modes: 0=Coincidence, 1=Coincidence with individual gates enabled
                        2=Singles      3=Singles with individual gates enabled     
  In modes 0 and 1 master gate is required and in modes 2 and 3 there is no master gate */
       if (Setup.Hardware.Properties[i].AdcMode == 0) strcpy(Str,"DATA = 9     ;Coin. no individual gates\n");
       if (Setup.Hardware.Properties[i].AdcMode == 1) strcpy(Str,"DATA = 25    ;Coin. + individual gates\n");
       if (Setup.Hardware.Properties[i].AdcMode == 2) strcpy(Str,"DATA = 12    ;Singles no individual gates\n");
       if (Setup.Hardware.Properties[i].AdcMode == 3) strcpy(Str,"DATA = 8     ;Sing. + individual gates\n");
       fprintf(Fp,Str);
       fprintf(Fp,"NAF : %d 0 16\n",i+1-MAX_CAMAC_STNS);
       fprintf(Fp,"DATA = %d\n",Setup.Hardware.Properties[i].AdcLLD);
       fprintf(Fp,"NAF : %d 0 17        ;Set LLD(mV)=2*number\n",i+1-MAX_CAMAC_STNS);
       if (Setup.Hardware.Properties[i].AdcLLD<30) LLDWarning=TRUE;
       fprintf(Fp,"DATA = 254\n");
       fprintf(Fp,"NAF : %d 1 17        ;Set ULD (0-255) to maximum\n",i+1-MAX_CAMAC_STNS);
       if (Setup.Hardware.Properties[i].AdcLam == Enabled)
          {
          fprintf(Fp,"NAF : %d 12 26         ;Enable LAM\n",i+1-MAX_CAMAC_STNS);
          if ( (Setup.Hardware.Properties[i].AdcMode == 0) || (Setup.Hardware.Properties[i].AdcMode == 1) ) ++CLam; 
          else ++SLam;
          }
       }
    }

fprintf(Fp,";---------------------------------------------------------------------------\n");
if (Setup.Hardware.NCrates==2) fprintf(Fp,"NAF : 23 0 9 ;Operate CM90 Sync. Module\n");
fprintf(Fp,"END\n");
fprintf(Fp,";---------------------------------------------------------------------------\n");
fprintf(Fp,";The folowing keywords are used in these files:\n");
fprintf(Fp,";SETZ (Camac Z), CLRC (Camac C), SETI (Set Inhibit), CLRI (Clear Inhibit),\n");
fprintf(Fp,";DATA = <decimal number>, NAF : <N> <A> <F> (Camac NAF command)\n");
fprintf(Fp,";DLY = <decimal number=Delay in milliseconds>\n");
fprintf(Fp,";The END at the end is not compulsory\n");
fprintf(Fp,";Comments can be put after a semicolon\n");
fprintf(Fp,";---------------------------------------------------------------------------\n");
fclose(Fp);

/*Issue warnings*/
if (Setup.Hardware.NCrates==2)
   {
   if (CLam>1)     
      {
      sprintf(Str,"WARNING!! Crate#2 has %d LAMs in coinc. mode\nNormal is 1 LAM per crate",CLam);
      Attention(270,Str);
      }
   if ( (CLam==0) && (SLam==0) )
      {
      sprintf(Str,"WARNING!! Crate#2 has no LAM enabled");                                           
      Attention(270,Str);
      }

   if (LLDWarning)
      { 
      sprintf(Str,"WARNING!! Such low values of LLD\n may jam AD413 ADCs"); 
      Attention(-370,Str); 
      }
   }

if ((Fp=fopen(ClrName,"w")) == NULL) 
   { 
   sprintf(Str,"nice -n 10 crash \"Could not create %s\" \"Could be a file permissions problem\"&",ClrName);
   system(Str); exit(1);
   }  

//MakeIniClr2 is only for the second crate in a two crate setup, so we need only the CM90 command
fprintf(Fp,"NAF : 23 0 9 ;CM90 Synchronisation Module\n"); 
for (i=MAX_CAMAC_STNS;i<2*MAX_CAMAC_STNS;++i)           //Only for modules which dont have a front panel clear facility
    {
    switch (Setup.Hardware.Modules[i])
           {
           case 4: fprintf(Fp,"NAF : %d 12 11          ;Clear BARC CM60\n",i+1-MAX_CAMAC_STNS); //BARC CM60: no f.p. clear
                   break;
           }
    }

fclose(Fp);
}
/*----------------------------------------------------------------------------------------------------------------------*/
