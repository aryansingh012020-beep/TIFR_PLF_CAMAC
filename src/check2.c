//check2.c Corrects list mode files that have become sync. mismatched
//A.Chatterjee :: 30 Sept 2004 Last Update 30 Sept 2004
//To compile :: cc `gtk-config --cflags --libs` check2.c parse.o useful.o -o check2
#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "lamps.h"

/*Global Variables*/
enum ProgramState ProgramState;
struct Setup Setup;

/*Function templates*/
void ReadSetup(GtkWidget *W,gpointer Data);
void Attention(gint XPos,gchar *Messg);                                                                       //in useful.c
/*---------------------------------------------------------------------------------------------------------------------*/
void Err(gchar *Messg)
{
gchar Str[80];
                                                                                                                             
sprintf(Str,"Setup File Error: %s",Messg); Attention(0,Str);
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint Read(gchar *ErrMessg)                                 //Source pasted from rw_setup.c, but call to CheckSetup removed
{
FILE *Fp;
gint i,j,Value[10],ToHere,L;
gchar Line[120],Str[200],Str1[200];
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

if ((Fp=fopen(Setup.FName,"r"))==NULL) { strcpy(ErrMessg,"Could not find setup file"); return 1; }
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
   { strcpy(ErrMessg,"Error reading setupfile\nSetup.ListMode.BufSiz"); return 1; }
Setup.ListMode.BufSiz=atoi(&Line[41]); //g_print("Setup.ListMode.BufSiz=%d\n",Setup.ListMode.BufSiz);
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
    if (Setup.Hardware.Modules[i]==7)                                                              //Silena Offset values
       {
       if ( (fgets(Line,100,Fp)==NULL) || Line[40] != '=')
          { sprintf(Str,"Error reading setupfile\nSilena Offsets Stn=%d",i+1); strcpy(ErrMessg,Str); return 1; }
       ParseTextToInt(&Line[41],0,8,Value,&ToHere); 
       for (j=0;j<8;++j) Setup.Hardware.Properties[i].SilenaOffset[j]=Value[j];
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
   { strcpy(ErrMessg,"Error reading setupfile\nHeading Spectra Setup absent"); return 1; }

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
    ParseTextToInt(&Line[41],0,2,Value,&ToHere);
    Setup.Oned.Par[i]=Value[0]; //g_print("Setup.Oned.Par[%d]=%d\n",i+1,Setup.Oned.Par[i]);
    Setup.Oned.Chan[i]=Value[1]; //g_print("Setup.Oned.Chan[%d]=%d\n",i+1,Setup.Oned.Chan[i]);
    if (Line[41+ToHere]) 
       { strcpy(Setup.Oned.Name[i],&Line[41+ToHere]); Setup.Oned.Name[i][strlen(Setup.Oned.Name[i])-1]='\0'; }
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
        strncpy(Setup.Oned.Gate2[i].Gate2d[j],&Str1[1],MAX_TEXT_FIELD); Setup.Oned.Gate2[i].Gate2d[j][L-2]='\0';
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
    ParseTextToInt(&Line[41],0,4,Value,&ToHere);
    Setup.Twod.XPar[i]=Value[0]; //g_print("Setup.Twod.XPar[%d]=%d\n",i+1,Setup.Twod.XPar[i]);
    Setup.Twod.YPar[i]=Value[1]; //g_print("Setup.Twod.YPar[%d]=%d\n",i+1,Setup.Twod.YPar[i]);
    Setup.Twod.XChan[i]=Value[2]; //g_print("Setup.Twod.XChan[%d]=%d\n",i+1,Setup.Twod.XChan[i]);
    Setup.Twod.YChan[i]=Value[3]; //g_print("Setup.Twod.YChan[%d]=%d\n",i+1,Setup.Twod.YChan[i]);
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
        strncpy(Setup.Twod.Gate2[i].Gate2d[j],&Str1[1],MAX_TEXT_FIELD); Setup.Twod.Gate2[i].Gate2d[j][L-2]='\0';
        //g_print("Setup.Twod.Gate2[%d].Gate2d[%d]=[%s]\n",i+1,j+1,Setup.Twod.Gate2[i].Gate2d[j]);
        }
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
    strcpy(Setup.Macro.Name[i],&Line[41]); Setup.Macro.Name[i][strlen(Setup.Macro.Name[i])-1]='\0';
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

fclose(Fp); Setup.Modified=FALSE; return 0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReadS(GtkWidget *W,GtkFileSelection *Fs)
{
gchar ErrMessg[200];
                                                                                                                             
strcpy(Setup.FName,gtk_file_selection_get_filename(GTK_FILE_SELECTION(Fs)));
if (Read(ErrMessg)) { Attention(0,ErrMessg); return; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReadSetup(GtkWidget *W,gpointer Data)
{
GtkWidget *FileW;

FileW=gtk_file_selection_new("Select Setup File"); gtk_grab_add(FileW);
gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(FileW));
gtk_file_selection_complete(GTK_FILE_SELECTION(FileW),"*.set");
gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(FileW)->ok_button),"clicked",GTK_SIGNAL_FUNC(ReadS),FileW);
gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(FileW)->ok_button),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(FileW));
gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(FileW)->cancel_button),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),                          GTK_OBJECT(FileW));
gtk_widget_show(FileW);
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint main(int argc,char *argv[])
{
GtkWidget *Win,*HBox,*VBox,*VBox1,*Lab,*But;
gint i;
gchar Str[80];

gtk_init(&argc,&argv);

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_main_quit),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"check2 :: Restore sync. in multi-crate data");
gtk_widget_set_uposition(GTK_WIDGET(Win),60,40);
gtk_widget_set_usize(GTK_WIDGET(Win),300,120);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox1,TRUE,FALSE,0);
Lab=gtk_label_new("This program reads zls files"); gtk_box_pack_start(GTK_BOX(VBox1),Lab,TRUE,FALSE,0);
Lab=gtk_label_new("Checks for multi-crate syncronization"); gtk_box_pack_start(GTK_BOX(VBox1),Lab,TRUE,FALSE,0);
Lab=gtk_label_new("Re-writes the file correcting sync. problems"); gtk_box_pack_start(GTK_BOX(VBox1),Lab,TRUE,FALSE,0);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label("Read\nSetup"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReadSetup),Win);
But=gtk_button_new_with_label("Check\nOnly"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
But=gtk_button_new_with_label("Correct &\nRe-write"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
But=gtk_button_new_with_label("  Quit  "); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_main_quit),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
gtk_main();
return(0);
}
/*------------------------------------------------------------------------------------------------------------------------*/
