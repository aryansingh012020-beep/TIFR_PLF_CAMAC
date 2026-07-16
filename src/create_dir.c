#include <gtk/gtk.h>
#include <sys/stat.h>
#include <stdio.h>
#include "lamps.h"

//External globals
extern gchar SetupDir[MAX_DIR_STRLEN],ListFDir[MAX_DIR_STRLEN],BanDir[MAX_DIR_STRLEN],SpecDir[MAX_DIR_STRLEN],
      BatDir[MAX_DIR_STRLEN],CalDir[MAX_DIR_STRLEN],LogDir[MAX_DIR_STRLEN],MacroDir[MAX_DIR_STRLEN];         //Dir prefs
//----------------------------------------------------------------------------------------------------------------------
void CreateDir(void)                             //Create directories as per user preferences if they dont exist already
{
struct stat StatBuf;

if (stat(SetupDir,&StatBuf)) mkdir(SetupDir,0755);
if (stat(ListFDir,&StatBuf)) mkdir(ListFDir,0755);
if (stat(BanDir,&StatBuf))   mkdir(BanDir,0755);
if (stat(SpecDir,&StatBuf))  mkdir(SpecDir,0755);
if (stat(BatDir,&StatBuf))   mkdir(BatDir,0755);
if (stat(CalDir,&StatBuf))   mkdir(CalDir,0755);
if (stat(LogDir,&StatBuf))   mkdir(LogDir,0755);
if (stat(MacroDir,&StatBuf)) mkdir(MacroDir,0755);
}
//----------------------------------------------------------------------------------------------------------------------
