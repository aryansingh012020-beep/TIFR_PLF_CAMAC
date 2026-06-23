/* This program executes CAMAC commands. Useful for hardware testing */
//   gcc -o cmc100test `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` cmc100test.c useful.c open.c
#define GTK_ENABLE_BROKEN
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "libcamac.h"

#define CMC_CTL_RESET   _IO('Z', 0)
#define CMC_CTL_R1      _IO('Z', 1)

#define MAX_FNAME_LENGTH   80                              //Max no of characters for file names (with full absolute path)
#define MAX_DIR_ENTRIES  5000                                      //FileX Widget: maximum number of files/sub-directories
#define MAX_DIR_STRLEN    255                           //FileX Widget: maximum string length for file and directory names
#define MAX_MASK_SIZE       5                                              //FileX Widget: maximum length of the file mask
#define TOP_SPACE         119                                                      //This space holds the main_menu widget

/*Global Variables*/
gint Fd;
gboolean DriverOpen=FALSE;

/*Global Definitions*/
struct CamTest {
               GtkWidget *Stn; 
               GtkWidget *SetMode; GtkWidget *SetLLD; GtkWidget *SetULD; GtkWidget *InputData;
               GtkWidget *SetA; GtkWidget *SetF; GtkWidget *SetOffs;
               GtkWidget *Output;
               };
struct CamTest *CamTest;
struct FileSelectType  {                                                                 //For the FileSelect widget FileX
                       GtkWidget *But; GtkWidget *FEntry; GtkWidget *PEntry; GtkWidget *MEntry; GtkWidget *Win;
                       GtkWidget *Table; GtkWidget *Label1; GtkWidget *Label2;
                       gint N; gint Index; gint Files;
                       gchar Mask[MAX_MASK_SIZE]; gchar Path[MAX_DIR_STRLEN]; gchar Names[MAX_DIR_ENTRIES][MAX_DIR_STRLEN];
                       gchar TargetFile[MAX_DIR_STRLEN];
                       };
struct FileSelectType *FileX;

/*Function templates*/
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void AtWin(gint X,gint Y,gint XSiz,gint YSiz,gchar *Messg);
void Attention(gint XPos,gchar *Messg);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
//-----------------------------------------------------------------------------------------------------------------------
void CamacZ()
{ 
gboolean XRes,QRes,Lam;

(void)CamacNAF(0,28,8,26,&XRes,&QRes,&Lam);
}
//-----------------------------------------------------------------------------------------------------------------------
void CamacC()
{
gboolean XRes,QRes,Lam;

(void)CamacNAF(0,28,9,26,&XRes,&QRes,&Lam);
}
//-----------------------------------------------------------------------------------------------------------------------
void SetI()
{ 
gboolean XRes,QRes,Lam;

(void)CamacNAF(0,30,9,26,&XRes,&QRes,&Lam);
}
//-----------------------------------------------------------------------------------------------------------------------
void ClrI()
{ 
gboolean XRes,QRes,Lam;

(void)CamacNAF(0,30,9,24,&XRes,&QRes,&Lam);
}
//-----------------------------------------------------------------------------------------------------------------------
int CamacNAF(gint Data,gint N,gint A,gint F,gboolean *XRes,gboolean *QRes,gboolean *Lam)
//Return value is Result. X=XRes, Q=QRes and Lam=Lam Status are made available 
//Note: The CMC100 response word is 0UUUKLQX DDDDDDDD DDDDDDDD DDDDDDDD
{
int x_res, q_res, lam;
int response = camac_naf(Fd, Data, N, A, F, &x_res, &q_res, &lam);
if (XRes) *XRes = x_res;
if (QRes) *QRes = q_res;
if (Lam) *Lam = lam;
return response & 0xFFFFFF;
}
//-----------------------------------------------------------------------------------------------------------------------
int FullCamacNAF(gint Data,gint N,gint A,gint F,gboolean *XRes,gboolean *QRes,gboolean *Lam)
//Return value is Result. X=XRes, Q=QRes and Lam=Lam Status are made available 
//Note: The CMC100 response word is 0UUUKLQX DDDDDDDD DDDDDDDD DDDDDDDD
{
int x_res, q_res, lam;
int response = camac_naf(Fd, Data, N, A, F, &x_res, &q_res, &lam);
if (XRes) *XRes = x_res;
if (QRes) *QRes = q_res;
if (Lam) *Lam = lam;
return response;
}
//-----------------------------------------------------------------------------------------------------------------------
void ExecuteNAF(GtkWidget *W,gpointer Unused)
{
gint Data,N,A,F,Result;
gboolean XRes,QRes,Lam;
gchar Str[80];

Data=atoi(gtk_entry_get_text(GTK_ENTRY(CamTest->InputData)));
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->SetA));
F=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->SetF));
Result=CamacNAF(Data,N,A,F,&XRes,&QRes,&Lam);
sprintf(Str,"Executed Data=%d N=%d A=%d F=%d\nResult:%d X=%d Q=%d L=%d\n",Data,N,A,F,Result,XRes,QRes,Lam);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void ExecuteFullNAF(GtkWidget *W,gpointer Unused)
{
gint Data,N,A,F,Result;
gboolean XRes,QRes,Lam;
gchar Str[80];

Data=atoi(gtk_entry_get_text(GTK_ENTRY(CamTest->InputData)));
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
A=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->SetA));
F=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->SetF));
Result=FullCamacNAF(Data,N,A,F,&XRes,&QRes,&Lam);
sprintf(Str,"Executed Data=%d N=%d A=%d F=%d\nResult:%d X=%d Q=%d L=%d\n",Data,N,A,F,Result,XRes,QRes,Lam);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
sprintf(Str,"24-bit:%d 16-bit:%d 12-bit\n",Result & 0xFFFFFF,Result & 0xFFFF,Result & 0xFFF);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void ExecuteZ(GtkWidget *W,gpointer Unused)
{ CamacZ(); gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Executed Z\n",-1); }
//-----------------------------------------------------------------------------------------------------------------------
void ExecuteC(GtkWidget *W,gpointer Unused)
{ CamacC(); gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Executed C\n",-1); }
//-----------------------------------------------------------------------------------------------------------------------
void ExecuteSetI(GtkWidget *W,gpointer Unused)
{ SetI(); gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Executed Inhibit\n",-1); }
//-----------------------------------------------------------------------------------------------------------------------
void ExecuteClrI(GtkWidget *W,gpointer Unused)
{ ClrI();   gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Cleared Inhibit\n",-1); }
//-----------------------------------------------------------------------------------------------------------------------
void SetDefaultState(GtkWidget *W,gpointer Unused)
{
gint Data,Result;
gboolean XRes,QRes,Lam;
gchar Str[80];

Data=0;
CamacNAF(Data,30,0,17,&XRes,&QRes,&Lam);                                                 //Write zero to control register
sprintf(Str,"Wrote zero to control register\n");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void ExecuteSelfTest(GtkWidget *W,gpointer Unused)
{
gint Data,Result,D;
gboolean XRes,QRes,Lam;
gchar Str[80];

gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Control register:",-1);
Data=g_random_int_range(100,1000);
CamacNAF(Data,30,0,17,&XRes,&QRes,&Lam);                                         //Write random value to control register
Result=CamacNAF(Data,30,0,1,&XRes,&QRes,&Lam);                                                    //Read control register
sprintf(Str,"Wrote: %d, Readback: %d\n",Data,Result);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);

gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Output stream:",-1);
D=0xFFFFFFFF; camac_write(Fd,&D,4); D=0x00000000; camac_write(Fd,&D,4);                                         //write header words
Data=g_random_int_range(100,1000);
D=(12<<24)|Data; camac_write(Fd,&D,4);                                                 //Write 24-bit literal to output stream
D=0x0E000000; camac_write(Fd,&D,4);                                                                    //Flush or trailer word
camac_read(Fd,&Result,4);                                        //Check the response. We expect to recover the 24-bit literal
sprintf(Str,"Wrote: %d, Readback: %d\n",Data,Result & 0xFFFFFF);                        //Dont forget to mask to 24-bits
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);

}
//-----------------------------------------------------------------------------------------------------------------------
void SelfTest(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47);
gtk_window_set_title(GTK_WINDOW(Win),"CMC100 Self Test");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
Label=gtk_label_new("CMC100 Self Test");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(TRUE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,TRUE,FALSE,5);
But=gtk_button_new_with_label("Write-Read"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSelfTest),NULL);
But=gtk_button_new_with_label("Set default state"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SetDefaultState),NULL);

Label=gtk_label_new("Output Window");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL);
gtk_widget_set_size_request(GTK_WIDGET(CamTest->Output),300,250);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE);
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void OperateCM90(GtkWidget *W,gpointer Unused)
{
gint N;
gboolean XRes,QRes,Lam;
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,0,9,&XRes,&QRes,&Lam);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"A=0 F=9 Done\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void OperateCM90inLoop(GtkWidget *W,gpointer Unused)
{
gint N,i;
gboolean XRes,QRes,Lam;
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (i=0;i<100000;++i)
    {
    (void)CamacNAF(0,N,0,9,&XRes,&QRes,&Lam);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"A=0 F=9 Done\n",-1);
    }
}
//-----------------------------------------------------------------------------------------------------------------------
void CM90SyncModule(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47);
gtk_window_set_title(GTK_WINDOW(Win),"CM90 Sync Module");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"BARC CM90 Sync Module Test");
Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(23,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->Stn),52,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Operate F9.A0"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OperateCM90),NULL);
But=gtk_button_new_with_label("Operate in Loop"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OperateCM90inLoop),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6);
gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_usize(GTK_WIDGET(CamTest->InputData),50,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetA),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetF),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL);
gtk_widget_set_usize(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE);
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void AD811ReadClear(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<8;++A)
    {
    //CamacNAF(N,A,0); Val=(gint)DataRead() & 4095;
    sprintf(Str,"(%d %d) ",A,Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
CamacC();
}
//-----------------------------------------------------------------------------------------------------------------------
void AD811Read(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<8;++A)
    {
    //CamacNAF(N,A,0); Val=(gint)DataRead() & 4095;
    sprintf(Str,"(%d %d) ",A,Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void AD811Test(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,0,25);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Test AD811 A=0 F=25\n",-1);
for (A=0;A<8;++A)
    {
    //CamacNAF(N,A,0); Val=(gint)DataRead() & 4095;
    sprintf(Str,"(%d %d) ",A,Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void AD811DisableLAM(GtkWidget *W,gpointer Unused)
{
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,12,24);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Disable LAM A=12 F=24\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void AD811EnableLAM(GtkWidget *W,gpointer Unused)
{
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,12,26);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Enable LAM A=12 F=26\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void AD811Reset(GtkWidget *W,gpointer Unused)
{
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,12,11);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Reset A=12 F=11\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void OrtecAD811(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47);
gtk_window_set_title(GTK_WINDOW(Win),"Ortec AD811");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"Ortec AD811 ADC Test in Crate");
Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(20,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->Stn),52,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Reset ADC"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD811Reset),NULL);
But=gtk_button_new_with_label("Enable LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD811EnableLAM),NULL);
But=gtk_button_new_with_label("Disable LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD811DisableLAM),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Self Test"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD811Test),NULL);
But=gtk_button_new_with_label("Read ADC"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD811Read),NULL);
But=gtk_button_new_with_label("Read+Clear"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD811ReadClear),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6);
gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_usize(GTK_WIDGET(CamTest->InputData),50,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetA),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetF),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL);
gtk_widget_set_usize(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE);
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM60ReadClear(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<4;++A)
    {
    //CamacNAF(N,A,2); Val=(gint)DataRead() & 4095;
    sprintf(Str,"%d ",Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
CamacC();
}
//-----------------------------------------------------------------------------------------------------------------------
void CM60Read(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<4;++A)
    {
    //CamacNAF(N,A,2); Val=(gint)DataRead() & 4095;
    sprintf(Str,"%d ",Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM60Clear(GtkWidget *W,gpointer Unused)
{
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,12,9);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Clear Module A=12 F=9\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM60DisableLAM(GtkWidget *W,gpointer Unused)
{
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,12,24);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Disable LAM A=12 F=24\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM60EnableLAM(GtkWidget *W,gpointer Unused)
{
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,12,26);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Enable LAM A=12 F=26\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM60Reset(GtkWidget *W,gpointer Unused)
{
gint N;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
//CamacNAF(N,12,11);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Reset A=12 F=11\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM60SetLLD(GtkWidget *W,gpointer Unused)
{
gint N,A;
gulong Data;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
Data=atol(gtk_entry_get_text(GTK_ENTRY(CamTest->SetLLD)));
//DataWrite(Data); for (A=0;A<4;++A) CamacNAF(N,A,17);
sprintf(Str,"Set LLD=%ld for Ch 0,1,2,3\n",Data);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void BarcCM60(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"BARC CM60");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"BARC CM-60 ADC Test in Crate");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(3,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->Stn),52,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
CamTest->SetLLD=gtk_entry_new_with_max_length(3);
gtk_entry_set_text(GTK_ENTRY(CamTest->SetLLD),"50");
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetLLD),110,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetLLD,FALSE,FALSE,0);
But=gtk_button_new_with_label("Set LLD"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM60SetLLD),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Reset ADC"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM60Reset),NULL);
But=gtk_button_new_with_label("Enable LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM60EnableLAM),NULL);
But=gtk_button_new_with_label("Disable LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM60DisableLAM),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Clear Module"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM60Clear),NULL);
But=gtk_button_new_with_label("Read ADC"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM60Read),NULL);
But=gtk_button_new_with_label("CM60 Read+Clear"); 
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM60ReadClear),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); 
gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_usize(GTK_WIDGET(CamTest->InputData),50,22); 
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetA),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE); 
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetF),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window"); 
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_usize(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); 
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88SetLLD(GtkWidget *W,gpointer Unused)
{
gint N,Data;
gboolean XRes,QRes,Lam;
gchar Str[80];
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
Data=atoi(gtk_entry_get_text(GTK_ENTRY(CamTest->SetLLD)));
(void)CamacNAF(Data,N,0,16,&XRes,&QRes,&Lam);
sprintf(Str,"Set LLD=%ld common for all 8 inputs\n",Data);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88SetULD(GtkWidget *W,gpointer Unused)
{
gint N,Data;
gboolean XRes,QRes,Lam;
gchar Str[80];
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
Data=atoi(gtk_entry_get_text(GTK_ENTRY(CamTest->SetULD)));
(void)CamacNAF(Data,N,0,17,&XRes,&QRes,&Lam);
sprintf(Str,"Set ULD=%ld common for all 8 inputs\n",Data);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88DisableLAM(GtkWidget *W,gpointer Unused)
{
gint N;
gboolean XRes,QRes,Lam;
gchar Str[80];
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,12,24,&XRes,&QRes,&Lam);
sprintf(Str,"Disable LAM: A=12 F=24\n");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88EnableLAM(GtkWidget *W,gpointer Unused)
{
gint N;
gboolean XRes,QRes,Lam;
gchar Str[80];
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,12,26,&XRes,&QRes,&Lam);
sprintf(Str,"Enable LAM: A=12 F=26\n");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88ClearLAM(GtkWidget *W,gpointer Unused)
{
gint N;
gboolean XRes,QRes,Lam;
gchar Str[80];
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,12,10,&XRes,&QRes,&Lam);
sprintf(Str,"Clear LAM: A=12 F=10\n");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88SetMode(GtkWidget *W,gpointer Unused)
{
gint N,Data;
gboolean XRes,QRes,Lam;
const gchar *Str;
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(CamTest->SetMode)->entry));
                                                                                                                             
CamacZ();
if (!strcmp(Str,"Coin. No Gates")) 
   { Data=9;  gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Coin. No Gates mode set\n",-1); }
if (!strcmp(Str,"Coin. + Gates")) 
   { Data=25;  gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Coin. + Gates mode set\n",-1); }
if (!strcmp(Str,"Sing. No Gates")) 
   { Data=12;  gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Sing. No Gates mode set\n",-1); }
if (!strcmp(Str,"Sing. + Gates")) 
   { Data=8; gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Sing. + Gates mode set\n",-1); }
(void)CamacNAF(Data,N,0,16,&XRes,&QRes,&Lam);
CamacC();
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88Read(GtkWidget *W,gpointer Unused)
{
gint N,A,Val,Result;
gboolean XRes,QRes,Lam;
gchar Str[80];
                                                                                                                             
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<8;++A)
    {
    Result=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Val=Result & 8191;
    sprintf(Str,"%d ",Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void CM88ReadClear(GtkWidget *W,gpointer Unused)
{
CM88Read(W,Unused);
CamacC();
}
//-----------------------------------------------------------------------------------------------------------------------
void BarcCM88(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];
GList *GList;

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"BARC CM88/CM48");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"BARC CM88/CM48 ADC Test in Crate");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(13,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_widget_set_size_request(GTK_WIDGET(CamTest->Stn),52,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

CamTest->SetMode=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetMode),130,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(CamTest->SetMode)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetMode,FALSE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Coin. No Gates"); GList=g_list_append(GList,"Coin. + Gates");
GList=g_list_append(GList,"Sing. No Gates"); GList=g_list_append(GList,"Sing. + Gates");
gtk_combo_set_popdown_strings(GTK_COMBO(CamTest->SetMode),GList);
But=gtk_button_new_with_label("Set Mode"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88SetMode),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
CamTest->SetLLD=gtk_entry_new_with_max_length(3);
gtk_entry_set_text(GTK_ENTRY(CamTest->SetLLD),"50");
gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetLLD),70,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetLLD,FALSE,FALSE,0);
But=gtk_button_new_with_label("Set LLD"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88SetLLD),NULL);
CamTest->SetULD=gtk_entry_new_with_max_length(3);
gtk_entry_set_text(GTK_ENTRY(CamTest->SetULD),"254");
gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetULD),70,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetULD,FALSE,FALSE,0);
But=gtk_button_new_with_label("Set ULD"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88SetULD),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Enable LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88EnableLAM),NULL);
But=gtk_button_new_with_label("Disable LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88DisableLAM),NULL);
But=gtk_button_new_with_label("Clear LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88ClearLAM),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Read ADC"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88Read),NULL);
But=gtk_button_new_with_label("Read+Clear"); 
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CM88ReadClear),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); 
gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_size_request(GTK_WIDGET(CamTest->InputData),50,-1); 
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetA),40,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE); 
gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetF),40,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window"); 
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_size_request(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); 
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void OtherRead(GtkWidget *W,gpointer Unused)
{
gint N,A,Result;
gchar Str[80];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<4;++A)
    {
    Result=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam);
    sprintf(Str,"%d: %ld masked: %d\n",A,Result,Result & 0xFFFF);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void ProceedExec(GtkWidget *W,gpointer Unused)
{
FILE *Fp;
gchar Str[80],FName[MAX_FNAME_LENGTH],Command[10],Del;
gint N,A,F,Delay,LoopVal,i;
gulong Val,Data;
gushort SVal;
gboolean Display,Loop;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(FName,"%s/%s",FileX->Path,FileX->TargetFile);
g_free(FileX);

if ((Fp=fopen(FName,"r")) == NULL) { sprintf(Str,"File %s not found",FName); Attention(0,Str); return; }
Display=FALSE; Loop=FALSE;
while (TRUE)
  {
  if (fgets(Str,70,Fp)==NULL) break;
  sscanf(Str,"%s",Command);
  if (!strncmp(Command,"LOOP",4)) { Loop=TRUE; sscanf(Str,"%s %c %d",Command,&Del,&LoopVal); }
  if (!strncmp(Command,"DATA",4)) sscanf(Str,"%s %c %ld",Command,&Del,&Data);
  if (!strncmp(Command,"DSP",3)) Display=TRUE;
  if (!strncmp(Command,"NAF",3))
     {
     sscanf(Str,"%s %c %d %d %d",Command,&Del,&N,&A,&F);
     if (!Loop)
        {
        //DataWrite(Data); CamacNAF(N,A,F); Val=DataRead(); SVal=Val & 0xFFFF;
        if (Display)
           {
           sprintf(Str,"Executed NAF: %d %d %d Data=%d (%X)\n",N,A,F,SVal,SVal);
           gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
           Display=FALSE;
           }
        }
     else      //Loop over NAF Command
        {
        for (i=0;i<LoopVal;++i)
            {
            //DataWrite(Data); CamacNAF(N,A,F); Val=DataRead(); SVal=Val & 0xFFFF;
            sprintf(Str,"LoopVal=%d Data=%d (%X)\n",i,SVal,SVal);
            //gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
            g_print("%s",Str);
            }
        Loop=FALSE; Display=FALSE;
        }
     }
  if (!strncmp(Command,"DLY",3))
     {
     sscanf(Str,"%s %c %d",Command,&Del,&Delay);
     usleep(1000*Delay);
     }
  if (!strncmp(Command,"SETI",3)) SetI();
  if (!strncmp(Command,"CLRI",3)) ClrI();
  if (!strncmp(Command,"SETZ",3)) CamacZ();
  if (!strncmp(Command,"CLRC",3)) CamacC();
  }
fclose(Fp);
}
//-----------------------------------------------------------------------------------------------------------------------
void ExecFile(GtkWidget *W,gpointer Unused)
{
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Read CAMAC exec file",NULL,300,TOP_SPACE,TRUE,".",".cam",FALSE,&ProceedExec,FALSE);
}
//-----------------------------------------------------------------------------------------------------------------------
void Other(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

if (!DriverOpen) { Attention(0,"Driver is not open"); return; }
Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"Other Unit");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"'Other' Unit Test in Crate");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(3,1,30,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_widget_set_size_request(GTK_WIDGET(CamTest->Stn),52,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Read"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OtherRead),NULL);
But=gtk_button_new_with_label("Exec File"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecFile),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); 
gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_size_request(GTK_WIDGET(CamTest->InputData),50,-1); 
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetA),40,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE); 
gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetF),40,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window"); 
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_size_request(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); 
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void CAENScalerRead(GtkWidget *W,gpointer Unused)
{
gint N,A,Result,XRes,QRes,Lam;
gulong Val;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<16;++A)
    {
    Result=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Val=Result & 0xFFFFFF;
    sprintf(Str,"%ld",Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    if (A==15) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n--------------------------------------\n",-1);
    else if (!((A+1)%4)) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
    else                 gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,", ",-1);
    }
}
//-----------------------------------------------------------------------------------------------------------------------
void CAENScaler(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"CAEN Scaler");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"CAEN Scaler Test in Crate");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(3,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->Stn),52,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Read"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(CAENScalerRead),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); 
gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_usize(GTK_WIDGET(CamTest->InputData),50,22); 
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetA),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE); 
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetF),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window"); 
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_usize(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); 
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void QuadScalerRead(GtkWidget *W,gpointer Unused)
{
gint N,A;
gulong Val;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<4;++A)
    {
    //CamacNAF(N,A,0); Val=DataRead();
    sprintf(Str,"%ld ",Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    if (A==3) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
    else      gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,", ",-1);
    }
}
//-----------------------------------------------------------------------------------------------------------------------
void QuadScaler(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"Quad Scaler");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"Quad Scaler Test in Crate");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(3,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->Stn),52,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Read"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(QuadScalerRead),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); 
gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_usize(GTK_WIDGET(CamTest->InputData),50,22); 
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetA),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); 
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE); 
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetF),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window"); 
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_usize(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); 
gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                          GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsTest2(GtkWidget *W,gpointer Unused)
{
gint N,A,Data;
gchar Str[100];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
CamacC();
(void)CamacNAF(0,N,2,25,&XRes,&QRes,&Lam);
for (A=0;A<16;++A)
    {
    Data=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Data=Data & 4095;
    sprintf(Str,"(%2d %5d); ",A,Data);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    if (!((A+1)%4)) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,
                "---------------------------------------------------\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsTest1(GtkWidget *W,gpointer Unused)
{
gint N,A,Data;
gchar Str[100];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
CamacC();
(void)CamacNAF(0,N,1,25,&XRes,&QRes,&Lam);
for (A=0;A<16;++A)
    {
    Data=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Data=Data & 4095;
    sprintf(Str,"(%2d %5d); ",A,Data);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    if (!((A+1)%4)) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,
                "---------------------------------------------------\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsReset(GtkWidget *W,gpointer Unused)
{
gint N;
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,3,11,&XRes,&QRes,&Lam);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,
                         "Executed F(11)A(3) Reset Hit Reg. and Data\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsReadHitReg(GtkWidget *W,gpointer Unused)
{
gint N,i;
guint Data;
gchar Str[10];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
Data=CamacNAF(0,N,1,6,&XRes,&QRes,&Lam); Data=Data & 65535;
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Hit Pattern: ",-1);
for (i=0;i<16;++i) 
    { 
    sprintf(Str,"%1d ",(Data>>i) & 1); 
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsClearLAM(GtkWidget *W,gpointer Unused)
{
gint N;
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,0,10,&XRes,&QRes,&Lam);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Executed F(10)A(0) Clear LAM\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsEnableLAM(GtkWidget *W,gpointer Unused)
{
gint N;
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,0,26,&XRes,&QRes,&Lam);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Executed F(26)A(0) Enable LAM\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsWrite(GtkWidget *W,gpointer Unused)
{
gint N,A,Data,Result;
gboolean XRes,QRes,Lam,Fault;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Writing 4095 to all sub-addresses\n",-1);
for (A=0;A<16;++A) (void)CamacNAF(4095L,N,A,16,&XRes,&QRes,&Lam);

Fault=FALSE;
g_print("Starting digital write-read test 0-4095 on all 16 channels. Please be patient...\n");
for (A=0;A<16;++A)
    {
    g_print("A=%d\n",A);
    for (Data=0;Data<4096;++Data)
        {
        (void)CamacNAF(Data,N,A,16,&XRes,&QRes,&Lam);
        Result=CamacNAF(Data,N,A,0,&XRes,&QRes,&Lam);
        if ((Result&4095) != Data) { g_print("Digital test failed %d\n",Result&4095); Fault=TRUE; }
        }
    g_print("\n");
    }
if (!Fault) g_print("All 16 channels have passed the Read-Write test with no problem\n");
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsReadClear(GtkWidget *W,gpointer Unused)
{
gint N,A,Data;
gchar Str[100];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<16;++A)
    {
    Data=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Data=Data & 4095;
    sprintf(Str,"(%2d %5d); ",A,Data);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    if (!((A+1)%4)) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,
                "---------------------------------------------------\n",-1);
(void)CamacNAF(0,N,3,11,&XRes,&QRes,&Lam);                                            //Reset Hit Register
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsRead(GtkWidget *W,gpointer Unused)
{
gint N,A,Data;
gchar Str[100];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<16;++A)
    {
    Data=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Data=Data & 65535;
    sprintf(Str,"(%2d %5d); ",A,Data);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    if (!((A+1)%4)) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,
                "---------------------------------------------------\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsSetupThresholds(GtkWidget *W,gpointer Unused)
{
gint N,A,Data;
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
(void)CamacNAF(0,N,0,17,&XRes,&QRes,&Lam); //Select the Pedestal Memory for the next F20 operation
for (A=0;A<16;++A) (void)CamacNAF(0,N,A,20,&XRes,&QRes,&Lam); //Write Pedestal=0 to all inputs
(void)CamacNAF(0,N,1,17,&XRes,&QRes,&Lam); //Select the Lower Threshold Memory for the next F20 operation
for (A=0;A<16;++A) (void)CamacNAF(10,N,A,20,&XRes,&QRes,&Lam); //Write LT=10 to all inputs
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Set LT=  10 for all inputs\n",-1);
(void)CamacNAF(0,N,2,17,&XRes,&QRes,&Lam); //Select the Upper Threshold Memory for the next F20 operation
for (A=0;A<16;++A) (void)CamacNAF(4090,N,A,20,&XRes,&QRes,&Lam); //Write UT=4090 to all inputs
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Set UT=4090 for all inputs\n",-1);
(void)CamacNAF(7,N,0,19,&XRes,&QRes,&Lam); //Enable LT and UT and Pedestal (2^2+2^1+1=7)
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Enabled LT, UT for all inputs\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void PhillipsSparseRead(GtkWidget *W,gpointer Unused)
{
gint N,A,Result,i,Data,Inp;
gboolean XRes,QRes,Lam;
gchar Str[120];

PhillipsReadHitReg(W,Unused);
N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"CAUTION: Before sparse read, disable Common input\n",-1);

for (i=0;i<17;++i)
   {
   Result=FullCamacNAF(0,N,A,4,&XRes,&QRes,&Lam);
   Data=Result & 0xFFF; Inp=(Result & 0xFFFF) >> 12;
   sprintf(Str,"Data=%4d Inp=%4d Q=%1d\n",Data,Inp,QRes);
   gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
   }
PhillipsReadHitReg(W,Unused);
}
//-----------------------------------------------------------------------------------------------------------------------
void Phillips(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47);
gtk_window_set_title(GTK_WINDOW(Win),"Phillips 71xx");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"Phillips 7164 ADC/7186 TDC/7166 QDC Test in Crate");
Label=gtk_label_new(Str);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(5,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0);
gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Read Data"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsRead),NULL);
But=gtk_button_new_with_label("Mask-Read+Clear"); 
gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsReadClear),NULL);
But=gtk_button_new_with_label("Write Data"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsWrite),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Enable LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsEnableLAM),NULL);
But=gtk_button_new_with_label("Clear LAM"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsClearLAM),NULL);
But=gtk_button_new_with_label("Read Hit Reg"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsReadHitReg),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Reset Hit&Data"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsReset),NULL);
But=gtk_button_new_with_label("Test 1/3"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsTest1),NULL);
But=gtk_button_new_with_label("Test 2/3"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsTest2),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_size_request(GTK_WIDGET(CamTest->InputData),50,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteFullNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("   Setup\nthresholds"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsSetupThresholds),NULL);
But=gtk_button_new_with_label("Sparse\n  Read"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(PhillipsSparseRead),NULL);

Label=gtk_label_new("Output Window"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_size_request(GTK_WIDGET(CamTest->Output),-1,320);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418SetMode(GtkWidget *W,gpointer Unused)
{
gint N;
const gchar *Str;
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(CamTest->SetMode)->entry));

CamacZ(); SetI(); CamacC(); 
if (!strcmp(Str,"LAM Enabled")) 
   { 
   (void)CamacNAF(18944L,N,14,20,&XRes,&QRes,&Lam);
   gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Status Register set to 18944 (2^9 + 2^11 + 2^14)\n",-1);
   }
else
   { 
   (void)CamacNAF(2560L,N,14,20,&XRes,&QRes,&Lam);
   gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Status Register set to 2560 (2^9 + 2^11)\n",-1);
   }
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418VerifyMode(GtkWidget *W,gpointer Unused)
{
gint N,i;
gulong Result;
gchar Str[100];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI();
Result=CamacNAF(0,N,14,4,&XRes,&QRes,&Lam);
if (Result==18944) 
   {
   sprintf(Str,"Status Register=18944 : Lam enabled mode\n");
   gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
   return;
   }
if (Result==2560)  
   {
   sprintf(Str,"Status Register=2560 : No Lam mode\n");
   gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
   return;
   }
sprintf(Str,"Status Register=%ld binary:",Result);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
for (i=0;i<15;++i) 
    { 
    sprintf(Str," %ld",(Result>>i)&1); 
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418SetThreshold(GtkWidget *W,gpointer Unused)
{
gint N;
gulong Data;
gchar Str[80];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI();
Data=atol(gtk_entry_get_text(GTK_ENTRY(CamTest->SetLLD)));
Data=MIN(255,Data);
(void)CamacNAF(Data,N,9,20,&XRes,&QRes,&Lam);
sprintf(Str,"Set Common Threshold=%ld\n",Data);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418VerifyThreshold(GtkWidget *W,gpointer Unused)
{
gint N,Val;
gchar Str[80];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI();
Val=CamacNAF(0,N,9,4,&XRes,&QRes,&Lam); Val=Val & 255;
sprintf(Str,"Found Common Threshold=%d\n",Val);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418SetOffset(GtkWidget *W,gpointer Unused)
{
gint N,A;
gulong Data;
gchar Str[80];
gboolean XRes,QRes,Lam;

sprintf(Str,"Offset=128 is normal, 0 is -3%% full scale, 255 is +3%%\n");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI();
Data=atol(gtk_entry_get_text(GTK_ENTRY(CamTest->SetOffs)));
Data=MIN(255,Data);
for (A=0;A<8;++A) CamacNAF(Data,N,A,20,&XRes,&QRes,&Lam);
sprintf(Str,"Set Ofset=%ld for all inputs\n",Data);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418VerifyOffset(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI();
for (A=0;A<8;++A)
    {
    Val=CamacNAF(0,N,A,4,&XRes,&QRes,&Lam); Val=Val & 255;
    sprintf(Str,"(Offset[%d]=%d)\n",A,Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418ReadADC(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
for (A=0;A<8;++A)
    {
    Val=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Val=Val & 4095;
    sprintf(Str,"(%d %d) ",A,Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418ReadClear(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<8;++A)
    {
    Val=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Val=Val & 4095;
    sprintf(Str,"(%d %d) ",A,Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
CamacC();
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418Test(GtkWidget *W,gpointer Unused)
{
gint N,A,Val;
gchar Str[80];
gboolean XRes,QRes,Lam;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
sprintf(Str,"Remove input gate before testing\n");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
sprintf(Str,"Going to known state...");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
CamacZ(); SetI(); CamacC();
(void)CamacNAF(18944L,N,14,20,&XRes,&QRes,&Lam);
ClrI();
sprintf(Str,"Performing self test F25.A0\n");
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
(void)CamacNAF(0,N,0,25,&XRes,&QRes,&Lam);                                                     //F(25).A(0) Test Function
for (A=0;A<8;++A)
    {
    Val=CamacNAF(0,N,A,0,&XRes,&QRes,&Lam); Val=Val & 4095;
    sprintf(Str,"(%d %d) ",A,Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void Silena4418(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
GList *GList;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"Silina4418 Test");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"Silena 4418/Q Test in Crate");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(6,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE); gtk_widget_set_usize(GTK_WIDGET(CamTest->Stn),52,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
CamTest->SetMode=gtk_combo_new(); gtk_widget_set_usize(GTK_WIDGET(CamTest->SetMode),110,16);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(CamTest->SetMode)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetMode,FALSE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"LAM Enabled"); GList=g_list_append(GList,"No LAM");
gtk_combo_set_popdown_strings(GTK_COMBO(CamTest->SetMode),GList);
But=gtk_button_new_with_label("Set Mode"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418SetMode),NULL);
But=gtk_button_new_with_label("Verify Mode"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418VerifyMode),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
CamTest->SetLLD=gtk_entry_new_with_max_length(3); gtk_entry_set_text(GTK_ENTRY(CamTest->SetLLD),"50");
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetLLD),32,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetLLD,FALSE,FALSE,0);
But=gtk_button_new_with_label("Set Threshold (0-255)"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),140,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418SetThreshold),NULL);
But=gtk_button_new_with_label("Verify Thresh."); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),100,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418VerifyThreshold),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
CamTest->SetOffs=gtk_entry_new_with_max_length(3); gtk_entry_set_text(GTK_ENTRY(CamTest->SetOffs),"128");
gtk_widget_set_usize(GTK_WIDGET(CamTest->SetOffs),32,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetOffs,FALSE,FALSE,0);
But=gtk_button_new_with_label("Set Offset (0-255)"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),140,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418SetOffset),NULL);
But=gtk_button_new_with_label("Verify Offset"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),100,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418VerifyOffset),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Self Test"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),100,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418Test),NULL);
But=gtk_button_new_with_label("Read"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),100,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418ReadADC),NULL);
But=gtk_button_new_with_label("Read+Clear"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),100,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Silena4418ReadClear),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_usize(GTK_WIDGET(CamTest->InputData),50,22); gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE); gtk_widget_set_usize(GTK_WIDGET(CamTest->SetA),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE); gtk_widget_set_usize(GTK_WIDGET(CamTest->SetF),40,22);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_usize(GTK_WIDGET(But),80,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_usize(GTK_WIDGET(CamTest->Output),30,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win); g_list_free(GList);
}
//-----------------------------------------------------------------------------------------------------------------------
void AD413SetMode(GtkWidget *W,gpointer Unused)
{
gint N,Data;
gboolean XRes,QRes,Lam;
const gchar *Str;

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
Str=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(CamTest->SetMode)->entry));

CamacZ(); SetI(); CamacC(); 
if (!strcmp(Str,"Singles+LAM")) Data=62208; else Data=58112;                                    //Singles+LAM or Coinc.+LAM
(void)CamacNAF(Data,N,0,16,&XRes,&QRes,&Lam);                                                          //Control Register 1
if (!strcmp(Str,"Singles+LAM")) Data=16; else Data=0;                                       //Disable or Enable Master Gate
CamacNAF(Data,N,1,16,&XRes,&QRes,&Lam);                                                                //Control Register 2
if (!strcmp(Str,"Singles+LAM")) gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Singles+LAM Mode set\n",-1);
else                            gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"Coinc.+LAM Mode set\n",-1);
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void AD413VerifyMode(GtkWidget *W,gpointer Unused)
{
gint N,Result,Reg1,Reg2;
gboolean XRes,QRes,Lam;
gchar Str[100];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI(); CamacC();
Result=CamacNAF(0,N,0,0,&XRes,&QRes,&Lam); Reg1=Result;
Result=CamacNAF(0,N,1,0,&XRes,&QRes,&Lam); Reg2=Result & 31;
if (Reg1==62208 && Reg2==16) sprintf(Str,"Regs=%d %d Mode: Singles+LAM\n",Reg1,Reg2);
else if (Reg1==58112 && Reg2==0)  sprintf(Str,"Regs=%d %d Mode: Coinc+LAM\n",Reg1,Reg2);
else sprintf(Str,"Regs=%d %d Mode: ?\n",Reg1,Reg2);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void AD413SetLLD(GtkWidget *W,gpointer Unused)
{
gint N,A,Data;
gboolean XRes,QRes,Lam;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI(); CamacC();
Data=atoi(gtk_entry_get_text(GTK_ENTRY(CamTest->SetLLD)));
for (A=0;A<4;++A) (void)CamacNAF(Data,N,A,17,&XRes,&QRes,&Lam);
sprintf(Str,"Set LLD=%ld for Ch 0,1,2,3\n",Data);
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void AD413VerifyLLD(GtkWidget *W,gpointer Unused)
{
gint N,A,Result,Val;
gboolean XRes,QRes,Lam;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
SetI(); CamacC();
for (A=0;A<4;++A)
    {
    Result=CamacNAF(0,N,A,1,&XRes,&QRes,&Lam); Val=Result & 255;
    sprintf(Str,"LLD=%d (%d mV) for Ch %d\n",Val,2*Val,A);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
ClrI();
}
//-----------------------------------------------------------------------------------------------------------------------
void AD413ReadADC(GtkWidget *W,gpointer Unused)
{
gint N,A,Val,Result;
gboolean XRes,QRes,Lam;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn)); 
for (A=0;A<4;++A)
    {
    Result=CamacNAF(0,N,A,2,&XRes,&QRes,&Lam); Val=Result & 8191;
    sprintf(Str,"%d ",Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
}
//-----------------------------------------------------------------------------------------------------------------------
void AD413ReadClear(GtkWidget *W,gpointer Unused)
{
gint N,A,Val,Result;
gboolean XRes,QRes,Lam;
gchar Str[80];

N=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(CamTest->Stn));
for (A=0;A<4;++A)
    {
    Result=CamacNAF(0,N,A,2,&XRes,&QRes,&Lam); Val=Result & 8191;
    sprintf(Str,"%d ",Val);
    gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,Str,-1);
    }
gtk_text_insert(GTK_TEXT(CamTest->Output),NULL,NULL,NULL,"\n",-1);
CamacC();
}
//-----------------------------------------------------------------------------------------------------------------------
void Ad413(GtkWidget *W,gpointer Unused)
{
GtkWidget *Win,*Label,*But,*HBox,*Frame,*VBox;
GtkAdjustment *Adj;
GList *GList;
gint Status;
gchar Str[80];

Win=gtk_dialog_new(); gtk_grab_add(Win);   
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"AD413A Test");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
sprintf(Str,"Ortec AD413A Test in Crate");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Station No."); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(10,1,23,1,5,0);
CamTest->Stn=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->Stn),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->Stn),TRUE); gtk_widget_set_size_request(GTK_WIDGET(CamTest->Stn),52,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->Stn,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
CamTest->SetMode=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetMode),120,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(CamTest->SetMode)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetMode,FALSE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Singles+LAM"); GList=g_list_append(GList,"Coinc.+LAM");
gtk_combo_set_popdown_strings(GTK_COMBO(CamTest->SetMode),GList);
But=gtk_button_new_with_label("Set Mode"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),110,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD413SetMode),NULL);
But=gtk_button_new_with_label("Verify Mode"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),110,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD413VerifyMode),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
CamTest->SetLLD=gtk_entry_new_with_max_length(3); gtk_entry_set_text(GTK_ENTRY(CamTest->SetLLD),"50");
gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetLLD),120,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetLLD,FALSE,FALSE,0);
But=gtk_button_new_with_label("Set LLD"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),110,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD413SetLLD),NULL);
But=gtk_button_new_with_label("Verify LLD"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),110,24);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD413VerifyLLD),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Read ADC"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),100,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD413ReadADC),NULL);
But=gtk_button_new_with_label("Read+Clear"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),100,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AD413ReadClear),NULL);

Frame=gtk_frame_new("NAF Commands"); gtk_frame_set_label_align(GTK_FRAME(Frame),0.5,0.0);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Frame,FALSE,FALSE,5);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Frame),VBox);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
Label=gtk_label_new("Data\n(if any)"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
CamTest->InputData=gtk_entry_new_with_max_length(6); gtk_entry_set_text(GTK_ENTRY(CamTest->InputData),"0");
gtk_widget_set_size_request(GTK_WIDGET(CamTest->InputData),50,-1); gtk_box_pack_start(GTK_BOX(HBox),CamTest->InputData,FALSE,FALSE,0);
Label=gtk_label_new("A="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,15,1,1,0);
CamTest->SetA=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetA),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetA),TRUE); gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetA),40,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetA,FALSE,FALSE,0);
Label=gtk_label_new("F="); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Adj=(GtkAdjustment *)gtk_adjustment_new(0,0,31,1,1,0);
CamTest->SetF=gtk_spin_button_new(Adj,0.5,0); gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(CamTest->SetF),TRUE);
gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(CamTest->SetF),TRUE); gtk_widget_set_size_request(GTK_WIDGET(CamTest->SetF),40,-1);
gtk_box_pack_start(GTK_BOX(HBox),CamTest->SetF,FALSE,FALSE,0);
But=gtk_button_new_with_label("Execute"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteNAF),NULL);

HBox=gtk_hbox_new(FALSE,5); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_with_label("Camac Z"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),80,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteZ),NULL);
But=gtk_button_new_with_label("Camac C"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),80,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteC),NULL);
But=gtk_button_new_with_label("Set I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),80,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteSetI),NULL);
But=gtk_button_new_with_label("Clear I"); gtk_box_pack_start(GTK_BOX(HBox),But,FALSE,FALSE,5);
gtk_widget_set_size_request(GTK_WIDGET(But),80,-1);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ExecuteClrI),NULL);

Label=gtk_label_new("Output Window"); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,FALSE,FALSE,5);
CamTest->Output=gtk_text_new(NULL,NULL); gtk_widget_set_size_request(GTK_WIDGET(CamTest->Output),220,140);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),CamTest->Output,FALSE,FALSE,0);
gtk_text_set_word_wrap(GTK_TEXT(CamTest->Output),TRUE); gtk_text_set_editable(GTK_TEXT(CamTest->Output),FALSE);

But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win); g_list_free(GList);
}
//----------------------------------------------------------------------------------------------------------------------
void EnableAllLams(GtkWidget *Win,gpointer Data)
{
gboolean XRes,QRes,Lam;

if (!DriverOpen) { Attention(100,"Driver is not opened"); return; }
(void)CamacNAF(0,30,10,26,&XRes,&QRes,&Lam);
Attention(100,"Enabled LAM at all Stations");
}
//----------------------------------------------------------------------------------------------------------------------
void DisableAllLams(GtkWidget *Win,gpointer Data)
{
gboolean XRes,QRes,Lam;

if (!DriverOpen) { Attention(100,"Driver is not opened"); return; }
(void)CamacNAF(0,30,10,24,&XRes,&QRes,&Lam);
Attention(100,"Disabled LAM at all Stations");
}
//----------------------------------------------------------------------------------------------------------------------
void ControlLams(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*But,*Label,*VBox;
gchar Str[80];

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_widget_set_uposition(GTK_WIDGET(Win),172,47); gtk_window_set_title(GTK_WINDOW(Win),"Enable/Disable LAMS");
gtk_container_set_border_width(GTK_CONTAINER(Win),14);
VBox=gtk_vbox_new(FALSE,5); gtk_container_add(GTK_CONTAINER(Win),VBox);
sprintf(Str,"Enable/Disable LAMS");
Label=gtk_label_new(Str); gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);
But=gtk_button_new_with_label("Enable All LAMs");
gtk_box_pack_start(GTK_BOX(VBox),But,TRUE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(EnableAllLams),NULL);
g_signal_connect_swapped(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_with_label("Disable All LAMs");
gtk_box_pack_start(GTK_BOX(VBox),But,TRUE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(DisableAllLams),NULL);
g_signal_connect_swapped(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_with_label("Cancel");
gtk_box_pack_start(GTK_BOX(VBox),But,TRUE,FALSE,0);
g_signal_connect_swapped(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//----------------------------------------------------------------------------------------------------------------------
void OpenCheckCmc100(GtkWidget *Win,GtkWidget *Entry)
{
gchar Name[256];
gint X;

X=atoi(gtk_entry_get_text(GTK_ENTRY(Entry)));
sprintf(Name,"/dev/cmcamac%d",X);
Fd=camac_open(Name);
if (Fd<0) Attention(0,"Could not open driver");
else { Attention(100,"Driver opened succesfully"); DriverOpen=TRUE; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void OpenCmc100(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*VBox,*HBox,*But,*Label,*Entry;
                                                                                                                             
if (DriverOpen) { Attention(100,"Driver is already opened"); return; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_widget_set_uposition(GTK_WIDGET(Win),300,47);
gtk_window_set_title(GTK_WINDOW(Win),"Open Cmc100");
gtk_container_set_border_width(GTK_CONTAINER(Win),20);
                                                                                                                             
VBox=gtk_vbox_new(FALSE,10); gtk_container_add(GTK_CONTAINER(Win),VBox);
                                                                                                                             
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,2);
Label=gtk_label_new("Crate ID#"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,0);
Entry=gtk_entry_new_with_max_length(1);
gtk_entry_set_text(GTK_ENTRY(Entry),"0");
gtk_widget_set_size_request(GTK_WIDGET(Entry),20,-1);
gtk_box_pack_start(GTK_BOX(HBox),Entry,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,2);
But=gtk_button_new_with_label("Ok"); gtk_box_pack_start(GTK_BOX(VBox),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(OpenCheckCmc100),Entry);
g_signal_connect_swapped(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
//-----------------------------------------------------------------------------------------------------------------------
void CloseCmc100(GtkWidget *W,gpointer Unused)
{
if (!DriverOpen) Attention(0,"Driver is not open!");
else { camac_close(Fd); DriverOpen=FALSE; Attention(0,"Driver is closed"); }
}
//-----------------------------------------------------------------------------------------------------------------------
void DestroyMain(GtkWidget *Win,gpointer Data)
{
g_free(CamTest);
gtk_main_quit();
}
//-----------------------------------------------------------------------------------------------------------------------
void ResetCmc100(GtkWidget *W,gpointer Unused)
{
if (!DriverOpen) { Attention(0,"Driver is not open!"); return; }
camac_reset(Fd);
Attention(0,"CMC100 has been reset");
}
//-----------------------------------------------------------------------------------------------------------------------
void Cmc100Status(GtkWidget *W,gpointer Unused)
{
char Str[80];
int Status;

if (!DriverOpen) { Attention(0,"Driver is not open!"); return; }
Status=camac_status(Fd);
sprintf(Str,"Unit No=%d S(0=Ok)=%d Lam=%d FIFO ms bits=%d\n",(Status&112)>>4,(Status&8)>>3,(Status&4)>>2,Status&3);
Attention(0,Str);
}
//-----------------------------------------------------------------------------------------------------------------------
void EraseLP(GtkWidget *W,gpointer Unused)
{
gint XData,ProgramLocation,D;

if (!DriverOpen) { Attention(0,"Driver is not open!"); return; }

XData=0xFFFFFFFF;   camac_write(Fd,&XData,4);                      //1st header word
XData=0x00000000;   camac_write(Fd,&XData,4);                      //2nd header word

for (ProgramLocation=0;ProgramLocation<512;++ProgramLocation)
   {
   XData=(3<<24) | ProgramLocation; camac_write(Fd,&XData,4);          //Store next word at ProgramLocation
   XData=(31<<24); camac_write(Fd,&XData,4);                           //Code 31: Quit program mode, go to idle
   }
D=0x0E000000; camac_write(Fd,&D,4);                                                                  //Flush or trailer word
Attention(0,"512 words of LP replaced with NOP");
}
//-----------------------------------------------------------------------------------------------------------------------
int main(int argc,char *argv[])
{
GtkWidget *Win,*VBox,*Label,*But,*HSep,*VBox1,*HBox;
GtkStyle *RedStyle,*GreenStyle,*BlueStyle;
static GdkColor C_Red    = {0,0xFFFF,0x0000,0x0000};
static GdkColor C_Green  = {0,0x0000,0x7777,0x0000};
static GdkColor C_Blue   = {0,0x0000,0x0000,0xAAAA};
gint i;

gtk_init(&argc,&argv);

CamTest=g_new(struct CamTest,1);

RedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) RedStyle->fg[i]=RedStyle->text[i]=C_Red;
GreenStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) GreenStyle->fg[i]=GreenStyle->text[i]=C_Green;
BlueStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) BlueStyle->fg[i]=BlueStyle->text[i]=C_Blue;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(GTK_WIDGET(Win),0,47);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(DestroyMain),NULL);
gtk_window_set_title(GTK_WINDOW(Win),"Cmc100Test");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);

VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);

VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox1,FALSE,FALSE,0); 
Label=gtk_label_new("Cmc100 TEST"); gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
Label=gtk_label_new("Make sure Crate is on\nand driver is installed"); gtk_box_pack_start(GTK_BOX(VBox1),Label,FALSE,FALSE,0);
SetStyleRecursively(Label,BlueStyle);

HSep=gtk_hseparator_new(); gtk_box_pack_start(GTK_BOX(VBox1),HSep,FALSE,FALSE,2);

VBox1=gtk_vbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),VBox1,FALSE,FALSE,10);
But=gtk_button_new_with_label("Open CMC100"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(OpenCmc100),NULL);
But=gtk_button_new_with_label("Self Test"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(SelfTest),NULL);
But=gtk_button_new_with_label("Reset CMC100"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(ResetCmc100),NULL);
But=gtk_button_new_with_label("Erase LP"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(EraseLP),NULL);
But=gtk_button_new_with_label("CMC100 Status"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(Cmc100Status),NULL);
But=gtk_button_new_with_label("Enable/Disable LAMs"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(ControlLams),NULL);
But=gtk_button_new_with_label("Ortec AD413 ADC"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(Ad413),NULL);
But=gtk_button_new_with_label("Silena 4418 ADC/QDC"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(Silena4418),NULL);
But=gtk_button_new_with_label("Phillips 71xx"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(Phillips),NULL);
But=gtk_button_new_with_label("BARC CM60 4K ADC"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(BarcCM60),NULL);
But=gtk_button_new_with_label("CAEN C414 TDC"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("LeCroy 2259 ADC/QDC"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
But=gtk_button_new_with_label("Ortec AD811 ADC"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(OrtecAD811),NULL);
But=gtk_button_new_with_label("CAEN C257 Scaler"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(CAENScaler),NULL);
But=gtk_button_new_with_label("Quad Scaler"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(QuadScaler),NULL);
But=gtk_button_new_with_label("BARC CM88/CM48 ADC"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(BarcCM88),NULL);
But=gtk_button_new_with_label("CM90 Sync Module"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(CM90SyncModule),NULL);
But=gtk_button_new_with_label("Other Unit"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(Other),NULL);
But=gtk_button_new_with_label("Close Cmc100"); gtk_box_pack_start(GTK_BOX(VBox1),But,FALSE,FALSE,0);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(CloseCmc100),NULL);

HSep=gtk_hseparator_new(); gtk_box_pack_start(GTK_BOX(VBox1),HSep,FALSE,FALSE,2);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,2); 
But=gtk_button_new_with_label("Exit"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
SetStyleRecursively(But,GreenStyle);
g_signal_connect(GTK_OBJECT(But),"clicked",G_CALLBACK(DestroyMain),NULL);

gtk_widget_show_all(Win);
gtk_style_unref(RedStyle); gtk_style_unref(GreenStyle); gtk_style_unref(BlueStyle);
gtk_main();
return(0);
}
//-----------------------------------------------------------------------------------------------------------------------
