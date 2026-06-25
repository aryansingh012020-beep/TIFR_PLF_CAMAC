/* Header file for lamps (see also constants.h) -- The constants below can be changed as required*/

#ifndef G_THREADS_IMPL_POSIX
#error G_THREADS_IMPL_POSIX should defined in glib so that we can use posix threads.
#endif

#include "constants.h"                                                                   //Constants also needed in user.F
#define MAX_FNAME_LENGTH   255                             //Max no of characters for file names (with full absolute path)
#define MAX_OFFLINE_FILES  200                                   //Max no of Offline files to be analysed in a single pass
#define MAX_REWRITE_FILES  200                                                //Max no of list mode files to be re-written
#define MAX_BAT            200                                                       //Max no of runs in batch acquisition
#define MAX_LGATES         10                                                         //Max no of gates in Gated List Mode
#define MAX_CAMAC_STNS     23                                                          //Number of stations in CAMAC Crate
#define CMC_BUF            1048000                          //Cmc100 FIFO buffer size in 32-bit words --see p.3 (not p.6!)
#define MAX_TEXT_FIELD     30                                                                   //For struct HardwareSetup
#define LONG_TEXT_FIELD    90                                                                   //For struct HardwareSetup
#define MAX_PAR            1000                                                                 //Max number of parameters
#define MAX_PSEUDO         800                                                           //Max number of pseudo-parameters
#define MAX_MACRO          50                                                                       //Max number of macros
#define NTOT (Setup.Parameter.NPar+Setup.Pseudo.N)                                            //Total number of parameters
#define MAX_TOTAL_PAR      (MAX_PAR+MAX_PSEUDO)
#define MAX_HDR            (MAX_PAR+15)/16                        //Max number of HdrWds (16-bit event masks) in zls files
#define MAX_1D             999            //Max number of 1d Spectra (>999 will have problems with the file naming scheme)
#define MAX_2D             500                                                                  //Max number of 2d Spectra
#define MAX_SPECTRA        (MAX_1D+MAX_2D)                                                   //Max total number of spectra
#define MAX_GATES_1D       20                                          //Max number of 1d gates both for 1d and 2d spectra
#define MAX_GATES_2D       20                                          //Max number of 2d gates both for 1d and 2d spectra
#define MAX_ADD_SPEC       200                                       //Max no of 1d/2d spectra that can be added in util.c
#define MAX_SCALER         64                                             //OCCAM program allows upto 64 scalers per crate
/* We stick to this because all the scalers could even be in 1 crate. Display and log file shows only 4 scalers, 
   other scalers are in scaler.log and periodic.log files                                                               */
#define MAX_DATABUF        150000                                              //Dimension of decoded buffer for rewrite.c
#define SCALER_OVERFLOW    16777216                                                             //For 24-bit CAMAC scalers
#define SCREEN_ROWS        3                                                                     //Rows or Spectra Screens
#define SCREEN_COLS        4                                                                    //Cols for Spectra Screens
#define SCREEN_TOT         (SCREEN_ROWS*SCREEN_COLS)                           //The number of spectrum windows per screen
#define MAX_SCREENS        1+(MAX_SPECTRA-1)/SCREEN_TOT                              //Max total number of spectra screens
#define MONITOR_XRES       1024                                                                 //The monitor X resolution
#define MONITOR_YRES       768                                                                  //The monitor Y resolution
#define X_BORDER           11                                                     //X pixels used in the window decoration
#define Y_BORDER           28                                                     //Y pixels used in the window decoration
#define TOP_SPACE          129                                                     //This space holds the main_menu widget
#define BOT_SPACE          56                                                         //This space holds the gnome toolbar
#define CANVAS_MIN_WIDTH   (MONITOR_XRES/SCREEN_COLS-X_BORDER-1)           //Spec window width when all windows are in use
#define CANVAS_MIN_HEIGHT  ((MONITOR_YRES-TOP_SPACE-BOT_SPACE)/SCREEN_ROWS-Y_BORDER-1)  //Spec win height with all windows 
#define ZOOM_LEVELS        5                                                     //No. of zoom levels in 1 and 2d displays
#define MAX_PEAKS          200                                              //Max no. of peaks in peak search and peak fit
#define MAX_CALIB_DATA     50                                              //Maximum number of data points for calibration
#define MAX_BAN_POINTS     200                                                    //Max number of points for a banana gate
#define MAX_BUFSIZ         32768                            //Max size of acquisition buffer (bytes) for 2 crates combined
#define MAX_OVERLAP        50           //Max number of overlap spectra, while changing see InitializeColours() in init.c!
#define MAX_ARRAY          16                                  //Max number of elements in the Array type pseudo parameter
#define MAX_DIR_ENTRIES  5000                                      //FileX Widget: maximum number of files/sub-directories
#define MAX_DIR_STRLEN    255                           //FileX Widget: maximum string length for file and directory names
#define MAX_MASK_SIZE       5                                              //FileX Widget: maximum length of the file mask

//Constants for Block Separators (in Sparse Readout Mode)
#define PHILLIPS_START       0xFF0001
#define PHILLIPS_END         0xFF0002
#define SEQUENTIAL_START     0xFF0003
#define SEQUENTIAL_END       0xFF0004
#define WIENER_START         0xFF0005
#define WIENER_END           0xFF0006
#define SCALERS_START        0xFF00F0
#define SCALERS_END          0xFF00F1
#define CLEAR_START          0xFF00F2
#define CLEAR_END            0xFF00F3

enum Condition         {And,Or};
enum ProgramState      {Free,DoingSetup,Invalid,AcqOn,AnalysisSetup,AnalysisOn,Reading,Writing,ReWriteOn};
enum AnalysisRequest   {NormalAnalysis,PauseAnalysis,StopAnalysis,StopAllAnalysis};
enum ReWriteRequest    {NormalReWrite,StopAllReWrite};
enum AdcLam            {Disabled,Enabled};
enum AdcMode           {Coincidence,CoinGate,Singles,SingGate,Test,NotRelevant};
enum PseudoType        {Sum,Product,Ratio,Position,PI,Multiplicity,User,Array,BGated};
enum MacroType         {MacroArea,MacroIntegral,MacroRectangle,MacroBanana,MacroRatio,MacroProduct,
                        MacroSum,MacroSqrt,MacroScaler,MacroUser};
enum TailOpt           {None,Left,Right,Both};
enum ShapeOpt          {Same,Indep,ShapeCal};
enum StatsOpt          {Normal,LowStat};
enum FitFlags          {Variable,Fixed,Constrained,NotApplicable};
enum MouseContext      {AddPeaks,DelPeaks};
enum AcqSignal         {Stop,Pause,Resume};
enum ExoOpt            {AcceptAll,RejectCsEvents,RejectPuEvents,RejectCsPu,OnlyCsEvents,OnlyPuEvents};   //For Exogam data
enum TScale            {Log,Sqrt,Lin};
enum MatrixType        {GammaGamma,DCO,Other};

struct FileSelectType  {                                                                 //For the FileSelect widget FileX
                       GtkWidget *But; GtkWidget *FEntry; GtkWidget *PEntry; GtkWidget *MEntry; GtkWidget *Win; 
                       GtkWidget *Label1; GtkWidget *Label2;
                       gint N; gint Index; gint Files;
                       gchar Mask[MAX_MASK_SIZE]; gchar Path[MAX_DIR_STRLEN]; gchar Names[MAX_DIR_ENTRIES][MAX_DIR_STRLEN];
                       gchar TargetFile[MAX_DIR_STRLEN];
                       };
struct WidgetInt       {GtkWidget *W; gint N;};                                            //Holds a widget and an integer

struct Gate1d          {gint Para; gint Lo; gint Hi;};                                            //Structure for 1d gates
struct BGate           {gint XPar; gint YPar; gint N; gint X[MAX_BAN_POINTS]; gint Y[MAX_BAN_POINTS]; 
                        gint XMin; gint XMax; gint YMin; gint YMax; };                            //Structure for 2d gates
struct SpecGate1       {gint NGates; enum Condition Cond; struct Gate1d Gate1d[MAX_GATES_1D];};      
                       //1d gates for both 1d and 2d spectra
struct SpecGate2       {gint NGates; enum Condition Cond; gchar Gate2d[MAX_GATES_2D][MAX_FNAME_LENGTH];
                        struct BGate BGates[MAX_GATES_2D];};                                          
                       //2d gates for both 1d and 2d spectra
struct CDdet           {gboolean Enabled; gint P[8];};                                         //For Twod type CD detector
struct Properties      {enum AdcLam AdcLam; enum AdcMode AdcMode; gint AdcLLD; gint AdcFCode; gint AdcGain; 
                        gint SilenaOffset[8]; gint LowerThreshold[16]; gint UpperThreshold[16];};
struct BatAcqSetup     {gint NBat; gchar RunName[MAX_BAT][MAX_FNAME_LENGTH]; gchar SetFName[MAX_BAT][MAX_FNAME_LENGTH];
                        gchar Start[MAX_BAT][MAX_TEXT_FIELD]; gchar Stop[MAX_BAT][LONG_TEXT_FIELD]; gint Current;};
struct OfflineSetup    {gint NFiles; gboolean Zero[MAX_OFFLINE_FILES]; 
                        gchar ListFName[MAX_OFFLINE_FILES][MAX_FNAME_LENGTH]; 
                        gchar SetFName[MAX_OFFLINE_FILES][MAX_FNAME_LENGTH]; gboolean SaveSpec[MAX_OFFLINE_FILES];
                        gchar SpecFName[MAX_OFFLINE_FILES][MAX_FNAME_LENGTH]; 
                        gint BufSkip[MAX_OFFLINE_FILES]; gchar BufRead[MAX_OFFLINE_FILES][MAX_TEXT_FIELD]; 
                        gint BufRefresh; gfloat Delay; };
struct ReWriteSetup    {gint NFiles; gchar ListFName[MAX_REWRITE_FILES][MAX_FNAME_LENGTH];
                        gint BufSkip[MAX_REWRITE_FILES]; gchar BufRead[MAX_REWRITE_FILES][MAX_TEXT_FIELD];
                        gchar OutFName[MAX_REWRITE_FILES][MAX_FNAME_LENGTH]; gchar OutFormat; 
                        gint NPar; gboolean Select[MAX_TOTAL_PAR]; };
struct ListModeSetup   {gint ListOn; gint Compr; gint Rate; gint NoOfLGates; struct Gate1d LGates[MAX_LGATES];};
struct HardwareSetup   {gint NCrates; 
                            gint CamacMode; gboolean UseGtSup; gint GtSupStn;
                            gint LoParStn[2*MAX_CAMAC_STNS]; gint NParStn[2*MAX_CAMAC_STNS]; gint LoSubStn[2*MAX_CAMAC_STNS];
                        gint Modules[2*MAX_CAMAC_STNS]; struct Properties Properties[2*MAX_CAMAC_STNS];
                        gchar Paras[2*MAX_CAMAC_STNS][MAX_TEXT_FIELD]; gchar SubAddr[2*MAX_CAMAC_STNS][MAX_TEXT_FIELD];
                        gchar ParaNames[2*MAX_CAMAC_STNS][LONG_TEXT_FIELD]; gint ZSupLLD[2*MAX_CAMAC_STNS]; 
                        gint ZSupULD[2*MAX_CAMAC_STNS]; gboolean AutoStopOn; gint AutoStop;};
struct DecodeSetup     {gint Modules; gint Module[2*MAX_CAMAC_STNS]; gint ParNo[2*MAX_CAMAC_STNS][16];};
struct ParameterSetup  {gint NPar; gchar Name[MAX_PAR][MAX_TEXT_FIELD]; gint N[MAX_PAR]; gint A[MAX_PAR]; gint F[MAX_PAR]; 
                        gboolean IgnoreCamac; gint Chan[MAX_TOTAL_PAR]; gint LLD[MAX_PAR]; gint ULD[MAX_PAR]; 
                        gint ExogamID[MAX_PAR]; enum ExoOpt ExoOpt[MAX_PAR];};
struct SpectraSetup    {gint Safety; gint SafetyTime; gboolean NoZero;};
struct OnedSetup       {gint N; gint WSz; gint Par[MAX_1D]; gint NPar[MAX_1D]; gint Chan[MAX_1D]; gint BitShift[MAX_1D];
                        struct SpecGate1 Gate1[MAX_1D]; struct SpecGate2 Gate2[MAX_1D];
                        gint NPeaks[MAX_1D]; gint Peaks[MAX_PEAKS][MAX_1D]; gdouble Fwhm[MAX_1D];
                        gchar Name[MAX_1D][MAX_TEXT_FIELD];};
struct TwodSetup       {gint N; gint WSz; gint XPar[MAX_2D]; gint NXPar[MAX_2D]; gint YPar[MAX_2D]; gint NYPar[MAX_2D];
                        gint XChan[MAX_2D]; gint YChan[MAX_2D];
                        gint XBitShift[MAX_2D]; gint YBitShift[MAX_2D];
                        struct SpecGate1 Gate1[MAX_2D]; struct SpecGate2 Gate2[MAX_2D];
                        gchar Name[MAX_2D][MAX_TEXT_FIELD]; struct CDdet CDdet[MAX_2D]; enum MatrixType MatrixType[MAX_2D];};
struct PseudoSetup     {gint N; gint P1[MAX_PSEUDO]; gint P2[MAX_PSEUDO]; gint Size[MAX_PSEUDO]; 
                        enum PseudoType Type[MAX_PSEUDO]; 
                        gchar Name[MAX_PSEUDO][MAX_TEXT_FIELD];
                        gfloat K1[MAX_PSEUDO]; gfloat K2[MAX_PSEUDO]; gfloat K3[MAX_PSEUDO];
                        gfloat O1[MAX_PSEUDO]; gfloat O2[MAX_PSEUDO]; gfloat O3[MAX_PSEUDO];
                        gfloat Power[MAX_PSEUDO]; gint L1[MAX_PSEUDO]; gint L2[MAX_PSEUDO];
                        gint ArrayN[MAX_PSEUDO]; gint ArrayPar[MAX_ARRAY][MAX_PSEUDO]; 
                        gint ArrayLLD[MAX_ARRAY][MAX_PSEUDO]; 
                        gint ArrayULD[MAX_ARRAY][MAX_PSEUDO]; gfloat ArrayOffset[MAX_ARRAY][MAX_PSEUDO]; 
                        gfloat ArraySlope[MAX_ARRAY][MAX_PSEUDO]; gfloat ArrayQuad[MAX_ARRAY][MAX_PSEUDO];};
struct PsBGated        {gchar Name[MAX_TEXT_FIELD]; struct BGate BGate;};                      //Banana gated pseudo type
struct MacroSetup      {gint N; gint RefreshRate; gchar Name[MAX_MACRO][MAX_TEXT_FIELD+2]; enum MacroType Type[MAX_MACRO]; 
                        gboolean Display[MAX_MACRO]; gint SpecNo[MAX_MACRO]; 
                        gint XMin[MAX_MACRO]; gint XMax[MAX_MACRO]; gint YMin[MAX_MACRO]; gint YMax[MAX_MACRO]; 
                        gfloat K1[MAX_MACRO]; gfloat K2[MAX_MACRO];
                        gint ScalerNo[MAX_MACRO]; gint Index1[MAX_MACRO]; gint Index2[MAX_MACRO];};
struct ScalerSetup     {gint NSc; gint NSc1; gchar Name[MAX_SCALER][MAX_TEXT_FIELD]; gint C[MAX_SCALER]; 
                        gint N[MAX_SCALER]; gint A[MAX_SCALER]; gint F[MAX_SCALER]; gint Master;}; 
struct FileList        {gchar Oned[MAX_1D][MAX_FNAME_LENGTH]; gchar Twod[MAX_2D][MAX_FNAME_LENGTH];};
struct PLogSetup       {gboolean On;gint BufCount;};
struct PrintSetup      {gboolean Yes; gint BufferNo; gint Wds;};
struct Setup           {gchar FName[MAX_FNAME_LENGTH]; gboolean Modified; struct OfflineSetup Offline; 
                        struct ListModeSetup ListMode; struct ReWriteSetup ReWrite;
                        struct HardwareSetup Hardware; struct ParameterSetup Parameter; struct SpectraSetup Spectra; 
                        struct OnedSetup Oned; struct TwodSetup Twod; struct PseudoSetup Pseudo; struct ScalerSetup Scaler;
                        struct FileList Files; struct PLogSetup PLogSetup; struct BatAcqSetup BatAcq;
                        struct MacroSetup Macro; struct DecodeSetup Decode; gboolean Simulator; struct PrintSetup Print;};
struct OnedProperties  {gint XLo; gint XHi; gfloat CountsLo; gfloat CountsHi; gint CountsAuto; gint CountsLog; 
                        gint BoxDrawn; gint BoxX1Ch; gint BoxX2Ch;
                        gint ZoomXLo[ZOOM_LEVELS]; gint ZoomXHi[ZOOM_LEVELS];
                        gboolean OverlapOff;
                        gint OvSpec[MAX_OVERLAP]; gint OvHShift[MAX_OVERLAP]; gint OvVShift[MAX_OVERLAP]; 
                        gboolean Overlap[MAX_OVERLAP]; gboolean ShowPeaks;};        //Settings for 1d windows and 2d projs
struct TwodProperties  {gint XLo; gint XHi; gint YLo; gint YHi; gfloat CountsLo; gfloat CountsHi; 
                        gint CountsAuto; enum TScale TScale; gint BoxDrawn; gint BoxX1Ch; gint BoxX2Ch; gint BoxY1Ch; 
                        gint BoxY2Ch;
                        gint ZoomXLo[ZOOM_LEVELS]; gint ZoomXHi[ZOOM_LEVELS]; 
                        gint ZoomYLo[ZOOM_LEVELS]; gint ZoomYHi[ZOOM_LEVELS];};                   //Setings for 2d windows
struct WinProperties   {struct OnedProperties One; struct TwodProperties Two; gint Open; gint InUse; gint X; gint Y; 
                        gint W; gint H;};                                              //Settings for 1d (& proj) & 2d win
struct BoxType         {gint X1; gint X2; gint Y1; gint Y2; gint X2P; gint Y2P;};              //Box coordinates in pixels
struct FitType         {gdouble P[6*MAX_PEAKS+8]; gdouble Err[6*MAX_PEAKS+8]; enum FitFlags F[6*MAX_PEAKS+8]; 
                        enum TailOpt TailOpt; enum ShapeOpt ShapeOpt; enum StatsOpt StatsOpt; gint NPeaks; gint NPts; 
                        gdouble Data[16384]; gint WinNo; gint Lc; GtkWidget *ScrlW; GtkWidget *Table1; GtkWidget *Table2;
                        enum MouseContext MouseContext; gchar Messg[40]; gboolean Busy;};                   //Data for Fit
struct CalibData       {gint NPts; gint WinNo; gdouble Ch[MAX_CALIB_DATA]; gdouble En[MAX_CALIB_DATA]; gint Terms; 
                        GtkWidget *ScrlW; GtkWidget *Table; GtkWidget *Output;};                        //Data for Calib
struct Calibration     {gdouble P[4]; gchar Units[30];};
struct Bat             {GtkWidget *ScrlW; gint Row; GtkWidget *Table; GtkWidget *Crit[3]; 
                        GtkWidget *Lab[7]; GtkWidget *QEntry[3];};
