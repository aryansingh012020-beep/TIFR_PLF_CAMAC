#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "cc2002.h"
#include "lamps.h"
#include "common.h"
#include "libcamac.h"

#define CMC_CTL_RESET   _IO('Z', 0)
#define CMC_CTL_R1      _IO('Z', 1)

/*Function Templates*/
void WriteSequence(gint Fd,gshort *Cmd,gshort *Arg);
void ExecuteIniCommands(void); void ProgramLP(void); void ProgramLP_QStop(void);
void ExecuteClrCommands(void); void ExecuteExtraCommands(void);
void SAttention(gchar *Messg);
void Attention(gint XPos,gchar *Messg);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void ProcessEvent(gshort *P,gboolean Option);
void ZeroOned(gint SNo); void ZeroTwod(gint SNo);
void RefreshAll(void);
void Safety(gchar *RunName,gdouble *ElapsedTime,gint *SpecSaved);
void PeriodicLog(gint Status,gchar *RunName,glong BuffersAcquired,glong BuffersProcessed,
                 glong BytesWritten,time_t TStart,gdouble StartTime);
void Write1d(gchar *FName,gint i,gboolean InThread);
void Write2d32(gchar *FName,gint i,gboolean InThread);
void Write2d16(gchar *FName,gint i,gboolean InThread);
gulong CalcScaler(guchar *ScalerByte,gint No);
void CorrectScaler(gint No);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
gboolean writeEventBlock(gint eventSize,gint ev_per_block,gushort data[],FILE *Fp);
void writeNames(int noOfAdcs,int noOfSingles,int noOfScalers,FILE *Fp);
void queue_text(enum blocktype block,gchar *txt,FILE *Fp);
void writeEndOfFile(FILE *Fp);
gint Read(gchar *ErrMessg); void StopNicely(void);
void fstart_(gchar *RunName,int Len); void fstop_();
void ComputeMacros(glong BuffersAcquired,FILE *MacroFp);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void GetNaf(gchar *Str,gint *N,gint *A,gint *F);
void RemoveInitialBlanks(gchar *Str);

guint16 *AcqBuf1,*AcqBuf2;                                                                       //Two acquisition buffers
/*External global variables*/
extern gulong ScalerCurr[MAX_SCALER],ScalerPrev[MAX_SCALER];         //Scaler values from the current and previous buffers
extern gint ScalerOverflows[MAX_SCALER];                                         //The number of overflows for each scaler
extern gulong ScalerPending[MAX_SCALER];                                                       //Scaler values after Pause
extern struct Setup Setup;                                                                           //The setup variables
extern enum ProgramState ProgramState;                                          //The current running state of the program
extern gint DrvFd;                                                                                //Driver file descriptor
extern gchar RunName[40],PrevRun[40];                                               //Name of current run and previous run
extern enum AcqSignal AcqSignal;                                                                   //Stop,Pause and Resume
extern GtkWidget *S_Stat[15],*S_Scaler[4];                                                            //Status bar widgets
extern gint RefreshRate,RefreshOptimum;        //The interval (no. of buffers) for screen refreshes, RefreshOptimum=1 or 0
extern gchar SStop[40];                                               //The stop time of time of this, or the previous run
extern gint ScalerWinOpen;
extern GtkWidget **ScalerTotal,**ScalerRate,**ScalerDRate;
extern gboolean BatchRunning;
extern gfloat MacroCurr[MAX_MACRO],MacroPrev[MAX_MACRO],MacroDiff[MAX_MACRO];                 //Computed values of macros
extern glong BytesWritten;
extern struct FileSelectType *FileX;
extern gchar SetupDir[MAX_DIR_STRLEN],ListFDir[MAX_DIR_STRLEN],SpecDir[MAX_DIR_STRLEN],
      BatDir[MAX_DIR_STRLEN],LogDir[MAX_DIR_STRLEN],MacroDir[MAX_DIR_STRLEN];                                 //Dir prefs

//Global Variables used in this file only
glong BuffersAcquired,BuffersProcessed,BufPrev;
gshort NPar1,NPar2;                                                                      //No. of parameters in each crate
gdouble TimePrev,StartTime,PauseDuration,PauseTime;
FILE *LogFp,*ScalerFp;                                                     //The ascii log files: lamps.log and scaler.log
struct Bat *Bat;                                       //This structure holds widgets of the batch processing setup screen
gboolean BatStop;                                                                    //Controls stopping batch acquisition
time_t TStart;

gulong XTotEvts;
gint XWds,XExtraPar,XEvents,XPar,XClrPar,XWdsLeft,XPtr;

//Function templates
void CreateDir(void);

//Function templates and global variables for threads
void *AcquireData(gpointer Data); void *BuildSpectra(void *Data); void *ControlBatch(gpointer Data);  
pthread_t Acq,Spec,CntlBat,AStop;
//----------------------------------------------------------------
gint CamacNAF(gint Data,gint N,gint A,gint F,gint *XRes,gint *QRes,gint *Lam)
//Return value is Result. X=XRes, Q=QRes and Lam=Lam Status are made available 
//Note: The CMC100 response word is 0UUUKLQX DDDDDDDD DDDDDDDD DDDDDDDD
{
gint Response;
Response = camac_naf(DrvFd, Data, N, A, F, XRes, QRes, Lam);
return Response & 0xFFFFFF;
}
//*----------------------------------------------------------------------------------------------------------------------
void CamacZ()
{
gint XRes,QRes,Lam;
(void)CamacNAF(0,28,8,26,&XRes,&QRes,&Lam);
}
//----------------------------------------------------------------------------------------------------------------------
void CamacC()
{
gint XRes,QRes,Lam;
(void)CamacNAF(0,28,9,26,&XRes,&QRes,&Lam);
}
//----------------------------------------------------------------------------------------------------------------------
void SetI()
{
gboolean XRes,QRes,Lam;
(void)CamacNAF(0,30,9,26,&XRes,&QRes,&Lam);
}
//-----------------------------------------------------------------------------------------------------------------------
void ClrI()
{
gboolean XRes,QRes,Lam;
(void)CamacNAF(0,30,9,24,&XRes,&QRes,&Lam);
}
//-----------------------------------------------------------------------------------------------------------------------
void Pad(gchar *Str,gint N)                                            //Adds blanks to Str making it exactly N chars long 
{
gint i;

for (i=strlen(Str);i<N;++i) Str[i]=' '; Str[N]='\0'; return;
}
/*----------------------------------------------------------------------------------------------------------------------*/
gulong CalcScaler(guchar *ScalerByte,gint No)
{
guchar x2;
gulong S0,S1,S2;

S0=(gulong)ScalerByte[4*No]; S1=(gulong)ScalerByte[4*No+1]; S2=(gulong)ScalerByte[4*No+2];
if (S1>127) { if (S2==255) x2=0; else x2=S2+1; }  else x2=S2;
return (S0 + (S1<<8) + (x2<<16));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CorrectScaler(gint i)                                                               //Correction for 24-bit overflow
{
if (ScalerCurr[i]<ScalerPrev[i]) { ScalerOverflows[i]++; ScalerCurr[i]+=SCALER_OVERFLOW; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void UpdateDisplay(gdouble *ElapsedTime,gboolean Scribe,gint XEffectiveByts)
{
struct timeval Tv;
gdouble TimeNow,Kbps,Evtps,DelTime,DKbps,DEvtps,DeadTime,DiffDeadTime;
gchar SDFree[60],SKbps[60],SEvtps[60],SBytes[60],SZSup[60],SBuffs[60],SProc[60],SElap[60],Str[40],SScaler[4][60],
      SEvts[60],SDead[60],NameF[MAX_DIR_STRLEN+20];
glong DelBuf,Masters,DMasters,DEvents;
struct tm *TmStop;
time_t TStop;
struct statfs StatBuf;
gfloat DFree;
gint i;

gettimeofday(&Tv,NULL); TimeNow=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06; 
*ElapsedTime=TimeNow-StartTime-PauseDuration;
Kbps=1.0e-03*XTotEvts*Setup.Parameter.NPar*2.0/(*ElapsedTime); 
DelBuf=BuffersAcquired-BufPrev; DelTime=TimeNow-TimePrev;
DKbps=1.e-03*XEffectiveByts/DelTime; DEvtps=XEvents/DelTime; 
if (RefreshOptimum) RefreshRate=(gint)MAX(2.0e03*DKbps/(1+XEffectiveByts),0.1)*10.0;                 //20 sec base value
if (Kbps>.1) sprintf(SKbps,"Kb/s: %.2f (%.2f)",Kbps,DKbps); else sprintf(SKbps,"Kb/s: %.2e (%.2e)",Kbps,DKbps);
Evtps=XTotEvts/(*ElapsedTime);
if (Evtps<1.0e3) sprintf(SEvtps,"Evt/s: %.2f",Evtps); else sprintf(SEvtps,"Evt/s: %.2fK",1.0e-3*Evtps);
if (DEvtps<1.0e3) sprintf(Str," (%.2f)",DEvtps); else sprintf(Str," (%.2fK)",1.0e-3*DEvtps);
strcat(SEvtps,Str);
sprintf(SElap,"Elapsed: %.1f s",*ElapsedTime);
sprintf(SBytes,"BytesAcqd.: %ld",XTotEvts*Setup.Parameter.NPar*2); 
sprintf(SBuffs,"Buffs Acq: %ld",BuffersAcquired); sprintf(SProc,"Processed: %ld",BuffersProcessed);
sprintf(SEvts,"EventsAcq: %ld",XTotEvts);
if (Setup.Scaler.Master>-1)
   {
   Masters=ScalerCurr[Setup.Scaler.Master]+ScalerPending[Setup.Scaler.Master]; 
   if (Masters) DeadTime=100.0*(1.0-(gdouble)XTotEvts/(gdouble)Masters); else DeadTime=0.0;
   if (DeadTime<0.0) DeadTime=0.0;
   sprintf(SDead,"Dead  Time: %.1f%%",DeadTime);
   DMasters=ScalerCurr[Setup.Scaler.Master]-ScalerPrev[Setup.Scaler.Master];
   DEvents=XEvents;
   if (DMasters && (AcqSignal==Resume) && XTotEvts) 
      { 
      DiffDeadTime=100.0*(1.0-(gdouble)DEvents/(gdouble)DMasters);
      if (DiffDeadTime<0.0) DiffDeadTime=0.0;
      sprintf(Str," (%.1f%%)",DiffDeadTime); strcat(SDead,Str);
      }
   }
TimePrev=TimeNow; BufPrev=BuffersAcquired;
if (Setup.ListMode.ListOn) sprintf(SZSup,"Z.Sup Bytes: %ld",BytesWritten); 
for (i=0;i<MIN(4,Setup.Scaler.NSc);++i) sprintf(SScaler[i],"%s:%ld",Setup.Scaler.Name[i],ScalerCurr[i]+ScalerPending[i]);

if (AcqSignal==Pause)
   {
   sprintf(NameF,"%s/scaler.log",LogDir); ScalerFp=fopen(NameF,"a");
   fprintf(ScalerFp,"Pause:%s Elapsed sec: %.1f\n",ctime(&TStop),*ElapsedTime);
   fclose(ScalerFp);
   }

if (Scribe)
   { 
   TStop=time(NULL); TmStop=localtime(&TStop);
   sprintf(SStop,"Stop: %02d:%02d:%02d",TmStop->tm_hour,TmStop->tm_min,TmStop->tm_sec);
   sprintf(NameF,"%s/lamps.log",LogDir);
   if ((LogFp=fopen(NameF,"a"))==NULL)                                    //Open log file and make the final entries
      { 
      SAttention("Couldnt open log file lamps.log\nFile permissions problem or disk full?"); 
      ProgramState=Free; pthread_exit(NULL); 
      }
   strcpy(Str,ctime(&TStop)); Str[24]='\0'; Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Stop:",Str);
   sprintf(Str,"%.1f",*ElapsedTime);        Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Elapsed sec:",Str);
   sprintf(Str,"%ld",BuffersAcquired);      Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Buffers Acquired:",Str);
   sprintf(Str,"%ld",BuffersProcessed);     Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Buffers Processed:",Str);
   sprintf(Str,"%ld",XTotEvts);             Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Events:",Str);
   sprintf(Str,"%.2f",Evtps);               Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Evt/s:",Str);
   if (Setup.Scaler.Master>-1) sprintf(Str,"%.1f%%",DeadTime); else sprintf(Str,"-");
                                            Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Dead Time:",Str);
   if (Setup.ListMode.ListOn) sprintf(Str,"%ld",BytesWritten); else sprintf(Str,"No list");
                                            Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","List Bytes:",Str);
   for (i=0;i<4;i++)                    //We always write only 4 scaler values here. All scalers are written to scaler.log
       { 
       if (i<=Setup.Scaler.NSc) sprintf(Str,"%ld",ScalerCurr[i]+ScalerPending[i]); else sprintf(Str,"0");
       Pad(Str,30); fprintf(LogFp,"%-6s%02d: %13s %s}\n","Scaler",i+1," ",Str); 
       }
   fprintf(LogFp,"-------------------------------------------------------\n");
   fclose(LogFp);

   sprintf(NameF,"%s/scaler.log",LogDir);
   if ((ScalerFp=fopen(NameF,"a"))==NULL)                        //Open scaler log file and make the final entries
      { 
      SAttention("Couldnt open log file scaler.log\nFile permissions problem or disk full?"); 
      ProgramState=Free; pthread_exit(NULL); 
      }
   if (AcqSignal==Stop) fprintf(ScalerFp,"Stop: %s Elapsed sec: %.1f\n",ctime(&TStop),*ElapsedTime);
   for (i=0;i<Setup.Scaler.NSc;++i) 
       fprintf(ScalerFp,"Scaler%02d %s %ld\n",i+1,Setup.Scaler.Name[i],ScalerCurr[i]+ScalerPending[i]);
   fprintf(ScalerFp,"-------------------------------------------------------\n");
   fclose(ScalerFp);
   }

gdk_threads_enter();
gtk_label_set_text(GTK_LABEL(S_Stat[ 9]),SKbps);   
gtk_label_set_text(GTK_LABEL(S_Stat[10]),SEvtps);
gtk_label_set_text(GTK_LABEL(S_Stat[11]),SBytes); 
gtk_label_set_text(GTK_LABEL(S_Stat[13]),SEvts); 
if (Setup.Scaler.Master>-1) gtk_label_set_text(GTK_LABEL(S_Stat[14]),SDead); 
if (Setup.ListMode.ListOn) 
   {
   statfs("./",&StatBuf);
   DFree=(gfloat)StatBuf.f_bavail*StatBuf.f_bsize/1024.0/1024.0/1024.0; sprintf(SDFree,"Free: %-3.3fGb",DFree);
   if (DFree<1.0) { DFree=1024.0*DFree; sprintf(SDFree,"Free: %-3.1fMb",DFree); }
   if (DFree<1.0) { DFree=1024.0*DFree; sprintf(SDFree,"Free: %-3.1fKb",DFree); }
   gtk_label_set_text(GTK_LABEL(S_Stat[2]),SDFree);                                                   //Free space on disk
   gtk_label_set_text(GTK_LABEL(S_Stat[12]),SZSup);                                                          //Z.Sup Bytes
   }
gtk_label_set_text(GTK_LABEL(S_Stat[ 7]),SBuffs); 
gtk_label_set_text(GTK_LABEL(S_Stat[ 8]),SProc);   
gtk_label_set_text(GTK_LABEL(S_Stat[ 5]),SElap);   
for (i=0;i<MIN(4,Setup.Scaler.NSc);i++) gtk_label_set_text(GTK_LABEL(S_Scaler[i]),SScaler[i]);
if (AcqSignal == Stop) 
   { gtk_label_set_text(GTK_LABEL(S_Stat[6]),SStop); gtk_label_set_text(GTK_LABEL(S_Stat[0]),"Status: Free"); }
if (BatchRunning)
   { sprintf(Str,"Batch: %d/%d",Setup.BatAcq.Current,Setup.BatAcq.NBat); gtk_label_set_text(GTK_LABEL(S_Stat[0]),Str); }
RefreshAll();
if (ScalerWinOpen)
   {
   for (i=0;i<Setup.Scaler.NSc;++i)
       {
       sprintf(Str,"%ld",ScalerCurr[i]+ScalerPending[i]);
       gtk_label_set_text(GTK_LABEL(GTK_BIN(ScalerTotal[i])->child),Str);
       if (*ElapsedTime>0.5) sprintf(Str,"%.1f",(gdouble)(ScalerCurr[i]+ScalerPending[i])/(*ElapsedTime)); 
       gtk_label_set_text(GTK_LABEL(GTK_BIN(ScalerRate[i])->child),Str);
       if (*ElapsedTime>0.5) sprintf(Str,"%.1f",(gdouble)(ScalerCurr[i]-ScalerPrev[i])*RefreshRate/DelTime); 
       gtk_label_set_text(GTK_LABEL(GTK_BIN(ScalerDRate[i])->child),Str);
       }
   }
gdk_threads_leave();
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ZlsError(void)
{
SAttention("Error: Disk Full...Acq Stopped!"); 
StopNicely();
}
/*----------------------------------------------------------------------------------------------------------------------*/
gboolean CheckLGates(gushort *Par)                                                                //Checks list mode gates
{
gint i;

for (i=0;i<Setup.ListMode.NoOfLGates;++i)
    {
    if ((Par[Setup.ListMode.LGates[i].Para-1] < Setup.ListMode.LGates[i].Lo) || 
        (Par[Setup.ListMode.LGates[i].Para-1] > Setup.ListMode.LGates[i].Hi)) return FALSE;
    }
return TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint CompressToDisk(FILE *Fp,gushort *AcqBuf)                      //At present the zls and candle formats are implemented
{
gint i,j,k,l,m,Ptr1,Ptr2,CPtr,CBufSiz,HdrWds;
gushort Par[MAX_PAR]; 
gushort CompBuf[500000];                                 //The compressed buffer can be large, the dimension is not finalised!
gushort BitMask[MAX_HDR];                                                //The event masks, 1 mask for every 16 parameters
static gchar Sign[4] = {"DAPS"};
gshort Code;
gboolean AcceptEvent;                                 //If all parameters are outside [LLD,ULD] we reject the entire event

switch (Setup.ListMode.Compr)
   {
   case 0: //NSC Candle format. Here CompBuf is used to hold the crate combined data
           for (i=0,Ptr1=0,Ptr2=XWds,CPtr=0;i<XEvents;++i) 
               {
               for (j=0;j<NPar1;++j) CompBuf[CPtr+j]=AcqBuf[Ptr1+j];
               CPtr+=NPar1;
               for (j=0;j<NPar2;++j) CompBuf[CPtr+j]=AcqBuf[Ptr2+j];
               CPtr+=NPar2;
               Ptr1+=NPar1; Ptr2+=NPar2;
               }
           if (!writeEventBlock(Setup.Parameter.NPar,XEvents,CompBuf,Fp)) { ZlsError(); return FALSE; }
           break;
   case 1: //Normal zls format
           HdrWds=(Setup.Parameter.NPar+15)/16;
           for (i=0,Ptr1=0,Ptr2=XWds,CPtr=0,AcceptEvent=FALSE;i<XEvents;++i)                 //Loop over events in AcqBuf
               {
               for (j=0;j<HdrWds;++j) BitMask[j]=0;
               for (j=0,m=0;j<Setup.Parameter.NPar;++j)
                   {
                   k=j/16; l=j%16;
                   if (j<NPar1) Par[j]=AcqBuf[Ptr1+j]; else Par[j]=AcqBuf[Ptr2+j-NPar1];
                   if ((Par[j] >= Setup.Parameter.LLD[j]) && (Par[j] <= Setup.Parameter.ULD[j])) 
                      { AcceptEvent=TRUE; BitMask[k]|=1<<l; CompBuf[CPtr+HdrWds+m]=Par[j]; m++; }
                   }
               if (Setup.ListMode.NoOfLGates) if (!CheckLGates(Par)) AcceptEvent=FALSE;                  //Gated List Mode
               if (AcceptEvent)
                  {
                  for (j=0;j<HdrWds;++j) CompBuf[CPtr+j]=BitMask[j];
                  CPtr+=HdrWds+m;
                  }
               Ptr1+=NPar1; Ptr2+=NPar2;
               }
                      // g_print("CPtr (Max Dimension)=%d Limit is 500000\n",CPtr);
           CBufSiz=2*CPtr;
           if (fwrite(Sign,4,1,Fp)<1) { ZlsError(); return FALSE; }
           if (fwrite(&CBufSiz,4,1,Fp)<1) { ZlsError(); return FALSE; }
           if (fwrite(CompBuf,2,CPtr,Fp)<CPtr) { ZlsError(); return FALSE; } 
           BytesWritten+=(8+2*CPtr);
              //g_print("BytesWritten=%d\n",BytesWritten);
           break;
   case 2: case 3: 
           AcqSignal=Stop; Code=STOP_ACQN; camac_write(DrvFd,&Code,sizeof(short)); 
           SAttention("Error: List format presently unsupported");
   }
return TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void *BuildSpectra(void *Data)  //Made for 1 crate only
{
gushort *AcqBuf;
gshort Para[MAX_TOTAL_PAR];
gint Evt,i,Ptr;

AcqBuf=(gushort *)Data;
for (Evt=0,Ptr=0;Evt<XEvents;++Evt)
   {
   for (i=0;i<NPar1;++i) Para[i]=AcqBuf[Ptr+i];
   ProcessEvent(Para,FALSE); Ptr+=NPar1;
   }
BuffersProcessed++; pthread_exit(NULL);
return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void *ControlAutoStop(gpointer Unused)
{ sleep(Setup.Hardware.AutoStop); StopNicely(); pthread_exit(NULL); return NULL;}
/*----------------------------------------------------------------------------------------------------------------------*/
void ExecuteClrCommands(void)
{
gint XRes,QRes,Lam,xN,xA,xF;
gchar ClrName[MAX_FNAME_LENGTH],Str[256];
FILE *Fp;

sprintf(ClrName,"%s/memory1.clr",SetupDir);
if ( (Fp=fopen(ClrName,"r"))==NULL) { SAttention("clr file missing!"); return; }
while (TRUE)
   {
   if (fgets(Str,80,Fp)==NULL) break;
   RemoveInitialBlanks(Str);
   if (!strncmp(Str,"END", 3) ) break;
   if (!strncmp(Str,"SETZ",4) ) CamacZ();
   if (!strncmp(Str,"SETI",4) ) SetI();
   if (!strncmp(Str,"CLRI",4) ) ClrI();
   if (!strncmp(Str,"CLRC",4) ) CamacC();
   if (!strncmp(Str,"NAF", 3) ) { GetNaf(Str,&xN,&xA,&xF); (void)CamacNAF(0,xN,xA,xF,&XRes,&QRes,&Lam); }
   if (!strncmp(Str,"DLY", 3) ) SAttention("DLY Command in CLR file not permitted!");
   if (!strncmp(Str,"DATA",4) ) SAttention("DATA Command in CLR file not permitted");
   }
fclose(Fp);
}
//----------------------------------------------------------------------------------------------------------------------
void ExecuteExtraCommands(void)
{
//If LAM is on a Phillips unit and CM90 is not used, we need to clear the Phillips unit to start acquisition
//This is because controller LAM was not unmasked while doing ExecuteIniCommands() 
gint N,XRes,QRes,Lam;

if (CheckPhillipsLam(&N) && !Setup.Hardware.UseGtSup)
   {
   (void)CamacNAF(0,N,3,11,&XRes,&QRes,&Lam);
   }
}
//----------------------------------------------------------------------------------------------------------------------
void HardResetCmc100(void)                                          //To empty the CMC100 FIFO and also the USB cache data
{ 
DrvFd=camac_open("/dev/cmcamac0");
camac_reset(DrvFd); 
camac_close(DrvFd);
}
//-----------------------------------------------------------------------------------------------------------------------
void PrintBuffer(guint *CBuf,gint CWds)
{
gint i;

g_print("Buffer contains %d words of which %d are shown below:\n",CWds,Setup.Print.Wds);
for (i=0;i<Setup.Print.Wds;++i) 
    {
    switch (CBuf[i] & 0xFFFFFF)
           {
           case PHILLIPS_START:    g_print("i=%d -----PHILLIPS_START-------\n",i); break;
           case SEQUENTIAL_START:  g_print("i=%d -----SEQUENTIAL_START-----\n",i); break;
           case WIENER_START:      g_print("i=%d -----WIENER_START---------\n",i); break;
           case SCALERS_START:     g_print("i=%d -----SCALERS_START--------\n",i); break;
           case CLEAR_START:       g_print("i=%d -----CLEAR_START----------\n",i); break;
           case PHILLIPS_END:      g_print("i=%d =====PHILLIPS_END=========\n",i); break;
           case SEQUENTIAL_END:    g_print("i=%d =====SEQUENTIAL_END=======\n",i); break;
           case WIENER_END:        g_print("i=%d =====WIENER_END===========\n",i); break;
           case SCALERS_END:       g_print("i=%d =====SCALERS_END==========\n",i); break;
           case CLEAR_END:         g_print("i=%d =====CLEAR_END============\n",i); break;
           default:                g_print("i=%d CBuf=%X Q=%d\n",i,CBuf[i],(CBuf[i]>>25) & 1);
           }
    }
}
//-----------------------------------------------------------------------------------------------------------------------
void DecodeSparseData(guint *CBuf,gint CWds,gushort *AcqBuf)
{
//CWds=no of words in CBuf
//Must calculate: XEvents,XWdsLeft,XPtr (all global)
//XEvents=no of complete events in buf, XWdsLeft no of unprocessed words starting at CBuf[XPtr]
gint i,Ptr,PtrEvtStart,ModuleNo,Q,BlockSeparator,AcqPtr,Stn,SubA,PNo;

if (Setup.Print.Yes && (BuffersAcquired==Setup.Print.BufferNo)) PrintBuffer(CBuf,CWds);

Ptr=0; XEvents=0; PtrEvtStart=0; ModuleNo=0; AcqPtr=0;
while (TRUE)
   {
   BlockSeparator=CBuf[Ptr] & 0xFFFFFF;                                   //To check 24-bit literal inserted in data stream
   switch (BlockSeparator)
      {
      case PHILLIPS_START:   Stn=Setup.Decode.Module[ModuleNo];
                             for (i=0;i<Setup.Hardware.NParStn[Stn];++i) AcqBuf[AcqPtr+i]=0;
                             for (i=0;i<17;++i) //Not more than 17 reads in any case
                                   {
                                   ++Ptr; if (Ptr==CWds) { XPtr=PtrEvtStart; XWdsLeft=CWds-PtrEvtStart; return; }
                                   if ((CBuf[Ptr] & 0xFFFFFF) == PHILLIPS_END) break;
                                   Q=(CBuf[Ptr]>>25) & 1;
                                   if (Q) { SubA=(CBuf[Ptr]>>12) & 0xF; AcqBuf[AcqPtr+SubA]=CBuf[Ptr] & 0xFFF; }
                                   }
                             ++ModuleNo; AcqPtr+=Setup.Hardware.NParStn[Stn];
                             break;
      case SEQUENTIAL_START: Stn=Setup.Decode.Module[ModuleNo];
                             for (i=0,PNo=Setup.Hardware.LoParStn[Stn];i<Setup.Hardware.NParStn[Stn];++i) 
                                 {
                                 ++Ptr; if (Ptr==CWds) { XPtr=PtrEvtStart; XWdsLeft=CWds-PtrEvtStart; return; }
                                 AcqBuf[AcqPtr+i]=CBuf[Ptr] & (Setup.Parameter.Chan[PNo++]-1);
                                 }
                             ++Ptr; if (Ptr==CWds) { XPtr=PtrEvtStart; XWdsLeft=CWds-PtrEvtStart; return; }
                             if ((CBuf[Ptr] & 0xFFFFFF) != SEQUENTIAL_END) g_print("Missing separator SEQUENTIAL_END\n");
                             ++ModuleNo; AcqPtr+=Setup.Hardware.NParStn[Stn];
                             break;
      case WIENER_START:     break; //Not programmed
      case SCALERS_START:    for (i=0;i<Setup.Scaler.NSc1;++i)
                                 {
                                 ++Ptr; if (Ptr==CWds) { XPtr=PtrEvtStart; XWdsLeft=CWds-PtrEvtStart; return; }
                                 ScalerCurr[i]= CBuf[Ptr] & 0xFFFFFF;
                                 }
                             ++Ptr; if (Ptr==CWds) { XPtr=PtrEvtStart; XWdsLeft=CWds-PtrEvtStart; return; }
                             if ((CBuf[Ptr] & 0xFFFFFF) != SCALERS_END) g_print("Missing separator SCALERS_END\n");
                             ++ModuleNo;
                             break;
      case CLEAR_START:      //N.B. There is always a CLEAR_START and CLEAR_END for every event
                             do
                               {
                               ++Ptr; if (Ptr==CWds) { XPtr=PtrEvtStart; XWdsLeft=CWds-PtrEvtStart; return; } 
                               } while ((CBuf[Ptr] & 0xFFFFFF) != CLEAR_END);
                             ++XEvents; ModuleNo=0; PtrEvtStart=Ptr+1;
                             break;
      default:               g_print("ERROR::Unidentified module code at Ptr=%d\n",Ptr);
      }
   ++Ptr; if (Ptr==CWds) { XPtr=PtrEvtStart; XWdsLeft=CWds-PtrEvtStart; return; }
   }
}
//-----------------------------------------------------------------------------------------------------------------------
void *AcquireData(gpointer Data)                                     //Data acquisition thread triggered by StartCallBack
{
gint i,j,Size,SpecSaved,NewBufs,I,ll;
gchar Buf[512],Header[5],FNam[MAX_FNAME_LENGTH],Str[256],NSC_txt[256],FileMode[10];
gchar IniName[MAX_FNAME_LENGTH],NameF[MAX_DIR_STRLEN+20];
guchar ScalerByte[4*MAX_SCALER];
glong PollTime,CollectedEvents;
gulong BAcq;
FILE *Fp,*ListFp,*MacroFp;
gshort NScaler1,NScaler2,ConfirmBufSiz[2];
gshort InitCmd[256],InitArg[256],ClrCmd[25],ClrArg[25],Cmd,Code;
gshort Naf[300],Mask[300],ScalerNaf[MAX_SCALER];
struct timeval Tv;
gdouble ElapsedTime;
struct tm *TmStart;
gushort *AcqBuf;

//Start new variables for Cmc100
gint XData,XRes,QRes,Lam,XByts,XStatus;
guint32 *CBuf,OldCBuf[MAX_PAR+MAX_SCALER+200];
//OldCBuf holds the last part (incomplete event) of CBuf which DecodeSparseData could not process  
gboolean XAcqDone;
//End new variables for Cmc100

AcqBuf1=g_new(guint16,16*CMC_BUF);
AcqBuf2=g_new(guint16,16*CMC_BUF);
CBuf=g_new(guint32,CMC_BUF);

for (i=0,NPar1=NPar2=0;i<Setup.Parameter.NPar;++i) { if (Setup.Parameter.N[i]>MAX_CAMAC_STNS) ++NPar2; else ++NPar1; } 
for (i=0,NScaler1=NScaler2=0;i<Setup.Scaler.NSc;++i) 
    { if (Setup.Scaler.C[i]==1) ++NScaler1; else ++NScaler2; }
for (i=0;i<NPar1;++i) Mask[i]=Setup.Parameter.Chan[i]-1;

if (!Setup.Simulator)
   {
   (void)CamacNAF(0,30,10,26,&XRes,&QRes,&Lam);                                             //Unmask LAM at all stations
   (void)CamacNAF(4,30,0,17,&XRes,&QRes,&Lam);                                                   //Allow LAM to start LP
   ExecuteClrCommands();                                                //Execute clr file commands to get the first LAM
   ExecuteExtraCommands();              //E.g. For Phillips LAM and no CM90, explicitly clear the Phillips unit to start
   (void)CamacNAF(0,Setup.Scaler.N[0],0,9,&XRes,&QRes,&Lam);  //Clear Scaler, assume there is only 1 scaler and it is cleared by A0.F9
   }
else g_printf("...AcquireData thread in Simulator Mode\n");

if (Setup.ListMode.ListOn) 
   {
   switch (Setup.ListMode.Compr)
      {
      case 0: sprintf(FNam,"%s/%s.001",ListFDir,RunName); break;                                //NSC Candle file format
      case 1: sprintf(FNam,"%s/%s.zls",ListFDir,RunName);                //Normal: The usual zls zero suppression scheme
      }
   if (AcqSignal==Pause) strcpy(FileMode,"a"); else strcpy(FileMode,"w");            //If run was paused use append mode 
   if ((ListFp=fopen(FNam,FileMode))==NULL)
      {
      SAttention("Couldnt open List File\nFile permissions problem or disk full?");
      ProgramState=Free; pthread_exit(NULL);
      }
   if (AcqSignal!=Pause) BytesWritten=0;
   if (Setup.ListMode.Compr==0)                                                                 //NSC Candle file header
      {
      if (AcqSignal==Pause)
         { sprintf(NSC_txt,"RESUME: %s resumed. Run #%04d\n",RunName,111); queue_text(resume,NSC_txt,ListFp); }
      else
         {
         sprintf(NSC_txt,"%s\n %s\n %s\n","LAMPS","A.Chatterjee","Candle Compatible File");
         queue_text(user,NSC_txt,ListFp); writeNames(Setup.Parameter.NPar,0,0,ListFp);
         sprintf(NSC_txt,"START : %s started. Run #%04d\n",RunName,111); queue_text(start,NSC_txt,ListFp);
         }
      }
   }

if (Setup.Macro.N) 
   {
   sprintf(FNam,"%s/%s.mac",MacroDir,RunName);
   if ((MacroFp=fopen(FNam,"w"))==NULL)
      {
      SAttention("Couldnt open file for macro dump\nFile permissions problem or disk full?");
      ProgramState=Free; pthread_exit(NULL);
      }
   }

if (AcqSignal!=Pause)
   { gettimeofday(&Tv,NULL); StartTime=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06; TStart=time(NULL); }
TmStart=localtime(&TStart);
gdk_threads_enter();
gtk_label_set_text(GTK_LABEL(S_Stat[0]),"Status: Acquiring");
gtk_label_set_text(GTK_LABEL(S_Stat[3]),RunName);
sprintf(Str,"Start: %02d:%02d:%02d",TmStart->tm_hour,TmStart->tm_min,TmStart->tm_sec); 
gtk_label_set_text(GTK_LABEL(S_Stat[4]),Str);
sprintf(Str,"Prev. %s",SStop); gtk_label_set_text(GTK_LABEL(S_Stat[6]),Str);
gdk_threads_leave();

if (Setup.Macro.N) 
   {
   fprintf(MacroFp,"#Macro file %s.mac opened at %02d:%02d:%02d\n",RunName,TmStart->tm_hour,TmStart->tm_min,
                    TmStart->tm_sec);
   fprintf(MacroFp,"#The following macros are shown as a function of buffer number in the table below\n");
   for (i=0,j=0;i<Setup.Macro.N;++i)
       {
       MacroCurr[i]=MacroPrev[i]=MacroDiff[i]=0.0;
       if (Setup.Macro.Display[i]) { fprintf(MacroFp,"# %s %s-diff",Setup.Macro.Name[i],Setup.Macro.Name[i]); ++j; }
       if (!((j+1)%4)) fprintf(MacroFp,"\n");
       }
    fprintf(MacroFp,"\n");
    }

for (i=0;i<Setup.Scaler.NSc;++i) { ScalerCurr[i]=0; ScalerOverflows[i]=0; }
if (AcqSignal!=Pause) for (i=0;i<Setup.Scaler.NSc;++i) ScalerPending[i]=0; 
gdk_threads_enter(); RefreshAll(); 
gtk_label_set_text(GTK_LABEL(S_Stat[5]),""); for (i=7;i<15;++i) gtk_label_set_text(GTK_LABEL(S_Stat[i]),"");
gdk_threads_leave(); 

sprintf(NameF,"%s/lamps.log",LogDir);
if ((LogFp=fopen(NameF,"r"))==NULL)
   {                                                                                    //No log file...so make a new one
   if ((LogFp=fopen(NameF,"w"))==NULL)                                 //Create log file and write default headings in it
      { 
      SAttention("Couldnt create file lamps.log\nFile permissions problem or disk full?"); 
      ProgramState=Free; pthread_exit(NULL); 
      }
   fprintf(LogFp,"Ion, q+             \nE, TV               \nNMR, I              \nCI (scale)          \n");
   fprintf(LogFp,"Target              \nTgt. Rot.           \nInfo 1              \nInfo 2              \n");
   fprintf(LogFp,"Info 3              \nInfo 4              \nInfo 5              \nInfo 6              \n");
   fprintf(LogFp,"-------------------------------------------------------\n");
   fclose(LogFp);
   }
else fclose(LogFp);

if (AcqSignal!=Pause)
   {
   sprintf(NameF,"%s/lamps.log",LogDir);
   if ((LogFp=fopen(NameF,"a"))==NULL)                           //Open log file and make the initial entries in it
      { 
      SAttention("Couldnt open log file lamps.log\nFile permissions problem or disk full?"); 
      ProgramState=Free; pthread_exit(NULL); 
      }
   sprintf(Str,"%s",RunName); Pad(Str,30); fprintf(LogFp,"%-23s %s}\n","Run Name:",Str);
   AbbreviateFileName(Str,Setup.FName,MAX_FNAME_LENGTH); if (Setup.Modified) strcat(Str,"(?)");
   if (!strlen(Setup.FName)) sprintf(Str,"%s","none");
   Pad(Str,30); fprintf(LogFp,"%-23s %s}\n","Setup File:",Str);
   strcpy(Str,ctime(&TStart)); Str[24]='\0'; Pad(Str,30);    fprintf(LogFp,"%-23s %s}\n","Start:",Str);
   if (Setup.Oned.WSz==1) sprintf(Str,"%d 16-bit",Setup.Oned.N); else sprintf(Str,"%d 32-bit",Setup.Oned.N);
   Pad(Str,30); fprintf(LogFp,"%-23s %s}\n","1d_Spec:",Str); 
   if (Setup.Twod.WSz==1) sprintf(Str,"%d 16-bit",Setup.Twod.N); else sprintf(Str,"%d 32-bit",Setup.Twod.N);
   Pad(Str,30); fprintf(LogFp,"%-23s %s}\n","2d_Spec:",Str); 
   fprintf(LogFp,"%-23s %30s}\n","User  1:"," "); fprintf(LogFp,"%-23s %30s}\n","User  2:"," "); 
   fprintf(LogFp,"%-23s %30s}\n","User  3:"," "); fprintf(LogFp,"%-23s %30s}\n","User  4:"," "); 
   fprintf(LogFp,"%-23s %30s}\n","User  5:"," "); fprintf(LogFp,"%-23s %30s}\n","User  6:"," ");
   fprintf(LogFp,"%-23s %30s}\n","User  7:"," "); fprintf(LogFp,"%-23s %30s}\n","User  8:"," "); 
   fprintf(LogFp,"%-23s %30s}\n","User  9:"," "); fprintf(LogFp,"%-23s %30s}\n","User 10:"," "); 
   fprintf(LogFp,"%-23s %30s}\n","User 11:"," "); fprintf(LogFp,"%-23s %30s}\n","User 12:"," ");
   fprintf(LogFp,"%-23s %40s]\n","Remarks 1:"," "); fprintf(LogFp,"%-23s %40s]\n","Remarks 2:"," "); 
   fprintf(LogFp,"%-23s %40s]\n","Remarks 3:"," "); fprintf(LogFp,"%-23s %40s]\n","Remarks 4:"," "); 
   fclose(LogFp);
   }

if (AcqSignal==Pause) PeriodicLog(4,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);
else
   {
   BuffersProcessed=0; BuffersAcquired=0; BufPrev=0; NewBufs=0;
   PeriodicLog(0,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);   //periodic.log initial entries
   sprintf(NameF,"%s/scaler.log",LogDir);
   if ((ScalerFp=fopen(NameF,"a"))==NULL)                           //Open scaler log file and make initial entries
      { 
      SAttention("Couldnt open log file scaler.log\nFile permissions problem or disk full?"); 
      ProgramState=Free; pthread_exit(NULL); 
      }
   fprintf(ScalerFp,"%s Start:%s\n",RunName,ctime(&TStart));
   fclose(ScalerFp);
   }

switch (Setup.ListMode.Rate)
   {
   case 0: PollTime=1.e6*256.0/(10.0*NPar1); break;
   case 1: PollTime=1.e6*256.0/(6.0*NPar1);  break;
   case 2: PollTime=1.e6*256.0/(1.25*NPar1); break;
   case 3: PollTime=1.e6*256.0/(0.25*NPar1); break;
   case 4: PollTime=1.e6*256.0/(0.01*NPar1);
   }
if (PollTime>2.0e6) PollTime=2.0e6;

if (Setup.Hardware.AutoStopOn) pthread_create(&AStop,NULL,ControlAutoStop,NULL);                    //Thread for Autostop

for (i=0;i<Setup.Scaler.NSc1;++i) { ScalerPrev[i]=0; ScalerCurr[i]=0; }
ElapsedTime=0.0; SpecSaved=0;
if (AcqSignal!=Pause) XTotEvts=0;
AcqSignal=Resume;                                                            //We set this when acquisition is in progress 
UpdateDisplay(&ElapsedTime,FALSE,0); 

XAcqDone=FALSE; BuffersAcquired=0; XWdsLeft=0;
//Acquisition loop begins here

if (Setup.Simulator) 
   {
   if (!Setup.Hardware.CamacMode)  //Normal Camac mode
      {
      while (TRUE)
         {
         usleep(PollTime);
         FillBufNormalCamac(100,CBuf); XByts=100*4*Setup.Parameter.NPar; XWds=XByts/4;  //Fills 100 simulated events per buffer
         ++BuffersAcquired;
         if (BuffersAcquired%2) AcqBuf=AcqBuf1; else AcqBuf=AcqBuf2;                         //Acquire alternately on 2 buffers
         XExtraPar=0;                                                     //No clr parameters and no scalers in simulation mode
         XPar=NPar1+XExtraPar; XEvents=XWds/XPar;
         for (i=0;i<XEvents;++i) for (j=0;j<NPar1;++j) AcqBuf[i*NPar1+j]=CBuf[i*XPar+j] & Mask[j]; 
         for (i=0;i<Setup.Scaler.NSc1;++i) { ScalerPrev[i]=ScalerCurr[i]; ScalerCurr[i]=CBuf[(XEvents-1)*XPar+NPar1+i] & 0xFFFFFF; }
         XTotEvts+=XEvents; ++NewBufs;
         if (BuffersAcquired>1) pthread_cancel(Spec);              //Request cancellation of the Spec thread (if it is running)
         if (Setup.ListMode.ListOn) if (!CompressToDisk(ListFp,AcqBuf)) break;
         if (BuffersAcquired>1) pthread_join(Spec,NULL);    //Make sure that the Spec is really killed before starting it again
         pthread_create(&Spec,NULL,BuildSpectra,(void *)AcqBuf);
         if (RefreshRate && (NewBufs>=RefreshRate))
            { UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1); NewBufs=0; Safety(RunName,&ElapsedTime,&SpecSaved); }
         else if (BuffersAcquired==1) UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1);
         if ((Setup.PLogSetup.On) && !(BuffersAcquired % Setup.PLogSetup.BufCount))
            PeriodicLog(1,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);
         if (Setup.Macro.N && !(BuffersAcquired % Setup.Macro.RefreshRate)) ComputeMacros(BuffersAcquired,MacroFp);
         if (XAcqDone) break;                              //The loop will be executed once more so as to flush the last buffer
         if ((AcqSignal==Stop) || (AcqSignal==Pause)) XAcqDone=TRUE;
         }
      }
   else                           //Sparse readout Camac mode
      {
      //while (TRUE) 
      for (ll=0;ll<1;++ll)
         {
         usleep(PollTime);
         XWds=FillBufSparseCamac(1,&CBuf[XWdsLeft]); XByts=4*XWds;
            g_print("After FillSparseCamac..XWds=%d\n",XWds);
            for (I=0;I<XWds;++I) g_print("%X ",CBuf[I]); g_print("\n");
         ++BuffersAcquired;
         if (BuffersAcquired%2) AcqBuf=AcqBuf1; else AcqBuf=AcqBuf2;                         //Acquire alternately on 2 buffers
         for (I=0;I<XWdsLeft;++I) CBuf[I]=OldCBuf[I];                       //Place the unprocessed part of the previous buffer
         DecodeSparseData(CBuf,XWdsLeft+XByts/4,AcqBuf);
         for (I=0;I<XWdsLeft;++I) OldCBuf[I]=CBuf[XPtr+I];                   //Hold the unprocessed part of the previous buffer
         XTotEvts+=XEvents; ++NewBufs;
         if (BuffersAcquired>1) pthread_cancel(Spec);              //Request cancellation of the Spec thread (if it is running)
         if (Setup.ListMode.ListOn) if (!CompressToDisk(ListFp,AcqBuf)) break;
         if (BuffersAcquired>1) pthread_join(Spec,NULL);    //Make sure that the Spec is really killed before starting it again
         pthread_create(&Spec,NULL,BuildSpectra,(void *)AcqBuf);
         if (RefreshRate && (NewBufs>=RefreshRate))
            { UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1); NewBufs=0; Safety(RunName,&ElapsedTime,&SpecSaved); }
         else if (BuffersAcquired==1) UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1);
         if ((Setup.PLogSetup.On) && !(BuffersAcquired % Setup.PLogSetup.BufCount))
            PeriodicLog(1,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);
         if (Setup.Macro.N && !(BuffersAcquired % Setup.Macro.RefreshRate)) ComputeMacros(BuffersAcquired,MacroFp);
         if (XAcqDone) break;                              //The loop will be executed once more so as to flush the last buffer
         if ((AcqSignal==Stop) || (AcqSignal==Pause)) XAcqDone=TRUE;
         }
      }
   }
else
{
if (!Setup.Hardware.CamacMode)
{
while (TRUE)
    {
    for (I=0;I<20;++I)                                               //Get controller buffer status repeating for validity
        {
        XStatus=camac_status(DrvFd);
        if (!((XStatus&8)>>3)) break;
        usleep(500);
        }
    if (!(XStatus&3) && (AcqSignal==Resume)) usleep(PollTime);
    XData=0x0E000000; camac_write(DrvFd,&XData,4);                                                                       //Flush
    XByts=camac_read(DrvFd,CBuf,4*CMC_BUF); XWds=XByts/4;
    if (XByts>0)
       {
       ++BuffersAcquired;
       if (BuffersAcquired%2) AcqBuf=AcqBuf1; else AcqBuf=AcqBuf2;                         //Acquire alternately on 2 buffers
       XPar=NPar1+XExtraPar; XEvents=XWds/XPar; 
       for (i=0;i<XEvents;++i) for (j=0;j<NPar1;++j) AcqBuf[i*NPar1+j]=CBuf[i*XPar+j] & Mask[j];
       for (i=0;i<Setup.Scaler.NSc1;++i) { ScalerPrev[i]=ScalerCurr[i]; ScalerCurr[i]=CBuf[(XEvents-1)*XPar+NPar1+i] & 0xFFFFFF; }
       XTotEvts+=XEvents; ++NewBufs;
       if (BuffersAcquired>1) pthread_cancel(Spec);              //Request cancellation of the Spec thread (if it is running)
       if (Setup.ListMode.ListOn) if (!CompressToDisk(ListFp,AcqBuf)) break;
       if (BuffersAcquired>1) pthread_join(Spec,NULL);    //Make sure that the Spec is really killed before starting it again
       pthread_create(&Spec,NULL,BuildSpectra,(void *)AcqBuf);
       if (RefreshRate && (NewBufs>=RefreshRate)) 
          { UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1); NewBufs=0; Safety(RunName,&ElapsedTime,&SpecSaved); }
       else if (BuffersAcquired==1) UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1);
       if ((Setup.PLogSetup.On) && !(BuffersAcquired % Setup.PLogSetup.BufCount)) 
          PeriodicLog(1,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);
       if (Setup.Macro.N && !(BuffersAcquired % Setup.Macro.RefreshRate)) ComputeMacros(BuffersAcquired,MacroFp);
       }
    if (XAcqDone) break;                              //The loop will be executed once more so as to flush the last buffer
    if ((AcqSignal==Stop) || (AcqSignal==Pause)) XAcqDone=TRUE;
    }
}
else
{
while (TRUE)
    {
    for (I=0;I<20;++I)                                               //Get controller buffer status repeating for validity
        {
        XStatus=camac_status(DrvFd);
        if (!((XStatus&8)>>3)) break;
        usleep(500);
        }
    if (!(XStatus&3) && (AcqSignal==Resume)) usleep(PollTime);
    XData=0x0E000000; camac_write(DrvFd,&XData,4);  //Flush
    XByts=camac_read(DrvFd,&CBuf[XWdsLeft],4*CMC_BUF);
    if (XByts>0)
       {
       ++BuffersAcquired;
       if (BuffersAcquired%2) AcqBuf=AcqBuf1; else AcqBuf=AcqBuf2;                         //Acquire alternately on 2 buffers
       for (I=0;I<XWdsLeft;++I) CBuf[I]=OldCBuf[I];                       //Place the unprocessed part of the previous buffer
       for (i=0;i<Setup.Scaler.NSc1;++i) ScalerPrev[i]=ScalerCurr[i];
       DecodeSparseData(CBuf,XWdsLeft+XByts/4,AcqBuf);
       for (I=0;I<XWdsLeft;++I) OldCBuf[I]=CBuf[XPtr+I];                   //Hold the unprocessed part of the previous buffer
       XTotEvts+=XEvents; ++NewBufs;
       if (BuffersAcquired>1) pthread_cancel(Spec);              //Request cancellation of the Spec thread (if it is running)
       if (Setup.ListMode.ListOn) if (!CompressToDisk(ListFp,AcqBuf)) break;
       if (BuffersAcquired>1) pthread_join(Spec,NULL);    //Make sure that the Spec is really killed before starting it again
       pthread_create(&Spec,NULL,BuildSpectra,(void *)AcqBuf);
       if (RefreshRate && (NewBufs>=RefreshRate)) 
          { UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1); NewBufs=0; Safety(RunName,&ElapsedTime,&SpecSaved); }
       else if (BuffersAcquired==1) UpdateDisplay(&ElapsedTime,FALSE,2*XEvents*NPar1);
       if ((Setup.PLogSetup.On) && !(BuffersAcquired % Setup.PLogSetup.BufCount)) 
          PeriodicLog(1,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);
       if (Setup.Macro.N && !(BuffersAcquired % Setup.Macro.RefreshRate)) ComputeMacros(BuffersAcquired,MacroFp);
       }
    if (XAcqDone) break;                              //The loop will be executed once more so as to flush the last buffer
    if ((AcqSignal==Stop) || (AcqSignal==Pause)) XAcqDone=TRUE;
    }
}
camac_close(DrvFd); HardResetCmc100();
}

if (AcqSignal==Stop) UpdateDisplay(&ElapsedTime,TRUE,2*XEvents*NPar1);
else  UpdateDisplay(&ElapsedTime,FALSE,XByts);
if (Setup.ListMode.ListOn) 
   {
   if (Setup.ListMode.Compr==0)                                                                      //Freedom file format
      {
      CollectedEvents=XByts/2/Setup.Parameter.NPar;
      if (AcqSignal==Stop)                                                                  //Stop for Freedom format file
         {
         sprintf(NSC_txt,"STOP: %s ends. Collected %ld events",RunName,CollectedEvents); 
         queue_text(stop,NSC_txt,ListFp); writeEndOfFile(ListFp);
         }
      else                                                                                 //Pause for Freedom format file
         { 
         sprintf(NSC_txt,"PAUSED: %s paused. So far %ld events",RunName,CollectedEvents); 
         queue_text(pauseNSC,NSC_txt,ListFp); 
         }
      }
   fclose(ListFp);
   }
for (i=0;i<Setup.Scaler.NSc;++i) ScalerPending[i]=ScalerCurr[i];                   //Useful only if acquisition was paused
if (AcqSignal==Stop)
   {
   PeriodicLog(2,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);     //periodic.log final entries
   if (Setup.Macro.N) 
      { ComputeMacros(BuffersAcquired,MacroFp); if (Setup.Macro.N) fclose(MacroFp); }              //finish macro log file
   for (i=0;i<Setup.Oned.N;i++) 
       { sprintf(FNam,"%s/Spec_%s/%s%03d.z1d",SpecDir,RunName,RunName,i+1); Write1d(FNam,i,1); }
   if (Setup.Twod.WSz==1) 
      for (i=0;i<Setup.Twod.N;++i) 
          { sprintf(FNam,"%s/Spec_%s/%s%03d.z2d",SpecDir,RunName,RunName,i+1); Write2d16(FNam,i,1); }
   else 
      for (i=0;i<Setup.Twod.N;++i) 
          { sprintf(FNam,"%s/Spec_%s/%s%03d.z2d",SpecDir,RunName,RunName,i+1); Write2d32(FNam,i,1); }
   }
else PeriodicLog(3,RunName,BuffersAcquired,BuffersProcessed,BytesWritten,TStart,StartTime);

g_free(AcqBuf1); g_free(AcqBuf2); g_free(CBuf);
ProgramState=Free; pthread_exit(NULL);
return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OverwriteStart(GtkWidget *W,GtkWidget *Win)
{ 
gtk_widget_destroy(Win);
ProgramState=AcqOn; 
if (!Setup.Spectra.NoZero) { ZeroOned(-1); ZeroTwod(-1); } 
fstart_(RunName,strlen(RunName));                                                       //Call user initialisation routine
PauseDuration=0L;
strcpy(PrevRun,RunName);
ExecuteIniCommands();
if (!Setup.Hardware.CamacMode) ProgramLP(); else ProgramLP_QStop();
pthread_create(&Acq,NULL,AcquireData,NULL);
}
//----------------------------------------------------------------------------------------------------------------------
void StartCallBack(GtkWidget *W,GtkWidget *StartWin)
{
gint Overwrite,i;
gchar ListFName[MAX_FNAME_LENGTH],Str[MAX_FNAME_LENGTH+20],DName[MAX_FNAME_LENGTH];
GtkWidget *Win,*Label,*But;

gtk_widget_destroy(GTK_WIDGET(StartWin));
Overwrite=0;

if (!Setup.Simulator)
   {
   if ( (DrvFd=camac_open("/dev/cmcamac0")) < 0)                                                           //Assume CrateID=0
      { 
      Attention(0,"The driver is not loaded!\nClick Acquisition->LOAD DRIVER");
      ProgramState=Free; return;
      }
   }

sprintf(DName,"%s/Spec_%s",SpecDir,RunName);
for (i=0;i<Setup.Oned.N;i++) sprintf(Setup.Files.Oned[i],"%s/%s%03d.z1d",DName,RunName,i+1);
for (i=0;i<Setup.Twod.N;i++) sprintf(Setup.Files.Twod[i],"%s/%s%03d.z2d",DName,RunName,i+1);
if (Setup.ListMode.ListOn)
   {
   switch (Setup.ListMode.Compr)
      {
      case 0: sprintf(ListFName,"%s/%s.001",ListFDir,RunName); break;                   //NSC Candle/Freedom file format
      case 1: sprintf(ListFName,"%s/%s.zls",ListFDir,RunName); break;    //Normal: The usual zls zero suppression scheme
      case 2: sprintf(ListFName,"%s/%s.gls",ListFDir,RunName);                  //Advanced: The group suppression scheme
      }
   if (!access(ListFName,F_OK)) Overwrite=1;
   }
else
   if (!access(DName,F_OK)) Overwrite=2;                                                      //Check existence of DName
mkdir(DName,0755);                                        //Create directory (if it already exists, nothing will happen)
if (Overwrite)
   {
   Win=gtk_dialog_new(); gtk_grab_add(Win);
   gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
   gtk_window_set_title(GTK_WINDOW(Win),"Overwrite?"); gtk_container_border_width(GTK_CONTAINER(Win),5);
   gtk_widget_set_uposition(GTK_WIDGET(Win),400,300);
   if (Overwrite==1) sprintf(Str,"Overwrite %s?",ListFName);
   else              sprintf(Str,"Prev. data exists. Overwrite Spec. Files?");
   Label=gtk_label_new(Str); gtk_misc_set_padding(GTK_MISC(Label),10,10);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,TRUE,TRUE,0);
   But=gtk_button_new_from_stock(GTK_STOCK_YES);
   gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OverwriteStart),Win);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,TRUE,0);
   But=gtk_button_new_from_stock(GTK_STOCK_CANCEL);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,TRUE,0);
   gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
   gtk_widget_show_all(Win);
   return;
   }
ProgramState=AcqOn; 
if (!Setup.Spectra.NoZero) { ZeroOned(-1); ZeroTwod(-1); }
fstart_(RunName,strlen(RunName));                                                     //Call user initialisation routine
PauseDuration=0L;
strcpy(PrevRun,RunName);
ExecuteIniCommands();
if (!Setup.Hardware.CamacMode) ProgramLP(); else ProgramLP_QStop();
pthread_create(&Acq,NULL,AcquireData,NULL);
}
//----------------------------------------------------------------------------------------------------------------------
void CancelCallBack(GtkWidget *W,GtkWidget *StartWin)
{ gtk_widget_destroy(GTK_WIDGET(StartWin)); }
//----------------------------------------------------------------------------------------------------------------------
void StartWinDestroyed(GtkWidget *W,gpointer Unused)
{ ProgramState=Free; }
//----------------------------------------------------------------------------------------------------------------------
void RunNameCallBack(GtkWidget *W,gpointer Unused)
{ strcpy(RunName,gtk_entry_get_text(GTK_ENTRY(W))); }
//----------------------------------------------------------------------------------------------------------------------
void IncrementRunName()
{
gint i,RunNo,L,S;
gchar Str[40];

strcpy(RunName,PrevRun);
if (!strcmp(PrevRun,"Dummy")) return;
L=strlen(PrevRun);
for (i=L-1;i>0;i--) if (!isdigit(PrevRun[i])) break;
if ( (i==L-1) && (L<36) ) { RunName[L]='1'; RunName[L+1]='\0'; return; }
RunNo=atoi(&PrevRun[i+1])+1; sprintf(Str,"%d",RunNo); S=strlen(Str);
RunName[L-S]='\0'; strcat(RunName,Str);
}
//----------------------------------------------------------------------------------------------------------------------
void AutoStopCallBack(GtkWidget *W,gpointer Unused)
{ Setup.Hardware.AutoStop=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
//----------------------------------------------------------------------------------------------------------------------
void AutoStopToggle(GtkWidget *CheckBut,GtkWidget *Entry)
{
if (GTK_TOGGLE_BUTTON(CheckBut)->active) { Setup.Hardware.AutoStopOn=TRUE;  gtk_widget_set_sensitive(Entry,TRUE); }
                                    else { Setup.Hardware.AutoStopOn=FALSE; gtk_widget_set_sensitive(Entry,FALSE); }
}
//----------------------------------------------------------------------------------------------------------------------
void StartAcquisition(void)
{
gint i;
gchar Str[MAX_FNAME_LENGTH+20];
GtkWidget *Win,*VBox,*HBox,*Label,*But,*Entry,*CheckBut;
static GdkColor White = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor Black = {0,0x0000,0x0000,0x0000};
static GdkColor Gold  = {0,0xBBBB,0x9999,0x0000};
static GdkColor Blue  = {0,0x0000,0x0000,0xFFFF};
GtkStyle *RunStyle,*BlueStyle;

CreateDir();                                       //Create directories as per user preferences if they dont exist already
RunStyle=gtk_style_copy(gtk_widget_get_default_style());                              //Copy default style to this style
RunStyle->text[0]=White  /*Normal Text*/; RunStyle->text[3]=White /*Selected text*/;
RunStyle->fg[5]  =Black /*Box boundary*/; RunStyle->text[5]=Gold       /*Box fill*/;
BlueStyle=gtk_style_copy(gtk_widget_get_default_style());                               //Copy default style to this style
for (i=0;i<5;i++) { BlueStyle->fg[i]=BlueStyle->text[i]=Blue; BlueStyle->bg[i]=White; }       //Set colours for all states

ProgramState=AcqOn; IncrementRunName();
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                    //Define a new modal window
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(StartWinDestroyed),NULL);
gtk_window_set_title(GTK_WINDOW(Win),"Start Acquisition"); gtk_widget_set_uposition(GTK_WIDGET(Win),300,300);
gtk_widget_set_usize(GTK_WIDGET(Win),245,180); gtk_container_set_border_width(GTK_CONTAINER(Win),10);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                       //VBox for the entire window

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Prev. Run:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
sprintf(Str,"%s",PrevRun); Label=gtk_label_new(Str); SetStyleRecursively(Label,BlueStyle);
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Run Name:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
Entry=gtk_entry_new_with_max_length(35); gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,0);
SetStyleRecursively(Entry,RunStyle);
gtk_entry_set_text(GTK_ENTRY(Entry),RunName);
gtk_widget_set_usize(GTK_WIDGET(Entry),130,25);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(RunNameCallBack),NULL);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Setup File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
AbbreviateFileName(Str,Setup.FName,MAX_FNAME_LENGTH);
if (Setup.Modified) strcat(Str,"(?)");
if (!strlen(Setup.FName)) sprintf(Str,"%s","none");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
CheckBut=gtk_check_button_new_with_label("Auto Stop");
gtk_box_pack_start(GTK_BOX(HBox),CheckBut,FALSE,FALSE,0);
GTK_TOGGLE_BUTTON(CheckBut)->active=Setup.Hardware.AutoStopOn;
Entry=gtk_entry_new_with_max_length(10); gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,0);
sprintf(Str,"%d",Setup.Hardware.AutoStop); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_widget_set_usize(GTK_WIDGET(Entry),50,25);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(AutoStopCallBack),NULL);
Label=gtk_label_new("sec"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
if (Setup.Hardware.AutoStopOn) gtk_widget_set_sensitive(Entry,TRUE); else gtk_widget_set_sensitive(Entry,FALSE);
gtk_signal_connect(GTK_OBJECT(CheckBut),"toggled",GTK_SIGNAL_FUNC(AutoStopToggle),Entry);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_from_stock(GTK_STOCK_YES);
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(StartCallBack),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL);
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CancelCallBack),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
gtk_style_unref(RunStyle); gtk_style_unref(BlueStyle);
}
//----------------------------------------------------------------------------------------------------------------------
void StartNoZero(GtkWidget *W,gpointer Data)
{
if ( (ProgramState==AnalysisOn) || (ProgramState==ReWriteOn) ) 
   { Attention(0,"Sorry...Another task is in progress"); return; }
if (BatchRunning) { Attention(0,"Not permitted... Batch acquisition in progress"); return; }
if (AcqSignal != Stop) { Attention(0,"Acquisition already in progress"); return; }
Setup.Spectra.NoZero=TRUE;
StartAcquisition();
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StartNormal(GtkWidget *W,gpointer Data)
{
if ( (ProgramState==AnalysisOn) || (ProgramState==ReWriteOn) ) 
   { Attention(0,"Sorry...Another task is in progress"); return; }
if (BatchRunning) { Attention(0,"Not permitted... Batch acquisition in progress"); return; }
if (BatchRunning) { Attention(0,"Not permitted during batch acquisition"); return; }
if (AcqSignal != Stop) { Attention(0,"Acquisition already in progress"); return; }
Setup.Spectra.NoZero=FALSE;
StartAcquisition();
}
/*----------------------------------------------------------------------------------------------------------------------*/
void WriteSequence(gint Fd,short *Cmd,gshort *Arg)
{ 
gint i;

for (i=0;;i++)
    {
    camac_write(Fd,&Cmd[i],sizeof(short));
    if (Cmd[i]==NAF || Cmd[i]==DATA || Cmd[i]==DLY) camac_write(Fd,&Arg[i],sizeof(short));
    else if (Cmd[i]==ENDI) break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PauseAcquisition(GtkWidget *W,gpointer Data)
// 7 April 2005: Send STOP_ACQN to transputers instead of PAUSE_ACQN to preserve synchronization for 2 crates
{
gshort Code;
struct timeval Tv;

if (BatchRunning) { Attention(0,"Not permitted during batch acquisition"); return; }
if (AcqSignal != Resume) { Attention(0,"Acquisition is not running"); return; }
AcqSignal=Pause; 
ioctl(DrvFd,IOC_VTRANSPD_WRITE1,NULL); Code=STOP_ACQN; camac_write(DrvFd,&Code,sizeof(short));    //Send stop code to Crate 1
if (Setup.Hardware.NCrates==2)
   {
   ioctl(DrvFd,IOC_VTRANSPD_WRITE2,NULL); Code=STOP_ACQN; camac_write(DrvFd,&Code,sizeof(short)); //Send stop code to Crate 2
   }
gtk_label_set_text(GTK_LABEL(S_Stat[0]),"Paused");
gettimeofday(&Tv,NULL); PauseTime=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06; 
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ResumeAcquisition(GtkWidget *W,gpointer Data)
{
struct timeval Tv;
gdouble TimeNow;

if ( (AcqSignal != Pause) || (ProgramState==AnalysisOn) || (ProgramState==ReWriteOn) || BatchRunning)
   { Attention(0,"Sorry...Resume not allowed at this point"); return; }
gtk_label_set_text(GTK_LABEL(S_Stat[0]),"Acq. On"); 
ProgramState=AcqOn; Setup.Spectra.NoZero=TRUE;

ProgramState=AcqOn;
gettimeofday(&Tv,NULL); TimeNow=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06; 
PauseDuration+=TimeNow-PauseTime;
strcpy(PrevRun,RunName);
ExecuteIniCommands();
if (!Setup.Hardware.CamacMode) ProgramLP(); else ProgramLP_QStop();
pthread_create(&Acq,NULL,AcquireData,NULL);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StopNicely(void)
{
gint XRes,QRes,Lam; 

AcqSignal=Stop; 
(void)CamacNAF(0,30,10,24,&XRes,&QRes,&Lam); //Disable all LAMS
fstop_();                                                    //Execute user written code everytime acquisition is stopped
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StopAcquisition(GtkWidget *W,gpointer Data)
{
gint XRes,QRes,Lam; 

if (BatchRunning) { Attention(0,"Not permitted... Batch acquisition running"); return; }
if (AcqSignal == Stop) { Attention(0,"Acquisition is not running"); return; }
if (AcqSignal == Pause) { Attention(0,"Please resume acquisition first"); return; }
AcqSignal=Stop; 
if (!Setup.Simulator) (void)CamacNAF(0,30,10,24,&XRes,&QRes,&Lam); //Disable all LAMs
fstop_();                                                    //Execute user written code everytime acquisition is stopped
}
/*----------------------------------------------------------------------------------------------------------------------*/
void RemoveInitialBlanks(gchar *Str)
{
gint i;
gchar Str2[100];

for (i=0;i<strlen(Str);i++) if (Str[i] != ' ') break;
strcpy(Str2,&Str[i]); strcpy(Str,Str2);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void GetNaf(gchar *Str,gint *N,gint *A,gint *F)
{
gchar TStr[100];

Str=index(Str,(gint)':'); strcpy(TStr,&Str[1]); RemoveInitialBlanks(TStr); *N=atoi(TStr); 
Str=index(TStr,(gint)' '); RemoveInitialBlanks(Str); *A=atoi(Str);
Str=index(Str,(gint)' '); RemoveInitialBlanks(Str); *F=atoi(Str);
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint GetVal(gchar *Str)
{
Str=index(Str,(gint)'='); return (gushort)atoi(&Str[1]);
}
//----------------------------------------------------------------------------------------------------------------------
gboolean CheckPhillipsLam(gint *N)      //Return true if LAM is on a Phillips module. Set *N=Station number of this unit
{
gint i;

for (i=0;i<MAX_CAMAC_STNS;++i) 
    if ((Setup.Hardware.Modules[i] == 5) && (Setup.Hardware.Properties[i].AdcLam == Enabled))
       { *N=i+1; return TRUE; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ProgramLP(void)                                                                     //At present its for 1 crate only
{
gint XData,N,A,F,i,NPar1,NPar2,ProgramLocation,Unused;
gchar Str[100],Msg[MAX_FNAME_LENGTH+40],ClrName[MAX_FNAME_LENGTH];
FILE *Fp;

for (i=0,NPar1=NPar2=0;i<Setup.Parameter.NPar;++i) { if (Setup.Parameter.N[i]>MAX_CAMAC_STNS) ++NPar2; else ++NPar1; }

if (Setup.Simulator) { g_print("No ProgramLP in Simulator mode\n"); return; }

XData=0xFFFFFFFF;   camac_write(DrvFd,&XData,4);                      //1st header word
XData=0x00000000;   camac_write(DrvFd,&XData,4);                      //2nd header word

ProgramLocation=1;
//If LAM is on a Phillips unit, insert what is supposed to be 8 us delay
if (CheckPhillipsLam(&Unused))
   {
   XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
   XData=(5<<24)| 3; camac_write(DrvFd,&XData,4);                           //Delay. Value=10 corresponds to 10*800ns=8us, but we find Value=3 works
   }

//Insert NAF commands into the LP Program
for (i=0;i<NPar1;++i)
    {
    XData=(3<<24) | ProgramLocation++;  camac_write(DrvFd,&XData,4);                      //Store next word at ProgramLocation and increment
    N=Setup.Parameter.N[i]; A=Setup.Parameter.A[i]; F=Setup.Parameter.F[i]; 
    XData=A|F<<4|N<<9;  camac_write(DrvFd,&XData,4);                          //NAF command
    //g_print("i+1=%d NAF=%d %d %d\n",i+1,N,A,F);
    }

//Insert scaler parameters into the LP Program
for (i=0;i<Setup.Scaler.NSc1;++i)
    {
    XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);
    N=Setup.Scaler.N[i]; A=Setup.Scaler.A[i]; F=Setup.Scaler.F[i]; XData=A|F<<4|N<<9; camac_write(DrvFd,&XData,4);
    //g_print("Scaler No=%d NAF=%d %d %d\n",NPar1+i+1,N,A,F); 
    }

//Insert clr file commands into the LP Program
sprintf(ClrName,"%s/memory1.clr",SetupDir);
if ( (Fp=fopen(ClrName,"r"))==NULL) { sprintf(Msg,"Missing file %s",ClrName); Attention(0,Msg); return; }
XClrPar=0;  //Commands in CLR file also produce a response in the buffer. The number of these is XClrPar. 
while (TRUE)
   {
   if (fgets(Str,80,Fp)==NULL) break;
   RemoveInitialBlanks(Str);
   //Issue instruction to store next word at program location, then the instruction and increment
   if (!strncmp(Str,"END", 3) ) break;
   if (!strncmp(Str,"SETZ",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=28; A=8; F=26; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); ++XClrPar; }
   if (!strncmp(Str,"SETI",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=30; A=9; F=26; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); ++XClrPar; }
   if (!strncmp(Str,"CLRI",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=30; A=9; F=24; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); ++XClrPar; }
   if (!strncmp(Str,"CLRC",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=28; A=9; F=26; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); ++XClrPar; }
   if (!strncmp(Str,"NAF", 3) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); GetNaf(Str,&N,&A,&F); XData=A|F<<4|N<<9; camac_write(DrvFd,&XData,4); ++XClrPar; }
   if (!strncmp(Str,"DLY", 3) ) { Attention(0,"DLY Command in CLR file not permitted!"); }
   if (!strncmp(Str,"DATA",4) ) { Attention(0,"DATA Command in CLR file not permitted"); }
   }
fclose(Fp);

XExtraPar=XClrPar+Setup.Scaler.NSc1; /*g_print("XExtraPar=%d\n",XExtraPar)*/;
//Last line of LP program is always Code 31: Quit program mode, go to idle
XData=(3<<24) | ProgramLocation; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation
XData=(31<<24); camac_write(DrvFd,&XData,4);                           //Code 31: Quit program mode, go to idle
usleep(30000);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ProgramLP_QStop(void)                                                               //At present its for 1 crate only
//At present QStop is implemented for Phillips units only. Other units are read as usual
{
gint Stn,XData,N,A,F,i,NPar1,NPar2,ProgramLocation,Unused;
gchar Str[100],Msg[MAX_FNAME_LENGTH+40],ClrName[MAX_FNAME_LENGTH];
FILE *Fp;

for (i=0,NPar1=NPar2=0;i<Setup.Parameter.NPar;++i) { if (Setup.Parameter.N[i]>MAX_CAMAC_STNS) ++NPar2; else ++NPar1; }

if (Setup.Simulator) { g_print("No ProgramLP_QStop in Simulator mode\n"); return; }

XData=0xFFFFFFFF;   camac_write(DrvFd,&XData,4);                      //1st header word
XData=0x00000000;   camac_write(DrvFd,&XData,4);                      //2nd header word

ProgramLocation=1;

//If LAM is on a Phillips unit, insert what is supposed to be 8 us delay
if (CheckPhillipsLam(&Unused))
   {
   XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
   XData=(5<<24)| 3; camac_write(DrvFd,&XData,4);                           //Delay. Value=10 corresponds to 10*800ns=8us, but we find Value=3 works
   }

//Insert NAF commands into the LP Program, proceeding by Station Number
//For Phillips 71xx units, insert command for Q-Stop block read
for (Stn=0;Stn<MAX_CAMAC_STNS;++Stn) //At present its only for 1 crate
    {
    switch (Setup.Hardware.Modules[Stn])
       {
       case 5: //Phillips 71xx -- QStop block mode
               XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
               XData=(12<<24)| PHILLIPS_START; camac_write(DrvFd,&XData,4);                         //Write 24-bit literal to output stream
               XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
               N=Stn+1; A=0; F=4; XData=A|F<<4|N<<9; camac_write(DrvFd,&XData,4);                      //First NAF command for sparse read
               XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
               XData=(2<<24) | (1<<23) | 15; camac_write(DrvFd,&XData,4);                         //Repeat upto 15 times, stopping if no Q
               XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
               XData=(12<<24)| PHILLIPS_END; camac_write(DrvFd,&XData,4);                           //Write 24-bit literal to output stream
               break;
       case 1: case 2: case 3: case 4:
       case 6: case 7: case 8: case 9: case 10: case 11: case 12:                                 //Other units -- normal camac mode 
               XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
               XData=(12<<24)| SEQUENTIAL_START; camac_write(DrvFd,&XData,4);                       //Write 24-bit literal to output stream
               for (i=Setup.Hardware.LoParStn[Stn];i<(Setup.Hardware.LoParStn[Stn]+Setup.Hardware.NParStn[Stn]);++i)
                   {
                   XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);      //Store next word at ProgramLocation and increment
                   N=Setup.Parameter.N[i]; A=Setup.Parameter.A[i]; F=Setup.Parameter.F[i]; 
                   XData=A|F<<4|N<<9;  camac_write(DrvFd,&XData,4);                                                          //NAF command
                   }
               XData=(3<<24) | ProgramLocation++;  camac_write(DrvFd,&XData,4);         //Store next word at ProgramLocation and increment
               XData=(12<<24)| SEQUENTIAL_END; camac_write(DrvFd,&XData,4);                         //Write 24-bit literal to output stream
               break;
       }
    }

//Insert scaler parameters into the LP Program
if (Setup.Scaler.NSc1)
   {
   XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
   XData=(12<<24)| SCALERS_START; camac_write(DrvFd,&XData,4);                          //Write 24-bit literal to output stream
   }
for (i=0;i<Setup.Scaler.NSc1;++i)
    {
    XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);         //Store next word at ProgramLocation and increment
    N=Setup.Scaler.N[i]; A=Setup.Scaler.A[i]; F=Setup.Scaler.F[i]; XData=A|F<<4|N<<9; camac_write(DrvFd,&XData,4);
    //g_print("Scaler No=%d NAF=%d %d %d\n",NPar1+i+1,N,A,F); 
    }
if (Setup.Scaler.NSc1)
   {
   XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
   XData=(12<<24)| SCALERS_END; camac_write(DrvFd,&XData,4);                            //Write 24-bit literal to output stream
   }

//Insert clr file commands into the LP Program
XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
XData=(12<<24)| CLEAR_START; camac_write(DrvFd,&XData,4);                            //Write 24-bit literal to output stream
sprintf(ClrName,"%s/memory1.clr",SetupDir);
if ( (Fp=fopen(ClrName,"r"))==NULL) { sprintf(Msg,"Missing file %s",ClrName); Attention(0,Msg); return; }
while (TRUE)
   {
   if (fgets(Str,80,Fp)==NULL) break;
   RemoveInitialBlanks(Str);
   //Issue instruction to store next word at program location, then the instruction and increment
   if (!strncmp(Str,"END", 3) ) break;
   if (!strncmp(Str,"SETZ",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=28; A=8; F=26; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); }
   if (!strncmp(Str,"SETI",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=30; A=9; F=26; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); }
   if (!strncmp(Str,"CLRI",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=30; A=9; F=24; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); }
   if (!strncmp(Str,"CLRC",4) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); N=28; A=9; F=26; XData=A|F<<4|N<<9;      camac_write(DrvFd,&XData,4); }
   if (!strncmp(Str,"NAF", 3) ) { XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4); GetNaf(Str,&N,&A,&F); XData=A|F<<4|N<<9; camac_write(DrvFd,&XData,4); }
   if (!strncmp(Str,"DLY", 3) ) { Attention(0,"DLY Command in CLR file not permitted!"); }
   if (!strncmp(Str,"DATA",4) ) { Attention(0,"DATA Command in CLR file not permitted"); }
   }
fclose(Fp);
XData=(3<<24) | ProgramLocation++; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation and increment
XData=(12<<24)| CLEAR_END; camac_write(DrvFd,&XData,4);                              //Write 24-bit literal to output stream
//Last line of LP program is always Code 31: Quit program mode, go to idle
XData=(3<<24) | ProgramLocation; camac_write(DrvFd,&XData,4);          //Store next word at ProgramLocation
XData=(31<<24); camac_write(DrvFd,&XData,4);                           //Code 31: Quit program mode, go to idle
usleep(30000);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ExecuteIniCommands(void)
{
FILE *Fp;
gchar Str[100],Msg[MAX_FNAME_LENGTH+40],IniName[MAX_FNAME_LENGTH];
gint XData,N,A,F,XRes,QRes,Lam,Delay;

guint32 *Discard;
gint D,Byts;

if (Setup.Simulator) { g_print("Simulator....no IniCommands executed!"); return; }

sprintf(IniName,"%s/memory1.ini",SetupDir);
if ( (Fp=fopen(IniName,"r"))==NULL) { sprintf(Msg,"Missing file %s",IniName); Attention(0,Msg); return; }
XData=0;
while (1)
   {
   if (fgets(Str,80,Fp)==NULL) break;
   RemoveInitialBlanks(Str);
   if (!strncmp(Str,"END", 3) ) break;
   if (!strncmp(Str,"SETZ",4) ) CamacZ();
   if (!strncmp(Str,"SETI",4) ) SetI();
   if (!strncmp(Str,"CLRI",4) ) ClrI();
   if (!strncmp(Str,"CLRC",4) ) CamacC();
   if (!strncmp(Str,"NAF", 3) ) { GetNaf(Str,&N,&A,&F); (void)CamacNAF(XData,N,A,F,&XRes,&QRes,&Lam); }
   if (!strncmp(Str,"DLY", 3) ) { Delay=GetVal(Str); usleep(1000*Delay); }  //Delay in millisecs
   if (!strncmp(Str,"DATA",4) ) XData=GetVal(Str);
   }
fclose(Fp);
usleep(30000);
//Lines added 19 April 2010 in the hope of flushing data
Discard=g_new(guint32,CMC_BUF);
D=0x0E000000; camac_write(DrvFd,&D,4);  //FIFO flush
Byts=camac_read(DrvFd,Discard,CMC_BUF); //Driver flush
if (Byts>0) g_print("Obtained %d bytes while flushing!\n",Byts);
g_free(Discard);
}
/*----------------------------------------------------------------------------------------------------------------------*
 * The rest of this file is batch acquistion. We did not make it a separate file because of global variables            *
/*----------------------------------------------------------------------------------------------------------------------*/
void CloseBatchAcq(GtkWidget *W,gpointer Data)
{
ProgramState=Free;
g_free(Bat);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CancelBatAcq(GtkWidget *W,GtkWidget *BatWin)
{ gtk_widget_destroy(BatWin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void BatRowSelect(gint Row,gpointer Data)
{ Bat->Row=GPOINTER_TO_INT(Data); }
/*----------------------------------------------------------------------------------------------------------------------*/
void RunNameChanged(GtkWidget *W,gpointer Data)
{
Bat->Row=GPOINTER_TO_INT(Data);
strcpy(Setup.BatAcq.RunName[Bat->Row],gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectSetF(GtkWidget *W,gpointer Unused)
{
GList *Node;                        //Note: The GList has the table elements in reverse order! and g_list_nth doesnt work!
gchar Str[MAX_FNAME_LENGTH+5];
gint i,j;

if (Bat->Row >= 0)
   {
   Node=g_list_last(GTK_TABLE(Bat->Table)->children);                                            //First element of table
   for (i=0;i<Bat->Row;++i) for (j=0;j<5;++j) Node=g_list_previous(Node);                   //Skip nodes upto SelectedRow
   for (j=0;j<2;++j)  Node=g_list_previous(Node);                                                     //Skip 2 more nodes

   if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
      { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
   sprintf(Setup.BatAcq.SetFName[Bat->Row],"%s/%s",FileX->Path,FileX->TargetFile);
   g_free(FileX);
   AbbreviateFileName(Str,Setup.BatAcq.SetFName[Bat->Row],18);
   gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),Str);  //Change text inside button
   }
else
   {
   if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
      { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
   sprintf(Setup.BatAcq.SetFName[0],"%s/%s",FileX->Path,FileX->TargetFile);
   g_free(FileX);
   AbbreviateFileName(Str,Setup.BatAcq.SetFName[0],18);
   gtk_label_set_text(GTK_LABEL(Bat->Lab[6]),Str);                                                 //Change text in label
   gtk_widget_show(Bat->Lab[6]);
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BatSetFChanged(GtkWidget *W,gpointer Data)
{
Bat->Row=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select Setup Files",NULL,50,225,TRUE,".",".set",FALSE,&SelectSetF,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StartChanged(GtkWidget *W,gpointer Data)
{
Bat->Row=GPOINTER_TO_INT(Data);
strcpy(Setup.BatAcq.Start[Bat->Row],gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void QCriterion(GtkWidget *W,gpointer Data)
{
gint i,Row;

Row=GPOINTER_TO_INT(Data);
strcpy(Setup.BatAcq.Stop[Row],gtk_entry_get_text(GTK_ENTRY(W)));
for (i=0;i<3;++i) gtk_widget_set_sensitive(Bat->Crit[i],FALSE);
for (i=0;i<6;++i) gtk_widget_set_sensitive(Bat->Lab[i],FALSE);
switch (Setup.BatAcq.Stop[Row][0])
   {
   case 'D': gtk_widget_set_sensitive(Bat->Crit[0],TRUE); gtk_widget_set_sensitive(Bat->Lab[0],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[1],TRUE); break;
   case 'E': gtk_widget_set_sensitive(Bat->Crit[1],TRUE); gtk_widget_set_sensitive(Bat->Lab[2],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[3],TRUE); break;
   case 'L': gtk_widget_set_sensitive(Bat->Crit[2],TRUE); gtk_widget_set_sensitive(Bat->Lab[4],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[5],TRUE);
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StopAccept(GtkWidget *W,GtkWidget *But)
{
switch (Setup.BatAcq.Stop[Bat->Row][0])
   {
   case 'D': sprintf(Setup.BatAcq.Stop[Bat->Row],"Duration %s",gtk_entry_get_text(GTK_ENTRY(Bat->Crit[0])));
             break;
   case 'E': sprintf(Setup.BatAcq.Stop[Bat->Row],"Events %s",gtk_entry_get_text(GTK_ENTRY(Bat->Crit[1])));
             break;
   case 'L': sprintf(Setup.BatAcq.Stop[Bat->Row],"ListBytes %s",gtk_entry_get_text(GTK_ENTRY(Bat->Crit[2])));
   }
gtk_label_set_text(GTK_LABEL(GTK_BIN(But)->child),Setup.BatAcq.Stop[Bat->Row]);               //Change text inside button
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StopChanged(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*HBox,*VBox,*Lab,*Combo,*But;
GList *GList;
gchar StrD[MAX_TEXT_FIELD],StrE[MAX_TEXT_FIELD],StrL[MAX_TEXT_FIELD];
gint i;

Bat->Row=GPOINTER_TO_INT(Data);
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_title(GTK_WINDOW(Win),"Change Stop");
gtk_widget_set_uposition(GTK_WIDGET(Win),622,50);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(StopAccept),W);

VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),10);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Lab=gtk_label_new("Criterion"); gtk_box_pack_start(GTK_BOX(HBox),Lab,FALSE,FALSE,0);
Combo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Combo),90,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Combo,FALSE,FALSE,11);
GList=NULL; GList=g_list_append(GList,"Duration");
GList=g_list_append(GList,"Events"); GList=g_list_append(GList,"ListBytes");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
strcpy(StrD," 00:01:00"); strcpy(StrE," 10K"); strcpy(StrL," 10MB");
switch (Setup.BatAcq.Stop[Bat->Row][0])
   {
   case 'D': gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Duration");
             strcpy(StrD,index(Setup.BatAcq.Stop[Bat->Row],' '));
             break;
   case 'E': gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Events");
             strcpy(StrE,index(Setup.BatAcq.Stop[Bat->Row],' '));
             break;
   case 'L': gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"ListBytes");
             strcpy(StrL,index(Setup.BatAcq.Stop[Bat->Row],' '));
   }
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(QCriterion),GINT_TO_POINTER(Bat->Row));

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Bat->Lab[0]=gtk_label_new("Duration"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[0],FALSE,FALSE,0);
Bat->Crit[0]=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Bat->Crit[0]),&StrD[1]);
gtk_widget_set_size_request(GTK_WIDGET(Bat->Crit[0]),80,-1); gtk_box_pack_start(GTK_BOX(HBox),Bat->Crit[0],FALSE,FALSE,12);
Bat->Lab[1]=gtk_label_new("hh:mm:ss"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[1],FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Bat->Lab[2]=gtk_label_new("Events"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[2],FALSE,FALSE,0);
Bat->Crit[1]=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Bat->Crit[1]),&StrE[1]);
gtk_widget_set_size_request(GTK_WIDGET(Bat->Crit[1]),80,-1); gtk_box_pack_start(GTK_BOX(HBox),Bat->Crit[1],FALSE,FALSE,22);
Bat->Lab[3]=gtk_label_new("e.g 3K, 5.5M, 1.2G"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[3],FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Bat->Lab[4]=gtk_label_new("ListBytes"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[4],FALSE,FALSE,0);
Bat->Crit[2]=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Bat->Crit[2]),&StrL[1]);
gtk_widget_set_size_request(GTK_WIDGET(Bat->Crit[2]),80,-1); gtk_box_pack_start(GTK_BOX(HBox),Bat->Crit[2],FALSE,FALSE,6);
Bat->Lab[5]=gtk_label_new("e.g 10.3KB, 7.8MB, 0.6GB"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[5],FALSE,FALSE,5);

for (i=0;i<3;++i) gtk_widget_set_sensitive(Bat->Crit[i],FALSE);
for (i=0;i<6;++i) gtk_widget_set_sensitive(Bat->Lab[i],FALSE);
switch (Setup.BatAcq.Stop[Bat->Row][0])
   {
   case 'D': gtk_widget_set_sensitive(Bat->Crit[0],TRUE); gtk_widget_set_sensitive(Bat->Lab[0],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[1],TRUE); break;
   case 'E': gtk_widget_set_sensitive(Bat->Crit[1],TRUE); gtk_widget_set_sensitive(Bat->Lab[2],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[3],TRUE); break;
   case 'L': gtk_widget_set_sensitive(Bat->Crit[2],TRUE); gtk_widget_set_sensitive(Bat->Lab[4],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[5],TRUE);
   }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

g_list_free(GList);
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MakeBatRow(gint Row,GtkWidget *Table)
{
gint ColWidth[5]={28,90,90,80,190};
GtkWidget *But,*Entry;
gchar Str[MAX_FNAME_LENGTH+10];
static GdkColor StopBg  = {0,0x9999,0x0000,0x0000};
static GdkColor StopFg  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor SetupBg  = {0,0xFFFF,0xFFFF,0x0000};
GtkStyle *StopStyle,*SetupStyle;
gint i;

StopStyle=gtk_style_copy(gtk_widget_get_default_style()); 
for (i=0;i<5;i++) { StopStyle->bg[i]=StopBg; StopStyle->fg[i]=StopStyle->text[i]=StopFg; }
SetupStyle=gtk_style_copy(gtk_widget_get_default_style()); for (i=0;i<5;i++) SetupStyle->bg[i]=SetupBg;

sprintf(Str,"%02d",Row+1); But=gtk_button_new_with_label(Str);
gtk_widget_set_usize(GTK_WIDGET(But),ColWidth[0],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatRowSelect),GINT_TO_POINTER(Row));
gtk_table_attach(GTK_TABLE(Table),But,0,1,Row,Row+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.BatAcq.RunName[Row]);
gtk_widget_set_usize(GTK_WIDGET(Entry),ColWidth[1],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(RunNameChanged),GINT_TO_POINTER(Row));
gtk_table_attach(GTK_TABLE(Table),Entry,1,2,Row,Row+1,GTK_FILL,GTK_SHRINK,0,0);

AbbreviateFileName(Str,Setup.BatAcq.SetFName[Row],18);
But=gtk_button_new_with_label(Str); SetStyleRecursively(But,SetupStyle);
gtk_widget_set_usize(GTK_WIDGET(But),ColWidth[2],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatSetFChanged),GINT_TO_POINTER(Row));
gtk_table_attach(GTK_TABLE(Table),But,3,4,Row,Row+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.BatAcq.Start[Row]);
gtk_widget_set_usize(GTK_WIDGET(Entry),ColWidth[3],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(StartChanged),GINT_TO_POINTER(Row));
gtk_table_attach(GTK_TABLE(Table),Entry,5,6,Row,Row+1,GTK_FILL,GTK_SHRINK,0,0);

But=gtk_button_new_with_label(""); SetStyleRecursively(But,StopStyle);
gtk_widget_set_usize(GTK_WIDGET(But),ColWidth[4],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(StopChanged),GINT_TO_POINTER(Row));
gtk_table_attach(GTK_TABLE(Table),But,6,7,Row,Row+1,GTK_FILL,GTK_SHRINK,0,0);
gtk_label_set_text(GTK_LABEL(GTK_BIN(But)->child),Setup.BatAcq.Stop[Row]);

gtk_style_unref(StopStyle); gtk_style_unref(SetupStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void AcceptQuickBat(GtkWidget *W,gpointer Unused)
{
gint i,j;


if (GTK_IS_WIDGET(Bat->Table)) gtk_widget_destroy(Bat->Table);
Bat->Table=gtk_table_new(Setup.BatAcq.NBat,4,FALSE);
Bat->Row=0;
Setup.BatAcq.NBat=CLAMP(atoi(gtk_entry_get_text(GTK_ENTRY(Bat->QEntry[0]))),0,MAX_BAT);
for (i=0;i<Setup.BatAcq.NBat;++i) 
    {
    sprintf(Setup.BatAcq.RunName[i],"%s%03d",gtk_entry_get_text(GTK_ENTRY(Bat->QEntry[1])),i+1);
    strcpy(Setup.BatAcq.SetFName[i],Setup.BatAcq.SetFName[0]);
    strcpy(Setup.BatAcq.Start[i],gtk_entry_get_text(GTK_ENTRY(Bat->QEntry[2])));
    }
switch (Setup.BatAcq.Stop[0][0])
   {
   case 'D': for (i=0;i<Setup.BatAcq.NBat;++i) 
             sprintf(Setup.BatAcq.Stop[i],"Duration %s",gtk_entry_get_text(GTK_ENTRY(Bat->Crit[0])));
             break;
   case 'E': for (i=0;i<Setup.BatAcq.NBat;++i) 
             sprintf(Setup.BatAcq.Stop[i],"Events %s",gtk_entry_get_text(GTK_ENTRY(Bat->Crit[1])));
             break;
   case 'L': for (i=0;i<Setup.BatAcq.NBat;++i) 
             sprintf(Setup.BatAcq.Stop[i],"ListBytes %s",gtk_entry_get_text(GTK_ENTRY(Bat->Crit[2])));
   }
for (i=0;i<Setup.BatAcq.NBat;++i) MakeBatRow(i,Bat->Table);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Bat->ScrlW),Bat->Table);
gtk_widget_show_all(Bat->ScrlW);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void QuickBat(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*But,*HBox,*VBox,*Lab,*Combo,*Frame,*VBox1;
GList *GList;
gchar Str[LONG_TEXT_FIELD],StrD[MAX_TEXT_FIELD],StrE[MAX_TEXT_FIELD],StrL[MAX_TEXT_FIELD];
gint i;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_title(GTK_WINDOW(Win),"Quick Set");
gtk_widget_set_uposition(GTK_WIDGET(Win),622,50);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(AcceptQuickBat),NULL);

VBox=gtk_vbox_new(FALSE,12); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),5);

Lab=gtk_label_new("Quick Set provides a starting point"); gtk_box_pack_start(GTK_BOX(VBox),Lab,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Lab=gtk_label_new("No of Runs"); gtk_box_pack_start(GTK_BOX(HBox),Lab,FALSE,FALSE,10);
Bat->QEntry[0]=gtk_entry_new_with_max_length(3);
sprintf(Str,"%d",Setup.BatAcq.NBat); gtk_entry_set_text(GTK_ENTRY(Bat->QEntry[0]),Str);
gtk_widget_set_size_request(GTK_WIDGET(Bat->QEntry[0]),40,-1); 
gtk_box_pack_start(GTK_BOX(HBox),Bat->QEntry[0],FALSE,FALSE,10);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Lab=gtk_label_new("Generic Run Name"); gtk_box_pack_start(GTK_BOX(HBox),Lab,FALSE,FALSE,10);
Bat->QEntry[1]=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH-4);
gtk_entry_set_text(GTK_ENTRY(Bat->QEntry[1]),"run");
gtk_widget_set_usize(GTK_WIDGET(Bat->QEntry[1]),90,-1); 
gtk_box_pack_start(GTK_BOX(HBox),Bat->QEntry[1],FALSE,FALSE,10);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Setup File:");
i=-1;
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatSetFChanged),GINT_TO_POINTER(i));
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,10);
Bat->Lab[6]=gtk_label_new("Current"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[6],FALSE,FALSE,10);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Lab=gtk_label_new("Start Delay"); gtk_box_pack_start(GTK_BOX(HBox),Lab,FALSE,FALSE,10);
Bat->QEntry[2]=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Bat->QEntry[2]),Setup.BatAcq.Start[0]);
gtk_widget_set_size_request(GTK_WIDGET(Bat->QEntry[2]),80,-1); 
gtk_box_pack_start(GTK_BOX(HBox),Bat->QEntry[2],FALSE,FALSE,10);

Frame=gtk_frame_new("Stop After"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox1=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox1);
gtk_container_set_border_width(GTK_CONTAINER(VBox1),10);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);
Lab=gtk_label_new("Criterion"); gtk_box_pack_start(GTK_BOX(HBox),Lab,FALSE,FALSE,0);
Combo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Combo),90,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Combo,FALSE,FALSE,11);
GList=NULL; GList=g_list_append(GList,"Duration"); 
GList=g_list_append(GList,"Events"); GList=g_list_append(GList,"ListBytes");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
strcpy(StrD," 00:01:00"); strcpy(StrE," 10K"); strcpy(StrL," 10MB"); 

switch (Setup.BatAcq.Stop[0][0])
   {
   case 'D': gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Duration"); 
             strcpy(StrD,index(Setup.BatAcq.Stop[0],' '));
             break;
   case 'E': gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Events");
             strcpy(StrE,index(Setup.BatAcq.Stop[0],' '));
             break;
   case 'L': gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"ListBytes");
             strcpy(StrL,index(Setup.BatAcq.Stop[0],' '));
   }

i=0;
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(QCriterion),GINT_TO_POINTER(i));

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);
Bat->Lab[0]=gtk_label_new("Duration"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[0],FALSE,FALSE,0);
Bat->Crit[0]=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Bat->Crit[0]),&StrD[1]);
gtk_widget_set_size_request(GTK_WIDGET(Bat->Crit[0]),80,-1); gtk_box_pack_start(GTK_BOX(HBox),Bat->Crit[0],FALSE,FALSE,12);
Bat->Lab[1]=gtk_label_new("hh:mm:ss"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[1],FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);
Bat->Lab[2]=gtk_label_new("Events"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[2],FALSE,FALSE,0);
Bat->Crit[1]=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Bat->Crit[1]),&StrE[1]);
gtk_widget_set_size_request(GTK_WIDGET(Bat->Crit[1]),80,-1); gtk_box_pack_start(GTK_BOX(HBox),Bat->Crit[1],FALSE,FALSE,22);
Bat->Lab[3]=gtk_label_new("e.g 3K, 5.5M, 1.2G"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[3],FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox,FALSE,FALSE,0);
Bat->Lab[4]=gtk_label_new("ListBytes"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[4],FALSE,FALSE,0);
Bat->Crit[2]=gtk_entry_new_with_max_length(8);
gtk_entry_set_text(GTK_ENTRY(Bat->Crit[2]),&StrL[1]);
gtk_widget_set_size_request(GTK_WIDGET(Bat->Crit[2]),80,-1); gtk_box_pack_start(GTK_BOX(HBox),Bat->Crit[2],FALSE,FALSE,6);
Bat->Lab[5]=gtk_label_new("e.g 10.3KB, 7.8MB, 0.6GB"); gtk_box_pack_start(GTK_BOX(HBox),Bat->Lab[5],FALSE,FALSE,5);

for (i=0;i<3;++i) gtk_widget_set_sensitive(Bat->Crit[i],FALSE);
for (i=0;i<6;++i) gtk_widget_set_sensitive(Bat->Lab[i],FALSE);
switch (Setup.BatAcq.Stop[0][0])
   {
   case 'D': gtk_widget_set_sensitive(Bat->Crit[0],TRUE); gtk_widget_set_sensitive(Bat->Lab[0],TRUE); 
             gtk_widget_set_sensitive(Bat->Lab[1],TRUE); break;
   case 'E': gtk_widget_set_sensitive(Bat->Crit[1],TRUE); gtk_widget_set_sensitive(Bat->Lab[2],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[3],TRUE); break;
   case 'L': gtk_widget_set_sensitive(Bat->Crit[2],TRUE); gtk_widget_set_sensitive(Bat->Lab[4],TRUE);
             gtk_widget_set_sensitive(Bat->Lab[5],TRUE);
   }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Accept"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

g_list_free(GList);
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BatAdd(GtkWidget *W,gpointer Unused)
{
gint Row,End;

if (Setup.BatAcq.NBat == MAX_BAT) return;                                                                 //Limit reached
Row=Setup.BatAcq.NBat;
++Setup.BatAcq.NBat;
if (Row)
   {
   strcpy(Setup.BatAcq.RunName[Row],Setup.BatAcq.RunName[Row-1]);
   strcpy(Setup.BatAcq.SetFName[Row],Setup.BatAcq.SetFName[Row-1]);
   strcpy(Setup.BatAcq.Start[Row],Setup.BatAcq.Start[Row-1]);
   strcpy(Setup.BatAcq.Stop[Row],Setup.BatAcq.Stop[Row-1]);
   }
else
   {
   strcpy(Setup.BatAcq.RunName[Row],"run001");
   strcpy(Setup.BatAcq.SetFName[Row],"Current");
   strcpy(Setup.BatAcq.Start[Row],"00:00:00");
   strcpy(Setup.BatAcq.Stop[Row],"Duration 00:01:00");   
   }
MakeBatRow(Row,Bat->Table); 
End=16.0*Row; if (Row>8) End=40.0*Row;                                                     //Fudging to scroll to the end!
gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Bat->ScrlW))),End);
gtk_widget_show_all(Bat->Table);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BatClear(GtkWidget *W,gpointer Unused)
{
gint i;

if (!Setup.BatAcq.NBat) return;
Bat->Row=CLAMP(Bat->Row,0,Setup.BatAcq.NBat-1);
if (Setup.BatAcq.NBat>0) --Setup.BatAcq.NBat;
for (i=Bat->Row;i<Setup.BatAcq.NBat;++i) 
    {
    strcpy(Setup.BatAcq.RunName[i],Setup.BatAcq.RunName[i+1]);
    strcpy(Setup.BatAcq.SetFName[i],Setup.BatAcq.SetFName[i+1]);
    strcpy(Setup.BatAcq.Start[i],Setup.BatAcq.Start[i+1]);
    strcpy(Setup.BatAcq.Stop[i],Setup.BatAcq.Stop[i+1]);
    }
Bat->Row=MIN(Bat->Row,Setup.BatAcq.NBat-1);
if (GTK_IS_WIDGET(Bat->Table)) gtk_widget_destroy(Bat->Table);
Bat->Table=gtk_table_new(Setup.BatAcq.NBat,4,FALSE);
for (i=0;i<Setup.BatAcq.NBat;++i) MakeBatRow(i,Bat->Table);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Bat->ScrlW),Bat->Table);
gtk_widget_show_all(Bat->ScrlW);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BatClearAll(GtkWidget *W,gpointer Unused)
{
if (GTK_IS_WIDGET(Bat->Table)) gtk_widget_destroy(Bat->Table);
Bat->Row=0; Setup.BatAcq.NBat=0;
Bat->Table=gtk_table_new(Setup.BatAcq.NBat,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Bat->ScrlW),Bat->Table);
gtk_widget_show_all(Bat->ScrlW);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LoadBatAcqF(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH],Str1[50],Str2[50];
gint i,j;
FILE *Fp;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
g_free(FileX);

BatClearAll(NULL,NULL);
if ((Fp=fopen(FName,"r")) != NULL)
   {
   fscanf(Fp,"%d",&Setup.BatAcq.NBat);
   for (i=0;i<Setup.BatAcq.NBat;++i)
       {
       fscanf(Fp,"%d %s %s %s %s %s",&j,Setup.BatAcq.RunName[i],Setup.BatAcq.SetFName[i],
              Setup.BatAcq.Start[i],Str1,Str2);
       sprintf(Setup.BatAcq.Stop[i],"%s %s",Str1,Str2);
       }
   fclose(Fp);
   }
for (i=0;i<Setup.BatAcq.NBat;++i) MakeBatRow(i,Bat->Table);
gtk_widget_show_all(Bat->Table); Bat->Row=0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LoadBatAcq(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Load Batch File",NULL,50,225,FALSE,BatDir,".acq",FALSE,&LoadBatAcqF,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveBatAcqF(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];
gint i;
FILE *Fp;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
g_free(FileX);

if ((Fp=fopen(FName,"w")) != NULL)
   {
   fprintf(Fp,"%d\n",Setup.BatAcq.NBat);
   for (i=0;i<Setup.BatAcq.NBat;++i)
       fprintf(Fp,"%d %s %s %s %s\n",i+1,Setup.BatAcq.RunName[i],Setup.BatAcq.SetFName[i],
               Setup.BatAcq.Start[i],Setup.BatAcq.Stop[i]);
   fclose(Fp);
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveBatAcq(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Save Batch File",NULL,50,225,FALSE,BatDir,".acq",FALSE,&SaveBatAcqF,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void *ControlBatch(gpointer Data)                                                    //Thread to control batch acquisition
{
gint i,j,Delay,Duration;
gpointer T;
gchar Str[200],IniName[MAX_FNAME_LENGTH],ClrName[MAX_FNAME_LENGTH],ErrMessg[200],Tc,DName[MAX_FNAME_LENGTH];
struct stat StatBuf;
glong StBuffers,StBytes;
gfloat Tf;

if (BatchRunning) { SAttention("Batch process already running!"); pthread_exit(NULL); }
BatchRunning=TRUE; strcpy(PrevRun,"");
for (i=0;i<Setup.BatAcq.NBat;++i)
    {
    Setup.BatAcq.Current=i+1;
    if (BatStop) break;
    Delay=3600*atoi(Setup.BatAcq.Start[i]);
    T=index(Setup.BatAcq.Start[i],':'); Delay+=60*atoi(++T);
    T=rindex(Setup.BatAcq.Start[i],':'); Delay+=atoi(++T);
    for (j=0;j<10;++j) { if (ProgramState == Free) break; sleep(2); }             //Not a good way to synchronize threads!
    if (ProgramState != Free) { SAttention("Cannot get Free status\nAborting batch process"); break; }
    if (strcmp(Setup.BatAcq.SetFName[i],"Current"))
       {
       strcpy(Setup.FName,Setup.BatAcq.SetFName[i]);
       if (Read(ErrMessg)) { SAttention(ErrMessg); break; }                              //Read setup file--break on error
       /*CheckSetup will already have created memory1.ini, memory1.clr, memory2.ini and memory2.clr,
       Now, just in case the user might have made manual changes to the .ini and .clr files,
       we will overwrite these four files*/
       if (index(Setup.FName,'.') == NULL) { sprintf(Str,"ERROR\nNo dot in %s",Setup.FName); SAttention(Str); }
       strcpy(IniName,Setup.FName); strcpy(ClrName,Setup.FName);
       strcpy(index(IniName,'.'),"1.ini"); strcpy(index(ClrName,'.'),"1.clr");
       if (stat(IniName,&StatBuf) == 0) { sprintf(Str,"cp %s memory1.ini",IniName); system(Str); }           //File exists
       if (stat(ClrName,&StatBuf) == 0) { sprintf(Str,"cp %s memory1.clr",ClrName); system(Str); }           //File exists
       strcpy(IniName,Setup.FName); strcpy(ClrName,Setup.FName);
       strcpy(index(IniName,'.'),"2.ini"); strcpy(index(ClrName,'.'),"2.clr");
       if (stat(IniName,&StatBuf) == 0) { sprintf(Str,"cp %s memory2.ini",IniName); system(Str); }           //File exists
       if (stat(ClrName,&StatBuf) == 0) { sprintf(Str,"cp %s memory2.clr",ClrName); system(Str); }           //File exists
       }
    strcpy(PrevRun,RunName); strcpy(RunName,Setup.BatAcq.RunName[i]);

    switch (Setup.BatAcq.Stop[i][0])
           {
           case 'D': T=index(Setup.BatAcq.Stop[i],' '); Duration=3600*atoi(++T);                       //Stop by Duration
                     T=index(T,':'); Duration+=60*atoi(++T);
                     T=index(T,':'); Duration+=atoi(++T); break;
           case 'E': T=index(Setup.BatAcq.Stop[i],' '); Tf=(gfloat)atoi(++T);                           //Stop by Events
                     Tc=Setup.BatAcq.Stop[i][strlen(Setup.BatAcq.Stop[i])-1];
                     if (Tc=='K') Tf=1.0E3*Tf; else if (Tc=='M') Tf=1.0E6*Tf; else if (Tc=='G') Tf=1.0E9*Tf; 
                     StBuffers=(glong)(Tf*2.0*Setup.Parameter.NPar/1000/*Setup.ListMode.BufSiz*/);  //Temp commented, need a work around here
                     break;
           case 'L': T=rindex(Setup.BatAcq.Stop[i],' '); Tf=(gfloat)atoi(++T);                      //Stop by List bytes
                     Tc=Setup.BatAcq.Stop[i][strlen(Setup.BatAcq.Stop[i])-2];
                     if (Tc=='K') Tf=1.0E3*Tf; else if (Tc=='M') Tf=1.0E6*Tf; else if (Tc=='G') Tf=1.0E9*Tf;
                     StBytes=(glong)Tf;
                     if (!Setup.ListMode.ListOn) 
                        {
                        SAttention("Batch Error: Cant stop by List bytes\nList is off...Aborting batch");
                        BatchRunning=FALSE; pthread_exit(NULL);
                        }
           }
    if (AcqSignal != Stop) { SAttention("Acquisition already in progress"); BatchRunning=FALSE; pthread_exit(NULL); }
    sprintf(DName,"%s/Spec_%s",SpecDir,RunName);
    mkdir(DName,0755); //Create spec dir (no effect if it already exists)
    for (j=0;j<Setup.Oned.N;++j) sprintf(Setup.Files.Oned[j],"%s/%s%03d.z1d",DName,RunName,j+1);
    for (j=0;j<Setup.Twod.N;++j) sprintf(Setup.Files.Twod[j],"%s/%s%03d.z2d",DName,RunName,j+1);
    //We make no checks for spec and list file overwriting. Also, the spectra are always zeroed out.
   if ( (DrvFd=camac_open("/dev/cmcamac0")) < 0)                                                           //Assume CrateID=0
      {
      Attention(0,"BatAcq..The driver is not loaded!\nClick Acquisition->LOAD DRIVER");
      ProgramState=Free; return;
      }
    ProgramState=AcqOn; Setup.Spectra.NoZero=FALSE;
    ZeroOned(-1); ZeroTwod(-1);
    sleep(Delay);                                                                                    //Delay before start
    fstart_(RunName,strlen(RunName));                                                  //Call user initialisation routine
    ExecuteIniCommands();
    if (!Setup.Hardware.CamacMode) ProgramLP(); else ProgramLP_QStop();
    pthread_create(&Acq,NULL,AcquireData,NULL);                                                       //Start acquisition
    switch (Setup.BatAcq.Stop[i][0])
           {
           case 'D': sleep(Duration);                                                                  //Stop by duration  
                     StopNicely(); 
                     break;
           case 'E': while (TRUE)                                                                        //Stop by Events
                        {
                        sleep(10);                                                              //Check only every 10 sec
                        if (BuffersAcquired>=StBuffers) { StopNicely(); break; }
                        }
                     break;
           case 'L': while (TRUE)                                                                     //Stop by ListBytes
                        {
                        sleep(10);                                                              //Check only every 10 sec
                        if (BytesWritten>=StBytes) { StopNicely(); break; }
                        }
           }      
    while (ProgramState!=Free) usleep(100000); sleep(1);           //Wait for PrograState to be Free then wait 1 sec more
    }
strcpy(PrevRun,RunName); BatchRunning=FALSE; pthread_exit(NULL);
return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StartBatAcq(GtkWidget *W,GtkWidget *BatWin)
{
gtk_widget_destroy(BatWin);
pthread_create(&CntlBat,NULL,ControlBatch,NULL);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BatchAcquire(GtkWidget *W,gpointer Unused)
{
static gchar *Titles[5]= {"No","Run Name","Setup File","Start Delay","Stop After"};
gint ColWidth[5]={28,90,90,80,190};
static GdkColor TitlesBg  = {0,0x0000,0x0000,0x7777};
static GdkColor TitlesFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *TitlesStyle;
GtkWidget *Win,*VBox,*VBox1,*HBox,*Table,*TitlesBut,*But;
gint i;

if (ProgramState != Free) { Attention(0,"Another task is in progress"); return; }
ProgramState=DoingSetup; BatStop=FALSE; Setup.Hardware.AutoStopOn=FALSE;

CreateDir();                                     //Create directories as per user preferences if they dont exist already

Bat=g_new(struct Bat,1);

TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { TitlesStyle->fg[i]=TitlesStyle->text[i]=TitlesFg; TitlesStyle->bg[i]=TitlesBg; } 

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_title(GTK_WINDOW(Win),"Batch Acquisition Setup");
gtk_widget_set_uposition(GTK_WIDGET(Win),0,TOP_SPACE);
gtk_widget_set_size_request(GTK_WIDGET(Win),-1,300);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(CloseBatchAcq),NULL);

VBox=gtk_vbox_new(FALSE,12); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),5);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,TRUE,0);
Bat->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Bat->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(GTK_WIDGET(Bat->ScrlW),520,30);
gtk_box_pack_start(GTK_BOX(VBox1),Bat->ScrlW,FALSE,FALSE,0);

Table=gtk_table_new(1,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Bat->ScrlW),Table);
for (i=0;i<5;++i)
    {
    TitlesBut=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(TitlesBut,TitlesStyle);
    gtk_widget_set_usize(GTK_WIDGET(TitlesBut),ColWidth[i],26);
    gtk_table_attach(GTK_TABLE(Table),TitlesBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

Bat->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Bat->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox1),Bat->ScrlW,TRUE,TRUE,0);

Bat->Table=gtk_table_new(Setup.BatAcq.NBat,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Bat->ScrlW),Bat->Table);
for (i=0;i<Setup.BatAcq.NBat;++i) MakeBatRow(i,Bat->Table);
Bat->Row=0;

VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,FALSE,FALSE,0);
But=gtk_button_new_with_label("Quick Set"); gtk_box_pack_start(GTK_BOX(VBox1),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(QuickBat),NULL);
But=gtk_button_new_with_label("Add"); gtk_box_pack_start(GTK_BOX(VBox1),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatAdd),NULL);
But=gtk_button_new_with_label("Clear"); gtk_box_pack_start(GTK_BOX(VBox1),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatClear),NULL);
But=gtk_button_new_with_label("Clear All"); gtk_box_pack_start(GTK_BOX(VBox1),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatClearAll),NULL);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Load Batch"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(LoadBatAcq),NULL);
But=gtk_button_new_with_label("Save Batch"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveBatAcq),NULL);
But=gtk_button_new_with_label("Start Batch"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(StartBatAcq),Win);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CancelBatAcq),Win);

gtk_widget_show_all(Win);
gtk_style_unref(TitlesStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BatStopAtOnce(GtkWidget *W,GtkWidget *Win)
{ 
StopNicely(); 
BatStop=TRUE; gtk_widget_destroy(Win);
strcpy(PrevRun,RunName); BatchRunning=FALSE;
pthread_cancel(CntlBat);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BatStopAfterCurrentRun(GtkWidget *W,GtkWidget *Win)
{ BatStop=TRUE; gtk_widget_destroy(Win); }
/*----------------------------------------------------------------------------------------------------------------------*/
void BatchTerminate(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*BBox,*But;

if (!BatchRunning) return;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_title(GTK_WINDOW(Win),"Terminate");
gtk_widget_set_uposition(GTK_WIDGET(Win),0,TOP_SPACE);
gtk_widget_set_usize(GTK_WIDGET(Win),125,150);
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

BBox=gtk_vbutton_box_new(); gtk_container_add(GTK_CONTAINER(Win),BBox);
gtk_container_set_border_width(GTK_CONTAINER(BBox),5);

But=gtk_button_new_with_label("Stop now");
gtk_container_add(GTK_CONTAINER(BBox),But);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatStopAtOnce),Win);

But=gtk_button_new_with_label("Stop after\ncurrent run");
gtk_container_add(GTK_CONTAINER(BBox),But);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(BatStopAfterCurrentRun),Win);

But=gtk_button_new_with_label("Cancel");
gtk_container_add(GTK_CONTAINER(BBox),But);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
