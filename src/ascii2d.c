#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define MAX_FNAME_LENGTH   80
#define MAX_DIR_ENTRIES  5000                                      //FileX Widget: maximum number of files/sub-directories
#define MAX_DIR_STRLEN    255                           //FileX Widget: maximum string length for file and directory names
#define MAX_MASK_SIZE       5                                              //FileX Widget: maximum length of the file mask

struct FileSelectType  {                                                                 //For the FileSelect widget FileX
                       GtkWidget *But; GtkWidget *FEntry; GtkWidget *PEntry; GtkWidget *MEntry; GtkWidget *Win;
                       GtkWidget *Table; GtkWidget *Label1; GtkWidget *Label2;
                       gint N; gint Index; gint Files;
                       gchar Mask[MAX_MASK_SIZE]; gchar Path[MAX_DIR_STRLEN]; gchar Names[MAX_DIR_ENTRIES][MAX_DIR_STRLEN];
                       gchar TargetFile[MAX_DIR_STRLEN];
                       };
/*Global Definitions*/
struct Ascii2d {GtkWidget *FileBut; GtkWidget *BitCombo; GtkWidget *OutEntry; gboolean Ready; gchar InFName[120];};
struct Ascii2d *Ascii2d;
struct FileSelectType *FileX;                                                                                //see open.c
/*Function templates*/
void Attention(gint XPos,gchar *Messg);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void InitMat16(gshort XMax,gshort YMax,guint16 *Data);
void InitMat32(gshort XMax,gshort YMax,guint32 *Data);
void Readz2d(gint wsize,gshort xsize,gshort ysize,FILE *iFp,guint16 *Data16,guint32 *Data32,gboolean *Error);
void Writez2d16(gshort XMax,gshort YMax,FILE *oFp,guint16 *Data,gboolean *Error);
void Writez2d32(gshort XMax,gshort YMax,FILE *oFp,guint32 *Data,gboolean *Error);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
/*----------------------------------------------------------------------------------------------------------------------*/
void DestroyMain(GtkWidget *Win,gpointer Data)
{ 
g_free(Ascii2d);
gtk_main_quit(); 
}
/*----------------------------------------------------------------------------------------------------------------------*/
void DoGnuPlot(GtkWidget *W,gpointer Data)
{
system("nice -n 10 gnuplot -persist \"work.gnu\"&");
}
/*----------------------------------------------------------------------------------------------------------------------*/
void MakeAscii2d(GtkWidget *Win,gpointer Data)
{
gchar OutFName[MAX_FNAME_LENGTH+5],Str[150];
const gchar *BitType;
FILE *Fp;
gshort XSize,YSize;
guint16 *Data16;
guint32 *Data32,DMax;
gint X,Y,WSize;
gboolean Error;
GtkWidget *W,*Label,*But;
                                                                                                                             
if (!Ascii2d->Ready) { Attention(0,"Error: No file selected!"); return; }
strcpy(OutFName,gtk_entry_get_text(GTK_ENTRY(Ascii2d->OutEntry)));
BitType=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Ascii2d->BitCombo)->entry));
                                                                                                                             
if ( !(Fp=fopen(Ascii2d->InFName,"r"))) 
   { sprintf(Str,"Could not open %s",Ascii2d->InFName); Attention(0,Str); return; }
fread(&XSize,sizeof(gshort),1,Fp); fread(&YSize,sizeof(gshort),1,Fp);
if (BitType[0]=='S') { WSize=1; Data16=g_new(guint16,XSize*YSize); InitMat16(XSize,YSize,Data16); }
else                 { WSize=2; Data32=g_new(guint32,XSize*YSize); InitMat32(XSize,YSize,Data32); }
Readz2d(WSize,XSize,YSize,Fp,Data16,Data32,&Error);
fclose(Fp);
if (Error) { sprintf(Str,"Error reading %s",Ascii2d->InFName); Attention(0,Str); return; }
                                                                                                                             
if ( !(Fp=fopen(OutFName,"w"))) { sprintf(Str,"Could not open %s",OutFName); Attention(0,Str); return; }
fprintf(Fp,"#Ascii file suitable for gnuplot: splot \"%s\"\n",OutFName);
if (WSize==1)
   {
   for (X=0,DMax=0;X<XSize;++X) 
       {
       for (Y=0;Y<YSize;++Y) 
           {
           fprintf(Fp,"%4d %6d %d\n",X,Y,Data16[X+XSize*Y]);
           if ((X>0) && (Y>0)) DMax=MAX(DMax,(guint32)Data16[X+XSize*Y]);
           }
       fprintf(Fp,"\n");
       }
   g_free(Data16);
   }
else
   {
   for (X=0,DMax=0;X<XSize;++X) 
       {
       for (Y=0;Y<YSize;++Y) 
           {
           fprintf(Fp,"%4d %6d %d\n",X,Y,Data32[X+XSize*Y]);
           if ((X>0) && (Y>0)) DMax=MAX(DMax,Data32[X+XSize*Y]);
           }
       fprintf(Fp,"\n");
       }
   g_free(Data32);
   }
fclose(Fp);

W=gtk_dialog_new(); gtk_grab_add(W); gtk_window_set_title(GTK_WINDOW(W),"Finished");
gtk_widget_set_uposition(GTK_WIDGET(W),350,350); gtk_widget_set_usize(GTK_WIDGET(W),300,100);
gtk_signal_connect_object(GTK_OBJECT(W),"delete_event",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(W));
sprintf(Str,"File ./%s written",OutFName);
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(W)->vbox),Label,TRUE,FALSE,5);
sprintf(Str,"For gnuplot: splot [0:%d] [0:%d] [1:%d] \"%s\"",XSize,YSize,DMax,OutFName);
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(W)->vbox),Label,TRUE,FALSE,5);
Fp=fopen("work.gnu","w");
fprintf(Fp,"splot [0:%d] [0:%d] [1:%d] \"%s\"",XSize,YSize,DMax,OutFName);
fclose(Fp);
But=gtk_button_new_with_label("Show with\ngnuplot");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(W)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DoGnuPlot),NULL);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(W));
But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(W)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(W));
gtk_widget_show_all(W);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void GetFName(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5];
                                                                                                                             
if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Ascii2d->InFName,"%s/%s",FileX->Path,FileX->TargetFile);
g_free(FileX);
AbbreviateFileName(Str,Ascii2d->InFName,MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Ascii2d->FileBut)->child),Str);
strcpy(rindex(Str,'.'),".txt");
gtk_entry_set_text(GTK_ENTRY(Ascii2d->OutEntry),Str);
Ascii2d->Ready=TRUE;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void Ascii2dBrowse(GtkWidget *W,gpointer Data)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select file",NULL,90,120,TRUE,".",".z2d",FALSE,&GetFName,FALSE);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
int main(int argc,char *argv[])
{
GtkWidget *Win,*HBox,*VBox,*Label,*But;
GList *GList;

gtk_init(&argc,&argv);
Ascii2d=g_new(struct Ascii2d,1);                                   //Create new structure to be destroyed in DestroyMain()
Ascii2d->Ready=FALSE;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(Win),"2d Spec to ASCII");
gtk_widget_set_uposition(GTK_WIDGET(Win),600,120);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(DestroyMain),NULL);

VBox=gtk_vbox_new(FALSE,14); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),10);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Make ASCII text 2d Spectrum"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,TRUE,0);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("2d File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Ascii2d->FileBut=gtk_button_new_with_label("Browse"); gtk_widget_set_size_request(GTK_WIDGET(Ascii2d->FileBut),130,-1);
gtk_box_pack_start(GTK_BOX(HBox),Ascii2d->FileBut,FALSE,FALSE,34);
gtk_signal_connect(GTK_OBJECT(Ascii2d->FileBut),"clicked",GTK_SIGNAL_FUNC(Ascii2dBrowse),NULL);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Type:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Ascii2d->BitCombo=gtk_combo_new();
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Ascii2d->BitCombo)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Ascii2d->BitCombo,FALSE,FALSE,8); GList=NULL;
GList=g_list_append(GList,"Single Word (16-bit)"); GList=g_list_append(GList,"Double Word (32-bit)");
gtk_combo_set_popdown_strings(GTK_COMBO(Ascii2d->BitCombo),GList);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Output File:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Ascii2d->OutEntry=gtk_entry_new_with_max_length(MAX_FNAME_LENGTH);
gtk_box_pack_start(GTK_BOX(HBox),Ascii2d->OutEntry,FALSE,FALSE,10);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(MakeAscii2d),NULL);
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DestroyMain),Win);

gtk_widget_show_all(Win);
g_list_free(GList);
gtk_main();
return(0);
}
/*---------------------------------------------------------------------------------------------------------------------*/
