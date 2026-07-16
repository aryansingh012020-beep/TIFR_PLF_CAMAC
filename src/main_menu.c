#include <gtk/gtk.h>
#include <stdlib.h>
#include <glib.h>
#include "lamps.h"
//
//Function templates
void StartNormal(GtkWidget *W,gpointer Data); void StopAcquisition(GtkWidget *W,gpointer Data); 
void StartNoZero(GtkWidget *W,gpointer Data); void PauseAcquisition(GtkWidget *W,gpointer Data); 
void ResumeAcquisition(GtkWidget *W,gpointer Data); void DestroyMain(GtkWidget *W,gpointer Data);
void OfflineSetup(GtkWidget *W,gpointer Data); void StopOffline(GtkWidget *W,gpointer Data);
void ReWriteSetup(GtkWidget *W,gpointer Data);
void StopAllOffline(GtkWidget *W,gpointer Data); void PauseOffline(GtkWidget *W,gpointer Data);
void ResumeOffline(GtkWidget *W,gpointer Data); void EditSetup(GtkWidget *W,gpointer Data);
void ReadSpectra(GtkWidget *W,gpointer Unused); void ReadOneByOne(GtkWidget *W,gpointer Unused);
void SaveSpectra(GtkWidget *W,gpointer Unused); void ReadSetup(GtkWidget *W,gpointer Data); 
void SaveSetup(GtkWidget *W,gpointer Data); void SaveAs(GtkWidget *W,gpointer Data);
void ParameterList(GtkWidget *W,gpointer Data); void ListModeSettings(GtkWidget *W,gpointer Data);
void OnedSpecSettings(GtkWidget *W,gpointer Data); void TwodSpecSettings(GtkWidget *W,gpointer Data); 
void PseudoSettings(GtkWidget *W,gpointer Data); void ScalerSettings(GtkWidget *W,gpointer Data);
void ZeroOnedSpectra(GtkWidget *W,gpointer Data); void ZeroTwodSpectra(GtkWidget *W,gpointer Data); 
void ZeroAllSpectra(GtkWidget *W,gpointer Data); void ZeroOned(gint SNo); void ZeroTwod(gint SNo);
void Rose1d(GtkWidget *W,gpointer Data); void Sky1d(GtkWidget *W,gpointer Data);
void Cucumber1d(GtkWidget *W,gpointer Data); void Sandy1d(GtkWidget *W,gpointer Data); 
void Plain1d(GtkWidget *W,gpointer Data); void Daylight2d(GtkWidget *W,gpointer Data); 
void Sunset2d(GtkWidget *W,gpointer Data); void Midnight2d(GtkWidget *W,gpointer Data);
void Plain2d(GtkWidget *W,gpointer Data); void CloseAllSpecWindows(GtkWidget *W,gpointer Data); 
void Tile(GtkWidget *W,gpointer Data); void MiniTile(GtkWidget *W,gpointer Data);
void FeedBack(GtkWidget *W,gpointer Data); void ReadCalib(GtkWidget *W,gpointer Data); 
void SaveCalib(GtkWidget *W,gpointer Data); void EditCalib(GtkWidget *W,gpointer Data); 
void NoCalib(GtkWidget *W,gpointer Data);
void RefreshAll(void); void RefreshAllOned(void); void RefreshAllTwod(void);
void Attention(gint XPos,gchar *Messg); void Chop1d(GtkWidget *W,gpointer Data); void Chop2d(GtkWidget *W,gpointer Data); 
void Comp1d(GtkWidget *W,gpointer Data); void Comp2d(GtkWidget *W,gpointer Data);
void Add1d(GtkWidget *W,gpointer Data); void Add2d(GtkWidget *W,gpointer Data);
void Ascii1d(GtkWidget *W,gpointer Data); void Scalers(GtkWidget *W,gpointer Data);
void Macros(GtkWidget *W,gpointer Data); void Help(GtkWidget *W,gpointer Unused);
void Prefs(GtkWidget *W,gpointer Unused); 
void BatchAcquire(GtkWidget *W,gpointer Data); void BatchTerminate(GtkWidget *W,gpointer Data);
void ViewLog(GtkWidget *W,gpointer Unused);
void LoadDriver(GtkWidget *W,gpointer Unused);
//----------------------------------------------------------------------------------------------------------------------
static gpointer LoadDriverThread(gpointer data)
{ system("pkexec ./ldcmc100"); return NULL; }
void LoadDriver(GtkWidget *W,gpointer Unused)
{ g_print("Loading driver...please wait 5 sec\n"); g_thread_new("load_driver",LoadDriverThread,NULL); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ZMass(GtkWidget *w,gpointer Unused)   //Run zmass as a separate process with lower priority (normal priority has n=0)
{ system("nice -n 10 zmass&"); }
/*----------------------------------------------------------------------------------------------------------------------*/
void Ascii2d(GtkWidget *w,gpointer Unused)                        //Run ascii2d as a separate process with lower priority
{ system("nice -n 10 ascii2d&"); }
/*----------------------------------------------------------------------------------------------------------------------*/
void MkWord(GtkWidget *W,gpointer Unused)                        //Run mkword as a separate process: creates lampslog.rtf
{ system("nice -n 10 mkword&"); }
/*----------------------------------------------------------------------------------------------------------------------*/
void CamacTest(GtkWidget *W,gpointer Unused)
{ system("nice -n 10 cmc100test&"); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ZeroOnedSpectra(GtkWidget *W,gpointer Data)
{ ZeroOned(-1); RefreshAllOned(); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ZeroTwodSpectra(GtkWidget *W,gpointer Data)
{ ZeroTwod(-1); RefreshAllTwod(); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ZeroAllSpectra(GtkWidget *W,gpointer Data)
{ ZeroOned(-1); ZeroTwod(-1); RefreshAll(); }
/*----------------------------------------------------------------------------------------------------------------------*/
GtkItemFactoryEntry menu_items[]={
{ "/_Acquisition",NULL,NULL,0,"<Branch>" },
{ "/Acquisition/Start Acquisition",NULL,StartNormal,0,NULL },
{ "/Acquisition/Stop Acquisition",NULL,StopAcquisition,0,NULL },
{ "/Acquisition/sep1",NULL,NULL,0,"<Separator>" },
{ "/Acquisition/Start (No Zero)",NULL,StartNoZero,0,NULL },
{ "/Acquisition/sep1",NULL,NULL,0,"<Separator>" },
{ "/Acquisition/Pause",NULL,PauseAcquisition,0,NULL },
{ "/Acquisition/Resume",NULL,ResumeAcquisition,0,NULL },
{ "/Acquisition/sep1",NULL,NULL,0,"<Separator>" },
{"/Acquisition/Batch",NULL,BatchAcquire,0,NULL },
{"/Acquisition/Terminate Batch",NULL,BatchTerminate,0,NULL },
{ "/Acquisition/sep1",NULL,NULL,0,"<Separator>" },
{ "/Acquisition/LOAD DRIVER",NULL,LoadDriver,0,NULL },
{ "/Acquisition/sep1",NULL,NULL,0,"<Separator>" },
{ "/Acquisition/Quit",NULL,DestroyMain,0,NULL },

{ "/_Analysis",NULL,NULL,0,"<Branch>" },
{ "/Analysis/Start",NULL,OfflineSetup,0,NULL },
{ "/Analysis/Stop",NULL,StopOffline,0,NULL },
{ "/Analysis/Stop All",NULL,StopAllOffline,0,NULL },
{ "/Analysis/sep1",NULL,NULL,0,"<Separator>" },
{ "/Analysis/Pause",NULL,PauseOffline,0,NULL },
{ "/Analysis/Resume",NULL,ResumeOffline,0,NULL },

{ "/_Setup",NULL,NULL,0,"<Branch>" },
{ "/Setup/Read Setup",NULL,ReadSetup,0,NULL },
{ "/Setup/Save Setup",NULL,SaveSetup,0,NULL },
{ "/Setup/Save As...",NULL,SaveAs,0,NULL },
{ "/Setup/sep1",NULL,NULL,0,"<Separator>" },
{ "/Setup/Edit Setup",NULL,EditSetup,0,NULL },
{ "/Setup/View Setup",NULL,NULL,0,"<Branch>" },
{ "/Setup/View Setup/Parameter List",NULL,ParameterList,0,NULL },
{ "/Setup/View Setup/List Mode Settings",NULL,ListModeSettings,0,NULL },
{ "/Setup/View Setup/1d Spectra Settings",NULL,OnedSpecSettings,0,NULL },
{ "/Setup/View Setup/2d Spectra Settings",NULL,TwodSpecSettings,0,NULL },
{ "/Setup/View Setup/Pseudo Parameters",NULL,PseudoSettings,0,NULL },
{ "/Setup/View Setup/Scaler Settings",NULL,ScalerSettings,0,NULL },
{ "/Setup/sep1",NULL,NULL,0,"<Separator>" },
{ "/Setup/Preferences",NULL,Prefs,0,NULL },

{ "/_Display",NULL,NULL,0,"<Branch>" },
{ "/Display/Refresh all",NULL,RefreshAll,0,NULL },
{ "/Display/Tile",NULL,Tile,0,NULL },
{ "/Display/Mini Tile",NULL,MiniTile,0,NULL },
{ "/Display/Close All",NULL,CloseAllSpecWindows,0,NULL },

{ "/_Spectra",NULL,NULL,0,"<Branch>" },
{ "/Spectra/Read Spectra",NULL,NULL,0,"<Branch>" },
{ "/Spectra/Read Spectra/By Run Name",NULL,ReadSpectra,0,NULL },
{ "/Spectra/Read Spectra/One by one",NULL,ReadOneByOne,0,NULL },

{ "/Spectra/Save Spectra",NULL,SaveSpectra,0,NULL },

{ "/Spectra/sep1",NULL,NULL,0,"<Separator>" },
{ "/Spectra/Zero Spectra",NULL,NULL,0,"<Branch>" },
{ "/Spectra/Zero Spectra/Zero all 1d spectra",NULL,ZeroOnedSpectra,0,NULL },
{ "/Spectra/Zero Spectra/Zero all 2d spectra",NULL,ZeroTwodSpectra,0,NULL },
{ "/Spectra/Zero Spectra/Zero all 1d and 2d",NULL,ZeroAllSpectra,0,NULL },

{ "/_Calibration",NULL,NULL,0,"<Branch>" },
{ "/Calibration/Read Calibration",NULL,ReadCalib,0,NULL },
{ "/Calibration/Save Calibration",NULL,SaveCalib,0,NULL },
{ "/Calibration/Edit Calibration",NULL,EditCalib,0,NULL },
{ "/Calibration/Calib as raw ADC",NULL,NoCalib,0,NULL },

{ "/_Themes",NULL,NULL,0,"<Branch>" },
{ "/Themes/1d Themes",NULL,NULL,0,"<Branch>" },
{ "/Themes/1d Themes/Rose",NULL,Rose1d,0,NULL },
{ "/Themes/1d Themes/Sky",NULL,Sky1d,0,NULL },
{ "/Themes/1d Themes/Cucumber",NULL,Cucumber1d,0,NULL },
{ "/Themes/1d Themes/Sandy",NULL,Sandy1d,0,NULL },
{ "/Themes/1d Themes/Plain",NULL,Plain1d,0,NULL },
{ "/Themes/2d Themes",NULL,NULL,0,"<Branch>" },
{ "/Themes/2d Themes/Daylight",NULL,Daylight2d,0,NULL },
{ "/Themes/2d Themes/Sunset",NULL,Sunset2d,0,NULL },
{ "/Themes/2d Themes/Midnight",NULL,Midnight2d,0,NULL },
{ "/Themes/2d Themes/Plain",NULL,Plain2d,0,NULL },

{ "/_Log File",NULL,NULL,0,"<Branch>" },
{ "/Log File/View, Edit",NULL,ViewLog,0,NULL },
{ "/Log File/Make Word File",NULL,MkWord,0,NULL },

{ "/_Test",NULL,CamacTest,0,"<Item>" },

{"/_Utilities",NULL,NULL,0,"<Branch>"},
{ "/Utilities/Add 1d Spectra",NULL,Add1d,0,NULL },
{ "/Utilities/Chop 1d Spectrum",NULL,Chop1d,0,NULL },
{ "/Utilities/Compress 1d Spec",NULL,Comp1d,0,NULL },
{ "/Utilities/1d Spec to ASCII",NULL,Ascii1d,0,NULL },
{ "/Utilities/sep1",NULL,NULL,0,"<Separator>" },
{ "/Utilities/Add 2d Spectra",NULL,Add2d,0,NULL },
{ "/Utilities/Chop 2d Spectrum",NULL,Chop2d,0,NULL },
{ "/Utilities/Compress 2d Spec",NULL,Comp2d,0,NULL },
{ "/Utilities/2d Spec to ASCII",NULL,Ascii2d,0,NULL },
{ "/Utilities/Gain Match, PI",NULL,ZMass,0,NULL },
{ "/Utilities/sep1",NULL,NULL,0,"<Separator>" },
{ "/Utilities/ReWrite List File",NULL,ReWriteSetup,0,NULL },

{ "/_Feedback",NULL,FeedBack,0,NULL },

{ "/_Scalers",NULL,Scalers,0,NULL },

{ "/_Macros",NULL,Macros,0,NULL },

{ "/_Help",NULL,Help,0,NULL},
};
/*-----------------------------------------------------------------------------------------------------------------*/
void GetMainMenu(GtkWidget*window,GtkWidget **menubar)
{
GtkItemFactory *item_factory;
GtkAccelGroup *accel_group;
gint nmenu_items=sizeof(menu_items)/sizeof(menu_items[0]);

accel_group=gtk_accel_group_new();
item_factory=gtk_item_factory_new(GTK_TYPE_MENU_BAR,"<main>",accel_group);
gtk_item_factory_create_items(item_factory,nmenu_items,menu_items,NULL);
gtk_window_add_accel_group(GTK_WINDOW(window),accel_group);
if (menubar) *menubar=gtk_item_factory_get_widget(item_factory,"<main>");
}
/*-----------------------------------------------------------------------------------------------------------------*/
