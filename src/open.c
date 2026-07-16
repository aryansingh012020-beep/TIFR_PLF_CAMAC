//Modified for GTK2 on 07-02-08. (Avoided destroying table elements one by one)
#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lamps.h"
//Function prototypes
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void DisplayFiles();
//External globals
extern struct FileSelectType *FileX;
extern GtkStyle *FolderStyle,*FileStyle; 

GtkWidget *ScrollW,*Table;
//----------------------------------------------------------------------------------------------------------------------
int FileFilter(char *FileName,char *Filter) 
//Modified. Now OK for GANIL files named like run_1209.dat.14-Sep-05.19:05:09 with .dat as mask
//The .dat must come either at the end (eg: fname.dat) or else must appear as .dat.
{
char *P;

if (!Filter[0]) return TRUE;                                                        //If mask is empty we accept all files
if (!(P=strstr(FileName,Filter))) return FALSE;                                            //Reject if mask is not located
P+=strlen(Filter);                                        //Advance pointer to location after the mask inside the FileName
if ( (*P == '\0') || (*P == '.') ) return TRUE; else return FALSE;                      //Accept if end of the string or . 
}
//----------------------------------------------------------------------------------------------------------------------
gint FSelect(GtkWidget *But,GdkEventButton *Event,gpointer Data)
{
gint I;
struct stat StatBuf;
gchar FullPath[NAME_MAX+255+5],FName[MAX_DIR_STRLEN],*Str,L2Str[LONG_TEXT_FIELD];

I=GPOINTER_TO_INT(Data); 
if (!strcmp(FileX->Path,"/")) sprintf(FullPath,"%s%s",FileX->Path,FileX->Names[I]);                  //If the path is "/"
else                          sprintf(FullPath,"%s%c%s",FileX->Path,'/',FileX->Names[I]);               //All other cases
stat(FullPath,&StatBuf);
if (Event->type==GDK_2BUTTON_PRESS)                                                                        //Double Click
   { 
   if (S_ISREG(StatBuf.st_mode))                                                                      //Double Click File 
      { 
      strcpy(FileX->TargetFile,FileX->Names[FileX->Index]); 
      gtk_signal_emit_by_name(GTK_OBJECT(FileX->But),"clicked",NULL); 
      }
   else                                                                                             //Double Click Folder
      {
      strcpy(FileX->Path,FullPath); gtk_entry_set_text(GTK_ENTRY(FileX->PEntry),FileX->Path);
      DisplayFiles();
      gtk_entry_set_text(GTK_ENTRY(FileX->FEntry),""); gtk_widget_set_sensitive(FileX->But,FALSE);
      gtk_label_set_text(GTK_LABEL(FileX->Label1),"");
      sprintf(L2Str,"%d files found",FileX->Files); gtk_label_set_text(GTK_LABEL(FileX->Label2),L2Str);
      }
   return TRUE;
   }
FileX->Index=I;                                                                                            //Single Click
if (S_ISREG(StatBuf.st_mode))                                                                         //Single Click File
   {
   strcpy(FName,FileX->Names[FileX->Index]);  if ((Str=strstr(FName,FileX->Mask))) Str[0]='\0';  //Get rid of mask and ff
   gtk_entry_set_text(GTK_ENTRY(FileX->FEntry),FName);
   AbbreviateFileName(L2Str,FileX->Names[FileX->Index],20);
   gtk_label_set_text(GTK_LABEL(FileX->Label1),L2Str);
   strcpy(FileX->TargetFile,FileX->Names[FileX->Index]);
   gtk_widget_set_sensitive(FileX->But,TRUE);
   }
return FALSE;
}
//----------------------------------------------------------------------------------------------------------------------
void DisplayFiles()
{
GtkWidget *HBox,*But,*Label;
struct dirent **NameList;
gchar LStr[LONG_TEXT_FIELD+5],DStr[LONG_TEXT_FIELD+8],FullPath[NAME_MAX+255+5];
struct stat StatBuf;
gint i,N,NRow,Row,Col;

N=scandir(FileX->Path,&NameList,NULL,alphasort);
for (i=0,FileX->N=0;i<N;++i)                                                    //First collect the subdirectory names
    { 
    if (FileX->N==MAX_DIR_ENTRIES) continue;                        //Terminate the list if there are too many entries
    if (NameList[i]->d_name[0]=='.') continue;                                              //Avoid hidden directories
    sprintf(FullPath,"%s%c%s",FileX->Path,'/',NameList[i]->d_name);
    stat(FullPath,&StatBuf);
    if (S_ISDIR(StatBuf.st_mode)) { strcpy(FileX->Names[FileX->N],NameList[i]->d_name); ++FileX->N; }
    }
for (i=0,FileX->Files=0;i<N;++i)                                    //Now collect files names that match the file type
    { 
    if (FileX->N==MAX_DIR_ENTRIES) continue;                        //Terminate the list if there are too many entries
    if (NameList[i]->d_name[0]=='.') continue;                                                    //Avoid hidden files
    sprintf(FullPath,"%s%c%s",FileX->Path,'/',NameList[i]->d_name);
    stat(FullPath,&StatBuf);
    if ( S_ISREG(StatBuf.st_mode) &&  FileFilter(NameList[i]->d_name,FileX->Mask) ) 
         { strcpy(FileX->Names[FileX->N],NameList[i]->d_name); ++FileX->N; ++FileX->Files; }
    }

if (GTK_IS_WIDGET(Table)) gtk_widget_destroy(Table);
NRow=FileX->N/3+((FileX->N%3)>0);
if (NRow>0) 
   {
   Table=gtk_table_new(NRow,3,TRUE);
   for (Row=0;Row<NRow;++Row) for (Col=0;Col<3;++Col)                               //Table to display folders and files
       {
       i=3*Row+Col; if (i>=FileX->N) continue;
       But=gtk_button_new(); gtk_widget_set_usize(GTK_WIDGET(But),152,0);
       gtk_table_attach(GTK_TABLE(Table),But,Col,Col+1,Row,Row+1,GTK_FILL,GTK_SHRINK,0,0);
       gtk_signal_connect(GTK_OBJECT(But),"button_press_event",GTK_SIGNAL_FUNC(FSelect),GINT_TO_POINTER(i));
       HBox=gtk_hbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(But),HBox);
       sprintf(FullPath,"%s%c%s",FileX->Path,'/',FileX->Names[i]);
       stat(FullPath,&StatBuf);
       if (S_ISDIR(StatBuf.st_mode))                                                                   //It is a directory
          {
          SetStyleRecursively(But,FolderStyle);
          AbbreviateFileName(LStr,FileX->Names[i],20); sprintf(DStr,"[%s]",LStr);
          Label=gtk_label_new(DStr); SetStyleRecursively(Label,FolderStyle);
          gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,3);
          }
      else                                                                            //It is a file of matching file type 
         {
         SetStyleRecursively(But,FileStyle);
         AbbreviateFileName(LStr,FileX->Names[i],20);
         Label=gtk_label_new(LStr); SetStyleRecursively(Label,FileStyle);
         gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
         }
       }
   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(ScrollW),Table);
   gtk_widget_show_all(ScrollW);
   }
if (N>-1) free(NameList);
}
//----------------------------------------------------------------------------------------------------------------------
void MaskChanged(GtkWidget *Entry,gpointer Unused)
{
gchar L2Str[30];

strcpy(FileX->Mask,gtk_entry_get_text(GTK_ENTRY(Entry))); 
sprintf(L2Str,"*%s Please Rescan",FileX->Mask);
gtk_label_set_text(GTK_LABEL(FileX->Label2),L2Str);
}
//----------------------------------------------------------------------------------------------------------------------
void FileChanged(GtkWidget *Entry,gpointer Unused)
{
sprintf(FileX->TargetFile,"%s%s",gtk_entry_get_text(GTK_ENTRY(Entry)),FileX->Mask);
gtk_label_set_text(GTK_LABEL(FileX->Label1),FileX->TargetFile);
gtk_widget_set_sensitive(FileX->But,TRUE);
}
//----------------------------------------------------------------------------------------------------------------------
void FileOpenUp(GtkWidget *But,gpointer Unused)
{
gchar L2Str[LONG_TEXT_FIELD];

strcpy(strrchr(FileX->Path,'/'),"\0"); if (strlen(FileX->Path)==0) strcpy(FileX->Path,"/");
gtk_entry_set_text(GTK_ENTRY(FileX->PEntry),FileX->Path);
DisplayFiles();
gtk_entry_set_text(GTK_ENTRY(FileX->FEntry),""); gtk_widget_set_sensitive(FileX->But,FALSE);
gtk_label_set_text(GTK_LABEL(FileX->Label1),"");
sprintf(L2Str,"%d files found",FileX->Files); gtk_label_set_text(GTK_LABEL(FileX->Label2),L2Str);
}
//----------------------------------------------------------------------------------------------------------------------
void FileOpenHome(GtkWidget *But,gpointer Unused)
{
char *Str,L2Str[LONG_TEXT_FIELD];

Str=getenv("HOME"); strcpy(FileX->Path,Str); gtk_entry_set_text(GTK_ENTRY(FileX->PEntry),FileX->Path);
DisplayFiles();
gtk_entry_set_text(GTK_ENTRY(FileX->FEntry),""); gtk_widget_set_sensitive(FileX->But,FALSE);
gtk_label_set_text(GTK_LABEL(FileX->Label1),"");
sprintf(L2Str,"%d files found",FileX->Files); gtk_label_set_text(GTK_LABEL(FileX->Label2),L2Str);
}
//----------------------------------------------------------------------------------------------------------------------
void FileOpenCancel(GtkWidget *But,GtkWidget *Win)
{ g_free(FileX); gtk_widget_destroy(Win); }
//----------------------------------------------------------------------------------------------------------------------
gint FileOpenDelete(GtkWidget *W,GdkEvent *Event,gpointer Data)
{ g_free(FileX); return FALSE; }
//----------------------------------------------------------------------------------------------------------------------
void FileOpenReScan(GtkWidget *But,gpointer Unused)
{
gchar L2Str[LONG_TEXT_FIELD];
const gchar *Entry;

Entry=gtk_entry_get_text(GTK_ENTRY(FileX->PEntry)); strcpy(FileX->Path,Entry);
DisplayFiles();
sprintf(L2Str,"%d files found",FileX->Files); gtk_label_set_text(GTK_LABEL(FileX->Label2),L2Str);
gtk_entry_set_text(GTK_ENTRY(FileX->FEntry),""); gtk_widget_set_sensitive(FileX->But,FALSE);
gtk_label_set_text(GTK_LABEL(FileX->Label1),"");
}
//----------------------------------------------------------------------------------------------------------------------
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist)
// Must have FileX=g_new(struct FileSelectType,1) just before reference to FileOpenNew
// Title=Window title, Parent=Window wrt to which the file browser will be set_transient (keep NULL if not needed)
// X,Y=Window position, OpenToRead=TRUE to open file for read or FALSE to open file for write
// StartPath=initial directory, eg. "." or "/home/lamps/lamps"
// Mask eg. ".ban" (limited to 3 chars); "" will give all files; "d" will give all files ending in 'd'
// MaskEditable=TRUE allows the user to change the Mask
// CallBack=name of callback routine eg. &ReadFile, must g_free(FileX) in this except if Persist=TRUE
// Persist=TRUE keeps the browser open after selection and the "Cancel" button is replaced by "Dismiss" 
//         in this case do not g_free(FileX) in CallBack
{
GtkWidget *VBox,*HBox,*But,*Label;
gint i,StatOut;
struct stat StatBuf;
gchar L2Str[LONG_TEXT_FIELD];
static GdkColor Red  = {0,0xFFFF,0x0000,0x0000};
static GdkColor Blue = {0,0x0000,0x0000,0xFFFF};
GtkStyle *RedStyle,*BlueStyle;

RedStyle=gtk_style_copy(gtk_widget_get_default_style());
RedStyle->fg[0]=RedStyle->text[0]=Red;
BlueStyle=gtk_style_copy(gtk_widget_get_default_style());
BlueStyle->fg[0]=BlueStyle->text[0]=Blue;

strcpy(FileX->Mask,Mask); FileX->Index=-1; FileX->N=0;
for (i=0;i<MAX_DIR_ENTRIES;++i) strcpy(FileX->Names[i],""); 

Table=NULL;
FileX->Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(FileX->Win);
if (Parent) gtk_window_set_transient_for(GTK_WINDOW(FileX->Win),GTK_WINDOW(Parent));
gtk_window_set_title(GTK_WINDOW(FileX->Win),Title);
gtk_signal_connect(GTK_OBJECT(FileX->Win),"delete_event",GTK_SIGNAL_FUNC(FileOpenDelete),NULL);
gtk_container_set_border_width(GTK_CONTAINER(FileX->Win),10);
gtk_widget_set_uposition(GTK_WIDGET(FileX->Win),X,Y);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(FileX->Win),VBox);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
if (OpenToRead) Label=gtk_label_new("Look in:"); else Label=gtk_label_new("Save in:");
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
if (!strcmp(StartPath,".")) getcwd(FileX->Path,LONG_TEXT_FIELD); else strcpy(FileX->Path,StartPath);
StatOut=stat(FileX->Path,&StatBuf);                               //Check if the supplied StartPath is a valid directory
if (StatOut==-1) getcwd(FileX->Path,LONG_TEXT_FIELD);                                                    //stat in error
else if (!S_ISDIR(StatBuf.st_mode)) getcwd(FileX->Path,LONG_TEXT_FIELD);                               //Not a directory
FileX->PEntry=gtk_entry_new_with_max_length(LONG_TEXT_FIELD); gtk_entry_set_text(GTK_ENTRY(FileX->PEntry),FileX->Path);
gtk_widget_set_size_request(GTK_WIDGET(FileX->PEntry),320,-1);
gtk_box_pack_start(GTK_BOX(HBox),FileX->PEntry,FALSE,FALSE,0);

But=gtk_button_new_from_stock(GTK_STOCK_GO_UP); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);

gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FileOpenUp),NULL);
But=gtk_button_new_from_stock(GTK_STOCK_HOME); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FileOpenHome),NULL);
But=gtk_button_new_with_label("Rescan"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FileOpenReScan),NULL);

ScrollW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrollW),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
gtk_box_pack_start(GTK_BOX(VBox),ScrollW,FALSE,FALSE,0);
gtk_widget_set_usize(GTK_WIDGET(ScrollW),0,340);

DisplayFiles();

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
Label=gtk_label_new("File Name:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
gtk_widget_set_usize(GTK_WIDGET(Label),100,0);
FileX->FEntry=gtk_entry_new_with_max_length(LONG_TEXT_FIELD); 
gtk_box_pack_start(GTK_BOX(HBox),FileX->FEntry,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(FileX->FEntry),160,-1);
gtk_signal_connect(GTK_OBJECT(FileX->FEntry),"changed",GTK_SIGNAL_FUNC(FileChanged),NULL);
FileX->Label1=gtk_label_new(""); gtk_box_pack_start(GTK_BOX(HBox),FileX->Label1,FALSE,FALSE,0);
SetStyleRecursively(FileX->Label1,RedStyle); gtk_widget_set_usize(GTK_WIDGET(FileX->Label1),170,0);
if (OpenToRead) FileX->But=gtk_button_new_from_stock(GTK_STOCK_OPEN); else FileX->But=gtk_button_new_from_stock(GTK_STOCK_SAVE);
gtk_box_pack_start(GTK_BOX(HBox),FileX->But,FALSE,FALSE,0);
gtk_widget_set_sensitive(FileX->But,FALSE);
gtk_signal_connect(GTK_OBJECT(FileX->But),"clicked",GTK_SIGNAL_FUNC(CallBack),NULL);
if (!Persist) gtk_signal_connect_object(GTK_OBJECT(FileX->But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                        GTK_OBJECT(FileX->Win));

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
if (OpenToRead) Label=gtk_label_new("Files of type:"); else Label=gtk_label_new("Save as type:");
gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(Label),100,-1);
FileX->MEntry=gtk_entry_new_with_max_length(4); 
gtk_box_pack_start(GTK_BOX(HBox),FileX->MEntry,FALSE,FALSE,0);
gtk_entry_set_text(GTK_ENTRY(FileX->MEntry),Mask); 
gtk_widget_set_usize(GTK_WIDGET(FileX->MEntry),110,0);
if (MaskEditable) gtk_entry_set_editable(GTK_ENTRY(FileX->MEntry),TRUE);
else              gtk_entry_set_editable(GTK_ENTRY(FileX->MEntry),FALSE);
gtk_signal_connect(GTK_OBJECT(FileX->MEntry),"changed",GTK_SIGNAL_FUNC(MaskChanged),NULL);
But=gtk_button_new_with_label("Rescan"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
sprintf(L2Str,"%d files found",FileX->Files);
FileX->Label2=gtk_label_new(L2Str); gtk_box_pack_start(GTK_BOX(HBox),FileX->Label2,FALSE,FALSE,0);
gtk_widget_set_size_request(GTK_WIDGET(FileX->Label2),170,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FileOpenReScan),NULL);
SetStyleRecursively(FileX->Label2,BlueStyle);
if (Persist) But=gtk_button_new_with_label("Dismiss"); 
else         But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); 
gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FileOpenCancel),FileX->Win);
gtk_widget_show_all(FileX->Win);
gtk_style_unref(RedStyle); gtk_style_unref(BlueStyle);
}
//----------------------------------------------------------------------------------------------------------------------
