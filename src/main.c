#include <gtk/gtk.h>
#include <sys/vfs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>   /* dirname() */
#include "lamps.h"
#include "common.h"                                                                                   //The NSC header file
#include "daq_engine.h"                                                                             //Phase 2: DAQ callbacks
#include "lamps_cmd.h"                                                                             //Phase 3: remote commands

/* Phase 4: SHM writer — declared here so main() can call open/close.
 * The full writer-side implementation lives in daq_engine.c (which
 * defines LAMPS_SHM_WRITER_SIDE before including lamps_shm.h).        */
extern int  lamps_shm_open_write(void);
extern void lamps_shm_close(void);

/*Function templates*/
void Attention(gint XPos,gchar *Messg);
void GetMainMenu(GtkWidget *Win,GtkWidget **MenuBar); 
void InitializeFont(void); void InitializeColours(void); void DefaultSetup(void); void InitializeFitParas(void); 
void InitializeCalibParas(void); void InitializeBGate(void); void InitializeFlags(void); void InitializeSpecFileNames();
void InitializeSearchedPeaks(void); void InitializeFileColours(void);
void NoCalib(GtkWidget *W,gpointer Unused); void InitializePrefs(void);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
/* Phase 2: DAQ engine callback handlers — implemented in control.c */
void GtkDaqErrorHandler(const char *msg);
void GtkDaqStatusHandler(const DaqStatusEvent *ev);
/* Phase 3: functions needed by the remote command dispatcher */
void StopNicely(void);
void ZeroOned(gint SNo);
void ZeroTwod(gint SNo);
/* Phase 4: headless start for remote control */
gint RemoteStart(const gchar *run_name);

void Screen1(GtkWidget *W,gpointer Data); void Screen2(GtkWidget *W,gpointer Data);
void Screen3(GtkWidget *W,gpointer Data); void Screen4(GtkWidget *W,gpointer Data);
void PrevScreen(GtkWidget *W,gpointer Data); void LastScreen(GtkWidget *W,gpointer Data);
void ScreenN(GtkWidget *W,gpointer Data); void NextScreen(GtkWidget *W,gpointer Data);

/*Global variables*/
datapack buf,readbuf;                                                                                   //NSC data buffer
struct Setup Setup;                                                                                 //The setup variables
struct PsBGated PsBGated[MAX_PSEUDO];                                                          //Banana gated pseudo type
enum ProgramState ProgramState;                                                //The current running state of the program
enum ReWriteRequest ReWriteRequest;                        //Control of list rewrite (see rewrite.c, zlsgls.c, freedom.c)
gboolean BatchRunning;                                                                      //Status of batch acquisition
gboolean CommonZoom,RealScroll,RealScroll2;                             //For common zoom and scroll of 1d and 2d spectra
guint16 *Oned16,*Twod16;                                                                    //Pointers for 16-bit spectra
guint32 *Oned32,*Twod32,*Proj;                                                              //Pointers for 32-bit spectra
gint Off1d[MAX_1D],Off2d[MAX_2D],OffProj[MAX_2D];                                          //Offsets into spectrum memory
gint Screen=0;                                                                        //Screen number for spectra display
gint FrozenWin=-1;                                                     //Window number that is frozen by user interaction
gboolean UtilBusy=0;                      //Flag indicating that Utilities is in use. Prevents running multiple utilities
gint ScreenSpecNo[SCREEN_TOT][MAX_SCREENS],ScreenSpecType[SCREEN_TOT][MAX_SCREENS];  //Info: each spec win of each screen
GtkWidget *SpecWin[SCREEN_TOT],*Canvas[SCREEN_TOT];                                             //Global spectrum widgets
GtkObject *HScrlAdj[SCREEN_TOT],*VScrlAdj[SCREEN_TOT];                   //Global adjustmnets for horiz. and vert. Scroll
gfloat XSlope[SCREEN_TOT],CountsSlope[SCREEN_TOT];              //These slope arrays are used both by plot1.c and plot2.c
gint CanvasW[SCREEN_TOT],CanvasH[SCREEN_TOT];                                          //Used by both plot1.c and plot2.c
gint AreaMonitor,AreaWinOpen,OvWinOpen,FitWinOpen,CalibWinOpen,AssistWinOpen,BananaWinOpen,BananaDrawMode,BananaShowing,
     BananaWinNo,BananaLineType,BananaMulti,PkSearchOpen,ScalerWinOpen;
GdkFont *Font;
gint Theme1d,Theme2d;                                                                    //Theme settings. -1 for random
GdkGC *Gc;                                                                            //Graphics context for 1d 2d plots
GdkColor Colour1D[10];   //0=Backg,1=Plot,2=FitBkg,3=Axis,4=Title,5=Cursors,6=Pk Markers,7=Fit ROI,8=Fit,9=Perm. Cursors
GdkColor Colour2D[10];                                      //0=Background,1=Axis,2=Title,3=Box,4=Permanent Box,5=Banana
GdkColor ColourOv[MAX_OVERLAP];                                                                    //Colours for overlap
GdkColor ColourMB[7];                                                               // Colours for multiple banana gates
glong D2PenColours[32][3];                                                                    //RGB Colours for 2d plots
gint ThemeChanged;
struct WinProperties Prop[SCREEN_TOT];
struct FitType Fit;                                                                                  //The Fit parameters
struct Calibration Calibration[MAX_TOTAL_PAR];                                            //Calibration of each parameter
struct BGate BGate;                                                                             //Banana gate held in RAM
GtkWidget *S_Stat[15],*S_Scaler[4];                                                                  //Status bar widgets
enum AnalysisRequest AnalysisRequest;
gchar SelRun[MAX_TEXT_FIELD];                                              //Used for run name in iospec.c and readfile.c
gulong ScalerCurr[MAX_SCALER],ScalerPrev[MAX_SCALER];               //Scaler values from the current and previous buffers
gint ScalerOverflows[MAX_SCALER];                                               //The number of overflows for each scaler
gulong ScalerPending[MAX_SCALER];                                                             //Pending value after Pause
gint DrvFd;                                                                                      //Driver file descriptor
gchar RunName[40],PrevRun[40];                                                     //Name of current run and previous run
enum AcqSignal AcqSignal;                                                                        //Stop, Pause and Resume
gint RefreshRate,RefreshOptimum;                                     //The interval (no. of buffers) for screen refreshes
gchar SStop[40];                                                     //The stop time of time of this, or the previous run
GtkWidget **ScalerTotal,**ScalerRate,**ScalerDRate;                                   //Globals for scaler display window
struct BGate MultiBGate[7];                                               //We allow 7 extra banana gates to be displayed
gchar MultiBGateNames[7][LONG_TEXT_FIELD];                                        //File names for the extra banana gates
gboolean MultiBGateFlags[7];                                                          //Flags to indicate bgate is active
GtkWidget **MultiBGateButtons;                                         //Buttons holding file names of multi banana gates
gfloat MacroCurr[MAX_MACRO],MacroPrev[MAX_MACRO],MacroDiff[MAX_MACRO];                        //Computed values of macros
gboolean LogOutBox,MacroOutBox;                                                           //To prevent multiple instances
glong BytesWritten;
struct FileOpenType *FileX;                                                                     //The file browser widget
gchar SetupDir[MAX_DIR_STRLEN],ListFDir[MAX_DIR_STRLEN],BanDir[MAX_DIR_STRLEN],SpecDir[MAX_DIR_STRLEN],
      BatDir[MAX_DIR_STRLEN],CalDir[MAX_DIR_STRLEN],LogDir[MAX_DIR_STRLEN],MacroDir[MAX_DIR_STRLEN];          //Dir prefs
gint TopOfset;                                                 //Accounts for space occupied by window manager at the top
/*----------------------------------------------------------------------------------------------------------------------*/
void SavePrefs(void)
{
FILE *Fp;

if ((Fp=fopen(".lamps_prefs","w")))
   {
   fprintf(Fp,"SetupDir= %s\n",SetupDir); fprintf(Fp,"ListFDir= %s\n",ListFDir);
   fprintf(Fp,"BanDir= %s\n",BanDir); fprintf(Fp,"SpecDir= %s\n",SpecDir);
   fprintf(Fp,"BatDir= %s\n",BatDir); fprintf(Fp,"CalDir= %s\n",CalDir);
   fprintf(Fp,"LogDir= %s\n",LogDir); fprintf(Fp,"MacroDir= %s\n",MacroDir);
   fprintf(Fp,"Theme1d= %d\n",Theme1d);
   fprintf(Fp,"Theme2d= %d\n",Theme2d);
   fclose(Fp);
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DestroyMain(GtkWidget *W,gpointer Data)
{
if (ProgramState == Free) { SavePrefs(); gtk_main_quit(); }
else                      Attention(0,"Cant quit now. Task in progress");
}
/*----------------------------------------------------------------------------------------------------------------------*/
gboolean DeleteMain(GtkWidget *W,GdkEvent *Event,gpointer Data)
{
if (ProgramState != Free) { Attention(0,"Cant quit now. Task in progress"); return TRUE; };
return FALSE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void RefreshRateCallBack(GtkWidget *W,gpointer Data)
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Optimum")) { RefreshRate=100; RefreshOptimum=1; return; }
else if (!strcmp(Str,"No Refresh")) { RefreshRate=0; RefreshOptimum=0; return; }
else { RefreshRate=atoi(Str); RefreshOptimum=0; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CommonZoomCallBack(GtkWidget *CheckBut,gpointer Data)
{
RealScroll=TRUE; RealScroll2=TRUE;                                                                     //Extra precaution!
if (GTK_TOGGLE_BUTTON(CheckBut)->active) CommonZoom=TRUE;
else                                     CommonZoom=FALSE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
/* Phase 3: epics_cmd_poll_cb()
 *
 * GTK timeout callback — called every 500 ms on the GTK main thread.
 * Reads one pending command from the bridge command SHM and dispatches it.
 *
 * Thread safety: runs on the GTK main thread (inside gdk_threads_enter),
 * so it is safe to set AcqSignal and call StopNicely/ZeroOned directly.
 *
 * Startup order independence: if the bridge was not yet running when LAMPS
 * started, s_cmd will be NULL. We retry lamps_cmd_open_read() on every tick
 * until it succeeds — so LAMPS and the bridge can start in any order.
 */
static gboolean epics_cmd_poll_cb(gpointer unused)
{
    uint32_t cmd;
    (void)unused;

    /* Lazy-attach: retry every 500 ms until the bridge creates the cmd SHM */
    if (!lamps_cmd_is_attached()) {
        if (lamps_cmd_open_read() == 0)
            fprintf(stderr, "[LAMPS] CMD channel attached (bridge came up)\n");
        return TRUE;   /* nothing to poll yet — wait for next tick */
    }

    cmd = lamps_cmd_poll();
    if (cmd == LAMPS_CMD_NONE) return TRUE;  /* nothing pending — keep timer */

    switch (cmd) {
    case LAMPS_CMD_STOP:
        fprintf(stderr, "[LAMPS] EPICS remote STOP received\n");
        if (AcqSignal != Stop) StopNicely();
        else fprintf(stderr, "[LAMPS] EPICS STOP ignored — not acquiring\n");
        break;

    case LAMPS_CMD_RESET:
        fprintf(stderr, "[LAMPS] EPICS remote RESET received\n");
        if (AcqSignal != Stop) StopNicely();
        /* Give the acquisition thread 200 ms to see the Stop signal,
         * then zero all spectra.  ZeroOned/ZeroTwod are GTK-safe here.  */
        usleep(200000);
        ZeroOned(-1);
        ZeroTwod(-1);
        fprintf(stderr, "[LAMPS] EPICS RESET: spectra cleared\n");
        break;

    case LAMPS_CMD_START: {
        char rn[LAMPS_CMD_NAME_LEN] = {0};
        lamps_cmd_get_run_name(rn, sizeof(rn));
        fprintf(stderr, "[LAMPS] EPICS remote START (run_name=\"%s\")\n", rn);
        RemoteStart(rn);
        break;
    }

    default:
        fprintf(stderr, "[LAMPS] EPICS: unknown command %u ignored\n", cmd);
        break;
    }

    return TRUE;   /* keep the timer running */
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint main(int argc,char *argv[])
{
GtkWidget *Win,*VBox,*VBox1,*MenuBox,*MenuBar,*HBox,*HBox1,*Combo,*Label,*But,*CheckBut;
GList *GList;
static GdkColor RedC    = {0,0xFFFF,0x0000,0x0000};
static GdkColor BlueC  =  {0,0x0000,0x0000,0xFFFF};
GtkStyle *RedStyle,*BlueStyle;
gint i;
struct statfs StatBuf;
gchar Str[80];
gfloat DFree;

/* ── Ensure we always run from the directory containing the lamps binary.
 * This makes ./ldcmc100, ./zmass, ./ascii2d etc. work regardless of
 * where the user launched lamps from (e.g. /home/user/Downloads/repo/).  */
{
    char argv0_copy[4096];
    strncpy(argv0_copy, argv[0], sizeof(argv0_copy) - 1);
    argv0_copy[sizeof(argv0_copy) - 1] = '\0';
    const char *bin_dir = dirname(argv0_copy);
    if (chdir(bin_dir) != 0)
        fprintf(stderr, "[LAMPS] WARNING: could not chdir to %s\n", bin_dir);
    else
        fprintf(stderr, "[LAMPS] Working directory set to %s\n", bin_dir);
}

Setup.Simulator=FALSE; Setup.Print.Yes=FALSE;
if (argc>1)
   {
   g_print("COMMAND LINE OPTIONS: lamps --simulator --print\n");
   for (i=1;;++i)
       {
       if (!argv[i]) break;
       if (!strcmp(argv[i],"--simulator")) { Setup.Simulator=TRUE; g_print("Starting in Simulator Mode...\n"); }
       if (!strcmp(argv[i],"--print")) 
          {
          Setup.Print.Yes=TRUE;
          g_print("Which buffer number to print?(>=1)"); scanf("%d",&Setup.Print.BufferNo);
          g_print("How many words to print?"); scanf("%d",&Setup.Print.Wds);
          g_print("%d words at the start of buffer number %d will be displayed\n",Setup.Print.Wds,Setup.Print.BufferNo);
          }
       }
   }

g_thread_init(NULL); gdk_threads_init(); gdk_threads_enter();

gtk_init(&argc,&argv); ProgramState=Free;
/* Phase 4: Create the shared-memory segment immediately at startup.
 * This replaces any stale /lamps_daq_shm left by a previous sim_shm or
 * bridge session, so the bridge immediately sees a fresh STOPPED state
 * rather than looping on stale RUNNING data from the old segment.      */
if (lamps_shm_open_write() < 0)
    fprintf(stderr, "[LAMPS] WARNING: could not create SHM segment\n");
else
    fprintf(stderr, "[LAMPS] SHM segment created (STATUS=STOPPED, waiting for acquisition)\n");
/* Phase 2: Register DAQ engine callbacks with the GTK handler functions */
daq_set_error_cb(GtkDaqErrorHandler);
daq_set_status_cb(GtkDaqStatusHandler);
InitializeFont(); InitializeColours(); InitializeFlags(); InitializePrefs(); DefaultSetup();
InitializeFitParas(); InitializeFileColours(); InitializeCalibParas(); NoCalib(NULL,NULL); 
InitializeBGate(); InitializeSpecFileNames(); InitializeSearchedPeaks();

strcpy(RunName,"Dummy"); strcpy(PrevRun,"Dummy");                                                              //Run names

RedStyle=gtk_style_copy(gtk_widget_get_default_style()); BlueStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { RedStyle->fg[i]=RedStyle->text[i]=RedC; BlueStyle->fg[i]=BlueStyle->text[i]=BlueC; }

statfs("./",&StatBuf);                                                                               //Get disk statistics

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_signal_connect(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(DeleteMain),NULL);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(DestroyMain),NULL);
gtk_window_set_title(GTK_WINDOW(Win),"LAMPS - CMC100  A.Chatterjee (01 Oct 2010)");
gtk_widget_set_uposition(Win,0,0);
gtk_window_set_policy(GTK_WINDOW(Win),FALSE,FALSE,TRUE);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
MenuBox=gtk_vbox_new(FALSE,1);
gtk_box_pack_start(GTK_BOX(VBox),MenuBox,FALSE,FALSE,0);

GetMainMenu(Win,&MenuBar); 
gtk_box_pack_start(GTK_BOX(MenuBox),MenuBar,FALSE,TRUE,0); 

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
/*--------------------------------------------------Screen Selector----------------------------------------------------*/
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0); 
Label=gtk_label_new("Screen Selector"); gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0); 
SetStyleRecursively(Label,RedStyle); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
But=gtk_button_new_with_label("1"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Screen1),NULL);
But=gtk_button_new_with_label("2"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Screen2),NULL);
But=gtk_button_new_with_label("+"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(NextScreen),NULL);
But=gtk_button_new_with_label("N"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ScreenN),NULL);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
But=gtk_button_new_with_label("3"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Screen3),NULL);
But=gtk_button_new_with_label("4"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Screen4),NULL);
But=gtk_button_new_with_label("-"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PrevScreen),NULL);
But=gtk_button_new_with_label("Last"); gtk_box_pack_start(GTK_BOX(HBox1),But,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(But),40,-1); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(LastScreen),NULL);
/*--------------------------------------------------Refresh Rate---------------------------------------------------------*/
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0); 
Label=gtk_label_new("Refresh Rate"); gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0); 
SetStyleRecursively(Label,BlueStyle);
Combo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Combo),87,-1); RefreshRate=1; RefreshOptimum=0;
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),TRUE);
GList=NULL;
GList=g_list_append(GList,"Optimum"); GList=g_list_append(GList,"1 Buffer"); GList=g_list_append(GList,"10 Buffers");
GList=g_list_append(GList,"100 Buffers"); GList=g_list_append(GList,"500 Buffers"); 
GList=g_list_append(GList,"No Refresh");
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);                                                 //Define the popup
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"1 Buffer");
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(RefreshRateCallBack),NULL);
gtk_box_pack_start(GTK_BOX(VBox1),Combo,FALSE,FALSE,1);
/*--------------------------------------------------Common Zoom----------------------------------------------------------*/
CheckBut=gtk_check_button_new_with_label("Common Zoom"); gtk_box_pack_start(GTK_BOX(VBox1),CheckBut,FALSE,FALSE,1); 
gtk_signal_connect(GTK_OBJECT(CheckBut),"toggled",GTK_SIGNAL_FUNC(CommonZoomCallBack),NULL);
/*--------------------------------------------------Status and Disk------------------------------------------------------*/
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[0]=gtk_label_new("Status: Free "); SetStyleRecursively(S_Stat[0],RedStyle); 
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[0],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
sprintf(Str,"Disk Cap: %3.1fGb",(gfloat)StatBuf.f_blocks*StatBuf.f_bsize/1024.0/1024.0/1024.0);
S_Stat[1]=gtk_label_new(Str); SetStyleRecursively(S_Stat[1],RedStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[1],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
DFree=(gfloat)StatBuf.f_bavail*StatBuf.f_bsize/1024.0/1024.0/1024.0; sprintf(Str,"Disk Free: %-3.3fGb",DFree);
if (DFree<1.0) { DFree=1024.0*DFree; sprintf(Str,"Disk Free: %-3.1fMb",DFree); }
if (DFree<1.0) { DFree=1024.0*DFree; sprintf(Str,"Disk Free: %-3.1fKb",DFree); }
S_Stat[2]=gtk_label_new(Str); SetStyleRecursively(S_Stat[2],RedStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[2],FALSE,FALSE,0);

/*------------------------------------------------Run Start/Stop---------------------------------------------------------*/
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[3]=gtk_label_new("Run:"); SetStyleRecursively(S_Stat[3],BlueStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[3],FALSE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[4]=gtk_label_new("Start:");   SetStyleRecursively(S_Stat[4],BlueStyle); 
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[4],FALSE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[5]=gtk_label_new("Elapsed:"); SetStyleRecursively(S_Stat[5],BlueStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[5],FALSE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[6]=gtk_label_new("Stop:");    SetStyleRecursively(S_Stat[6],BlueStyle); 
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[6],FALSE,FALSE,0);

/*-----------------------------------------------Buffers and Speed-------------------------------------------------------*/
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[ 7]=gtk_label_new("Buffs Acq:"); SetStyleRecursively(S_Stat[ 7],RedStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[7],FALSE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[ 8]=gtk_label_new("Processed:"); SetStyleRecursively(S_Stat[ 8],RedStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[8],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[ 9]=gtk_label_new("Kb/s:"); SetStyleRecursively(S_Stat[ 9],RedStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[9],FALSE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[10]=gtk_label_new("Evt/s:"); SetStyleRecursively(S_Stat[10],RedStyle); 
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[10],FALSE,FALSE,0);

/*-----------------------------------------------Bytes Acq, File Bytes---------------------------------------------------*/
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[11]=gtk_label_new("BytesAcqd.:"); SetStyleRecursively(S_Stat[11],BlueStyle); 
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[11],FALSE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[12]=gtk_label_new("ListF Bytes:");  SetStyleRecursively(S_Stat[12],BlueStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[12],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[13]=gtk_label_new("EventsAcq:");  SetStyleRecursively(S_Stat[13],BlueStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[13],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Stat[14]=gtk_label_new("Dead  Time:");  SetStyleRecursively(S_Stat[14],BlueStyle);
gtk_box_pack_start(GTK_BOX(HBox1),S_Stat[14],FALSE,FALSE,0);

/*-----------------------------------------------------Scalers-----------------------------------------------------------*/
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0); 
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Scaler[0]=gtk_label_new("Scaler1:"); gtk_box_pack_start(GTK_BOX(HBox1),S_Scaler[0],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Scaler[1]=gtk_label_new("Scaler2:"); gtk_box_pack_start(GTK_BOX(HBox1),S_Scaler[1],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Scaler[2]=gtk_label_new("Scaler3:"); gtk_box_pack_start(GTK_BOX(HBox1),S_Scaler[2],FALSE,FALSE,0);
HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox1),HBox1,FALSE,FALSE,0);
S_Scaler[3]=gtk_label_new("Scaler4:"); gtk_box_pack_start(GTK_BOX(HBox1),S_Scaler[3],FALSE,FALSE,0);

gtk_widget_show_all(Win);
gtk_style_unref(RedStyle); gtk_style_unref(BlueStyle);
gtk_window_get_position(GTK_WINDOW(Win),NULL,&TopOfset);

/* Phase 3: attach command SHM reader and poll every 500 ms on the GTK
 * main thread. Safe to call StopNicely/ZeroOned here because we are
 * running inside gdk_threads_enter (the GTK main thread).             */
lamps_cmd_open_read();   /* non-fatal if bridge not yet started        */
g_timeout_add(500, epics_cmd_poll_cb, NULL);

gtk_main(); gdk_threads_leave();
/* Phase 4: clean up SHM on normal exit so the bridge sees OFFLINE      */
lamps_shm_close();
g_list_free(GList);
return 0;
}
/*------------------------------------------------------------------------------------------------------------------------*/

