#define GTK_ENABLE_BROKEN
#include <gtk/gtk.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "lamps.h"
#include "common.h"

/*Function Templates*/
void Attention(gint XPos,gchar *Messg);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void bstart_(void); void fstart_(gchar *RunName,gint Len); void fstop_(void); void bstop_(void);
void SAttention(gchar *Messg);
int FreedomRead(FILE *ListFile,glong *ZBufs,glong *EvtsRead,glong *BytsRead,
                gboolean OptionReWrite,gushort *OutBuf,gint *NewEvts);
int FreedomSkip(FILE *ListFile);
gboolean writeEventBlock(gint eventSize,gint ev_per_block,gushort data[],FILE *Fp);
void writeNames(int noOfAdcs,int noOfSingles,int noOfScalers,FILE *Fp);
void queue_text(enum blocktype block,gchar *txt,FILE *Fp);
void writeEndOfFile(FILE *Fp);
gint ZlsGlsRead(FILE *ListFile,gushort *Buf,glong *ZBufs,glong *EvtsRead,glong *BytsRead,
                gboolean OptionReWrite,gushort *OutBuf,gint *NewEvts);
gint ZlsGlsSkip(FILE *ListFile);
void Update(gdouble StartTime,gchar *FName,glong FSize,glong ZBufs,glong BytsRead,glong EvtsRead);
gint ZlsWrite(FILE *Fp,gushort *Buf,gint NEvt,gint NPar);
void RadwareWrite(FILE *Fp,gushort *OutBuf,gint NEvt,gint NPar);
void RadwareWriteHeader(FILE *Fp);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
void ReWriteMakeRow(gint i);
void GetRunName(gchar *FileName,gchar *RunName);

//External Global variables
extern struct Setup Setup;
extern enum ProgramState ProgramState;
extern enum ReWriteRequest ReWriteRequest;
extern GtkWidget *S_Stat[15];
extern struct FileSelectType *FileX;
extern gboolean UtilBusy;                //Flag indicating that Utilities is in use. Prevents running multiple utilities
extern gint TopOfset;                                         //Accounts for space occupied by window manager at the top

//Global variables within this file only
gint TheRow;                                                                           //Selected row while adding files
struct ReWrt {GtkWidget *Win; GtkWidget *Table; GtkWidget *ScrlW;};
struct ReWrt *ReWrt;                                                        //Global structure to be destroyed after use
struct Progress {GtkWidget *W; GtkWidget *PBar1; GtkWidget *PBar2; gint Timer; gfloat F1; gfloat F2; } *Progress;
GtkWidget **MulCheckBut;                                                     //Set of Check Buttons to select parameters

//Function templates and global variables for threads
void *ReWriteFile(gpointer Data);
//----------------------------------------------------------------------------------------------------------------------
gint ProgressTimeout(gpointer Unused)
{ 
gfloat Old1,Old2;

Old1=gtk_progress_get_value(GTK_PROGRESS(Progress->PBar1));
if (Progress->F1 > Old1) gtk_progress_set_value(GTK_PROGRESS(Progress->PBar1),Progress->F1); 
Old2=gtk_progress_get_value(GTK_PROGRESS(Progress->PBar2));
if (Progress->F2 != Old2) gtk_progress_set_value(GTK_PROGRESS(Progress->PBar2),Progress->F2); 
return TRUE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void StopReWrite(GtkWidget *W,gpointer Unused)
{ 
ReWriteRequest=StopAllReWrite; 
gtk_widget_destroy(GTK_WIDGET(Progress->W));
gtk_timeout_remove(Progress->Timer); Progress->Timer=0; Progress->W=NULL; g_free(Progress); 
ProgramState=Free;
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint DeleteProgress(GtkWidget *W,gpointer Unused)
{ return TRUE; }
/*----------------------------------------------------------------------------------------------------------------------*/
void *ReWriteFile(gpointer Data)
{
gint BRead,NewEvts;
gchar Str[80],FName[MAX_FNAME_LENGTH],NSC_txt[256],RunName[MAX_FNAME_LENGTH];
gfloat Kbps,Evtps,PerCent;
glong FSize,ZBufs,BytsRead,EvtsRead,EvtsReWritten;
struct stat StatBuf;
FILE *ListFile,*OutpFile;
gint FileNo,I,SkipStatus;
gushort Buf[32768],OutBuf[MAX_DATABUF];    //MAX_DATABUF in lamps.h should allow storage of the new uncompressed buffer
                                                                      //holding the parameters that are to be rewritten
struct timeval Tv;
gdouble StartTime;
gboolean DoClose;
                                                                                                                             
bstart_();                                                                            //Call SUBROUTINE BSTART (in user.F)
for (FileNo=0,Progress->F1=0.0;FileNo<Setup.ReWrite.NFiles;++FileNo)
    {
    sprintf(Str,"Read %d of %d",FileNo+1,Setup.ReWrite.NFiles);
    gdk_threads_enter(); gtk_label_set_text(GTK_LABEL(S_Stat[0]),Str); gdk_threads_leave();
    GetRunName(Setup.ReWrite.ListFName[FileNo],RunName);
    fstart_(RunName,strlen(RunName)); 
    BRead=atoi(Setup.ReWrite.BufRead[FileNo]);
    if (BRead<=0) BRead=-1;                               //If BufRead is "All" or any non-numeric string read all buffers
    Kbps=0.0; Evtps=0.0; AbbreviateFileName(FName,Setup.ReWrite.ListFName[FileNo],18); ZBufs=0; PerCent=0.0;
    stat(Setup.ReWrite.ListFName[FileNo],&StatBuf); FSize=StatBuf.st_size;
    sprintf(Str,"File Bytes: %ld",FSize);
    gdk_threads_enter(); gtk_label_set_text(GTK_LABEL(S_Stat[12]),Str); gdk_threads_leave();
                                                                                                                             
    if ((ListFile=fopen(Setup.ReWrite.ListFName[FileNo],"r"))==NULL)                                     //File not found!
       { sprintf(Str,"Cant open %s",FName); SAttention(Str); continue; }

    if (strcasecmp(Setup.ReWrite.OutFName[FileNo],"continue"))                 //Open a new file if its not a continuation 
       {
       if ((OutpFile=fopen(Setup.ReWrite.OutFName[FileNo],"w"))==NULL)
          { sprintf(Str,"Cant open %s",Setup.ReWrite.OutFName[FileNo]); SAttention(Str); continue; }
       if (Setup.ReWrite.OutFormat=='C')                                                            //Candle output format
          {
          sprintf(NSC_txt,"%s\n %s\n %s\n","LAMPS","A.Chatterjee","Candle Compatible File");
          queue_text(user,NSC_txt,OutpFile); writeNames(Setup.ReWrite.NPar,0,0,OutpFile);
          sprintf(NSC_txt,"START : %s started. Run #%04d\n","rewritten",111); queue_text(start,NSC_txt,OutpFile);
          }
       }
                                                                                                                             
    gettimeofday(&Tv,NULL); StartTime=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06;                          //Start timer
                                                                                                                             
    for (I=0,SkipStatus=0;I<Setup.ReWrite.BufSkip[FileNo];++I)                                              //Skip Buffers
        {
        switch (Setup.ListMode.Compr)
           {
           case 0: while ((SkipStatus=FreedomSkip(ListFile))==2); break;                            //Freedom format files
           case 1: case 2: SkipStatus=ZlsGlsSkip(ListFile);                                      //Zls or Gls format files
                                                                         //Skip for Exogam format files is not implemented
           }
        if (SkipStatus==1) break;                                                               //Invalid Signature or EOF
        }
    ZBufs=0; BytsRead=0; EvtsRead=0; EvtsReWritten=0;
    if (Setup.ReWrite.OutFormat=='R') RadwareWriteHeader(OutpFile);
    while (TRUE)                                                                            //Read Buffers
         {
         if (ReWriteRequest==StopAllReWrite) { Update(StartTime,FName,FSize,ZBufs,BytsRead,EvtsRead); break; }
         if (ZBufs%100) Update(StartTime,FName,FSize,ZBufs,BytsRead,EvtsRead);
         if (Setup.ListMode.Compr==0)                                               //Freedom format files
            { if (FreedomRead(ListFile,&ZBufs,&EvtsRead,&BytsRead,TRUE,OutBuf,&NewEvts)) break; }
         else if (Setup.ListMode.Compr==3)                                           //Exogam format files 
                 { SAttention("Exogam format not implemented"); UtilBusy=0; pthread_exit(NULL); }
         else {if (ZlsGlsRead(ListFile,Buf,&ZBufs,&EvtsRead,&BytsRead,TRUE,OutBuf,&NewEvts)) break;}//zls
         if (NewEvts>0)             //04-03-08: Important as IUAC data has blocks other than event blocks
            {
            if (Setup.ReWrite.OutFormat=='N')                          //N=Normal....Output in zls format
               { if (ZlsWrite(OutpFile,OutBuf,NewEvts,Setup.ReWrite.NPar)) break; }
            else if (Setup.ReWrite.OutFormat=='C')                  //C=Candle....Output in Candle format
               { if (!writeEventBlock(Setup.ReWrite.NPar,NewEvts,OutBuf,OutpFile)) break; }
            else if (Setup.ReWrite.OutFormat=='R')                   //R=Radware...Output in Radware frmat
            RadwareWrite(OutpFile,OutBuf,NewEvts,Setup.ReWrite.NPar);
            EvtsReWritten+=NewEvts;
            Progress->F2=100.0*(gfloat)BytsRead/FSize;
            }

         if ( (ZBufs==BRead) && (BRead>0) ) break;                                        //Limit reached
         }
    fclose(ListFile); 

    if (FileNo==Setup.ReWrite.NFiles-1) DoClose=TRUE; else DoClose=FALSE;             //TRUE for last file
    if (!DoClose) if (strcasecmp(Setup.ReWrite.OutFName[FileNo+1],"continue")) DoClose=TRUE;    //next is not continuation
    if (DoClose)                                        //Close if this is the last file or next one is not a continuation
       {
       if (Setup.ReWrite.OutFormat=='C') 
          {
          sprintf(NSC_txt,"STOP: %s ends. Collected %ld events","rewritten",EvtsReWritten);
          queue_text(stop,NSC_txt,OutpFile); writeEndOfFile(OutpFile);
          }
       fclose(OutpFile);
       }

    fstop_();                                                                         //Call SUBROUTINE FSTOP (in user.F)
    if (ReWriteRequest==StopAllReWrite) break;
    Progress->F1=100.0*(gfloat)(FileNo+1)/Setup.ReWrite.NFiles;
    }
Update(StartTime,FName,FSize,ZBufs,BytsRead,EvtsRead);                                    //Update the display at the end
bstop_();                                                                              //Call SUBROUTINE BSTOP (in user.F)
if (ProgramState != Free) StopReWrite(NULL,NULL);
gdk_threads_enter(); gtk_label_set_text(GTK_LABEL(S_Stat[0]),"Status: Free"); gdk_threads_leave();
UtilBusy=FALSE; pthread_exit(NULL);
return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReWriteRowSelect(GtkWidget *W,gpointer Data)
{ TheRow=GPOINTER_TO_INT(Data); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ReWriteBufSkipChanged(GtkWidget *W,gpointer Data)
{
TheRow=GPOINTER_TO_INT(Data);
Setup.ReWrite.BufSkip[TheRow]=atoi(gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReWriteBufReadChanged(GtkWidget *W,gpointer Data)
{
TheRow=GPOINTER_TO_INT(Data);
strcpy(Setup.ReWrite.BufRead[TheRow],gtk_entry_get_text(GTK_ENTRY(W)));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OutFNameChanged(GtkWidget *W,gpointer Data)
{
TheRow=GPOINTER_TO_INT(Data);
strcpy(Setup.ReWrite.OutFName[TheRow],gtk_entry_get_text(GTK_ENTRY(W)));
}
//----------------------------------------------------------------------------------------------------------------------
void GenerateOutFName()                                 //Create Setup.ReWrite.OutFName based on Setup.ReWrite.ListFName
{
gchar Str[MAX_FNAME_LENGTH+5];
gint L;

AbbreviateFileName(Str,Setup.ReWrite.ListFName[TheRow],MAX_FNAME_LENGTH-1);
L=strlen(Str); 
if (Setup.ReWrite.OutFormat=='N') strcpy(&Str[L-4],".zls");                                     //Output format "Normal"
else if (Setup.ReWrite.OutFormat=='C') strcpy(&Str[L-4],".001");                               //Output format Candle/Freedom
else strcpy(&Str[L-4],".dmp");
strcpy(Setup.ReWrite.OutFName[TheRow],Str);
}
//----------------------------------------------------------------------------------------------------------------------
void ReWriteFSelected(GtkWidget *W,gpointer Unused)                                     //Get the selected list filename
{
gint ColWidth[5]={28,150,80,80,150};
gchar Str[MAX_FNAME_LENGTH+5];
GtkWidget *But,*Entry;
gfloat End;

if (Setup.ReWrite.NFiles == MAX_REWRITE_FILES) return;                                  //Limit reached. Need a popup here
Setup.ReWrite.NFiles=MIN(MAX_REWRITE_FILES,Setup.ReWrite.NFiles+1);                                     //Increment NFiles
TheRow=MAX(0,Setup.ReWrite.NFiles-1);                                                                //TheRow=selected row
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Setup.ReWrite.ListFName[TheRow],"%s/%s",FileX->Path,FileX->TargetFile);                          //Store full path

gtk_table_resize(GTK_TABLE(ReWrt->Table),Setup.ReWrite.NFiles,5);

sprintf(Str,"%02d",TheRow+1);
But=gtk_button_new_with_label(Str);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReWriteRowSelect),GINT_TO_POINTER(TheRow));
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
gtk_table_attach(GTK_TABLE(ReWrt->Table),But,0,1,TheRow,TheRow+1,GTK_FILL,GTK_SHRINK,0,0);

AbbreviateFileName(Str,Setup.ReWrite.ListFName[TheRow],18);
But=gtk_button_new_with_label(Str);
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[1],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReWriteRowSelect),GINT_TO_POINTER(TheRow));
gtk_table_attach(GTK_TABLE(ReWrt->Table),But,1,2,TheRow,TheRow+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
sprintf(Str,"%d",Setup.ReWrite.BufSkip[TheRow]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[2],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ReWriteBufSkipChanged),GINT_TO_POINTER(TheRow));
gtk_table_attach(GTK_TABLE(ReWrt->Table),Entry,2,3,TheRow,TheRow+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.ReWrite.BufRead[TheRow]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[3],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ReWriteBufReadChanged),GINT_TO_POINTER(TheRow));
gtk_table_attach(GTK_TABLE(ReWrt->Table),Entry,3,4,TheRow,TheRow+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
GenerateOutFName();
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.ReWrite.OutFName[TheRow]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[4],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(OutFNameChanged),GINT_TO_POINTER(TheRow));
gtk_table_attach(GTK_TABLE(ReWrt->Table),Entry,4,5,TheRow,TheRow+1,GTK_FILL,GTK_SHRINK,0,0);

End=16.0*TheRow; if (TheRow>8) End=40.0*TheRow;                                            //Fudging to scroll to the end!
gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(ReWrt->ScrlW))),End); 
gtk_widget_show_all(ReWrt->Table);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void AddReWrite(GtkWidget *W,gpointer Unused)                                              //User clicked the "Add" button
{
if (Setup.ReWrite.NFiles == MAX_OFFLINE_FILES) return;                                                     //Limit reached
FileX=g_new(struct FileSelectType,1);
switch (Setup.ListMode.Compr)                                                  //File selector masks depends on list types
   {
   case 0: FileOpenNew("Select List Mode Files",ReWrt->Win,0,226,TRUE,".",".001",TRUE,&ReWriteFSelected,TRUE);
           break;                                                                                      //NSC Candle format
   case 1: case 2: FileOpenNew("Select List Mode Files",ReWrt->Win,0,226,TRUE,".",".zls",TRUE,&ReWriteFSelected,TRUE);
           break;                                                                            //Normal and Advanced formats
   case 3: FileOpenNew("Select List Mode Files",ReWrt->Win,0,226,TRUE,".",".dat",TRUE,&ReWriteFSelected,TRUE);
                                                                                                           //Exogam format
   }
}
//----------------------------------------------------------------------------------------------------------------------
void ClearReWrite(gpointer Unused)                                                         //User clicked "Clear" button
{
gint i,j;
gchar Str[MAX_FNAME_LENGTH+5];

if (TheRow>Setup.ReWrite.NFiles-1) return;
Setup.ReWrite.NFiles=MAX(0,Setup.ReWrite.NFiles-1);
for (i=TheRow;i<Setup.ReWrite.NFiles;++i)               //Move the info of rows one step up, overwriting the deleted row
    {
    strcpy(Setup.ReWrite.ListFName[i],Setup.ReWrite.ListFName[i+1]);
    Setup.ReWrite.BufSkip[i]=Setup.ReWrite.BufSkip[i+1];
    strcpy(Setup.ReWrite.BufRead[i],Setup.ReWrite.BufRead[i+1]);
    strcpy(Setup.ReWrite.OutFName[i],Setup.ReWrite.OutFName[i+1]);
    }
TheRow=MAX(0,TheRow-1);                                                                            //Move up the pointer
if (GTK_IS_WIDGET(ReWrt->Table)) gtk_widget_destroy(ReWrt->Table);

ReWrt->Table=gtk_table_new(Setup.ReWrite.NFiles,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ReWrt->ScrlW),ReWrt->Table);         //Pack Table into ScrollW
for (i=0;i<Setup.ReWrite.NFiles;++i) ReWriteMakeRow(i);
gtk_widget_show_all(ReWrt->ScrlW);
}
//----------------------------------------------------------------------------------------------------------------------
void ClearAllReWrite(gpointer Unused)                                                     //User clicked "ClearAll" button
{
if (GTK_IS_WIDGET(ReWrt->Table)) gtk_widget_destroy(ReWrt->Table);
TheRow=0; Setup.ReWrite.NFiles=0;
ReWrt->Table=gtk_table_new(Setup.ReWrite.NFiles,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ReWrt->ScrlW),ReWrt->Table);
gtk_widget_show_all(ReWrt->ScrlW);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CloseReWrite(GtkWidget *W,gpointer Unused)
{
g_free(ReWrt);                                                                                               //Important!
ProgramState=Free;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CancelReWrite(GtkWidget *W,GtkWidget *ReWin)
{ UtilBusy=FALSE; gtk_widget_destroy(ReWin); }
/*----------------------------------------------------------------------------------------------------------------------*/
void StartReWrite(GtkWidget *Button,GtkWidget *ReWin)
{
GtkWidget *VBox,*HBox,*Label,*But;
gint i;
pthread_t ReWriter;

gtk_widget_destroy(ReWin);                           //Close dialog window, this will also call CloseReWrite automatically
if (!Setup.ReWrite.NFiles) { Attention(100,"No files selected for rewriting"); UtilBusy=FALSE; return; }
for (i=0,Setup.ReWrite.NPar=0;i<NTOT;++i) if (Setup.ReWrite.Select[i]) ++Setup.ReWrite.NPar;
if (!Setup.ReWrite.NPar) { Attention(-100,"No parameters selected for rewriting"); UtilBusy=FALSE; return; }
ReWriteRequest=NormalReWrite;
ProgramState=ReWriteOn;

//Create window with progress bar
Progress=g_new(struct Progress,1); 
Progress->W=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(Progress->W,400,294); gtk_widget_set_usize(Progress->W,240,120);
gtk_window_set_title(GTK_WINDOW(Progress->W),"Rewriting Progress");
gtk_window_set_policy(GTK_WINDOW(Progress->W),FALSE,FALSE,TRUE);
gtk_signal_connect(GTK_OBJECT(Progress->W),"delete_event",GTK_SIGNAL_FUNC(DeleteProgress),NULL);

VBox=gtk_vbox_new(FALSE,5);
gtk_container_set_border_width(GTK_CONTAINER(VBox),10);
gtk_container_add(GTK_CONTAINER(Progress->W),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Files Done:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Progress->PBar1=gtk_progress_bar_new();
gtk_progress_set_format_string(GTK_PROGRESS(Progress->PBar1),"%v%%");
gtk_progress_set_show_text(GTK_PROGRESS(Progress->PBar1),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),Progress->PBar1,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("This File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,7);
Progress->PBar2=gtk_progress_bar_new();
gtk_progress_set_format_string(GTK_PROGRESS(Progress->PBar2),"%v%%");
gtk_progress_set_show_text(GTK_PROGRESS(Progress->PBar2),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),Progress->PBar2,FALSE,FALSE,0);

Progress->Timer=gtk_timeout_add(100,ProgressTimeout,NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_with_label("Halt Rewrite"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(StopReWrite),NULL);
gtk_widget_show_all(Progress->W);
//End of progress bar stuff

pthread_create(&ReWriter,NULL,ReWriteFile,NULL);                                                            //Start thread
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReWriteMakeRow(gint i)
{
gint ColWidth[5]={28,150,80,80,150};
GtkWidget *But,*Entry;
gchar Str[MAX_FNAME_LENGTH+10];

sprintf(Str,"%02d",i+1); But=gtk_button_new_with_label(Str); 
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReWriteRowSelect),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(ReWrt->Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

AbbreviateFileName(Str,Setup.ReWrite.ListFName[i],18);
But=gtk_button_new_with_label(Str); 
gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[1],-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReWriteRowSelect),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(ReWrt->Table),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
sprintf(Str,"%d",Setup.ReWrite.BufSkip[i]); gtk_entry_set_text(GTK_ENTRY(Entry),Str);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[2],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ReWriteBufSkipChanged),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(ReWrt->Table),Entry,2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.ReWrite.BufRead[i]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[3],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(ReWriteBufReadChanged),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(ReWrt->Table),Entry,3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);

Entry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_entry_set_text(GTK_ENTRY(Entry),Setup.ReWrite.OutFName[i]);
gtk_widget_set_size_request(GTK_WIDGET(Entry),ColWidth[4],-1);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(OutFNameChanged),GINT_TO_POINTER(i));
gtk_table_attach(GTK_TABLE(ReWrt->Table),Entry,4,5,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OutputFormatCallBack(GtkWidget *W,gpointer Unused)
{
gchar Temp[4];

strncpy(Temp,gtk_entry_get_text(GTK_ENTRY(W)),1);
Setup.ReWrite.OutFormat=Temp[0];            //Setup.ReWrite.OutFormat='N' :Normal(zls), 'C' :Candle/Freedom, 'R' :Radware 
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectAll(GtkWidget *W,gpointer Data)
{
int i;

for (i=0;i<NTOT;++i)
    {
    Setup.ReWrite.Select[i]=TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(MulCheckBut[i]),TRUE);
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DeSelectAll(GtkWidget *W,gpointer Data)
{
int i;

for (i=0;i<NTOT;++i)
    {
    Setup.ReWrite.Select[i]=FALSE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(MulCheckBut[i]),FALSE);
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReWriteSelect(GtkWidget *W,gpointer Data)
{
if (GTK_TOGGLE_BUTTON(W)->active) Setup.ReWrite.Select[GPOINTER_TO_INT(Data)]=TRUE;
else                              Setup.ReWrite.Select[GPOINTER_TO_INT(Data)]=FALSE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectParamsDone(GtkWidget *W,gpointer Data)
{
g_free(MulCheckBut);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectParams(GtkWidget *W,gpointer Data)
{
GtkStyle *Style;
const GdkColor BlueC   = {0,0x0000,0x0000,0x5555};
const GdkColor WhiteC  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkWidget *Win,*HBox,*VBox,*Table,*But,*ScrollW;
gint i;
gchar Str[20];

MulCheckBut=g_new(GtkWidget *,NTOT);                                             //Check buttons for selecting parameters

Style=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { Style->fg[i]=Style->text[i]=WhiteC; Style->bg[i]=BlueC; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_window_set_title(GTK_WINDOW(Win),"Select Parameters");
gtk_widget_set_uposition(GTK_WIDGET(Win),58,TOP_SPACE+TopOfset);
gtk_widget_set_size_request(GTK_WIDGET(Win),365,600);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(SelectParamsDone),NULL);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),5);                                                   //Breathing room

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Para#"); SetStyleRecursively(But,Style); gtk_widget_set_size_request(But,44,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Name"); SetStyleRecursively(But,Style); gtk_widget_set_size_request(But,240,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Write"); SetStyleRecursively(But,Style); gtk_widget_set_size_request(But,44,-1);
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);
Table=gtk_table_new(NTOT,3,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),1); gtk_table_set_col_spacings(GTK_TABLE(Table),5);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);

for (i=0;i<NTOT;++i)
    {
    sprintf(Str,"%3d",i+1); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(But,44,-1);
    gtk_table_attach(GTK_TABLE(Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (i<Setup.Parameter.NPar) But=gtk_button_new_with_label(Setup.Parameter.Name[i]); 
    else But=gtk_button_new_with_label(Setup.Pseudo.Name[i-Setup.Parameter.NPar]);
    gtk_widget_set_size_request(But,240,-1);
    gtk_table_attach(GTK_TABLE(Table),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    MulCheckBut[i]=gtk_check_button_new(); gtk_widget_set_size_request(GTK_WIDGET(MulCheckBut[i]),20,-1);
    gtk_table_attach(GTK_TABLE(Table),MulCheckBut[i],2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (Setup.ReWrite.Select[i]) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(MulCheckBut[i]),TRUE);
    gtk_signal_connect(GTK_OBJECT(MulCheckBut[i]),"toggled",GTK_SIGNAL_FUNC(ReWriteSelect),GINT_TO_POINTER(i));
    }

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Select\n   All"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SelectAll),NULL);
But=gtk_button_new_with_label("DeSelect\n     All"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DeSelectAll),NULL);
But=gtk_button_new_with_label("Okay"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ReWriteSetup(GtkWidget *W,gpointer Data)
{
static gchar *Titles[5]= {"No\n","List File\nName","Buffs to\nSkip","Buffs to\nRead","Out File\nor continue"};
gint ColWidth[5]={28,150,80,80,150};
static GdkColor TitlesBg  = {0,0x7777,0x0000,0x7777};
static GdkColor TitlesFg  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor Red =  {0,0xffff,0x0000,0x0000};
static GdkColor Blue = {0,0x0000,0x0000,0xffff};
GtkStyle *TitlesStyle;
GtkWidget *VBox,*VBox1,*VBox2,*HBox,*HBox1,*HBox2,*Table,*ScrollW,*TitlesBut,*But,*Combo,*Label,*Text;
GList *GList;
gint i;

if (ProgramState != Free) { Attention(0,"Another task is in progress"); return; }
if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;
ReWrt=g_new(struct ReWrt,1);                                                       //Allocate memory, must be freed later

TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());                            //Copy default style to this style
for (i=0;i<5;i++) { TitlesStyle->fg[i]=TitlesStyle->text[i]=TitlesFg; TitlesStyle->bg[i]=TitlesBg; }

ReWrt->Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(ReWrt->Win),"Re-Write Files");
gtk_widget_set_uposition(GTK_WIDGET(ReWrt->Win),435,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(ReWrt->Win),"destroy",GTK_SIGNAL_FUNC(CloseReWrite),NULL);

VBox=gtk_vbox_new(FALSE,12); gtk_container_add(GTK_CONTAINER(ReWrt->Win),VBox); 
gtk_container_set_border_width(GTK_CONTAINER(VBox),5);                                                   //Breathing room

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Text=gtk_text_new(NULL,NULL); gtk_widget_set_usize(GTK_WIDGET(Text),400,152);
gtk_text_set_editable(GTK_TEXT(Text),FALSE); gtk_text_set_word_wrap(GTK_TEXT(Text),TRUE);
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,"      For more info see Help/Contents/Utilities/ReWrite List File\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"1. Edit user.F for user defined pseudos. ",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"Events to be filtered should have all relevant paras and pseudos ",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"set to zero in user.F\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"2. Prepare a Setup that can read the file(s)\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"3. Select parameters (and pseudos) to be written to output\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"4. Click Add for all the required files\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"5. Enter output file names (.zls or .001) or\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,"continue",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL," to combine with previous file\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"6. Select output format\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"7. Click Start ReWrite",-1);
gtk_box_pack_start(GTK_BOX(HBox),Text,FALSE,FALSE,0);

VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,FALSE,0);
But=gtk_button_new_with_label("    Select\nParameters"); gtk_box_pack_start(GTK_BOX(VBox1),But,TRUE,FALSE,20);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SelectParams),NULL);

HBox=gtk_hbox_new(FALSE,10); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox1,TRUE,TRUE,0); 

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(GTK_WIDGET(ScrollW),500,48);
gtk_box_pack_start(GTK_BOX(VBox1),ScrollW,FALSE,FALSE,0);

Table=gtk_table_new(1,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);                      //Pack Table into ScrollW
for (i=0;i<5;++i)
    {
    TitlesBut=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(TitlesBut,TitlesStyle);
    gtk_widget_set_usize(GTK_WIDGET(TitlesBut),ColWidth[i],44);
    gtk_table_attach(GTK_TABLE(Table),TitlesBut,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

ReWrt->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ReWrt->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_widget_set_size_request(GTK_WIDGET(ReWrt->ScrlW),-1,200);
gtk_box_pack_start(GTK_BOX(VBox1),ReWrt->ScrlW,TRUE,TRUE,0);

ReWrt->Table=gtk_table_new(Setup.ReWrite.NFiles,5,FALSE);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ReWrt->ScrlW),ReWrt->Table);         //Pack Table into ScrollW
for (i=0;i<Setup.ReWrite.NFiles;++i) ReWriteMakeRow(i);

VBox2=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox),VBox2,FALSE,FALSE,0);
But=gtk_button_new_with_label("Add"); gtk_box_pack_start(GTK_BOX(VBox2),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AddReWrite),NULL);
But=gtk_button_new_with_label("Clear"); gtk_box_pack_start(GTK_BOX(VBox2),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ClearReWrite),NULL);
But=gtk_button_new_with_label("Clear All"); gtk_box_pack_start(GTK_BOX(VBox2),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ClearAllReWrite),NULL);

HBox1=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox1,FALSE,FALSE,0);

HBox2=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(HBox1),HBox2,FALSE,FALSE,0);
Label=gtk_label_new("Output Format:"); gtk_box_pack_start(GTK_BOX(HBox2),Label,FALSE,FALSE,0);
Combo=gtk_combo_new();
GList=NULL; GList=g_list_append(GList,"Normal (zls)"); GList=g_list_append(GList,"Candle/Freedom");
GList=g_list_append(GList,"Radware Dump");
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Combo)->entry),FALSE);
gtk_combo_set_popdown_strings(GTK_COMBO(Combo),GList);
gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Combo)->entry),"Normal (zls)");
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Combo)->entry),"changed",GTK_SIGNAL_FUNC(OutputFormatCallBack),NULL);
gtk_box_pack_start(GTK_BOX(HBox2),Combo,TRUE,FALSE,6);

But=gtk_button_new_with_label("Start ReWrite"); gtk_box_pack_start(GTK_BOX(HBox1),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(StartReWrite),ReWrt->Win);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox1),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CancelReWrite),ReWrt->Win);

gtk_widget_show_all(ReWrt->Win);                                                     //Show all the widgets in the window
gtk_style_unref(TitlesStyle);
g_list_free(GList);
}
/*----------------------------------------------------------------------------------------------------------------------*/
