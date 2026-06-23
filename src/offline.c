#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "lamps.h"

//Function Templates
void Attention(gint XPos,gchar *Messg);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void SavePrefs(void); void SaveBatchFile(gchar *FName);
void MakeRow(gint i);
void CreateDir(void);
//External Global variables
extern struct Setup Setup;
extern enum ProgramState ProgramState;
extern enum AnalysisRequest AnalysisRequest;
extern struct FileSelectType *FileX;
extern gchar SetupDir[MAX_DIR_STRLEN],ListFDir[MAX_DIR_STRLEN],BatDir[MAX_DIR_STRLEN];                   //Directory prefs
 
//Global variables within this file only
gint SelRow;                                                            //Selected row while adding files to offline setup
struct Offl {GtkWidget *Win; GtkWidget *Table; GtkWidget *ScrlW;};
struct Offl *Offl;                                                            //Global structure to be destroyed after use 

//Function templates and global variables for threads
void *ReadFile(gpointer Data);
pthread_t Reader;
/*--------------------------------------------------------------------------------------------------------------------*/
void ZeroChanged(GtkWidget *W,gpointer Data)
{
SelRow=GPOINTER_TO_INT(Data);
if (Setup.Offline.Zero[SelRow]) 
   { Setup.Offline.Zero[SelRow]=FALSE; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"No "); }
else                            
   { Setup.Offline.Zero[SelRow]=TRUE;  gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Yes"); }
}
/*--------------------------------------------------------------------------------------------------------------------*/
void RowSelect(GtkWidget *W,gpointer Data)
{ SelRow=GPOINTER_TO_INT(Data); }
/*--------------------------------------------------------------------------------------------------------------------*/
void SetFSelected(GtkWidget *W,gpointer Unused)
{
GList *Node;                      //Note: The GList has the table elements in reverse order! and g_list_nth doesnt work!
gchar Str[MAX_FNAME_LENGTH+5];
gint i,j;

Node=g_list_last(GTK_TABLE(Offl->Table)->children);                                             //First element of table
for (i=0;i<SelRow;i++) for (j=0;j<8;j++) Node=g_list_previous(Node);                            //Skip nodes upto SelRow
for (j=0;j<3;j++)  Node=g_list_previous(Node);                                                       //Skip 3 more nodes

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Setup.Offline.SetFName[SelRow],"%s/%s",FileX->Path,FileX->TargetFile);                          //Store full path
g_free(FileX);

AbbreviateFileName(Str,Setup.Offline.SetFName[SelRow],18);
gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),Str);     //Change text inside button
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SetFNameChanged(GtkWidget *W,gpointer Data)
{
SelRow=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select Setup File",Offl->Win,200,226,TRUE,".",".set",FALSE,&SetFSelected,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveSpecChanged(GtkWidget *W,gpointer Data)
{
SelRow=GPOINTER_TO_INT(Data);
if (Setup.Offline.SaveSpec[SelRow]) 
   { Setup.Offline.SaveSpec[SelRow]=FALSE; gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"No "); }
else                                
   { Setup.Offline.SaveSpec[SelRow]=TRUE;  gtk_label_set_text(GTK_LABEL(GTK_BIN(W)->child),"Yes"); }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SpecFNameChanged(GtkWidget *W,gpointer Data)
{
SelRow=GPOINTER_TO_INT(Data);
strcpy(Setup.Offline.SpecFName[SelRow],gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BufSkipChanged(GtkWidget *W,gpointer Data)
{
SelRow=GPOINTER_TO_INT(Data);
Setup.Offline.BufSkip[SelRow]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BufReadChanged(GtkWidget *W,gpointer Data)
{
SelRow=GPOINTER_TO_INT(Data);
strcpy(Setup.Offline.BufRead[SelRow],gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ListFSelected(GtkWidget *W,gpointer Unused)                                          //Get the selected list filename
{
gint ColWidth[8]={28,40,150,90,40,80,80,80};
gchar Str[MAX_FNAME_LENGTH+5];
GtkWidget *But,*Entry;
gfloat End;

if (Setup.Offline.NFiles == MAX_OFFLINE_FILES) return;                                  //Limit reached. Need a popup here
Setup.Offline.NFiles=MIN(MAX_OFFLINE_FILES,Setup.Offline.NFiles+1);                                     //Increment NFiles
SelRow=MAX(0,Setup.Offline.NFiles-1);                                                                //SelRow=selected row
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Setup.Offline.ListFName[SelRow],"%s/%s",FileX->Path,FileX->TargetFile);                          //Store full path 
strcpy(ListFDir,FileX->Path); SavePrefs();                                                                 //Preserve path
gtk_table_resize(GTK_TABLE(Offl->Table),Setup.Offline.NFiles,9);

sprintf(Str,"%02d",SelRow+1);
But=gtk_button_new_with_label(Str);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(RowSelect),GINT_TO_POINTER(SelRow));
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
gtk_table_attach(GTK_TABLE(Offl->Table),But,0,1,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

if (Setup.Offline.Zero[SelRow]) But=gtk_button_new_with_label("Yes");
else                            But=gtk_button_new_with_label("No ");
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ZeroChanged),GINT_TO_POINTER(SelRow));
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[1],-1);
gtk_table_attach(GTK_TABLE(Offl->Table),But,1,2,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

AbbreviateFileName(Str,Setup.Offline.ListFName[SelRow],18);
But=gtk_button_new_with_label(Str);
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[2],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(RowSelect),GINT_TO_POINTER(SelRow));
gtk_table_attach(GTK_TABLE(Offl->Table),But,2,3,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

AbbreviateFileName(Str,Setup.Offline.SetFName[SelRow],18);
But=gtk_button_new_with_label(Str); 
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[3],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SetFNameChanged),GINT_TO_POINTER(SelRow));
gtk_table_attach(GTK_TABLE(Offl->Table),But,3,4,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

if (Setup.Offline.SaveSpec[SelRow]) But=gtk_button_new_with_label("Yes"); else  But=gtk_button_new_with_label("No ");
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveSpecChanged),GINT_TO_POINTER(SelRow));
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[4],-1);
gtk_table_attach(GTK_TABLE(Offl->Table),But,4,5,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.Offline.SpecFName[SelRow]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[5],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SpecFNameChanged),GINT_TO_POINTER(SelRow));
gtk_table_attach(GTK_TABLE(Offl->Table),Entry,5,6,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
sprintf(Str,"%d",Setup.Offline.BufSkip[SelRow]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[6],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(BufSkipChanged),GINT_TO_POINTER(SelRow));
gtk_table_attach(GTK_TABLE(Offl->Table),Entry,6,7,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.Offline.BufRead[SelRow]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[7],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(BufReadChanged),GINT_TO_POINTER(SelRow));
gtk_table_attach(GTK_TABLE(Offl->Table),Entry,7,8,SelRow,SelRow+1,GTK_FILL,GTK_SHRINK,0,0);

End=16.0*SelRow; if (SelRow>8) End=40.0*SelRow;                                            //Fudging to scroll to the end!
gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Offl->ScrlW))),End); 
                                                                                                           //Scroll to end
gtk_widget_show_all(Offl->Table);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ButtonAdd(GtkWidget *W,gpointer Unused)                             //User clicked the "Add" button from OfflineSetup
{
if (Setup.Offline.NFiles == MAX_OFFLINE_FILES) return;                                                     //Limit reached
FileX=g_new(struct FileSelectType,1);
switch (Setup.ListMode.Compr)                                                                        //File selector masks
   {
   case 0: FileOpenNew("Select List Mode Files",Offl->Win,514,226,TRUE,ListFDir,".001",TRUE,&ListFSelected,TRUE); 
           break;                                                                                      //NSC Candle format
   case 1: case 2: FileOpenNew("Select List Mode Files",Offl->Win,514,226,TRUE,ListFDir,".zls",TRUE,&ListFSelected,TRUE);
           break;                                                                            //Normal and Advanced formats
   }
}
//----------------------------------------------------------------------------------------------------------------------
void ButtonClear(gpointer Unused)                                        //User clicked "Clear" button from OfflineSetup
{
gint i;

if (SelRow>Setup.Offline.NFiles-1) return;
Setup.Offline.NFiles=MAX(0,Setup.Offline.NFiles-1);
for (i=SelRow;i<Setup.Offline.NFiles;++i)               //Move the info of rows one step up, overwriting the deleted row
    {
    Setup.Offline.Zero[i]=Setup.Offline.Zero[i+1];
    strcpy(Setup.Offline.ListFName[i],Setup.Offline.ListFName[i+1]);
    strcpy(Setup.Offline.SetFName[i],Setup.Offline.SetFName[i+1]);
    Setup.Offline.SaveSpec[i]=Setup.Offline.SaveSpec[i+1];
    strcpy(Setup.Offline.SpecFName[i],Setup.Offline.SpecFName[i+1]);
    Setup.Offline.BufSkip[i]=Setup.Offline.BufSkip[i+1];
    strcpy(Setup.Offline.BufRead[i],Setup.Offline.BufRead[i+1]);
    }
SelRow=MAX(0,SelRow-1);                                                                            //Move up the pointer
if (GTK_IS_WIDGET(Offl->Table)) gtk_widget_destroy(Offl->Table);
Offl->Table=gtk_table_new(Setup.Offline.NFiles,8,FALSE);
for (i=0;i<Setup.Offline.NFiles;i++) MakeRow(i);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Offl->ScrlW),Offl->Table);
gtk_widget_show_all(Offl->ScrlW);
}
//----------------------------------------------------------------------------------------------------------------------
void ButtonClearAll(gpointer Unused)                                  //User clicked "ClearAll" button from OfflineSetup
{
if (GTK_IS_WIDGET(Offl->Table)) gtk_widget_destroy(Offl->Table);
SelRow=0; Setup.Offline.NFiles=0;
Offl->Table=gtk_table_new(Setup.Offline.NFiles,8,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Offl->ScrlW),Offl->Table);
gtk_widget_show_all(Offl->ScrlW);
}
//----------------------------------------------------------------------------------------------------------------------
void CloseAnalysis(GtkWidget *W,gpointer Unused)                                        //Offline Analysis Window closed
{
g_free(Offl);                                                                                               //Important!
ProgramState=Free;
}
//----------------------------------------------------------------------------------------------------------------------
void CancelAnalysis(GtkWidget *W,GtkWidget *OffWin)
{ gtk_widget_destroy(OffWin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void StartAnalysis(GtkWidget *Button,GtkWidget *OffWin)
{
pthread_attr_t TAttr;
struct sched_param Param;

gtk_widget_destroy(OffWin);                //Close offline dialog window, this will also call CloseAnalysis automatically
if (!Setup.Offline.NFiles) return;
AnalysisRequest=NormalAnalysis;                                              //This flag is for pause/resume/stop control
SaveBatchFile(".lamps_bat");
ProgramState=AnalysisOn;

pthread_attr_init(&TAttr);
Param.sched_priority=5;
pthread_attr_setschedparam(&TAttr,&Param);

pthread_create(&Reader,/*&Param*/NULL,ReadFile,NULL);                                                       //Start thread
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PauseOffline(GtkWidget *W,gpointer Data)
{ if (AnalysisRequest==NormalAnalysis) AnalysisRequest=PauseAnalysis; }
/*----------------------------------------------------------------------------------------------------------------------*/
void ResumeOffline(GtkWidget *W,gpointer Data)
{ if (AnalysisRequest==PauseAnalysis) AnalysisRequest=NormalAnalysis; }
/*----------------------------------------------------------------------------------------------------------------------*/
void StopOffline(GtkWidget *W,gpointer Data)                                      //Stops the analysis of the current file
{ if (AnalysisRequest==NormalAnalysis) AnalysisRequest=StopAnalysis; }
/*----------------------------------------------------------------------------------------------------------------------*/
void StopAllOffline(GtkWidget *W,gpointer Data)                           //Stops the entire analysis batch process thread
{ if (AnalysisRequest==NormalAnalysis) AnalysisRequest=StopAllAnalysis; }
/*----------------------------------------------------------------------------------------------------------------------*/
void RefreshRateCallback(GtkWidget *W,gpointer Data)
{
Setup.Offline.BufRefresh=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
if (Setup.Offline.BufRefresh<=0) Setup.Offline.BufRefresh=-1;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MakeRow(gint i)
{
gint ColWidth[8]={28,40,150,90,40,80,80,80};
GtkWidget *But,*Entry;
gchar Str[MAX_FNAME_LENGTH+10];

sprintf(Str,"%02d",i+1); But=gtk_button_new_with_label(Str); 
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(RowSelect),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(Offl->Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

if (Setup.Offline.Zero[i]) But=gtk_button_new_with_label("Yes"); else  But=gtk_button_new_with_label("No ");
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ZeroChanged),GINT_TO_POINTER(i));
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[1],-1);
gtk_table_attach(GTK_TABLE(Offl->Table),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

AbbreviateFileName(Str,Setup.Offline.ListFName[i],18);
But=gtk_button_new_with_label(Str); 
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[2],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(RowSelect),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(Offl->Table),But,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

AbbreviateFileName(Str,Setup.Offline.SetFName[i],18);
But=gtk_button_new_with_label(Str); 
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[3],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SetFNameChanged),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(Offl->Table),But,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

if (Setup.Offline.SaveSpec[i]) But=gtk_button_new_with_label("Yes"); else  But=gtk_button_new_with_label("No ");
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveSpecChanged),GINT_TO_POINTER(i));
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[4],-1);
gtk_table_attach(GTK_TABLE(Offl->Table),But,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.Offline.SpecFName[i]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[5],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SpecFNameChanged),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(Offl->Table),Entry,5,6,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
sprintf(Str,"%d",Setup.Offline.BufSkip[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[6],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(BufSkipChanged),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(Offl->Table),Entry,6,7,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.Offline.BufRead[i]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[7],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(BufReadChanged),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(Offl->Table),Entry,7,8,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LoadBatchFile(gchar *FName)
{
gint i,j;
FILE *Fp;

if ((Fp=fopen(FName,"r")) != NULL)
   {
   fscanf(Fp,"%d",&Setup.Offline.NFiles);
   for (i=0;i<Setup.Offline.NFiles;i++)
       fscanf(Fp,"%d %d %s %s %d %s %d %s",&j,&Setup.Offline.Zero[i],Setup.Offline.ListFName[i],Setup.Offline.SetFName[i],
               &Setup.Offline.SaveSpec[i],Setup.Offline.SpecFName[i],&Setup.Offline.BufSkip[i],Setup.Offline.BufRead[i]);
   fscanf(Fp,"%d %f",&Setup.Offline.BufRefresh,&Setup.Offline.Delay);
   fclose(Fp);
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LoadBatF(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];
gint i;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(BatDir,FileX->Path); SavePrefs();                                                                 //Preserve path
g_free(FileX);

if (Setup.Offline.NFiles) ButtonClearAll(NULL);                                            //Get rid of the existing rows

LoadBatchFile(FName);
for (i=0;i<Setup.Offline.NFiles;i++) MakeRow(i);
gtk_widget_show_all(Offl->Table);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LoadBat(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Load Batch File",Offl->Win,0,226,TRUE,BatDir,".bat",FALSE,&LoadBatF,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveBatchFile(gchar *FName)
{
gint i;
FILE *Fp;

if ((Fp=fopen(FName,"w")) != NULL)
   {
   fprintf(Fp,"%d\n",Setup.Offline.NFiles);
   for (i=0;i<Setup.Offline.NFiles;i++)
       fprintf(Fp,"%d %d %s %s %d %s %d %s\n",i+1,Setup.Offline.Zero[i],Setup.Offline.ListFName[i],
               Setup.Offline.SetFName[i],Setup.Offline.SaveSpec[i],Setup.Offline.SpecFName[i],
               Setup.Offline.BufSkip[i],Setup.Offline.BufRead[i]);
   fprintf(Fp,"%d %f\n",Setup.Offline.BufRefresh,Setup.Offline.Delay);
   fclose(Fp);
   }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveBatF(GtkWidget *W,gpointer Unused)
{
gchar FName[MAX_FNAME_LENGTH];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
strcpy(BatDir,FileX->Path); SavePrefs();                                                                  //Preserve path
g_free(FileX);
SaveBatchFile(FName);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SaveBat(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Save Batch File",Offl->Win,0,226,FALSE,BatDir,".bat",FALSE,&SaveBatF,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DelayCallBack(GtkWidget *W,gpointer Data)
{
Setup.Offline.Delay=atof(gtk_entry_get_text(GTK_ENTRY(W)));
Setup.Offline.Delay=CLAMP(Setup.Offline.Delay,0.0,5.0);                             //Lets restrict to something sensible
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OfflineSetup(GtkWidget *W,gpointer Data)
{
static gchar *Titles[8]= {"No\n","Zero\nSpec","List File\nName","Setup File\nName","Save\nSpec","Name for\nSpecFiles",
                              "Buffs to\nSkip","Buffs to\nRead"};
gint ColWidth[8]={28,40,150,90,40,80,80,80};
static GdkColor TitlesBg  = {0,0xFFFF,0x0000,0x0000};
static GdkColor TitlesFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *TitlesStyle;
GtkWidget *VBox,*VBox1,*VBox2,*HBox,*HBox1,*HBox2,*HBoxA,*HBoxB,*Table,*ScrollW,*TitlesBut,*But,*But1,*But2,
          *Label,*Combo;
GList *GList1,*GList2;
gint i;
gchar Str[MAX_FNAME_LENGTH+10];

if (ProgramState != Free) { Attention(0,"Another task is in progress"); return; }
ProgramState=AnalysisSetup;
CreateDir();                                      //Create directories as per user preferences if they dont exist already
Offl=g_new(struct Offl,1);                                                         //Allocate memory, must be freed later

TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());                            //Copy default style to this style
for (i=0;i<5;i++) { TitlesStyle->fg[i]=TitlesStyle->text[i]=TitlesFg; TitlesStyle->bg[i]=TitlesBg; }

Offl->Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);                                               //Window for Offline Settings
gtk_widget_set_size_request(Offl->Win,-1,432);
gtk_window_set_title(GTK_WINDOW(Offl->Win),"Offline Analysis");
gtk_signal_connect(GTK_OBJECT(Offl->Win),"destroy",GTK_SIGNAL_FUNC(CloseAnalysis),NULL);

VBox=gtk_vbox_new(FALSE,12); gtk_container_add(GTK_CONTAINER(Offl->Win),VBox); 
gtk_container_set_border_width(GTK_CONTAINER(VBox),5);                                                   //Breathing room

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,TRUE,0); 

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(ScrollW,628,48);
gtk_box_pack_start(GTK_BOX(VBox1),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(1,8,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                      //Pack Table into ScrollW
for (i=0;i<8;i++)
    {
    TitlesBut=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(TitlesBut,TitlesStyle);
    gtk_widget_set_usize(GTK_WIDGET(TitlesBut),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),TitlesBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

Offl->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Offl->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox1),Offl->ScrlW,TRUE,TRUE,0);

Offl->Table=gtk_table_new(Setup.Offline.NFiles,8,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Offl->ScrlW),Offl->Table);                 //Pack Table into ScrollW
for (i=0;i<Setup.Offline.NFiles;i++) MakeRow(i);

VBox2=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox2,FALSE,FALSE,0);
But=gtk_button_new_from_stock(GTK_STOCK_ADD); gtk_box_pack_start(GTK_BOX(VBox2),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ButtonAdd),NULL);
But1=gtk_button_new_from_stock(GTK_STOCK_CLEAR); gtk_box_pack_start(GTK_BOX(VBox2),But1,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But1),"clicked",GTK_SIGNAL_FUNC(ButtonClear),NULL);
But2=gtk_button_new_with_label("Clear All"); gtk_box_pack_start(GTK_BOX(VBox2),But2,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But2),"clicked",GTK_SIGNAL_FUNC(ButtonClearAll),NULL);

HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox1,FALSE,FALSE,0);

HBoxA=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(HBox1),HBoxA,TRUE,FALSE,0);
Label=gtk_label_new("Refresh After"); gtk_box_pack_start(GTK_BOX(HBoxA),Label,FALSE,FALSE,20);
Combo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Combo),60,-1);
gtk_box_pack_start(GTK_BOX(HBoxA),Combo,FALSE,FALSE,0); GList1=NULL;
GList1=g_list_append(GList1,"1000"); GList1=g_list_append(GList1,"500"); GList1=g_list_append(GList1,"100");
GList1=g_list_append(GList1,"50");  GList1=g_list_append(GList1,"Never");                                //5 popup choices
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList1);                                                 //Define the popup
sprintf(Str,"%d",Setup.Offline.BufRefresh);
if (Setup.Offline.BufRefresh<0) gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Never");
                           else gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);        //Set the initial entry
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(RefreshRateCallback),NULL);
Label=gtk_label_new("Buffs"); gtk_box_pack_start(GTK_BOX(HBoxA),Label,FALSE,FALSE,0);

HBoxB=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBoxB,TRUE,FALSE,2);
Label=gtk_label_new("Delay"); gtk_box_pack_start(GTK_BOX(HBoxB),Label,FALSE,FALSE,0);
Combo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Combo),60,-1);
gtk_box_pack_start(GTK_BOX(HBoxB),Combo,FALSE,FALSE,0); GList2=NULL;
GList2=g_list_append(GList2,"0"); GList2=g_list_append(GList2,"0.1"); GList2=g_list_append(GList2,"0.25");
GList2=g_list_append(GList2,"0.5");  GList2=g_list_append(GList2,"1.0");                                //5 popup choices
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList2);                                                //Define the popup
Setup.Offline.Delay=CLAMP(Setup.Offline.Delay,0.0,5.0);                             //Lets restrict to something sensible
sprintf(Str,"%.1f",Setup.Offline.Delay);
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),Str);                                       //Set the initial entry
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(DelayCallBack),NULL);  //Connect signal
Label=gtk_label_new("sec"); gtk_box_pack_start(GTK_BOX(HBoxB),Label,FALSE,FALSE,0);

HBox2=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox2,FALSE,FALSE,0);
But=gtk_button_new_with_label("Load Batch File"); gtk_box_pack_start(GTK_BOX(HBox2),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(LoadBat),NULL);
But=gtk_button_new_with_label("Save Batch File"); gtk_box_pack_start(GTK_BOX(HBox2),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveBat),NULL);
But=gtk_button_new_with_label("Start Analysis"); gtk_box_pack_start(GTK_BOX(HBox2),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(StartAnalysis),Offl->Win);
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox2),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CancelAnalysis),Offl->Win);

gtk_widget_show_all(Offl->Win);                                                     //Show all the widgets in the window
gtk_style_unref(TitlesStyle);
g_list_free(GList1); g_list_free(GList2);
}
/*----------------------------------------------------------------------------------------------------------------------*/
