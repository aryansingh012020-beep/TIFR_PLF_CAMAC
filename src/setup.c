#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lamps.h"

/*Function templates*/
void Attention(gint XPos,gchar *Messg); void FileOkSel(GtkWidget *W,GtkFileSelection *Fs); void Err(gchar *Messg);
void SetStyleRecursively(GtkWidget *W,gpointer Data); 
void FNameDialog(gint Action); void CheckSetup(void);
void CloseAllSpecWindows(GtkWidget *W,gpointer Data);
void MacroSetup(GtkWidget *VBox);
void ClearCrate(gint CrateNo,GtkWidget *Table);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void ListOfAdcModes(gint StnIndex);
void SavePrefs(void);
/* Global variables */
extern enum ProgramState ProgramState;
extern struct Setup Setup;
extern struct PsBGated PsBGated[MAX_PSEUDO];                                                   //Banana gated pseudo type
extern struct FileSelectType *FileX;
extern gchar BanDir[MAX_DIR_STRLEN];                                                                     //Directory prefs
/* Global within this file only*/
GtkWidget *EdWin;                    //The main edit window. The modal windows have to made transient with respect to this
gint GlobalIntRow,PsNo;                //Row number in HardwareSetup's ModuleChanged function, PseudoNo in Array, PsBGated
gint SpecNo;                                                          //Needed by OnedGates1d,TwoGates and their callbacks
GtkWidget *GlobalW;                                                                        //General purpose global widget
GtkWidget **WArray;                                                  //This array of widgets is used in CAMAC MODULE setup
       //WArray[0] is Module, WArray[1] is LAM, WArray[2] is Mode, WArray[3] is Gain,  WArray[4] is LLD, WArray[5] is Func
                                     //It is re-used in Define 1d Spectra and also inDefine 2d Spectra for Global Shortcut
GtkWidget **ScalerWidget;                                                                          //Used in ScalerSetup()
struct BanBr {gint Row; GtkWidget *Table; GtkWidget *ScrlW;};
struct BanBr *BanBr;
GtkWidget **CrateScrollArray;
/*----------------------------------------------------------------------------------------------------------------------*/
void Err(gchar *Messg)
{
gchar Str[80];

sprintf(Str,"Setup File Error: %s",Messg); Attention(0,Str);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void EdWinClosed(GtkWidget *W,gpointer Data)                                                          /*Close Edit Setup*/
{
if (ProgramState !=DoingSetup) { Attention(0,"Invalid operation"); return; }
gtk_widget_destroy(GTK_WIDGET(EdWin));
g_free(CrateScrollArray);
if (!Setup.Parameter.IgnoreCamac) g_free(ScalerWidget);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void EdWinDestroyed(GtkWidget *W,gpointer Data)
{
if (ProgramState !=DoingSetup) { Attention(0,"Invalid operation"); return; }
CheckSetup(); ProgramState=Free;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ToggleListOn(GtkWidget *CheckButton,gpointer Data)             //Call back from ListModeSetup that turns ListOn = 0,1
{
if (GTK_TOGGLE_BUTTON(CheckButton)->active) 
   { Setup.ListMode.ListOn=1; gtk_label_set_text(GTK_LABEL(GTK_BIN(CheckButton)->child),"List Mode On "); }
else
   { Setup.ListMode.ListOn=0; gtk_label_set_text(GTK_LABEL(GTK_BIN(CheckButton)->child),"List Mode Off"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CompressionCallback(GtkWidget *W,gpointer Data) 
                                            //Callback from ListMode Setup. Compr =0:Candle 1:Normal 2:Advanced 3:Exogam
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Normal")) Setup.ListMode.Compr=1;
else if (!strcmp(Str,"Advanced")) Setup.ListMode.Compr=2;
else if (!strcmp(Str,"Candle")) Setup.ListMode.Compr=0;                                                  //Candle format
else if (!strcmp(Str,"Exogam")) Setup.ListMode.Compr=3;                                                    //Exogam format
else Setup.ListMode.Compr=1;         //Compr=Normal if user types junk, neddless precaution: gtk_entry_set_editable() used
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DataRateCallBack(GtkWidget *W,GtkSpinButton *Spin)                                       //Call back from ListModeSetup
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,">10K")) Setup.ListMode.Rate=0;
else if (!strcmp(Str,"2K-10K")) Setup.ListMode.Rate=1;
else if (!strcmp(Str,"500Hz-2K")) Setup.ListMode.Rate=2;
else if (!strcmp(Str,"10Hz-500Hz")) Setup.ListMode.Rate=3;
else Setup.ListMode.Rate=4; 
}
/*----------------------------------------------------------------------------------------------------------------------*/
void WinClosed(GtkWidget *W,GtkWidget *Win)
{ gtk_grab_remove(Win); gtk_widget_destroy(Win); }
/*----------------------------------------------------------------------------------------------------------------------*/
void LGatesParaChanged(GtkWidget *W,gpointer Data)
{ Setup.ListMode.LGates[GPOINTER_TO_INT(Data)].Para=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void LGatesLoLimChanged(GtkWidget *W,gpointer Data)
{ Setup.ListMode.LGates[GPOINTER_TO_INT(Data)].Lo=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void LGatesHiLimChanged(GtkWidget *W,gpointer Data)
{ Setup.ListMode.LGates[GPOINTER_TO_INT(Data)].Hi=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void DefineListGates(GtkWidget *W,gpointer Data)                                             //Callback from ListModeSetup
{
static gchar Heading[4][25] = {"No","Para No.","Lo Limit","Hi Limit"};
gint ColWidth[4]={32,60,60,60};
GtkWidget *Win,*VBox,*HeadBut,*HBox,*But,*Table,*NumBut,*ParaEntry,*LoLimEntry,*HiLimEntry,*ScrollW;
gint i;
gchar Str[40];
static GdkColor HeadingBg  = {0,0x0000,0x0000,0x0000};                                           //Colour for HeadingStyle
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};                                           //Colour for HeadingStyle
GtkStyle *HeadingStyle;

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                        //Make the window modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                       //Ensure visibility of modal window
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Define Gates"); gtk_widget_set_uposition(GTK_WIDGET(Win),275,51);
gtk_widget_set_size_request(Win,252,225); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                       //VBox for the entire window

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_usize(GTK_WIDGET(ScrollW),200,26);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(1,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);
for (i=0;i<4;i++)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle); 
    gtk_widget_set_size_request(HeadBut,ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(ScrollW,200,160);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(Setup.ListMode.NoOfLGates,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);
for (i=0;i<Setup.ListMode.NoOfLGates;i++)
    {
    sprintf(Str,"%d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(NumBut,ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),NumBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ParaEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.ListMode.LGates[i].Para);
    gtk_entry_set_text(GTK_ENTRY(ParaEntry),Str);
    gtk_signal_connect(GTK_OBJECT(ParaEntry),"changed",GTK_SIGNAL_FUNC(LGatesParaChanged),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(ParaEntry,ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(Table),ParaEntry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    LoLimEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.ListMode.LGates[i].Lo);
    gtk_entry_set_text(GTK_ENTRY(LoLimEntry),Str);
    gtk_signal_connect(GTK_OBJECT(LoLimEntry),"changed",GTK_SIGNAL_FUNC(LGatesLoLimChanged),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(LoLimEntry,ColWidth[2],-1);
    gtk_table_attach(GTK_TABLE(Table),LoLimEntry,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    HiLimEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.ListMode.LGates[i].Hi);
    gtk_entry_set_text(GTK_ENTRY(HiLimEntry),Str);
    gtk_signal_connect(GTK_OBJECT(HiLimEntry),"changed",GTK_SIGNAL_FUNC(LGatesHiLimChanged),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(HiLimEntry,ColWidth[3],-1);
    gtk_table_attach(GTK_TABLE(Table),HiLimEntry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);   
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);           
gtk_widget_show_all(Win);                                                              
gtk_style_unref(HeadingStyle);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void NoOfLGates(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.ListMode.NoOfLGates=gtk_spin_button_get_value_as_int(Spin); }
/*--------------------------------------------------------------------------------------------------------------------*/
void ListModeSetup(GtkWidget *VBox)
{
GtkWidget *VBox0,*Frame,*HBox,*ComprCombo,*HBox1,*HBox2,*HBox3,*HBoxA,*HBoxB,*CheckButton,*Label,
          *GatesBut,*LGatesSpin,*DataRateCombo;
GtkAdjustment *Adj;
GList *GList;
gchar Str[40];
static GdkColor FrameBg  = {0,0x7777,0x7777,0x7777};
static GdkColor FrameFg  = {0,0xDDDD,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;

FrameStyle=gtk_style_copy(gtk_widget_get_default_style());                              //Copy default style to this style
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }              //Set colours

Frame=gtk_frame_new("LIST MODE SETTINGS"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.5);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox0=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Frame),VBox0);
gtk_container_set_border_width(GTK_CONTAINER(VBox0),5);

HBox1=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox0),HBox1);
HBox2=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox2,TRUE,FALSE,0);
HBox3=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox3,TRUE,FALSE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox2),HBox,FALSE,FALSE,0);
if (Setup.ListMode.ListOn) strcpy(Str,"List Mode On "); else strcpy(Str,"List Mode Off");
CheckButton=gtk_check_button_new_with_label(Str); gtk_box_pack_start(GTK_BOX(HBox),CheckButton,TRUE,FALSE,0);
if (Setup.ListMode.ListOn) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckButton),TRUE);
                      else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckButton),FALSE);
gtk_signal_connect(GTK_OBJECT(CheckButton),"toggled",GTK_SIGNAL_FUNC(ToggleListOn),NULL);

Frame=gtk_frame_new("Compression"); gtk_box_pack_start(GTK_BOX(HBox3),Frame,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Frame),HBox);
ComprCombo=gtk_combo_new(); gtk_widget_set_size_request(ComprCombo,100,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(ComprCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),ComprCombo,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Normal"); GList=g_list_append(GList,"Advanced"); GList=g_list_append(GList,"Candle");
GList=g_list_append(GList,"Exogam");                                                                     //4 popup choices
gtk_combo_set_popdown_strings(GTK_COMBO(ComprCombo),GList);                                             //Define the popup
switch (Setup.ListMode.Compr)                                  //Set the initial entry depending upon Setup.ListMode.Compr
   {
   case 0:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ComprCombo)->entry),"Candle"); break;
   case 1:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ComprCombo)->entry),"Normal"); break;
   case 2:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ComprCombo)->entry),"Advanced"); break;
   case 3:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ComprCombo)->entry),"Exogam"); break;
   default: gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ComprCombo)->entry),"Normal");
   }
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(ComprCombo)->entry),"changed",GTK_SIGNAL_FUNC(CompressionCallback),NULL);

HBox1=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox0),HBox1);
HBox2=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox2,TRUE,FALSE,0);
HBox3=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox3,TRUE,FALSE,0);

Frame=gtk_frame_new("Event rate"); gtk_box_pack_start(GTK_BOX(HBox2),Frame,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Frame),HBox);
DataRateCombo=gtk_combo_new(); gtk_widget_set_size_request(DataRateCombo,100,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(DataRateCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),DataRateCombo,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,">10K"); GList=g_list_append(GList,"2K-10K"); GList=g_list_append(GList,"500Hz-2K");
GList=g_list_append(GList,"10Hz-500Hz"); GList=g_list_append(GList,"<10Hz");
gtk_combo_set_popdown_strings(GTK_COMBO(DataRateCombo),GList);
switch (Setup.ListMode.Rate)
   {
   case 0:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(DataRateCombo)->entry),">10K"); break;
   case 1:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(DataRateCombo)->entry),"2K-10K"); break;
   case 2:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(DataRateCombo)->entry),"500Hz-2K"); break;
   case 3:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(DataRateCombo)->entry),"10Hz-500Hz"); break;
   default: gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(DataRateCombo)->entry),"<10Hz");
   }
g_signal_connect(GTK_COMBO(DataRateCombo)->entry,"changed",G_CALLBACK(DataRateCallBack),NULL);

Frame=gtk_frame_new("Gated List Mode"); gtk_box_pack_start(GTK_BOX(HBox3),Frame,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,8); gtk_container_add(GTK_CONTAINER(Frame),HBox);
HBoxA=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),HBoxA,FALSE,FALSE,0);
HBoxB=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),HBoxB,FALSE,FALSE,0);

Label=gtk_label_new("Gates:"); gtk_box_pack_start(GTK_BOX(HBoxB),Label,FALSE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.ListMode.NoOfLGates,0.0,MAX_LGATES,1.0,5.0,0.0));   //NoOfLGates spin
LGatesSpin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(LGatesSpin),TRUE);
gtk_box_pack_start(GTK_BOX(HBoxB),LGatesSpin,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(NoOfLGates),LGatesSpin);
GatesBut=gtk_button_new_with_label("Define\nGates"); gtk_box_pack_start(GTK_BOX(HBoxB),GatesBut,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(GatesBut),"clicked",GTK_SIGNAL_FUNC(DefineListGates),NULL);

g_list_free(GList);
gtk_style_unref(FrameStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ModuleWinClosed(GtkWidget *W,gpointer Data)
{
gtk_grab_remove(GTK_WIDGET(((W->parent)->parent)->parent));                                 //Take away the modal property
gtk_widget_destroy(GTK_WIDGET(((W->parent)->parent)->parent));                                            //Destroy window
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ModuleWinDestroy(GtkWidget *W,GtkWidget *Module)
{
SetStyleRecursively(Module,gtk_widget_get_default_style());                       //Restore default colours for the module
g_free(WArray);                                                         //Important to free the memory allocated to WArray
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ModuleNameCallBack(GtkWidget *W,GtkWidget *Module)                              //Callback for Module Names Combo Box
{
static gchar ModuleNames[13][25]=                                                                        //13 Module types
       { "Empty","Ortec 413 ADC","Ortec 811 ADC","LeCroy ADC/QDC","BARC CM60","Phillips 71xx",
         "CAEN TDC","Silena 4418/Q","BiRa Bit Register","BARC CM88","Sync. Scaler","Unknown","LeCroy MTDC"};
gchar Str1[MAX_TEXT_FIELD];
const gchar *Str;
gint Row,j;
                                                                                                                             
Row=GlobalIntRow;
Str=gtk_entry_get_text(GTK_ENTRY(W));
for (j=0;j<13;++j) if (!strcmp(Str,ModuleNames[j])) { Setup.Hardware.Modules[Row]=j; break; }
gtk_label_set_text(GTK_LABEL(GTK_BIN(Module)->child),ModuleNames[j]);
switch (Setup.Hardware.Modules[Row])
   {
   case 0: Setup.Hardware.Properties[Row].AdcGain=8192;                                                    //Empty module
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 1: Setup.Hardware.Properties[Row].AdcGain=8192;                                                //Ortec AD413A ADC
           Setup.Hardware.Properties[Row].AdcMode=Coincidence;                                //Default mode: Coincidence
           Setup.Hardware.Properties[Row].AdcLLD=50;
           Setup.Hardware.Properties[Row].AdcFCode=2;
           break;
   case 2: Setup.Hardware.Properties[Row].AdcGain=4096;                   //Ortec AD811 ADC (Gain=4K because of overflows)
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 3: Setup.Hardware.Properties[Row].AdcGain=2048;                                                  //LeCroy ADC/QDC
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 4: Setup.Hardware.Properties[Row].AdcGain=4096;                                                   //BARC CM60 ADC
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=50;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 5: Setup.Hardware.Properties[Row].AdcGain=4096;                                                   //Phillips 71xx
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 6: Setup.Hardware.Properties[Row].AdcGain=4096;                                                        //CAEN TDC
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 7: Setup.Hardware.Properties[Row].AdcGain=4096;                                                   //Silena 4418/Q
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=50;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 8: Setup.Hardware.Properties[Row].AdcGain=4986;                                                //BiRa Bit Register
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=2;
           break;
   case 9: Setup.Hardware.Properties[Row].AdcGain=8192;                                                   //BARC CM88 ADC
           Setup.Hardware.Properties[Row].AdcMode=Coincidence;       //default mode: Coincidence without individual gates
           Setup.Hardware.Properties[Row].AdcLLD=50;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 10:Setup.Hardware.Properties[Row].AdcGain=65536;                                                   //Sync. Scaler
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 11:Setup.Hardware.Properties[Row].AdcGain=4096;                                                        //Unknown
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   case 12:Setup.Hardware.Properties[Row].AdcGain=65536;                                                   //LeCroy MTDC
           Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
           Setup.Hardware.Properties[Row].AdcLLD=0;
           Setup.Hardware.Properties[Row].AdcFCode=0;
           break;
   }
ListOfAdcModes(Row);
if (Setup.Hardware.Properties[Row].AdcGain>512) sprintf(Str1,"%d K",Setup.Hardware.Properties[Row].AdcGain/1024);
else                                            sprintf(Str1,"%d",Setup.Hardware.Properties[Row].AdcGain);
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),Str1);
sprintf(Str1,"%d",Setup.Hardware.Properties[Row].AdcLLD); gtk_entry_set_text(GTK_ENTRY(WArray[4]),Str1);
sprintf(Str1,"%d",Setup.Hardware.Properties[Row].AdcFCode); gtk_entry_set_text(GTK_ENTRY(WArray[5]),Str1);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LAMOptsCallBack(GtkWidget *W,gpointer i)                                            //Callback for LAMOpts Combo Box
{
static gchar LAMOpts[2][12]= {"Disabled","Enabled"};
const gchar *Str;
gint Row,j;

Row=GPOINTER_TO_INT(i);
Str=gtk_entry_get_text(GTK_ENTRY(W));
for (j=0;j<2;j++) if (!strcmp(Str,LAMOpts[j])) { Setup.Hardware.Properties[Row].AdcLam=j; break; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ModeOptsCallBack(GtkWidget *W,gpointer i)                                          //Callback for ModeOpts Combo Box
{
const gchar *Str;
gint Row;

Row=GPOINTER_TO_INT(i);
Str=gtk_entry_get_text(GTK_ENTRY(W));
if (Setup.Hardware.Modules[Row] == 1)  //Ortec 413 ADC
   {
   if (!strcmp(Str,"Singles")) Setup.Hardware.Properties[Row].AdcMode=Singles; 
   else                        Setup.Hardware.Properties[Row].AdcMode=Coincidence;
   }
if (Setup.Hardware.Modules[Row] == 9)  //BARC CM88 ADC
   {
   if (!strcmp(Str,"Coincidence"))     Setup.Hardware.Properties[Row].AdcMode=Coincidence;
   else if (!strcmp(Str,"Coin+Gates")) Setup.Hardware.Properties[Row].AdcMode=CoinGate;
   else if (!strcmp(Str,"Singles"))    Setup.Hardware.Properties[Row].AdcMode=Singles;
   else                                Setup.Hardware.Properties[Row].AdcMode=SingGate;
   }
if (Setup.Hardware.Modules[Row] == 5)  //Phillips 71xx
   {
   if (!strcmp(Str,"Test")) Setup.Hardware.Properties[Row].AdcMode=Test; 
   else                     Setup.Hardware.Properties[Row].AdcMode=NotRelevant;
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LLDCallBack(GtkWidget *W,gpointer i)                                                      //Callback for LLD TextBox
{
gint Row;

Row=GPOINTER_TO_INT(i);
Setup.Hardware.Properties[Row].AdcLLD=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void FCodeCallBack(GtkWidget *W,gpointer i)                                                   //Callback for FCode TextBox
{
gint Row;

Row=GPOINTER_TO_INT(i);
Setup.Hardware.Properties[Row].AdcFCode=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void GainCallBack(GtkWidget *W,gpointer i)                                                 //Callback for AdcGain TextBox
{
gint Row;
const gchar *Str;

Row=GPOINTER_TO_INT(i);
Str=gtk_entry_get_text(GTK_ENTRY(W));
if (Str[strlen(Str)-1]=='K') Setup.Hardware.Properties[Row].AdcGain=atoi(Str)*1024; 
else                         Setup.Hardware.Properties[Row].AdcGain=atoi(Str);
}
//------------------------------------------------------------------------------------------------------------------------
void SetSilenaOffset(GtkWidget *W,gpointer Index)
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Hardware.Properties[GPOINTER_TO_INT(Index)/8].SilenaOffset[GPOINTER_TO_INT(Index)%8]=CLAMP(atoi(Str),0,255);
}
//------------------------------------------------------------------------------------------------------------------------
void SetLowerThreshold(GtkWidget *W,gpointer Index)
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Hardware.Properties[GPOINTER_TO_INT(Index)/16].LowerThreshold[GPOINTER_TO_INT(Index)%16]=CLAMP(atoi(Str),0,4095);
}
//------------------------------------------------------------------------------------------------------------------------
void SetUpperThreshold(GtkWidget *W,gpointer Index)
{
const gchar *Str;

Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Hardware.Properties[GPOINTER_TO_INT(Index)/16].UpperThreshold[GPOINTER_TO_INT(Index)%16]=CLAMP(atoi(Str),0,4095);
}
//------------------------------------------------------------------------------------------------------------------------
void SpecialProperties(GtkWidget *W,gpointer i)                                //Used for Silena 4418/Q and Phillips 71xx
{
gint Row,j;
GtkWidget *Win,*Label,*But,*HBox,*VBox,*VBox2,*Entry;
gchar Str[40];
static GdkColor HeadingFg  = {0,0xAAAA,0x0000,0x0000};
GtkStyle *HeadingStyle;

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());                            //Copy default style to this style
for (j=0;j<5;++j) { HeadingStyle->fg[j]=HeadingStyle->text[j]=HeadingFg; }                                   //Set colours

Row=GPOINTER_TO_INT(i);
if (Setup.Hardware.Modules[Row] == 7)                                                                   //Silena 4418/Q
   {
   Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
   gtk_widget_set_size_request(Win,400,360);
   gtk_window_set_title(GTK_WINDOW(Win),"Silena Special Properties");
   gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
   gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
   
   VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);
   
   VBox2=gtk_vbox_new(TRUE,0);gtk_box_pack_start(GTK_BOX(VBox),VBox2,TRUE,FALSE,0);
   Label=gtk_label_new("Silena QDC offsets and thresholds"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Offsets range is 0-255"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
   Label=gtk_label_new("0 for -3%, 127 for 0%, 255 for +3%"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Default is 127 for 0%"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Thresholds are used only in Q Stop Block Mode"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Threshold values must be in the range 0 to 4095"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);

   VBox2=gtk_vbox_new(FALSE,0);gtk_box_pack_start(GTK_BOX(VBox),VBox2,TRUE,FALSE,0);
   HBox=gtk_hbox_new(FALSE,0);gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
   Label=gtk_label_new("No"); gtk_widget_set_size_request(GTK_WIDGET(Label),30,-1);
   SetStyleRecursively(Label,HeadingStyle); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Offset"); gtk_widget_set_size_request(GTK_WIDGET(Label),70,-1);
   SetStyleRecursively(Label,HeadingStyle); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Lower\nThreshold"); gtk_widget_set_size_request(GTK_WIDGET(Label),70,-1);
   SetStyleRecursively(Label,HeadingStyle); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
   Label=gtk_label_new("Upper\nThreshold"); gtk_widget_set_size_request(GTK_WIDGET(Label),70,-1);
   SetStyleRecursively(Label,HeadingStyle); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
   for (j=0;j<8;++j)
       {
       HBox=gtk_hbox_new(FALSE,0);gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
       sprintf(Str,"%d:",j); Label=gtk_label_new(Str); gtk_widget_set_size_request(GTK_WIDGET(Label),30,-1);
       gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       Entry=gtk_entry_new_with_max_length(3); sprintf(Str,"%d",Setup.Hardware.Properties[Row].SilenaOffset[j]); 
       gtk_entry_set_text(GTK_ENTRY(Entry),Str); gtk_widget_set_size_request(GTK_WIDGET(Entry),70,-1);
       gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,5);
       gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SetSilenaOffset),GINT_TO_POINTER(Row*8+j));
       Entry=gtk_entry_new_with_max_length(4); sprintf(Str,"%d",Setup.Hardware.Properties[Row].LowerThreshold[j]);
       gtk_entry_set_text(GTK_ENTRY(Entry),Str); gtk_widget_set_size_request(GTK_WIDGET(Entry),70,-1);
       gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,5);
       gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SetLowerThreshold),GINT_TO_POINTER(Row*16+j));
       Entry=gtk_entry_new_with_max_length(4); sprintf(Str,"%d",Setup.Hardware.Properties[Row].UpperThreshold[j]);
       gtk_entry_set_text(GTK_ENTRY(Entry),Str); gtk_widget_set_size_request(GTK_WIDGET(Entry),70,-1);
       gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,5);
       gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SetUpperThreshold),GINT_TO_POINTER(Row*16+j));
       }

   HBox=gtk_hbox_new(TRUE,5);gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
   But=gtk_button_new_with_label("Ok"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
   gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

   gtk_widget_show_all(Win);
   }
if (Setup.Hardware.Modules[Row] == 5)                                                                 //Phillips 71xx
   {
   Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
   gtk_widget_set_size_request(Win,400,520);
   gtk_window_set_title(GTK_WINDOW(Win),"Phillips Special Properties");
   gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
   gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

   VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);

   VBox2=gtk_vbox_new(TRUE,0);gtk_box_pack_start(GTK_BOX(VBox),VBox2,TRUE,FALSE,0);
   Label=gtk_label_new("Phillips 71xx Lower and Upper Thresholds"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
   Label=gtk_label_new("These thresholds are used only in Q Stop Block Mode"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Threshold values must be in the range 0 to 4095"); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);

   VBox2=gtk_vbox_new(FALSE,0);gtk_box_pack_start(GTK_BOX(VBox),VBox2,TRUE,FALSE,0);
   HBox=gtk_hbox_new(FALSE,0);gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
   Label=gtk_label_new("No"); gtk_widget_set_size_request(GTK_WIDGET(Label),30,-1);
   SetStyleRecursively(Label,HeadingStyle); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Lower\nThreshold"); gtk_widget_set_size_request(GTK_WIDGET(Label),70,-1);
   SetStyleRecursively(Label,HeadingStyle); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
   Label=gtk_label_new("Upper\nThreshold"); gtk_widget_set_size_request(GTK_WIDGET(Label),70,-1);
   SetStyleRecursively(Label,HeadingStyle); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,20);
   for (j=0;j<16;++j)
       {
       HBox=gtk_hbox_new(FALSE,0);gtk_box_pack_start(GTK_BOX(VBox2),HBox,TRUE,FALSE,0);
       sprintf(Str,"%d:",j); Label=gtk_label_new(Str); gtk_widget_set_size_request(GTK_WIDGET(Label),30,-1);
       gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       Entry=gtk_entry_new_with_max_length(4); sprintf(Str,"%d",Setup.Hardware.Properties[Row].LowerThreshold[j]);
       gtk_entry_set_text(GTK_ENTRY(Entry),Str); gtk_widget_set_size_request(GTK_WIDGET(Entry),70,-1);
       gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,0);
       gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SetLowerThreshold),GINT_TO_POINTER(Row*16+j));
       Entry=gtk_entry_new_with_max_length(4); sprintf(Str,"%d",Setup.Hardware.Properties[Row].UpperThreshold[j]);
       gtk_entry_set_text(GTK_ENTRY(Entry),Str); gtk_widget_set_size_request(GTK_WIDGET(Entry),70,-1);
       gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,20);
       gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SetUpperThreshold),GINT_TO_POINTER(Row*16+j));
       }

   HBox=gtk_hbox_new(TRUE,5);gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
   But=gtk_button_new_with_label("Ok"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
   gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

   gtk_widget_show_all(Win);
   }
gtk_style_unref(HeadingStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ListOfAdcModes(gint StnIndex)                                                  //Sets the popup strings for ADC Modes
{
GList *GList;

GList=NULL;
switch (Setup.Hardware.Modules[StnIndex])
   {
   case 1:  //Ortec 413 ADC
            GList=g_list_append(GList,"Coincidence");
            GList=g_list_append(GList,"Singles");
            gtk_combo_set_popdown_strings(GTK_COMBO(WArray[2]),GList);
            if (Setup.Hardware.Properties[StnIndex].AdcMode==Coincidence)
                 gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Coincidence");
            else gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Singles");
            gtk_widget_set_sensitive(WArray[2],TRUE);
            break;
   case 5:  //Phillips 71xx
            GList=g_list_append(GList,"Normal");
            GList=g_list_append(GList,"Test");
            gtk_combo_set_popdown_strings(GTK_COMBO(WArray[2]),GList);
            if (Setup.Hardware.Properties[StnIndex].AdcMode!=Test)
                 gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Normal");
            else gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Test");
            gtk_widget_set_sensitive(WArray[2],TRUE);
            break;
   case 9:  //BARC CM60
/*This ADC has 4 modes. 0=Coincidence without individual gates 1=Coincidence with individual gates
                        2=Singles without individual gates     3=Singles with individual gates
             In modes 0 and 1 a master gate must be present and in 2 and 3 there is no master gate
*/
            GList=g_list_append(GList,"Coincidence");
            GList=g_list_append(GList,"Coin+Gates");
            GList=g_list_append(GList,"Singles");
            GList=g_list_append(GList,"Sing+Gates");
            gtk_combo_set_popdown_strings(GTK_COMBO(WArray[2]),GList);
            switch (Setup.Hardware.Properties[StnIndex].AdcMode)
               {
               case Coincidence: gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Coincidence"); break;
               case CoinGate:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Coin+Gates"); break;
               case Singles:     gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Singles"); break;
               case SingGate:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Sing+Gates"); break;
               default:          gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"Coincidence");
               }
            gtk_widget_set_sensitive(WArray[2],TRUE);
            break;
   default: //Other ADCs dont have any special modes
            GList=g_list_append(GList,"Normal");
            gtk_combo_set_popdown_strings(GTK_COMBO(WArray[2]),GList);
            gtk_widget_set_sensitive(WArray[2],FALSE);
   }
g_list_free(GList);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ModuleClicked(GtkWidget *W,gpointer i)
{
static gchar ModuleNames[13][25]=                                                                        //13 Module types
       { "Empty","Ortec 413 ADC","Ortec 811 ADC","LeCroy ADC/QDC","BARC CM60","Phillips 71xx",
         "CAEN TDC","Silena 4418/Q","BiRa Bit Register","BARC CM88","Sync. Scaler","Unknown","LeCroy MTDC"};
static gchar LAMOpts[2][12]={"Disabled","Enabled"};
static gchar GainOpts[7][4]={"64K","32K","16K","8 K","4 K","2 K","1 K"};
static GdkColor SelectedBg = {0,0xFFFF,0x0000,0x0000};
static GdkColor SelectedFg = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *SelectedStyle;
GtkWidget *Win,*VBox,*HBox,*Label,*But;
GList *GList;
gint CrateNo,StnIndex,Row,j;
gchar Str[80];
                                                                                                                             
StnIndex=GPOINTER_TO_INT(i); GlobalIntRow=StnIndex;                               //0..22 for Crate1 then 23-45 for Crate2
CrateNo=StnIndex/MAX_CAMAC_STNS+1; Row=StnIndex%MAX_CAMAC_STNS;
SelectedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (j=0;j<5;j++) { SelectedStyle->fg[j]=SelectedStyle->text[j]=SelectedFg; SelectedStyle->bg[j]=SelectedBg; }
SetStyleRecursively(W,SelectedStyle);
                                                                                                                             
WArray=g_new(GtkWidget *,6);                                                          //Memory for an array of 5 widgets
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                      //Define a new window and make it modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                     //Ensure visibility of modal window
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(ModuleWinDestroy),W);     //Callback for the destroy signal
gtk_window_set_title(GTK_WINDOW(Win),"CAMAC MODULE");
gtk_window_set_position(GTK_WINDOW(Win),GTK_WIN_POS_CENTER_ALWAYS); 
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
sprintf(Str,"      Props. of Module at Crate %d Station %d",CrateNo,Row+1);
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new(" Module:  "); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
WArray[0]=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBox),WArray[0],FALSE,FALSE,0); GList=NULL;
for (j=0;j<13;j++) GList=g_list_append(GList,ModuleNames[j]);                                         // 13 popup choices
gtk_combo_set_popdown_strings(GTK_COMBO(WArray[0]),GList);                                             //Define the popup
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[0])->entry),ModuleNames[Setup.Hardware.Modules[StnIndex]]);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(WArray[0])->entry),FALSE);
gtk_widget_set_size_request(GTK_WIDGET(WArray[0]),164,-1);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(WArray[0])->entry),"changed",GTK_SIGNAL_FUNC(ModuleNameCallBack),W);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("    LAM:  "); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
WArray[1]=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBox),WArray[1],FALSE,FALSE,0); GList=NULL;
GList=g_list_append(GList,LAMOpts[0]); GList=g_list_append(GList,LAMOpts[1]);                          // 2 popup choices
gtk_combo_set_popdown_strings(GTK_COMBO(WArray[1]),GList);                                             //Define the popup
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[1])->entry),LAMOpts[Setup.Hardware.Properties[StnIndex].AdcLam]);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(WArray[1])->entry),FALSE);
gtk_widget_set_size_request(GTK_WIDGET(WArray[1]),148,-1);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(WArray[1])->entry),"changed",GTK_SIGNAL_FUNC(LAMOptsCallBack),i);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);                     //New HBox for Mode
Label=gtk_label_new("   Mode:  "); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
WArray[2]=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBox),WArray[2],FALSE,FALSE,0); GList=NULL;
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),FALSE);
gtk_widget_set_size_request(GTK_WIDGET(WArray[2]),148,-1);
ListOfAdcModes(StnIndex);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(WArray[2])->entry),"changed",GTK_SIGNAL_FUNC(ModeOptsCallBack),i);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,8); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);                            //A new HBox
Label=gtk_label_new("Gain"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
WArray[3]=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBox),WArray[3],FALSE,FALSE,0); GList=NULL;
for (j=0;j<7;j++) GList=g_list_append(GList,GainOpts[j]);
gtk_combo_set_popdown_strings(GTK_COMBO(WArray[3]),GList);                                              //Define the popup
if (Setup.Hardware.Properties[StnIndex].AdcGain>512) sprintf(Str,"%d K",Setup.Hardware.Properties[StnIndex].AdcGain/1024);
else                                                 sprintf(Str,"%d",Setup.Hardware.Properties[StnIndex].AdcGain);
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),Str);                                    //Set the initial entry
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),FALSE);
gtk_widget_set_size_request(GTK_WIDGET(WArray[3]),64,-1);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(WArray[3])->entry),"changed",GTK_SIGNAL_FUNC(GainCallBack),i);
                                                                                                                             
Label=gtk_label_new("LLD"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
WArray[4]=gtk_entry_new_with_max_length(4); gtk_box_pack_start(GTK_BOX(HBox),WArray[4],FALSE,FALSE,0);
sprintf(Str,"%d",Setup.Hardware.Properties[StnIndex].AdcLLD); gtk_entry_set_text(GTK_ENTRY(WArray[4]),Str);
gtk_widget_set_size_request(GTK_WIDGET(WArray[4]),36,-1);
gtk_signal_connect(GTK_OBJECT(WArray[4]),"changed",GTK_SIGNAL_FUNC(LLDCallBack),i);
                                                                                                                             
Label=gtk_label_new("Func"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
WArray[5]=gtk_entry_new_with_max_length(2); gtk_box_pack_start(GTK_BOX(HBox),WArray[5],FALSE,FALSE,0);
sprintf(Str,"%d",Setup.Hardware.Properties[StnIndex].AdcFCode); gtk_entry_set_text(GTK_ENTRY(WArray[5]),Str);
gtk_widget_set_size_request(GTK_WIDGET(WArray[5]),36,-1);
gtk_signal_connect(GTK_OBJECT(WArray[5]),"changed",GTK_SIGNAL_FUNC(FCodeCallBack),i);                   //Connect callback
                                                                                                                             
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);     //Special Properties: Silena Offsets
But=gtk_button_new_with_label ("Special"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,10);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SpecialProperties),i);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,FALSE,0);
But=gtk_button_new_with_label ("Ok"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ModuleWinClosed),NULL);
                                                                                                                             
gtk_widget_show_all(Win); g_list_free(GList); gtk_style_unref(SelectedStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ParaChanged(GtkWidget *W,gpointer Data)                                                //Call back from HardwareSetup
{
strcpy(Setup.Hardware.Paras[GPOINTER_TO_INT(Data)],gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SubAddrChanged(GtkWidget *W,gpointer Data)                                            //Call back from HardwareSetup
{
strcpy(Setup.Hardware.SubAddr[GPOINTER_TO_INT(Data)],gtk_entry_get_text(GTK_ENTRY(W)));                          //Store
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ParaNamesChanged(GtkWidget *W,gpointer Data)                                           //Call back from HardwareSetup
{
strcpy(Setup.Hardware.ParaNames[GPOINTER_TO_INT(Data)],gtk_entry_get_text(GTK_ENTRY(W)));                          //Store
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ZSupLLDChanged(GtkWidget *W,gpointer Data)                                             //Call back from HardwareSetup
{
Setup.Hardware.ZSupLLD[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));                              //Store
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ZSupULDChanged(GtkWidget *W,gpointer Data)                                             //Call back from HardwareSetup
{
Setup.Hardware.ZSupULD[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));                              //Store
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ClearCrate(gint CrateNo,GtkWidget *Table)
{
gint i,j,StnIndex;
GList *Node;                        //Note: The GList has the table elements in reverse order! and g_list_nth doesnt work!
                                                                                                                             
Node=g_list_last(GTK_TABLE(Table)->children);                                                //First element of table
for (i=0;i<MAX_CAMAC_STNS;++i)                                                              //Table contents for each row
    {
    StnIndex=(CrateNo-1)*MAX_CAMAC_STNS+i;
    Setup.Hardware.Modules[StnIndex]=0; Setup.Hardware.Properties[StnIndex].AdcLam=Disabled;
    Setup.Hardware.Properties[StnIndex].AdcMode=NotRelevant;
    Setup.Hardware.Properties[StnIndex].AdcLLD=50;                                //A safe default for both CM60 and AD413
    Setup.Hardware.Properties[StnIndex].AdcFCode=0;                                   //FCode=0 is common for many modules
    Setup.Hardware.Properties[StnIndex].AdcGain=8192;                                                   //Default ADC gain
    strcpy(Setup.Hardware.Paras[StnIndex],"");                                                             //Empty strings
    strcpy(Setup.Hardware.ParaNames[StnIndex],"");                                                  //Default names: blank
    Setup.Hardware.ZSupLLD[StnIndex]=1; Setup.Hardware.ZSupULD[StnIndex]=8190;                //Safe defaults for all ADCs
    Node=g_list_previous(Node);                                                           //Skip 1 element (Stn No button)
    gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),"Empty");  //Change text in button
    for (j=0;j<3;++j)
        {
        Node=g_list_previous(Node);                                                                         //Next element
        gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),"");               //Change text in text entry
        }
    Node=g_list_previous(Node);                                                                             //Next element
    gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),"1");                  //Change text in text entry
    Node=g_list_previous(Node);                                                                             //Next element
    gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),"8190");               //Change text in text entry
    if (i<MAX_CAMAC_STNS-1) Node=g_list_previous(Node);                                               //Go to the next row
    }
gtk_widget_show_all(Table);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ClearCrate1(GtkWidget *VCrate,GtkWidget *Table)
{ ClearCrate(1,Table); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ClearCrate2(GtkWidget *VCrate,GtkWidget *Table)
{ ClearCrate(2,Table); }
/*----------------------------------------------------------------------------------------------------------------------*/
void CrateSetup(GtkWidget *VCrate,gint CrateNo)
{
static gchar ModuleNames[13][25]=                                                                        //13 Module types
       { "Empty","Ortec 413 ADC","Ortec 811 ADC","LeCroy ADC/QDC","BARC CM60","Phillips 71xx",
         "CAEN TDC","Silena 4418/Q","BiRa Bit Register","BARC CM88","Sync. Scaler","Unknown","LeCroy MTDC"};
static gchar Heading[7][25]=
       {"Stn","Module Type","Parameters","SubAdds","Para Names","Z.Sup LLD","Z.Sup ULD"};
gint ColWidth[7]={40,130,100,100,100,90,90};
GtkWidget *Table,*HeadBut,*StnBut,*ParaEntry,*SubAddrEntry,*ParaNamesEntry,*ZSupLLDEntry,*ZSupULDEntry,*Module,
          *Label,*ClearBut,*HBox;
gint i,StnIndex; 
gchar Str[25];
static GdkColor HeadingBg  = {0,0x8000,0x5000,0x0000};
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor Red        = {0,0xFFFF,0x0000,0x0000};
GtkStyle *HeadingStyle,*LStyle;

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }
LStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) LStyle->fg[i]=Red;

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VCrate),HBox,FALSE,FALSE,0);
sprintf(Str,"SETUP FOR CRATE %d",CrateNo); Label=gtk_label_new(Str);
SetStyleRecursively(Label,LStyle);
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,40);
if (CrateNo==1) ClearBut=gtk_button_new_with_label("Empty Crate 1"); 
else            ClearBut=gtk_button_new_with_label("Empty Crate 2"); 
gtk_box_pack_start(GTK_BOX(HBox),ClearBut,FALSE,FALSE,40);

Table=gtk_table_new(1,7,FALSE);
for (i=0;i<7;i++)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle); 
    gtk_widget_set_size_request(HeadBut,ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }
gtk_box_pack_start(GTK_BOX(VCrate),Table,FALSE,FALSE,2);

CrateScrollArray[CrateNo-1]=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(CrateScrollArray[CrateNo-1]),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_box_pack_start(GTK_BOX(VCrate),CrateScrollArray[CrateNo-1],FALSE,FALSE,0);
if (Setup.Hardware.NCrates==2) gtk_widget_set_size_request(CrateScrollArray[CrateNo-1],-1,350);
else if (CrateNo==1) gtk_widget_set_size_request(CrateScrollArray[CrateNo-1],-1,530); 
else gtk_widget_set_size_request(CrateScrollArray[CrateNo-1],-1,8);

Table=gtk_table_new(MAX_CAMAC_STNS,7,FALSE);                                                         //Define a new table
for (i=0;i<MAX_CAMAC_STNS;i++)                                                              //Table contents for each row
    {
    StnIndex=(CrateNo-1)*MAX_CAMAC_STNS+i;
    sprintf(Str,"%2d",i+1); StnBut=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(GTK_WIDGET(StnBut),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),StnBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Module=gtk_button_new_with_label(ModuleNames[Setup.Hardware.Modules[StnIndex]]);
    gtk_signal_connect(GTK_OBJECT(Module),"clicked",GTK_SIGNAL_FUNC(ModuleClicked),GINT_TO_POINTER(StnIndex));
    gtk_widget_set_size_request(GTK_WIDGET(Module),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(Table),Module,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ParaEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    gtk_entry_set_text(GTK_ENTRY(ParaEntry),Setup.Hardware.Paras[StnIndex]);
    gtk_signal_connect(GTK_OBJECT(ParaEntry),"changed",GTK_SIGNAL_FUNC(ParaChanged),GINT_TO_POINTER(StnIndex));
    gtk_widget_set_size_request(GTK_WIDGET(ParaEntry),ColWidth[2],-1);
    gtk_table_attach(GTK_TABLE(Table),ParaEntry,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    SubAddrEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    gtk_entry_set_text(GTK_ENTRY(SubAddrEntry),Setup.Hardware.SubAddr[StnIndex]);
    gtk_signal_connect(GTK_OBJECT(SubAddrEntry),"changed",GTK_SIGNAL_FUNC(SubAddrChanged),GINT_TO_POINTER(StnIndex));
    gtk_widget_set_size_request(GTK_WIDGET(SubAddrEntry),ColWidth[3],-1);
    gtk_table_attach(GTK_TABLE(Table),SubAddrEntry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ParaNamesEntry=gtk_entry_new_with_max_length(LONG_TEXT_FIELD);
    gtk_entry_set_text(GTK_ENTRY(ParaNamesEntry),Setup.Hardware.ParaNames[StnIndex]);
    gtk_signal_connect(GTK_OBJECT(ParaNamesEntry),"changed",GTK_SIGNAL_FUNC(ParaNamesChanged),GINT_TO_POINTER(StnIndex));
    gtk_widget_set_size_request(GTK_WIDGET(ParaNamesEntry),ColWidth[4],-1);
    gtk_table_attach(GTK_TABLE(Table),ParaNamesEntry,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ZSupLLDEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); 
    sprintf(Str,"%d",Setup.Hardware.ZSupLLD[StnIndex]); gtk_entry_set_text(GTK_ENTRY(ZSupLLDEntry),Str);
    gtk_signal_connect(GTK_OBJECT(ZSupLLDEntry),"changed",GTK_SIGNAL_FUNC(ZSupLLDChanged),GINT_TO_POINTER(StnIndex));
    gtk_widget_set_size_request(GTK_WIDGET(ZSupLLDEntry),ColWidth[5],-1);
    gtk_table_attach(GTK_TABLE(Table),ZSupLLDEntry,5,6,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ZSupULDEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Hardware.ZSupULD[StnIndex]); gtk_entry_set_text(GTK_ENTRY(ZSupULDEntry),Str);
    gtk_signal_connect(GTK_OBJECT(ZSupULDEntry),"changed",GTK_SIGNAL_FUNC(ZSupULDChanged),GINT_TO_POINTER(StnIndex));
    gtk_widget_set_size_request(GTK_WIDGET(ZSupULDEntry),ColWidth[6],-1);
    gtk_table_attach(GTK_TABLE(Table),ZSupULDEntry,6,7,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(CrateScrollArray[CrateNo-1]),Table);
gtk_style_unref(HeadingStyle); gtk_style_unref(LStyle);

//Add signals for the Clear All button only at this stage
if (CrateNo==1) gtk_signal_connect(GTK_OBJECT(ClearBut),"clicked",GTK_SIGNAL_FUNC(ClearCrate1),Table);
else            gtk_signal_connect(GTK_OBJECT(ClearBut),"clicked",GTK_SIGNAL_FUNC(ClearCrate2),Table);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedGates1dParaChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Oned.Gate1[SpecNo].Gate1d[i].Para=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Oned.Gate1[SpecNo].Gate1d[i].Para=CLAMP(Setup.Oned.Gate1[SpecNo].Gate1d[i].Para,1,MAX_PAR);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedGates1dLoLimChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Oned.Gate1[SpecNo].Gate1d[i].Lo=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Oned.Gate1[SpecNo].Gate1d[i].Lo=CLAMP(Setup.Oned.Gate1[SpecNo].Gate1d[i].Lo,0,16383);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedGates1dHiLimChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Oned.Gate1[SpecNo].Gate1d[i].Hi=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Oned.Gate1[SpecNo].Gate1d[i].Hi=CLAMP(Setup.Oned.Gate1[SpecNo].Gate1d[i].Hi,0,16383);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedGates1d(GtkWidget *W,gpointer Data)
{
static gchar Heading[4][25] = {"Gate No","Para No.","Lo Limit","Hi Limit"};
gint ColWidth[4]={55,60,60,60};
GtkWidget *Win,*VBox,*HeadBut,*HBox,*But,*Table,*NumBut,*ParaEntry,*LoLimEntry,*HiLimEntry,*ScrollW;
gint i;
gchar Str[40];
static GdkColor HeadingBg  = {0,0x0000,0x0000,0x0000};
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *HeadingStyle;

SpecNo=GPOINTER_TO_INT(Data);
if (Setup.Oned.Gate1[SpecNo].NGates<=0) return;

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                      //Make the window modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                     //Ensure visibility of modal window
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
sprintf(Str,"1d Gates for 1d Spec# %d",SpecNo+1);
gtk_window_set_title(GTK_WINDOW(Win),Str); gtk_widget_set_uposition(GTK_WIDGET(Win),0,338);
gtk_widget_set_usize(GTK_WIDGET(Win),300,225); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                     //VBox for the entire window

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_usize(GTK_WIDGET(ScrollW),260,26);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(1,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                    //Pack Table into ScrollW
for (i=0;i<4;i++)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle); 
    gtk_widget_set_usize(GTK_WIDGET(HeadBut),ColWidth[i],22);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);

Table=gtk_table_new(Setup.Oned.Gate1[SpecNo].NGates,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                    //Pack Table into ScrollW
for (i=0;i<Setup.Oned.Gate1[SpecNo].NGates;i++)
    {
    sprintf(Str,"%d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_widget_set_usize(GTK_WIDGET(NumBut),ColWidth[0],22);
    gtk_table_attach(GTK_TABLE(Table),NumBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ParaEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Oned.Gate1[SpecNo].Gate1d[i].Para);
    gtk_entry_set_text(GTK_ENTRY(ParaEntry),Str);
    gtk_signal_connect(GTK_OBJECT(ParaEntry),"changed",GTK_SIGNAL_FUNC(OnedGates1dParaChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(ParaEntry),ColWidth[1],22);
    gtk_table_attach(GTK_TABLE(Table),ParaEntry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    LoLimEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Oned.Gate1[SpecNo].Gate1d[i].Lo);
    gtk_entry_set_text(GTK_ENTRY(LoLimEntry),Str);
    gtk_signal_connect(GTK_OBJECT(LoLimEntry),"changed",GTK_SIGNAL_FUNC(OnedGates1dLoLimChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(LoLimEntry),ColWidth[2],22);
    gtk_table_attach(GTK_TABLE(Table),LoLimEntry,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    HiLimEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Oned.Gate1[SpecNo].Gate1d[i].Hi);
    gtk_entry_set_text(GTK_ENTRY(HiLimEntry),Str);
    gtk_signal_connect(GTK_OBJECT(HiLimEntry),"changed",GTK_SIGNAL_FUNC(OnedGates1dHiLimChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(HiLimEntry),ColWidth[3],22);
    gtk_table_attach(GTK_TABLE(Table),HiLimEntry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);   
gtk_container_set_border_width(GTK_CONTAINER(HBox),5);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);           
gtk_widget_show_all(Win);                                                              
gtk_style_unref(HeadingStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectOnedGates2d(GtkWidget *W,gpointer Unused)
{
GList *Node;                        //Note: The GList has the table elements in reverse order! and g_list_nth doesnt work!
gint i,ScrollTo;
gchar Str[MAX_TEXT_FIELD+5];

Node=g_list_last(GTK_TABLE(BanBr->Table)->children);                                              //First element of table
for (i=0;i<2*(BanBr->Row)+1;++i) Node=g_list_previous(Node);                                                  //Skip nodes
sprintf(Setup.Oned.Gate2[SpecNo].Gate2d[BanBr->Row],"%s/%s",FileX->Path,FileX->TargetFile);              //Store full path
AbbreviateFileName(Str,Setup.Oned.Gate2[SpecNo].Gate2d[BanBr->Row],MAX_TEXT_FIELD);
strcpy(BanDir,FileX->Path); SavePrefs();
gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),Str);      //Change text inside button
if (BanBr->Row<Setup.Oned.Gate2[SpecNo].NGates-1) 
   {
   BanBr->Row++; 
   ScrollTo=0; if (BanBr->Row>6) ScrollTo=15.0*BanBr->Row;                                           //Fudging to scroll
   gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment
                                                                          (GTK_SCROLLED_WINDOW(BanBr->ScrlW))),ScrollTo);
   }
else BanBr->Row=0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedGates2dClicked(GtkWidget *W,gpointer Data)
{
BanBr->Row=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select Banana File",EdWin,310,TOP_SPACE,TRUE,".",".ban",TRUE,&SelectOnedGates2d,TRUE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ClearNames1(GtkWidget *W,gpointer Data)
{
GList *Node;                        //Note: The GList has the table elements in reverse order! and g_list_nth doesnt work!
gint i;

Node=g_list_last(GTK_TABLE(BanBr->Table)->children); Node=g_list_previous(Node);                 //Second element of table
for (i=0;i<Setup.Oned.Gate2[SpecNo].NGates;++i)
    {
    strcpy(Setup.Oned.Gate2[SpecNo].Gate2d[i],"");
    gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),"Browse");  //Change text in button
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                         //Skip 2 table elements
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Gates2dClosed(GtkWidget *W,GtkWidget *Win)
{ g_free(BanBr); }
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedGates2d(GtkWidget *W,gpointer Data)
{
static gchar Heading[2][25] = {"Gate No","File Name"};
gint ColWidth[2]={60,200};
GtkWidget *Win,*VBox,*HeadBut,*HBox,*But,*Table,*NumBut;
gint i;
gchar Str[40],Str2[MAX_TEXT_FIELD+5];
static GdkColor HeadingBg  = {0,0x0000,0x0000,0x0000};
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *HeadingStyle;

SpecNo=GPOINTER_TO_INT(Data);
if (Setup.Oned.Gate2[SpecNo].NGates<=0) return;

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }

BanBr=g_new(struct BanBr,1);

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                       //Make the window modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                      //Ensure visibility of modal window
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(Gates2dClosed),Win);
sprintf(Str,"2d Gates for 1d Spec# %d",SpecNo+1);
gtk_window_set_title(GTK_WINDOW(Win),Str); gtk_widget_set_uposition(GTK_WIDGET(Win),0,338);
gtk_widget_set_size_request(GTK_WIDGET(Win),300,250); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                      //VBox for the entire window

BanBr->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(BanBr->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(GTK_WIDGET(BanBr->ScrlW),260,28);
gtk_box_pack_start(GTK_BOX(VBox),BanBr->ScrlW,FALSE,FALSE,0);

Table=gtk_table_new(1,2,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(BanBr->ScrlW),Table);
for (i=0;i<2;++i)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle);
    gtk_widget_set_size_request(GTK_WIDGET(HeadBut),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

BanBr->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(BanBr->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),BanBr->ScrlW,TRUE,TRUE,0);

BanBr->Table=gtk_table_new(Setup.Oned.Gate2[SpecNo].NGates,2,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(BanBr->ScrlW),BanBr->Table);
for (i=0;i<Setup.Oned.Gate2[SpecNo].NGates;++i)
    {
    sprintf(Str,"%d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(GTK_WIDGET(NumBut),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(BanBr->Table),NumBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    if (!strlen(Setup.Oned.Gate2[SpecNo].Gate2d[i])) But=gtk_button_new_with_label("Browse");
    else
       {
       AbbreviateFileName(Str2,Setup.Oned.Gate2[SpecNo].Gate2d[i],MAX_TEXT_FIELD);
       But=gtk_button_new_with_label(Str2);
       }
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OnedGates2dClicked),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(BanBr->Table),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
gtk_container_set_border_width(GTK_CONTAINER(HBox),5);
But=gtk_button_new_with_label("Clear all names"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ClearNames1),NULL);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
gtk_style_unref(HeadingStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedParChanged(GtkWidget *W,gpointer Data)                                                  //Call back from SetOned
{
gint i,Par2;
const gchar *Str; gchar *Pos;

i=GPOINTER_TO_INT(Data);
Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Oned.Par[i]=atoi(Str);
Setup.Oned.Par[i]=CLAMP(Setup.Oned.Par[i],0,NTOT);                         //Setup.Oned.Par[i]=0 for hit-pattern spectrum
Pos=index(Str,(int)':');
if (Pos && (Pos+1))                               //If the delimiter ':' is found, it is a multi-update (vector) spectrum
   { Par2=atoi(Pos+1); Par2=CLAMP(Par2,Setup.Oned.Par[i]+1,NTOT); Setup.Oned.NPar[i]=Par2-Setup.Oned.Par[i]+1; }
else Setup.Oned.NPar[i]=1;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedChanChanged(GtkWidget *W,gpointer Data)
{
gint i;
const gchar *Str;

i=GPOINTER_TO_INT(Data);
Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Oned.Chan[i]=atoi(Str);
if (Str[2]=='K') Setup.Oned.Chan[i]*=1024;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedNGates1Changed(GtkWidget *W,gpointer Data)                                      
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Oned.Gate1[i].NGates=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Oned.Gate1[i].NGates=CLAMP(Setup.Oned.Gate1[i].NGates,0,MAX_GATES_1D);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedNGates2Changed(GtkWidget *W,gpointer Data)                                     
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Oned.Gate2[i].NGates=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Oned.Gate2[i].NGates=CLAMP(Setup.Oned.Gate2[i].NGates,0,MAX_GATES_2D);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedCond1(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
if (Setup.Oned.Gate1[i].Cond == And) 
   { Setup.Oned.Gate1[i].Cond=Or; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"OR"); }
else 
   { Setup.Oned.Gate1[i].Cond=And; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"AND"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OnedCond2(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
if (Setup.Oned.Gate2[i].Cond == And) 
   { Setup.Oned.Gate2[i].Cond=Or; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"OR"); }
else 
   { Setup.Oned.Gate2[i].Cond=And; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"AND"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Glob1(GtkWidget *W,GtkWidget *Table)
{
gchar *Str1,Str2[MAX_TEXT_FIELD],Str3[MAX_TEXT_FIELD];
const gchar *Str;
gint LoNum,HiNum,NumStep,LoPar,ParStep,i,Par,j;
GList *Node;

Str=gtk_entry_get_text(GTK_ENTRY(WArray[0]));
if (!strlen(Str)) return;
LoNum=CLAMP(atoi(Str)-1,0,Setup.Oned.N-1); HiNum=LoNum;
Str1=index(Str,'-'); if (Str1) HiNum=atoi(&Str1[1])-1; HiNum=CLAMP(HiNum,LoNum,Setup.Oned.N-1);
NumStep=1; Str1=index(Str,'p'); if (Str1) NumStep=atoi(&Str1[1]); NumStep=MAX(1,NumStep);
Str=gtk_entry_get_text(GTK_ENTRY(WArray[1]));
if (!strlen(Str)) LoPar=1; else LoPar=atoi(Str); LoPar=MIN(MAX_PAR,LoPar);
ParStep=1; Str1=index(Str,'p'); if (Str1) ParStep=atoi(&Str1[1]); 
Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry)); 
Setup.Oned.Chan[LoNum]=atoi(Str); if (Str[2]=='K') Setup.Oned.Chan[LoNum]*=1024;

if (Setup.Oned.Chan[LoNum]==16384)     sprintf(Str3,"%dK",Setup.Oned.Chan[LoNum]/1024);
else if (Setup.Oned.Chan[LoNum]>=1024) sprintf(Str3,"%d K",Setup.Oned.Chan[LoNum]/1024);
else                                   sprintf(Str3,"%d",Setup.Oned.Chan[LoNum]);

Node=g_list_last(GTK_TABLE(Table)->children); Node=g_list_previous(Node);
for (i=0;i<LoNum;++i) for (j=0;j<10;++j) Node=g_list_previous(Node);
for (i=LoNum,Par=LoPar;i<=HiNum;i+=NumStep)
    { 
    Setup.Oned.Par[i]=Par; Setup.Oned.Chan[i]=Setup.Oned.Chan[LoNum];
    sprintf(Str2,"%d",Par); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str2);
    Node=g_list_previous(Node); 
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(((GtkTableChild *)Node->data)->widget)->entry),Str3);
    Par+=ParStep; Par=MIN(MAX_PAR,Par);
    if (i<HiNum) for (j=0;j<10*NumStep-1;++j) Node=g_list_previous(Node);
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SetOnedDestroyed(GtkWidget *W,gpointer Unused)
{ g_free(WArray); }
/*----------------------------------------------------------------------------------------------------------------------*/
void Name1dChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
sprintf(Setup.Oned.Name[i],"%s",gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SetOned(GtkWidget *W,gpointer Data)
{
static gchar Heading[10][28]= {"No\n","Para\n","Spec.\nSize","No of\n1d Gates","Condition\n","Define\n1d Gates",
                              "No of\n2d Gates","Condition\n","Define\n2dGates","Name\n"};
gint ColWidth[10]={37,37,60,68,69,68,68,69,68,68};
GtkWidget *Win,*VBox,*ScrollW,*Table,*HeadBut,*OKBut,*HBox,*NumBut,*ParaEntry,*ChanCombo,*Def1,*Def2,*CBut1,*CBut2,
          *OnedNGates1,*OnedNGates2,*But,*Label,*Entry;
static GdkColor RedC    = {0,0xFFFF,0x0000,0x0000};
static GdkColor WhiteC  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor BlueC   = {0,0x0000,0x0000,0xFFFF};
GtkStyle *HeadingStyle,*BoldStyle;
gint i,GlobSize=1024;
gchar Str[40];
GList *GList;

if (!Setup.Oned.N) return;
WArray=g_new(GtkWidget *,3);

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=WhiteC; HeadingStyle->bg[i]=RedC; }
BoldStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { BoldStyle->fg[i]=BoldStyle->text[i]=BlueC; BoldStyle->bg[i]=WhiteC; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                 /*Define Win and make it modal*/
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                       /*Ensure visibility of modal window*/
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(SetOnedDestroyed),NULL);
gtk_window_set_title(GTK_WINDOW(Win),"Define 1d Spectra"); gtk_widget_set_uposition(GTK_WIDGET(Win),0,52);
gtk_widget_set_size_request(Win,-1,460); 
gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);

ScrollW=gtk_scrolled_window_new(NULL,NULL); 
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
//gtk_widget_set_size_request(ScrollW,220,-1);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(1,10,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                                     /*Pack Table into ScrollW*/
for (i=0;i<10;i++)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle);
    gtk_widget_set_size_request(HeadBut,ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);

Table=gtk_table_new(Setup.Oned.N,10,FALSE);                                                                        /*Define a new table*/
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                                     /*Pack Table into ScrollW*/
for (i=0;i<Setup.Oned.N;i++)
    {
    sprintf(Str,"%2d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(NumBut,ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),NumBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ParaEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    if (Setup.Oned.NPar[i]==1) sprintf(Str,"%d",Setup.Oned.Par[i]);
    else sprintf(Str,"%d:%d",Setup.Oned.Par[i],Setup.Oned.Par[i]+Setup.Oned.NPar[i]-1);
    gtk_entry_set_text(GTK_ENTRY(ParaEntry),Str);
    gtk_signal_connect(GTK_OBJECT(ParaEntry),"changed",GTK_SIGNAL_FUNC(OnedParChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(ParaEntry),ColWidth[1],22);
    gtk_table_attach(GTK_TABLE(Table),ParaEntry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ChanCombo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(ChanCombo),ColWidth[2],-1);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),FALSE);
    GList=NULL; GList=g_list_append(GList,"16K"); GList=g_list_append(GList,"8 K"); 
    GList=g_list_append(GList,"4 K"); GList=g_list_append(GList,"2 K");
    GList=g_list_append(GList,"1 K"); GList=g_list_append(GList,"512"); GList=g_list_append(GList,"256");
    GList=g_list_append(GList,"128"); GList=g_list_append(GList,"64"); GList=g_list_append(GList,"32"); GList=g_list_append(GList,"16");
    gtk_combo_set_popdown_strings(GTK_COMBO(ChanCombo),GList);                                                           //Define the popup
    switch (Setup.Oned.Chan[i])                                                                                     //Set the initial entry
      {
      case 16384: gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"16K"); break;
      case 8192:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"8 K"); break;
      case 4096:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"4 K"); break;
      case 2048:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"2 K"); break;
      case 1024:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"1 K"); break;
      case 512:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"512"); break;
      case 256:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"256"); break;
      case 128:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"128"); break;
      case 64:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"64");  break;
      case 32:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"32");  break;
      case 16:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"16");  break;
      default:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ChanCombo)->entry),"1 K");
      }
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(ChanCombo)->entry),"changed",GTK_SIGNAL_FUNC(OnedChanChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),ChanCombo,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    OnedNGates1=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Oned.Gate1[i].NGates); gtk_entry_set_text(GTK_ENTRY(OnedNGates1),Str);
    gtk_signal_connect(GTK_OBJECT(OnedNGates1),"changed",GTK_SIGNAL_FUNC(OnedNGates1Changed),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(GTK_WIDGET(OnedNGates1),ColWidth[3],-1);
    gtk_table_attach(GTK_TABLE(Table),OnedNGates1,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    if (Setup.Oned.Gate1[i].Cond == And) strcpy(Str,"AND"); else strcpy(Str,"OR"); CBut1=gtk_button_new_with_label(Str);
    gtk_signal_connect(GTK_OBJECT(CBut1),"clicked",GTK_SIGNAL_FUNC(OnedCond1),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(CBut1,ColWidth[4],-1);
    gtk_table_attach(GTK_TABLE(Table),CBut1,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Def1=gtk_button_new_with_label("Define"); 
    gtk_signal_connect(GTK_OBJECT(Def1),"clicked",GTK_SIGNAL_FUNC(OnedGates1d),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(Def1,ColWidth[5],-1);
    gtk_table_attach(GTK_TABLE(Table),Def1,5,6,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    OnedNGates2=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Oned.Gate2[i].NGates); gtk_entry_set_text(GTK_ENTRY(OnedNGates2),Str);
    gtk_signal_connect(GTK_OBJECT(OnedNGates2),"changed",GTK_SIGNAL_FUNC(OnedNGates2Changed),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(OnedNGates2,ColWidth[6],-1);
    gtk_table_attach(GTK_TABLE(Table),OnedNGates2,6,7,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    if (Setup.Oned.Gate2[i].Cond == And) strcpy(Str,"AND"); else strcpy(Str,"OR"); CBut2=gtk_button_new_with_label(Str);
    gtk_signal_connect(GTK_OBJECT(CBut2),"clicked",GTK_SIGNAL_FUNC(OnedCond2),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(CBut2,ColWidth[7],-1);
    gtk_table_attach(GTK_TABLE(Table),CBut2,7,8,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Def2=gtk_button_new_with_label("Define"); 
    gtk_signal_connect(GTK_OBJECT(Def2),"clicked",GTK_SIGNAL_FUNC(OnedGates2d),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(Def2,ColWidth[8],-1);
    gtk_table_attach(GTK_TABLE(Table),Def2,8,9,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    gtk_entry_set_text(GTK_ENTRY(Entry),Setup.Oned.Name[i]);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(Name1dChanged),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(Entry,ColWidth[9],-1);
    gtk_table_attach(GTK_TABLE(Table),Entry,9,10,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,8);
Label=gtk_label_new("Global Shortcut:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Label=gtk_label_new(" Set ranges e.g. 1-20 or 1-40 step 2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
SetStyleRecursively(Label,BoldStyle);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("No. Range"); gtk_widget_set_size_request(GTK_WIDGET(But),100,-1); 
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,4);
But=gtk_button_new_with_label("Para Range"); gtk_widget_set_size_request(GTK_WIDGET(But),100,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,4);
But=gtk_button_new_with_label("Size"); gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[2],-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,4);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
WArray[0]=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(WArray[0],100,-1);
gtk_box_pack_start(GTK_BOX(HBox),WArray[0],FALSE,FALSE,4);
WArray[1]=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(WArray[1],100,-1);
gtk_box_pack_start(GTK_BOX(HBox),WArray[1],FALSE,FALSE,4);
WArray[2]=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(WArray[2]),ColWidth[2],-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),FALSE);
gtk_combo_set_popdown_strings(GTK_COMBO(WArray[2]),GList);
switch (GlobSize)
  {
  case 16384: gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"16K"); break;
  case 8192:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"8 K"); break;
  case 4096:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"4 K"); break;
  case 2048:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"2 K"); break;
  case 1024:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"1 K"); break;
  case 512:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"512"); break;
  case 256:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"256"); break;
  case 128:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"128"); break;
  case 64:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"64");  break;
  case 32:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"32");  break;
  case 16:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"16");  break;
  default:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[2])->entry),"1 K");
  }
gtk_box_pack_start(GTK_BOX(HBox),WArray[2],FALSE,FALSE,4);
But=gtk_button_new_with_label("Apply");
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,4);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Glob1),Table);

HBox=gtk_hbox_new(TRUE,0);  gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Note: Para=0 builds a hit-pattern spectrum"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);                                    /*HBox for OK Button*/
OKBut=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),OKBut,TRUE,FALSE,0);                          /*Define OK Button*/
gtk_signal_connect(GTK_OBJECT(OKBut),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);                           /*Connect signal for OK Button*/

gtk_widget_show_all(Win);
gtk_style_unref(HeadingStyle); gtk_style_unref(BoldStyle); g_list_free(GList);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodGates1dParaChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Twod.Gate1[SpecNo].Gate1d[i].Para=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Twod.Gate1[SpecNo].Gate1d[i].Para=CLAMP(Setup.Twod.Gate1[SpecNo].Gate1d[i].Para,1,MAX_PAR);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodGates1dLoLimChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Twod.Gate1[SpecNo].Gate1d[i].Lo=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Twod.Gate1[SpecNo].Gate1d[i].Lo=CLAMP(Setup.Twod.Gate1[SpecNo].Gate1d[i].Lo,0,16383);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodGates1dHiLimChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Twod.Gate1[SpecNo].Gate1d[i].Hi=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Twod.Gate1[SpecNo].Gate1d[i].Hi=CLAMP(Setup.Twod.Gate1[SpecNo].Gate1d[i].Hi,0,16383);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodGates1d(GtkWidget *W,gpointer Data)
{
static gchar Heading[4][25] = {"Gate No","Para No.","Lo Limit","Hi Limit"};
gint ColWidth[4]={55,60,60,60};
GtkWidget *Win,*VBox,*HeadBut,*HBox,*But,*Table,*NumBut,*ParaEntry,*LoLimEntry,*HiLimEntry,*ScrollW;
gint i;
gchar Str[40];
static GdkColor HeadingBg  = {0,0x0000,0x0000,0x0000};                                           //Colour for HeadingStyle
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};                                           //Colour for HeadingStyle
GtkStyle *HeadingStyle;

SpecNo=GPOINTER_TO_INT(Data);
if (Setup.Twod.Gate1[SpecNo].NGates<=0) return;

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());                            //Copy default style to this style
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                        //Make the window modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                       //Ensure visibility of modal window
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
sprintf(Str,"1d Gates for 2d Spec# %d",SpecNo+1);
gtk_window_set_title(GTK_WINDOW(Win),Str); gtk_widget_set_uposition(GTK_WIDGET(Win),0,338);
gtk_widget_set_usize(GTK_WIDGET(Win),300,225); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                                     /*VBox for the entire window*/

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);     /*Scroll bars only if needed*/
gtk_widget_set_usize(GTK_WIDGET(ScrollW),260,26);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(1,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                                     /*Pack Table into ScrollW*/
for (i=0;i<4;i++)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle); 
    gtk_widget_set_usize(GTK_WIDGET(HeadBut),ColWidth[i],22);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);     /*Scroll bars only if needed*/
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);

Table=gtk_table_new(Setup.Twod.Gate1[SpecNo].NGates,4,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                                     /*Pack Table into ScrollW*/
for (i=0;i<Setup.Twod.Gate1[SpecNo].NGates;i++)
    {
    sprintf(Str,"%d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_widget_set_usize(GTK_WIDGET(NumBut),ColWidth[0],22);
    gtk_table_attach(GTK_TABLE(Table),NumBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    ParaEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Twod.Gate1[SpecNo].Gate1d[i].Para);
    gtk_entry_set_text(GTK_ENTRY(ParaEntry),Str);
    gtk_signal_connect(GTK_OBJECT(ParaEntry),"changed",GTK_SIGNAL_FUNC(TwodGates1dParaChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(ParaEntry),ColWidth[1],22);
    gtk_table_attach(GTK_TABLE(Table),ParaEntry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    LoLimEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Twod.Gate1[SpecNo].Gate1d[i].Lo);
    gtk_entry_set_text(GTK_ENTRY(LoLimEntry),Str);
    gtk_signal_connect(GTK_OBJECT(LoLimEntry),"changed",GTK_SIGNAL_FUNC(TwodGates1dLoLimChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(LoLimEntry),ColWidth[2],22);
    gtk_table_attach(GTK_TABLE(Table),LoLimEntry,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    HiLimEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Twod.Gate1[SpecNo].Gate1d[i].Hi);
    gtk_entry_set_text(GTK_ENTRY(HiLimEntry),Str);
    gtk_signal_connect(GTK_OBJECT(HiLimEntry),"changed",GTK_SIGNAL_FUNC(TwodGates1dHiLimChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(HiLimEntry),ColWidth[3],22);
    gtk_table_attach(GTK_TABLE(Table),HiLimEntry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);   
gtk_container_set_border_width(GTK_CONTAINER(HBox),5);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);           
gtk_widget_show_all(Win);                                                              
gtk_style_unref(HeadingStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectTwodGates2d(GtkWidget *W,gpointer Unused)
{
GList *Node;                        //Note: The GList has the table elements in reverse order! and g_list_nth doesnt work!
gint i,ScrollTo;
gchar Str[MAX_TEXT_FIELD+5];
                                                                                                                             
Node=g_list_last(GTK_TABLE(BanBr->Table)->children);                                              //First element of table
for (i=0;i<2*(BanBr->Row)+1;++i) Node=g_list_previous(Node);                                                  //Skip nodes
sprintf(Setup.Twod.Gate2[SpecNo].Gate2d[BanBr->Row],"%s/%s",FileX->Path,FileX->TargetFile);              //Store full path
AbbreviateFileName(Str,Setup.Twod.Gate2[SpecNo].Gate2d[BanBr->Row],MAX_TEXT_FIELD);
strcpy(BanDir,FileX->Path); SavePrefs();
gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),Str);      //Change text inside button
if (BanBr->Row<Setup.Twod.Gate2[SpecNo].NGates-1) 
   {
   BanBr->Row++; 
   ScrollTo=0; if (BanBr->Row>6) ScrollTo=15.0*BanBr->Row;                                           //Fudging to scroll
   gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment
                                                                          (GTK_SCROLLED_WINDOW(BanBr->ScrlW))),ScrollTo);
   }
else BanBr->Row=0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodGates2dClicked(GtkWidget *W,gpointer Data)
{
BanBr->Row=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select Banana File",EdWin,310,TOP_SPACE,TRUE,".",".ban",TRUE,&SelectTwodGates2d,TRUE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ClearNames2(GtkWidget *W,gpointer Data)
{
GList *Node;                        //Note: The GList has the table elements in reverse order! and g_list_nth doesnt work!
gint i;
                                                                                                                             
Node=g_list_last(GTK_TABLE(BanBr->Table)->children); Node=g_list_previous(Node);                 //Second element of table
for (i=0;i<Setup.Twod.Gate2[SpecNo].NGates;++i)
    {
    strcpy(Setup.Twod.Gate2[SpecNo].Gate2d[i],"");
    gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),"Browse");  //Change text in button
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                         //Skip 2 table elements
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodGates2d(GtkWidget *W,gpointer Data)
{
static gchar Heading[2][25] = {"Gate No","File Name"};
gint ColWidth[2]={60,200};
GtkWidget *Win,*VBox,*HeadBut,*HBox,*But,*Table,*NumBut;
gint i;
gchar Str[40],Str2[MAX_TEXT_FIELD+2];
static GdkColor HeadingBg  = {0,0x0000,0x0000,0x0000};                                           //Colour for HeadingStyle
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};                                           //Colour for HeadingStyle
GtkStyle *HeadingStyle;

SpecNo=GPOINTER_TO_INT(Data);
if (Setup.Twod.Gate2[SpecNo].NGates<=0) return;

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());                            //Copy default style to this style
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }

BanBr=g_new(struct BanBr,1);

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                       //Make the window modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                      //Ensure visibility of modal window
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(Gates2dClosed),Win);
sprintf(Str,"2d Gates for 2d Spec# %d",SpecNo+1);
gtk_window_set_title(GTK_WINDOW(Win),Str); gtk_widget_set_uposition(GTK_WIDGET(Win),0,338);
gtk_widget_set_size_request(GTK_WIDGET(Win),300,250); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                       //VBox for the entire window

BanBr->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(BanBr->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(GTK_WIDGET(BanBr->ScrlW),260,28);
gtk_box_pack_start(GTK_BOX(VBox),BanBr->ScrlW,FALSE,FALSE,0);

Table=gtk_table_new(1,2,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(BanBr->ScrlW),Table);
for (i=0;i<2;i++)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle);
    gtk_widget_set_size_request(GTK_WIDGET(HeadBut),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

BanBr->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(BanBr->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),BanBr->ScrlW,TRUE,TRUE,0);

BanBr->Table=gtk_table_new(Setup.Twod.Gate2[SpecNo].NGates,2,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(BanBr->ScrlW),BanBr->Table);
for (i=0;i<Setup.Twod.Gate2[SpecNo].NGates;i++)
    {
    sprintf(Str,"%d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(GTK_WIDGET(NumBut),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(BanBr->Table),NumBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    if (!strlen(Setup.Twod.Gate2[SpecNo].Gate2d[i])) But=gtk_button_new_with_label("Browse");
    else
       {
       AbbreviateFileName(Str2,Setup.Twod.Gate2[SpecNo].Gate2d[i],MAX_TEXT_FIELD);
       But=gtk_button_new_with_label(Str2);
       }
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(TwodGates2dClicked),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(BanBr->Table),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
gtk_container_set_border_width(GTK_CONTAINER(HBox),5);
But=gtk_button_new_with_label("Clear all names"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ClearNames2),NULL);
But=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
gtk_style_unref(HeadingStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodXParChanged(GtkWidget *W,gpointer Data)                                                 //Call back from SetTwod
{
gint i,Par2;
const gchar *Str; gchar *Pos;

i=GPOINTER_TO_INT(Data); Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Twod.XPar[i]=atoi(Str);
Setup.Twod.XPar[i]=CLAMP(Setup.Twod.XPar[i],1,MAX_TOTAL_PAR);
Pos=index(Str,(int)':');
if (Pos && (Pos+1))                               //If the delimiter ':' is found, it is a multi-update (vector) spectrum
   { Par2=atoi(Pos+1); Par2=CLAMP(Par2,Setup.Twod.XPar[i]+1,NTOT); Setup.Twod.NXPar[i]=Par2-Setup.Twod.XPar[i]+1; }
else Setup.Twod.NXPar[i]=1;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodYParChanged(GtkWidget *W,gpointer Data)                                                  //Call back from SetTwod
{
gint i,Par2;
const gchar *Str; gchar *Pos;

i=GPOINTER_TO_INT(Data); Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Twod.YPar[i]=atoi(Str);
Setup.Twod.YPar[i]=CLAMP(Setup.Twod.YPar[i],1,MAX_TOTAL_PAR);
Pos=index(Str,(int)':');
if (Pos && (Pos+1))                                //If the delimiter ':' is found, it is a multi-update (vector) spectrum
   { Par2=atoi(Pos+1); Par2=CLAMP(Par2,Setup.Twod.YPar[i]+1,NTOT); Setup.Twod.NYPar[i]=Par2-Setup.Twod.YPar[i]+1; }
else Setup.Twod.NYPar[i]=1;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodXChanChanged(GtkWidget *W,gpointer Data)
{
gint i;
const gchar *Str;

i=GPOINTER_TO_INT(Data);
Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Twod.XChan[i]=atoi(Str);
if (Str[2]=='K') Setup.Twod.XChan[i]*=1024;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodYChanChanged(GtkWidget *W,gpointer Data)
{
gint i;
const gchar *Str;

i=GPOINTER_TO_INT(Data);
Str=gtk_entry_get_text(GTK_ENTRY(W));
Setup.Twod.YChan[i]=atoi(Str);
if (Str[2]=='K') Setup.Twod.YChan[i]*=1024;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodNGates1Changed(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Twod.Gate1[i].NGates=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Twod.Gate1[i].NGates=CLAMP(Setup.Twod.Gate1[i].NGates,0,MAX_GATES_2D);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodNGates2Changed(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Twod.Gate2[i].NGates=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Twod.Gate2[i].NGates=CLAMP(Setup.Twod.Gate2[i].NGates,0,MAX_GATES_2D);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodCond1(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
if (Setup.Twod.Gate1[i].Cond == And) 
   { Setup.Twod.Gate1[i].Cond=Or; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"OR"); }
else 
   { Setup.Twod.Gate1[i].Cond=And; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"AND"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void TwodCond2(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
if (Setup.Twod.Gate2[i].Cond == And) 
   { Setup.Twod.Gate2[i].Cond=Or; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"OR"); }
else 
   { Setup.Twod.Gate2[i].Cond=And; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"AND"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Name2dChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
sprintf(Setup.Twod.Name[i],"%s",gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Glob2(GtkWidget *W,GtkWidget *Table)
{
gchar *Str1,Str2[MAX_TEXT_FIELD],Str3[MAX_TEXT_FIELD],Str4[MAX_TEXT_FIELD];
const gchar *Str;
gint LoNum,HiNum,NumStep,LoXPar,XParStep,XPar,LoYPar,YParStep,YPar,i,j;
GList *Node;

Str=gtk_entry_get_text(GTK_ENTRY(WArray[0]));
if (!strlen(Str)) return;
LoNum=CLAMP(atoi(Str)-1,0,Setup.Twod.N-1); HiNum=LoNum;
Str1=index(Str,'-'); if (Str1) HiNum=atoi(&Str1[1])-1; HiNum=CLAMP(HiNum,LoNum,Setup.Twod.N-1);
NumStep=1; Str1=index(Str,'p'); if (Str1) NumStep=atoi(&Str1[1]); NumStep=MAX(1,NumStep);

Str=gtk_entry_get_text(GTK_ENTRY(WArray[1]));
if (!strlen(Str)) LoXPar=1; else LoXPar=atoi(Str); LoXPar=MIN(MAX_PAR,LoXPar);
XParStep=1; Str1=index(Str,'p'); if (Str1) XParStep=atoi(&Str1[1]);

Str=gtk_entry_get_text(GTK_ENTRY(WArray[2]));
if (!strlen(Str)) LoYPar=1; else LoYPar=atoi(Str); LoYPar=MIN(MAX_PAR,LoYPar);
YParStep=1; Str1=index(Str,'p'); if (Str1) YParStep=atoi(&Str1[1]);

Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry));
Setup.Twod.XChan[LoNum]=atoi(Str); if (Str[2]=='K') Setup.Twod.XChan[LoNum]*=1024;
if (Setup.Twod.XChan[LoNum]>=1024) sprintf(Str3,"%d K",Setup.Twod.XChan[LoNum]/1024);
else                               sprintf(Str3,"%d",Setup.Twod.XChan[LoNum]);

Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry));
Setup.Twod.YChan[LoNum]=atoi(Str); if (Str[2]=='K') Setup.Twod.YChan[LoNum]*=1024;
if (Setup.Twod.YChan[LoNum]>=1024) sprintf(Str4,"%d K",Setup.Twod.YChan[LoNum]/1024);
else                               sprintf(Str4,"%d",Setup.Twod.YChan[LoNum]);

Node=g_list_last(GTK_TABLE(Table)->children); Node=g_list_previous(Node);
for (i=0;i<LoNum;++i) for (j=0;j<12;++j) Node=g_list_previous(Node);
for (i=LoNum,XPar=LoXPar,YPar=LoYPar;i<=HiNum;i+=NumStep)
    {
    Setup.Twod.XPar[i]=XPar; Setup.Twod.XChan[i]=Setup.Twod.XChan[LoNum];
    sprintf(Str2,"%d",XPar); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str2);
    Node=g_list_previous(Node);
    Setup.Twod.YPar[i]=YPar; Setup.Twod.YChan[i]=Setup.Twod.YChan[LoNum];
    sprintf(Str2,"%d",YPar); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str2);
    Node=g_list_previous(Node);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(((GtkTableChild *)Node->data)->widget)->entry),Str3);
    Node=g_list_previous(Node);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(((GtkTableChild *)Node->data)->widget)->entry),Str4);
    XPar+=XParStep; XPar=MIN(MAX_PAR,XPar); YPar+=YParStep; YPar=MIN(MAX_PAR,YPar);
    if (i<HiNum) for (j=0;j<12*NumStep-3;++j) Node=g_list_previous(Node);
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SetTwodDestroyed(GtkWidget *W,gpointer Unused)
{ g_free(WArray); }
/*----------------------------------------------------------------------------------------------------------------------*/
void SetTwod(GtkWidget *W,gpointer Data)
{
static gchar Heading[12][28]= {"No\n","X\nPara","Y\nPara","X\nSize","Y\nSize","No of\n1d Gates","Cond.\n",
                               "Define\n1d Gates","No of\n2d Gates","Cond.\n","Define\n2dGates","Name\n"};
gint ColWidth[12]={28,37,37,60,60,68,48,68,68,48,68,68};
GtkWidget *Win,*VBox,*ScrollW,*Table,*HeadBut,*OKBut,*HBox,*NumBut,*XParEntry,*YParEntry,*XChan,*YChan,*Def1,*Def2,
          *CBut1,*CBut2,*TwodNGates1,*TwodNGates2,*Entry,*Label,*But;
static GdkColor GreenC  = {0,0x4444,0x8888,0x2222};
static GdkColor WhiteC  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor BlueC   = {0,0x0000,0x0000,0xFFFF};
GtkStyle *HeadingStyle,*BoldStyle;
gint i,GlobXSize=512,GlobYSize=512;
gchar Str[40];
GList *GListX,*GListY;

if (!Setup.Twod.N) return;
WArray=g_new(GtkWidget *,5);

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=WhiteC; HeadingStyle->bg[i]=GreenC; }
BoldStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { BoldStyle->fg[i]=BoldStyle->text[i]=BlueC; BoldStyle->bg[i]=WhiteC; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                  //Define Win and make it modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                        //Ensure visibility of modal window
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(SetTwodDestroyed),NULL);
gtk_window_set_title(GTK_WINDOW(Win),"Define 2d Spectra"); gtk_widget_set_uposition(Win,0,52);
gtk_widget_set_size_request(Win,-1,350); gtk_container_set_border_width(GTK_CONTAINER(Win),5);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);

ScrollW=gtk_scrolled_window_new(NULL,NULL); 
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_widget_set_size_request(ScrollW,220,-1);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(1,12,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                        //Pack Table into ScrollW
for (i=0;i<12;i++)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle);
    gtk_widget_set_size_request(HeadBut,ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);

Table=gtk_table_new(Setup.Twod.N,12,FALSE);                                                                        /*Define a new table*/
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                                     /*Pack Table into ScrollW*/
for (i=0;i<Setup.Twod.N;++i)
    {
    sprintf(Str,"%2d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(NumBut,ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),NumBut,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    XParEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    if (Setup.Twod.NXPar[i]==1) sprintf(Str,"%d",Setup.Twod.XPar[i]);
    else sprintf(Str,"%d:%d",Setup.Twod.XPar[i],Setup.Twod.XPar[i]+Setup.Twod.NXPar[i]-1);
    gtk_entry_set_text(GTK_ENTRY(XParEntry),Str);
    gtk_signal_connect(GTK_OBJECT(XParEntry),"changed",GTK_SIGNAL_FUNC(TwodXParChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(XParEntry),ColWidth[1],22);
    gtk_table_attach(GTK_TABLE(Table),XParEntry,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    YParEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    if (Setup.Twod.NYPar[i]==1) sprintf(Str,"%d",Setup.Twod.YPar[i]);
    else sprintf(Str,"%d:%d",Setup.Twod.YPar[i],Setup.Twod.YPar[i]+Setup.Twod.NYPar[i]-1);
    gtk_entry_set_text(GTK_ENTRY(YParEntry),Str);
    gtk_signal_connect(GTK_OBJECT(YParEntry),"changed",GTK_SIGNAL_FUNC(TwodYParChanged),GINT_TO_POINTER(i));
    gtk_widget_set_usize(GTK_WIDGET(YParEntry),ColWidth[2],22);
    gtk_table_attach(GTK_TABLE(Table),YParEntry,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    XChan=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(XChan),ColWidth[3],-1);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(XChan)->entry),FALSE);
    GListX=NULL; GListX=g_list_append(GListX,"8 K"); GListX=g_list_append(GListX,"4 K"); GListX=g_list_append(GListX,"2 K");
    GListX=g_list_append(GListX,"1 K"); GListX=g_list_append(GListX,"512"); GListX=g_list_append(GListX,"256");
    GListX=g_list_append(GListX,"128"); GListX=g_list_append(GListX,"64"); GListX=g_list_append(GListX,"32"); 
    GListX=g_list_append(GListX,"16");
    gtk_combo_set_popdown_strings(GTK_COMBO(XChan),GListX);                                                           //Define the popup
    switch (Setup.Twod.XChan[i])                                                                                     //Set the initial entry
      {
      case 8192:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"8 K"); break;
      case 4096:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"4 K"); break;
      case 2048:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"2 K"); break;
      case 1024:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"1 K"); break;
      case 512:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"512"); break;
      case 256:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"256"); break;
      case 128:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"128"); break;
      case 64:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"64");  break;
      case 32:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"32");  break;
      case 16:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"16");  break;
      default:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(XChan)->entry),"1 K");
      }
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(XChan)->entry),"changed",GTK_SIGNAL_FUNC(TwodXChanChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),XChan,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    YChan=gtk_combo_new(); gtk_widget_set_size_request(YChan,ColWidth[4],-1);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(YChan)->entry),FALSE);
    GListY=NULL; GListY=g_list_append(GListY,"8 K"); GListY=g_list_append(GListY,"4 K"); GListY=g_list_append(GListY,"2 K");
    GListY=g_list_append(GListY,"1 K"); GListY=g_list_append(GListY,"512"); GListY=g_list_append(GListY,"256");
    GListY=g_list_append(GListY,"128"); GListY=g_list_append(GListY,"64"); GListY=g_list_append(GListY,"32"); 
    GListY=g_list_append(GListY,"16");
    gtk_combo_set_popdown_strings(GTK_COMBO(YChan),GListY);                                                           //Define the popup
    switch (Setup.Twod.YChan[i])                                                                                     //Set the initial entry
      {
      case 8192:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"8 K"); break;
      case 4096:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"4 K"); break;
      case 2048:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"2 K"); break;
      case 1024:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"1 K"); break;
      case 512:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"512"); break;
      case 256:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"256"); break;
      case 128:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"128"); break;
      case 64:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"64");  break;
      case 32:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"32");  break;
      case 16:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"16");  break;
      default:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(YChan)->entry),"1 K");
      }
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(YChan)->entry),"changed",GTK_SIGNAL_FUNC(TwodYChanChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),YChan,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    TwodNGates1=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Twod.Gate1[i].NGates); gtk_entry_set_text(GTK_ENTRY(TwodNGates1),Str);
    gtk_signal_connect(GTK_OBJECT(TwodNGates1),"changed",GTK_SIGNAL_FUNC(TwodNGates1Changed),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(TwodNGates1,ColWidth[5],-1);
    gtk_table_attach(GTK_TABLE(Table),TwodNGates1,5,6,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
 
    if (Setup.Twod.Gate1[i].Cond == And) strcpy(Str,"AND"); else strcpy(Str,"OR"); CBut1=gtk_button_new_with_label(Str);
    gtk_signal_connect(GTK_OBJECT(CBut1),"clicked",GTK_SIGNAL_FUNC(TwodCond1),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(CBut1,ColWidth[6],-1);
    gtk_table_attach(GTK_TABLE(Table),CBut1,6,7,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Def1=gtk_button_new_with_label("Define"); 
    gtk_signal_connect(GTK_OBJECT(Def1),"clicked",GTK_SIGNAL_FUNC(TwodGates1d),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(Def1,ColWidth[7],-1);
    gtk_table_attach(GTK_TABLE(Table),Def1,7,8,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    TwodNGates2=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    sprintf(Str,"%d",Setup.Twod.Gate2[i].NGates); gtk_entry_set_text(GTK_ENTRY(TwodNGates2),Str);
    gtk_signal_connect(GTK_OBJECT(TwodNGates2),"changed",GTK_SIGNAL_FUNC(TwodNGates2Changed),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(TwodNGates2,ColWidth[8],-1);
    gtk_table_attach(GTK_TABLE(Table),TwodNGates2,8,9,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    if (Setup.Twod.Gate2[i].Cond == And) strcpy(Str,"AND"); else strcpy(Str,"OR"); CBut2=gtk_button_new_with_label(Str);
    gtk_signal_connect(GTK_OBJECT(CBut2),"clicked",GTK_SIGNAL_FUNC(TwodCond2),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(CBut2,ColWidth[9],-1);
    gtk_table_attach(GTK_TABLE(Table),CBut2,9,10,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Def2=gtk_button_new_with_label("Define"); 
    gtk_signal_connect(GTK_OBJECT(Def2),"clicked",GTK_SIGNAL_FUNC(TwodGates2d),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(Def2,ColWidth[10],-1);
    gtk_table_attach(GTK_TABLE(Table),Def2,10,11,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
    gtk_entry_set_text(GTK_ENTRY(Entry),Setup.Twod.Name[i]);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(Name2dChanged),GINT_TO_POINTER(i));
    gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[11],-1);
    gtk_table_attach(GTK_TABLE(Table),Entry,11,12,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,8);
Label=gtk_label_new("Global Shortcut:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Label=gtk_label_new(" Set ranges e.g. 1-20 or 1-40 step 2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
SetStyleRecursively(Label,BoldStyle);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("No.\nRange"); gtk_widget_set_size_request(But,100,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,2);
But=gtk_button_new_with_label("X Para\nRange"); gtk_widget_set_size_request(But,100,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,2);
But=gtk_button_new_with_label("Y Para\nRange"); gtk_widget_set_size_request(But,100,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,2);
But=gtk_button_new_with_label("X\nSize"); gtk_widget_set_size_request(But,ColWidth[3],-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,2);
But=gtk_button_new_with_label("Y\nSize"); gtk_widget_set_size_request(But,ColWidth[4],-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,2);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
WArray[0]=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(WArray[0],100,-1);
gtk_box_pack_start(GTK_BOX(HBox),WArray[0],FALSE,FALSE,2);
WArray[1]=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(WArray[1],100,-1);
gtk_box_pack_start(GTK_BOX(HBox),WArray[1],FALSE,FALSE,2);
WArray[2]=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(WArray[2],100,-1);
gtk_box_pack_start(GTK_BOX(HBox),WArray[2],FALSE,FALSE,2);
WArray[3]=gtk_combo_new(); gtk_widget_set_size_request(WArray[3],ColWidth[3],-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),FALSE);
gtk_combo_set_popdown_strings(GTK_COMBO(WArray[3]),GListX);
switch (GlobXSize)
  {
  case 8192:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"8 K"); break;
  case 4096:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"4 K"); break;
  case 2048:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"2 K"); break;
  case 1024:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"1 K"); break;
  case 512:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"512"); break;
  case 256:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"256"); break;
  case 128:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"128"); break;
  case 64:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"64");  break;
  case 32:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"32");  break;
  case 16:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"16");  break;
  default:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[3])->entry),"1 K");
  }
gtk_box_pack_start(GTK_BOX(HBox),WArray[3],FALSE,FALSE,2);

WArray[4]=gtk_combo_new(); gtk_widget_set_size_request(WArray[4],ColWidth[4],-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),FALSE);
gtk_combo_set_popdown_strings(GTK_COMBO(WArray[4]),GListY);
switch (GlobYSize)
  {
  case 8192:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"8 K"); break;
  case 4096:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"4 K"); break;
  case 2048:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"2 K"); break;
  case 1024:  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"1 K"); break;
  case 512:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"512"); break;
  case 256:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"256"); break;
  case 128:   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"128"); break;
  case 64:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"64");  break;
  case 32:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"32");  break;
  case 16:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"16");  break;
  default:    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(WArray[4])->entry),"1 K");
  }
gtk_box_pack_start(GTK_BOX(HBox),WArray[4],FALSE,FALSE,2);

But=gtk_button_new_with_label("Apply");
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,2);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Glob2),Table);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);                      //HBox for OK Button
gtk_container_set_border_width(GTK_CONTAINER(HBox),5);                                                    //Breathing room
OKBut=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),OKBut,TRUE,FALSE,0);            //Define OK Button
gtk_signal_connect(GTK_OBJECT(OKBut),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);             //Connect signal for OK Button

gtk_widget_show_all(Win);
gtk_style_unref(HeadingStyle); gtk_style_unref(BoldStyle); g_list_free(GListX); g_list_free(GListY);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void N1Changed(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Oned.N=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void N2Changed(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Twod.N=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ToggleSpectraSafety(GtkWidget *CheckButton,GtkWidget *SafetyCombo)  //Call back from SpectraSetup: turns Safety = 0,1
{
if (GTK_TOGGLE_BUTTON(CheckButton)->active) 
   { 
   Setup.Spectra.Safety=1; gtk_label_set_text(GTK_LABEL(GTK_BIN(CheckButton)->child),"On "); 
   gtk_widget_set_sensitive(SafetyCombo,TRUE); 
   }
else
   { 
   Setup.Spectra.Safety=0; gtk_label_set_text(GTK_LABEL(GTK_BIN(CheckButton)->child),"Off"); 
   gtk_widget_set_sensitive(SafetyCombo,FALSE);
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SpecSafetyCallback(GtkWidget *W,gpointer Data)                                                         /*Callback from SpectraSetup*/
{ Setup.Spectra.SafetyTime=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void SpecWSz1(GtkWidget *W,gpointer Data)                                                                   /*Callback from SpectraSetup*/
{
if (Setup.Oned.WSz==1) { Setup.Oned.WSz=2; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Double\nWord"); }
else                   { Setup.Oned.WSz=1; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Single\nWord"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SpecWSz2(GtkWidget *W,gpointer Data)                                                                   /*Callback from SpectraSetup*/
{
if (Setup.Twod.WSz==1) { Setup.Twod.WSz=2; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Double\nWord"); }
else                   { Setup.Twod.WSz=1; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Single\nWord"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SpectraSetup(GtkWidget *VBox)
{
GtkWidget *VBox0,*HBox,*Label,*N1Spin,*N2Spin,*But1,*But2,*Frame,*HBoxA,*HBoxB,*CheckButton,*SafetyCombo,*Tog1,*Tog2;
gchar Str[40];
GList *GList;
GtkAdjustment *Adj;
static GdkColor FrameBg  = {0,0x7777,0x7777,0x7777};
static GdkColor FrameFg  = {0,0xDDDD,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;

FrameStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }

Frame=gtk_frame_new("SPECTRA SETTINGS"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.5);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox0=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Frame),VBox0);
gtk_container_set_border_width(GTK_CONTAINER(VBox0),5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox0),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No of 1d Spec."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
Adj=(GtkAdjustment *)gtk_adjustment_new(Setup.Oned.N,0,MAX_1D,1,5,0);
N1Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(N1Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(N1Spin),TRUE); gtk_widget_set_size_request(N1Spin,52,-1);
gtk_box_pack_start(GTK_BOX(HBox),N1Spin,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(N1Changed),(gpointer)N1Spin);
if (Setup.Oned.WSz==1) strcpy(Str,"Single\nWord"); else strcpy(Str,"Double\nWord");
Tog1=gtk_toggle_button_new_with_label(Str); gtk_box_pack_start(GTK_BOX(HBox),Tog1,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Tog1),"toggled",GTK_SIGNAL_FUNC(SpecWSz1),NULL);
But1=gtk_button_new_with_label("Define"); gtk_box_pack_start(GTK_BOX(HBox),But1,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But1),"clicked",GTK_SIGNAL_FUNC(SetOned),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox0),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No of 2d Spec."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,5);
Adj=(GtkAdjustment *)gtk_adjustment_new(Setup.Twod.N,0,MAX_2D,1,5,0);
N2Spin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(N2Spin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(N2Spin),TRUE); gtk_widget_set_size_request(N2Spin,52,-1);
gtk_box_pack_start(GTK_BOX(HBox),N2Spin,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(N2Changed),(gpointer)N2Spin);
if (Setup.Twod.WSz==1) strcpy(Str,"Single\nWord"); else strcpy(Str,"Double\nWord");
Tog2=gtk_toggle_button_new_with_label(Str); gtk_box_pack_start(GTK_BOX(HBox),Tog2,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Tog2),"toggled",GTK_SIGNAL_FUNC(SpecWSz2),NULL);
But2=gtk_button_new_with_label("Define"); gtk_box_pack_start(GTK_BOX(HBox),But2,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But2),"clicked",GTK_SIGNAL_FUNC(SetTwod),NULL);

HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(VBox0),HBox);
Frame=gtk_frame_new("Safety");
gtk_box_pack_start(GTK_BOX(HBox),Frame,TRUE,FALSE,0);
HBox=gtk_hbox_new(FALSE,8); gtk_container_add(GTK_CONTAINER(Frame),HBox);
HBoxA=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),HBoxA,FALSE,FALSE,0);
HBoxB=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),HBoxB,FALSE,FALSE,0);
if (Setup.Spectra.Safety) { CheckButton=gtk_check_button_new_with_label("On "); GTK_TOGGLE_BUTTON(CheckButton)->active=TRUE;  }
                     else { CheckButton=gtk_check_button_new_with_label("Off"); GTK_TOGGLE_BUTTON(CheckButton)->active=FALSE; }
gtk_box_pack_start(GTK_BOX(HBoxA),CheckButton,FALSE,FALSE,0);
SafetyCombo=gtk_combo_new(); gtk_box_pack_start(GTK_BOX(HBoxB),SafetyCombo,FALSE,FALSE,0);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(SafetyCombo)->entry),FALSE);
gtk_widget_set_size_request(GTK_WIDGET(SafetyCombo),96,-1);
gtk_signal_connect(GTK_OBJECT(CheckButton),"toggled",GTK_SIGNAL_FUNC(ToggleSpectraSafety),SafetyCombo);
GList=NULL; GList=g_list_append(GList,"10 min"); GList=g_list_append(GList,"15 min");
GList=g_list_append(GList,"30 min"); GList=g_list_append(GList,"60 min"); GList=g_list_append(GList,"120 min");
gtk_combo_set_popdown_strings(GTK_COMBO(SafetyCombo),GList);
Setup.Spectra.SafetyTime=CLAMP(Setup.Spectra.SafetyTime,10,120);
sprintf(Str,"%d min",Setup.Spectra.SafetyTime);
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(SafetyCombo)->entry),Str);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(SafetyCombo)->entry),"changed",GTK_SIGNAL_FUNC(SpecSafetyCallback),NULL);  
if (Setup.Spectra.Safety) gtk_widget_set_sensitive(SafetyCombo,TRUE); else gtk_widget_set_sensitive(SafetyCombo,FALSE);

g_list_free(GList);
gtk_style_unref(FrameStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void K1Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.K1[GPOINTER_TO_INT(Data)]=(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(W)));                                         /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void O1Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.O1[GPOINTER_TO_INT(Data)]=(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(W)));                                          /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void K2Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.K2[GPOINTER_TO_INT(Data)]=(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(W)));                                          /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void O2Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.O2[GPOINTER_TO_INT(Data)]=(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(W)));                                          /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void K3Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.K3[GPOINTER_TO_INT(Data)]=(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(W)));                                          /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void O3Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.O3[GPOINTER_TO_INT(Data)]=(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(W)));                                          /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PowerChanged(GtkWidget *W,gpointer Data)                                                               /*Callback from DefinePseudo*/
{
Setup.Pseudo.Power[GPOINTER_TO_INT(Data)]=(gfloat)atof(gtk_entry_get_text(GTK_ENTRY(W)));                                        /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void L1Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.L1[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));                                                  /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void L2Changed(GtkWidget *W,gpointer Data)                                                                  /*Callback from DefinePseudo*/
{
Setup.Pseudo.L2[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));                                                  /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ArrayQuadChanged(GtkWidget *W,gpointer Data)
{
gint Code,PseudoNo,Element;

Code=GPOINTER_TO_INT(Data); PseudoNo=Code/MAX_ARRAY; Element=Code%MAX_ARRAY;
Setup.Pseudo.ArrayQuad[Element][PseudoNo]=atof(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ArraySlopeChanged(GtkWidget *W,gpointer Data)
{
gint Code,PseudoNo,Element;

Code=GPOINTER_TO_INT(Data); PseudoNo=Code/MAX_ARRAY; Element=Code%MAX_ARRAY;
Setup.Pseudo.ArraySlope[Element][PseudoNo]=atof(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ArrayOffsetChanged(GtkWidget *W,gpointer Data)
{
gint Code,PseudoNo,Element;

Code=GPOINTER_TO_INT(Data); PseudoNo=Code/MAX_ARRAY; Element=Code%MAX_ARRAY;
Setup.Pseudo.ArrayOffset[Element][PseudoNo]=atof(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ArrayULDChanged(GtkWidget *W,gpointer Data)
{
gint Code,PseudoNo,Element;

Code=GPOINTER_TO_INT(Data); PseudoNo=Code/MAX_ARRAY; Element=Code%MAX_ARRAY;
Setup.Pseudo.ArrayULD[Element][PseudoNo]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ArrayLLDChanged(GtkWidget *W,gpointer Data)
{
gint Code,PseudoNo,Element;

Code=GPOINTER_TO_INT(Data); PseudoNo=Code/MAX_ARRAY; Element=Code%MAX_ARRAY;
Setup.Pseudo.ArrayLLD[Element][PseudoNo]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ArrayParChanged(GtkWidget *W,gpointer Data)
{
gint Code,PseudoNo,Element;

Code=GPOINTER_TO_INT(Data); PseudoNo=Code/MAX_ARRAY; Element=Code%MAX_ARRAY;
Setup.Pseudo.ArrayPar[Element][PseudoNo]=CLAMP(atoi(gtk_entry_get_text(GTK_ENTRY(W))),1,Setup.Parameter.NPar+PseudoNo);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ArrayNChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Pseudo.ArrayN[PsNo]=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectPseudoBGate(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_TEXT_FIELD+5];
                                                                                                                             
sprintf(PsBGated[PsNo].Name,"%s/%s",FileX->Path,FileX->TargetFile);                                      //Store full path
AbbreviateFileName(Str,PsBGated[PsNo].Name,MAX_TEXT_FIELD);
gtk_label_set_text(GTK_LABEL(GTK_BIN(GlobalW)->child),Str);                                    //Change text inside button
strcpy(BanDir,FileX->Path); SavePrefs();
g_free(FileX);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BGatedPseudoClicked(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select Banana File",EdWin,40,100,TRUE,".",".ban",TRUE,&SelectPseudoBGate,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DefinePseudo(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*HBox,*VBox,*OKBut,*Label,*But,*PixW,*K1Entry,*K2Entry,*K3Entry,*O1Entry,*O2Entry,*O3Entry,
          *PowerEntry,*L1Entry,*L2Entry,*Spin,*Table,*ScrollW,*Entry,*VBox2;
GtkAdjustment *Adj;
GdkPixmap *PixMap;
GdkBitmap *Mask;
GtkStyle *Style,*HeadingStyle,*RedStyle;
gint i,j;
static gchar PseudoName[9][15]={"Sum","Product","Ratio","Position","PI","Multip.","User","Array","BGated"};
static gchar Heading[7][25]= {"No","Para","LLD","ULD","Offset","Slope","Quad"};
gint ColWidth[7]={20,40,40,40,80,80,80};
static GdkColor HeadingBg  = {0,0x0000,0xA000,0xA000};                                           //Colour for HeadingStyle
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};                                           //Colour for HeadingStyle
static GdkColor RedBg  = {0,0xFFFF,0xFFFF,0xFFFF};                                                   //Colour for RedStyle
static GdkColor RedFg  = {0,0xFFFF,0x0000,0x0000};                                                   //Colour for RedStyle
gchar Str[40],Str2[MAX_TEXT_FIELD+5];

#include "Sum.xpm"
#include "Product.xpm"
#include "Ratio.xpm"
#include "Position.xpm"
#include "Pi.xpm"
#include "Multiplicity.xpm"
#include "Array.xpm"

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }
RedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { RedStyle->fg[i]=RedStyle->text[i]=RedFg; RedStyle->bg[i]=RedBg; }

i=GPOINTER_TO_INT(Data);
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                 //Define Win and make it modal
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                      //Ensure visibility of modal window
gtk_window_set_title(GTK_WINDOW(Win),"Define Pseudo"); gtk_widget_set_uposition(GTK_WIDGET(Win),550,100);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);

gtk_widget_show(Win); Style=gtk_widget_get_style(Win);
switch (Setup.Pseudo.Type[i])
   {
   case Sum:
        PixMap=gdk_pixmap_create_from_xpm_d(Win->window,&Mask,&Style->bg[GTK_STATE_NORMAL],(gchar **)Sum_xpm); break;
   case Product:      
        PixMap=gdk_pixmap_create_from_xpm_d(Win->window,&Mask,&Style->bg[GTK_STATE_NORMAL],(gchar **)Product_xpm); break;
   case Ratio:
        PixMap=gdk_pixmap_create_from_xpm_d(Win->window,&Mask,&Style->bg[GTK_STATE_NORMAL],(gchar **)Ratio_xpm); break;
   case Position:
        PixMap=gdk_pixmap_create_from_xpm_d(Win->window,&Mask,&Style->bg[GTK_STATE_NORMAL],(gchar **)Position_xpm); break;
   case PI:
        PixMap=gdk_pixmap_create_from_xpm_d(Win->window,&Mask,&Style->bg[GTK_STATE_NORMAL],(gchar **)Pi_xpm); break;
   case Multiplicity:
        PixMap=gdk_pixmap_create_from_xpm_d(Win->window,&Mask,&Style->bg[GTK_STATE_NORMAL],(gchar **)Multiplicity_xpm); 
        break;
   case User: case BGated: break;
   case Array:
        gtk_widget_set_usize(GTK_WIDGET(Win),458,300);
        PixMap=gdk_pixmap_create_from_xpm_d(Win->window,&Mask,&Style->bg[GTK_STATE_NORMAL],(gchar **)Array_xpm); break;
   }

VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
sprintf(Str,"Type: %s",PseudoName[Setup.Pseudo.Type[i]]);
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
if ( (Setup.Pseudo.Type[i]!=User) && (Setup.Pseudo.Type[i]!=BGated) )
   {
   HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
   PixW=gtk_pixmap_new(PixMap,Mask);
   But=gtk_button_new(); gtk_container_add(GTK_CONTAINER(But),PixW);
   gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
   }

switch (Setup.Pseudo.Type[i])
  {
  case User:
       VBox2=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox2,FALSE,FALSE,5);
       Label=gtk_label_new("Pseudo defined in user.F");
       SetStyleRecursively(Label,RedStyle); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
       Label=gtk_label_new("Editing user.F and 'make' should have been completed beforehand"); 
       SetStyleRecursively(Label,RedStyle); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
       break;
  case Sum: case Product: case Ratio: case Position: 
       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  K1"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       K1Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(K1Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.K1[i]); gtk_entry_set_text(GTK_ENTRY(K1Entry),Str);
       gtk_signal_connect(GTK_OBJECT(K1Entry),"changed",GTK_SIGNAL_FUNC(K1Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),K1Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  O1"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O1Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O1Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O1[i]); gtk_entry_set_text(GTK_ENTRY(O1Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O1Entry),"changed",GTK_SIGNAL_FUNC(O1Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O1Entry,FALSE,FALSE,0);

       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  K2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       K2Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(K2Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.K2[i]); gtk_entry_set_text(GTK_ENTRY(K2Entry),Str);
       gtk_signal_connect(GTK_OBJECT(K2Entry),"changed",GTK_SIGNAL_FUNC(K2Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),K2Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  O2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O2Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O2Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O2[i]); gtk_entry_set_text(GTK_ENTRY(O2Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O2Entry),"changed",GTK_SIGNAL_FUNC(O2Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O2Entry,FALSE,FALSE,0);

       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  K3"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       K3Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(K3Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.K3[i]); gtk_entry_set_text(GTK_ENTRY(K3Entry),Str);
       gtk_signal_connect(GTK_OBJECT(K3Entry),"changed",GTK_SIGNAL_FUNC(K3Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),K3Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  O3"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O3Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O3Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O3[i]); gtk_entry_set_text(GTK_ENTRY(O3Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O3Entry),"changed",GTK_SIGNAL_FUNC(O3Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O3Entry,FALSE,FALSE,0);
       break;
  case BGated:
       VBox2=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox2,FALSE,FALSE,5);
       Label=gtk_label_new("When the defined banana gate is FALSE the Pseudo will be set to 0"); 
       SetStyleRecursively(Label,RedStyle); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
       Label=gtk_label_new("Otherwise Pseudo will be set equal to Para1. Para2 is unused"); 
       SetStyleRecursively(Label,RedStyle); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
       Label=gtk_label_new("This pseudo can be referenced inside user.F to test a banana gate condition"); 
       SetStyleRecursively(Label,RedStyle); gtk_box_pack_start(GTK_BOX(VBox2),Label,FALSE,FALSE,0);
       PsNo=i; HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("Banana Gate File Name"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);  
       if (!strlen(PsBGated[i].Name)) GlobalW=gtk_button_new_with_label("Browse");
       else
          {
          AbbreviateFileName(Str2,PsBGated[i].Name,MAX_TEXT_FIELD);
          GlobalW=gtk_button_new_with_label(Str2);
          }
       gtk_box_pack_start(GTK_BOX(HBox),GlobalW,FALSE,FALSE,10);
       gtk_signal_connect(GTK_OBJECT(GlobalW),"clicked",GTK_SIGNAL_FUNC(BGatedPseudoClicked),NULL);
       break;
  case PI:
       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  K1"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       K1Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(K1Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.K1[i]); gtk_entry_set_text(GTK_ENTRY(K1Entry),Str);
       gtk_signal_connect(GTK_OBJECT(K1Entry),"changed",GTK_SIGNAL_FUNC(K1Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),K1Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  O1"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O1Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O1Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O1[i]); gtk_entry_set_text(GTK_ENTRY(O1Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O1Entry),"changed",GTK_SIGNAL_FUNC(O1Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O1Entry,FALSE,FALSE,0);

       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  K2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       K2Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(K2Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.K2[i]); gtk_entry_set_text(GTK_ENTRY(K2Entry),Str);
       gtk_signal_connect(GTK_OBJECT(K2Entry),"changed",GTK_SIGNAL_FUNC(K2Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),K2Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  O2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O2Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O2Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O2[i]); gtk_entry_set_text(GTK_ENTRY(O2Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O2Entry),"changed",GTK_SIGNAL_FUNC(O2Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O2Entry,FALSE,FALSE,0);

       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  K3"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       K3Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(K3Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.K3[i]); gtk_entry_set_text(GTK_ENTRY(K3Entry),Str);
       gtk_signal_connect(GTK_OBJECT(K3Entry),"changed",GTK_SIGNAL_FUNC(K3Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),K3Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  O3"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O3Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O3Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O3[i]); gtk_entry_set_text(GTK_ENTRY(O3Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O3Entry),"changed",GTK_SIGNAL_FUNC(O3Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O3Entry,FALSE,FALSE,0);
   
       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  Power"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       PowerEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(PowerEntry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.Power[i]); gtk_entry_set_text(GTK_ENTRY(PowerEntry),Str);
       gtk_signal_connect(GTK_OBJECT(PowerEntry),"changed",GTK_SIGNAL_FUNC(PowerChanged),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),PowerEntry,FALSE,FALSE,0);
       break;
  case Multiplicity:
       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  L1"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       L1Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(L1Entry,80,22);
       sprintf(Str,"%d",Setup.Pseudo.L1[i]); gtk_entry_set_text(GTK_ENTRY(L1Entry),Str);
       gtk_signal_connect(GTK_OBJECT(L1Entry),"changed",GTK_SIGNAL_FUNC(L1Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),L1Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  L2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       L2Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(L2Entry,80,22);
       sprintf(Str,"%d",Setup.Pseudo.L2[i]); gtk_entry_set_text(GTK_ENTRY(L2Entry),Str);
       gtk_signal_connect(GTK_OBJECT(L2Entry),"changed",GTK_SIGNAL_FUNC(L2Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),L2Entry,FALSE,FALSE,0); 

       HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("  O1"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O1Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O1Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O1[i]); gtk_entry_set_text(GTK_ENTRY(O1Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O1Entry),"changed",GTK_SIGNAL_FUNC(O1Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O1Entry,FALSE,FALSE,0);
       Label=gtk_label_new("  O2"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       O2Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_usize(O2Entry,80,22);
       sprintf(Str,"%g",Setup.Pseudo.O2[i]); gtk_entry_set_text(GTK_ENTRY(O2Entry),Str);
       gtk_signal_connect(GTK_OBJECT(O2Entry),"changed",GTK_SIGNAL_FUNC(O2Changed),GINT_TO_POINTER(i));
       gtk_box_pack_start(GTK_BOX(HBox),O2Entry,FALSE,FALSE,0); 
       break;
  case Array:
       PsNo=i; HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
       Label=gtk_label_new("No of Array Elements:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
       Setup.Pseudo.ArrayN[i]=CLAMP(Setup.Pseudo.ArrayN[i],1,MAX_ARRAY);
       Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.Pseudo.ArrayN[i],1.0,(gfloat)MAX_ARRAY,1.0,2.0,0.0));
       Spin=gtk_spin_button_new(Adj,0,0); gtk_box_pack_start(GTK_BOX(HBox),Spin,FALSE,FALSE,0);
       gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(ArrayNChanged),Spin);
       VBox2=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox2,FALSE,FALSE,0);
       ScrollW=gtk_scrolled_window_new(NULL,NULL); //gtk_container_set_border_width(GTK_CONTAINER(ScrollW),5);
       gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
       gtk_widget_set_usize(ScrollW,200,31);
       gtk_box_pack_start(GTK_BOX(VBox2),ScrollW,FALSE,FALSE,0);
       Table=gtk_table_new(1,7,FALSE);
       gtk_table_set_row_spacings(GTK_TABLE(Table),2); gtk_table_set_col_spacings(GTK_TABLE(Table),5);
       for (j=0;j<7;++j)
           {
           But=gtk_button_new_with_label(Heading[j]); SetStyleRecursively(But,HeadingStyle);
           gtk_widget_set_usize(But,ColWidth[j],22);
           gtk_table_attach(GTK_TABLE(Table),But,j,j+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
           }
       gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);
       ScrollW=gtk_scrolled_window_new(NULL,NULL);
       gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
       gtk_widget_set_usize(ScrollW,200,108);
       gtk_box_pack_start(GTK_BOX(VBox2),ScrollW,FALSE,FALSE,0);
       Table=gtk_table_new(MAX_ARRAY,7,FALSE);
       gtk_table_set_row_spacings(GTK_TABLE(Table),2); gtk_table_set_col_spacings(GTK_TABLE(Table),5);
       for (j=0;j<MAX_ARRAY;++j)
           {
           Label=gtk_label_new(""); gtk_widget_set_usize(Label,ColWidth[0],22); sprintf(Str,"%d",j+1);
           gtk_label_set_text(GTK_LABEL(Label),Str);
           gtk_table_attach(GTK_TABLE(Table),Label,0,1,j,j+1,GTK_FILL,GTK_SHRINK,0,0);

           Entry=gtk_entry_new_with_max_length(5); gtk_widget_set_usize(Entry,ColWidth[1],22);
           sprintf(Str,"%d",Setup.Pseudo.ArrayPar[j][i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
           gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ArrayParChanged),GINT_TO_POINTER(i*MAX_ARRAY+j));
           gtk_table_attach(GTK_TABLE(Table),Entry,1,2,j,j+1,GTK_FILL,GTK_SHRINK,0,0);

           Entry=gtk_entry_new_with_max_length(5); gtk_widget_set_usize(Entry,ColWidth[2],22);
           sprintf(Str,"%d",Setup.Pseudo.ArrayLLD[j][i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
           gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ArrayLLDChanged),GINT_TO_POINTER(i*MAX_ARRAY+j));
           gtk_table_attach(GTK_TABLE(Table),Entry,2,3,j,j+1,GTK_FILL,GTK_SHRINK,0,0);

           Entry=gtk_entry_new_with_max_length(5); gtk_widget_set_usize(Entry,ColWidth[3],22);
           sprintf(Str,"%d",Setup.Pseudo.ArrayULD[j][i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
           gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ArrayULDChanged),GINT_TO_POINTER(i*MAX_ARRAY+j));
           gtk_table_attach(GTK_TABLE(Table),Entry,3,4,j,j+1,GTK_FILL,GTK_SHRINK,0,0);

           Entry=gtk_entry_new_with_max_length(12); gtk_widget_set_usize(Entry,ColWidth[4],22);
           sprintf(Str,"%g",Setup.Pseudo.ArrayOffset[j][i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
           gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ArrayOffsetChanged),GINT_TO_POINTER(i*MAX_ARRAY+j));
           gtk_table_attach(GTK_TABLE(Table),Entry,4,5,j,j+1,GTK_FILL,GTK_SHRINK,0,0);

           Entry=gtk_entry_new_with_max_length(12); gtk_widget_set_usize(Entry,ColWidth[5],22);
           sprintf(Str,"%g",Setup.Pseudo.ArraySlope[j][i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
           gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ArraySlopeChanged),GINT_TO_POINTER(i*MAX_ARRAY+j));
           gtk_table_attach(GTK_TABLE(Table),Entry,5,6,j,j+1,GTK_FILL,GTK_SHRINK,0,0);

           Entry=gtk_entry_new_with_max_length(12); gtk_widget_set_usize(Entry,ColWidth[6],22);
           sprintf(Str,"%g",Setup.Pseudo.ArrayQuad[j][i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
           gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ArrayQuadChanged),GINT_TO_POINTER(i*MAX_ARRAY+j));
           gtk_table_attach(GTK_TABLE(Table),Entry,6,7,j,j+1,GTK_FILL,GTK_SHRINK,0,0);
           }
       gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);
       break; 
  }

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_end(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
OKBut=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),OKBut,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(OKBut),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win); gtk_style_unref(Style); gtk_style_unref(HeadingStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PseudoP1Changed(GtkWidget *W,gpointer Data)                                                              /*Call back from SetPseudo*/
{
Setup.Pseudo.P1[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));                                                   /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PseudoP2Changed(GtkWidget *W,gpointer Data)                                                              /*Call back from SetPseudo*/
{
Setup.Pseudo.P2[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));                                                   /*Store*/
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PseudoSizeChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
Setup.Pseudo.Size[i]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PseudoTypeChanged(GtkWidget *W,gpointer Data)
{
gint i;
const gchar *Str;

i=GPOINTER_TO_INT(Data);
Str=gtk_entry_get_text(GTK_ENTRY(W));
if (!strcmp(Str,"Sum"))      { Setup.Pseudo.Type[i]=0; return; }
if (!strcmp(Str,"Product"))  { Setup.Pseudo.Type[i]=1; return; }
if (!strcmp(Str,"Ratio"))    { Setup.Pseudo.Type[i]=2; return; }
if (!strcmp(Str,"Position")) { Setup.Pseudo.Type[i]=3; return; }
if (!strcmp(Str,"PI"))       { Setup.Pseudo.Type[i]=4; return; }
if (!strcmp(Str,"Multip."))  { Setup.Pseudo.Type[i]=5; return; }
if (!strcmp(Str,"User"))     { Setup.Pseudo.Type[i]=6; return; }      
if (!strcmp(Str,"Array"))    { Setup.Pseudo.Type[i]=7; return; }      
if (!strcmp(Str,"BGated"))   { Setup.Pseudo.Type[i]=8; return; }      
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PseudoNameChanged(GtkWidget *W,gpointer Data)
{
gint i;

i=GPOINTER_TO_INT(Data);
sprintf(Setup.Pseudo.Name[i],gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SetPseudo(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*VBox,*ScrollW,*Table,*HeadBut,*OKBut,*HBox,*NumBut,*Entry,*SizeCombo,*TypeCombo,*Def;
static gchar Heading[6][25]= {"No","Para 1","Para 2","Size","Type","Name"};
static GdkColor HeadingBg  = {0,0xA000,0x0000,0xA000};                                           //Colour for HeadingStyle
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};                                           //Colour for HeadingStyle
GtkStyle *HeadingStyle;
gint i;
GList *GList;
gchar Str[40];
static gchar PseudoName[9][15]={"Sum","Product","Ratio","Position","PI","Multip.","User","Array","BGated"};

if (!Setup.Pseudo.N) return;                                                                                    //No pseudos
HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());                              //Copy default style to this style
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; }      //Set colours

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                 //Define Win and make it modal
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));                       //Ensure visibility of modal window
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Set Pseudos"); gtk_widget_set_uposition(GTK_WIDGET(Win),120,100);
gtk_widget_set_size_request(Win,-1,220);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
ScrollW=gtk_scrolled_window_new(NULL,NULL); gtk_container_set_border_width(GTK_CONTAINER(ScrollW),5);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);
Table=gtk_table_new(Setup.Pseudo.N+1,7,FALSE);
for (i=0;i<6;++i)                                                                                 //Headings for each column
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }
for (i=0;i<Setup.Pseudo.N;i++)
    {
    sprintf(Str,"%2d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_table_attach(GTK_TABLE(Table),NumBut,0,1,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,50,-1);
    sprintf(Str,"%d",Setup.Pseudo.P1[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PseudoP1Changed),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),Entry,1,2,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,50,-1);
    sprintf(Str,"%d",Setup.Pseudo.P2[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PseudoP2Changed),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),Entry,2,3,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    SizeCombo=gtk_combo_new(); gtk_widget_set_size_request(SizeCombo,80,-1);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(SizeCombo)->entry),FALSE);
    GList=NULL;
    GList=g_list_append(GList,"16");   GList=g_list_append(GList,"32");   GList=g_list_append(GList,"64"); 
    GList=g_list_append(GList,"128");  GList=g_list_append(GList,"256");  GList=g_list_append(GList,"512"); 
    GList=g_list_append(GList,"1024"); GList=g_list_append(GList,"2048"); GList=g_list_append(GList,"4096");
    GList=g_list_append(GList,"8192"); GList=g_list_append(GList,"16384");
    gtk_combo_set_popdown_strings(GTK_COMBO(SizeCombo),GList);
    sprintf(Str,"%d",Setup.Pseudo.Size[i]);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(SizeCombo)->entry),Str);
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(SizeCombo)->entry),"changed",GTK_SIGNAL_FUNC(PseudoSizeChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),SizeCombo,3,4,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    TypeCombo=gtk_combo_new(); gtk_widget_set_size_request(TypeCombo,90,-1);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(TypeCombo)->entry),FALSE);
    GList=NULL;
    GList=g_list_append(GList,"Sum"); GList=g_list_append(GList,"Product"); GList=g_list_append(GList,"Ratio");
    GList=g_list_append(GList,"Position"); GList=g_list_append(GList,"PI"); GList=g_list_append(GList,"Multip.");
    GList=g_list_append(GList,"User");  GList=g_list_append(GList,"Array"); GList=g_list_append(GList,"BGated");
    gtk_combo_set_popdown_strings(GTK_COMBO(TypeCombo),GList);
    strcpy(Str,PseudoName[Setup.Pseudo.Type[i]]);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(TypeCombo)->entry),Str);
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(TypeCombo)->entry),"changed",GTK_SIGNAL_FUNC(PseudoTypeChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),TypeCombo,4,5,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(Entry,90,-1);
    gtk_entry_set_text(GTK_ENTRY(Entry),Setup.Pseudo.Name[i]);
    gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(PseudoNameChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),Entry,5,6,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);
    Def=gtk_button_new_with_label("Define");
    gtk_signal_connect(GTK_OBJECT(Def),"clicked",GTK_SIGNAL_FUNC(DefinePseudo),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),Def,6,7,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);
    }

gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);
HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
OKBut=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),OKBut,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(OKBut),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win); g_list_free(GList); gtk_style_unref(HeadingStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void NPChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Pseudo.N=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void PseudoSetup(GtkWidget *VBox)
{
GtkWidget *VBox0,*HBox,*NPSpin,*Label,*But,*Frame;
GtkAdjustment *Adj;
static GdkColor FrameBg  = {0,0x7777,0x7777,0x7777};
static GdkColor FrameFg  = {0,0xDDDD,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;

FrameStyle=gtk_style_copy(gtk_widget_get_default_style());                                          /*Copy default style to this style*/
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }   /*Set colours for all states*/

Frame=gtk_frame_new("PSEUDO PARAMETER SETTINGS"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.5);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox0=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Frame),VBox0);
gtk_container_set_border_width(GTK_CONTAINER(VBox0),5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox0),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Number of Pseudos"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
Adj=(GtkAdjustment *)gtk_adjustment_new(Setup.Pseudo.N,0,MAX_PSEUDO,1,5,0);
NPSpin=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(NPSpin),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(NPSpin),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),NPSpin,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(NPChanged),(gpointer)NPSpin);
But=gtk_button_new_with_label("Define"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SetPseudo),NULL);

gtk_style_unref(FrameStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ScalerNameChanged(GtkWidget *W,gpointer Data)
{ strcpy(Setup.Scaler.Name[GPOINTER_TO_INT(Data)],gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ScalerNChanged(GtkWidget *W,gpointer Data)
{ Setup.Scaler.N[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ScalerAChanged(GtkWidget *W,gpointer Data)
{ Setup.Scaler.A[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ScalerFChanged(GtkWidget *W,gpointer Data)
{ Setup.Scaler.F[GPOINTER_TO_INT(Data)]=atoi(gtk_entry_get_text(GTK_ENTRY(W))); }
/*----------------------------------------------------------------------------------------------------------------------*/
void SetScaler(GtkWidget *W,gpointer Data)
{
GtkWidget *Label,*Win,*VBox,*ScrollW,*Table,*HeadBut,*OKBut,*HBox,*NumBut,*NameEntry,*CEntry,*NEntry,*AEntry,*FEntry;

static gchar Heading[6][25]= {"No","Name","Crate","Stn No","Sub Addr","Function"};
static GdkColor HeadingBg  = {0,0x0000,0xA000,0xA000};
static GdkColor HeadingFg  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor LabelFg    = {0,0xE000,0x0000,0x0000};
GtkStyle *HeadingStyle,*LabelStyle;
gint i;
gchar Str[80];

HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());
LabelStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) 
    { 
    HeadingStyle->fg[i]=HeadingStyle->text[i]=HeadingFg; HeadingStyle->bg[i]=HeadingBg; 
    LabelStyle->fg[i]=LabelStyle->text[i]=LabelFg;
    }

for (i=0;i<Setup.Scaler.NSc;++i) { if (i<Setup.Scaler.NSc1) Setup.Scaler.C[i]=1; else Setup.Scaler.C[i]=2; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_transient_for(GTK_WINDOW(Win),GTK_WINDOW(EdWin));
gtk_signal_connect_object(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Set Scaler"); gtk_widget_set_uposition(GTK_WIDGET(Win),278,100);
gtk_widget_set_size_request(Win,-1,450);
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
Label=gtk_label_new("Note: Master gates should be counted in a scaler named 'Masters'\nThis will be used for Dead Time calculation"); 
SetStyleRecursively(Label,LabelStyle);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
ScrollW=gtk_scrolled_window_new(NULL,NULL); gtk_container_set_border_width(GTK_CONTAINER(ScrollW),5);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);
Table=gtk_table_new(Setup.Scaler.NSc+1,6,FALSE);
for (i=0;i<6;++i)
    {
    HeadBut=gtk_button_new_with_label(Heading[i]); SetStyleRecursively(HeadBut,HeadingStyle);
    gtk_table_attach(GTK_TABLE(Table),HeadBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }
for (i=0;i<Setup.Scaler.NSc;++i)
    {
    sprintf(Str,"%2d",i+1); NumBut=gtk_button_new_with_label(Str);
    gtk_table_attach(GTK_TABLE(Table),NumBut,0,1,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    NameEntry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD); gtk_widget_set_size_request(NameEntry,160,-1);
    sprintf(Str,"%s",Setup.Scaler.Name[i]); gtk_entry_set_text(GTK_ENTRY(NameEntry),Str);
    gtk_signal_connect(GTK_OBJECT(NameEntry),"changed",GTK_SIGNAL_FUNC(ScalerNameChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),NameEntry,1,2,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    CEntry=gtk_entry_new_with_max_length(2); gtk_widget_set_size_request(CEntry,20,-1);
    sprintf(Str,"%d",Setup.Scaler.C[i]); gtk_entry_set_text(GTK_ENTRY(CEntry),Str);
    gtk_entry_set_editable(GTK_ENTRY(CEntry),FALSE);
    gtk_table_attach(GTK_TABLE(Table),CEntry,2,3,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    NEntry=gtk_entry_new_with_max_length(2); gtk_widget_set_size_request(NEntry,20,-1);
    sprintf(Str,"%d",Setup.Scaler.N[i]); gtk_entry_set_text(GTK_ENTRY(NEntry),Str);
    gtk_signal_connect(GTK_OBJECT(NEntry),"changed",GTK_SIGNAL_FUNC(ScalerNChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),NEntry,3,4,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    AEntry=gtk_entry_new_with_max_length(2); gtk_widget_set_size_request(AEntry,20,-1);
    sprintf(Str,"%d",Setup.Scaler.A[i]); gtk_entry_set_text(GTK_ENTRY(AEntry),Str);
    gtk_signal_connect(GTK_OBJECT(AEntry),"changed",GTK_SIGNAL_FUNC(ScalerAChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),AEntry,4,5,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);

    FEntry=gtk_entry_new_with_max_length(2); gtk_widget_set_size_request(FEntry,20,-1);
    sprintf(Str,"%d",Setup.Scaler.F[i]); gtk_entry_set_text(GTK_ENTRY(FEntry),Str);
    gtk_signal_connect(GTK_OBJECT(FEntry),"changed",GTK_SIGNAL_FUNC(ScalerFChanged),GINT_TO_POINTER(i));
    gtk_table_attach(GTK_TABLE(Table),FEntry,5,6,i+1,i+2,GTK_FILL,GTK_SHRINK,0,0);
    }

gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
OKBut=gtk_button_new_with_label("OK"); gtk_box_pack_start(GTK_BOX(HBox),OKBut,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(OKBut),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

gtk_widget_show_all(Win);
gtk_style_unref(HeadingStyle); gtk_style_unref(LabelStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void NScChanged(GtkWidget *W,gpointer Data)
{ 
gchar Str[20];

Setup.Scaler.NSc=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ScalerWidget[0]));
if (Setup.Hardware.NCrates==2)  //Two crate case
   {
   if (Setup.Scaler.NSc<Setup.Scaler.NSc1)
      {
      Setup.Scaler.NSc=Setup.Scaler.NSc1;
      gtk_adjustment_set_value(GTK_ADJUSTMENT(Data),Setup.Scaler.NSc);
      }
   sprintf(Str,"%d",Setup.Scaler.NSc-Setup.Scaler.NSc1); gtk_entry_set_text(GTK_ENTRY(ScalerWidget[2]),Str);
   }
else                            //One crate case
   {
   Setup.Scaler.NSc1=Setup.Scaler.NSc;
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(ScalerWidget[1]),(gdouble)Setup.Scaler.NSc1);
   gtk_entry_set_text(GTK_ENTRY(ScalerWidget[2]),"0");
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void NSc1Changed(GtkWidget *W,gpointer Data)
{ 
gchar Str[20];

Setup.Scaler.NSc1=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ScalerWidget[1]));
if (Setup.Scaler.NSc1>Setup.Scaler.NSc)
   {
   Setup.Scaler.NSc1=Setup.Scaler.NSc; 
   gtk_adjustment_set_value(GTK_ADJUSTMENT(Data),Setup.Scaler.NSc1);
   }
sprintf(Str,"%d",Setup.Scaler.NSc-Setup.Scaler.NSc1); gtk_entry_set_text(GTK_ENTRY(ScalerWidget[2]),Str);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ScalerSetup(GtkWidget *VBox)
{
GtkWidget *HBox,*Label,*But,*Frame,*VBox0,*VBox1;
GtkAdjustment *Adj;
static GdkColor FrameBg  = {0,0x7777,0x7777,0x7777};
static GdkColor FrameFg  = {0,0xDDDD,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;
gchar Str[20];

ScalerWidget=g_new(GtkWidget *,3);                                                        //Memory for an array of 3 widgets   
FrameStyle=gtk_style_copy(gtk_widget_get_default_style());                                //Copy default style to this style
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }

Frame=gtk_frame_new("SCALERS SETUP"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.5);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
VBox0=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Frame),VBox0);
gtk_container_set_border_width(GTK_CONTAINER(VBox0),5);

HBox=gtk_hbox_new(FALSE,20); gtk_box_pack_start(GTK_BOX(VBox0),HBox,FALSE,FALSE,0);
VBox1=gtk_vbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(HBox),VBox1,FALSE,FALSE,0);
Label=gtk_label_new("Total"); gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(Setup.Scaler.NSc,0,MAX_SCALER,1,5,0);
ScalerWidget[0]=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(ScalerWidget[0]),TRUE);
gtk_box_pack_start(GTK_BOX(VBox1),ScalerWidget[0],FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(NScChanged),(gpointer)Adj);
VBox1=gtk_vbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(HBox),VBox1,FALSE,FALSE,0);
Label=gtk_label_new("Crate 1"); gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(Setup.Scaler.NSc1,0,MAX_SCALER,1,5,0);
ScalerWidget[1]=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(ScalerWidget[1]),TRUE);
gtk_box_pack_start(GTK_BOX(VBox1),ScalerWidget[1],FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(NSc1Changed),(gpointer)Adj);
if (Setup.Hardware.NCrates==1) gtk_widget_set_sensitive(ScalerWidget[1],FALSE);
VBox1=gtk_vbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(HBox),VBox1,FALSE,FALSE,0);
Label=gtk_label_new("Crate 2"); gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
ScalerWidget[2]=gtk_entry_new_with_max_length(2); gtk_widget_set_size_request(ScalerWidget[2],20,-1);
sprintf(Str,"%d",Setup.Scaler.NSc-Setup.Scaler.NSc1); gtk_entry_set_text(GTK_ENTRY(ScalerWidget[2]),Str);
gtk_entry_set_editable(GTK_ENTRY(ScalerWidget[2]),FALSE);
gtk_box_pack_start(GTK_BOX(VBox1),ScalerWidget[2],FALSE,FALSE,0);
if (Setup.Hardware.NCrates==1) gtk_widget_set_sensitive(ScalerWidget[2],FALSE);
But=gtk_button_new_with_label("Define"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SetScaler),NULL);

gtk_style_unref(FrameStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PLogToggle(GtkWidget *CheckBut,GtkWidget *Spin)
{
if (GTK_TOGGLE_BUTTON(CheckBut)->active)
   {
   Setup.PLogSetup.On=TRUE; gtk_label_set_text(GTK_LABEL(GTK_BIN(CheckBut)->child),"On "); 
   gtk_widget_set_sensitive(Spin,TRUE); 
   }
else
   { 
   Setup.PLogSetup.On=FALSE; gtk_label_set_text(GTK_LABEL(GTK_BIN(CheckBut)->child),"Off"); 
   gtk_widget_set_sensitive(Spin,FALSE); 
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PLogValue(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.PLogSetup.BufCount=gtk_spin_button_get_value_as_int(Spin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void PeriodicLogSetup(GtkWidget *VBox)
{
GtkWidget *HBox,*CheckBut,*Frame,*Spin,*Label;
GtkAdjustment *Adj;
static GdkColor FrameBg  = {0,0x7777,0x7777,0x7777};
static GdkColor FrameFg  = {0,0xDDDD,0x0000,0x0000};
GtkStyle *FrameStyle;
gint i;

FrameStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { FrameStyle->fg[i]=FrameStyle->text[i]=FrameFg; FrameStyle->bg[i]=FrameBg; }

Frame=gtk_frame_new("PERIODIC LOG"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.5);
SetStyleRecursively(Frame,FrameStyle);
gtk_box_pack_start(GTK_BOX(VBox),Frame,FALSE,FALSE,0);
HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Frame),HBox);
gtk_container_set_border_width(GTK_CONTAINER(HBox),5);

if (Setup.PLogSetup.On) { CheckBut=gtk_check_button_new_with_label("On "); GTK_TOGGLE_BUTTON(CheckBut)->active=TRUE;  }
                   else { CheckBut=gtk_check_button_new_with_label("Off"); GTK_TOGGLE_BUTTON(CheckBut)->active=FALSE; }
gtk_box_pack_start(GTK_BOX(HBox),CheckBut,FALSE,FALSE,0);
Label=gtk_label_new("buffers"); gtk_box_pack_end(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)Setup.PLogSetup.BufCount,1.0,8192.0,31.0,255.0,0.0));
Spin=gtk_spin_button_new(Adj,0.5,0); gtk_widget_set_size_request(GTK_WIDGET(Spin),60,-1);
gtk_box_pack_end(GTK_BOX(HBox),Spin,FALSE,FALSE,0);
if (Setup.PLogSetup.On) gtk_widget_set_sensitive(Spin,TRUE); else gtk_widget_set_sensitive(Spin,FALSE);
gtk_signal_connect(GTK_OBJECT(CheckBut),"toggled",GTK_SIGNAL_FUNC(PLogToggle),Spin);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(PLogValue),Spin);

gtk_style_unref(FrameStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void NCratesChanged(GtkWidget *W,gpointer Data)
{ 
gint i;

Setup.Hardware.NCrates=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
if (Setup.Hardware.NCrates==2) 
   {
   for (i=0;i<2;++i) gtk_widget_set_size_request(CrateScrollArray[i],-1,275);
   gtk_widget_set_sensitive(ScalerWidget[1],TRUE); gtk_widget_set_sensitive(ScalerWidget[2],TRUE);
   }
else 
   { 
   gtk_widget_set_size_request(CrateScrollArray[0],-1,530); gtk_widget_set_size_request(CrateScrollArray[1],-1,8); 
   Setup.Scaler.NSc1=Setup.Scaler.NSc;
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(ScalerWidget[1]),(gdouble)Setup.Scaler.NSc1);
   gtk_entry_set_text(GTK_ENTRY(ScalerWidget[2]),"0");
   gtk_widget_set_sensitive(ScalerWidget[1],FALSE); gtk_widget_set_sensitive(ScalerWidget[2],FALSE);
   }
}
//----------------------------------------------------------------------------------------------------------------------
void CamacModeChanged(GtkWidget *W,gpointer Data)
{
if (strcmp("Normal Mode",gtk_entry_get_text(GTK_ENTRY(W)))) Setup.Hardware.CamacMode=1; else Setup.Hardware.CamacMode=0;
}
//----------------------------------------------------------------------------------------------------------------------
void GtSupStnChanged(GtkWidget *W,GtkSpinButton *Spin)
{ Setup.Hardware.GtSupStn=gtk_spin_button_get_value_as_int(Spin); }
//----------------------------------------------------------------------------------------------------------------------
void ToggleUseGtSup(GtkWidget *W,gpointer Unused)
{ if (GTK_TOGGLE_BUTTON(W)->active) Setup.Hardware.UseGtSup=TRUE; else Setup.Hardware.UseGtSup=FALSE; }
//----------------------------------------------------------------------------------------------------------------------
void EditSetup(GtkWidget *W,gpointer Data)
{
GtkWidget *HBox,*VBox0,*VBox,*But,*Label,*Combo,*HBox1,*HBox2,*CheckButton,*Spin;
GtkAdjustment *Adj;
GList *GList;
gchar Str[40];

if (ProgramState != Free) { Attention(0,"EditSetup is not allowed in the current Program State"); return; }
CloseAllSpecWindows(NULL,NULL);
ProgramState=DoingSetup; 
Setup.Modified=TRUE;                        //We set this flag though user may exit EditSetup without making any changes!
CrateScrollArray=g_new(GtkWidget *,2);
EdWin=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_title(GTK_WINDOW(EdWin),"Edit Setup"); 
gtk_signal_connect(GTK_OBJECT(EdWin),"destroy",GTK_SIGNAL_FUNC(EdWinDestroyed),NULL);
gtk_container_set_border_width(GTK_CONTAINER(EdWin),4);
HBox=gtk_hbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(EdWin),HBox);                     //HBox for the whole window

VBox=gtk_vbox_new(FALSE,2); gtk_box_pack_start(GTK_BOX(HBox),VBox,FALSE,FALSE,0);                   //VBox for Crate Setup

if (!Setup.Parameter.IgnoreCamac)
   {
   HBox1=gtk_hbox_new(FALSE,15); gtk_box_pack_start(GTK_BOX(VBox),HBox1,FALSE,FALSE,0);

   HBox2=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox2,FALSE,FALSE,0);
   Label=gtk_label_new("No of Crates"); gtk_box_pack_start(GTK_BOX(HBox2),Label,FALSE,FALSE,0);
   Combo=gtk_combo_new(); gtk_widget_set_size_request(Combo,50,-1);
   gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),FALSE);
   GList=NULL; GList=g_list_append(GList,"1"); GList=g_list_append(GList,"2");
   gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
   sprintf(Str,"%d",Setup.Hardware.NCrates);
   gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);
   gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(NCratesChanged),NULL);
   gtk_box_pack_start(GTK_BOX(HBox2),Combo,FALSE,FALSE,0);

   HBox2=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox2,FALSE,FALSE,0);
   Label=gtk_label_new("Camac Mode"); gtk_box_pack_start(GTK_BOX(HBox2),Label,FALSE,FALSE,0);
   Combo=gtk_combo_new(); gtk_widget_set_size_request(Combo,120,-1);
   gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),FALSE);
   GList=NULL; GList=g_list_append(GList,"Normal Mode"); GList=g_list_append(GList,"Q-Stop Mode");
   gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
   if (Setup.Hardware.CamacMode) gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Q-Stop Mode"); 
   else                          gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Normal Mode");
   gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(CamacModeChanged),NULL);
   gtk_box_pack_start(GTK_BOX(HBox2),Combo,FALSE,FALSE,0);

   HBox2=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox2,FALSE,FALSE,0);
   CheckButton=gtk_check_button_new(); gtk_box_pack_start(GTK_BOX(HBox2),CheckButton,FALSE,FALSE,0);
   if (Setup.Hardware.UseGtSup) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckButton),TRUE);
                           else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckButton),FALSE);
   g_signal_connect(GTK_OBJECT(CheckButton),"toggled",G_CALLBACK(ToggleUseGtSup),NULL);
   Label=gtk_label_new("CM90 Gate Suppressor at StnNo:"); gtk_box_pack_start(GTK_BOX(HBox2),Label,FALSE,FALSE,0);
   Adj=(GtkAdjustment *)gtk_adjustment_new(Setup.Hardware.GtSupStn,1,23,1,5,0);
   Spin=gtk_spin_button_new(Adj,0.5,0);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin),TRUE); gtk_widget_set_size_request(Spin,48,-1);
   gtk_box_pack_start(GTK_BOX(HBox2),Spin,FALSE,FALSE,0);
   g_signal_connect(GTK_OBJECT(Adj),"value_changed",G_CALLBACK(GtSupStnChanged),(gpointer)Spin);

   CrateSetup(VBox,1); CrateSetup(VBox,2);
   }

VBox0=gtk_vbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(HBox),VBox0,FALSE,FALSE,0);
VBox=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox0),VBox,FALSE,FALSE,0); ListModeSetup(VBox); 
VBox=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox0),VBox,FALSE,FALSE,0); SpectraSetup(VBox);
VBox=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox0),VBox,FALSE,FALSE,0); PseudoSetup(VBox); 

if (!Setup.Parameter.IgnoreCamac)
   {
   VBox=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox0),VBox,FALSE,FALSE,0); ScalerSetup(VBox);
   VBox=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox0),VBox,FALSE,FALSE,0); PeriodicLogSetup(VBox);
   VBox=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox0),VBox,FALSE,FALSE,0); MacroSetup(VBox);
   }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox0),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("SETUP\nDONE"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(EdWinClosed),NULL);

if (!Setup.Parameter.IgnoreCamac) g_list_free(GList);
gtk_widget_show_all(EdWin);
}
/*----------------------------------------------------------------------------------------------------------------------*/
