//util.c:: Utilities for manipulating 1d and 2d spectra
#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include "lamps.h"

/*Function templates*/
void Attention(gint XPos,gchar *Messg);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void WinClosed(GtkWidget *W,GtkWidget *Win);                                    //This function is to be found in setup.c
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void Readz2d(gint wsize,gshort xsize,gshort ysize,FILE *Fp,guint16 *Data16,guint32 *Data32,gboolean *Error);  //see useful.c
void InitMat16(gshort XMax,gshort YMax,guint16 *Data);                                                        //see useful.c
void InitMat32(gshort XMax,gshort YMax,guint32 *Data);                                                        //see useful.c
void Writez2d16(gshort XMax,gshort YMax,FILE *Fp,guint16 *Data,gboolean *Error);                              //see useful.c
void Writez2d32(gshort XMax,gshort YMax,FILE *Fp,guint32 *Data,gboolean *Error);                              //see useful.c
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);

/*Function templates and global variables for threads*/
void *ProcessAdd2d(gpointer Data);

/*External Global Variables*/
extern gboolean UtilBusy;                    //Flag indicating that Utilities is in use. Prevents running multiple utilities
extern struct FileSelectType *FileX;
extern gint TopOfset;                                             //Accounts for space occupied by window manager at the top

/*Global Variables in this file only*/
struct Util1 {
             GtkWidget *FileBut; GtkWidget *OutEntry; GtkWidget *BitCombo; gchar InFName[MAX_FNAME_LENGTH]; 
             gboolean Ready;
             };
struct Util1 *Util1;                                                                //Structure used for 1d Spec to Ascii
struct Util2 {GtkWidget *NSpin; GtkWidget *OutEntry; GtkWidget *SizeEntry[MAX_ADD_SPEC]; 
              GtkWidget *BitCombo; GtkWidget *OutBitCombo;
              GtkWidget *FileButs[MAX_ADD_SPEC]; gchar FNames[MAX_ADD_SPEC][MAX_FNAME_LENGTH]; 
              GtkWidget *MultEntry[MAX_ADD_SPEC]; GtkWidget *ScrlW; gboolean BrowseOn; gint Row;
              GtkWidget *Win;};
struct Util2 *Util2;                                                                 //Structure used for Add1d and Add2d
struct Util3 {GtkWidget *FileBut; gchar InFName[MAX_FNAME_LENGTH]; GtkWidget *OutEntry;
              GtkWidget *FBytes; GtkWidget *BitCombo; GtkWidget *XSize; GtkWidget *XComp;
              GtkWidget *YSize; GtkWidget *YComp;}; 
                                                                                    //Used by Chop1d,Chop2d,Comp1d,Comp2d
struct Util3 *Util3;

/*---------------------------------------------------------------------------------------------------------------------*/
void Chop2dDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (GTK_IS_WIDGET(Win)) gtk_widget_destroy(Win);
g_free(Util3);                                                                                 //Destroy global structure
UtilBusy=FALSE;
}
/*---------------------------------------------------------------------------------------------------------------------*/
void SelectToChop2d(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Util3->InFName,"%s/%s",FileX->Path,FileX->TargetFile);                                          //Store full path
g_free(FileX);
AbbreviateFileName(Str,Util3->InFName,MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Util3->FileBut)->child),Str);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void BrowseChop2d(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 2d File",NULL,460,TOP_SPACE+TopOfset,TRUE,".",".z2d",FALSE,&SelectToChop2d,FALSE);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void ProceedChop2d(GtkWidget *W,GtkWidget *Win)
{
const gchar *OutFName;
FILE *Inp,*Out;
guint32 Spec32[16384];
guint16 Spec16[16384];
gint i,Y,P1,P2;
guint16 XSize,YSize,NZer,NDat;

OutFName=gtk_entry_get_text(GTK_ENTRY(Util3->OutEntry));
if (!(Inp=fopen(Util3->InFName,"r"))) { Attention(0,"Failed to open Input File"); return; }
if (!(Out=fopen(OutFName,"w"))) { Attention(0,"Failed to open Output File"); return; }

fread(&XSize,2,1,Inp); fread(&YSize,2,1,Inp); fwrite(&XSize,2,1,Out); fwrite(&YSize,2,1,Out);
for (Y=0,P2=0;Y<YSize;Y++)
    {
    P1=0;
    do
       {
       fread(&NZer,2,1,Inp); fread(&NDat,2,1,Inp); fwrite(&NZer,2,1,Out); fwrite(&NDat,2,1,Out); 
       P1+=NZer;
       if (fread(Spec32,4,NDat,Inp)<NDat) { Attention(0,"Spec read error"); fclose(Inp); fclose(Out); return; }
       for (i=0;i<NDat;i++) Spec16[i]=(guint16)Spec32[i];
       if (fwrite(Spec16,2,NDat,Out)<NDat) { Attention(0,"Spec write error"); fclose(Inp); fclose(Out); return; }
       P1+=NDat;
       } while (P1<XSize);
    P2+=XSize;
    }
fclose(Inp); fclose(Out);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void Chop2d(GtkWidget *W,gpointer Data)                                         //Chops 2d spectrum from 32-bit to 16-bit
{
GtkWidget *Win,*VBox,*HBox,*Label,*But;

if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;

Util3=g_new(struct Util3,1);                                   //Create new structure to be destroyed in Chop2dDestroyed()

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(Win,129,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(Chop2dDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Chop 2d Spectrum");

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Chops a 2d Spectrum file from 32-bit to 16-bit"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new(" Input 32-bit File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->FileBut=gtk_button_new_with_label("Browse");
gtk_box_pack_start(GTK_BOX(HBox),Util3->FileBut,FALSE,FALSE,10);
gtk_signal_connect(GTK_OBJECT(Util3->FileBut),"clicked",GTK_SIGNAL_FUNC(BrowseChop2d),NULL);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output 16-bit File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_box_pack_start(GTK_BOX(HBox),Util3->OutEntry,FALSE,FALSE,4);
gtk_entry_set_text(GTK_ENTRY(Util3->OutEntry),"outfile.z2d");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedChop2d),Win);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void Chop1dDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (GTK_IS_WIDGET(Win)) gtk_widget_destroy(Win);
g_free(Util3);                                                                                //Destroy global structure
UtilBusy=FALSE;
}
/*---------------------------------------------------------------------------------------------------------------------*/
void SelectToChop1d(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Util3->InFName,"%s/%s",FileX->Path,FileX->TargetFile);                                          //Store full path
g_free(FileX);
AbbreviateFileName(Str,Util3->InFName,MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Util3->FileBut)->child),Str);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void BrowseChop1d(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 1d File",NULL,415,TOP_SPACE+TopOfset,TRUE,".",".z1d",FALSE,&SelectToChop1d,FALSE);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void ProceedChop1d(GtkWidget *W,GtkWidget *Win)
{
const gchar *OutFName;
FILE *Fp;
guint32 Spec32[16384];
guint16 Spec16[16384];
gint WdsRead,i;

OutFName=gtk_entry_get_text(GTK_ENTRY(Util3->OutEntry));
if (!(Fp=fopen(Util3->InFName,"r"))) { Attention(0,"Failed to open Input File"); return; }
WdsRead=fread(Spec32,4,16384,Fp); fclose(Fp);
for (i=0;i<WdsRead;i++) Spec16[i]=(guint16)Spec32[i];
if (!(Fp=fopen(OutFName,"w"))) { Attention(0,"Failed to open Output File"); return; }
fwrite(Spec16,2,WdsRead,Fp); fclose(Fp);
}
/*---------------------------------------------------------------------------------------------------------------------*/
void Chop1d(GtkWidget *W,gpointer Data)                                        //Chops 1d spectrum from 32-bit to 16-bit
{
GtkWidget *Win,*VBox,*HBox,*Label,*But;

if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;

Util3=g_new(struct Util3,1);                                 //Create new structure to be destroyed in Chop1dDestroyed()

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(Win,129,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(Chop1dDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Chop 1d Spectrum");

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Chops a 1d Spectrum file from 32-bit to 16-bit"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new(" Input 32-bit File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->FileBut=gtk_button_new_with_label("Browse");
gtk_box_pack_start(GTK_BOX(HBox),Util3->FileBut,FALSE,FALSE,10);
gtk_signal_connect(GTK_OBJECT(Util3->FileBut),"clicked",GTK_SIGNAL_FUNC(BrowseChop1d),NULL);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output 16-bit File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); gtk_box_pack_start(GTK_BOX(HBox),Util3->OutEntry,FALSE,FALSE,4);
gtk_entry_set_text(GTK_ENTRY(Util3->OutEntry),"outfile.z1d");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedChop1d),Win);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void CompDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (GTK_IS_WIDGET(Win)) gtk_widget_destroy(Win);
g_free(Util3);                                                                                //Destroy global structure
UtilBusy=FALSE;
}
/*--------------------------------------------------------------------------------------------------------------------*/
void SelectToComp1d(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5]; const gchar *BitType;
struct stat StatBuf;
gulong Byts;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Util3->InFName,"%s/%s",FileX->Path,FileX->TargetFile);
g_free(FileX);

AbbreviateFileName(Str,Util3->InFName,MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Util3->FileBut)->child),Str);
if (!stat(Util3->InFName,&StatBuf)) Byts=StatBuf.st_size; else Byts=0;
sprintf(Str,"%ld",Byts);
gtk_entry_set_text(GTK_ENTRY(Util3->FBytes),Str);
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->BitCombo)->entry));
if (BitType[0]=='3') sprintf(Str,"%ld",Byts/4);                                                            //32-bit case
else                 sprintf(Str,"%ld",Byts/2);                                                            //16-bit case
gtk_entry_set_text(GTK_ENTRY(Util3->XSize),Str);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BrowseComp1d(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 1d File",NULL,375,TOP_SPACE+TopOfset,TRUE,".",".z1d",FALSE,&SelectToComp1d,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ProceedComp1d(GtkWidget *W,GtkWidget *Win)
{
const gchar *OutFName,*BitType; gchar Str[120];
FILE *Fp;
guint32 Inp32[16384],Out32[16384];
guint16 Inp16[16384],Out16[16384];
gint Ch,WdsRead,CFact,ChNew,i;

OutFName=gtk_entry_get_text(GTK_ENTRY(Util3->OutEntry));
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->BitCombo)->entry));
CFact=atoi(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->XComp)->entry)));
if (!(Fp=fopen(Util3->InFName,"r"))) { Attention(0,"Failed to open Input File"); return; }
if (BitType[0]=='3')                                                                                          //32-bit case
   {
   WdsRead=fread(Inp32,4,16384,Fp); fclose(Fp);
   ChNew=WdsRead/CFact; if (ChNew<16) { ChNew=16; CFact=WdsRead/ChNew; }
   for (Ch=0;Ch<ChNew;++Ch) { Out32[Ch]=0; for (i=0;i<CFact;++i) Out32[Ch]+=Inp32[CFact*Ch+i]; }
   if (!(Fp=fopen(OutFName,"w"))) { Attention(0,"Failed to open Output File"); return; }
   fwrite(Out32,4,ChNew,Fp); fclose(Fp); 
   }
else
   {
   WdsRead=fread(Inp16,2,16384,Fp); fclose(Fp);
   ChNew=WdsRead/CFact; if (ChNew<16) { ChNew=16; CFact=WdsRead/ChNew; }
   for (Ch=0;Ch<ChNew;++Ch) { Out16[Ch]=0; for (i=0;i<CFact;++i) Out16[Ch]+=Inp16[CFact*Ch+i]; }
   if (!(Fp=fopen(OutFName,"w"))) { Attention(0,"Failed to open Output File"); return; }
   fwrite(Out16,2,ChNew,Fp); fclose(Fp);
   }
sprintf(Str,"File: %s %d Chs written",OutFName,ChNew);
Attention(0,Str);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void BitComp1d(GtkWidget *W,gpointer Unused)
{
gulong Byts;
const gchar *BitType; gchar Str[80];
struct stat StatBuf;

Byts=0;
if (strlen(Util3->InFName)) if (!stat(Util3->InFName,&StatBuf)) Byts=StatBuf.st_size;
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->BitCombo)->entry));
if (BitType[0]=='3') sprintf(Str,"%ld",Byts/4);                                                                //32-bit case
else                 sprintf(Str,"%ld",Byts/2);                                                                //16-bit case
gtk_entry_set_text(GTK_ENTRY(Util3->XSize),Str);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Comp1d(GtkWidget *W,gpointer Data)                                                            //Compress a 1d Spectrum
{
GtkWidget *Win,*VBox,*HBox,*Label,*But;
GList *GList1,*GList2;

if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;

Util3=g_new(struct Util3,1);                                            //Create new structure to be destroyed when finished
strcpy(Util3->InFName,"");

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(Win,20,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(CompDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Compress 1d Spec.");

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Reduces the number of channels in 1d spectra"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Input 1d File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->FileBut=gtk_button_new_with_label("Browse");
gtk_box_pack_start(GTK_BOX(HBox),Util3->FileBut,FALSE,FALSE,10);
gtk_signal_connect(GTK_OBJECT(Util3->FileBut),"clicked",GTK_SIGNAL_FUNC(BrowseComp1d),NULL);

Util3->BitCombo=gtk_combo_new();
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util3->BitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util3->BitCombo,TRUE,FALSE,0); GList1=NULL;
GList1=g_list_append(GList1,"32-bit"); GList1=g_list_append(GList1,"16-bit");
gtk_combo_set_popdown_strings(GTK_COMBO(Util3->BitCombo),GList1);
gtk_signal_connect(GTK_OBJECT(GTK_COMBO(Util3->BitCombo)->entry),"changed",GTK_SIGNAL_FUNC(BitComp1d),NULL);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Bytes:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->FBytes=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),Util3->FBytes,FALSE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(Util3->FBytes),""); gtk_entry_set_editable(GTK_ENTRY(Util3->FBytes),FALSE);
gtk_widget_set_size_request(Util3->FBytes,50,-1);
Label=gtk_label_new("Chans:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,6);
Util3->XSize=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),Util3->XSize,FALSE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(Util3->XSize),""); gtk_entry_set_editable(GTK_ENTRY(Util3->XSize),FALSE);
gtk_widget_set_usize(Util3->XSize,50,22);
Label=gtk_label_new("Comp. Factor:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,6);
Util3->XComp=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Util3->XComp),50,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util3->XComp)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util3->XComp,TRUE,FALSE,0); GList2=NULL;
GList2=g_list_append(GList2,"2"); GList2=g_list_append(GList2,"4"); GList2=g_list_append(GList2,"8"); 
GList2=g_list_append(GList2,"16"); GList2=g_list_append(GList2,"32"); GList2=g_list_append(GList2,"64"); 
GList2=g_list_append(GList2,"128"); GList2=g_list_append(GList2,"256"); GList2=g_list_append(GList2,"512");
GList2=g_list_append(GList2,"1024"); gtk_combo_set_popdown_strings(GTK_COMBO(Util3->XComp),GList2);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output 1d File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_box_pack_start(GTK_BOX(HBox),Util3->OutEntry,FALSE,FALSE,4);
gtk_entry_set_text(GTK_ENTRY(Util3->OutEntry),"outfile.z1d");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedComp1d),Win);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win); g_list_free(GList1); g_list_free(GList2);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SelectToComp2d(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5]; const gchar *BitType;
guint16 XSize,YSize; 
FILE *Fp;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Util3->InFName,"%s/%s",FileX->Path,FileX->TargetFile);
g_free(FileX);

AbbreviateFileName(Str,Util3->InFName,MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Util3->FileBut)->child),Str);

XSize=YSize=0;
if (!(Fp=fopen(Util3->InFName,"r"))) { Attention(0,"Failed to open Input File"); return; }
fread(&XSize,2,1,Fp); fread(&YSize,2,1,Fp); fclose(Fp);

sprintf(Str,"%d",XSize); gtk_entry_set_text(GTK_ENTRY(Util3->XSize),Str);
sprintf(Str,"%d",YSize); gtk_entry_set_text(GTK_ENTRY(Util3->YSize),Str);
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->BitCombo)->entry));
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BrowseComp2d(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 2d File",NULL,375,TOP_SPACE+TopOfset,TRUE,".",".z2d",FALSE,&SelectToComp2d,FALSE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ProceedComp2d(GtkWidget *W,GtkWidget *Win)
{
const gchar *OutFName,*BitType; gchar Str[120];
FILE *Fp;
gint XFact,YFact,WSize,Nx,Ny,X,Y;
guint16 XSize,YSize,NXSize,NYSize;
guint16 *Data16,*Outp16;
guint32 *Data32,*Outp32;
gboolean Error;

OutFName=gtk_entry_get_text(GTK_ENTRY(Util3->OutEntry));
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->BitCombo)->entry));
XFact=atoi(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->XComp)->entry)));
YFact=atoi(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util3->YComp)->entry)));
if (!(Fp=fopen(Util3->InFName,"r"))) { Attention(0,"Failed to open Input File"); return; }
fread(&XSize,2,1,Fp); fread(&YSize,2,1,Fp);                                                     //Begin reading the 2d file
NXSize=CLAMP(XSize/XFact,16,8192); NYSize=CLAMP(YSize/YFact,16,8192);
XFact=XSize/NXSize; YFact=YSize/NYSize;
if (BitType[0]=='1') 
   { 
   WSize=1;
   Data16=g_new(guint16,XSize*YSize); InitMat16(XSize,YSize,Data16); 
   Outp16=g_new(guint16,NXSize*NYSize); InitMat16(NXSize,NYSize,Outp16); 
   }
else                 
   { 
   WSize=2; 
   Data32=g_new(guint32,XSize*YSize); InitMat32(XSize,YSize,Data32); 
   Outp32=g_new(guint32,NXSize*NYSize); InitMat32(NXSize,NYSize,Outp32); 
   }
Readz2d(WSize,XSize,YSize,Fp,Data16,Data32,&Error);                                                          //Read 2d file
fclose(Fp);
if (Error) 
   { 
   Attention(0,"Error Reading...wrong word size?"); 
   if (WSize==1) g_free(Data16); else g_free(Data32);
   return; 
   }
for (Nx=0;Nx<NXSize;++Nx) for (Ny=0;Ny<NYSize;++Ny)
    {
    if (WSize==1) Outp16[Nx+NXSize*Ny]=0; else Outp32[Nx+NXSize*Ny]=0;
    for (X=Nx*XFact;X<(Nx+1)*XFact;++X) for (Y=Ny*YFact;Y<(Ny+1)*YFact;++Y) 
        { 
        if (WSize==1) Outp16[Nx+NXSize*Ny]+=Data16[X+XSize*Y]; 
        else Outp32[Nx+NXSize*Ny]+=Data32[X+XSize*Y];
        }
    }

if (!(Fp=fopen(OutFName,"w"))) 
   { if (WSize==1) g_free(Data16); else g_free(Data32); Attention(0,"Failed to open output File"); return; }
if (WSize==1) { Writez2d16(NXSize,NYSize,Fp,Outp16,&Error); g_free(Data16); }
else          { Writez2d32(NXSize,NYSize,Fp,Outp32,&Error); g_free(Data32); }
fclose(Fp);
if (!Error)
   {
   if (WSize==2) sprintf(Str,"File: %s (%dx%d) 32-bit written",OutFName,NXSize,NYSize); 
   else          sprintf(Str,"File: %s (%dx%d) 16-bit written",OutFName,NXSize,NYSize);
   }
else sprintf(Str,"Could not write file: %s",OutFName);
Attention(0,Str);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Comp2d(GtkWidget *W,gpointer Data)                                                          //Compress a 2d Spectrum
{
GtkWidget *Win,*VBox,*HBox,*Label,*But;
GList *GList1,*GList2;

if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;

Util3=g_new(struct Util3,1);                                            //Create new structure to be destroyed when finished
strcpy(Util3->InFName,"");

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(Win,20,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(CompDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Compress 2d Spec.");

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Reduces the number of X/Y channels in 2d spectra"); 
gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Input 2d File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->FileBut=gtk_button_new_with_label("Browse");
gtk_box_pack_start(GTK_BOX(HBox),Util3->FileBut,FALSE,FALSE,10);
gtk_signal_connect(GTK_OBJECT(Util3->FileBut),"clicked",GTK_SIGNAL_FUNC(BrowseComp2d),NULL);

Util3->BitCombo=gtk_combo_new();
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util3->BitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util3->BitCombo,TRUE,FALSE,0); GList1=NULL;
GList1=g_list_append(GList1,"32-bit"); GList1=g_list_append(GList1,"16-bit");
gtk_combo_set_popdown_strings(GTK_COMBO(Util3->BitCombo),GList1);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("XSize:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,32);
Util3->XSize=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),Util3->XSize,FALSE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(Util3->XSize),""); gtk_entry_set_editable(GTK_ENTRY(Util3->XSize),FALSE);
gtk_widget_set_size_request(Util3->XSize,50,-1);
Label=gtk_label_new("YSize:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,45);
Util3->YSize=gtk_entry_new(); gtk_box_pack_start(GTK_BOX(HBox),Util3->YSize,FALSE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(Util3->YSize),""); gtk_entry_set_editable(GTK_ENTRY(Util3->YSize),FALSE);
gtk_widget_set_size_request(Util3->YSize,50,-1);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);

Label=gtk_label_new("X Comp. Factor:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->XComp=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Util3->XComp),50,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util3->XComp)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util3->XComp,TRUE,FALSE,0); GList2=NULL; GList2=g_list_append(GList2,"1");
GList2=g_list_append(GList2,"2"); GList2=g_list_append(GList2,"4"); GList2=g_list_append(GList2,"8"); 
GList2=g_list_append(GList2,"16"); GList2=g_list_append(GList2,"32"); GList2=g_list_append(GList2,"64"); 
GList2=g_list_append(GList2,"128"); GList2=g_list_append(GList2,"256"); GList2=g_list_append(GList2,"512");
GList2=g_list_append(GList2,"1024"); gtk_combo_set_popdown_strings(GTK_COMBO(Util3->XComp),GList2);

Label=gtk_label_new("Y Comp. Factor:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,6);
Util3->YComp=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Util3->YComp),50,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util3->YComp)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util3->YComp,TRUE,FALSE,0);
gtk_combo_set_popdown_strings(GTK_COMBO(Util3->YComp),GList2);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output 2d File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util3->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_box_pack_start(GTK_BOX(HBox),Util3->OutEntry,FALSE,FALSE,4);
gtk_entry_set_text(GTK_ENTRY(Util3->OutEntry),"outfile.z2d");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedComp2d),Win);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win); g_list_free(GList1); g_list_free(GList2);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Get2dSpec16(gchar *FName,gint ChX,gint ChY,guint16 *Spec)
{
gint i,Y,P1,P2;
FILE *Fp;
guint16 XSize,YSize,NZer,NDat;
gchar Str[MAX_FNAME_LENGTH+20];

for (i=0;i<ChX*ChY;i++) Spec[i]=0;
if ((Fp=fopen(FName,"r")) == NULL) { sprintf(Str,"Error opening %s",FName); Attention(0,Str); return; }
fread(&XSize,2,1,Fp); fread(&YSize,2,1,Fp);
if ( (XSize != ChX) || (YSize != ChY) ) { Attention(0,"Spec size mismatch"); return; }
for (Y=0,P2=0;Y<YSize;Y++)
    {
    P1=0;
    do
       {
       fread(&NZer,2,1,Fp); fread(&NDat,2,1,Fp); P1+=NZer;
       if (fread(&Spec[P2+P1],2,NDat,Fp)<NDat) { Attention(0,"Spec read error"); return; }
       P1+=NDat;
       } while (P1<XSize);
    P2+=XSize;
    }
fclose(Fp);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Get2dSpec32(gchar *FName,gint ChX,gint ChY,guint32 *Spec)
{
gint i,Y,P1,P2;
FILE *Fp;
guint16 XSize,YSize,NZer,NDat;
gchar Str[MAX_FNAME_LENGTH+20];

for (i=0;i<ChX*ChY;i++) Spec[i]=0;
if ((Fp=fopen(FName,"r")) == NULL) { sprintf(Str,"Error opening %s",FName); Attention(0,Str); return; }
fread(&XSize,2,1,Fp); fread(&YSize,2,1,Fp);
if ( (XSize != ChX) || (YSize != ChY) ) { Attention(0,"Spec size mismatch"); return; }
for (Y=0,P2=0;Y<YSize;Y++)
    {
    P1=0;
    do
       {
       fread(&NZer,2,1,Fp); fread(&NDat,2,1,Fp); P1+=NZer;
       if (fread(&Spec[P2+P1],4,NDat,Fp)<NDat) { Attention(0,"Spec read error"); return; }
       P1+=NDat;
       } while (P1<XSize);
    P2+=XSize;
}
fclose(Fp);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Save2dSpec16(const gchar *FName,gint ChX,gint ChY,gdouble *Spec)
{
FILE *Fp;
gchar Str[200];
gint Row,P1,P2,i;
guint16 XSize,YSize,NZer,NDat,x16;

if ((Fp=fopen(FName,"w")) == NULL) { sprintf(Str,"Error opening %s",FName); Attention(0,Str); return; }
XSize=ChX; YSize=ChY;
if (fwrite(&XSize,2,1,Fp) < 1) { Attention(0,"File write error"); return; }
if (fwrite(&YSize,2,1,Fp) < 1) { Attention(0,"File write error"); return; }
for (Row=0;Row<ChY;Row++)
    {
    P1=0;
    while (P1<XSize)
       {
       P2=P1;
       while ( (!(guint16)Spec[XSize*Row+P2]) && (P2<XSize) ) P2++; NZer=P2-P1;
       if (fwrite(&NZer,2,1,Fp) < 1) { Attention(0,"File write error"); return; };                     //Output no of zeroes
       P1=P2;
       while ( ((guint16)Spec[XSize*Row+P2]) && (P2<XSize) ) P2++; NDat=P2-P1;
       if (fwrite(&NDat,2,1,Fp) < 1) { Attention(0,"File write error"); return; };
       for (i=0;i<NDat;i++) { x16=(guint16)Spec[XSize*Row+P1+i]; fwrite(&x16,2,1,Fp); }         //Output no of non-zero data
       P1=P2;
       }
    }
fclose(Fp);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Save2dSpec32(const gchar *FName,gint ChX,gint ChY,gdouble *Spec)
{
FILE *Fp;
gchar Str[200];
gint Row,P1,P2,i;
guint16 XSize,YSize,NZer,NDat;
guint x32;

if ((Fp=fopen(FName,"w")) == NULL) { sprintf(Str,"Error opening %s",FName); Attention(0,Str); return; }
XSize=ChX; YSize=ChY;
if (fwrite(&XSize,2,1,Fp) < 1) { Attention(0,"File write error"); return; }
if (fwrite(&YSize,2,1,Fp) < 1) { Attention(0,"File write error"); return; }
for (Row=0;Row<ChY;Row++)
    {
    P1=0;
    while (P1<XSize)
       {
       P2=P1;
       while ( (!(guint32)Spec[XSize*Row+P2]) && (P2<XSize) ) P2++; NZer=P2-P1;
       if (fwrite(&NZer,2,1,Fp) < 1) { Attention(0,"File write error"); return; };                     //Output no of zeroes
       P1=P2;
       while ( ((guint32)Spec[XSize*Row+P2]) && (P2<XSize) ) P2++; NDat=P2-P1;
       if (fwrite(&NDat,2,1,Fp) < 1) { Attention(0,"File write error"); return; };
       for (i=0;i<NDat;i++) { x32=(guint32)Spec[XSize*Row+P1+i]; fwrite(&x32,4,1,Fp); }         //Output no of non-zero data
       P1=P2;                                                                                                    
       }
    }
fclose(Fp);
}
/*------------------------------------------------------------------------------------------------------------------------*/
void Add2dDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (GTK_IS_WIDGET(Win)) gtk_widget_destroy(Win);
g_free(Util2);                                                                                    //Destroy global structure
}
/*------------------------------------------------------------------------------------------------------------------------*/
void ClearNamesAdd2d(GtkWidget *W,gpointer Unused)
{
gint i;

for (i=0;i<MAX_ADD_SPEC;i++)
    {
    strcpy(Util2->FNames[i],"");
    gtk_label_set_text(GTK_LABEL(GTK_BIN(Util2->FileButs[i])->child),"Browse");
    gtk_entry_set_text(GTK_ENTRY(Util2->SizeEntry[i]),"");
    }
Util2->Row=0;
}
/*------------------------------------------------------------------------------------------------------------------------*/
void *ProcessAdd2d(gpointer Label)
{
gint N,i,ChX,ChY,Ch,j;
const gchar *OutFName,*Size,*BitType,*OutBitType; gchar Str[80];
gdouble XFactor[MAX_ADD_SPEC],*SOut;
guint16 *Spec16;
guint32 *Spec32;
FILE *Fp;

gdk_threads_enter();
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Util2->NSpin));
OutFName=gtk_entry_get_text(GTK_ENTRY(Util2->OutEntry));
Size=gtk_entry_get_text(GTK_ENTRY(Util2->SizeEntry[0]));
ChX=atoi(Size); strcpy(Str,index(Size,'x')); ChY=atoi(&Str[1]); Ch=ChX*ChY;
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry));
OutBitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry));
for (i=0;i<N;++i) XFactor[i]=atof(gtk_entry_get_text(GTK_ENTRY(Util2->MultEntry[i])));
if (BitType[0]=='S') Spec16=g_new(guint16,Ch); else Spec32=g_new(guint32,Ch);
SOut=g_new(gdouble,Ch);
for (i=0;i<Ch;++i) SOut[i]=0;
for (j=0;j<N;++j)
    {
    if (BitType[0]=='S') { Get2dSpec16(Util2->FNames[j],ChX,ChY,Spec16); for (i=0;i<Ch;++i) SOut[i]+=XFactor[j]*Spec16[i]; }
    else                 { Get2dSpec32(Util2->FNames[j],ChX,ChY,Spec32); for (i=0;i<Ch;++i) SOut[i]+=XFactor[j]*Spec32[i]; }
    sprintf(Str,"Done %d of %d 2d files ...",j+1,N);
    gtk_label_set_text(GTK_LABEL(Label),Str);
    }
if (OutBitType[0]=='S') Save2dSpec16(OutFName,ChX,ChY,SOut); else Save2dSpec32(OutFName,ChX,ChY,SOut);
if (BitType[0]=='S') g_free(Spec16); else g_free(Spec32);
g_free(SOut);

if ((Fp=fopen("Add2dList.dat","w")))                                                  //Save information to Add2dList.dat
   {
   fprintf(Fp,"Add2dList...List of 2d files added\n");
   fprintf(Fp,"NoOfSpectra= %d\n",N);
   fprintf(Fp,"OutFName= %s\n",OutFName);
   fprintf(Fp,"XSize= %d\n",ChX);
   fprintf(Fp,"YSize= %d\n",ChY);
   if (BitType[0]=='S')    fprintf(Fp,"Input  BitType= 1\n"); else fprintf(Fp,"Input  BitType= 2\n");
   if (OutBitType[0]=='S') fprintf(Fp,"Output BitType= 1\n"); else fprintf(Fp,"Output BitType= 2\n");
   for (i=0;i<N;++i) fprintf(Fp,"%4d %s %f\n",i+1,Util2->FNames[i],XFactor[i]);
   fclose(Fp);
   }
UtilBusy=FALSE; gtk_widget_destroy(Util2->Win);
gdk_threads_leave();
return NULL;
}
/*------------------------------------------------------------------------------------------------------------------------*/
gint Del2dWin(GtkWidget *W,GdkEvent *E,gpointer Unused)                            //Ensures that Stat2dWin cannot be killed
{ return TRUE; }
/*------------------------------------------------------------------------------------------------------------------------*/
void ProceedAdd2d(GtkWidget *W,GtkWidget *Win)
{
pthread_t Processor;
GtkWidget *Stat2dWin,*Label,*VBox,*But;

Stat2dWin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(Stat2dWin,200,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Stat2dWin),"delete_event",GTK_SIGNAL_FUNC(Del2dWin),NULL);
gtk_window_set_title(GTK_WINDOW(Stat2dWin),"Adding 2d");
VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Stat2dWin),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);
Label=gtk_label_new("Adding 2d Files"); 
gtk_box_pack_start(GTK_BOX(VBox),Label,TRUE,TRUE,0);
But=gtk_button_new_with_label("Close");
gtk_box_pack_start(GTK_BOX(VBox),But,FALSE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Stat2dWin));
gtk_widget_show_all(Stat2dWin);

pthread_create(&Processor,NULL,ProcessAdd2d,Label);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void SelectToAdd2d(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5];
const gchar *BitType;
gint16 XSize,YSize;
FILE *Fp;
gint ScrollTo;
gfloat Ht=22.0;                                                                          //Text height. Used for ScrollTo
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Util2->NSpin));
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry));
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Util2->FNames[Util2->Row],"%s/%s",FileX->Path,FileX->TargetFile);

AbbreviateFileName(Str,Util2->FNames[Util2->Row],MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Util2->FileButs[Util2->Row])->child),Str);
if (!(Fp=fopen(Util2->FNames[Util2->Row],"r"))) { Attention(0,"Open file failed"); return; }
fread(&XSize,2,1,Fp); fread(&YSize,2,1,Fp); fclose(Fp);
sprintf(Str,"%dx%d",XSize,YSize); gtk_entry_set_text(GTK_ENTRY(Util2->SizeEntry[Util2->Row]),Str);
if (Util2->Row<N-1) 
   {
   Util2->Row++; 
   ScrollTo=(Util2->Row/10)*(10.0*Ht)-Ht;                                                 //Scroll after every 10 entries
   gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment
                                                                          (GTK_SCROLLED_WINDOW(Util2->ScrlW))),ScrollTo);
   }
else Util2->Row=0;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void BrowseTwodFile(GtkWidget *W,gpointer Data)
{
Util2->Row=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 2d File",NULL,447,TOP_SPACE+TopOfset,TRUE,".",".z2d",FALSE,&SelectToAdd2d,TRUE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void CancelAdd2d(GtkWidget *W,GtkWidget *Win)
{
UtilBusy=FALSE;
gtk_widget_destroy(Win);
}
/*----------------------------------------------------------------- -----------------------------------------------------*/
void AddNSpin(GtkWidget *W,gpointer Unused)              //Used by both Add1d and Add2d. Sets rows beyond N to insensitive
{
gint i,N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Util2->NSpin));
for (i=0;i<N;++i)
    {
    gtk_widget_set_sensitive(Util2->FileButs[i],TRUE);
    gtk_widget_set_sensitive(Util2->MultEntry[i],TRUE);
    gtk_widget_set_sensitive(Util2->SizeEntry[i],TRUE);
    }
for (i=N;i<MAX_ADD_SPEC;++i)
    {
    gtk_widget_set_sensitive(Util2->FileButs[i],FALSE);
    gtk_widget_set_sensitive(Util2->MultEntry[i],FALSE);
    gtk_widget_set_sensitive(Util2->SizeEntry[i],FALSE);
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Add2d(GtkWidget *W,gpointer Data)                                  //Adds several 2d spectrum files to an output file
{
static gchar *Titles[4]= {"No","File Name","Multiply by","Size"};
gint ColWidth[4]={36,158,98,88};
static GdkColor TitlesBg  = {0,0x7777,0x0000,0x0000};
static GdkColor TitlesFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *TitlesStyle;
GtkWidget *VBox,*HBox,*Label,*But,*Table;
GtkAdjustment *Adj;
gint i,N,BitType,OutBitType;
gchar Str[10],Line[120],Input[100],OutFName[100],AbbFName[MAX_FNAME_LENGTH+5],XFactor[MAX_ADD_SPEC][30],
      XSize[10],YSize[10],Size[21];
GList *GList;
FILE *Fp;

if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;

TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { TitlesStyle->fg[i]=TitlesStyle->text[i]=TitlesFg; TitlesStyle->bg[i]=TitlesBg; }

Util2=g_new(struct Util2,1);                                      //Create new structure to be destroyed in Add2dDestroyed()
for (i=0;i<MAX_ADD_SPEC;i++) strcpy(Util2->FNames[i],"");

N=2; strcpy(OutFName,"outfile.z1d"); BitType=1; OutBitType=1; strcpy(Size,"");
for (i=0;i<MAX_ADD_SPEC;++i) strcpy(XFactor[i],"1.0");

if ((Fp=fopen("Add2dList.dat","r")))                                                           //Retreive saved information
   {
   fgets(Line,118,Fp); fgets(Line,116,Fp);
   sscanf(Line,"%s %s",Input,Input); N=atoi(Input);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,OutFName);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,XSize);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,YSize); sprintf(Size,"%sx%s",XSize,YSize);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,Input); BitType=atoi(Input);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,Input); OutBitType=atoi(Input);
   for (i=0;i<N;++i) { fgets(Line,116,Fp); sscanf(Line,"%s %s %s",Input,Util2->FNames[i],XFactor[i]); }
   fclose(Fp);
   }

Util2->Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(Util2->Win,0,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Util2->Win),"destroy",GTK_SIGNAL_FUNC(Add2dDestroyed),Util2->Win);
gtk_window_set_title(GTK_WINDOW(Util2->Win),"Add 2d Spectra");

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Util2->Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("All input spectra must be of same size, shape and bits"); 
gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No of Spectra:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)N,1.0,(gfloat)MAX_ADD_SPEC,1.0,1.0,0.0));
Util2->NSpin=gtk_spin_button_new(Adj,0.5,0); gtk_widget_set_size_request(GTK_WIDGET(Util2->NSpin),60,-1);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(AddNSpin),NULL);
gtk_box_pack_start(GTK_BOX(HBox),Util2->NSpin,FALSE,FALSE,4);
Label=gtk_label_new("   All input files: "); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util2->BitCombo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Util2->BitCombo),168,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util2->BitCombo,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Single Word (16-bit)"); GList=g_list_append(GList,"Double Word (32-bit)");
gtk_combo_set_popdown_strings(GTK_COMBO(Util2->BitCombo),GList);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util2->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_box_pack_start(GTK_BOX(HBox),Util2->OutEntry,FALSE,FALSE,4);
gtk_entry_set_text(GTK_ENTRY(Util2->OutEntry),"outfile.z2d");
Util2->OutBitCombo=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Util2->OutBitCombo),168,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util2->OutBitCombo,TRUE,FALSE,0);
gtk_combo_set_popdown_strings(GTK_COMBO(Util2->OutBitCombo),GList);
if (OutBitType==2) gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry),"Double Word (32-bit)");
else               gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry),"Single Word (16-bit)");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Table=gtk_table_new(1,4,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_box_pack_start(GTK_BOX(HBox),Table,FALSE,FALSE,0);
for (i=0;i<4;i++)
    {
    But=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(But,TitlesStyle);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

Util2->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Util2->ScrlW),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_widget_set_size_request(GTK_WIDGET(Util2->ScrlW),-1,300);
gtk_box_pack_start(GTK_BOX(VBox),Util2->ScrlW,TRUE,TRUE,0);
Table=gtk_table_new(10,3,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
for (i=0;i<MAX_ADD_SPEC;++i)
    {
    sprintf(Str,"%d",i+1); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (!strlen(Util2->FNames[i])) Util2->FileButs[i]=gtk_button_new_with_label("Browse");
    else
        {
        AbbreviateFileName(AbbFName,Util2->FNames[i],MAX_FNAME_LENGTH);
        Util2->FileButs[i]=gtk_button_new_with_label(AbbFName);
        }
    gtk_widget_set_size_request(GTK_WIDGET(Util2->FileButs[i]),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(Table),Util2->FileButs[i],1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (i>=N) gtk_widget_set_sensitive(Util2->FileButs[i],FALSE);
    gtk_signal_connect(GTK_OBJECT(Util2->FileButs[i]),"clicked",GTK_SIGNAL_FUNC(BrowseTwodFile),GINT_TO_POINTER(i));
    Util2->MultEntry[i]=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(Util2->MultEntry[i]),XFactor[i]);
    gtk_widget_set_size_request(GTK_WIDGET(Util2->MultEntry[i]),ColWidth[2],-1);
    gtk_table_attach(GTK_TABLE(Table),Util2->MultEntry[i],2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (i>=N) gtk_widget_set_sensitive(Util2->MultEntry[i],FALSE);
    Util2->SizeEntry[i]=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(Util2->SizeEntry[i]),Size);
    gtk_widget_set_usize(GTK_WIDGET(Util2->SizeEntry[i]),ColWidth[3],22);
    gtk_entry_set_editable(GTK_ENTRY(Util2->SizeEntry[i]),FALSE);
    gtk_entry_set_editable(GTK_ENTRY(Util2->SizeEntry[i]),FALSE);
    gtk_table_attach(GTK_TABLE(Table),Util2->SizeEntry[i],3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (i>=N) gtk_widget_set_sensitive(Util2->SizeEntry[i],FALSE);
    }
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Util2->ScrlW),Table);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_with_label("Clear Names"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ClearNamesAdd2d),NULL);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedAdd2d),NULL);
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CancelAdd2d),Util2->Win);

gtk_widget_show_all(Util2->Win);
g_list_free(GList); gtk_style_unref(TitlesStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Add1dDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (GTK_IS_WIDGET(Win)) gtk_widget_destroy(Win);
g_free(Util2);                                                                                  //Destroy global structure
UtilBusy=FALSE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ClearNamesAdd1d(GtkWidget *W,gpointer Unused)
{
gint i;

for (i=0;i<MAX_ADD_SPEC;i++)
    {
    strcpy(Util2->FNames[i],"");
    gtk_label_set_text(GTK_LABEL(GTK_BIN(Util2->FileButs[i])->child),"Browse");
    gtk_entry_set_text(GTK_ENTRY(Util2->SizeEntry[i]),"");
    }
Util2->Row=0;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void ProceedAdd1d(GtkWidget *W,gpointer Unused)
{
gint N,i,Chs,j;
const gchar *OutFName,*Size,*BitType,*OutBitType; gchar Str[80];
gdouble XFactor[MAX_ADD_SPEC],SOut[16384];
guint16 Spec16[16384],x16;
guint32 Spec32[16384],x32;
FILE *Fp;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Util2->NSpin));
OutFName=gtk_entry_get_text(GTK_ENTRY(Util2->OutEntry));
Size=gtk_entry_get_text(GTK_ENTRY(Util2->SizeEntry[0]));
Chs=atoi(Size);
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry));
OutBitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry));
for (i=0;i<MAX_ADD_SPEC;++i) XFactor[i]=atof(gtk_entry_get_text(GTK_ENTRY(Util2->MultEntry[i])));
for (i=0;i<Chs;i++) SOut[i]=0.0;
if (BitType[0]=='S')                                                                                        //16-bit case
   {
   for (j=0;j<N;j++)
       {
       if (!(Fp=fopen(Util2->FNames[j],"r"))) { sprintf(Str,"Failed to open file # %d",j+1); Attention(0,Str); return; }
       if (fread(Spec16,2,Chs,Fp) < Chs) { sprintf(Str,"Incorrect size, file # %d",j+1); Attention(0,Str); return; }
       fclose(Fp);
       for (i=0;i<Chs;i++) SOut[i]+=XFactor[j]*Spec16[i];
       }
   }
else                                                                                                        //32-bit case
   {
   for (j=0;j<N;j++)
       {
       if (!(Fp=fopen(Util2->FNames[j],"r"))) { sprintf(Str,"Failed to open file # %d",j+1); Attention(0,Str); return; }
       if (fread(Spec32,4,Chs,Fp) < Chs) { sprintf(Str,"Incorrect size, file # %d",j+1); Attention(0,Str); return; }
       fclose(Fp);
       for (i=0;i<Chs;i++) SOut[i]+=XFactor[j]*Spec32[i];
       }
   }
if (!(Fp=fopen(OutFName,"w")))  { Attention(0,"Couldnt open output file"); return; }
for (i=0;i<Chs;i++) 
    {
    if (SOut[i]>0.0) x32=(guint32)SOut[i]; else x32=0;
    if (OutBitType[0]=='S') {x16=(guint16)x32; fwrite(&x16,2,1,Fp); } else fwrite(&x32,4,1,Fp);
    }
fclose(Fp);

if ((Fp=fopen("Add1dList.dat","w")))                                                  //Save information to Add1dList.dat
   {
   fprintf(Fp,"Add1dList...List of 1d files added\n");
   fprintf(Fp,"NoOfSpectra= %d\n",N);
   fprintf(Fp,"OutFName= %s\n",OutFName);
   fprintf(Fp,"Size= %d\n",Chs);
   if (BitType[0]=='S')    fprintf(Fp,"Input  BitType= 1\n"); else fprintf(Fp,"Input  BitType= 2\n");
   if (OutBitType[0]=='S') fprintf(Fp,"Output BitType= 1\n"); else fprintf(Fp,"Output BitType= 2\n");
   for (i=0;i<N;++i) fprintf(Fp,"%4d %s %f\n",i+1,Util2->FNames[i],XFactor[i]);
   fclose(Fp);
   }
sprintf(Str,"Added %d 1d spectra to %s",N,OutFName); Attention(0,Str);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void SelectToAdd1d(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5];
struct stat StatBuf;
const gchar *BitType;
gint ScrollTo;
gfloat Ht=22.0;                                                                          //Text height. Used for ScrollTo
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Util2->NSpin));

BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry));
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Util2->FNames[Util2->Row],"%s/%s",FileX->Path,FileX->TargetFile);

AbbreviateFileName(Str,Util2->FNames[Util2->Row],MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Util2->FileButs[Util2->Row])->child),Str);
stat(Util2->FNames[Util2->Row],&StatBuf);
if (BitType[0]=='S') sprintf(Str,"%ld",(glong)(StatBuf.st_size/2)); else sprintf(Str,"%ld",(glong)(StatBuf.st_size/4));
gtk_entry_set_text(GTK_ENTRY(Util2->SizeEntry[Util2->Row]),Str);
if (Util2->Row<N-1) 
   {
   Util2->Row++;
   ScrollTo=(Util2->Row/10)*(10.0*Ht)-Ht;                                                 //Scroll after every 10 entries
   gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment
                                                                          (GTK_SCROLLED_WINDOW(Util2->ScrlW))),ScrollTo);
   }
else Util2->Row=0;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void BrowseOnedFile(GtkWidget *W,gpointer Data)
{
Util2->Row=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 1d File",NULL,415,TOP_SPACE+TopOfset,TRUE,".",".z1d",FALSE,&SelectToAdd1d,TRUE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Add1d(GtkWidget *W,gpointer Data)                                  //Adds several 1d spectrum files to an output file
{
static gchar *Titles[4]= {"No","File Name","Multiply by","Size"};
gint ColWidth[4]={30,130,80,60};
static GdkColor TitlesBg  = {0,0x0000,0x0000,0x0000};
static GdkColor TitlesFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *TitlesStyle;
GtkWidget *Win,*VBox,*HBox,*Label,*But,*Table;
GtkAdjustment *Adj;
gint i,N,BitType,OutBitType;
gchar Str[10],Line[120],Input[100],OutFName[100],AbbFName[MAX_FNAME_LENGTH+5],XFactor[MAX_ADD_SPEC][30],Size[10];
GList *GList;
FILE *Fp;

if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;

TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) { TitlesStyle->fg[i]=TitlesStyle->text[i]=TitlesFg; TitlesStyle->bg[i]=TitlesBg; }

Util2=g_new(struct Util2,1);                                     //Create new structure to be destroyed in Add1dDestroyed()
for (i=0;i<MAX_ADD_SPEC;i++) strcpy(Util2->FNames[i],"");

N=2; strcpy(OutFName,"outfile.z1d"); strcpy(Size,"1024"); BitType=2; OutBitType=2;
for (i=0;i<MAX_ADD_SPEC;++i) strcpy(XFactor[i],"1.0");
if ((Fp=fopen("Add1dList.dat","r")))                                                           //Retreive saved information
   {
   fgets(Line,118,Fp); fgets(Line,116,Fp);
   sscanf(Line,"%s %s",Input,Input); N=atoi(Input);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,OutFName);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,Size);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,Input); BitType=atoi(Input);
   fgets(Line,116,Fp); sscanf(Line,"%s %s",Input,Input); OutBitType=atoi(Input);
   for (i=0;i<N;++i) { fgets(Line,116,Fp); sscanf(Line,"%s %s %s",Input,Util2->FNames[i],XFactor[i]); }
   fclose(Fp);
   }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_widget_set_size_request(GTK_WIDGET(Win),-1,400);
gtk_widget_set_uposition(Win,20,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(Add1dDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"Add 1d Spectra");

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("All input spectra files must be of same size and bits"); 
gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("No of Spectra:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=GTK_ADJUSTMENT(gtk_adjustment_new((gfloat)N,1.0,(gfloat)MAX_ADD_SPEC,1.0,1.0,0.0));
Util2->NSpin=gtk_spin_button_new(Adj,0.5,0); gtk_widget_set_size_request(GTK_WIDGET(Util2->NSpin),60,-1);
gtk_signal_connect(GTK_OBJECT(Adj),"value_changed",GTK_SIGNAL_FUNC(AddNSpin),NULL);
gtk_box_pack_start(GTK_BOX(HBox),Util2->NSpin,FALSE,FALSE,4);
Label=gtk_label_new("   All input files: "); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util2->BitCombo=gtk_combo_new();
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util2->BitCombo,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Double Word (32-bit)"); GList=g_list_append(GList,"Single Word (16-bit)");
gtk_combo_set_popdown_strings(GTK_COMBO(Util2->BitCombo),GList);
if (BitType==2) gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry),"Double Word (32-bit)");
else            gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Util2->BitCombo)->entry),"Single Word (16-bit)");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util2->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_box_pack_start(GTK_BOX(HBox),Util2->OutEntry,FALSE,FALSE,4);
gtk_entry_set_text(GTK_ENTRY(Util2->OutEntry),OutFName);
Util2->OutBitCombo=gtk_combo_new();
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util2->OutBitCombo,TRUE,FALSE,0);
gtk_combo_set_popdown_strings(GTK_COMBO(Util2->OutBitCombo),GList);
if (OutBitType==2) gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry),"Double Word (32-bit)");
else               gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(Util2->OutBitCombo)->entry),"Single Word (16-bit)");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Table=gtk_table_new(1,4,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_box_pack_start(GTK_BOX(HBox),Table,FALSE,FALSE,0);
for (i=0;i<4;i++)
    {
    But=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(But,TitlesStyle);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

Util2->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Util2->ScrlW),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_box_pack_start(GTK_BOX(VBox),Util2->ScrlW,TRUE,TRUE,0);
Table=gtk_table_new(MAX_ADD_SPEC,3,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
for (i=0;i<MAX_ADD_SPEC;++i)
    {
    sprintf(Str,"%d",i+1); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (!strlen(Util2->FNames[i])) Util2->FileButs[i]=gtk_button_new_with_label("Browse");
    else 
        {
        AbbreviateFileName(AbbFName,Util2->FNames[i],MAX_FNAME_LENGTH);
        Util2->FileButs[i]=gtk_button_new_with_label(AbbFName);
        }
    gtk_widget_set_size_request(GTK_WIDGET(Util2->FileButs[i]),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(Table),Util2->FileButs[i],1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (i>=N) gtk_widget_set_sensitive(Util2->FileButs[i],FALSE);
    gtk_signal_connect(GTK_OBJECT(Util2->FileButs[i]),"clicked",GTK_SIGNAL_FUNC(BrowseOnedFile),GINT_TO_POINTER(i));
    Util2->MultEntry[i]=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(Util2->MultEntry[i]),XFactor[i]);
    gtk_widget_set_size_request(GTK_WIDGET(Util2->MultEntry[i]),ColWidth[2],-1);
    gtk_table_attach(GTK_TABLE(Table),Util2->MultEntry[i],2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (i>=N) gtk_widget_set_sensitive(Util2->MultEntry[i],FALSE);
    Util2->SizeEntry[i]=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(Util2->SizeEntry[i]),Size);
    gtk_widget_set_size_request(GTK_WIDGET(Util2->SizeEntry[i]),ColWidth[3],-1);
    gtk_entry_set_editable(GTK_ENTRY(Util2->SizeEntry[i]),FALSE);
    gtk_table_attach(GTK_TABLE(Table),Util2->SizeEntry[i],3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    if (i>=N) gtk_widget_set_sensitive(Util2->SizeEntry[i],FALSE);
    }
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Util2->ScrlW),Table);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_with_label("Clear Names"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ClearNamesAdd1d),NULL);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ProceedAdd1d),Win);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
g_list_free(GList); gtk_style_unref(TitlesStyle);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MakeAscii1d(GtkWidget *W,gpointer Data)
{
gchar OutFName[MAX_FNAME_LENGTH+5],Str[150]; const gchar *BitType;
gint Ch,i;
guint16 Spec16[16384];
guint32 Spec32[16384];
FILE *Fp;

if (!Util1->Ready) { Attention(0,"Error: No file selected!"); return; }
strcpy(OutFName,gtk_entry_get_text(GTK_ENTRY(Util1->OutEntry)));
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Util1->BitCombo)->entry));

if ( (Fp=fopen(Util1->InFName,"r")) == NULL) { sprintf(Str,"Could not open %s",Util1->InFName); Attention(0,Str); return; }
if (BitType[0]=='S') Ch=fread(Spec16,2,16384,Fp); else Ch=fread(Spec32,4,16384,Fp);
fclose(Fp);

if ( (Fp=fopen(OutFName,"w")) == NULL) { sprintf(Str,"Could not open %s",OutFName); Attention(0,Str); return; }
for (i=0;i<Ch;i++) 
    if (BitType[0]=='S') fprintf(Fp,"%4d    %d\n",i,Spec16[i]); 
    else                 fprintf(Fp,"%4d    %d\n",i,Spec32[i]);
fclose(Fp);
sprintf(Str,"File ./%s written",OutFName);
Attention(0,Str);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void GetFName(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5];

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Util1->InFName,"%s/%s",FileX->Path,FileX->TargetFile);
g_free(FileX);
AbbreviateFileName(Str,Util1->InFName,MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Util1->FileBut)->child),Str);
strcpy(rindex(Str,'.'),".txt");
gtk_entry_set_text(GTK_ENTRY(Util1->OutEntry),Str);
Util1->Ready=TRUE;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void Ascii1dBrowse(GtkWidget *W,gpointer Data)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select file",NULL,90,120,TRUE,".",".z1d",FALSE,&GetFName,FALSE);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void Ascii1dDestroyed(GtkWidget *W,GtkWidget *Win)
{
if (Win) gtk_widget_destroy(Win);
g_free(Util1);                                                                                   //Destroy global structure
UtilBusy=FALSE;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void Ascii1d(GtkWidget *W,gpointer Data)                       //Reads a 1d spectrum file and outputs to an ASCII text file
{
GtkWidget *Win,*VBox,*HBox,*Label,*But;
GList *GList;

if (UtilBusy)  { Attention(0,"Another utility is already running"); return; }
UtilBusy=TRUE;
Util1=g_new(struct Util1,1);                                  //Create new structure to be destroyed in Ascii1dDestroyed()
Util1->Ready=FALSE;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);                                         //Make the window modal
gtk_widget_set_uposition(Win,600,TOP_SPACE+TopOfset);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(Ascii1dDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"1d Spec to ASCII");

VBox=gtk_vbox_new(FALSE,14); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),10);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Make ASCII text 1d Spectrum"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("1d File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util1->FileBut=gtk_button_new_with_label("Browse");
gtk_box_pack_start(GTK_BOX(HBox),Util1->FileBut,FALSE,FALSE,34);
gtk_signal_connect(GTK_OBJECT(Util1->FileBut),"clicked",GTK_SIGNAL_FUNC(Ascii1dBrowse),NULL);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Type:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util1->BitCombo=gtk_combo_new();
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Util1->BitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Util1->BitCombo,FALSE,FALSE,8); GList=NULL;
GList=g_list_append(GList,"Double Word (32-bit)"); GList=g_list_append(GList,"Single Word (16-bit)");
gtk_combo_set_popdown_strings(GTK_COMBO(Util1->BitCombo),GList);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Util1->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH); 
gtk_box_pack_start(GTK_BOX(HBox),Util1->OutEntry,FALSE,FALSE,10);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(MakeAscii1d),NULL);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(WinClosed),Win);

g_list_free(GList);
gtk_widget_show_all(Win);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
