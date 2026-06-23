#include <gtk/gtk.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include "lamps.h"

/*External Globals*/
extern enum ProgramState ProgramState;                                        //The current running state of the program
extern struct Setup Setup;
extern struct PsBGated PsBGated[MAX_PSEUDO];                                                  //Banana gated pseudo type
extern guint16 *Oned16,*Twod16;
extern guint32 *Oned32,*Twod32;
extern gint Off1d[MAX_1D],Off2d[MAX_2D];
extern GtkWidget *S_Stat[15];                                                                       //Status bar widgets
extern enum AnalysisRequest AnalysisRequest;
extern gchar SelRun[MAX_TEXT_FIELD];
extern gfloat MacroCurr[MAX_MACRO],MacroPrev[MAX_MACRO],MacroDiff[MAX_MACRO];                //Computed values of macros
extern gchar MacroDir[MAX_DIR_STRLEN];                                                           //Directory preferences

/*Function Templates*/
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen); gint TestBGate(gint X,gint Y,struct BGate BGate);
void Attention(gint XPos,gchar *Messg); void RefreshAll(void); gint Read(gchar *ErrMessg); 
void ZeroOned(gint SNo); void ZeroTwod(gint SNo);
void CloseAllSpecWindows(GtkWidget *W,gpointer Data);
void Screen1(GtkWidget *W,gpointer Data);
void SaveSpectraOk(GtkWidget *W,GtkWidget *Win);
void bstart_(void); void fstart_(gchar *RunName,gint Len); void usersub_(gint *NPar,gint *NPseudo,gshort *P); 
void fstop_(void); void bstop_(void);
int FreedomRead(FILE *ListFile,glong *ZBufs,glong *EvtsRead,glong *BytsRead,
                gboolean Option,gushort *DataBuf,gint *NewEvts);
int FreedomSkip(FILE *ListFile);
gint ZlsGlsRead(FILE *ListFile,gushort *Buf,glong *ZBufs,glong *EvtsRead,glong *BytsRead,
                gboolean OptionReWrite,gshort *OutBuf,gint *NewEvts);
gint ZlsGlsSkip(FILE *ListFile);
void Exogam(gushort *Buf,glong *ZBufs,glong *EvtsRead,glong *BytsRead);
void ComputeMacros(glong ZBufs,FILE *MacroFp);
//----------------------------------------------------------------------------------------------------------------------
gboolean Test1dGatesFor1dSpec(gshort *P,gint SNo)
{
gint GNo,V;

if (Setup.Oned.Gate1[SNo].Cond==And)                           //And condition. If any gate is unstaisfied we can return
   {
   for (GNo=0;GNo<Setup.Oned.Gate1[SNo].NGates;GNo++)
       {
       V=P[Setup.Oned.Gate1[SNo].Gate1d[GNo].Para-1];
       if ( (V<Setup.Oned.Gate1[SNo].Gate1d[GNo].Lo) || (V>Setup.Oned.Gate1[SNo].Gate1d[GNo].Hi) ) return FALSE;
       }
   return TRUE;                                                                                 //All gates were satisfied
   }
else                                                                //Or condition. If any gate is satisfied we can return
   {
   for (GNo=0;GNo<Setup.Oned.Gate1[SNo].NGates;GNo++)
       {
       V=P[Setup.Oned.Gate1[SNo].Gate1d[GNo].Para-1]; 
       if ( (V>=Setup.Oned.Gate1[SNo].Gate1d[GNo].Lo) && (V<=Setup.Oned.Gate1[SNo].Gate1d[GNo].Hi) ) return TRUE;
       }
   return FALSE;                                                                        //None of the gates were satisfied
   }
}
//----------------------------------------------------------------------------------------------------------------------
gboolean Test1dGatesFor2dSpec(gshort *P,gint SNo)
{
gint GNo,V;

if (Setup.Twod.Gate1[SNo].Cond==And)                            //And condition. If any gate is unstaisfied we can return
   {
   for (GNo=0;GNo<Setup.Twod.Gate1[SNo].NGates;GNo++)
       {
       V=P[Setup.Twod.Gate1[SNo].Gate1d[GNo].Para-1];
       if ( (V<Setup.Twod.Gate1[SNo].Gate1d[GNo].Lo) || (V>Setup.Twod.Gate1[SNo].Gate1d[GNo].Hi) ) return FALSE;
       }
   return TRUE;                                                                                 //All gates were satisfied
   }
else                                                                //Or condition. If any gate is satisfied we can return
   {
   for (GNo=0;GNo<Setup.Twod.Gate1[SNo].NGates;GNo++)
       {
       V=P[Setup.Twod.Gate1[SNo].Gate1d[GNo].Para-1]; 
       if ( (V>=Setup.Twod.Gate1[SNo].Gate1d[GNo].Lo) && (V<=Setup.Twod.Gate1[SNo].Gate1d[GNo].Hi) ) return TRUE;
       }
   return FALSE;                                                                       //None of the gates were satisfied
   }
}
//----------------------------------------------------------------------------------------------------------------------
gboolean Test2dGatesFor1dSpec(gshort *P,gint SNo)
{
gint GNo,Vx,Vy,Ix,Iy;

if (Setup.Oned.Gate2[SNo].Cond==And)
   {
   for (GNo=0;GNo<Setup.Oned.Gate2[SNo].NGates;GNo++)
       {
       Ix=Setup.Oned.Gate2[SNo].BGates[GNo].XPar-1; Iy=Setup.Oned.Gate2[SNo].BGates[GNo].YPar-1;
       Vx=8192.0*P[Ix]/Setup.Parameter.Chan[Ix]; Vy=8192.0*P[Iy]/Setup.Parameter.Chan[Iy];
       if (!TestBGate(Vx,Vy,Setup.Oned.Gate2[SNo].BGates[GNo])) return FALSE;
       }
   return TRUE;
   }
else
   {
   for (GNo=0;GNo<Setup.Oned.Gate2[SNo].NGates;GNo++)
       {
       Ix=Setup.Oned.Gate2[SNo].BGates[GNo].XPar-1; Iy=Setup.Oned.Gate2[SNo].BGates[GNo].YPar-1;
       Vx=8192.0*P[Ix]/Setup.Parameter.Chan[Ix]; Vy=8192.0*P[Iy]/Setup.Parameter.Chan[Iy];
       if (TestBGate(Vx,Vy,Setup.Oned.Gate2[SNo].BGates[GNo])) return TRUE;
       }
   return FALSE;
   }
}
//----------------------------------------------------------------------------------------------------------------------
gboolean Test2dGatesFor2dSpec(gshort *P,gint SNo)
{
gint GNo,Vx,Vy,Ix,Iy;

if (Setup.Twod.Gate2[SNo].Cond==And)
   {
   for (GNo=0;GNo<Setup.Twod.Gate2[SNo].NGates;GNo++)
       {
       Ix=Setup.Twod.Gate2[SNo].BGates[GNo].XPar-1; Iy=Setup.Twod.Gate2[SNo].BGates[GNo].YPar-1;
       Vx=8192.0*P[Ix]/Setup.Parameter.Chan[Ix]; Vy=8192.0*P[Iy]/Setup.Parameter.Chan[Iy];
       if (!TestBGate(Vx,Vy,Setup.Twod.Gate2[SNo].BGates[GNo])) return FALSE;
       }
   return TRUE;
   }
else
   {
   for (GNo=0;GNo<Setup.Twod.Gate2[SNo].NGates;GNo++)
       {
       Ix=Setup.Twod.Gate2[SNo].BGates[GNo].XPar-1; Iy=Setup.Twod.Gate2[SNo].BGates[GNo].YPar-1;
       Vx=8192.0*P[Ix]/Setup.Parameter.Chan[Ix]; Vy=8192.0*P[Iy]/Setup.Parameter.Chan[Iy];
       if (TestBGate(Vx,Vy,Setup.Twod.Gate2[SNo].BGates[GNo])) return TRUE;
       }
   return FALSE;
   }
}
//----------------------------------------------------------------------------------------------------------------------
void ProcessEvent(gshort *P,gboolean Option)      //Option=FALSE build pseudos and spectra Option=TRUE builds pseudos only
{
gint SNo,ParNo,XPar,YPar,Chan,ChX,ChY,Ps,i,Mult,Det,DetMax,Val,Ix,Iy,Vx,Vy,j;
gdouble A,B,C,D;

//        for (i=0;i<Setup.Parameter.NPar;i++) P[i]=CLAMP(P[i],0,Setup.Parameter.Chan[i]-1);    Clamp: too slow, avoid
for (Ps=0;Ps<Setup.Pseudo.N;Ps++)                                                            //Compute Pseudo Parameters
    {
    switch (Setup.Pseudo.Type[Ps])
       {
       case Sum:          A=Setup.Pseudo.K1[Ps]*P[Setup.Pseudo.P1[Ps]-1]+Setup.Pseudo.O1[Ps];
                          B=Setup.Pseudo.K2[Ps]*P[Setup.Pseudo.P2[Ps]-1]+Setup.Pseudo.O2[Ps];
                          C=Setup.Pseudo.K3[Ps]*(A+B)+Setup.Pseudo.O3[Ps];
                          P[Setup.Parameter.NPar+Ps]=CLAMP((gint)(C+0.5),0,Setup.Pseudo.Size[Ps]-1);
                          break;
       case Product:      A=Setup.Pseudo.K1[Ps]*P[Setup.Pseudo.P1[Ps]-1]+Setup.Pseudo.O1[Ps];
                          B=Setup.Pseudo.K2[Ps]*P[Setup.Pseudo.P2[Ps]-1]+Setup.Pseudo.O2[Ps];
                          C=Setup.Pseudo.K3[Ps]*B*(A+B)+Setup.Pseudo.O3[Ps];
                          P[Setup.Parameter.NPar+Ps]=CLAMP((gint)(C+0.5),0,Setup.Pseudo.Size[Ps]-1);
                          break;
       case Ratio:        A=Setup.Pseudo.K1[Ps]*P[Setup.Pseudo.P1[Ps]-1]+Setup.Pseudo.O1[Ps];
                          B=Setup.Pseudo.K2[Ps]*P[Setup.Pseudo.P2[Ps]-1]+Setup.Pseudo.O2[Ps];
                          if (B==0.0) B=1.0E-04;
                          C=Setup.Pseudo.K3[Ps]*(A/B)+Setup.Pseudo.O3[Ps];
                          P[Setup.Parameter.NPar+Ps]=CLAMP((gint)(C+0.5),0,Setup.Pseudo.Size[Ps]-1);
                          break;
       case Position:     A=Setup.Pseudo.K1[Ps]*P[Setup.Pseudo.P1[Ps]-1]+Setup.Pseudo.O1[Ps];
                          B=Setup.Pseudo.K2[Ps]*P[Setup.Pseudo.P2[Ps]-1]+Setup.Pseudo.O2[Ps];
                          C=A+B; if (C==0.0) C=1.0E-04;
                          C=Setup.Pseudo.K3[Ps]*(A/C)+Setup.Pseudo.O3[Ps];
                          P[Setup.Parameter.NPar+Ps]=CLAMP((gint)(C+0.5),0,Setup.Pseudo.Size[Ps]-1);
                          break;
       case PI:           A=Setup.Pseudo.K1[Ps]*P[Setup.Pseudo.P1[Ps]-1]+Setup.Pseudo.O1[Ps];
                          B=Setup.Pseudo.K2[Ps]*P[Setup.Pseudo.P2[Ps]-1]+Setup.Pseudo.O2[Ps];
                          D=Setup.Pseudo.Power[Ps];
                          C=Setup.Pseudo.K3[Ps]*(pow((A+B),D)-pow(A,D))+Setup.Pseudo.O3[Ps];
                          P[Setup.Parameter.NPar+Ps]=CLAMP((gint)(C+0.5),0,Setup.Pseudo.Size[Ps]-1);
                          break;
       case Multiplicity: for (i=Setup.Pseudo.L1[Ps],Mult=0;i<=Setup.Pseudo.L2[Ps];i++)
                              {
                              if ((P[Setup.Pseudo.P1[Ps]-1]<=Setup.Pseudo.O2[Ps]) && 
                                  (P[Setup.Pseudo.P1[Ps]-1]>=Setup.Pseudo.O1[Ps]))
                                 Mult++;
                              }
                          C=(gdouble)Mult;
                          P[Setup.Parameter.NPar+Ps]=CLAMP((gint)(C+0.5),0,Setup.Pseudo.Size[Ps]-1);
                          break;
       case User:         break; //do nothing       
       case Array:        C=0.0;
                          for (i=0;i<Setup.Pseudo.ArrayN[Ps];++i)
                              {
                              Val=P[Setup.Pseudo.ArrayPar[i][Ps]-1];
                              if ( (Val>=Setup.Pseudo.ArrayLLD[i][Ps]) && (Val<=Setup.Pseudo.ArrayULD[i][Ps]) )
                                 C+=Setup.Pseudo.ArrayOffset[i][Ps]+Setup.Pseudo.ArraySlope[i][Ps]*Val+
                                    Setup.Pseudo.ArrayQuad[i][Ps]*Val*Val;
                              }
                          C+=rand()/(RAND_MAX+1.0);                                               //Add random value (0-1)
                          P[Setup.Parameter.NPar+Ps]=CLAMP((gint)C,0,Setup.Pseudo.Size[Ps]-1);   //Truncate, not round off
                          break;
       case BGated:       Ix=PsBGated[Ps].BGate.XPar-1; Iy=PsBGated[Ps].BGate.YPar-1;
                          Vx=8192.0*P[Ix]/Setup.Parameter.Chan[Ix]; Vy=8192.0*P[Iy]/Setup.Parameter.Chan[Iy];
                          if (TestBGate(Vx,Vy,PsBGated[Ps].BGate))
                             P[Setup.Parameter.NPar+Ps]=MIN(P[Setup.Pseudo.P1[Ps]-1],Setup.Pseudo.Size[Ps]-1);
                          else P[Setup.Parameter.NPar+Ps]=0;
                          break; //only for safety
       }
    }

usersub_(&Setup.Parameter.NPar,&Setup.Pseudo.N,P);                                   //Call SUBROUTINE USERSUB (in user.F)
if (Option) return;                                                               //If Option=TRUE we do not build spectra
for (SNo=0;SNo<Setup.Oned.N;++SNo)                                                                    //Build Oned Spectra
    {
    if (!Test1dGatesFor1dSpec(P,SNo)) continue;                                                            //Test 1d gates
    if (!Test2dGatesFor1dSpec(P,SNo)) continue;                                                            //Test 2d Gates 
    ParNo=Setup.Oned.Par[SNo]-1;
    if (ParNo==-1)                                                                                 //Hit Pattern spectrum
       {
       DetMax=MIN(Setup.Oned.Chan[SNo]-1,Setup.Parameter.NPar);
       for (Det=0;Det<DetMax;Det++)
           if ( (P[Det] >= Setup.Parameter.LLD[Det]) && (P[Det] <= Setup.Parameter.ULD[Det]) )
              { if (Setup.Oned.WSz==1) Oned16[Off1d[SNo]+Det]++; else Oned32[Off1d[SNo]+Det]++; }
       }
    else
       {
       for (i=0;i<Setup.Oned.NPar[SNo];++i)                                  //Loop added for Vector 1d Spectra, Nov 2006
           {                                                //NB: All the ADCs in the vector should have the same MaxChan
           Chan=P[ParNo+i] >> Setup.Oned.BitShift[SNo];
           if (Setup.Oned.WSz==1) Oned16[Off1d[SNo]+Chan]++; else Oned32[Off1d[SNo]+Chan]++;
           }
       }
    }

for (SNo=0;SNo<Setup.Twod.N;++SNo)                                                                   //Build Twod Spectra
    {
    if (!Test1dGatesFor2dSpec(P,SNo)) continue;                                                           //Test 1d gates
    if (!Test2dGatesFor2dSpec(P,SNo)) continue;                                                           //Test 2d gates
    if (Setup.Twod.CDdet[SNo].Enabled)                                 //The CDdet spectrum is a composite of 4 quadrants
       {                                                     //At present the CDdet spectrum facility is not in lamps ...
       for (j=0;j<4;++j)                                     //... when its needed we have to copy some lines from glamps
           {
           XPar=Setup.Twod.CDdet[SNo].P[2*j]-1;   ChX=P[XPar];
           YPar=Setup.Twod.CDdet[SNo].P[2*j+1]-1; ChY=P[YPar];
           Chan=ChX+Setup.Twod.XChan[SNo]*ChY;
           if (Setup.Twod.WSz==1) Twod16[Off2d[SNo]+Chan]++; else Twod32[Off2d[SNo]+Chan]++;
           }
       }
    else
       {                                                                //Loops added for X,Y Vector 2d Spectra, Nov 2006
       if (Setup.Twod.MatrixType[SNo]==GammaGamma)                                           //Gamma-Gamma RADWARE matrix
          {
          for (i=0;i<Setup.Twod.NXPar[SNo];++i)                   //All the ADCs in X Vector should have the same MaxChan
              {
              XPar=Setup.Twod.XPar[SNo]-1; ChX=P[XPar+i] >> Setup.Twod.XBitShift[SNo];
              for (j=i+1;j<Setup.Twod.NYPar[SNo];++j)             //All the ADCs in Y Vector should have the same MaxChan
                  {  //To avoid double counting, j starts from i+1 (For GammaGamma matrices x y axes are the same vector)
                  YPar=Setup.Twod.YPar[SNo]-1;
                  ChY=P[YPar+j] >> Setup.Twod.YBitShift[SNo];
                  Chan=ChX+Setup.Twod.XChan[SNo]*ChY;
                  if (Setup.Twod.WSz==1) Twod16[Off2d[SNo]+Chan]++; else Twod32[Off2d[SNo]+Chan]++;
                                                           //In fact GammaGamma RADWARE matrix must always be single word
                  }
              }
          }
       else                                              //DCO RADWARE matrix or Other matrix, also non-vector 2d spectra
          {
          for (i=0;i<Setup.Twod.NXPar[SNo];++i)                   //All the ADCs in X Vector should have the same MaxChan
              {
              XPar=Setup.Twod.XPar[SNo]-1; ChX=P[XPar+i] >> Setup.Twod.XBitShift[SNo];
              for (j=0;j<Setup.Twod.NYPar[SNo];++j)               //All the ADCs in Y Vector should have the same MaxChan
                  {                                                   //Should not check for double counting in this case
                  YPar=Setup.Twod.YPar[SNo]-1;
                  ChY=P[YPar+j] >> Setup.Twod.YBitShift[SNo];
                  Chan=ChX+Setup.Twod.XChan[SNo]*ChY;
                  if (Setup.Twod.WSz==1) Twod16[Off2d[SNo]+Chan]++; else Twod32[Off2d[SNo]+Chan]++;
                  }
              }
          }
       }
    }
}
//----------------------------------------------------------------------------------------------------------------------
void Update(gdouble StartTime,gchar *FName,glong FSize,glong ZBufs,glong BytsRead,glong EvtsRead)
{
struct timeval Tv;
gdouble TimeNow,ElapsedTime,Kbps,Evtps;
gchar SKbps[60],SEvtps[60],SBytes[60],SBuffs[60],SProc[60];

gettimeofday(&Tv,NULL); TimeNow=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06; ElapsedTime=TimeNow-StartTime;
Kbps=1.0e-03*BytsRead/ElapsedTime; sprintf(SKbps,"Kb/s: %.0f",Kbps); 
Evtps=EvtsRead/ElapsedTime; sprintf(SEvtps,"Evt/s: %.2e",Evtps); 
sprintf(SBytes,"Bytes: %ld",BytsRead); 
sprintf(SBuffs,"Buffers: %ld",ZBufs); 
sprintf(SProc,"Processed: %.1f%%",100.0*(gfloat)BytsRead/FSize); 

gdk_threads_enter();
gtk_label_set_text(GTK_LABEL(S_Stat[ 9]),SKbps);  
gtk_label_set_text(GTK_LABEL(S_Stat[10]),SEvtps);
gtk_label_set_text(GTK_LABEL(S_Stat[ 3]),FName);  
gtk_label_set_text(GTK_LABEL(S_Stat[11]),SBytes); 
gtk_label_set_text(GTK_LABEL(S_Stat[ 7]),SBuffs); 
gtk_label_set_text(GTK_LABEL(S_Stat[ 8]),SProc);
if (ProgramState != ReWriteOn) RefreshAll();
gdk_threads_leave();
usleep((gint)(Setup.Offline.Delay*1.e6));                                                      //Slow down for visibility
}
//----------------------------------------------------------------------------------------------------------------------
void SAttention(gchar *Messg)                                                           //Thread safe version of Attention
{ gdk_threads_enter(); Attention(0,Messg); gdk_threads_leave(); }
//----------------------------------------------------------------------------------------------------------------------
void GetRunName(gchar *FileName,gchar *RunName)
{
gchar RName[50];
gint i;

strcpy(RName,rindex(FileName,'/')); 
for (i=1;i<strlen(RName);++i) 
    {
    if (RName[i]=='.') { RunName[i-1]='\0'; break; }
    else RunName[i-1]=RName[i];
    }
}
//----------------------------------------------------------------------------------------------------------------------
void *ReadFile(gpointer Data)
{
gint BRead;
gchar Str[40],FName[25],ErrMessg[200],RunName[40],MacroFName[MAX_FNAME_LENGTH];
gfloat Kbps,Evtps,PerCent;
glong FSize,ZBufs,BytsRead,EvtsRead;
struct stat StatBuf;
FILE *ListFile,*MacroFp;
gint FileNo,I,SkipStatus,Unused,i,j;
gushort Buf[500000];
struct timeval Tv;
gdouble StartTime;

bstart_();                                                                            //Call SUBROUTINE BSTART (in user.F)
for (FileNo=0;FileNo<Setup.Offline.NFiles;++FileNo)
    {
    sprintf(Str,"Build %d of %d",FileNo+1,Setup.Offline.NFiles);
    gdk_threads_enter(); gtk_label_set_text(GTK_LABEL(S_Stat[0]),Str); gdk_threads_leave();
    GetRunName(Setup.Offline.ListFName[FileNo],RunName);
    if (Setup.Macro.N)
       {
       sprintf(MacroFName,"%s/%s_x.mac",MacroDir,RunName);
       MacroFp=fopen(MacroFName,"w");
       fprintf(MacroFp,"#Macro file %s opened\n",MacroFName);
       fprintf(MacroFp,"#The following macros are shown as a function of buffer number in the table below\n");
       for (i=0,j=0;i<Setup.Macro.N;++i)
           {
           MacroCurr[i]=MacroPrev[i]=MacroDiff[i]=0.0;
           if (Setup.Macro.Display[i]) { fprintf(MacroFp,"# %s %s-diff",Setup.Macro.Name[i],Setup.Macro.Name[i]); ++j; }
           if (!((j+1)%4)) fprintf(MacroFp,"\n");
           }
       fprintf(MacroFp,"\n");
       }
    fstart_(RunName,strlen(RunName));                                                 //Call SUBROUTINE FSTART (in user.F) 
    if (strcmp(Setup.Offline.SetFName[FileNo],"Current"))
       {
       strcpy(Setup.FName,Setup.Offline.SetFName[FileNo]);
       if (Read(ErrMessg)) { SAttention(ErrMessg); continue; }                            //Read setup file--skip on error
       gdk_threads_enter(); Screen1(NULL,NULL); gdk_threads_leave(); //Screen1 closes all windows before creating new ones
       }
    if (Setup.Offline.Zero[FileNo]) { ZeroOned(-1); ZeroTwod(-1); }
    BRead=atoi(Setup.Offline.BufRead[FileNo]); 
    if (BRead<=0) BRead=-1;                               //If BufRead is "All" or any non-numeric string read all buffers
    Kbps=0.0; Evtps=0.0; AbbreviateFileName(FName,Setup.Offline.ListFName[FileNo],18); ZBufs=0; PerCent=0.0;
    stat(Setup.Offline.ListFName[FileNo],&StatBuf); FSize=StatBuf.st_size;
    sprintf(Str,"File Bytes: %ld",FSize);
    gdk_threads_enter(); gtk_label_set_text(GTK_LABEL(S_Stat[12]),Str); gdk_threads_leave();

    if ((ListFile=fopen(Setup.Offline.ListFName[FileNo],"r"))==NULL)                                     //File not found!
       { sprintf(Str,"Cant open %s",FName); SAttention(Str); continue; }

    gettimeofday(&Tv,NULL); StartTime=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06;                          //Start timer

    for (I=0,SkipStatus=0;I<Setup.Offline.BufSkip[FileNo];++I)                                              //Skip Buffers
        {
        switch (Setup.ListMode.Compr)
           {
           case 0: while ((SkipStatus=FreedomSkip(ListFile))==2); break;                            //Freedom format files
           case 1: case 2: SkipStatus=ZlsGlsSkip(ListFile);                                      //Zls or Gls format files
                                                                         //Skip for Exogam format files is not implemented
           }
        if (SkipStatus==1) break;                                                               //Invalid Signature or EOF
        }
    ZBufs=0; BytsRead=0; EvtsRead=0;
    while (TRUE)                                                                                            //Read Buffers
         {
         if (AnalysisRequest==StopAnalysis || AnalysisRequest==StopAllAnalysis) 
            { 
            Update(StartTime,FName,FSize,ZBufs,BytsRead,EvtsRead); 
            if (AnalysisRequest==StopAnalysis) AnalysisRequest=NormalAnalysis; 
            break; 
            }
         while (AnalysisRequest==PauseAnalysis) sleep(1);                                           //Loop here if paused
         if ((Setup.Offline.BufRefresh>0) && !(ZBufs%Setup.Offline.BufRefresh)) 
            Update(StartTime,FName,FSize,ZBufs,BytsRead,EvtsRead);

         if (Setup.ListMode.Compr==0)                                                              //Freedom format files
            { if (FreedomRead(ListFile,&ZBufs,&EvtsRead,&BytsRead,FALSE,NULL,&Unused)) break; }
         else if (Setup.ListMode.Compr==3)                                                          //Exogam format files
            { 
            if (!fread(Buf,16384,1,ListFile)) break;
            Exogam(Buf,&ZBufs,&EvtsRead,&BytsRead); 
            }
         else {if (ZlsGlsRead(ListFile,Buf,&ZBufs,&EvtsRead,&BytsRead,FALSE,NULL,&Unused)) break;}    //zls and gls files
         if (Setup.Macro.N && !(ZBufs % Setup.Macro.RefreshRate)) ComputeMacros(ZBufs,MacroFp);
         if ( (ZBufs==BRead) && (BRead>0) ) break;                                                        //Limit reached
         }
    fclose(ListFile);
    if (Setup.Offline.SaveSpec[FileNo]) { strcpy(SelRun,Setup.Offline.SpecFName[FileNo]); SaveSpectraOk(NULL,NULL); }
    fstop_();                                                                         //Call SUBROUTINE FSTOP (in user.F)
    if (AnalysisRequest==StopAllAnalysis) break;
    if (Setup.Macro.N) { ComputeMacros(ZBufs,MacroFp); fclose(MacroFp); }
    }
Update(StartTime,FName,FSize,ZBufs,BytsRead,EvtsRead);                    //Update the display at the end of the analysis
bstop_();                                                                             //Call SUBROUTINE BSTOP (in user.F)
ProgramState=Free;
gdk_threads_enter(); gtk_label_set_text(GTK_LABEL(S_Stat[0]),"Status: Free"); gdk_threads_leave();
pthread_exit(NULL);
return NULL;
}
//----------------------------------------------------------------------------------------------------------------------
