//This routine generates randomized ADC data. Works for both Normal and Q-Stop CAMAC modes
#include <gtk/gtk.h>
#include <stdlib.h>
#include "lamps.h"
#define RAND ((float)rand()/(float)RAND_MAX) 
//External globals:
extern struct Setup Setup;                      //The setup variables
//Local globals
int Adc[MAX_PAR];

//-----------------------------------------------------------------------------------------------
float GaussRand(int N)                //Returns the mean of N uniform random numbers in range 0-1
{
int i; float X;

X=0; for (i=0;i<N;++i) X+=RAND; return X/N;
}
//-----------------------------------------------------------------------------------------------
void Event()
{
int ModuleNo,Stn,A,Mult,Throw,Na,FillData[16],PNo,i;
float R;

for (ModuleNo=0;ModuleNo<Setup.Decode.Modules;++ModuleNo)
    {
    Stn=Setup.Decode.Module[ModuleNo];
    R=RAND; Mult=Setup.Hardware.NParStn[Stn]*R+0.5; Mult=CLAMP(Mult,1,Setup.Hardware.NParStn[Stn]-1); //g_print("Mult=%d\n",Mult);
    for (Na=0;Na<Setup.Hardware.NParStn[Stn];++Na) FillData[Na]=0;
    for (Throw=0;Throw<Mult;++Throw)  //Throw dice Mult times to decide which A's to fill
        {
        while (TRUE)
           {
           Na=(int)(RAND*Setup.Hardware.NParStn[Stn]-0.5); Na=CLAMP(Na,0,Setup.Hardware.NParStn[Stn]-1);
           if (FillData[Na]==0) { FillData[Na]=1; break; }
           }
        }
    for (Na=0;Na<Setup.Hardware.NParStn[Stn];++Na)
        {
        A=Na+Setup.Hardware.LoSubStn[Stn];
        PNo=Setup.Decode.ParNo[Stn][A];
        //g_print("Na=%d A=%d PNo=%d FillData=%d\n",Na,A,PNo,FillData[Na]);
        if (FillData[Na]) Adc[PNo]=(Setup.Parameter.Chan[PNo]-1)*GaussRand(100);
        else              Adc[PNo]=0;
        }
    }
//for (PNo=0;PNo<Setup.Parameter.NPar;++PNo) g_print("(%d %d) ",PNo+1,Adc[PNo]); g_print("\n");
} 
//-----------------------------------------------------------------------------------------------
void FillBufNormalCamac(gint NEvents,guint *CBuf)
{
int i,j;

for (i=0;i<NEvents;++i)
    {
    Event(); for (j=0;j<Setup.Parameter.NPar;++j) CBuf[i*Setup.Parameter.NPar+j]=Adc[j];
    }
}
//-----------------------------------------------------------------------------------------------
gint FillBufSparseCamac(gint NEvents,guint *CBuf)  //For only Phillips modules presently
{
gint Evt,Ptr,ModuleNo,Stn,PNo,SubA,L,Q,X,i;

Ptr=0;
for (Evt=0;Evt<NEvents;++Evt)
    {
    Event();
    for (ModuleNo=0;ModuleNo<Setup.Decode.Modules;++ModuleNo)
        {
        Stn=Setup.Decode.Module[ModuleNo];
        if (Setup.Hardware.Modules[Stn]==5) //Phillips module
           {
           if (Setup.Hardware.Properties[Stn].AdcLam == Enabled) { L=1; CBuf[Ptr++]=0xCFF0001; } //Phillips start with Lam
           else { L=0; CBuf[Ptr++]=0x8FF0001; }                                               //Phillips start without Lam
           for (PNo=Setup.Hardware.LoParStn[Stn]+Setup.Hardware.NParStn[Stn]-1;PNo>=Setup.Hardware.LoParStn[Stn];--PNo)
               {
               SubA=PNo-Setup.Hardware.LoParStn[Stn]; X=1; Q=1;
               if (Adc[PNo]>0) CBuf[Ptr++]=Adc[PNo] | (SubA<< 12) | (L<<26) | (Q<<25) | (X<<24); //Valid reads in descending order with Q=1
               }
           CBuf[Ptr++]=0x1000FFF;  //The extra read that gives Q=0
           CBuf[Ptr++]=0x8FF0002;  //Phillips End
           }
        }
    if (Setup.Scaler.NSc1)
       {
       CBuf[Ptr++]=0x8FF00F0;   //Scalers Start
       for (i=0;i<Setup.Scaler.NSc1;++i) CBuf[Ptr++]=Evt;  //Put Event number in all the scalers
       CBuf[Ptr++]=0x8FF00F1;   //Scalers End
       }
    CBuf[Ptr++]=0x8FF00F2;   //Clear Start
    CBuf[Ptr++]=0;
    CBuf[Ptr++]=0x8FF00F3;   //Clear End
    }
return Ptr;  //Returns the number of words written to CBuf
}
//-----------------------------------------------------------------------------------------------
