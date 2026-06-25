/* daq_engine.c — DAQ Engine Implementation
 *
 * Phase 2: Incremental extraction from control.c.
 * This file must compile without any GTK headers.
 *
 * Extraction order (per DAQ_DATAFLOW.md):
 *   Step 1 (current): CheckLGates(), CompressToDisk()  ← LOW risk
 *   Step 2 (future):  DecodeSparseData()               ← MEDIUM risk
 *   Step 3 (future):  BuildSpectra()                   ← HIGH risk
 *   Step 4 (future):  AcquireData() loop               ← HIGH risk
 */

#include <gtk/gtk.h>      /* Must be first: defines GtkWidget used in lamps.h */
#include "daq_engine.h"
#include "lamps.h"        /* Setup struct, glib types, MAX_* constants     */
#include "common.h"
#include "cc2002.h"       /* STOP_ACQN and other CAMAC protocol constants  */
#include "libcamac.h"     /* camac_write_short, camac_read, etc.           */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Phase 4: Shared-memory EPICS bridge — writer side.
 * The LAMPS_SHM_WRITER_SIDE guard enables the extern global declarations
 * inside lamps_shm.c that resolve against the lamps binary.              */
#define LAMPS_SHM_WRITER_SIDE
#include "lamps_shm.h"

/* -----------------------------------------------------------------------
 * ARCHITECTURAL NOTE (Phase 2):
 * daq_engine.c includes lamps.h and gtk/gtk.h for glib types and the
 * Setup struct. However, the boundary rule is:
 *   - This file MUST NOT call any gtk_*() functions directly.
 *   - All UI updates go through the DaqErrorCb callback.
 *   - GTK headers are only included for type compatibility.
 *
 * In Phase 3/4, lamps.h will be split into lamps_types.h (GTK-free) and
 * lamps_gui.h, allowing daq_engine.c to become fully GTK-independent.
 * ----------------------------------------------------------------------- */


/* -----------------------------------------------------------------------
 * Error callback — default: print to stderr.
 * GTK layer replaces this with SAttention() via daq_set_error_cb().
 * ----------------------------------------------------------------------- */
static DaqErrorCb s_error_cb = NULL;

void daq_set_error_cb(DaqErrorCb cb)
{
    s_error_cb = cb;
}

static void daq_error(const char *msg)
{
    if (s_error_cb)
        s_error_cb(msg);
    else
        fprintf(stderr, "[DAQ ERROR] %s\n", msg);
}

/* -----------------------------------------------------------------------
 * Status callback — replaces gdk_threads_enter/gtk_label_set_text blocks
 * that live inside AcquireData. The GTK layer registers its own handler
 * which performs the actual widget updates inside gdk_threads_enter/leave.
 * The DAQ engine fires daq_notify_status() without knowing about GTK at all.
 * ----------------------------------------------------------------------- */
static DaqStatusCb s_status_cb = NULL;

void daq_set_status_cb(DaqStatusCb cb)
{
    s_status_cb = cb;
}

void daq_notify_status(const char *event, const char *run_name,
                       const char *prev_run, int hh, int mm, int ss)
{
    if (s_status_cb) {
        DaqStatusEvent ev;
        ev.event    = event;
        ev.run_name = run_name;
        ev.prev_run = prev_run;
        ev.hh = hh; ev.mm = mm; ev.ss = ss;
        s_status_cb(&ev);
    }
}

/* -----------------------------------------------------------------------
 * Internal DAQ engine state
 * ----------------------------------------------------------------------- */
static pthread_t s_acq_thread;

/* -----------------------------------------------------------------------
 * Phase 4: Spectrum mutex
 * Protects Oned16/Oned32/Twod16/Twod32 between the DAQ writer thread
 * (ProcessEvent inside daq_build_spectra) and lamps_shm_update().
 * Initialized once in daq_acquire_data before the first buffer.
 * ----------------------------------------------------------------------- */
static pthread_mutex_t s_spec_mutex = PTHREAD_MUTEX_INITIALIZER;

void daq_spec_lock(void)   { pthread_mutex_lock(&s_spec_mutex);   }
void daq_spec_unlock(void) { pthread_mutex_unlock(&s_spec_mutex); }

/* -----------------------------------------------------------------------
 * External globals still required during Phase 2.

 * These will be progressively eliminated as the engine matures.
 * NOTE: marked clearly so they are easy to find and remove later.
 * ----------------------------------------------------------------------- */
extern struct Setup    Setup;
extern gint            XEvents, XPtr, XWdsLeft, XWds;
extern gint            XExtraPar, XPar, XClrPar;
extern gshort          NPar1, NPar2;
extern glong           BytesWritten, BuffersAcquired, BuffersProcessed, BufPrev;
extern gulong          XTotEvts;
extern gulong          ScalerCurr[], ScalerPrev[], ScalerPending[];
extern gint            ScalerOverflows[];
extern enum AcqSignal  AcqSignal;
extern gint            DrvFd;
extern enum ProgramState ProgramState;
extern gint            RefreshRate;
extern gchar           RunName[], SStop[];
extern gchar           ListFDir[], SpecDir[], LogDir[], MacroDir[];
extern guint16        *AcqBuf1, *AcqBuf2;
extern gfloat          MacroCurr[], MacroPrev[], MacroDiff[];
extern pthread_t       Spec, AStop;
extern FILE           *LogFp, *ScalerFp;
extern gdouble         StartTime, TimePrev, PauseDuration, PauseTime;
extern time_t          TStart;

/* External logic functions still needed */
void ProcessEvent(gshort *P, gboolean Option);

/* Forward declaration: writeEventBlock lives in freedom.c (NSC Candle format) */
gboolean writeEventBlock(gint eventSize, gint ev_per_block, gushort data[], FILE *Fp);
void     writeNames(int noOfAdcs, int noOfSingles, int noOfScalers, FILE *Fp);
void     queue_text(enum blocktype block, gchar *txt, FILE *Fp);
void     writeEndOfFile(FILE *Fp);

/* Forward declaration: StopNicely lives in control.c */
void StopNicely(void);

/* Functions called by AcquireData still living in control.c / other files */
void  ExecuteClrCommands(void);
void  ExecuteExtraCommands(void);
void  HardResetCmc100(void);
void  ControlAutoStop(gpointer Unused);  /* thread fn */
void  UpdateDisplay(gdouble *ElapsedTime, gboolean Final, gint XBytes);
void  Safety(gchar *RunName, gdouble *ElapsedTime, gint *SpecSaved);
void  PeriodicLog(gint Status, gchar *RunName, glong BAcq, glong BProc,
                  glong Bytes, time_t TStart, gdouble StartTime);
void  ComputeMacros(glong BuffersAcquired, FILE *MacroFp);
void  Write1d(gchar *FName, gint i, gboolean InThread);
void  Write2d16(gchar *FName, gint i, gboolean InThread);
void  Write2d32(gchar *FName, gint i, gboolean InThread);
void  SAttention(gchar *Messg);
void  Pad(gchar *Str, gint Len);
void  AbbreviateFileName(gchar *Dest, gchar *Src, gint MaxLen);
void *BuildSpectra(void *Data);
void  fstart_(gchar *RunName, int Len);


/* -----------------------------------------------------------------------
 * STEP 1 EXTRACTION: daq_check_lgates()
 * Moved from control.c:281 — CheckLGates()
 *
 * Checks whether a single decoded event (Par[]) passes all configured
 * list-mode gates. Returns FALSE if any gate rejects the event.
 * ----------------------------------------------------------------------- */
gboolean daq_check_lgates(gushort *Par)
{
    gint i;

    for (i = 0; i < Setup.ListMode.NoOfLGates; ++i) {
        if ((Par[Setup.ListMode.LGates[i].Para - 1] < Setup.ListMode.LGates[i].Lo) ||
            (Par[Setup.ListMode.LGates[i].Para - 1] > Setup.ListMode.LGates[i].Hi))
            return FALSE;
    }
    return TRUE;
}

/* -----------------------------------------------------------------------
 * Internal: ZlsError — disk-full handler.
 * Reports error via registered callback and triggers a clean acquisition stop.
 * Replaces the old ZlsError() in control.c which called SAttention() directly.
 * ----------------------------------------------------------------------- */
static void zls_error(void)
{
    daq_error("Error: Disk Full...Acq Stopped!");
    StopNicely();
}

/* -----------------------------------------------------------------------
 * STEP 1 EXTRACTION: daq_compress_to_disk()
 * Moved from control.c:293 — CompressToDisk()
 *
 * Compresses one buffer of decoded 16-bit events (AcqBuf) and writes
 * them to the open list-mode file Fp in the configured format:
 *   case 0: NSC Candle / Freedom format
 *   case 1: Normal ZLS zero-suppression format
 *
 * Returns TRUE on success, FALSE on write error.
 * ----------------------------------------------------------------------- */
gint daq_compress_to_disk(FILE *Fp, gushort *AcqBuf)
{
    gint i, j, k, l, m, Ptr1, Ptr2, CPtr, CBufSiz, HdrWds;
    gushort Par[MAX_PAR];
    gushort CompBuf[500000]; /* compressed buffer — dimension not finalised */
    gushort BitMask[MAX_HDR]; /* one 16-bit event mask per 16 parameters   */
    static gchar Sign[4] = {"DAPS"};
    gshort Code;
    gboolean AcceptEvent; /* reject event if all params outside [LLD,ULD] */

    switch (Setup.ListMode.Compr) {

    case 0: /* NSC Candle format — interleave crate 1 & crate 2 params */
        for (i = 0, Ptr1 = 0, Ptr2 = XWds, CPtr = 0; i < XEvents; ++i) {
            for (j = 0; j < NPar1; ++j) CompBuf[CPtr + j] = AcqBuf[Ptr1 + j];
            CPtr += NPar1;
            for (j = 0; j < NPar2; ++j) CompBuf[CPtr + j] = AcqBuf[Ptr2 + j];
            CPtr += NPar2;
            Ptr1 += NPar1;
            Ptr2 += NPar2;
        }
        if (!writeEventBlock(Setup.Parameter.NPar, XEvents, CompBuf, Fp)) {
            zls_error();
            return FALSE;
        }
        break;

    case 1: /* Normal ZLS format — bit-mask header + non-zero words only */
        HdrWds = (Setup.Parameter.NPar + 15) / 16;
        for (i = 0, Ptr1 = 0, Ptr2 = XWds, CPtr = 0, AcceptEvent = FALSE;
             i < XEvents; ++i) {
            for (j = 0; j < HdrWds; ++j) BitMask[j] = 0;
            for (j = 0, m = 0; j < Setup.Parameter.NPar; ++j) {
                k = j / 16;
                l = j % 16;
                Par[j] = (j < NPar1) ? AcqBuf[Ptr1 + j]
                                      : AcqBuf[Ptr2 + j - NPar1];
                if ((Par[j] >= Setup.Parameter.LLD[j]) &&
                    (Par[j] <= Setup.Parameter.ULD[j])) {
                    AcceptEvent = TRUE;
                    BitMask[k] |= 1 << l;
                    CompBuf[CPtr + HdrWds + m] = Par[j];
                    m++;
                }
            }
            /* Gated list mode: apply extra gate conditions */
            if (Setup.ListMode.NoOfLGates)
                if (!daq_check_lgates(Par)) AcceptEvent = FALSE;
            if (AcceptEvent) {
                for (j = 0; j < HdrWds; ++j) CompBuf[CPtr + j] = BitMask[j];
                CPtr += HdrWds + m;
            }
            Ptr1 += NPar1;
            Ptr2 += NPar2;
        }
        CBufSiz = 2 * CPtr;
        if (fwrite(Sign, 4, 1, Fp) < 1)          { zls_error(); return FALSE; }
        if (fwrite(&CBufSiz, 4, 1, Fp) < 1)       { zls_error(); return FALSE; }
        if (fwrite(CompBuf, 2, CPtr, Fp) < CPtr)  { zls_error(); return FALSE; }
        BytesWritten += (8 + 2 * CPtr);
        break;

    case 2:
    case 3:
        /* Unsupported formats — stop acquisition cleanly */
        AcqSignal = Stop;
        Code = STOP_ACQN;
        camac_write_short(DrvFd, Code);
        daq_error("Error: List format presently unsupported");
        break;
    }

    return TRUE;
}

/* -----------------------------------------------------------------------
 * STEP 2 EXTRACTION: daq_print_buffer()
 * Moved from control.c:362 — PrintBuffer()
 *
 * Debug utility: prints the raw CAMAC word buffer to stdout.
 * Only called when Setup.Print.Yes is set for a specific buffer.
 * Pure g_print() — no GTK calls.
 * ----------------------------------------------------------------------- */
void daq_print_buffer(guint *CBuf, gint CWds)
{
    gint i;

    g_print("Buffer contains %d words of which %d are shown below:\n",
            CWds, Setup.Print.Wds);
    for (i = 0; i < Setup.Print.Wds; ++i) {
        switch (CBuf[i] & 0xFFFFFF) {
        case PHILLIPS_START:   g_print("i=%d -----PHILLIPS_START-------\n",   i); break;
        case SEQUENTIAL_START: g_print("i=%d -----SEQUENTIAL_START-----\n",   i); break;
        case WIENER_START:     g_print("i=%d -----WIENER_START---------\n",   i); break;
        case SCALERS_START:    g_print("i=%d -----SCALERS_START--------\n",   i); break;
        case CLEAR_START:      g_print("i=%d -----CLEAR_START----------\n",   i); break;
        case PHILLIPS_END:     g_print("i=%d =====PHILLIPS_END=========\n",   i); break;
        case SEQUENTIAL_END:   g_print("i=%d =====SEQUENTIAL_END=======\n",   i); break;
        case WIENER_END:       g_print("i=%d =====WIENER_END===========\n",   i); break;
        case SCALERS_END:      g_print("i=%d =====SCALERS_END==========\n",   i); break;
        case CLEAR_END:        g_print("i=%d =====CLEAR_END============\n",   i); break;
        default:               g_print("i=%d CBuf=%X Q=%d\n", i, CBuf[i],
                                        (CBuf[i] >> 25) & 1);              break;
        }
    }
}

/* -----------------------------------------------------------------------
 * STEP 2 EXTRACTION: daq_decode_sparse_data()
 * Moved from control.c:386 — DecodeSparseData()
 *
 * Parses one raw CMC100 word buffer (CBuf, CWds words) and fills AcqBuf
 * with decoded per-event ADC values.
 *
 * The CMC100 uses a sparse-readout protocol where each data block is
 * bracketed by known literal separator codes (see cc2002.h):
 *   PHILLIPS_START/END   — sparse Phillips ADC hit data
 *   SEQUENTIAL_START/END — sequential (non-sparse) ADC data
 *   SCALERS_START/END    — scaler values appended after ADC data
 *   CLEAR_START/END      — end-of-event marker, increments XEvents
 *
 * On a partial event at buffer boundary:
 *   Sets XPtr and XWdsLeft so the caller can prepend the tail to the
 *   next buffer before calling this function again.
 *
 * Globals written: XEvents, XPtr, XWdsLeft, ScalerCurr[]
 * Globals read:    Setup, BuffersAcquired
 * ----------------------------------------------------------------------- */
void daq_decode_sparse_data(guint *CBuf, gint CWds, gushort *AcqBuf)
{
    gint i, Ptr, PtrEvtStart, ModuleNo, Q, BlockSeparator, AcqPtr, Stn, SubA, PNo;

    /* Optional debug dump of raw buffer words */
    if (Setup.Print.Yes && (BuffersAcquired == Setup.Print.BufferNo))
        daq_print_buffer(CBuf, CWds);

    Ptr = 0; XEvents = 0; PtrEvtStart = 0; ModuleNo = 0; AcqPtr = 0;

    while (1) {
        BlockSeparator = CBuf[Ptr] & 0xFFFFFF; /* 24-bit literal in data stream */

        switch (BlockSeparator) {

        case PHILLIPS_START:
            Stn = Setup.Decode.Module[ModuleNo];
            for (i = 0; i < Setup.Hardware.NParStn[Stn]; ++i)
                AcqBuf[AcqPtr + i] = 0;
            for (i = 0; i < 17; ++i) { /* not more than 17 reads per Phillips module */
                ++Ptr;
                if (Ptr == CWds) { XPtr = PtrEvtStart; XWdsLeft = CWds - PtrEvtStart; return; }
                if ((CBuf[Ptr] & 0xFFFFFF) == PHILLIPS_END) break;
                Q = (CBuf[Ptr] >> 25) & 1;
                if (Q) {
                    SubA = (CBuf[Ptr] >> 12) & 0xF;
                    AcqBuf[AcqPtr + SubA] = CBuf[Ptr] & 0xFFF;
                }
            }
            ++ModuleNo;
            AcqPtr += Setup.Hardware.NParStn[Stn];
            break;

        case SEQUENTIAL_START:
            Stn = Setup.Decode.Module[ModuleNo];
            for (i = 0, PNo = Setup.Hardware.LoParStn[Stn];
                 i < Setup.Hardware.NParStn[Stn]; ++i) {
                ++Ptr;
                if (Ptr == CWds) { XPtr = PtrEvtStart; XWdsLeft = CWds - PtrEvtStart; return; }
                AcqBuf[AcqPtr + i] = CBuf[Ptr] & (Setup.Parameter.Chan[PNo++] - 1);
            }
            ++Ptr;
            if (Ptr == CWds) { XPtr = PtrEvtStart; XWdsLeft = CWds - PtrEvtStart; return; }
            if ((CBuf[Ptr] & 0xFFFFFF) != SEQUENTIAL_END)
                g_print("Missing separator SEQUENTIAL_END\n");
            ++ModuleNo;
            AcqPtr += Setup.Hardware.NParStn[Stn];
            break;

        case WIENER_START:
            break; /* not programmed */

        case SCALERS_START:
            for (i = 0; i < Setup.Scaler.NSc1; ++i) {
                ++Ptr;
                if (Ptr == CWds) { XPtr = PtrEvtStart; XWdsLeft = CWds - PtrEvtStart; return; }
                ScalerCurr[i] = CBuf[Ptr] & 0xFFFFFF;
            }
            ++Ptr;
            if (Ptr == CWds) { XPtr = PtrEvtStart; XWdsLeft = CWds - PtrEvtStart; return; }
            if ((CBuf[Ptr] & 0xFFFFFF) != SCALERS_END)
                g_print("Missing separator SCALERS_END\n");
            ++ModuleNo;
            break;

        case CLEAR_START: /* end-of-event marker — one per event */
            do {
                ++Ptr;
                if (Ptr == CWds) { XPtr = PtrEvtStart; XWdsLeft = CWds - PtrEvtStart; return; }
            } while ((CBuf[Ptr] & 0xFFFFFF) != CLEAR_END);
            ++XEvents;
            ModuleNo = 0;
            PtrEvtStart = Ptr + 1;
            break;

        default:
            g_print("ERROR::Unidentified module code at Ptr=%d\n", Ptr);
            break;
        }

        ++Ptr;
        if (Ptr == CWds) { XPtr = PtrEvtStart; XWdsLeft = CWds - PtrEvtStart; return; }
    }
}

/* -----------------------------------------------------------------------
 * STEP 3 EXTRACTION: daq_build_spectra()
 * Moved from control.c:300 — BuildSpectra()
 *
 * Loops over the decoded 16-bit events in AcqBuf and calls ProcessEvent
 * to calculate pseudo parameters, apply gates, and fill spectra.
 * ----------------------------------------------------------------------- */
void daq_build_spectra(gushort *AcqBuf)
{
    gshort Para[MAX_TOTAL_PAR];
    gint Evt, i, Ptr;

    /* Phase 4: hold the spectrum mutex for the entire fill pass so that
     * lamps_shm_update() (called immediately after) reads a consistent
     * snapshot.  ProcessEvent() writes Oned16/Oned32; we must not allow
     * a concurrent snapshot in the middle of the loop.                 */
    daq_spec_lock();

    for (Evt = 0, Ptr = 0; Evt < XEvents; ++Evt) {
        for (i = 0; i < NPar1; ++i)
            Para[i] = AcqBuf[Ptr + i];

        ProcessEvent(Para, FALSE);
        Ptr += NPar1;
    }
    BuffersProcessed++;

    /* Still under the mutex: update shared memory while spectra are stable */
    lamps_shm_update();  /* Phase 4: push spectrum snapshot to EPICS bridge */

    daq_spec_unlock();
}

/* -----------------------------------------------------------------------
 * Phase 2 Step 4: Extracted AcquireData body from control.c
 * ----------------------------------------------------------------------- */
void *daq_acquire_data(gpointer Data)
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
lamps_shm_open_write();  /* Phase 4: create shared-memory segment for EPICS bridge */

for (i=0,NPar1=NPar2=0;i<Setup.Parameter.NPar;++i) { if (Setup.Parameter.N[i]>MAX_CAMAC_STNS) ++NPar2; else ++NPar1; } 
for (i=0,NScaler1=NScaler2=0;i<Setup.Scaler.NSc;++i) 
    { if (Setup.Scaler.C[i]==1) ++NScaler1; else ++NScaler2; }
for (i=0;i<NPar1;++i) Mask[i]=Setup.Parameter.Chan[i]-1;

if (!Setup.Simulator)
   {
   (void)camac_naf(DrvFd, 0,30,10,26,&XRes,&QRes,&Lam);                                             //Unmask LAM at all stations
   (void)camac_naf(DrvFd, 4,30,0,17,&XRes,&QRes,&Lam);                                                   //Allow LAM to start LP
   ExecuteClrCommands();                                                //Execute clr file commands to get the first LAM
   ExecuteExtraCommands();              //E.g. For Phillips LAM and no CM90, explicitly clear the Phillips unit to start
   (void)camac_naf(DrvFd, 0,Setup.Scaler.N[0],0,9,&XRes,&QRes,&Lam);  //Clear Scaler, assume there is only 1 scaler and it is cleared by A0.F9
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
/* Phase 2: GTK status update moved to DaqStatusCb — no direct GTK calls here */
daq_notify_status("start", RunName, SStop,
                  TmStart->tm_hour, TmStart->tm_min, TmStart->tm_sec);

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
/* Phase 2: RefreshAll + status-bar clear moved to DaqStatusCb */
daq_notify_status("refresh", RunName, SStop,
                  TmStart->tm_hour, TmStart->tm_min, TmStart->tm_sec);

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
    camac_flush(DrvFd);                                                                       //Flush
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
    camac_flush(DrvFd);  //Flush
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
lamps_shm_close();  /* Phase 4: remove shared-memory segment */
ProgramState=Free; pthread_exit(NULL);
return NULL;
}

/* -----------------------------------------------------------------------
 * Phase 3: daq_start()
 * Spawn the acquisition thread. GTK no longer calls pthread_create.
 * ----------------------------------------------------------------------- */
void daq_start(void)
{
    pthread_create(&s_acq_thread, NULL, daq_acquire_data, NULL);
}

/* -----------------------------------------------------------------------
 * Phase 4: daq_stop()
 * Signal the acquisition thread to stop.  Safe to call from any thread
 * (EPICS bridge, CLI, etc.).  Mirrors StopAcquisition() in control.c
 * but without the GTK Attention() guards — callers must check status
 * themselves with daq_get_status().
 * ----------------------------------------------------------------------- */
void daq_stop(void)
{
    gint XRes, QRes, Lam;

    if (AcqSignal == Stop)
        return;  /* already stopped — no-op */

    AcqSignal = Stop;

    /* Disable all LAMs in hardware if the CAMAC driver is open */
    if (DrvFd >= 0 && !Setup.Simulator)
        camac_naf(DrvFd, 0, 30, 10, 24, &XRes, &QRes, &Lam);
}

/* -----------------------------------------------------------------------
 * Phase 4: daq_get_status()
 * Returns the current acquisition state as an integer:
 *   0 = STOPPED, 1 = PAUSED, 2 = RUNNING
 * ----------------------------------------------------------------------- */
int daq_get_status(void)
{
    switch (AcqSignal) {
    case Stop:   return 0;
    case Pause:  return 1;
    case Resume: return 2;
    default:     return 0;
    }
}
