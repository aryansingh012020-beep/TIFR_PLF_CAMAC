#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_DIR_STRLEN 255                                                                  //Must be same as in lamps.h

//Global
gchar LogDir[MAX_DIR_STRLEN];

//Function templates
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void Attention(gint XPos,gchar *Messg);
void UnPad(gchar *Str);

//------------------------------------------------------------------------------------------------------------------------
void MakeWord(GtkWidget *W,gpointer Unused)
{
FILE *Out,*Inp;
gchar Fmt[18][120],Nl[120],Str[120],SetupName[100],Oned[100],Twod[100],User1[100],User2[100],User3[100],
      User4[100],User5[100],User6[100],User7[100],User8[100],User9[100],User10[100],User11[100],User12[100],
      Rem1[100],Rem2[100],Rem3[100],Rem4[100],NameF[MAX_DIR_STRLEN+20];
gint i,SNo,Eof;
glong FPos;

strcpy(Fmt[0],"\\trgaph108\\trleft-108\\trbrdrt\\brdrs\\brdrw10\\trbrdrl\\brdrs\\brdrw10\r\n");
strcpy(Fmt[1],"\\trbrdrb\\brdrs\\brdrw10\\trbrdrr\\brdrs\\brdrw10\\trbrdrh\\brdrs\\brdrw10\r\n");
strcpy(Fmt[2],"\\trbrdrv\\brdrs\\brdrw10\\trftsWidth1\\trautofit1\\trpaddl108\\trpaddr108\r\n");
strcpy(Fmt[3],"\\trpaddfl3\\trpaddfr3\\clvertalt\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\\brdrs\r\n");
strcpy(Fmt[4],"\\brdrw10\\clbrdrb\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10\\cltxlrtb\\clftsWidth1\r\n");
strcpy(Fmt[5],"\\cellx469\\clvertalt\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\\brdrs\\brdrw10\\clbrdrb\r\n");
strcpy(Fmt[6],"\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10\\cltxlrtb\\clftsWidth1\r\n");
strcpy(Fmt[7],"\\cellx1046\\clvertalt\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\\brdrs\\brdrw10\\clbrdrb\r\n");
strcpy(Fmt[8],"\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10\\cltxlrtb\\clftsWidth1\r\n");
strcpy(Fmt[9],"\\cellx1623\\clvertalt\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\\brdrs\\brdrw10\\clbrdrb\r\n");
strcpy(Fmt[10],"\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10\\cltxlrtb\\clftsWidth1\r\n");
strcpy(Fmt[11],"\\cellx2200\\clvertalt\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\\brdrs\\brdrw10\\clbrdrb\r\n");
strcpy(Fmt[12],"\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10\\cltxlrtb\\clftsWidth1\r\n");
strcpy(Fmt[13],"\\cellx2777\\clvertalt\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\\brdrs\\brdrw10\\clbrdrb\r\n");
strcpy(Fmt[14],"\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10\\cltxlrtb\\clftsWidth1\r\n");
strcpy(Fmt[15],"\\cellx3354\\clvertalt\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\r\n");
strcpy(Fmt[16],"\\brdrs\\brdrw10\\clbrdrb\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10\\cltxlrtb\r\n");
strcpy(Fmt[17],"\\clftsWidth1\\cellx3931\r\n");

strcpy(Nl,"\\pard\\ql\\widctlpar\\intbl\\faauto\r\n");

sprintf(NameF,"%s/lampslog.rtf",LogDir); g_print("NameF=%s\n",NameF);
if (!(Out=fopen(NameF,"w"))) { Attention(0,"Could not create lampslog.rtf"); gtk_main_quit(); }
sprintf(NameF,"%s/lamps.log",LogDir); g_print("NameF=%s\n",NameF);
if (!(Inp=fopen(NameF,"r")))    { Attention(0,"There is no file lamps.log"); gtk_main_quit(); }

fprintf(Out,"{\\rtf1\\ansi\\ansicpg1252\\uc1\\deff0\\deflang1033\\deflangfe1033\r\n");
fprintf(Out,"{\\fonttbl{\\f2\\fmodern\\fcharset0\\fprq1\r\n");
fprintf(Out,"{\\*\\panose 02070309020205020404}Courier New;}}\r\n");
fprintf(Out,"{\\stylesheet\r\n");
fprintf(Out,"{\\ql\\li0\\ri0\\widctlpar\\aspalpha\\aspnum\\faauto\\adjustright\\rin0\\lin0\\itap0\r\n");
fprintf(Out,"\\fs20\\lang2057\\langfe1033\\cgrid\\langnp2057\\langfenp1033\\snext0 Normal;}\r\n");
fprintf(Out,"{\\*\\cs10\\additive Default Paragraph Font;}}\r\n");
fprintf(Out,"\\paperw16834\\paperh11909\\margl1440\\margr1440\\margt1797\\margb1797\r\n");
fprintf(Out,"\\trowd\r\n\r\n");

for (i=0;i<18;++i) fprintf(Out,"%s",Fmt[i]);
fprintf(Out,"\r\n{\\f2\r\n"); fprintf(Out,"%s",Nl); fprintf(Out,"SNo\\cell RunName\\par StartDate\\par StartTime\\par\r\n");
fprintf(Out,"Stop\\par ElapsedSec\\par 1dSpec\\par 2dSpec\\cell\r\n");
fprintf(Out,"SetupFile\\par Bufs_Acq.\\par Bufs_Proc.\\par Events\\par Events/s\\par\r\n"); 
fprintf(Out,"DeadTime\\par ListBytes\\cell\r\n");
fprintf(Out,"Scaler1\\par Scaler2\\par Scaler3\\par Scaler4\\cell\r\n");

for (i=0;i<6;++i) { fgets(Str,60,Inp); fprintf(Out,"%s",Str); if (i<5) fprintf(Out,"\\par\r\n"); } fprintf(Out,"\\cell\r\n");
for (i=0;i<6;++i) { fgets(Str,60,Inp); if (i<5) fprintf(Out,"%s\\par\r\n",Str); } fprintf(Out,"\\cell\r\n");
fgets(Str,60,Inp); fprintf(Out,"Remarks\\cell\r\n");
for (SNo=0,Eof=0;;)
    {
    while (1)
       {
       if (!fgets(Str,60,Inp)) { Eof=1; break; }                                                          //EOF reached
       if (!strncmp(Str,"Run Name:",8))  { ++SNo; break; }                                                  //Found run
       }
    if (Eof) break;
    Str[54]='\0'; UnPad(Str);
    fprintf(Out,"%s",Nl);
    fprintf(Out,"\\trowd\r\n\r\n");
    for (i=0;i<18;++i) fprintf(Out,"%s",Fmt[i]);
    fprintf(Out,"\r\n\\row\r\n\\trowd\r\n\r\n");
    for (i=0;i<18;++i) fprintf(Out,"%s",Fmt[i]);
    fprintf(Out,"\r\n%s",Nl);

    fprintf(Out,"%3d\\cell %s\\par\r\n",SNo,&Str[24]);                                                 //SNo and Run Name
    fgets(SetupName,60,Inp); SetupName[54]='\0'; UnPad(SetupName);                                      //Setup File name
    fgets(Str,60,Inp); Str[34]='\0'; Str[43]='\0';
    fprintf(Out,"%s\\par %s\\par\r\n",&Str[24],&Str[35]);                                           //Start Date and Time
    fgets(Oned,60,Inp); fgets(Twod,60,Inp); fgets(User1,60,Inp);
    fgets(User2,60,Inp); fgets(User3,60,Inp); fgets(User4,60,Inp);
    fgets(User5,60,Inp); fgets(User6,60,Inp); fgets(User7,60,Inp);
    fgets(User8,60,Inp); fgets(User9,60,Inp); fgets(User10,60,Inp);
    fgets(User11,60,Inp); fgets(User12,60,Inp); fgets(Rem1,70,Inp);
    fgets(Rem2,70,Inp); fgets(Rem3,70,Inp); fgets(Rem4,70,Inp);
    FPos=ftell(Inp); fgets(Str,60,Inp);
    if (strncmp(Str,"Stop:",4))                                                                                //no stop!
       {
       fprintf(Out,"No Stop!\\par\r\n");                                                                      //Stop Time
       fprintf(Out,"??\\par\r\n");                                                                          //Elapsed Sec
       Oned[54]='\0'; UnPad(Oned); fprintf(Out,"%s\\par\r\n",&Oned[24]);                                        //1d Spec
       Twod[54]='\0'; UnPad(Twod); fprintf(Out,"%s\\cell\r\n",&Twod[24]);                                       //2d Spec
       fprintf(Out,"??\\cell\r\n");                                                                               //Blank
       fprintf(Out,"??\\cell\r\n");                                                                               //Blank
       fseek(Inp,FPos,SEEK_SET);
       }
    else
       {
       Str[43]='\0';
       fprintf(Out,"%s\\par\r\n",&Str[35]);                                                                  //Stop Time
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                   //Elapsed Sec
       Oned[54]='\0'; UnPad(Oned); fprintf(Out,"%s\\par\r\n",&Oned[24]);                                       //1d Spec
       Twod[54]='\0'; UnPad(Twod); fprintf(Out,"%s\\cell\r\n",&Twod[24]);                                      //2d Spec

       fprintf(Out,"%s\\par\r\n",&SetupName[24]);                                                      //Setup File name
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                     //Bufs Acq.
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                    //Bufs Proc.
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                        //Events
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                         //Evt/s
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                     //Dead Time
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\cell\r\n",&Str[24]);                   //List Bytes

       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                     //Scaler 01
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                     //Scaler 02
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\par\r\n",&Str[24]);                     //Scaler 03
       fgets(Str,60,Inp); Str[54]='\0'; UnPad(Str); fprintf(Out,"%s\\cell\r\n",&Str[24]);                    //Scaler 04
       }

    User1[54]='\0'; UnPad(User1); fprintf(Out,"%s\\par\r\n",&User1[24]);                                         //User1
    User2[54]='\0'; UnPad(User2); fprintf(Out,"%s\\par\r\n",&User2[24]);                                         //User2
    User3[54]='\0'; UnPad(User3); fprintf(Out,"%s\\par\r\n",&User3[24]);                                         //User3
    User4[54]='\0'; UnPad(User4); fprintf(Out,"%s\\par\r\n",&User4[24]);                                         //User4
    User5[54]='\0'; UnPad(User5); fprintf(Out,"%s\\par\r\n",&User5[24]);                                         //User5
    User6[54]='\0'; UnPad(User6); fprintf(Out,"%s\\cell\r\n",&User6[24]);                                        //User6

    User7[54]='\0'; UnPad(User7); fprintf(Out,"%s\\par\r\n",&User7[24]);                                         //User7
    User8[54]='\0'; UnPad(User8); fprintf(Out,"%s\\par\r\n",&User8[24]);                                         //User8
    User9[54]='\0'; UnPad(User9); fprintf(Out,"%s\\par\r\n",&User9[24]);                                         //User9
    User10[54]='\0'; UnPad(User10); fprintf(Out,"%s\\par\r\n",&User10[24]);                                     //User10
    User11[54]='\0'; UnPad(User11); fprintf(Out,"%s\\par\r\n",&User11[24]);                                     //User11
    User12[54]='\0'; UnPad(User12); fprintf(Out,"%s\\cell\r\n",&User12[24]);                                    //User12

    Rem1[64]='\0'; UnPad(Rem1); fprintf(Out,"%s\\par\r\n",&Rem1[24]);                                             //Rem1
    Rem2[64]='\0'; UnPad(Rem2); fprintf(Out,"%s\\par\r\n",&Rem2[24]);                                             //Rem2
    Rem3[64]='\0'; UnPad(Rem3); fprintf(Out,"%s\\par\r\n",&Rem3[24]);                                             //Rem3
    Rem4[64]='\0'; UnPad(Rem4); fprintf(Out,"%s\\cell\r\n",&Rem4[24]);                                            //Rem4
    }
fprintf(Out,"%s",Nl); fprintf(Out,"\\trowd\r\n\r\n"); for (i=0;i<18;++i) fprintf(Out,"%s",Fmt[i]);
fprintf(Out,"\r\n\\row\r\n"); fprintf(Out,"%s","\\pard\\ql\\widctlpar\\faauto\r\n"); fprintf(Out,"\\par }}\r\n");
fclose(Out); fclose(Inp);

gtk_main_quit();
}
//----------------------------------------------------------------------------------------------------------------------
int main(int argc,char *argv[])
{
GtkWidget *Win,*VBox,*Label,*But,*HSep,*HBox,*VBox1;
GtkStyle *RedStyle,*GreenStyle,*BlueStyle;
static GdkColor C_Red    = {0,0xFFFF,0x0000,0x0000};
static GdkColor C_Green  = {0,0x0000,0x7777,0x0000};
static GdkColor C_Blue   = {0,0x0000,0x0000,0xAAAA};
gint i;
gchar Skip[125];
FILE *Fp;

strcpy(LogDir,"./");
if ((Fp=fopen(".lamps_prefs","r")))
   {
   for (i=0;i<6;++i) fgets(Skip,120,Fp);
   fscanf(Fp,"%s %s\n",Skip,LogDir);
   fclose(Fp);
   }

gtk_init(&argc,&argv);

RedStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) RedStyle->fg[i]=RedStyle->text[i]=C_Red;
GreenStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) GreenStyle->fg[i]=GreenStyle->text[i]=C_Green;
BlueStyle=gtk_style_copy(gtk_widget_get_default_style());
for (i=0;i<5;i++) BlueStyle->fg[i]=BlueStyle->text[i]=C_Blue;

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_uposition(GTK_WIDGET(Win),415,280);
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_main_quit),GTK_OBJECT(Win));
gtk_window_set_title(GTK_WINDOW(Win),"Make Word File");
gtk_container_set_border_width(GTK_CONTAINER(Win),4);
VBox=gtk_vbox_new(FALSE,0); gtk_container_add(GTK_CONTAINER(Win),VBox);
VBox1=gtk_vbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),VBox1,FALSE,FALSE,0); 
gtk_widget_set_usize(GTK_WIDGET(VBox1),300,88);
Label=gtk_label_new("File lampslog.rtf will be created"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,RedStyle);
Label=gtk_label_new("Open with MS Word under Windows"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,GreenStyle);
Label=gtk_label_new("Or OpenOffice.org under Linux"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,BlueStyle);
Label=gtk_label_new("You will need to resize the columns\nby dragging the column separators"); 
gtk_box_pack_start(GTK_BOX(VBox1),Label,TRUE,FALSE,0);
SetStyleRecursively(Label,BlueStyle);

HSep=gtk_hseparator_new(); gtk_box_pack_start(GTK_BOX(VBox1),HSep,TRUE,FALSE,2);
HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,5);
But=gtk_button_new_from_stock(GTK_STOCK_OK); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(MakeWord),NULL); 
But=gtk_button_new_from_stock(GTK_STOCK_CANCEL); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_main_quit),NULL); 
gtk_widget_show_all(Win);
gtk_style_unref(RedStyle); gtk_style_unref(GreenStyle); gtk_style_unref(BlueStyle);
gtk_main();
return(0);
}
//----------------------------------------------------------------------------------------------------------------------
