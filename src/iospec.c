#include <gtk/gtk.h>
#include <stdio.h>
#include <glob.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "lamps.h"
#define MAX_RUNS 500                                               //The maximum number of runs in the current directory

//External Global variables
extern guint16 *Oned16,*Twod16;
extern guint32 *Oned32,*Twod32;
extern gint Off1d[MAX_1D],Off2d[MAX_2D];
extern enum ProgramState ProgramState;
extern struct Setup Setup;
extern gchar SelRun[MAX_TEXT_FIELD];
extern struct FileSelectType *FileX;
extern gchar SpecDir[MAX_DIR_STRLEN];

//Global variables for this file only
gint Row1,Row2;
GtkWidget **MulW;          //MulW[0-1] are the Tables for 1d and 2d files MulW[1-2] are the scr wins for 1d and 2d files

//Function templates
void FindRuns(gint *NoOfRuns,gchar *RunName[]);
void ZeroOned(gint SpecNo); void ZeroTwod(gint SpecNo);
void Attention(gint XPos,gchar *Messg);
void SAttention(gchar *Messg);
void RefreshAll(void);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
//----------------------------------------------------------------------------------------------------------------------
void ZeroOned(gint SNo)        //Zeroes out 1d Spectrum number SNo (=0,1,..). If SNo is -1, all 1d Spectra are zeroed out
{
gint i,j;

if (SNo==-1)                                                                                       //Zero all 1d Spectra
   {
   for (i=0;i<Setup.Oned.N;i++)
       {
       for (j=0;j<Setup.Oned.Chan[i];j++) 
           { 
           if (Setup.Oned.WSz == 1) Oned16[Off1d[i]+j]=0; 
           else                     Oned32[Off1d[i]+j]=0; 
           }
       }
   }
else                                                                                             //Zero only 1 spectrum
   {
   for (j=0;j<Setup.Oned.Chan[SNo];j++) 
       { 
       if (Setup.Oned.WSz == 1) Oned16[Off1d[SNo]+j]=0; 
       else                     Oned32[Off1d[SNo]+j]=0; 
       }
   }
}
//----------------------------------------------------------------------------------------------------------------------
void ZeroTwod(gint SNo)      //Zeroes out 2d Spectrum number SNo (=0,1,..). If SNo is -1, all 2d Spectra are zeroed out 
{
gint i,j;

if (SNo==-1)                                                                                     //Zero all 2d Spectra
   {
   for (i=0;i<Setup.Twod.N;i++)
       {
       for (j=0;j<(Setup.Twod.XChan[i]*Setup.Twod.YChan[i]);j++)
           {
           if (Setup.Twod.WSz == 1) Twod16[Off2d[i]+j]=0;
           else                     Twod32[Off2d[i]+j]=0;
           }
       }
   }
else
   {
   for (j=0;j<(Setup.Twod.XChan[SNo]*Setup.Twod.YChan[SNo]);j++)
       {
       if (Setup.Twod.WSz == 1) Twod16[Off2d[SNo]+j]=0;
       else                     Twod32[Off2d[SNo]+j]=0;
       }
   }
}
//----------------------------------------------------------------------------------------------------------------------
void XAttention(gint XPos,gchar *Messg,gboolean InThread)
{ if (InThread) SAttention(Messg); else Attention(XPos,Messg); }
//----------------------------------------------------------------------------------------------------------------------
void Write1d(gchar *FName,gint i,gboolean InThread)
{
FILE *Fp;
size_t Chans;
gchar Str[200];

if ((Fp=fopen(FName,"w")) == NULL) { sprintf(Str,"Error opening %s",FName); XAttention(0,Str,InThread); return; }
if (Setup.Oned.WSz == 1) Chans=fwrite(&Oned16[Off1d[i]],2,Setup.Oned.Chan[i],Fp);
else                     Chans=fwrite(&Oned32[Off1d[i]],4,Setup.Oned.Chan[i],Fp);
fclose(Fp);
if (Chans<Setup.Oned.Chan[i]) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); }
}
//----------------------------------------------------------------------------------------------------------------------
void Write2d32(gchar *FName,gint i,gboolean InThread)
{
FILE *Fp;
gchar Str[200];
gint Row,P1,P2;
guint16 XSize,YSize,NZer,NDat;

if ((Fp=fopen(FName,"w")) == NULL) { sprintf(Str,"Error opening %s",FName); XAttention(0,Str,InThread); return; }
XSize=Setup.Twod.XChan[i]; YSize=Setup.Twod.YChan[i];
if (fwrite(&XSize,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; } 
if (fwrite(&YSize,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; } 
for (Row=0;Row<Setup.Twod.YChan[i];Row++)
    {
    P1=0;
    while (P1<XSize)
       {
       P2=P1;
       while ( (!Twod32[Off2d[i]+XSize*Row+P2]) && (P2<XSize) ) P2++; NZer=P2-P1;
       if (fwrite(&NZer,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; };//Output no of zeroes
       P1=P2;
       while ( (Twod32[Off2d[i]+XSize*Row+P2]) && (P2<XSize) ) P2++; NDat=P2-P1;
       if (fwrite(&NDat,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; }; 
                                                                                                                //Output no of non-zero data
       if (NDat>0) if (fwrite(&Twod32[Off2d[i]+XSize*Row+P1],4,(size_t)NDat,Fp) < NDat)
                         { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return;}
       P1=P2;
       }
    }
fclose(Fp);
}
//----------------------------------------------------------------------------------------------------------------------
void Write2d16(gchar *FName,gint i,gboolean InThread)
{
FILE *Fp;
gchar Str[200];
gint Row,P1,P2;
guint16 XSize,YSize,NZer,NDat;

if ((Fp=fopen(FName,"w")) == NULL) { sprintf(Str,"Error opening %s",FName); XAttention(0,Str,InThread); return; }
XSize=Setup.Twod.XChan[i]; YSize=Setup.Twod.YChan[i];
if (fwrite(&XSize,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; } 
if (fwrite(&YSize,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; } 
for (Row=0;Row<Setup.Twod.YChan[i];Row++)
    {
    P1=0;
    while (P1<XSize)
       {
       P2=P1;
       while ( (!Twod16[Off2d[i]+XSize*Row+P2]) && (P2<XSize) ) P2++; NZer=P2-P1;
       if (fwrite(&NZer,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; };//Output no of zeroes
       P1=P2;
       while ( (Twod16[Off2d[i]+XSize*Row+P2]) && (P2<XSize) ) P2++; NDat=P2-P1;
       if (fwrite(&NDat,2,1,Fp) < 1) { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return; };  
                                                                                                                 //Output no of non-zero data
       if (NDat>0) if (fwrite(&Twod16[Off2d[i]+XSize*Row+P1],2,(size_t)NDat,Fp) < NDat)
                         { sprintf(Str,"Error writing %s",FName); XAttention(-200,Str,InThread); return;}
       P1=P2;
       }
    }
fclose(Fp);
}
//----------------------------------------------------------------------------------------------------------------------
void RadwareMatrixRead(gchar *FName,gint i)
{  //Modified 25 April 2008. Read RadwareRadware .spn (Double Word) files instead of .mat (Single Word)
FILE *Fp;
gchar Str[200];
gint ChY;

if ((Fp=fopen(FName,"r")) == NULL) { sprintf(Str,"Error opening %s",FName); XAttention(0,Str,FALSE); return; }
for (ChY=0;ChY<4096;++ChY) fread(&Twod32[Off2d[i]+ChY*4096],4,4096,Fp); 
fclose(Fp);
}
//----------------------------------------------------------------------------------------------------------------------
void RadwareMatrixWrite(gchar *FName,gint i)
{  //Modified 25 April 2008. Write RadwareRadware .spn (Double Word) files instead of .mat (Single Word)
FILE *Fp;
gchar Str[200];
gint ChY;

if ((Fp=fopen(FName,"w")) == NULL) { sprintf(Str,"Error opening %s",FName); XAttention(0,Str,FALSE); return; }
for (ChY=0;ChY<4096;++ChY) fwrite(&Twod32[Off2d[i]+ChY*4096],4,4096,Fp); 
fclose(Fp);
}
//----------------------------------------------------------------------------------------------------------------------
void YesSaveSpectra(GtkWidget *W,GtkWidget *Win)
{
gint i;
gchar DName[MAX_FNAME_LENGTH+10],FName[MAX_FNAME_LENGTH+50];

sprintf(DName,"%s/Spec_%s",SpecDir,SelRun);
for (i=0;i<Setup.Oned.N;++i) { sprintf(FName,"%s/%s%03d.z1d",DName,SelRun,i+1); Write1d(FName,i,0); }
if (Setup.Twod.WSz==1) for (i=0;i<Setup.Twod.N;++i) { sprintf(FName,"%s/%s%03d.z2d",DName,SelRun,i+1); Write2d16(FName,i,0); }
else                   for (i=0;i<Setup.Twod.N;++i) { sprintf(FName,"%s/%s%03d.z2d",DName,SelRun,i+1); Write2d32(FName,i,0); }
if (Win) gtk_widget_destroy(Win);
}
//----------------------------------------------------------------------------------------------------------------------
void SaveSpectraOk(GtkWidget *W,GtkWidget *Win)
{
gchar DName[MAX_FNAME_LENGTH+10],Messg[MAX_FNAME_LENGTH+50];
GtkWidget *CWin,*But,*Label;

sprintf(DName,"%s/Spec_%s",SpecDir,SelRun);
if (mkdir(DName,0755))
   {
   CWin=gtk_dialog_new(); gtk_grab_add(CWin); gtk_widget_set_uposition(GTK_WIDGET(CWin),387,51);
   gtk_signal_connect_object(GTK_OBJECT(CWin),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(CWin));
   gtk_window_set_title(GTK_WINDOW(CWin),"Overwrite?"); gtk_container_border_width(GTK_CONTAINER(CWin),5);
   sprintf(Messg,"Overwrite %s files?",DName);
   Label=gtk_label_new(Messg); gtk_misc_set_padding(GTK_MISC(Label),10,10);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(CWin)->vbox),Label,TRUE,TRUE,0);
   But=gtk_button_new_from_stock(GTK_STOCK_YES);
   gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(YesSaveSpectra),Win);
   gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(CWin));
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(CWin)->action_area),But,TRUE,TRUE,0);
   But=gtk_button_new_from_stock(GTK_STOCK_NO);
   gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(CWin));
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(CWin)->action_area),But,TRUE,TRUE,0);
   gtk_widget_show_all(CWin);
   }
else YesSaveSpectra(NULL,Win);
}
//----------------------------------------------------------------------------------------------------------------------
void SaveSpectraEntry(GtkWidget *W,gpointer Unused)
{ strcpy(SelRun,gtk_entry_get_text(GTK_ENTRY(W))); }
//----------------------------------------------------------------------------------------------------------------------
void SaveSpectra(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*VBox,*Entry,*Label,*HBox,*But;
gchar Str[72+MAX_DIR_STRLEN];

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                    //Make the window modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(gtk_widget_destroy),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Save Spectra"); gtk_widget_set_uposition(GTK_WIDGET(Win),127,51);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Enter a Run Name\nFiles will be saved as name001.z1d etc\nin %s/Spec_name/",SpecDir);
Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
Entry=gtk_entry_new_with_max_length(MAX_TEXT_FIELD);
gtk_signal_connect(GTK_OBJECT(Entry),"changed",GTK_SIGNAL_FUNC(SaveSpectraEntry),NULL); 
gtk_box_pack_start(GTK_BOX(VBox),Entry,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_from_stock(GTK_STOCK_OK); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveSpectraOk),Win); 
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));     

gtk_widget_show_all(Win);
}
//----------------------------------------------------------------------------------------------------------------------
void ExtractRunName(gchar *Path,gchar *RName) //Extract run name by leaving out the path and leading Spec_ in DirName
{ 
gchar *Str;
Str=rindex(Path,(int)'/'); strcpy(RName,Str+6); 
}
//----------------------------------------------------------------------------------------------------------------------
void FindRuns(gint *NoOfRuns,gchar *RunName[])                       //Changed 23 Mar 2008 for SpecDir/Spec_ directories
//With no subdirs glob("*.z{1,2}d,GLOB_BRACE)" was used, now we use glob("Spec_*",GLOB_ONLYDIR)
{
glob_t PGlob;
gint i;
gchar Str[MAX_DIR_STRLEN+10];

#ifndef GLOB_ONLYDIR
    #define GLOB_ONLYDIR 8196
#endif
sprintf(Str,"%s/Spec_*",SpecDir); glob(Str,GLOB_ONLYDIR,NULL,&PGlob);
*NoOfRuns=PGlob.gl_pathc;
for (i=0;i<*NoOfRuns;++i) ExtractRunName(PGlob.gl_pathv[i],RunName[i]);
globfree(&PGlob);
}
//----------------------------------------------------------------------------------------------------------------------
void ListOfRunsDestroyed(GtkWidget *W,gpointer Win)
{
gtk_grab_remove(GTK_WIDGET(Win));                                                       //Take away the modal property
gtk_widget_destroy(GTK_WIDGET(Win));                                                                  //Destroy window
ProgramState=Free;
}
//----------------------------------------------------------------------------------------------------------------------
void RowSelected(GtkWidget *CList,gint Row,gint Col,GdkEventButton *Event,gpointer Data)
{
gchar *RName;

gtk_clist_get_text(GTK_CLIST(CList),Row,Col,&RName);
strcpy(SelRun,RName);
} 
//----------------------------------------------------------------------------------------------------------------------
void LoadFiles(GtkWidget *W,gpointer Win)       //Used in two contexts: Load files by run name and Load files one by one
{                                            //These two contexts are distinguished only by Win=NULL in the latter case!
gint i,Y,P1,P2;
guint16 XSize,YSize,NZer,NDat;
FILE *Fp;
gchar FName[MAX_FNAME_LENGTH+10],Str[200];
size_t ChansRead;

if (Win) { gtk_grab_remove(GTK_WIDGET(Win)); gtk_widget_destroy(GTK_WIDGET(Win)); }
for (i=0;i<Setup.Oned.N;++i)                                                                           //Read 1d Spectra
    {
    if (Win)                                                                                    //Load files by run name
       { sprintf(FName,"%s/Spec_%s/%s%03d.z1d",SpecDir,SelRun,SelRun,i+1); strcpy(Setup.Files.Oned[i],FName); }
    else strcpy(FName,Setup.Files.Oned[i]);                                                      //Load files one by one
    if (strlen(FName)>0)
       {
       ZeroOned(i);                                                                                //Zero all 1d Spectra
       if ((Fp=fopen(FName,"r")) == NULL) 
          { 
          if (i) sprintf(Str,"Could read only %d 1d spectra\nout of %d!",i,Setup.Oned.N); 
          else   sprintf(Str,"Could not read any 1d spectra!");
          Attention(CLAMP(-350+10*i,-350,350),Str); break; 
          }
       if (Setup.Oned.WSz == 1) ChansRead=fread(&Oned16[Off1d[i]],2,Setup.Oned.Chan[i],Fp);
       else                     ChansRead=fread(&Oned32[Off1d[i]],4,Setup.Oned.Chan[i],Fp);
       fclose(Fp);
       if (ChansRead<Setup.Oned.Chan[i]) 
          { 
          sprintf(Str,"1d Spec# %d read only %d channels\nout of %d!",i+1,ChansRead,Setup.Oned.Chan[i]); 
          Attention(-200,Str); 
          }
       }
    }
for (i=0;i<Setup.Twod.N;i++)
    {
    if (Win)                                                                                    //Load files by run name
       { sprintf(FName,"%s/Spec_%s/%s%03d.z2d",SpecDir,SelRun,SelRun,i+1); strcpy(Setup.Files.Twod[i],FName); }
    else strcpy(FName,Setup.Files.Twod[i]);                                                      //Load files one by one
    if (strlen(FName)>0)
       {
       ZeroTwod(i);                                                                                    //Zero all 2d spectra
       if ((Fp=fopen(FName,"r")) == NULL)
          {
          if (i) sprintf(Str,"Could read only %d 2d spectra\nout of %d!",i,Setup.Twod.N);
          else   sprintf(Str,"Could not read any 2d spectra!");
          Attention(CLAMP(150+10*i,150,400),Str); break; 
          }
       fread(&XSize,2,1,Fp); fread(&YSize,2,1,Fp);
       if ( (XSize != Setup.Twod.XChan[i]) || (YSize != Setup.Twod.YChan[i]) ) 
          { 
          sprintf(Str,"2d Spec# %d is (%dx%d) instead of (%dx%d)!\n",i+1,XSize,YSize,Setup.Twod.XChan[i],
                  Setup.Twod.YChan[i]); 
          Attention(CLAMP(200+10*i,200,400),Str); break; 
          }
       else
          {
          for (Y=0,P2=0;Y<YSize;Y++)
              {
              P1=0;
              do
                 {
                 fread(&NZer,2,1,Fp); fread(&NDat,2,1,Fp); P1+=NZer;
                 if (Setup.Twod.WSz == 1) ChansRead=fread(&Twod16[Off2d[i]+P2+P1],2,NDat,Fp);
                 else                     ChansRead=fread(&Twod32[Off2d[i]+P2+P1],4,NDat,Fp);
                 if (ChansRead<NDat)
                    { sprintf(Str,"2d Spec# %d not read correctly (wrong word size?)",i+1); Attention(250,Str); return; }
                 P1+=NDat;
                 } while (P1<XSize);
              P2+=XSize;
              }
          }
       fclose(Fp);
       }
    }
ProgramState=Free;
RefreshAll();
}
//----------------------------------------------------------------------------------------------------------------------
void ReadOneByOneClosed(GtkWidget *W,gpointer Data)
{
g_free(MulW);                                                                                                //Important
ProgramState=Free;
}
//----------------------------------------------------------------------------------------------------------------------
void Select1(GtkWidget *W,gpointer Unused)
{
gint i,j;
GList *Node;
gchar Str[MAX_FNAME_LENGTH+5];
gfloat End;

Node=g_list_last(GTK_TABLE(MulW[0])->children);                                                 //First element of table
for (i=0;i<Row1;i++) for (j=0;j<2;j++) Node=g_list_previous(Node);                                //Skip nodes upto Row1
Node=g_list_previous(Node);                                                                           //Skip 1 more node
sprintf(Setup.Files.Oned[Row1],"%s/%s",FileX->Path,FileX->TargetFile);                                 //Store full path
AbbreviateFileName(Str,Setup.Files.Oned[Row1],18);
gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),Str);    //Change text inside button
End=(Row1/8)*216.0;                                                                       //Fudging to scroll to the end
gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(MulW[2]))),End);
Row1++; if (Row1==Setup.Oned.N) Row1=0;                                          //Automatically advance to the next row
}
//----------------------------------------------------------------------------------------------------------------------
void OnedFile(GtkWidget *W,gpointer Data)
{
Row1=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 1d File(s)",NULL,11,50,TRUE,SpecDir,".z1d",FALSE,&Select1,TRUE);
}
//----------------------------------------------------------------------------------------------------------------------
void Select2(GtkWidget *W,gpointer Unused)
{
gint i,j;
GList *Node;
gchar Str[MAX_FNAME_LENGTH+5];
gfloat End;

Node=g_list_last(GTK_TABLE(MulW[1])->children);                                                 //First element of table
for (i=0;i<Row2;i++) for (j=0;j<2;j++) Node=g_list_previous(Node);                                //Skip nodes upto Row1
Node=g_list_previous(Node);                                                                           //Skip 1 more node
sprintf(Setup.Files.Twod[Row2],"%s/%s",FileX->Path,FileX->TargetFile);                                 //Store full path
AbbreviateFileName(Str,Setup.Files.Twod[Row2],18);
gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),Str);    //Change text inside button
End=(Row1/8)*216.0;                                                                       //Fudging to scroll to the end
gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(MulW[2]))),End);
Row2++; if (Row2==Setup.Twod.N) Row2=0;                                          //Automatically advance to the next row
}
//----------------------------------------------------------------------------------------------------------------------
void TwodFile(GtkWidget *W,gpointer Data)
{
Row2=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 2d File(s)",NULL,11,50,TRUE,SpecDir,".z2d",FALSE,&Select2,TRUE);
}
//----------------------------------------------------------------------------------------------------------------------
void ReadOneByOneOk(GtkWidget *W,GtkWidget *Win)
{
FILE *Fp;
gint i;

gtk_widget_destroy(Win);                                                //This will automatically call ReadOneByOneClosed
LoadFiles(NULL,NULL);                                 //This will read in the files and refresh any open spectrum windows
if ((Fp=fopen("lamps.lst","w")) != NULL)                                                         //Save the list of files
   {
   fprintf(Fp,"%d 1d File Names:\n",Setup.Oned.N);
   for (i=0;i<Setup.Oned.N;i++) fprintf(Fp,"%d %s\n",i+1,Setup.Files.Oned[i]);
   fprintf(Fp,"%d 2d File Names:\n",Setup.Twod.N);
   for (i=0;i<Setup.Twod.N;i++) fprintf(Fp,"%d %s\n",i+1,Setup.Files.Twod[i]);
   fclose(Fp);
   }
}
//----------------------------------------------------------------------------------------------------------------------
void Clear1dNames(GtkWidget *W,gpointer Unused)
{
gint i;
GList *Node;

Node=g_list_last(GTK_TABLE(MulW[0])->children);                                                  //First element of table
Node=g_list_previous(Node);                                                                                 //Skip 1 node
for (i=0;i<Setup.Oned.N;i++)
    {
    strcpy(Setup.Files.Oned[i],"");                                                                           //Blank out
    gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),"");                  //Blank out
    if (i<Setup.Oned.N-1) { Node=g_list_previous(Node); Node=g_list_previous(Node); }                      //Skip 2 nodes
    }
Row1=0;
}
//----------------------------------------------------------------------------------------------------------------------
void Clear2dNames(GtkWidget *W,gpointer Unused)
{
gint i;
GList *Node;

Node=g_list_last(GTK_TABLE(MulW[1])->children);                                                    //First element of table
Node=g_list_previous(Node);                                                                                   //Skip 1 node
for (i=0;i<Setup.Twod.N;i++)
    {
    strcpy(Setup.Files.Twod[i],"");                                                                             //Blank out
    gtk_label_set_text(GTK_LABEL(GTK_BIN(((GtkTableChild *)Node->data)->widget)->child),"");                    //Blank out
    if (i<Setup.Twod.N-1) { Node=g_list_previous(Node); Node=g_list_previous(Node); }                        //Skip 2 nodes
    }
Row2=0;
}
//----------------------------------------------------------------------------------------------------------------------
void ReadOneByOne(GtkWidget *W,gpointer Unused)
{
static gchar *Titles[2]= {"Spec\nNo","    File Name\n(Click to browse)"};
gint ColWidth[2]={44,130};
static GdkColor TitlesBg  = {0,0x0000,0x0000,0x0000};
static GdkColor TitlesFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *TitlesStyle;
GtkWidget *Win,*VBox,*HBox,*Sc,*Table,*But,*Label;
gint i,NFiles,FNo;
gchar Str[MAX_FNAME_LENGTH+5];
FILE *Fp;

if (ProgramState != Free) { Attention(0,"Program is not in the Free State"); return; }
ProgramState=Reading;

if ((Fp=fopen("lamps.lst","r")) != NULL)                                                 //Get file names from lamps.lst
   {
   fgets(Str,75,Fp); sscanf(Str,"%d",&NFiles);
   for (i=0;i<NFiles;i++) { fgets(Str,75,Fp); sscanf(Str,"%d %s",&FNo,Setup.Files.Oned[i]); }
   fgets(Str,75,Fp); sscanf(Str,"%d",&NFiles);
   for (i=0;i<NFiles;i++) { fgets(Str,75,Fp); sscanf(Str,"%d %s",&FNo,Setup.Files.Twod[i]); }
   fclose(Fp);
   }

MulW=g_new(GtkWidget *,4);      //MulW[0-1] are the two tables and [2-3] are the scrl wins. g_free in ReadOneByOneClosed
                  //We need the tables to update the file names and the scrl wins so that we can scroll them if required
TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { TitlesStyle->fg[i]=TitlesStyle->text[i]=TitlesFg; TitlesStyle->bg[i]=TitlesBg; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_title(GTK_WINDOW(Win),"Read 1d and 2d Spectra");
gtk_widget_set_uposition(GTK_WIDGET(Win),550,50);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(ReadOneByOneClosed),NULL);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),5);                                                  //Breathing room

HBox=gtk_hbox_new(TRUE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Oned Files"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);
Label=gtk_label_new("Twod Files"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,0);

HBox=gtk_hbox_new(TRUE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);

Sc=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Sc),GTK_POLICY_NEVER,GTK_POLICY_NEVER);
gtk_widget_set_size_request(Sc,200,-1);
gtk_box_pack_start(GTK_BOX(HBox),Sc,TRUE,FALSE,0);
Table=gtk_table_new(1,2,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),2); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Sc),Table);                               //Pack Table into Sc
for (i=0;i<2;i++)
    {
    But=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(But,TitlesStyle);
    gtk_widget_set_size_request(But,ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

Sc=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Sc),GTK_POLICY_NEVER,GTK_POLICY_NEVER);
gtk_widget_set_size_request(Sc,200,-1);
gtk_box_pack_start(GTK_BOX(HBox),Sc,TRUE,FALSE,0);
Table=gtk_table_new(1,2,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),2); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Sc),Table);                               //Pack Table into Sc
for (i=0;i<2;i++)
    {
    But=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(But,TitlesStyle);
    gtk_widget_set_size_request(But,ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(TRUE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

MulW[2]=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(MulW[2]),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_widget_set_size_request(MulW[2],200,220);
gtk_box_pack_start(GTK_BOX(HBox),MulW[2],TRUE,FALSE,0);
MulW[0]=gtk_table_new(Setup.Oned.N,2,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(MulW[0]),0); gtk_table_set_col_spacings(GTK_TABLE(MulW[0]),2);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(MulW[2]),MulW[0]);                   //Pack Table into MulW[2]
for (i=0;i<Setup.Oned.N;i++)
    {
    sprintf(Str,"%d",i+1); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(But,ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(MulW[0]),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    AbbreviateFileName(Str,Setup.Files.Oned[i],18); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(But,ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(MulW[0]),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OnedFile),GINT_TO_POINTER(i));
    }

MulW[3]=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(MulW[3]),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_widget_set_size_request(MulW[3],200,220);
gtk_box_pack_start(GTK_BOX(HBox),MulW[3],TRUE,FALSE,0);
MulW[1]=gtk_table_new(Setup.Twod.N,2,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(MulW[1]),0); gtk_table_set_col_spacings(GTK_TABLE(MulW[1]),2);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(MulW[3]),MulW[1]);                   //Pack Table into MulW[3]
for (i=0;i<Setup.Twod.N;i++)
    {
    sprintf(Str,"%d",i+1); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(But,ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(MulW[1]),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    AbbreviateFileName(Str,Setup.Files.Twod[i],18); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(But,ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(MulW[1]),But,1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(TwodFile),GINT_TO_POINTER(i));
    }

HBox=gtk_hbox_new(TRUE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Clear 1d Names"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Clear1dNames),NULL);
But=gtk_button_new_with_label("Clear 2d Names"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Clear2dNames),NULL);
But=gtk_button_new_with_label("Ok"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReadOneByOneOk),Win);
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
gtk_style_unref(TitlesStyle);
}
//----------------------------------------------------------------------------------------------------------------------
void ReadSpectra(GtkWidget *W,gpointer Unused)
{
gint i,NoOfRuns;
GtkWidget *Win,*HBox,*VBox,*Label,*But,*ScrollW,*CList;
static gchar *Titles[1] = {"Run Name"};
gchar *Txt[1],*RunName[MAX_RUNS],Str[MAX_DIR_STRLEN+20];

if (ProgramState != Free) { Attention(0,"Program is not in the Free State"); return; }
ProgramState=Reading;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                      //Make the window modal
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(ListOfRunsDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"List of Runs"); gtk_widget_set_uposition(GTK_WIDGET(Win),127,51);
gtk_container_set_border_width(GTK_CONTAINER(Win),5);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Click on a run name and then select Ok");
gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);
ScrollW=gtk_scrolled_window_new(NULL,NULL); gtk_widget_set_size_request(ScrollW,-1,100);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,TRUE,TRUE,0);
CList=gtk_clist_new_with_titles(1,Titles); gtk_clist_set_shadow_type(GTK_CLIST(CList),GTK_SHADOW_OUT);
gtk_clist_set_column_width(GTK_CLIST(CList),0,7*(MAX_TEXT_FIELD+2));
gtk_signal_connect(GTK_OBJECT(CList),"select_row",GTK_SIGNAL_FUNC(RowSelected),NULL);
gtk_container_add(GTK_CONTAINER(ScrollW),CList);
for (i=0;i<MAX_RUNS;i++) RunName[i]=g_malloc((MAX_TEXT_FIELD+9)*sizeof(gchar));
FindRuns(&NoOfRuns,RunName);
for (i=0;i<NoOfRuns;i++) { Txt[0]=RunName[i]; gtk_clist_append(GTK_CLIST(CList),Txt); }
for (i=0;i<MAX_RUNS;i++) g_free(RunName[i]);
strcpy(SelRun,"");                                                                          //No run selected by default
if (!NoOfRuns)
   {
   sprintf(Str,"No runs in %s",SpecDir);
   Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,TRUE,0);
   }

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_from_stock(GTK_STOCK_OK); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(LoadFiles),Win); 
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ListOfRunsDestroyed),Win);     

gtk_widget_show_all(Win);
}
//----------------------------------------------------------------------------------------------------------------------
