#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lamps.h"

/*Function templates*/
void Attention(gint XPos,gchar *Messg);
void ParseTextToInt(gchar *TextBuf,gint From,gint n,gint *Val,gint *ToHere);
void CheckSetup(void);
void ParseTextToStr(gchar *TextBuf,gint From,gchar *OutText,gint *ToHere);
void ReadS(GtkWidget *W,gpointer Unused);
void YesOverwrite(GtkWidget *W,GtkWidget *Win); void SaveAsS(GtkWidget *W,gpointer Unused); 
void ReadSetup(GtkWidget *W,gpointer Unused); void SaveSetup(GtkWidget *W,gpointer Unused); 
void SaveAs(GtkWidget *W,gpointer Unused); 
void Save(gint Opt);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void SavePrefs(); void ParameterList(GtkWidget *W,gpointer Data);
/* Global variables */
extern enum ProgramState ProgramState;
extern struct Setup Setup;
extern struct PsBGated PsBGated[MAX_PSEUDO];                                                  //Banana gated pseudo type
extern struct FileSelectType *FileX;
extern gchar SetupDir[MAX_DIR_STRLEN];                                                                 //Directory prefs
extern gint TopOfset;                                         //Accounts for space occupied by window manager at the top
//----------------------------------------------------------------------------------------------------------------------
gint Read(gchar *ErrMessg,gint Opt)    //Does the actual file reading. Can be called from a thread outside gtk_main_loop
//Opt is normally 0. When Opt=1 we read ".lamps_set
{
FILE *Fp;
gint i,j,Value[20],ToHere,L;
gchar Line[200],Str[200],Str1[200];
static gchar Head[18][85] = { "------------------------ListMode Setup starts here------------------------------",
                              "------------------------ListMode Setup end here---------------------------------",
                              "------------------------Hardware Setup starts here------------------------------",
                              "------------------------Hardware Setup ends here--------------------------------",
                              "------------------------Spectra Setup starts here-------------------------------",
                              "------------------------Spectra Setup ends here---------------------------------",
                              "------------------------Oned Setup starts here----------------------------------",
                              "------------------------Oned Setup ends here------------------------------------",
                              "------------------------Twod Setup starts here----------------------------------",
                              "------------------------Twod Setup ends here------------------------------------",
                              "--Pseudo (0=Sum,1=Product,2=Ratio,3=Position,4=PI,5=Multiplicty,6=User,7=Array)-",
                              "------------------------Pseudo Setup ends here----------------------------------",
                              "------------------------Scaler Setup starts here--------------------------------",
                              "------------------------Scaler Setup ends here----------------------------------",
                              "------------------------Periodic Log Setup starts here--------------------------",
                              "------------------------Periodic Log Setup ends here----------------------------",
                              "------------------------Macro Setup starts here---------------------------------",
                              "------------------------Macro Setup ends here-----------------------------------"};

if (Opt==0) { if ((Fp=fopen(Setup.FName,"r"))==NULL)  { strcpy(ErrMessg,"Could not find setup file"); return 1; }}
else        { if ((Fp=fopen(".lamps_set","r"))==NULL) { strcpy(ErrMessg,"Could not find setup file"); return 1; }}
//----------------------------------------------ListMode Setup-------------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[0],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading ListMode Setup absent"); return 1; }

if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.ListMode.ListOn"); return 1; }
Setup.ListMode.ListOn=atoi(&Line[41]); //g_print("Setup.ListMode.ListOn=%d\n",Setup.ListMode.ListOn);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.ListMode.Compr"); return 1; }
Setup.ListMode.Compr=atoi(&Line[41]); //g_print("Setup.ListMode.Compr=%d\n",Setup.ListMode.Compr);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.ListMode.Rate"); return 1; }
Setup.ListMode.Rate=atoi(&Line[41]); //g_print("Setup.ListMode.Rate=%d\n",Setup.ListMode.Rate);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.ListMode.NoOfLGates"); return 1; }
Setup.ListMode.NoOfLGates=atoi(&Line[41]); //g_print("Setup.ListMode.NoOfLGates=%d\n",Setup.ListMode.NoOfLGates);
for (i=0;i<Setup.ListMode.NoOfLGates;i++)
    {
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.ListMode.LGates[%d]",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToInt(&Line[41],0,3,Value,&ToHere);
    Setup.ListMode.LGates[i].Para=Value[0]; 
    //g_print("Setup.ListMode.LGates[%d].Para=%d\n",i+1,Setup.ListMode.LGates[i].Para);
    Setup.ListMode.LGates[i].Lo=Value[1]; //g_print("Setup.ListMode.LGates[%d].Lo=%d\n",i+1,Setup.ListMode.LGates[i].Lo);
    Setup.ListMode.LGates[i].Hi=Value[2]; //g_print("Setup.ListMode.LGates[%d].Hi=%d\n",i+1,Setup.ListMode.LGates[i].Hi);
    }

if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[1],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nListMode Setup end absent"); return 1; }
//------------------------------------------------Hardware Setup-----------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[2],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Hardware Setup absent"); return 1; }
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { sprintf(Str,"Error reading setupfile\nSetup.Hardware.NCrates"); strcpy(ErrMessg,Str); return 1; }
ParseTextToInt(&Line[41],0,1,Value,&ToHere); Setup.Hardware.NCrates=Value[0];
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { sprintf(Str,"Error reading setupfile\nSetup.Hardware.CamacMode"); strcpy(ErrMessg,Str); return 1; }
ParseTextToInt(&Line[41],0,1,Value,&ToHere); Setup.Hardware.CamacMode=Value[0];
    //g_print("Setup.Hardware.CamacMode=%d\n",Setup.Hardware.CamacMode);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { sprintf(Str,"Error reading setupfile\nSetup.Hardware.UseGtSup"); strcpy(ErrMessg,Str); return 1; }
ParseTextToInt(&Line[41],0,1,Value,&ToHere); Setup.Hardware.UseGtSup=Value[0];
    //g_print("Setup.Hardware.UseGtSup=%d\n",Setup.Hardware.UseGtSup);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { sprintf(Str,"Error reading setupfile\nSetup.Hardware.GtSupStn"); strcpy(ErrMessg,Str); return 1; }
ParseTextToInt(&Line[41],0,1,Value,&ToHere); Setup.Hardware.GtSupStn=Value[0];
    //g_print("Setup.Hardware.GtSupStn=%d\n",Setup.Hardware.GtSupStn);
for (i=0;i<2*MAX_CAMAC_STNS;i++)
    {
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Hardware.Modules[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToInt(&Line[41],0,6,Value,&ToHere);
    Setup.Hardware.Modules[i]=Value[0]; //g_print("Setup.Hardware.Modules[%d]=%d\n",i+1,Setup.Hardware.Modules[i]);
    Setup.Hardware.Properties[i].AdcLam=Value[1]; 
    //g_print("Setup.Hardware.Properties[%d].AdcLam=%d\n",i+1,Setup.Hardware.Properties[i].AdcLam);
    Setup.Hardware.Properties[i].AdcMode=Value[2];
    //g_print("Setup.Hardware.Properties[%d].AdcMode=%d\n",i+1,Setup.Hardware.Properties[i].AdcMode);
    Setup.Hardware.Properties[i].AdcLLD=Value[3];
    //g_print("Setup.Hardware.Properties[%d].AdcLLD=%d\n",i+1,Setup.Hardware.Properties[i].AdcLLD);
    Setup.Hardware.Properties[i].AdcFCode=Value[4];
    //g_print("Setup.Hardware.Properties[%d].AdcFCode=%d\n",i+1,Setup.Hardware.Properties[i].AdcFCode);
    Setup.Hardware.Properties[i].AdcGain=Value[5];
    //g_print("Setup.Hardware.Properties[%d].AdcGain=%d\n",i+1,Setup.Hardware.Properties[i].AdcGain);
    if (Setup.Hardware.Modules[i]==7)                                                                    //Silena Offset values
       {
       if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
          { sprintf(Str,"Error reading setupfile\nSilena Offsets Stn=%d",i+1); strcpy(ErrMessg,Str); return 1; }
       ParseTextToInt(&Line[41],0,8,Value,&ToHere); 
       for (j=0;j<8;++j) Setup.Hardware.Properties[i].SilenaOffset[j]=Value[j];
       }

    if ( (Setup.Hardware.Modules[i]==7) || (Setup.Hardware.Modules[i]==5) )       //Silena and Phillips Lower Threshold Values
       {
       if ( (fgets(Line,150,Fp)==NULL) || Line[40] != '=')
          { sprintf(Str,"Error reading setupfile\nLower Thresholds Stn=%d",i+1); strcpy(ErrMessg,Str); return 1; }
       ParseTextToInt(&Line[41],0,16,Value,&ToHere); 
       for (j=0;j<16;++j) Setup.Hardware.Properties[i].LowerThreshold[j]=Value[j];
       if ( (fgets(Line,150,Fp)==NULL) || Line[40] != '=')
          { sprintf(Str,"Error reading setupfile\nUpper Thresholds Stn=%d",i+1); strcpy(ErrMessg,Str); return 1; }
       ParseTextToInt(&Line[41],0,16,Value,&ToHere); 
       for (j=0;j<16;++j) Setup.Hardware.Properties[i].UpperThreshold[j]=Value[j];
       }

    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Harwdare.Paras[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToStr(&Line[41],0,Str1,&ToHere); L=strlen(Str1); 
    if ( (Str1[0] != '<') || (Str1[L-1] != '>') )
       { sprintf(Str,"Error reading setupfile\nSetup.Harwdare.Paras[%d] missing <>",i+1); strcpy(ErrMessg,Str); return 1; }
    strncpy(Setup.Hardware.Paras[i],&Str1[1],MAX_TEXT_FIELD); Setup.Hardware.Paras[i][L-2]='\0';
    //g_print("Setup.Hardware.Paras[%d]=[%s]\n",i+1,Setup.Hardware.Paras[i]);
    ParseTextToStr(&Line[41],ToHere,Str1,&ToHere); L=strlen(Str1);
    if ( (Str1[0] != '<') || (Str1[L-1] != '>') )
       { 
       sprintf(Str,"Error reading setupfile\nSetup.Hardware.SubAddr[%d] missing <>",i+1); 
       strcpy(ErrMessg,Str); return 1; 
       }
    strncpy(Setup.Hardware.SubAddr[i],&Str1[1],MAX_TEXT_FIELD); Setup.Hardware.SubAddr[i][L-2]='\0';
    //g_print("Setup.Hardware.SubAddr[%d]=<%s>\n",i+1,Setup.Hardware.SubAddr[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Hardware.ParaNames[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToStr(&Line[41],0,Str1,&ToHere); L=strlen(Str1);
    if ( (Str1[0] != '<') || (Str1[L-1] != '>') )
       { 
       sprintf(Str,"Error reading setupfile\nSetup.Hardware.ParaNames[%d] missing <>",i+1); 
       strcpy(ErrMessg,Str); return 1; 
       }
    strncpy(Setup.Hardware.ParaNames[i],&Str1[1],MAX_TEXT_FIELD); Setup.Hardware.ParaNames[i][L-2]='\0';
    //g_print("Setup.Hardware.ParaNames[%d]=<%s>\n",i+1,Setup.Hardware.ParaNames[i]);
    ParseTextToInt(&Line[41],ToHere,2,Value,&ToHere);
    Setup.Hardware.ZSupLLD[i]=Value[0]; //g_print("Setup.Hardware.ZSupLLD[%d]=%d\n",i+1,Setup.Hardware.ZSupLLD[i]);
    Setup.Hardware.ZSupULD[i]=Value[1]; //g_print("Setup.Hardware.ZSupULD[%d]=%d\n",i+1,Setup.Hardware.ZSupULD[i]);
    }
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[3],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHardware Setup end absent"); return 1; }
//-------------------------------------------------Spectra Setup----------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[4],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Spectra Setup absent"); return 1; }

if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Spectra.Safety"); return 1; }
Setup.Spectra.Safety=atoi(&Line[41]); //g_print("Setup.Spectra.Safety=%d\n",Setup.Spectra.Safety);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Spectra.SafetyTime"); return 1; }
Setup.Spectra.SafetyTime=atoi(&Line[41]); //g_print("Setup.Spectra.SafetyTime=%d\n",Setup.Spectra.SafetyTime);
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[5],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nSpectra Setup end absent"); return 1; }
//---------------------------------------------------Oned Setup-----------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[6],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Oned Setup absent"); return 1; }

if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Oned.N"); return 1; }
Setup.Oned.N=atoi(&Line[41]); //g_print("Setup.Oned.N=%d\n",Setup.Oned.N);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Oned.WSz"); return 1; }
Setup.Oned.WSz=atoi(&Line[41]); //g_print("Setup.Oned.WSz=%d\n",Setup.Oned.WSz);
for (i=0;i<Setup.Oned.N;i++)
    {
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Oned.Par[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToInt(&Line[41],0,3,Value,&ToHere);
    Setup.Oned.Par[i]=Value[0]; //g_print("Setup.Oned.Par[%d]=%d\n",i+1,Setup.Oned.Par[i]);
    Setup.Oned.NPar[i]=Value[1]; //g_print("Setup.Oned.NPar[%d]=%d\n",i+1,Setup.NPar.Chan[i]);
    Setup.Oned.Chan[i]=Value[2]; //g_print("Setup.Oned.Chan[%d]=%d\n",i+1,Setup.Oned.Chan[i]);
    if (Line[41+ToHere])
       { strncpy(Setup.Oned.Name[i],&Line[41+ToHere],MAX_TEXT_FIELD-1); Setup.Oned.Name[i][MAX_TEXT_FIELD-1]='\0';
         if (strlen(Setup.Oned.Name[i])>0 && Setup.Oned.Name[i][strlen(Setup.Oned.Name[i])-1]=='\n') Setup.Oned.Name[i][strlen(Setup.Oned.Name[i])-1]='\0'; }
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Oned.Gate1[%d].NGates etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToInt(&Line[41],0,4,Value,&ToHere);
    Setup.Oned.Gate1[i].NGates=Value[0]; //g_print("Setup.Oned.Gate1[%d].NGates=%d\n",i+1,Setup.Oned.Gate1[i].NGates);
    Setup.Oned.Gate1[i].Cond=Value[1]; //g_print("Setup.Oned.Gate1[%d].Cond=%d\n",i+1,Setup.Oned.Gate1[i].Cond);
    Setup.Oned.Gate2[i].NGates=Value[2]; //g_print("Setup.Oned.Gate2[%d].NGates=%d\n",i+1,Setup.Oned.Gate2[i].NGates);
    Setup.Oned.Gate2[i].Cond=Value[3]; //g_print("Setup.Oned.Gate2[%d].Cond=%d\n",i+1,Setup.Oned.Gate2[i].Cond);
    for (j=0;j<Setup.Oned.Gate1[i].NGates;j++)
        {
        if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
           {
           sprintf(Str,"Error reading setupfile\nOned.Gate1[%d].Gate1d[%d] etc",i+1,j+1);
           strcpy(ErrMessg,Str); return 1;
           }
        ParseTextToInt(&Line[41],0,3,Value,&ToHere);
        if (Line[41+ToHere])
           { strcpy(Setup.Twod.Name[i],&Line[41+ToHere]); Setup.Twod.Name[i][strlen(Setup.Twod.Name[i])-1]='\0'; }
        Setup.Oned.Gate1[i].Gate1d[j].Para=Value[0];
        //g_print("Setup.Oned.Gate1[%d].Gate1d[%d].Para=%d\n",i+1,j+1,Setup.Oned.Gate1[i].Gate1d[j].Para);
        Setup.Oned.Gate1[i].Gate1d[j].Lo=Value[1];
        //g_print("Setup.Oned.Gate1[%d].Gate1d[%d].Lo=%d\n",i+1,j+1,Setup.Oned.Gate1[i].Gate1d[j].Lo);
        Setup.Oned.Gate1[i].Gate1d[j].Hi=Value[2];
        //g_print("Setup.Oned.Gate1[%d].Gate1d[%d].Hi=%d\n",i+1,j+1,Setup.Oned.Gate1[i].Gate1d[j].Hi);
        }

    for (j=0;j<Setup.Oned.Gate2[i].NGates;j++)
        {
        if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
           {
           sprintf(Str,"Error reading setupfile\nOned.Gate2[%d].Gate2d[%d] etc",i+1,j+1);
           strcpy(ErrMessg,Str); return 1;
           }
        ParseTextToStr(&Line[41],0,Str1,&ToHere); L=strlen(Str1);
        if ( (Str1[0] != '<') || (Str1[L-1] != '>') )
           {
           sprintf(Str,"Error reading setupfile\nOned.Gate2[%d].Gate2d[%d] missing <>",i+1,j+1);
           strcpy(ErrMessg,Str); return 1;
           }
        strncpy(Setup.Oned.Gate2[i].Gate2d[j],&Str1[1],LONG_TEXT_FIELD); Setup.Oned.Gate2[i].Gate2d[j][L-2]='\0';
        //g_print("Setup.Oned.Gate2[%d].Gate2d[%d]=[%s]\n",i+1,j+1,Setup.Oned.Gate2[i].Gate2d[j]);
        }
    }

if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[7],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nOned Setup end absent"); return 1; }
//---------------------------------------------------Twod Setup-----------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[8],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Spectra Setup absent"); return 1; }

if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Twod.N"); return 1; }
Setup.Twod.N=atoi(&Line[41]); //g_print("Setup.Twod.N=%d\n",Setup.Twod.N);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Twod.WSz"); return 1; }
Setup.Twod.WSz=atoi(&Line[41]); //g_print("Setup.Twod.WSz=%d\n",Setup.Twod.WSz);
for (i=0;i<Setup.Twod.N;i++)
    {
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Twod.XPar[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToInt(&Line[41],0,6,Value,&ToHere);
    Setup.Twod.XPar[i]=Value[0]; //g_print("Setup.Twod.XPar[%d]=%d\n",i+1,Setup.Twod.XPar[i]);
    Setup.Twod.NXPar[i]=Value[1]; //g_print("Setup.Twod.NXPar[%d]=%d\n",i+1,Setup.Twod.NXPar[i]);
    Setup.Twod.YPar[i]=Value[2]; //g_print("Setup.Twod.YPar[%d]=%d\n",i+1,Setup.Twod.YPar[i]);
    Setup.Twod.NYPar[i]=Value[3]; //g_print("Setup.Twod.NYPar[%d]=%d\n",i+1,Setup.Twod.NYPar[i]);
    Setup.Twod.XChan[i]=Value[4]; //g_print("Setup.Twod.XChan[%d]=%d\n",i+1,Setup.Twod.XChan[i]);
    Setup.Twod.YChan[i]=Value[5]; //g_print("Setup.Twod.YChan[%d]=%d\n",i+1,Setup.Twod.YChan[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Twod.Gate1[%d].NGates etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToInt(&Line[41],0,4,Value,&ToHere);
    Setup.Twod.Gate1[i].NGates=Value[0]; //g_print("Setup.Twod.Gate1[%d].NGates=%d\n",i+1,Setup.Twod.Gate1[i].NGates);
    Setup.Twod.Gate1[i].Cond=Value[1]; //g_print("Setup.Twod.Gate1[%d].Cond=%d\n",i+1,Setup.Twod.Gate1[i].Cond);
    Setup.Twod.Gate2[i].NGates=Value[2]; //g_print("Setup.Twod.Gate2[%d].NGates=%d\n",i+1,Setup.Twod.Gate2[i].NGates);
    Setup.Twod.Gate2[i].Cond=Value[3]; //g_print("Setup.Twod.Gate2[%d].Cond=%d\n",i+1,Setup.Twod.Gate2[i].Cond);
    for (j=0;j<Setup.Twod.Gate1[i].NGates;j++)
        {
        if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
           { sprintf(Str,"Error reading setupfile\nTwod.Gate1[%d].Gate1d[%d] etc",i+1,j+1); strcpy(ErrMessg,Str); return 1; }
        ParseTextToInt(&Line[41],0,3,Value,&ToHere);
        Setup.Twod.Gate1[i].Gate1d[j].Para=Value[0];
        //g_print("Setup.Twod.Gate1[%d].Gate1d[%d].Para=%d\n",i+1,j+1,Setup.Twod.Gate1[i].Gate1d[j].Para);
        Setup.Twod.Gate1[i].Gate1d[j].Lo=Value[1];
        //g_print("Setup.Twod.Gate1[%d].Gate1d[%d].Lo=%d\n",i+1,j+1,Setup.Twod.Gate1[i].Gate1d[j].Lo);
        Setup.Twod.Gate1[i].Gate1d[j].Hi=Value[2];
        //g_print("Setup.Twod.Gate1[%d].Gate1d[%d].Hi=%d\n",i+1,j+1,Setup.Twod.Gate1[i].Gate1d[j].Hi);
        }
    for (j=0;j<Setup.Twod.Gate2[i].NGates;j++)
        {
        if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
           { sprintf(Str,"Error reading setupfile\nTwod.Gate2[%d].Gate2d[%d] etc",i+1,j+1); strcpy(ErrMessg,Str); return 1; }
        ParseTextToStr(&Line[41],0,Str1,&ToHere); L=strlen(Str1);
        if ( (Str1[0] != '<') || (Str1[L-1] != '>') )
           { sprintf(Str,"Error reading setupfile\nTwod.Gate2[%d].Gate2d[%d] missing <>",i+1,j+1); strcpy(ErrMessg,Str); return 1; }
        strncpy(Setup.Twod.Gate2[i].Gate2d[j],&Str1[1],LONG_TEXT_FIELD); Setup.Twod.Gate2[i].Gate2d[j][L-2]='\0';
        //g_print("Setup.Twod.Gate2[%d].Gate2d[%d]=[%s]\n",i+1,j+1,Setup.Twod.Gate2[i].Gate2d[j]);
        }
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
           { sprintf(Str,"Error reading setupfile\nSetup.Twod.CDdet.Enabled"); strcpy(ErrMessg,Str); return 1; }
    Setup.Twod.CDdet[i].Enabled=atoi(&Line[41]);
    /*Uncomment this part for CD detector (see glamps)
    if (Setup.Twod.CDdet[i].Enabled)
       {
       for (j=0;j<8;++j)
           {
           if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
              { sprintf(Str,"Error reading setupfile\nSetup.Twod.CDdet.Parameter %d",j); strcpy(ErrMessg,Str); return 1; }
           Setup.Twod.CDdet[i].P[j]=atoi(&Line[41]);
           }
       }
    */

    }
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[9],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nTwod Setup end absent"); return 1; }
//---------------------------------------------------Pseudo Setup---------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[10],75) )
    { strcpy(ErrMessg,"Error reading setupfile\nHeading Pseudo Setup absent"); return 1; }

if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Pseudo.N"); return 1; }
Setup.Pseudo.N=atoi(&Line[41]); //g_print("Setup.Pseudo.N=%d\n",Setup.Pseudo.N);

for (i=0;i<Setup.Pseudo.N;++i)
    {
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=') 
       { sprintf(Str,"Error reading setupfile\nSetup.Pseudo.P1[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToInt(&Line[41],0,4,Value,&ToHere);
    Setup.Pseudo.P1[i]=Value[0]; //g_print("Setup.Pseudo.P1[%d]=%d\n",i+1,Setup.Pseudo.P1[i]);
    Setup.Pseudo.P2[i]=Value[1]; //g_print("Setup.Pseudo.P2[%d]=%d\n",i+1,Setup.Pseudo.P2[i]);
    Setup.Pseudo.Size[i]=Value[2]; //g_print("Setup.Pseudo.Size[%d]=%d\n",i+1,Setup.Pseudo.Size[i]);
    Setup.Pseudo.Type[i]=Value[3]; //g_print("Setup.Pseudo.Type[%d]=%d\n",i+1,Setup.Pseudo.Type[i]);
    if (Line[41+ToHere]) { strcpy(Setup.Pseudo.Name[i],&Line[41+ToHere]); Setup.Pseudo.Name[i][strlen(Setup.Pseudo.Name[i])-1]='\0'; }

    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Pseudo.K1[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    sscanf(&Line[41],"%g %g %g %g %g %g",&Setup.Pseudo.K1[i],&Setup.Pseudo.O1[i],&Setup.Pseudo.K2[i],&Setup.Pseudo.O2[i],
                                         &Setup.Pseudo.K3[i],&Setup.Pseudo.O3[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Pseudo.Power[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    sscanf(&Line[41],"%g %d %d",&Setup.Pseudo.Power[i],&Setup.Pseudo.L1[i],&Setup.Pseudo.L2[i]);
    if (Setup.Pseudo.Type[i] == Array)
       {
       if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
          { sprintf(Str,"Error reading setupfile\nSetup.Pseudo.ArrayN[%d]",i+1); strcpy(ErrMessg,Str); return 1; }
       sscanf(&Line[41],"%d",&Setup.Pseudo.ArrayN[i]); //g_print("Setup.Pseudo.ArrayN[%d]=%d\n",i+1,Setup.Pseudo.ArrayN[i]);
       for (j=0;j<Setup.Pseudo.ArrayN[i];++j)
           {
           if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
              { sprintf(Str,"Error reading setupfile\nSetup.Pseudo.ArrayPar etc[%d]",i+1); strcpy(ErrMessg,Str); return 1; }
           sscanf(&Line[41],"%d %d %d",&Setup.Pseudo.ArrayPar[j][i],&Setup.Pseudo.ArrayLLD[j][i],&Setup.Pseudo.ArrayULD[j][i]);
           //g_print("Par,LLD,ULD[%d]=%d %d %d\n",i+1,Setup.Pseudo.ArrayPar[j][i],Setup.Pseudo.ArrayLLD[j][i],Setup.Pseudo.ArrayULD[j][i]);
           if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
              { sprintf(Str,"Error reading setupfile\nSetup.Pseudo.ArrayOffset etc[%d]",i+1); strcpy(ErrMessg,Str); return 1; }
           sscanf(&Line[41],"%g %g %g",&Setup.Pseudo.ArrayOffset[j][i],&Setup.Pseudo.ArraySlope[j][i],&Setup.Pseudo.ArrayQuad[j][i]);
           //g_print("Offs,Slope,Quad[%d]=%g %g %g\n",i+1,Setup.Pseudo.ArrayOffset[j][i],Setup.Pseudo.ArraySlope[j][i],
           //        Setup.Pseudo.ArrayQuad[j][i]);
           }
       }
    if (Setup.Pseudo.Type[i] == BGated)
       {
       if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
          { sprintf(Str,"Error reading setupfile\nBananaFile name for Pseudo %d",i+1); strcpy(ErrMessg,Str); return 1; }
       ParseTextToStr(&Line[41],0,Str1,&ToHere); L=strlen(Str1); 
       if ( (Str1[0] != '<') || (Str1[L-1] != '>') )
          { 
          sprintf(Str,"Error reading setupfile\nPs[%d] BGate file name missing <>",i+1); 
          strcpy(ErrMessg,Str); return 1; 
          }
        strncpy(PsBGated[i].Name,&Str1[1],LONG_TEXT_FIELD); PsBGated[i].Name[L-2]='\0';
        //g_print("PsBGated[%d].Name=[%s]\n",i+1,PsBGated[i].Name);
       }
    }

if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[11],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nPseudo Setup end absent"); return 1; }
//---------------------------------------------------Scaler Setup---------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[12],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Scaler Setup absent"); return 1; }

if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Scaler.NSc"); return 1; }
Setup.Scaler.NSc=atoi(&Line[41]); //g_print("Setup.Scaler.NSc=%d\n",Setup.Scaler.NSc);

for (i=0;i<Setup.Scaler.NSc;i++)
    {
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { sprintf(Str,"Error reading setupfile\nSetup.Scaler.Name[%d] etc",i+1); strcpy(ErrMessg,Str); return 1; }
    ParseTextToStr(&Line[41],0,Str1,&ToHere); L=strlen(Str1);
    if ( (Str1[0] != '<') || (Str1[L-1] != '>') )
       { sprintf(Str,"Error reading setupfile\nSetup.Scaler.Name[%d] missing <>",i+1); strcpy(ErrMessg,Str); return 1; }
    strncpy(Setup.Scaler.Name[i],&Str1[1],MAX_TEXT_FIELD); Setup.Scaler.Name[i][L-2]='\0';
    //g_print("Setup.Scaler.Name[%d]=<%s>\n",i+1,Setup.Scaler.Name[i]);
    ParseTextToInt(&Line[41],ToHere,4,Value,&ToHere);
    Setup.Scaler.C[i]=Value[0]; //g_print("Setup.Scaler.N[%d]=%d\n",i+1,Setup.Scaler.N[i]);
    Setup.Scaler.N[i]=Value[1]; //g_print("Setup.Scaler.N[%d]=%d\n",i+1,Setup.Scaler.N[i]);
    Setup.Scaler.A[i]=Value[2]; //g_print("Setup.Scaler.A[%d]=%d\n",i+1,Setup.Scaler.A[i]);
    Setup.Scaler.F[i]=Value[3]; //g_print("Setup.Scaler.F[%d]=%d\n",i+1,Setup.Scaler.F[i]);
    }

if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[13],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nScaler Setup end absent"); return 1; }

//---------------------------------------------------Periodic Log Setup---------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[14],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Periodic Log Setup absent"); return 1; }
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.PLogSafety.On"); return 1; }
Setup.PLogSetup.On=atoi(&Line[41]); //g_print("Setup.PLogSetup.On=%d\n",Setup.PLogSetup.On);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.PLogSetup.BufCount"); return 1; }
Setup.PLogSetup.BufCount=atoi(&Line[41]); //g_print("Setup.PLogSetup.BufCount=%d\n",Setup.PLogSetup.BufCount);
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[15],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nPeriodic Log Setup end absent"); return 1; }

//------------------------------------------------------Macro Setup------------------------------------------------------
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[16],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Macro Setup absent"); return 1; }
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.N absent"); return 1; }
Setup.Macro.N=atoi(&Line[41]); //g_print("Setup.Macro.N=%d\n",Setup.Macro.N);
if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.RefreshRate"); return 1; }
Setup.Macro.RefreshRate=atoi(&Line[41]); //g_print("Setup.Macro.BufRefreshRate=%d\n",Setup.Macro.RefreshRate);
for (i=0;i<Setup.Macro.N;i++)
    {
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.Name absent"); return 1; }
    strncpy(Setup.Macro.Name[i],&Line[41],MAX_TEXT_FIELD+1); Setup.Macro.Name[i][MAX_TEXT_FIELD+1]='\0';
    if (strlen(Setup.Macro.Name[i])>0 && Setup.Macro.Name[i][strlen(Setup.Macro.Name[i])-1]=='\n') Setup.Macro.Name[i][strlen(Setup.Macro.Name[i])-1]='\0';
    //g_print("Setup.Macro.Name=%s\n",Setup.Macro.Name[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.Type absent"); return 1; }
    Setup.Macro.Type[i]=atoi(&Line[41]); //g_print("Setup.Macro.Type=%d\n",Setup.Macro.Type[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.Display absent"); return 1; }
    Setup.Macro.Display[i]=atoi(&Line[41]); //g_print("Setup.Macro.Display=%d\n",Setup.Macro.Display[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.SpecNo absent"); return 1; }
    Setup.Macro.SpecNo[i]=atoi(&Line[41]); //g_print("Setup.Macro.SpecNo=%d\n",Setup.Macro.SpecNo[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.XMin absent"); return 1; }
    Setup.Macro.XMin[i]=atoi(&Line[41]); //g_print("Setup.Macro.XMin=%d\n",Setup.Macro.XMin[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.XMax absent"); return 1; }
    Setup.Macro.XMax[i]=atoi(&Line[41]); //g_print("Setup.Macro.XMax=%d\n",Setup.Macro.XMax[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.YMin absent"); return 1; }
    Setup.Macro.YMin[i]=atoi(&Line[41]); //g_print("Setup.Macro.YMin=%d\n",Setup.Macro.YMin[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.YMax absent"); return 1; }
    Setup.Macro.YMax[i]=atoi(&Line[41]); //g_print("Setup.Macro.YMax=%d\n",Setup.Macro.YMax[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.K1 absent"); return 1; }
    Setup.Macro.K1[i]=atof(&Line[41]); //g_print("Setup.Macro.K1=%f\n",Setup.Macro.K1[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.K2 absent"); return 1; }
    Setup.Macro.K2[i]=atof(&Line[41]); //g_print("Setup.Macro.K2=%f\n",Setup.Macro.K2[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.ScalerNo absent"); return 1; }
    Setup.Macro.ScalerNo[i]=atoi(&Line[41]); //g_print("Setup.Macro.ScalerNo=%d\n",Setup.Macro.ScalerNo[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.Index1 absent"); return 1; }
    Setup.Macro.Index1[i]=atoi(&Line[41]); //g_print("Setup.Macro.Index1=%d\n",Setup.Macro.Index1[i]);
    if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
       { strcpy(ErrMessg,"Error reading setupfile\nSetup.Macro.Index2 absent"); return 1; }
    Setup.Macro.Index2[i]=atoi(&Line[41]); //g_print("Setup.Macro.Index2=%d\n",Setup.Macro.Index2[i]);
    }
if ( (fgets(Line,100,Fp)==NULL) || strncmp(Line,Head[17],75) )
   { strcpy(ErrMessg,"Error reading setupfile\nMacro Setup end absent"); return 1; }

//------------------IgnoreCamac section could be missing for older files---------------------------
if (fgets(Line,100,Fp)==NULL) Setup.Parameter.IgnoreCamac=FALSE;
else if (!strncmp(Line,"IgnoreCamac",11)) Setup.Parameter.IgnoreCamac=atoi(&Line[41]); 
if (Setup.Parameter.IgnoreCamac)
   {
   if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
      { strcpy(ErrMessg,"Error reading setupfile IgnoreCamac Section\nSetup.Parameter.NPar"); return 1; }
   Setup.Parameter.NPar=atof(&Line[41]); //g_print("IgnoreCamac...Setup.Parameter.NPar=%d\n",Setup.Parameter.NPar);
   for (i=0;i<Setup.Parameter.NPar;++i)
       {
       if (!fgets(Line,100,Fp)) { strcpy(ErrMessg,"Error reading setupfile\nParameter List"); return 1; }
       sscanf(Line,"%s %d",Setup.Parameter.Name[i],&Setup.Parameter.Chan[i]); 
       //g_print("%s %d\n",Setup.Parameter.Name[i],Setup.Parameter.Chan[i]);
       }
   }

fclose(Fp);
if (Opt==0) CheckSetup();
Setup.Modified=FALSE; return 0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReadS(GtkWidget *W,gpointer Unused)
{
struct stat StatBuf;
gchar ErrMessg[200];
gchar Str[200],IniName[MAX_FNAME_LENGTH],ClrName[MAX_FNAME_LENGTH];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH) 
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Setup.FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SetupDir,FileX->Path); SavePrefs();                                                                    //Store path
g_free(FileX);
if (Read(ErrMessg,0))       //Kept all the real work in this function which can be called from a thread outside gtk_main()
   { Attention(0,ErrMessg); return; }
/*CheckSetup will already have created memory1.ini, memory1.clr, memory2.ini and memory2.clr, 
Now, just in case the user might have made manual changes to the .ini and .clr files, 
we will overwrite these four files*/
if (rindex(Setup.FName,'.') == NULL) { sprintf(Str,"ERROR\nNo dot in %s",Setup.FName); Attention(0,Str); }
sprintf(IniName,"%s/%s",SetupDir,Setup.FName); sprintf(ClrName,"%s/%s",SetupDir,Setup.FName);
strcpy(index(IniName,'.'),"1.ini"); strcpy(index(ClrName,'.'),"1.clr");
if (stat(IniName,&StatBuf) == 0) { sprintf(Str,"cp %s %s/memory1.ini",IniName,SetupDir); system(Str); }     //File exists
if (stat(ClrName,&StatBuf) == 0) { sprintf(Str,"cp %s %s/memory1.clr",ClrName,SetupDir); system(Str); }     //File exists
strcpy(IniName,Setup.FName); strcpy(ClrName,Setup.FName);
strcpy(index(IniName,'.'),"2.ini"); strcpy(index(ClrName,'.'),"2.clr");
if (stat(IniName,&StatBuf) == 0) { sprintf(Str,"cp %s %s/memory2.ini",IniName,SetupDir); system(Str); }     //File exists
if (stat(ClrName,&StatBuf) == 0) { sprintf(Str,"cp %s %s/memory2.clr",ClrName,SetupDir); system(Str); }     //File exists
}
/*----------------------------------------------------------------------------------------------------------------------*/
void YesOverwrite(GtkWidget *W,GtkWidget *Win)
{
gtk_widget_destroy(Win);
Save(0);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveAsS(GtkWidget *W,gpointer Unused)
{
GtkWidget *Label,*But,*Win;
char Str[MAX_FNAME_LENGTH+20];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH) 
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Setup.FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(SetupDir,FileX->Path); SavePrefs();                                                                   //Store path
g_free(FileX);

if (access(Setup.FName,0)==0) 
   {
   Win=gtk_dialog_new(); gtk_grab_add(Win);
   gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
   gtk_window_set_title(GTK_WINDOW(Win),"Overwrite?"); gtk_container_border_width(GTK_CONTAINER(Win),5);
   sprintf(Str,"Overwrite %s?",Setup.FName);
   Label=gtk_label_new(Str); gtk_misc_set_padding(GTK_MISC(Label),10,10);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,TRUE,TRUE,0);
   But=gtk_button_new_from_stock(GTK_STOCK_YES);
   gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(YesOverwrite),Win);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,TRUE,0);
   But=gtk_button_new_from_stock(GTK_STOCK_NO);
   gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,TRUE,0);
   gtk_widget_show_all(Win);
   }
else Save(0);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReadSetup(GtkWidget *W,gpointer Unused)
{
if (ProgramState != Free) { Attention(0,"ReadSetup is not allowed in the current Program State"); return; }
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Read Setup",NULL,300,TOP_SPACE+TopOfset,TRUE,SetupDir,".set",FALSE,&ReadS,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveSetup(GtkWidget *W,gpointer Unused)
{
if (ProgramState != Free) { Attention(0,"SaveSetup is not allowed in the current Program State"); return; }
if (strlen(Setup.FName)==0) 
   {
   FileX=g_new(struct FileSelectType,1);
   FileOpenNew("Save Setup",NULL,300,TOP_SPACE+TopOfset,FALSE,SetupDir,".set",FALSE,&SaveAsS,FALSE);
   }
else Save(0);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveAs(GtkWidget *W,gpointer Unused)
{
if (ProgramState != Free) { Attention(0,"SaveSetup is not allowed in the current Program State"); return; }
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Save Setup",NULL,300,TOP_SPACE+TopOfset,FALSE,SetupDir,".set",FALSE,&SaveAsS,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Save(gint Opt)                    //Normal: Opt=0 write Setup.Fname. Opt=1 write ".lamps_set" suppress error messages
{
FILE *Fp;
gint i,j;
gchar IniName[MAX_FNAME_LENGTH],ClrName[MAX_FNAME_LENGTH],Str[120],MPath[MAX_FNAME_LENGTH];
struct stat StatBuf;

if (Opt==0) { if ((Fp=fopen(Setup.FName,"w"))==NULL) { Attention(0,"Could not write setup file"); return; }}
else        { if ((Fp=fopen(".lamps_set","w"))==NULL) return; }
fprintf(Fp,"------------------------ListMode Setup starts here------------------------------\n");            
fprintf(Fp,"%-40s=%d\n","ListOn",Setup.ListMode.ListOn);
fprintf(Fp,"%-40s=%d\n","Compression",Setup.ListMode.Compr);
fprintf(Fp,"%-40s=%d\n","Event Rate (0-4)",Setup.ListMode.Rate);
fprintf(Fp,"%-40s=%d\n","No of Gates",Setup.ListMode.NoOfLGates);
for (i=0;i<Setup.ListMode.NoOfLGates;i++)
     fprintf(Fp,"Gate No. %d Para,Lo,Hi                   =%d %d %d\n",i+1,Setup.ListMode.LGates[i].Para,
             Setup.ListMode.LGates[i].Lo,Setup.ListMode.LGates[i].Hi);
fprintf(Fp,"------------------------ListMode Setup end here---------------------------------\n");            

fprintf(Fp,"------------------------Hardware Setup starts here------------------------------\n");            
fprintf(Fp,"%-40s=%d\n","Number of Crates",Setup.Hardware.NCrates);
fprintf(Fp,"%-40s=%d\n","Camac Mode",Setup.Hardware.CamacMode);
fprintf(Fp,"%-40s=%d\n","Use Gate Supressor",Setup.Hardware.UseGtSup);
fprintf(Fp,"%-40s=%d\n","Gate Supressor StNo",Setup.Hardware.GtSupStn);
for (i=0;i<2*MAX_CAMAC_STNS;i++)
    {
    fprintf(Fp,"Stn.No. %-3d Module,Lam,Mode,LLD,F,Gain  =%d %d %d %d %d %d\n",i+1,
       Setup.Hardware.Modules[i],Setup.Hardware.Properties[i].AdcLam,Setup.Hardware.Properties[i].AdcMode,
       Setup.Hardware.Properties[i].AdcLLD,Setup.Hardware.Properties[i].AdcFCode,Setup.Hardware.Properties[i].AdcGain);
    if (Setup.Hardware.Modules[i] == 7)                                                             //Silena Offset values
       {
       fprintf(Fp,"Silena Offsets                          =");
       for (j=0;j<8;++j) fprintf(Fp,"%d ",Setup.Hardware.Properties[i].SilenaOffset[j]);
       fprintf(Fp,"\n"); 
       }
    if ( (Setup.Hardware.Modules[i] == 7) || (Setup.Hardware.Modules[i] == 5) )                         //Silena and Phillips Lower Thresholds
       {
       fprintf(Fp,"Lower Thresholds                        =");
       for (j=0;j<16;++j) fprintf(Fp,"%d ",Setup.Hardware.Properties[i].LowerThreshold[j]);
       fprintf(Fp,"\n");
       }
    if ( (Setup.Hardware.Modules[i] == 7) || (Setup.Hardware.Modules[i] == 5) )                         //Silena and Phillips Upper Thresholds
       {
       fprintf(Fp,"Upper Thresholds                        =");
       for (j=0;j<16;++j) fprintf(Fp,"%d ",Setup.Hardware.Properties[i].UpperThreshold[j]);
       fprintf(Fp,"\n");
       }
    fprintf(Fp,"Parameters, SubAddresses                =<%s> <%s>\n",Setup.Hardware.Paras[i],Setup.Hardware.SubAddr[i]);
    fprintf(Fp,"ParaNames, ZSupLLD, ZSupULD             =<%s> %d %d\n",
            Setup.Hardware.ParaNames[i],Setup.Hardware.ZSupLLD[i],Setup.Hardware.ZSupULD[i]);
    }
fprintf(Fp,"------------------------Hardware Setup ends here--------------------------------\n");            

fprintf(Fp,"------------------------Spectra Setup starts here-------------------------------\n");            
fprintf(Fp,"%-40s=%d\n","Spectra Safety",Setup.Spectra.Safety);
fprintf(Fp,"%-40s=%d\n","Safety Time",Setup.Spectra.SafetyTime);
fprintf(Fp,"------------------------Spectra Setup ends here---------------------------------\n");            

fprintf(Fp,"------------------------Oned Setup starts here----------------------------------\n");
fprintf(Fp,"%-40s=%d\n","No. of 1d Spectra",Setup.Oned.N);
fprintf(Fp,"%-40s=%d\n","Word Size",Setup.Oned.WSz);
for (i=0;i<Setup.Oned.N;i++)
    {
    fprintf(Fp,"Spec. No. %-4d Para,NPar,Size,Name      =%d %d %d %s\n",i+1,Setup.Oned.Par[i],Setup.Oned.NPar[i],Setup.Oned.Chan[i],Setup.Oned.Name[i]);
    fprintf(Fp,"NGates1,Cond,NGates2,Cond               =%d %d %d %d\n",Setup.Oned.Gate1[i].NGates,Setup.Oned.Gate1[i].Cond,
            Setup.Oned.Gate2[i].NGates,Setup.Oned.Gate2[i].Cond);
    for (j=0;j<Setup.Oned.Gate1[i].NGates;j++)
        fprintf(Fp,"Gate No. %-4d Para,Lo,Hi                =%d %d %d\n",j+1,Setup.Oned.Gate1[i].Gate1d[j].Para,
                Setup.Oned.Gate1[i].Gate1d[j].Lo,Setup.Oned.Gate1[i].Gate1d[j].Hi);
    for (j=0;j<Setup.Oned.Gate2[i].NGates;j++)
        fprintf(Fp,"Gate. No. %-4d BananaFile               =<%s>\n",j+1,Setup.Oned.Gate2[i].Gate2d[j]);
    }
fprintf(Fp,"------------------------Oned Setup ends here------------------------------------\n");

fprintf(Fp,"------------------------Twod Setup starts here----------------------------------\n");
fprintf(Fp,"%-40s=%d\n","No. of 2d Spectra",Setup.Twod.N);
fprintf(Fp,"%-40s=%d\n","Word Size",Setup.Twod.WSz);
for (i=0;i<Setup.Twod.N;i++)
    {
    fprintf(Fp,"Spec. No. %-4d XPr,Nx,YPr,Ny,XSz,YSz,Nm =%d %d %d %d %d %d %s\n",i+1,Setup.Twod.XPar[i],Setup.Twod.NXPar[i],
            Setup.Twod.YPar[i],Setup.Twod.NYPar[i],Setup.Twod.XChan[i],Setup.Twod.YChan[i],Setup.Twod.Name[i]);
    fprintf(Fp,"NGates1,Cond,NGates2,Cond               =%d %d %d %d\n",Setup.Twod.Gate1[i].NGates,Setup.Twod.Gate1[i].Cond,
            Setup.Twod.Gate2[i].NGates,Setup.Twod.Gate2[i].Cond);
    for (j=0;j<Setup.Twod.Gate1[i].NGates;j++)
        fprintf(Fp,"Gate No. %-4d Para,Lo,Hi                =%d %d %d\n",j+1,Setup.Twod.Gate1[i].Gate1d[j].Para,
                Setup.Twod.Gate1[i].Gate1d[j].Lo,Setup.Twod.Gate1[i].Gate1d[j].Hi);
    for (j=0;j<Setup.Twod.Gate2[i].NGates;j++)
        fprintf(Fp,"Gate. No. %-4d BananaFile               =<%s>\n",j+1,Setup.Twod.Gate2[i].Gate2d[j]);
    fprintf(Fp,"Spectrum Type (0=Normal, 1=CDdet)       =%d\n",Setup.Twod.CDdet[i].Enabled);
    if (Setup.Twod.CDdet[i].Enabled)
       for (j=0;j<8;++j) fprintf(Fp,"Parameter %2d                            =%d\n",j,Setup.Twod.CDdet[i].P[j]);
    }
fprintf(Fp,"------------------------Twod Setup ends here------------------------------------\n");

fprintf(Fp,"--Pseudo (0=Sum,1=Product,2=Ratio,3=Position,4=PI,5=Multiplicty,6=User,7=Array)-\n");
fprintf(Fp,"%-40s=%d\n","No. of Pseudo Parameters",Setup.Pseudo.N);
for (i=0;i<Setup.Pseudo.N;i++)
    {
    fprintf(Fp,"Pseudo No. %-4d P1,P2,Size,Type,Name    =%d %d %d %d %s\n",i+1,Setup.Pseudo.P1[i],Setup.Pseudo.P2[i],
            Setup.Pseudo.Size[i],Setup.Pseudo.Type[i],Setup.Pseudo.Name[i]);
    fprintf(Fp,"K1,O1,K2,O2,K3,O3                       =%g %g %g %g %g %g\n",Setup.Pseudo.K1[i],Setup.Pseudo.O1[i],
            Setup.Pseudo.K2[i],Setup.Pseudo.O2[i],Setup.Pseudo.K3[i],Setup.Pseudo.O3[i]);
    fprintf(Fp,"Power,L1,L2                             =%g %d %d\n",Setup.Pseudo.Power[i],Setup.Pseudo.L1[i],Setup.Pseudo.L2[i]);
    if (Setup.Pseudo.Type[i] == Array)
       {
       fprintf(Fp,"ArrayN                                  =%d\n",Setup.Pseudo.ArrayN[i]);
       for (j=0;j<Setup.Pseudo.ArrayN[i];++j)
           {
           fprintf(Fp,"Par,LLD,ULD                             =%d %d %d\n",
           Setup.Pseudo.ArrayPar[j][i],Setup.Pseudo.ArrayLLD[j][i],Setup.Pseudo.ArrayULD[j][i]);
           fprintf(Fp,"Offset,Slope,Quad                       =%g %g %g\n",
           Setup.Pseudo.ArrayOffset[j][i],Setup.Pseudo.ArraySlope[j][i],Setup.Pseudo.ArrayQuad[j][i]);
           }
       }
    if (Setup.Pseudo.Type[i] == BGated)
       fprintf(Fp,"BananaFile for BGated Pseudo            =<%s>\n",PsBGated[i].Name);
    }
fprintf(Fp,"------------------------Pseudo Setup ends here----------------------------------\n"); 

fprintf(Fp,"------------------------Scaler Setup starts here--------------------------------\n");            
fprintf(Fp,"%-40s=%d\n","No. of Scalers",Setup.Scaler.NSc);
for (i=0;i<Setup.Scaler.NSc;i++)
    fprintf(Fp,"Scaler No. %2d Name, N, A, F             =<%s> %d %d %d %d\n",i+1,Setup.Scaler.Name[i],
            Setup.Scaler.C[i],Setup.Scaler.N[i],Setup.Scaler.A[i],Setup.Scaler.F[i]);
fprintf(Fp,"------------------------Scaler Setup ends here----------------------------------\n"); 

fprintf(Fp,"------------------------Periodic Log Setup starts here--------------------------\n");            
fprintf(Fp,"%-40s=%d\n","Periodic Log on?",Setup.PLogSetup.On);
fprintf(Fp,"%-40s=%d\n","BufCount",Setup.PLogSetup.BufCount);
fprintf(Fp,"------------------------Periodic Log Setup ends here----------------------------\n"); 

fprintf(Fp,"------------------------Macro Setup starts here---------------------------------\n");
fprintf(Fp,"%-40s=%d\n","Number of Macros",Setup.Macro.N);
fprintf(Fp,"%-40s=%d\n","Macro Refresh Rate",Setup.Macro.RefreshRate);
for (i=0;i<Setup.Macro.N;++i)
    {
    fprintf(Fp,"%-40s=%s\n","Macro Name",Setup.Macro.Name[i]);
    fprintf(Fp,"%-40s=%d\n","Macro Type",Setup.Macro.Type[i]);
    fprintf(Fp,"%-40s=%d\n","Display?(0/1)",Setup.Macro.Display[i]);
    fprintf(Fp,"%-40s=%d\n","1d/2d SpecNo",Setup.Macro.SpecNo[i]);
    fprintf(Fp,"%-40s=%d\n","XMin",Setup.Macro.XMin[i]);
    fprintf(Fp,"%-40s=%d\n","XMax",Setup.Macro.XMax[i]);
    fprintf(Fp,"%-40s=%d\n","YMin",Setup.Macro.YMin[i]);
    fprintf(Fp,"%-40s=%d\n","YMax",Setup.Macro.YMax[i]);
    fprintf(Fp,"%-40s=%f\n","K1",Setup.Macro.K1[i]);
    fprintf(Fp,"%-40s=%f\n","K2",Setup.Macro.K2[i]);
    fprintf(Fp,"%-40s=%d\n","Scaler Number",Setup.Macro.ScalerNo[i]);
    fprintf(Fp,"%-40s=%d\n","Index1",Setup.Macro.Index1[i]);
    fprintf(Fp,"%-40s=%d\n","Index2",Setup.Macro.Index2[i]);
    }
fprintf(Fp,"------------------------Macro Setup ends here-----------------------------------\n");
fprintf(Fp,"%-40s=%d\n","IgnoreCamac",Setup.Parameter.IgnoreCamac);
if (Setup.Parameter.IgnoreCamac)
   {
   fprintf(Fp,"%-40s=%d\n","Setup.Parameter.NPar",Setup.Parameter.NPar);
   for (i=0;i<Setup.Parameter.NPar;++i)
       fprintf(Fp,"%s %d\n",Setup.Parameter.Name[i],Setup.Parameter.Chan[i]);
   }

fclose(Fp);

/*  Now, we will copy memory1.ini, memory2.ini, memory1.clr, memory2.clr to <setupfile><1/2>.ini and .clr files,   *
 *  This allows the user to make changes in the these files after setup, if he so desires                          */
if (Opt==0)  //Dont write ini and clr files while saving .lamps_set. The memory<1/2> files would be used in any case
   {
   if (index(Setup.FName,'.') == NULL) { sprintf(Str,"ERROR\nNo dot in %s",Setup.FName); Attention(0,Str); }
   strcpy(IniName,Setup.FName); strcpy(ClrName,Setup.FName);
   strcpy(rindex(IniName,'.'),"1.ini"); strcpy(rindex(ClrName,'.'),"1.clr");
   sprintf(MPath,"%s/memory1.ini",SetupDir);
   if (stat(MPath,&StatBuf) == 0) { sprintf(Str,"cp %s %s",MPath,IniName); system(Str); }
   sprintf(MPath,"%s/memory1.clr",SetupDir);
   if (stat(MPath,&StatBuf) == 0) { sprintf(Str,"cp %s %s",MPath,ClrName); system(Str); }
   strcpy(IniName,Setup.FName); strcpy(ClrName,Setup.FName);
   strcpy(rindex(IniName,'.'),"2.ini"); strcpy(rindex(ClrName,'.'),"2.clr");
   sprintf(MPath,"%s/memory2.ini",SetupDir);
   if (stat(MPath,&StatBuf) == 0) { sprintf(Str,"cp %s %s",MPath,IniName); system(Str); }
   sprintf(MPath,"%s/memory2.clr",SetupDir);
   if (stat(MPath,&StatBuf) == 0) { sprintf(Str,"cp %s %s",MPath,ClrName); system(Str); }
   }
Setup.Modified=FALSE;
}
//----------------------------------------------------------------------------------------------------------------------
