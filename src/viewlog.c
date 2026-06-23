#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "lamps.h"
#define FIELD_WIDTH 56
#define REM_WIDTH 66
#define START_LINES 17
#define REM_LINES 4
#define STOP_LINES 13
#define FULL_OFFSET (FIELD_WIDTH*(START_LINES+STOP_LINES)+(REM_WIDTH*REM_LINES))
#define PART_OFFSET (FIELD_WIDTH*START_LINES+REM_WIDTH*REM_LINES)

/*Global Definitions*/
struct LogG            {gchar Heading[8][90]; GtkWidget *Title[4]; GtkWidget *But; GtkWidget *CWin; GtkWidget *G[8];
                        long FilePointer; gint Status;};
struct LogG *LogG;
gchar NameF[MAX_DIR_STRLEN];

/*External Globals*/
extern gboolean LogOutBox;                                                               //To prevent multiple instances
extern enum ProgramState ProgramState;                                        //The current running state of the program
extern gchar LogDir[MAX_DIR_STRLEN];

/*Function templates*/
void Attention(gint XPos,gchar *Messg);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void UnPad(gchar *Str);
/*----------------------------------------------------------------------------------------------------------------------*/
void ShowEntry(gint Which)                             //Show a log file entry. Which=0(Last), 1(Prev), 2(Next), 3(First)
{                                                             //LogG->Status: 0=No file, 1=Empty, 2=No run stop, 3=Normal
FILE *Fp;
gchar Str[74],Str1[166];
gint L,i;
glong X;

if (!(Fp=fopen(NameF,"r"))) { LogG->Status=0; return; }                                  //File lamps.log does not exist
switch (Which)
   {
   case 0: //Last entry
      if (fseek(Fp,-PART_OFFSET,SEEK_END)==-1)                                                    //No entries in log file
         {
         LogG->Status=1; 
         for (i=0;i<8;++i) gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[i])->child),"");                 //Clear the table
         return;
         }
      LogG->FilePointer=ftell(Fp); fgets(Str,60,Fp);  LogG->Status=2;                       //The run has not been stopped
      if (strncmp("Run Name:",Str,9))                                                                        //Normal stop 
         { fseek(Fp,-FULL_OFFSET,SEEK_END); LogG->FilePointer=ftell(Fp); fgets(Str,60,Fp); LogG->Status=3; }
      break;
   case 1: //Prev entry
      fseek(Fp,LogG->FilePointer,SEEK_SET);
      if (fseek(Fp,-FULL_OFFSET,SEEK_CUR)==-1) { fclose(Fp); return; }
      while (TRUE)                                                                  //Read ahead searching for "Run Name:"
         {
         X=ftell(Fp); 
         if (fgets(Str,60,Fp)==NULL) { fclose(Fp); return; } 
         if (!strncmp("Run Name:",Str,9)) { LogG->FilePointer=X; break; }
         }
      break;
   case 2: //Next entry
      fseek(Fp,LogG->FilePointer,SEEK_SET);
      if (fseek(Fp,PART_OFFSET,SEEK_CUR)==-1) { fclose(Fp); return; }
      while (TRUE)                                                                  //Read ahead searching for "Run Name:"
         { 
         X=ftell(Fp); 
         if (fgets(Str,60,Fp)==NULL) { fclose(Fp); return; } 
         if (!strncmp("Run Name:",Str,9)) { LogG->FilePointer=X; break; }
         }
      break;
   case 3: //First entry 
      while (TRUE)                           //Start from the begining of the file and read ahead searching for "Run Name:"
         { 
         X=ftell(Fp); 
         if (fgets(Str,60,Fp)==NULL) { fclose(Fp); return; } 
         if (!strncmp("Run Name:",Str,9)) { LogG->FilePointer=X; break; }
         }
         break;
   }
strcpy(Str1,""); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //Run Name
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                  //Setup file 
fgets(Str,60,Fp); L=strlen(Str1);                                                                    //Start date and time 
strncpy(&Str1[L],&Str[24],10); Str1[L+10]='\n'; L+=11;
strncpy(&Str1[L],&Str[35],8); Str1[L+8]='\0';
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[0])->child),Str1);

fgets(Str,60,Fp); fgets(Str,60,Fp);                                                                     //1d Spec, 2d Spec

strcpy(Str1,"");
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User  1
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User  2
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User  3
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1);                                                        //User  4
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[4])->child),Str1);

strcpy(Str1,"");
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User  5
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User  6
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User  7
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1);                                                        //User  8
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[5])->child),Str1);

strcpy(Str1,"");
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User  9
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User 10
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                     //User 11
fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1);                                                        //User 12
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[6])->child),Str1);

strcpy(Str1,"");
fgets(Str,70,Fp); strncat(Str1,&Str[24],40); UnPad(Str1); strcat(Str1,"\n");                                   //Remarks 1
fgets(Str,70,Fp); strncat(Str1,&Str[24],40); UnPad(Str1); strcat(Str1,"\n");                                   //Remarks 2
fgets(Str,70,Fp); strncat(Str1,&Str[24],40); UnPad(Str1); strcat(Str1,"\n");                                   //Remarks 3
fgets(Str,70,Fp); strncat(Str1,&Str[24],40); UnPad(Str1);                                                      //Remarks 4
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[7])->child),Str1);

fgets(Str,60,Fp);
if (!strncmp(Str,"Stop",4))                      //The remaining fields are available only if the run was properly stopped
   {
   strcpy(Str1,"");  strncat(Str1,&Str[35], 8); UnPad(Str1); strcat(Str1,"\n");                                     //Stop
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                              //Elapsed sec
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                         //Buffers Acquired
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1);                                           //Buffers Processed
   gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[1])->child),Str1);

   strcpy(Str1,"");
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                   //Events
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                    //Evt/s
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                //Dead Time
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1);                                                  //List Bytes
   gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[2])->child),Str1);

   strcpy(Str1,"");
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                 //Scaler 1
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                 //Scaler 2
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1); strcat(Str1,"\n");                                 //Scaler 3
   fgets(Str,60,Fp); strncat(Str1,&Str[24],19); UnPad(Str1);                                                    //Scaler 4

   gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[3])->child),Str1);
   }
else                                                                                       //No stop information available
   {
   gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[1])->child),"No information");
   gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[2])->child),"No information");
   gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[3])->child),"No information");
   }
fclose(Fp);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ViewLogPrev(GtkWidget *W,gpointer Data)
{ if (LogG->Status>1) ShowEntry(1); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ViewLogNext(GtkWidget *W,gpointer Data)
{ if (LogG->Status>1) ShowEntry(2); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ViewLogHome(GtkWidget *W,gpointer Data)
{ if (LogG->Status>1) ShowEntry(3); }
/*----------------------------------------------------------------------------------------------------------------------*/
void ViewLogEnd(GtkWidget *W,gpointer Data)
{ if (LogG->Status>1) ShowEntry(0); }
/*----------------------------------------------------------------------------------------------------------------------*/
void LogDestroyed(GtkWidget *W,gpointer Win)
{
gtk_widget_destroy(GTK_WIDGET(Win));                                                                     //Destroy window
g_free(LogG);
LogOutBox=FALSE;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void LogDelEntry(GtkWidget *W,gpointer Data)
{
FILE *Dp,*Fp;
gchar Str[165];

if (LogG->Status<2) return;                                                                //Either 'No file' or 'Empty'
if (!(Fp=fopen(NameF,"r"))) return;                            //lamps.log does not exist (wont happen, see prev. line)
if (!(Dp=fopen("lamps.tmp","w"))) { Attention(0,"Couldnt create temp file\nOperation failed!"); fclose(Fp); return; }
while (TRUE) { if (ftell(Fp)==LogG->FilePointer) break; fgets(Str,164,Fp); fputs(Str,Dp); }//Copy lines upto current entry
if (fseek(Fp,PART_OFFSET,SEEK_CUR)==-1) { fclose(Fp); return; }
while (TRUE)                                                                        //Read ahead searching for "Run Name:"
      {
      if (fgets(Str,60,Fp)==NULL) 
         { 
         fclose(Dp); fclose(Fp); 
         if (ProgramState==AcqOn) Attention(0,"Can't delete... run in progress");
         else { rename("lamps.tmp",NameF); ShowEntry(0); }
         return; 
         }
      if (!strncmp("Run Name:",Str,9)) { fputs(Str,Dp); break; }
      }
while (TRUE) { if (fgets(Str,164,Fp)==NULL) break; fputs(Str,Dp); }                         //Copy lines (if any) upto EOF
fclose(Dp); fclose(Fp); rename("lamps.tmp",NameF);                                                             //Cleanup
if ((LogG->FilePointer)>FULL_OFFSET) { (LogG->FilePointer)-=FULL_OFFSET; ShowEntry(2); }
else ShowEntry(0);
}
//----------------------------------------------------------------------------------------------------------------------
void YesDelAll(GtkWidget *W,GtkWidget *Win)
{
FILE *Dp,*Fp;
gchar Str[165];
gint i;

if (LogG->Status<2) return;                                                                //Either 'No file' or 'Empty'
if (!(Fp=fopen(NameF,"r"))) return;                                                      //File lamps.log does not exist
if (!(Dp=fopen("lamps.tmp","w"))) { Attention(0,"Couldnt create temp file\nOperation failed!"); fclose(Fp); return; }
while (TRUE) { if (fgets(Str,164,Fp)==NULL) break; fputs(Str,Dp); if (Str[0] == '-') break; }    //Copy the header block
fclose(Dp); fclose(Fp); rename("lamps.tmp",NameF);                                                             //Cleanup
for (i=0;i<8;++i) gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[i])->child),"");                            //Clear table
gtk_widget_destroy(Win);                                                                    //Remove 'Are you sure?' box
}
/*--------------------------------------------------------------------------------------------------------------------*/
void LogDelAll(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But;

if (ProgramState==AcqOn) 
   { Attention(0,"Not permitted ... run in progress"); return; }                 //Cant allow DeleteAll, run in progress
Win=gtk_dialog_new(); gtk_grab_add(Win); gtk_window_set_title(GTK_WINDOW(Win),"Are you sure?");
gtk_widget_set_uposition(GTK_WIDGET(Win),415,280); gtk_widget_set_usize(GTK_WIDGET(Win),300,100);
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
Label=gtk_label_new("Really clear all log file entries?"); 
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,TRUE,FALSE,5);
Label=gtk_label_new("(Headings will be retained)"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,TRUE,FALSE,5);
But=gtk_button_new_with_label("Yes"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(YesDelAll),GTK_OBJECT(Win));
But=gtk_button_new_with_label("No"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win); 
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ChangeTitlesOkay(GtkWidget *W,gpointer Data)
{
gint Col,i,j,L;
const gchar *Entry;
FILE *Fp;

Col=GPOINTER_TO_INT(Data);
for (i=0;i<4;++i)
    {
    Entry=gtk_entry_get_text(GTK_ENTRY(LogG->Title[i]));
    L=strlen(Entry);
    strcpy(&(LogG->Heading[Col][21*i]),Entry); for (j=L;j<20;++j) LogG->Heading[Col][21*i+j]=' '; 
    LogG->Heading[Col][21*i+20]='\n';
    }
LogG->Heading[Col][83]='\0';
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->But)->child),LogG->Heading[Col]);
gtk_widget_destroy(LogG->CWin);
if (!(Fp=fopen(NameF,"r+")))                                                               //Open lamps.log for re-writing
   if (!(Fp=fopen(NameF,"w"))) { Attention(0,"Couldnt create lamps.log"); return; }            //Create and open lamps.log
for (i=4;i<7;++i) fprintf(Fp,"%s\n",LogG->Heading[i]);          //Write user defined headings at the begining of lamps.log
fprintf(Fp,"-------------------------------------------------------\n");
fclose(Fp);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ChangeTitles(GtkWidget *W,gpointer Data)
{
GtkWidget *VBox,*Label,*HBox,*But;
gint Col,i;
gchar Str[40],Ttl[4][20];

Col=GPOINTER_TO_INT(Data);
LogG->But=W;
LogG->CWin=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(LogG->CWin);                         //Make the window modal
gtk_window_set_title(GTK_WINDOW(LogG->CWin),"Change Titles");
gtk_widget_set_uposition(GTK_WIDGET(LogG->CWin),100,388);
gtk_widget_set_usize(GTK_WIDGET(LogG->CWin),236,150);
VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(LogG->CWin),VBox);                //VBox for the whole window
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Enter new titles"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,10);
for (i=0;i<4;i++)
    {
    strncpy(Ttl[i],&(LogG->Heading[Col][21*i]),20); Ttl[i][20]='\0';
    UnPad(Ttl[i]);                                                                             //Get rid of the rhs blanks
    HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
    sprintf(Str,"Title %d",i+1); Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
    LogG->Title[i]=gtk_entry_new_with_max_length(20); gtk_box_pack_start(GTK_BOX(HBox),LogG->Title[i],FALSE,FALSE,2);
    gtk_widget_set_usize(GTK_WIDGET(LogG->Title[i]),172,22);
    gtk_entry_set_text(GTK_ENTRY(LogG->Title[i]),Ttl[i]);
    }
HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Okay"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ChangeTitlesOkay),GINT_TO_POINTER(Col));
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(LogG->CWin));
gtk_widget_show_all(LogG->CWin);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ChangeRemOkay(GtkWidget *W,gpointer Data)
{
gint Col,i,j,L;
const gchar *Entry; 
gchar Str[165],Str1[74];
FILE *Fp;

Col=GPOINTER_TO_INT(Data);
for (i=0;i<4;++i)
    {
    Entry=gtk_entry_get_text(GTK_ENTRY(LogG->Title[i])); L=strlen(Entry);
    strcpy(&Str[41*i],Entry); for (j=L;j<40;++j) Str[41*i+j]=' '; Str[41*i+40]='\n';
    }
Str[163]='\0';
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[Col])->child),Str);
gtk_widget_destroy(LogG->CWin);

if (!(Fp=fopen(NameF,"r+"))) return;                                                     //File lamps.log does not exist
fseek(Fp,LogG->FilePointer,SEEK_SET);
for (i=0;i<(Col-3);++i) for (j=0;j<4;++j) fgets(Str1,70,Fp);
fgets(Str1,60,Fp);
for (i=0;i<4;++i)
    {
    strncpy(Str1,&Str[41*i],40); Str1[40]='\0';
    fprintf(Fp,"Remarks %1d:              %s]\n",i+1,Str1);
    }
fclose(Fp);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ChangeRem(GtkWidget *W,gpointer Data)
{
GtkWidget *VBox,*Label,*HBox,*But;
gint Col,i,J,Js;
gchar *Str1[180],Str[60],Ttl[4][40],Str2[165];

if (LogG->Status<2) return;
Col=GPOINTER_TO_INT(Data);
LogG->But=W;
LogG->CWin=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(LogG->CWin);                        //Make the window modal
gtk_window_set_title(GTK_WINDOW(LogG->CWin),"Change Remarks");
gtk_widget_set_uposition(GTK_WIDGET(LogG->CWin),400,388);
gtk_widget_set_usize(GTK_WIDGET(LogG->CWin),370,170);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(LogG->CWin),VBox);                //VBox for the whole window

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Enter new remarks"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,10);

gtk_label_get(GTK_LABEL(GTK_BIN(LogG->G[Col])->child),Str1);
strncpy(Str2,*Str1,163); Str2[163]='\0';

for (i=0,Js=0;i<4;++i)
    {
    for (J=Js;J<164;++J) 
        { 
        if ((Str2[J]=='\n') || (Str2[J]=='\0')) { Ttl[i][J-Js]='\0'; break; } 
        else                                    Ttl[i][J-Js]=Str2[J]; 
        }
    UnPad(Ttl[i]); Js=J+1;
    HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
    sprintf(Str,"Remark %d",i+1); Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
    LogG->Title[i]=gtk_entry_new_with_max_length(40); gtk_box_pack_start(GTK_BOX(HBox),LogG->Title[i],FALSE,FALSE,2);
    gtk_widget_set_usize(GTK_WIDGET(LogG->Title[i]),270,22);
    gtk_entry_set_text(GTK_ENTRY(LogG->Title[i]),Ttl[i]);
    }

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Okay"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ChangeRemOkay),GINT_TO_POINTER(Col));
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(LogG->CWin));

gtk_widget_show_all(LogG->CWin);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ChangeInfoOkay(GtkWidget *W,gpointer Data)
{
gint Col,i,j,L;
const gchar *Entry; 
gchar Str[100],Str1[64];
FILE *Fp;

Col=GPOINTER_TO_INT(Data);
for (i=0;i<4;++i)
    {
    Entry=gtk_entry_get_text(GTK_ENTRY(LogG->Title[i])); L=strlen(Entry);
    strcpy(&Str[20*i],Entry); for (j=L;j<19;++j) Str[20*i+j]=' '; Str[20*i+19]='\n';
    }
Str[79]='\0';
gtk_label_set_text(GTK_LABEL(GTK_BIN(LogG->G[Col])->child),Str);
gtk_widget_destroy(LogG->CWin);

if (!(Fp=fopen(NameF,"r+"))) return;                                                     //File lamps.log does not exist
fseek(Fp,LogG->FilePointer,SEEK_SET);
for (i=0;i<(Col-3);++i) for (j=0;j<4;++j) fgets(Str1,60,Fp); 
fgets(Str1,60,Fp);
for (i=0;i<4;++i) 
    { 
    strncpy(Str1,&Str[20*i],20); Str1[19]='\0'; 
    fprintf(Fp,"User %2d:                %s           }\n",4*(Col-4)+i+1,Str1); 
    }
fclose(Fp);
}
/*--------------------------------------------------------------------------------------------------------------------*/
void ChangeInfo(GtkWidget *W,gpointer Data)
{
GtkWidget *VBox,*Label,*HBox,*But;
gint Col,i,J,Js;
gchar *Str1[100],Str[40],Ttl[4][20],Str2[100];

if (LogG->Status<2) return;
Col=GPOINTER_TO_INT(Data);
LogG->But=W;
LogG->CWin=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(LogG->CWin);                         //Make the window modal
gtk_window_set_title(GTK_WINDOW(LogG->CWin),"Change Entries");
gtk_widget_set_uposition(GTK_WIDGET(LogG->CWin),400,388);
gtk_widget_set_usize(GTK_WIDGET(LogG->CWin),200,170);

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(LogG->CWin),VBox);                //VBox for the whole window

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Enter new information"); gtk_box_pack_start(GTK_BOX(HBox),Label,TRUE,FALSE,10);

gtk_label_get(GTK_LABEL(GTK_BIN(LogG->G[Col])->child),Str1);
strncpy(Str2,*Str1,80); Str2[80]='\0'; 

for (i=0,Js=0;i<4;i++)
    {
    for (J=Js;J<81;++J) { if ((Str2[J]=='\n') || (Str2[J]=='\0')) { Ttl[i][J-Js]='\0'; break; } else Ttl[i][J-Js]=Str2[J]; }
    UnPad(Ttl[i]); Js=J+1;
    HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
    sprintf(Str,"Line %d",i+1); Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,10);
    LogG->Title[i]=gtk_entry_new_with_max_length(19); gtk_box_pack_start(GTK_BOX(HBox),LogG->Title[i],FALSE,FALSE,2);
    gtk_widget_set_usize(GTK_WIDGET(LogG->Title[i]),130,22);
    gtk_entry_set_text(GTK_ENTRY(LogG->Title[i]),Ttl[i]);
    }

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Okay"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ChangeInfoOkay),GINT_TO_POINTER(Col));
But=gtk_button_new_with_label("Cancel"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(LogG->CWin));

gtk_widget_show_all(LogG->CWin);
}
//----------------------------------------------------------------------------------------------------------------------
void ReadHeadings(void)                            //lamps.log has possibly changed headings at the begining of the file
{
gint i,j;
gchar Str[25];
FILE *Fp;

if (!(Fp=fopen(NameF,"r"))) return;                                                      //File lamps.log does not exist
for (i=4;i<7;++i) 
    {
    strcpy(LogG->Heading[i],"");
    for (j=0;j<4;++j) { fgets(Str,22,Fp); Str[22]='\0'; strcat(LogG->Heading[i],Str); }
    }
fclose(Fp);
}
//----------------------------------------------------------------------------------------------------------------------
void ViewLog(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*HBox,*VBox,*But,*Scr,*Table;
gint ColWidth[8]={100,100,100,100,144,144,144,300};
GtkStyle *HeadingStyle,*FixStyle,*FlexStyle;
static GdkColor C_White  = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor C_Blue   = {0,0x0000,0x0000,0xAAAA};
static GdkColor C_Red    = {0,0xAAAA,0x0000,0x0000};
static GdkColor C_Green  = {0,0x0000,0x5555,0x0000};
gint i;

if (LogOutBox) return;
sprintf(NameF,"%s/lamps.log",LogDir);
HeadingStyle=gtk_style_copy(gtk_widget_get_default_style());                              //Copy default style to this style
for (i=0;i<5;i++) { HeadingStyle->fg[i]=HeadingStyle->text[i]=C_Blue; HeadingStyle->bg[i]=C_White; }           //Set colours
FixStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) FixStyle->fg[i]=FixStyle->text[i]=C_Green;
FlexStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) FlexStyle->fg[i]=FlexStyle->text[i]=C_Red;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); LogOutBox=TRUE;
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(LogDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"View/Edit Log File");
gtk_widget_set_uposition(GTK_WIDGET(Win),0,TOP_SPACE+1);
gtk_container_set_border_width(GTK_CONTAINER(Win),4);

LogG=g_new(struct LogG,1);                                         //Create new structure to be destroyed in LogDestroyed()
strcpy(LogG->Heading[0],"Run Name\nSetup File\nStart Date\nStart Time");                           //Setup default headings
strcpy(LogG->Heading[1],"Stop\nElapsed sec\nBufs Acq\nBufs Proc");
strcpy(LogG->Heading[2],"Events\nEvt/s\nDead Time\nList Bytes");
strcpy(LogG->Heading[3],"Scaler 1\nScaler 2\nScaler 3\nScaler 4");
strcpy(LogG->Heading[4],"Ion, q+             \nE, TV               \nNMR, I              \nCI (scale)          ");
strcpy(LogG->Heading[5],"Target              \nTgt. Rot.           \nInfo 1              \nInfo 2              ");
strcpy(LogG->Heading[6],"Info 3              \nInfo 4              \nInfo 5              \nInfo 6              ");
strcpy(LogG->Heading[7],"Remarks");

ReadHeadings();                                              //Check lamps.log to see if the user has changed some headings

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);                         //VBox for the whole window

Scr=gtk_scrolled_window_new(NULL,NULL); gtk_box_pack_start(GTK_BOX(VBox),Scr,FALSE,FALSE,0);
gtk_widget_set_usize(GTK_WIDGET(Scr),1004,210);
gtk_container_set_border_width(GTK_CONTAINER(Scr),6);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Scr),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
Table=gtk_table_new(2,8,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),2); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Scr),Table);                                //Pack Table into Scr
for (i=0;i<8;++i)
    {
    But=gtk_button_new_with_label(LogG->Heading[i]); SetStyleRecursively(But,HeadingStyle);
    gtk_widget_set_usize(GTK_WIDGET(But),ColWidth[i],80);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    if ((i>3) && (i<7)) gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ChangeTitles),GINT_TO_POINTER(i));
    }

for (i=0;i<8;++i)
    {
    LogG->G[i]=gtk_button_new_with_label(""); 
    if (i<4) SetStyleRecursively(LogG->G[i],FixStyle); else SetStyleRecursively(LogG->G[i],FlexStyle);
    gtk_widget_set_usize(GTK_WIDGET(LogG->G[i]),ColWidth[i],80);
    gtk_table_attach(GTK_TABLE(Table),LogG->G[i],i,i+1,1,2,GTK_FILL,GTK_SHRINK,0,0);
    if (i>3 && i<7) gtk_signal_connect(GTK_OBJECT(LogG->G[i]),"clicked",GTK_SIGNAL_FUNC(ChangeInfo),GINT_TO_POINTER(i));
    if (i==7) gtk_signal_connect(GTK_OBJECT(LogG->G[i]),"clicked",GTK_SIGNAL_FUNC(ChangeRem),GINT_TO_POINTER(i));
    }
ShowEntry(0);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Prev"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ViewLogPrev),NULL);
But=gtk_button_new_with_label("Next"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ViewLogNext),NULL);
But=gtk_button_new_with_label("Home"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ViewLogHome),NULL);
But=gtk_button_new_with_label("End"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ViewLogEnd),NULL);
But=gtk_button_new_with_label("Delete  Entry"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(LogDelEntry),NULL);
But=gtk_button_new_with_label("Delete All"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(LogDelAll),NULL);
But=gtk_button_new_with_label("Exit"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));

gtk_widget_show_all(Win);
gtk_style_unref(HeadingStyle); gtk_style_unref(FixStyle); gtk_style_unref(FlexStyle);
}
/*--------------------------------------------------------------------------------------------------------------------*/
