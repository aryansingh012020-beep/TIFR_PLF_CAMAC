#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>

/*Function templates*/
void Attention(gint XPos,gchar *Messg);
void AtWin(gint X,gint Y,gint XSiz,gint YSiz,gchar *Messg);
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen);
void SetStyleRecursively(GtkWidget *W,gpointer Data);
void InitMat16(gshort XMax,gshort YMax,guint16 *Data);
void InitMat32(gshort XMax,gshort YMax,guint32 *Data);
void Readz2d(gint wsize,gshort xsize,gshort ysize,FILE *iFp,guint16 *Data16,guint32 *Data32,gboolean *Error);
void Writez2d16(gshort XMax,gshort YMax,FILE *oFp,guint16 *Data,gboolean *Error);
void Writez2d32(gshort XMax,gshort YMax,FILE *oFp,guint32 *Data,gboolean *Error);

//These global variables defined here are used by the FileOpen widget in open.c
GtkStyle *FolderStyle,*FileStyle;
/*----------------------------------------------------------------------------------------------------------------------*/
void InitializeFileColours(void)                             //The styles for the file widget open.c are set permanently
{
static GdkColor Red       = {0,0xDDDD,0x0000,0x0000};
static GdkColor Yellow    = {0,0xFFFF,0xFFFF,0xBBBB};
static GdkColor GrYellow  = {0,0xDDDD,0xDDDD,0x7777};
static GdkColor White     = {0,0xFFFF,0xFFFF,0xFFFF};
static GdkColor Black     = {0,0x0000,0x0000,0x0000};
static GdkColor Gray      = {0,0xDDDD,0xDDDD,0xDDDD};

FolderStyle=gtk_style_copy(gtk_widget_get_default_style());
FolderStyle->fg[0]=FolderStyle->text[0]=Black; FolderStyle->bg[0]=White;                                  //Normal
FolderStyle->fg[1]=FolderStyle->text[1]=Black; FolderStyle->bg[1]=Gray;                             //Button press
FolderStyle->fg[2]=FolderStyle->text[2]=Black; FolderStyle->bg[2]=Gray;                                    //Focus
FileStyle=gtk_style_copy(gtk_widget_get_default_style());
FileStyle->fg[0]=FileStyle->text[0]=Red; FileStyle->bg[0]=Yellow;                                          //Normal
FileStyle->fg[1]=FileStyle->text[1]=Red; FileStyle->bg[1]=GrYellow;                                  //Button press
FileStyle->fg[2]=FileStyle->text[2]=Red; FileStyle->bg[2]=GrYellow;                                         //Focus
}
/*---------------------------------------------------------------------------------------------------------------------*/
void Attention(gint XPos,gchar *Messg)          //Displays message in a modal window. XPos=horiz. shift wrt screen centre
{
GtkWidget *Win,*Label,*But;

Win=gtk_dialog_new(); gtk_grab_add(Win); gtk_window_set_title(GTK_WINDOW(Win),"Attention");
gtk_widget_set_uposition(GTK_WIDGET(Win),415+XPos,280);
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_DIALOG_INFO); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),But,TRUE,FALSE,5);
Label=gtk_label_new(Messg); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,TRUE,FALSE,5);
But=gtk_button_new_from_stock(GTK_STOCK_OK);
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void AtWin(gint X,gint Y,gint XSiz,gint YSiz,gchar *Messg)                          //A more flexible verison of Attention
{
GtkWidget *Win,*Label,*But;

Win=gtk_dialog_new(); gtk_grab_add(Win); gtk_window_set_title(GTK_WINDOW(Win),"Attention");
gtk_widget_set_uposition(GTK_WIDGET(Win),X,Y); 
gtk_signal_connect_object(GTK_OBJECT(Win),"delete_event",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
But=gtk_button_new_from_stock(GTK_STOCK_DIALOG_INFO); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),But,TRUE,FALSE,5);
Label=gtk_label_new(Messg); gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->vbox),Label,TRUE,FALSE,5);
But=gtk_button_new_with_label("Dismiss");
gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Win)->action_area),But,TRUE,FALSE,0);
gtk_signal_connect_object(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(Win));
gtk_widget_show_all(Win);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void AbbreviateFileName(gchar *Dest,gchar *Src,gint MaxLen)     //Remove path name. If reqd. truncate and put ~ at the end
{
gint L,i;
gchar *Str;

Str=rindex(Src,(int)'/');
if (Str==NULL) { strncpy(Dest,Src,MaxLen); Dest[MaxLen-1]='~'; Dest[MaxLen]='\0'; return; }    //Path character not found!
L=strlen(Str);
if (L <= MaxLen+1) { strcpy(Dest,&Str[1]); return; }
for (i=0;i<MaxLen-1;i++) Dest[i]=Str[i+1];
Dest[MaxLen-1]='~'; Dest[MaxLen]='\0';
}
/*----------------------------------------------------------------------------------------------------------------------*/
void SetStyleRecursively(GtkWidget *W,gpointer Data)
{
GtkStyle *Style;

Style=(GtkStyle *)Data; gtk_widget_set_style(W,Style);
if (GTK_IS_CONTAINER(W)) gtk_container_foreach(GTK_CONTAINER(W),SetStyleRecursively,Style);
}
/*----------------------------------------------------------------------------------------------------------------------*/
void UnPad(gchar *Str)
{
gint i;

for (i=strlen(Str)-1;i>0;--i) { if (Str[i] != ' ') break; else Str[i]='\0'; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitMat16(gshort XMax,gshort YMax,guint16 *Data)
{
guint X;
                                                                                                                             
for (X=0;X<(XMax*YMax);++X) *(Data+X)=0;
return;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void InitMat32(gshort XMax,gshort YMax,guint32 *Data)
{
guint X;
                                                                                                                             
for (X=0;X<(XMax*YMax);++X) *(Data+X)=0;
return;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Readz2d(gint wsize,gshort xsize,gshort ysize,FILE *iFp,guint16 *Data16,guint32 *Data32,gboolean *Error)
{
gushort NZero,NData;
guint P1,P2,Y,ChansRead;
                                                                                                                             
*Error=FALSE;
for (Y=0,P2=0;Y<ysize;Y++)
    {
    P1=0;
    do
      {
      fread(&NZero,2,1,iFp); fread(&NData,2,1,iFp); P1+=NZero;
      if (wsize == 1) ChansRead=fread(&Data16[0]+P2+P1,2,NData,iFp);
      else            ChansRead=fread(&Data32[0]+P2+P1,4,NData,iFp);
      if (ChansRead<NData) { *Error=TRUE; return; }
      P1+=NData;
      } while (P1<xsize);
    P2+=xsize;
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Writez2d16(gshort XMax,gshort YMax,FILE *oFp,guint16 *Data,gboolean *Error)
{
guint P1,P2,Row;
gushort NZero,NData;
                                                                                                                             
*Error=FALSE;
if (fwrite(&XMax,2,1,oFp) < 1) { *Error=TRUE; return; }
if (fwrite(&YMax,2,1,oFp) < 1) { *Error=TRUE; return; }
for (Row=0;Row<YMax;Row++)
    {
    P1=0;
    while (P1<XMax)
       {
       P2=P1;
       while ( (!Data[XMax*Row+P2]) && (P2<XMax) ) P2++; NZero=P2-P1;
       if (fwrite(&NZero,2,1,oFp) < 1) { *Error=TRUE; return; };//Output no of zeroes
       P1=P2;
       while ( (Data[XMax*Row+P2]) && (P2<XMax) ) P2++; NData=P2-P1;
       if (fwrite(&NData,2,1,oFp) < 1) { *Error=TRUE; return; }; //Output no of non-zero data
       if (NData>0) if (fwrite(&Data[XMax*Row+P1],2,NData,oFp) < NData) { *Error=TRUE; return;}
       P1=P2;
       }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void Writez2d32(gshort XMax,gshort YMax,FILE *oFp,guint32 *Data,gboolean *Error)
{
guint P1,P2,Row;
gushort NZero,NData;
                                                                                                                             
*Error=FALSE;
if (fwrite(&XMax,2,1,oFp) < 1) { *Error=TRUE; return; }
if (fwrite(&YMax,2,1,oFp) < 1) { *Error=TRUE; return; }
for (Row=0;Row<YMax;Row++)
    {
    P1=0;
    while (P1<XMax)
       {
       P2=P1;
       while ( (!Data[XMax*Row+P2]) && (P2<XMax) ) P2++; NZero=P2-P1;
       if (fwrite(&NZero,2,1,oFp) < 1) { *Error=TRUE; return; }   //Output no of zeroes
       P1=P2;
       while ( (Data[XMax*Row+P2]) && (P2<XMax) ) P2++; NData=P2-P1;
       if (fwrite(&NData,2,1,oFp) < 1) { *Error=TRUE; return; } //Output no of non-zero data
       if (NData>0) if (fwrite(&Data[XMax*Row+P1],4,NData,oFp) < NData)
                         { *Error=TRUE; return; }
       P1=P2;
       }
    }
}
/*----------------------------------------------------------------------------------------------------------------------*/
