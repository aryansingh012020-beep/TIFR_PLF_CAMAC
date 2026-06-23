//To compile type: gcc -Wall zmass.c useful.c -o zmass `gtk-config --cflags` `gtk-config --libs`

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/*CAUTION: For (unlikely) changes in lamps.h we need changes here too*/
#define MAX_FNAME_LENGTH  255                              //Max no of characters for file names (with full absolute path)
#define MAX_DIR_ENTRIES  5000                                      //FileX Widget: maximum number of files/sub-directories
#define MAX_DIR_STRLEN    255                           //FileX Widget: maximum string length for file and directory names
#define MAX_MASK_SIZE       5                                              //FileX Widget: maximum length of the file mask
#define TOP_SPACE         129                                                      //This space holds the main_menu widget

/*Global Definitions*/
struct Zmass {
             GtkWidget *WdSz; GtkWidget *Opt; GtkWidget *RelG; GtkWidget *Gsf; GtkWidget *Pow; GtkWidget *Psf; 
             GtkWidget *FBut[20]; GtkWidget *OBut[20]; GtkWidget *Status[20]; gint Row;
             gchar FNames[20][MAX_FNAME_LENGTH+10]; GtkWidget *ScrlW;
             };
struct Zmass *Zmass;
struct FileSelectType  {                                                                 //For the FileSelect widget FileX
                       GtkWidget *But; GtkWidget *FEntry; GtkWidget *PEntry; GtkWidget *MEntry; GtkWidget *Win;
                       GtkWidget *Label1; GtkWidget *Label2;
                       gint N; gint Index; gint Files;
                       gchar Mask[MAX_MASK_SIZE]; gchar Path[MAX_DIR_STRLEN]; gchar Names[MAX_DIR_ENTRIES][MAX_DIR_STRLEN];
                       gchar TargetFile[MAX_DIR_STRLEN];
                       };
struct FileSelectType *FileX;

/*Function templates*/
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void InitMat16(gshort XMax,gshort YMax,guint16 *Data);
void InitMat32(gshort XMax,gshort YMax,guint32 *Data);
void Readz2d(gint wsize,gshort xsize,gshort ysize,FILE *iFp,guint16 *Data16,guint32 *Data32,gboolean *Error);
void Writez2d16(gshort XMax,gshort YMax,FILE *oFp,guint16 *Data,gboolean *Error);
void Writez2d32(gshort XMax,gshort YMax,FILE *oFp,guint32 *Data,gboolean *Error);
void Attention(gint XPos,gchar *Messg);
void FileOpenNew(gchar *Title,GtkWidget *Parent,gint X,gint Y,gboolean OpenToRead,gchar *StartPath,gchar *Mask,
                 gboolean MaskEditable,void (*CallBack)(GtkWidget *,gpointer),gboolean Persist);
/*----------------------------------------------------------------------------------------------------------------------*/
void DestroyMain(GtkWidget *Win,gpointer Data)
{ 
g_free(Zmass);
gtk_main_quit(); 
}
/*----------------------------------------------------------------------------------------------------------------------*/
void GainMatch16(gshort XMax,gshort YMax,guint16 *Data,gdouble RelGain,gdouble GScale)
{
guint16 *Temp;
gushort index;
guint Row,Col;
gdouble norm;

Temp=g_new(guint16,XMax);

norm=(float)XMax/((float)XMax + RelGain*(float)YMax);
for (Row=0;Row<YMax;Row++)
    {
    for (Col=0;Col<XMax;Col++) Temp[Col]=0;
    for (Col=0;Col<XMax;Col++)
        {
        index=(unsigned short)((((float)Col+RelGain*(float)Row)*GScale*norm)+0.5);
        Temp[index]+=Data[Row*XMax+Col];
        }
    for (Col=0;Col<XMax;Col++) Data[Row*XMax+Col]=Temp[Col];
   }
g_free(Temp);
return;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void GainMatch32(gshort XMax,gshort YMax,guint32 *Data,gdouble RelGain,gdouble GScale)
{
gushort index;
guint32 *Temp;
guint Row,Col;
gfloat norm;

Temp=g_new(guint32,XMax);

norm=(float)XMax/((float)XMax + RelGain*(float)YMax);
for (Row=0;Row<YMax;Row++)
    {
    for (Col=0;Col<XMax;Col++) Temp[Col]=0;
    for (Col=0;Col<XMax;Col++)
        {
        index = (unsigned short)((((float)Col+RelGain*(float)Row)*GScale*norm)+0.5);
        Temp[index] += Data[Row*XMax+Col];
        }
    for (Col=0;Col<XMax;Col++) Data[Row*XMax+Col]=Temp[Col];
   }
g_free(Temp);
return;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PI16(gshort XMax,gshort YMax,guint16 *Data,gdouble RelGain,gdouble GScale,gdouble PIPower,gdouble PIScale)
{
gushort xindex,yindex;
guint Row,Col;
gdouble xnorm,ynorm,Etemp,Et,Ediff;  /*--Variables used for temporary manipulation --*/
guint16 *Temp;

Temp=g_new(guint16,XMax*YMax);

InitMat16(XMax,YMax,Temp);
Etemp=(double)XMax + RelGain*(double)YMax;
xnorm=(double)XMax/Etemp;
ynorm=(double)YMax/(pow(Etemp,(double)PIPower)-pow((double)XMax,(double)PIPower));

for (Row=0;Row<YMax;Row++)
    {
    for (Col=0;Col<XMax;Col++)
        {
        xindex = (unsigned short)((((float)Col+RelGain*(float)Row)*GScale*xnorm)+0.5);
        Et = (double)Col+RelGain*(double)Row;
        Ediff = pow(Et,(double)PIPower)-pow((double)Col,(double)PIPower);
        yindex = (unsigned short)((Ediff*PIScale*ynorm)+0.5);
        if ((xindex < XMax) && (yindex < YMax)) Temp[yindex*XMax+xindex] += Data[Row*XMax+Col];
        }
    }
for (Row=0;Row<YMax;Row++) for (Col=0;Col<XMax;Col++) Data[Row*XMax+Col]=Temp[Row*XMax+Col];
g_free(Temp);
return;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void PI32(gshort XMax,gshort YMax,guint32 *Data,gdouble RelGain,gdouble GScale,gdouble PIPower,gdouble PIScale)
{
gushort xindex,yindex;
guint Row,Col;
gdouble xnorm,ynorm,Etemp,Et,Ediff;  /*--Variables used for temporary manipulation --*/
guint32 *Temp;

Temp=g_new(guint32,XMax*YMax);

InitMat32(XMax,YMax,Temp);
Etemp=(double)XMax + RelGain*(double)YMax;
xnorm=(double)XMax/Etemp;
ynorm=(double)YMax/(pow(Etemp,(double)PIPower)-pow((double)XMax,(double)PIPower));

for (Row=0;Row<YMax;Row++)
    {
    for (Col=0;Col<XMax;Col++)
        {
        xindex = (unsigned short)((((float)Col+RelGain*(float)Row)*GScale*xnorm)+0.5);
        Et = (double)Col+RelGain*(double)Row;
        Ediff = pow(Et,(double)PIPower)-pow((double)Col,(double)PIPower);
        yindex = (unsigned short)((Ediff*PIScale*ynorm)+0.5);
        if ((xindex < XMax) && (yindex < YMax)) Temp[yindex*XMax+xindex] += Data[Row*XMax+Col];
        }
    }
for (Row=0;Row<YMax;Row++)
    {
    for (Col=0;Col<XMax;Col++) Data[Row*XMax+Col]=Temp[Row*XMax+Col];
   }
g_free(Temp);
return;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Proceed(GtkWidget *W,gpointer Unused)
{
gint i,WSize;
gshort XSize,YSize;
const gchar *WdSz,*Opt,*OutFName;
gdouble RelGain,GScale,PIPower,PIScale;
FILE *IFp,*OFp;
guint16 *Data16;
guint32 *Data32;
gboolean Error;

WdSz=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Zmass->WdSz)->entry));
Opt=gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(Zmass->Opt)->entry));
RelGain=atof(gtk_entry_get_text(GTK_ENTRY(Zmass->RelG)));
GScale=atof(gtk_entry_get_text(GTK_ENTRY(Zmass->Gsf)));
PIPower=atof(gtk_entry_get_text(GTK_ENTRY(Zmass->Pow)));
PIScale=atof(gtk_entry_get_text(GTK_ENTRY(Zmass->Psf)));

for (i=0;i<20;++i)
    {
    OutFName=gtk_entry_get_text(GTK_ENTRY(Zmass->OBut[i]));
    if ((IFp=fopen(Zmass->FNames[i],"r"))==NULL) 
       { 
       if (strlen(Zmass->FNames[i])==0) gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"");
       else  gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"Failed"); 
       continue; 
       }
    fread(&XSize,sizeof(gshort),1,IFp); fread(&YSize,sizeof(gshort),1,IFp);
    if (WdSz[0]=='S') { WSize=1; Data16=g_new(guint16,XSize*YSize); InitMat16(XSize,YSize,Data16); }
    else              { WSize=2; Data32=g_new(guint32,XSize*YSize); InitMat32(XSize,YSize,Data32); }
    Readz2d(WSize,XSize,YSize,IFp,Data16,Data32,&Error); 
    fclose(IFp);
    if (Error) { gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"Failed"); continue; }
    if ((OFp=fopen(OutFName,"w"))==NULL) { gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"Failed"); continue; }
    if (WSize==1)
       {
       if (!strncmp(Opt,"Make dE",7)) GainMatch16(XSize,YSize,Data16,RelGain,GScale);
       else PI16(XSize,YSize,Data16,RelGain,GScale,PIPower,PIScale);
       Writez2d16(XSize,YSize,OFp,Data16,&Error);
       g_free(Data16);
       }
    else
       {
       if (!strncmp(Opt,"Make dE",7)) GainMatch32(XSize,YSize,Data32,RelGain,GScale);
       else PI32(XSize,YSize,Data32,RelGain,GScale,PIPower,PIScale);
       Writez2d32(XSize,YSize,OFp,Data32,&Error);
       g_free(Data32);
       }
    fclose(OFp);
    if (Error) { gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"Failed"); continue; }
    gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"Done");
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void ClearNames(GtkWidget *W,gpointer Unused)
{
gint i;

for (i=0;i<20;i++)
    {
    strcpy(Zmass->FNames[i],"");
    gtk_label_set_text(GTK_LABEL(GTK_BIN(Zmass->FBut[i])->child),"Browse");
    gtk_entry_set_text(GTK_ENTRY(Zmass->OBut[i]),"");
    gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"");
    }
Zmass->Row=0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Select(GtkWidget *W,gpointer Unused)
{
gchar Str[MAX_FNAME_LENGTH+5];
gint i,L,ScrollTo;

if (strlen(FileX->Path)+strlen(FileX->TargetFile)+1>MAX_FNAME_LENGTH)
   { Attention(0,"ERROR: MAX_FNAME_LENGTH exceeded"); return; }
sprintf(Zmass->FNames[Zmass->Row],"%s/%s",FileX->Path,FileX->TargetFile);                               //Store full path

AbbreviateFileName(Str,Zmass->FNames[Zmass->Row],MAX_FNAME_LENGTH);
gtk_label_set_text(GTK_LABEL(GTK_BIN(Zmass->FBut[Zmass->Row])->child),Str);
L=strlen(Str);
if (L>6)
   {
   for (i=L;i>L-8;--i) Str[i+1]=Str[i];
   Str[L-7]='g';
   }
else strcpy(Str,"out.z2d");
gtk_entry_set_text(GTK_ENTRY(Zmass->OBut[Zmass->Row]),Str);
gtk_entry_set_text(GTK_ENTRY(Zmass->Status[Zmass->Row]),"");
if (Zmass->Row<19) 
   {
   Zmass->Row++;
   ScrollTo=0; if (Zmass->Row>8) ScrollTo=15.0*Zmass->Row;                                           //Fudging to scroll
   gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_scrolled_window_get_vadjustment
                                                                          (GTK_SCROLLED_WINDOW(Zmass->ScrlW))),ScrollTo);
   }
else Zmass->Row=0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void BrowseTwodFile(GtkWidget *W,gpointer Data)
{
Zmass->Row=GPOINTER_TO_INT(Data);
FileX=g_new(struct FileSelectType,1);
FileOpenNew("Select 2d Files",NULL,424,TOP_SPACE,TRUE,".",".z2d",FALSE,&Select,TRUE);
}
/*----------------------------------------------------------------------------------------------------------------------*/
int main(int argc,char *argv[])
{
static gchar *Titles[4]= {"No","Input File","Output File","Status"};
gint ColWidth[4]={30,130,130,50};
static GdkColor TitlesBg  = {0,0x7777,0x0000,0x0000};
static GdkColor TitlesFg  = {0,0xFFFF,0xFFFF,0xFFFF};
GtkStyle *TitlesStyle;
GtkWidget *Win,*Label,*VBox,*HBox,*Table,*But;
GList *GList;
gint i;
gchar Str[20];
    
gtk_init(&argc,&argv);
    
Zmass=g_new(struct Zmass,1);                                       //Create new structure to be destroyed in DestroyMain()
TitlesStyle=gtk_style_copy(gtk_widget_get_default_style());                             //Copy default style to this style
for (i=0;i<5;i++) { TitlesStyle->fg[i]=TitlesStyle->text[i]=TitlesFg; TitlesStyle->bg[i]=TitlesBg; }

Win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_title(GTK_WINDOW(Win),"Gain match/PI");
gtk_widget_set_uposition(GTK_WIDGET(Win),10,TOP_SPACE);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(DestroyMain),NULL);
VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);
gtk_container_set_border_width(GTK_CONTAINER(VBox),4);
Label=gtk_label_new("Gain Match PI Transformation");
gtk_box_pack_start(GTK_BOX(VBox),Label,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Zmass->WdSz=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Zmass->WdSz),144,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Zmass->WdSz)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Zmass->WdSz,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Double Word (32-bit)"); GList=g_list_append(GList,"Single Word (16-bit)");
gtk_combo_set_popdown_strings(GTK_COMBO(Zmass->WdSz),GList);

Zmass->Opt=gtk_combo_new(); gtk_widget_set_size_request(GTK_WIDGET(Zmass->Opt),144,-1);
gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(Zmass->Opt)->entry),FALSE);
gtk_box_pack_start(GTK_BOX(HBox),Zmass->Opt,TRUE,FALSE,0); GList=NULL;
GList=g_list_append(GList,"Make dE vs. E-total"); GList=g_list_append(GList,"Make PI vs E-total");
gtk_combo_set_popdown_strings(GTK_COMBO(Zmass->Opt),GList);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Relative Gain:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,4);
Zmass->RelG=gtk_entry_new_with_max_length(10); gtk_box_pack_start(GTK_BOX(HBox),Zmass->RelG,FALSE,FALSE,2);
gtk_widget_set_size_request(GTK_WIDGET(Zmass->RelG),60,-1);
gtk_entry_set_text(GTK_ENTRY(Zmass->RelG),"0.12");

Label=gtk_label_new("Multiply E-total by:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,8);
Zmass->Gsf=gtk_entry_new_with_max_length(10); gtk_box_pack_start(GTK_BOX(HBox),Zmass->Gsf,FALSE,FALSE,2);
gtk_widget_set_size_request(GTK_WIDGET(Zmass->Gsf),60,-1);
gtk_entry_set_text(GTK_ENTRY(Zmass->Gsf),"1.0");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Label=gtk_label_new("Power for PI:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,8);
Zmass->Pow=gtk_entry_new_with_max_length(10); gtk_box_pack_start(GTK_BOX(HBox),Zmass->Pow,FALSE,FALSE,2);
gtk_widget_set_usize(GTK_WIDGET(Zmass->Pow),60,22);
gtk_entry_set_text(GTK_ENTRY(Zmass->Pow),"1.65");

Label=gtk_label_new("Multiply PI by:"); gtk_box_pack_start(GTK_BOX(HBox),Label,FALSE,FALSE,20);
Zmass->Psf=gtk_entry_new_with_max_length(10); gtk_box_pack_start(GTK_BOX(HBox),Zmass->Psf,FALSE,FALSE,2);
gtk_widget_set_size_request(GTK_WIDGET(Zmass->Psf),60,-1);
gtk_entry_set_text(GTK_ENTRY(Zmass->Psf),"1.0");

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Table=gtk_table_new(1,3,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_box_pack_start(GTK_BOX(HBox),Table,FALSE,FALSE,12);
for (i=0;i<4;i++)
    {
    But=gtk_button_new_with_label(Titles[i]); SetStyleRecursively(But,TitlesStyle);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[i],-1);
    gtk_table_attach(GTK_TABLE(Table),But,i,i+1,0,1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
Zmass->ScrlW=gtk_scrolled_window_new(NULL,NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zmass->ScrlW),GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);     //Scroll bars
gtk_widget_set_size_request(GTK_WIDGET(Zmass->ScrlW),375,250);
gtk_box_pack_start(GTK_BOX(HBox),Zmass->ScrlW,TRUE,FALSE,0);

Zmass->Row=0;
Table=gtk_table_new(20,4,FALSE);
gtk_table_set_row_spacings(GTK_TABLE(Table),0); gtk_table_set_col_spacings(GTK_TABLE(Table),2);
gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Zmass->ScrlW),Table);
for (i=0;i<20;i++)
    {
    sprintf(Str,"%d",i+1); But=gtk_button_new_with_label(Str);
    gtk_widget_set_size_request(GTK_WIDGET(But),ColWidth[0],-1);
    gtk_table_attach(GTK_TABLE(Table),But,0,1,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    Zmass->FBut[i]=gtk_button_new_with_label("Browse"); strcpy(Zmass->FNames[i],"");
    gtk_widget_set_size_request(GTK_WIDGET(Zmass->FBut[i]),ColWidth[1],-1);
    gtk_table_attach(GTK_TABLE(Table),Zmass->FBut[i],1,2,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    gtk_signal_connect(GTK_OBJECT(Zmass->FBut[i]),"clicked",GTK_SIGNAL_FUNC(BrowseTwodFile),GINT_TO_POINTER(i));
    Zmass->OBut[i]=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(Zmass->OBut[i]),"");
    gtk_widget_set_size_request(GTK_WIDGET(Zmass->OBut[i]),ColWidth[2],-1);
    gtk_table_attach(GTK_TABLE(Table),Zmass->OBut[i],2,3,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    Zmass->Status[i]=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(Zmass->Status[i]),"");
    gtk_widget_set_size_request(GTK_WIDGET(Zmass->Status[i]),ColWidth[3],-1);
    gtk_table_attach(GTK_TABLE(Table),Zmass->Status[i],3,4,i,i+1,GTK_FILL,GTK_SHRINK,0,0);
    }

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,10);
But=gtk_button_new_with_label("Clear Names"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ClearNames),NULL);
But=gtk_button_new_from_stock(GTK_STOCK_EXECUTE); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(Proceed),NULL);
But=gtk_button_new_from_stock(GTK_STOCK_QUIT); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,TRUE,0);
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(DestroyMain),NULL);

gtk_widget_show_all(Win);
g_list_free(GList); gtk_style_unref(TitlesStyle);
gtk_main();
return(0);
}
/*---------------------------------------------------------------------------------------------------------------------*/
