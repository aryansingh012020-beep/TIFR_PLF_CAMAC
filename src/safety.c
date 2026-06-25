/* Safety: dumps spectra files peroidically. Periodic Log: periodically writes info in periodic.log*/
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include "lamps.h"

/*External global variables*/
extern struct Setup Setup;                                                                             //The setup variables
extern gulong ScalerCurr[MAX_SCALER];                                                //Scaler values from the current buffer
extern gchar SpecDir[MAX_DIR_STRLEN],LogDir[MAX_DIR_STRLEN];

/*Function Templates*/
void Write1d(gchar *FName,gint i,gboolean InThread);
void Write2d32(gchar *FName,gint i,gboolean InThread);
void Write2d16(gchar *FName,gint i,gboolean InThread);
void Attention(gint XPos,gchar *Messg);
/*------------------------------------------------------------------------------------------------------------------------*/
void Safety(gchar *RunName,gdouble *ElapsedTime,gint *SpecSaved)
{
gint SpecN,i;
gchar FNam[MAX_FNAME_LENGTH];

if (Setup.Spectra.Safety)
   {
   SpecN=(gint)(*ElapsedTime/60.0)/Setup.Spectra.SafetyTime;
   if (SpecN>*SpecSaved)
      {
      for (i=0;i<Setup.Oned.N;i++)
          { sprintf(FNam,"%s/Spec_%s/%s%03d.z1d",SpecDir,RunName,RunName,i+1); Write1d(FNam,i,1); }
      if (Setup.Twod.WSz==1)
         for (i=0;i<Setup.Twod.N;++i)
             { sprintf(FNam,"%s/Spec_%s/%s%03d.z2d",SpecDir,RunName,RunName,i+1); Write2d16(FNam,i,1); }
      else
         for (i=0;i<Setup.Twod.N;++i)
             { sprintf(FNam,"%s/Spec_%s/%s%03d.z2d",SpecDir,RunName,RunName,i+1); Write2d32(FNam,i,1); }
      *SpecSaved=SpecN;
      }
   }
}
/*-------------------------------------------------------------------------------------------------------------------------*/
void PeriodicLog(gint Status,gchar *RunName,glong BuffersAcquired,glong BuffersProcessed,
                 glong BytesWritten,time_t TStart,gdouble StartTime)
{
FILE *Fp;
gchar Str[120],FNam[MAX_FNAME_LENGTH];
struct tm *TmNow;
time_t TNow;
gint i;
struct timeval Tv;
gdouble ElapsedSec,TimeNow;

sprintf(FNam,"%s/periodic.log",LogDir);
if ((Fp=fopen(FNam,"a")) == NULL)
   {
   sprintf(Str,"nice -n 10 crash \"Couldnt open periodic.log\" \"Could be a file permissions problem\"&");
   system(Str); exit(1);
   }
gettimeofday(&Tv,NULL); TimeNow=(double)Tv.tv_sec+(double)Tv.tv_usec*1.0e-06; ElapsedSec=TimeNow-StartTime;
switch (Status)
{
case 0: //Initial entries
        fprintf(Fp,"%s Started at: %s",RunName,ctime(&TStart));
        break;
case 1: //Periodic entries
        fprintf(Fp,"%s, %8ld bufs(%8ld), %8.2f sec, ",RunName,BuffersAcquired,BuffersProcessed,ElapsedSec);
        TNow=time(NULL); TmNow=localtime(&TNow);
        fprintf(Fp,"%10ld bytes, %02d:%02d:%02d\n",BytesWritten,TmNow->tm_hour,TmNow->tm_min,TmNow->tm_sec);
        for (i=0;i<Setup.Scaler.NSc;++i) fprintf(Fp,"%s: %ld\n",Setup.Scaler.Name[i],ScalerCurr[i]);
        break;
case 2: //Final entries
        fprintf(Fp,"Stopped: %8ld bufs(%8ld), %8.1f sec, ",BuffersAcquired,BuffersProcessed,ElapsedSec);
        TNow=time(NULL); TmNow=localtime(&TNow);
        fprintf(Fp,"%10ld bytes, %02d:%02d:%02d\n",BytesWritten,TmNow->tm_hour,TmNow->tm_min,TmNow->tm_sec);
        for (i=0;i<Setup.Scaler.NSc;++i) fprintf(Fp,"%s: %ld\n",Setup.Scaler.Name[i],ScalerCurr[i]);
        break;
case 3: //Pause
        TNow=time(NULL);
        fprintf(Fp,"%s Paused at: %s",RunName,ctime(&TNow));
        break;
case 4: //Resume
        TNow=time(NULL);
        fprintf(Fp,"%s Resumed at: %s",RunName,ctime(&TNow));
}
fprintf(Fp,"---------------------------------------------------------------------------------------------\n");
fclose(Fp);
}
/*-------------------------------------------------------------------------------------------------------------------------*/
