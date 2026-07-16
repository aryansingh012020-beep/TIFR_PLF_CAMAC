#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "lamps.h"

#define MAX_PARAS    500
#define CONSTR       6            //The "parameter distance" for constrained parameters (=no. of paras per peak=6)
#define MATINV_ERROR 1            //Diagonal element is zero
#define MATINV_OKAY  0            //Normal
#define PAR_STEP     0.0001       //For derivative calculation
#define FRAC_CHISQ   0.01         //Relative difference in Chisq
#define ABS_CHISQ    0.01         //Absolute difference in Chisq
#define MIN_CHISQ    1.0e-05      //Stop if ChisqDF goes below this
#define TIME_OUT     30           //Stop after 30 sec
#define DISP_STEP    1            //Update step if used

/*Function Templates*/
gdouble BiGauss(gint i,gdouble *P,gdouble *X);
void Refresh(GtkWidget *W,gpointer Data);

/*External global variables*/
extern struct FitType Fit;
/*----------------------------------------------------------------------------------------------------------------------------------------*/
gint MatInv(gint N,gdouble *C)
{
gdouble Q,T;
gint i,j,k;
for (i=0;i<N;i++)
    {
    if ((C[i*N+i])==0.0L) return MATINV_ERROR;
    Q=1.0/C[i*N+i];
    for (j=0;j<N;j++) C[i*N+j]*=Q;
    for (j=0;j<N;j++) if (j==i) C[i*N+j]=Q; else { T=C[j*N+i]; for (k=0;k<N;k++) C[j*N+k]-=C[i*N+k]*T; C[j*N+i]=-Q*T; }
    }
return MATINV_OKAY;
}
//-----------------------------------------------------------------------------------------------------------------------------------------
gint DoFit(gint NPts,gint NPara,gdouble (*Func)(),gint Option,gdouble *X,gdouble *Y,gdouble *P,gdouble *Err,enum FitFlags *Flag,
           gchar *Messg)
//This function is made quite general so that it can be used both for peak fit and calibration and any other fit
//Only thing is that for Constrained parameters the "distance" is set as CONSTR #defined above. This works for CONSTR parameters per peak
//Returns 0=normal, 1=Error. N.B. Option=0 (error=sqrt(Y)), Option=1 (uniform errors)
//If IterLabel is not NULL, intermediate results are displayed in IterLabel
{
gint i,NVary,j,Iter,Finished,k,Temp,h,ii,jj;
gdouble p[MAX_PARAS],PrevChisq,CurrChisq,S,C,A,Q,Hh,Rd[MAX_PARAS],D[MAX_PARAS],Qd[MAX_PARAS],DChi,Cd[MAX_PARAS*MAX_PARAS],ChisqDf,ErrSq;
time_t TStart,TElapsed;

for (i=0,NVary=0;i<NPara;i++) if (Flag[i]==0) NVary++;
if (NPts<=NVary) { strcpy(Messg,"ERROR: Too few points"); return 1; }
for (i=0,j=0;i<NPara;i++)
    {
    if (Flag[i]==0) { p[j]=P[i]; j++; }                                                                                      //Variable
    if (Flag[i]==2) P[i]=P[i-CONSTR];                         //Constrained. Here its assumed that there are CONSTR parameters per peak
    }
PrevChisq=1.0e25; Iter=0; Finished=0; TStart=time(NULL); TElapsed=0;
while (TElapsed<TIME_OUT)                                                                               //Start Chisq minimization loop
   {
   Iter++; CurrChisq=0.0;
   for (k=0;k<NVary;k++)
       {
       Rd[k]=0.0; Temp=k*NVary;
       for (h=0;h<=k;h++) Cd[Temp+h]=0.0;
       }
   for (i=0;i<NPts;i++)
       {
       if (Option) ErrSq=1.0; else ErrSq=Y[i]+1.0;
       for (ii=0,jj=0;ii<NPara;ii++)
           {
           if (Flag[ii]==0) { P[ii]=p[jj]; jj++; }                                                                            //Variable
           if (Flag[ii]==2) P[ii]=P[ii-CONSTR];                                                                            //Constrained
           }
       S=(*Func)(i,P,X);
       C=Y[i]-S;
       CurrChisq+=C*C/ErrSq;
       if (CurrChisq>1.0e+10) { strcpy(Messg,"ERROR: chisq overflow"); return 1; }
       A=S;
       for (k=0;k<NVary;k++)
           {
           Temp=k*NVary; Q=p[k];
           if (Q!=0.0L) Hh=0.0; else Hh=1.0;
           p[k]=Q+(Q+Hh)*PAR_STEP;
           for (ii=0,jj=0;ii<NPara;ii++)
               {
               if (Flag[ii]==0) { P[ii]=p[jj]; jj++; }                                                                        //Variable
               if (Flag[ii]==2) P[ii]=P[ii-CONSTR];                                                                        //Constrained
               }
           S=(*Func)(i,P,X); p[k]=Q; D[k]=(S-A)/PAR_STEP/(Q+Hh); Rd[k]+=C*D[k]/ErrSq;
           for (h=0;h<=k;h++) Cd[Temp+h]+=D[k]*D[h]/ErrSq;
           }
       }
   ChisqDf=CurrChisq/(NPts-NVary); DChi=fabs(CurrChisq-PrevChisq);
   /*if (!(Iter%DISP_STEP)) g_print("Iter# %d, Chisq/DF= %8g P[0]=%f P[1]=%f TElapsed=%ld\n",Iter,ChisqDf,P[0],P[1],TElapsed);*/
   if ( (DChi<FRAC_CHISQ*CurrChisq) || (DChi<ABS_CHISQ) || (ChisqDf<MIN_CHISQ) ) Finished=1;
   if ( (CurrChisq>PrevChisq) && !Finished ) for (k=0;k<NVary;k++) { Qd[k]=0.50*Qd[k]; p[k]-=Qd[k];}
   else
      {
      for (k=0;k<NVary;k++) { Temp=k*NVary; for (h=k+1;h<NVary;h++) Cd[Temp+h]=Cd[h*NVary+k]; }
      if (MatInv(NVary,Cd)==MATINV_ERROR) { strcpy(Messg,"Fit exact or failed?"); return 1; }
      if (Finished)
         {
         sprintf(Messg,"Iter# %d, Chisq/DF= %8g",Iter,ChisqDf);
         for (ii=0,jj=0;ii<NPara;ii++)
             if (Flag[ii]==0) { Err[ii]=sqrt(fabs(ChisqDf*Cd[jj*NVary+jj])); jj++; }                      //Errors for Variable paras only
             else  Err[ii]=0.0;
         return 0;
         }
      for (k=0;k<NVary;k++)
          {
          Qd[k]=0.0; Temp=k*NVary;
          for (h=0;h<NVary;h++) Qd[k]+=Cd[Temp+h]*Rd[h];
          }
      for (k=0;k<NVary;k++) p[k]+=Qd[k];
      PrevChisq=CurrChisq;
      }
   TElapsed=time(NULL)-TStart;
   }
strcpy(Messg,"Time out!"); return 1;
}
//------------------------------------------------------------------------------------------------------------------------
void FinishedFit(void)
{
gint i,j;
GList *Node;
gchar Str[40];

Fit.Busy=FALSE;
gdk_threads_enter();
Refresh(NULL,GINT_TO_POINTER(Fit.WinNo));

Node=g_list_last(GTK_TABLE(Fit.Table1)->children);                                                             //First member of the GList
Node=g_list_previous(Node); Node=g_list_previous(Node); Node=g_list_previous(Node);                               //Skip past the headings
sprintf(Str,"%.3f",Fit.P[0]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                               //BkgL
Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                     //Skip 2 nodes
sprintf(Str,"%.3f",Fit.P[1]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                               //BkgR
Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                     //Skip 2 nodes
sprintf(Str,"%.3f",Fit.P[2]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                           //Bkg Quad
    
Node=g_list_last(GTK_TABLE(Fit.Table2)->children);                                                             //First member of the GList
for (i=0;i<Fit.NPeaks;i++)
    {
    Node=g_list_previous(Node);                                                                                    //Skip to the next node
    j=6*i;
    sprintf(Str,"%.3f",Fit.P[j+3]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                         //Posn
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                 //Skip 2 nodes
    sprintf(Str,"%.3f",Fit.P[j+4]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                         //FWHM
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                 //Skip 2 nodes
    sprintf(Str,"%.3f",Fit.P[j+5]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                         //Asym
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                 //Skip 2 nodes
    sprintf(Str,"%.3f",Fit.P[j+6]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                        //LTail
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                 //Skip 2 nodes
    sprintf(Str,"%.3f",Fit.P[j+7]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                        //RTail
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                 //Skip 2 nodes
    sprintf(Str,"%.3f",Fit.P[j+8]); gtk_entry_set_text(GTK_ENTRY(((GtkTableChild *)Node->data)->widget),Str);                       //Height
    Node=g_list_previous(Node); Node=g_list_previous(Node);                                                                 //Skip 2 nodes
    }
gdk_threads_leave();
}
//------------------------------------------------------------------------------------------------------------------------
void *FitThread(gpointer IterLabel)
//This function is for peak fit only
//For Constrained parameters the "distance" is set as CONSTR #defined above. This works for CONSTR parameters per peak
// May 2008. Modification of Chisq expression for Low Statistics introduced. 
//              Normal: Chisq=Sum[(y_i-fit_i)^2/max(y_i,1)]            (Modified Neyman's Criterion)
//      Low Statistics: Chisq=Sum[(y_i+min(y_i,1)-fit_i)^2/(y_i+1)]    (Mighel, Astrophysical Jour. 518 (1999) 380
{
gint i,NVary,j,Iter,Finished,k,Temp,h,ii,jj,NPts,NPara;
gdouble p[MAX_PARAS],PrevChisq,CurrChisq,S,C,A,Q,Hh,Rd[MAX_PARAS],D[MAX_PARAS],Qd[MAX_PARAS],DChi,Cd[MAX_PARAS*MAX_PARAS],ChisqDf,ErrSq;
time_t TStart,TElapsed;
gchar Str[40];

NPts=Fit.NPts; NPara=6*Fit.NPeaks+3; 

for (i=0,NVary=0;i<NPara;i++) if (Fit.F[i]==0) NVary++; 
if (NPts<=NVary) { strcpy(Fit.Messg,"ERROR: Too few points"); FinishedFit(); pthread_exit(NULL); }
for (i=0,j=0;i<NPara;i++)
    {
    if (Fit.F[i]==0) { p[j]=Fit.P[i]; j++; }                                                                                     //Variable
    if (Fit.F[i]==2) Fit.P[i]=Fit.P[i-CONSTR];                    //Constrained. Here its assumed that there are CONSTR parameters per peak
    }
PrevChisq=1.0e25; Iter=0; Finished=0; TStart=time(NULL); TElapsed=0;
while (TElapsed<TIME_OUT)                                                                               //Start Chisq minimization loop
   {
   Iter++; CurrChisq=0.0;
   for (k=0;k<NVary;k++)
       {
       Rd[k]=0.0; Temp=k*NVary;
       for (h=0;h<=k;h++) Cd[Temp+h]=0.0;
       }
   for (i=0;i<NPts;i++)
       {
       if (Fit.StatsOpt==Normal) { if (Fit.Data[i]) ErrSq=Fit.Data[i]; else ErrSq=1.0; }  //Neyman modified Chisq for Poisson statistics
       else                      ErrSq=Fit.Data[i]+1.0;                    //Mighell's proposal for low statistics [Ap J 518 (1999) 380]
       for (ii=0,jj=0;ii<NPara;ii++)
           {
           if (Fit.F[ii]==0) { Fit.P[ii]=p[jj]; jj++; }                                                                       //Variable
           if (Fit.F[ii]==2) Fit.P[ii]=Fit.P[ii-CONSTR];                                                                   //Constrained
           }
       S=BiGauss(i,Fit.P,NULL); 
       C=Fit.Data[i]-S; if ((Fit.StatsOpt==LowStat) && (Fit.Data[i]==0)) C=C+1.0;                //Mighell's proposal for low statistics
       CurrChisq+=C*C/ErrSq; 
       if (CurrChisq>1.0e+10) { strcpy(Fit.Messg,"ERROR: chisq overflow"); FinishedFit(); pthread_exit(NULL); }
       A=S;
       for (k=0;k<NVary;k++)
           {
           Temp=k*NVary; Q=p[k];
           if (Q!=0.0L) Hh=0.0; else Hh=1.0;
           p[k]=Q+(Q+Hh)*PAR_STEP;
           for (ii=0,jj=0;ii<NPara;ii++)
               {
               if (Fit.F[ii]==0) { Fit.P[ii]=p[jj]; jj++; }                                                                   //Variable
               if (Fit.F[ii]==2) Fit.P[ii]=Fit.P[ii-CONSTR];                                                               //Constrained
               }
           S=BiGauss(i,Fit.P,NULL); p[k]=Q; D[k]=(S-A)/PAR_STEP/(Q+Hh); Rd[k]+=C*D[k]/ErrSq;
           for (h=0;h<=k;h++) Cd[Temp+h]+=D[k]*D[h]/ErrSq;
           }
       }
   ChisqDf=CurrChisq/(NPts-NVary); DChi=fabs(CurrChisq-PrevChisq);
   if (!(Iter%DISP_STEP)) 
      {
      sprintf(Str,"Iter# %d, Chisq/DF= %8g Elapsed Time=%ld\n",Iter,ChisqDf,TElapsed);
      gdk_threads_enter();
      gtk_label_set_text(GTK_LABEL(IterLabel),Str);
      gdk_threads_leave();
      }
   if ( (DChi<FRAC_CHISQ*CurrChisq) || (DChi<ABS_CHISQ) || (ChisqDf<MIN_CHISQ) ) Finished=1;
   if ( (CurrChisq>PrevChisq) && !Finished ) for (k=0;k<NVary;k++) { Qd[k]=0.50*Qd[k]; p[k]-=Qd[k];}
   else
      {
      for (k=0;k<NVary;k++) { Temp=k*NVary; for (h=k+1;h<NVary;h++) Cd[Temp+h]=Cd[h*NVary+k]; }
      if (MatInv(NVary,Cd)==MATINV_ERROR) { strcpy(Fit.Messg,"Fit exact or failed?"); FinishedFit(); pthread_exit(NULL); }
      if (Finished)
         {
         sprintf(Fit.Messg,"Iter# %d, Chisq/DF= %8g",Iter,ChisqDf); 
         for (ii=0,jj=0;ii<NPara;ii++)
             if (Fit.F[ii]==0) { Fit.Err[ii]=sqrt(fabs(ChisqDf*Cd[jj*NVary+jj])); jj++; }                 //Errors for Variable paras only
             else  Fit.Err[ii]=0.0;
         FinishedFit(); pthread_exit(NULL);
         }
      for (k=0;k<NVary;k++)
          {
          Qd[k]=0.0; Temp=k*NVary;
          for (h=0;h<NVary;h++) Qd[k]+=Cd[Temp+h]*Rd[h];
          }
      for (k=0;k<NVary;k++) p[k]+=Qd[k];
      PrevChisq=CurrChisq;
      }
   TElapsed=time(NULL)-TStart;
   }
strcpy(Fit.Messg,"Time out!"); FinishedFit(); pthread_exit(NULL);
return NULL;
}
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
