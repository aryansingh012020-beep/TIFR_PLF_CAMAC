/*feedback.c :: Allows the user to add his comments, bug reports etc.*/
#define GTK_ENABLE_BROKEN
#include <gtk/gtk.h>
#include <stdio.h>
#include <time.h>

enum FeedBackStatus {None,OldComments,NewComment};
/*Global in this file*/
enum FeedBackStatus FeedBackStatus;

/*------------------------------------------------------------------------------------------------------------------------*/
void FeedBackDestroyed(GtkWidget *W,gpointer Win)
{
gtk_grab_remove(GTK_WIDGET(Win));                                                         /*Take away the modal property*/
gtk_widget_destroy(GTK_WIDGET(Win));                                                                    /*Destroy window*/
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void ReadComments(GtkWidget *W,GtkWidget *Text)
{
FILE *File;
gchar Buffer[1024];
gint NChars;

FeedBackStatus=OldComments;
gtk_text_set_editable(GTK_TEXT(Text),FALSE);
gtk_text_set_point(GTK_TEXT(Text),0); 
gtk_text_forward_delete(GTK_TEXT(Text),gtk_text_get_length(GTK_TEXT(Text)));
gtk_text_freeze(GTK_TEXT(Text));
File=fopen("user_comments.txt","r");
if (File)
   {
   while (1)
     {
     NChars=fread(Buffer,1,1024,File);
     gtk_text_insert(GTK_TEXT(Text),NULL,NULL,NULL,Buffer,NChars);
     if (NChars<1024) break;
     }
   fclose(File);
   }
gtk_text_thaw(GTK_TEXT(Text));
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void SaveComment(GtkWidget *W,GtkWidget *Text)
{
FILE *File;
gchar *TextBuf;

if (FeedBackStatus==NewComment)
   {
   File=fopen("user_comments.txt","a");
   TextBuf=gtk_editable_get_chars(GTK_EDITABLE(Text),0,-1);
   fprintf(File,"%s\n",TextBuf);
   fclose(File);
   g_free(TextBuf);
   FeedBackStatus=None;
   }
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void AddComment(GtkWidget *W,GtkWidget *Text)
{
static GdkColor Red =  {0,0xffff,0x0000,0x0000};
static GdkColor Blue = {0,0x0000,0x0000,0xffff};
gchar Str[40];
time_t Time;

gtk_text_set_editable(GTK_TEXT(Text),TRUE);
gtk_text_set_point(GTK_TEXT(Text),0); 
gtk_text_forward_delete(GTK_TEXT(Text),gtk_text_get_length(GTK_TEXT(Text)));
gtk_text_insert(GTK_TEXT(Text),NULL,&Text->style->black,NULL,"------------------------------------------------\n",-1);
Time=time(NULL); sprintf(Str,"Date: %s",ctime(&Time));
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,Str,-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"Comment submitted by",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Text->style->black,NULL,": \n",-1);
FeedBackStatus=NewComment;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void FeedBack(GtkWidget *W,gpointer Data)
{
GtkWidget *Win,*HBox,*VBox,*But,*Text,*SBar;
static GdkColor Red   = {0,0xffff,0x0000,0x0000};
static GdkColor Blue  = {0,0x0000,0x0000,0xffff};
static GdkColor Green = {0,0x0000,0x7777,0x0000};
static GdkColor Black = {0,0x0000,0x0000,0x0000};

FeedBackStatus=None;
Win=gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_grab_add(Win);
gtk_signal_connect(GTK_OBJECT(Win),"destroy",GTK_SIGNAL_FUNC(FeedBackDestroyed),Win);
gtk_window_set_title(GTK_WINDOW(Win),"User Feedback");

VBox=gtk_vbox_new(FALSE,4); gtk_container_add(GTK_CONTAINER(Win),VBox);

HBox=gtk_hbox_new(FALSE,0); gtk_box_pack_start(GTK_BOX(VBox),HBox,TRUE,TRUE,0);
Text=gtk_text_new(NULL,NULL); gtk_widget_set_usize(GTK_WIDGET(Text),400,150);
gtk_text_set_editable(GTK_TEXT(Text),TRUE); gtk_text_set_word_wrap(GTK_TEXT(Text),TRUE);
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,"Contact information updated 02 Dec 2008\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,"To add a comment click ",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"Add New Comment\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,"To read previous comments click ",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"Read Comments\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Red,NULL,"Outstation users can send user_comments.txt to ",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Blue,NULL,"DrAmbar@gmail.com\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Black,NULL,"For urgent problems ring A.Chatterjee:\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Green,NULL,"TIFR:22782449, BARC:25593881, Res:25503094 Cell: 9920467733\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Black,NULL,"K.Ramachandran:\n",-1);
gtk_text_insert(GTK_TEXT(Text),NULL,&Green,NULL,"TIFR: 22782464, BARC: 25593881, Res: 22804569/22783144",-1);
gtk_box_pack_start(GTK_BOX(HBox),Text,FALSE,FALSE,0);
SBar=gtk_vscrollbar_new(GTK_TEXT(Text)->vadj);
gtk_box_pack_start(GTK_BOX(HBox),SBar,FALSE,FALSE,0);

HBox=gtk_hbox_new(FALSE,4); gtk_box_pack_start(GTK_BOX(VBox),HBox,FALSE,FALSE,0);
But=gtk_button_new_with_label("Add New \nComment"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(AddComment),Text); 
But=gtk_button_new_with_label("Save \nComment"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(SaveComment),Text); 
But=gtk_button_new_with_label("Read \nComments"); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(ReadComments),Text); 
But=gtk_button_new_from_stock(GTK_STOCK_QUIT); gtk_box_pack_start(GTK_BOX(HBox),But,TRUE,FALSE,0); 
gtk_signal_connect(GTK_OBJECT(But),"clicked",GTK_SIGNAL_FUNC(FeedBackDestroyed),Win); 
gtk_widget_show_all(Win);                                                             /*Show all the widgets in the window*/
}
/*------------------------------------------------------------------------------------------------------------------------*/
