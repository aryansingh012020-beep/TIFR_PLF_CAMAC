#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lamps.h"

/*Function templates*/
void Err(gchar *Messg);
void ParseTextToInt(gchar *TextBuf,gint From,gint n,gint *Val,gint *ToHere);
void ParseTextToFileName(gchar *TextBuf,gint From,gchar *OutText,gint *ToHere); 
void ParseTextToStr(gchar *TextBuf,gint From,gchar *OutText,gint *ToHere);
/*---------------------------------------------------------------------------------------------------------------------*/
void ParseTextToInt(gchar *TextBuf,gint From,gint n,gint *Val,gint *ToHere)
/*Extracts n integer values from TextBuf to Val                            *
  From marks the starting position                                         *
  Parsing stops at on encountering either \n or \0                         *
  ToHere marks the next position after parsing n values and skipping \n    *
  When we reach the end of TextBuf (i.e \0) we return ToHere=-1            */
{
gint i,j,k;
gchar c,Str[20];

while ( (TextBuf[From]==' ') || (TextBuf[From]=='\n') ) From++;             /*Skip over initial blanks or \n*/
for (i=0;i<n;i++) Val[i]=0; Str[18]=Str[19]='\0'; *ToHere=From;             /*Initialise*/
for (i=0,k=From;i<n;i++)
    {
    while (TextBuf[k] == ' ') k++;                                          /*Skip over blanks*/
    for (j=0;j<18;j++)       
        {
        c=TextBuf[k+j]; 
        if ( (c=='\n') || (c=='\0') || (c==' ') ) 
           { Str[j]='\0'; Val[i]=atoi(Str); k=k+j+1; *ToHere=k; break;} 
        else Str[j]=c;
        }
    if (j==18) { Err("Typing errors in Text Box!"); return; } 
    if ( (c=='\n') || (c=='\0') ) break; 
    }
}
/*---------------------------------------------------------------------------------------------------------------------*/
void ParseTextToFileName(gchar *TextBuf,gint From,gchar *OutText,gint *ToHere)
{
gint i;

while ( (TextBuf[From]==' ') || (TextBuf[From]=='\n') ) From++;                        /*Skip over initial blanks or \n*/
for (i=0;i<MAX_FNAME_LENGTH;i++) OutText[i]='\0'; *ToHere=From;                                            /*Initialise*/
while (TextBuf[From] != ' ')                                                               /*Locate 1st blank character*/
      { From++; if ( (TextBuf[From]=='\n') || (TextBuf[From]=='\0') ) return; }
while (TextBuf[From]==' ') From++;                                                          /*Skip over any more blanks*/
for (i=0;i<MAX_FNAME_LENGTH-1;i++,From++)                                            /*Now copy characters into OutText*/
    {
    if ( (TextBuf[From] == ' ') || (TextBuf[From] == '\n') || (TextBuf[From] == '\0') ) break;
    OutText[i]=TextBuf[From];
    }
OutText[i+1]='\0'; *ToHere=From+1; return;
}
/*---------------------------------------------------------------------------------------------------------------------*/
void ParseTextToStr(gchar *TextBuf,gint From,gchar *OutText,gint *ToHere)
{
gint i;

while ( (TextBuf[From]==' ') || (TextBuf[From]=='\n') ) From++;                        /*Skip over initial blanks or \n*/
for (i=0;i<LONG_TEXT_FIELD;i++) OutText[i]='\0'; *ToHere=From;                                             /*Initialise*/
for (i=0;i<LONG_TEXT_FIELD-1;i++,From++)                                             /*Now copy characters into OutText*/
    {
    if ( (TextBuf[From] == ' ') || (TextBuf[From] == '\n') || (TextBuf[From] == '\0') ) break;
    OutText[i]=TextBuf[From];
    }
OutText[i+1]='\0'; *ToHere=From+1; return;
}
/*---------------------------------------------------------------------------------------------------------------------*/
