//Read, Write and Skip routines for zls and gls format files
#include <string.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "lamps.h"

/*External globals*/
extern struct Setup Setup;
extern enum ReWriteRequest ReWriteRequest;
/*Function prototypes*/
void SAttention(gchar *Messg);
void ProcessEvent(gshort *Para,gboolean Option);

/*----------------------------------------------------------------------------------------------------------------------*/
gint ZlsGlsSkip(FILE *ListFile)
//Returns 0 for a successful buffer skip, Returns 1 if EOF or invalid signature
{
gushort Buf[32768];
gchar Sign[5];
gint WdsInBuf;

if (!fread(Sign,1,4,ListFile)) return 1;                                                                  //return if EOF
Sign[4]='\0';
if (strcmp(Sign,"DAPS") && strcmp(Sign,"GSUP")) return 1;                                             //Invalid signature
if (!fread(Buf,2,2,ListFile)) return 1;
WdsInBuf=Buf[0]/2;
if (!fread(Buf,2,WdsInBuf,ListFile)) return 1;
return 0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint ZlsGlsRead(FILE *ListFile,gushort *Buf,glong *ZBufs,glong *EvtsRead,glong *BytsRead,
                gboolean OptionReWrite,gshort *OutBuf,gint *NewEvts)
//Reads a block of zls data
//Return 0 normally, 1 on Invalid Signature or EOF
//    When OptionReWrite=TRUE we wish to rewrite files. Now:
//    1. Spectra building is avoided. The call to ProcessEvent calculates pseudos but does not increment spectra
//    2. A copy of the de-coded buffer with only selected parameters (and pseudos) is placed in OutBuf
//    3. Events where all relevant parameters (i.e selected for rewrite) are zero are filtered out
//    4. The number of events in OutBuf is stored in NewEvts 
{
glong WdsRead;
gchar Sign[5];
gint CBufSiz,I,J,WdsInBuf,Evt,WrdPtr,OutPtr,BitSum,Bit,HdrWds,GrpHdrWds,Grps,Ptr2,i,Ix,j,GrpBit,LastBit,NewBufPtr;
gushort EvtHdr[128];
gshort Para[MAX_TOTAL_PAR];
gboolean SkipEvent;

HdrWds=(Setup.Parameter.NPar+15)/16; *NewEvts=0; NewBufPtr=0;
Grps=(Setup.Parameter.NPar+3)/4; GrpHdrWds=((Setup.Parameter.NPar+3)/4+15)/16;

WdsRead=fread(Sign,1,4,ListFile);
if (WdsRead<4) return 1;                                                                        //Normal exit from zls file
Sign[4]='\0'; (*BytsRead)+=4;
if (strcmp(Sign,"DAPS") && strcmp(Sign,"GSUP")) { SAttention("Invalid signature in zls/gls file"); return 1; }
if ((WdsRead=fread(&CBufSiz,4,1,ListFile))<1) return 1; 
WdsInBuf=CBufSiz/2; (*BytsRead)+=2*WdsRead; //g_print("CBufSiz=%d\n",CBufSiz);
if (WdsInBuf>500000) { SAttention("ERROR::Zbuf too large"); return 1; }
WdsRead=fread(Buf,2,WdsInBuf,ListFile); (*BytsRead)+=2*WdsRead;
if (WdsRead<WdsInBuf) { SAttention("Sudden EOF while reading"); return 1; }
Evt=0; WrdPtr=0; OutPtr=0;
while (WrdPtr<WdsInBuf)                                                                  //Loop over events in this buffer
      {
      if (Sign[0]=='D')                                                                         //Decoding for DAPS scheme
         {
         for (I=0;I<HdrWds;++I) EvtHdr[I]=Buf[WrdPtr+I];
         WrdPtr+=HdrWds; BitSum=0;
         for (J=0;J<HdrWds;J++)
             {
             for (I=0;I<16;++I)
                 {
                 Bit=(EvtHdr[J] >> I) & 1; BitSum+=Bit;
                 if (Bit==1) { Para[I+16*J]=Buf[WrdPtr]; WrdPtr++; } else Para[I+16*J]=0;
                 }
             }
         if (BitSum>Setup.Parameter.NPar) { SAttention("BitSum Error"); return 1; }
         }
      else                                                                                      //Decoding for GSUP scheme
         {
         Ptr2=GrpHdrWds;
         for (i=0;i<Grps;++i)
             {
             Ix=i/16;
             for (j=0;j<4;j++) Para[4*i+j]=0;
             GrpBit=(Buf[WrdPtr+Ix]>>(i%16)) & 1;
             if (GrpBit)                                                    //At least one detector in the group has fired
                {
                for (j=0;j<4;j++)
                    {
                    J=(Buf[WrdPtr+Ptr2]>>13) & 3;                                   //Det no. within group from bits 13-14
                    LastBit=Buf[WrdPtr+Ptr2]>>15;                                       //Set if its last det of the group
                    Para[4*i+J]=Buf[WrdPtr+Ptr2] & 8191;                                          //mask off bits 13,14,15
                    Ptr2++;
                    if (LastBit) break;                                               //Reached last detector in the group
                    }
                }
             }
             WrdPtr+=Ptr2;
         }
         ProcessEvent(Para,OptionReWrite);
         if (OptionReWrite)                                      //Make parameter block available for re-writing list file
            {
            for (i=0,SkipEvent=TRUE;i<NTOT;++i) if ((Setup.ReWrite.Select[i]) && (Para[i]>0)) {SkipEvent=FALSE; break;}
            if (!SkipEvent)                                               //Skip event if all relevant parameters are zero
               {
               ++(*NewEvts);
               if (NewBufPtr+Setup.ReWrite.NPar > MAX_DATABUF)
                  {
                  ReWriteRequest=StopAllReWrite;
                  SAttention("Error...Please increase MAX_DATABUF in lamps.h\nand recompile lamps");
                  return 1;
                  }
               for (i=0;i<NTOT;++i) if (Setup.ReWrite.Select[i]) OutBuf[NewBufPtr++]=Para[i];
               }
            }
         ++Evt;
      }
++(*ZBufs); (*EvtsRead)+=Evt;
return 0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
void RadwareWriteHeader(FILE *Fp)
{
//Write a header block of 240 bytes
gchar Comment[240];
gint RadBufSize;

RadBufSize=240;
strcpy(Comment,"dmp file generated by lamps\n");
if (fwrite(&RadBufSize,4,1,Fp)<1) { SAttention("Error: Disk Full?"); return; }
if (fwrite(Comment,240,1,Fp)<1)   { SAttention("Error: Disk Full?"); return; }
if (fwrite(&RadBufSize,4,1,Fp)<1) { SAttention("Error: Disk Full?"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
void RadwareWrite(FILE *Fp,gushort *Buf,gint NEvt,gint NPar)
//Writes a block of list data in Radware readable format
// This function is employed for rewriting list files (rewrite.c)  
{
gint i,j,k,Ptr,RPtr,RadEvts,RadRecLen,RadBufSize;
gushort RadBuf[32768],Adc[5],Head[4];

for (i=0,Ptr=0,RPtr=0,RadEvts=0;i<NEvt;++i)                            //Loop over events in Buf
    {
    if (RPtr>32700) { SAttention("RadBuf[Exceeded!]"); return; }
    for (j=0;j<5;++j) Adc[j]=0;
    for (j=0,k=0;j<NPar;++j) 
        {
        if (Buf[Ptr+j]>0) { Adc[k]=Buf[Ptr+j]; ++k; }
        if (k==5) break;
        }
    if (k>=3) 
       {
       for (j=0;j<5;++j) { RadBuf[RPtr]=Adc[j]; ++RPtr; } 
       ++RadEvts;
       }
    Ptr+=NPar;
    }
//g_print("NEvt=%d RadEvts=%d\n",NEvt,RadEvts);
RadRecLen=5*RadEvts; RadBufSize=(4+RadRecLen)*2;
//g_print("RadRecLen=%d RadBufSize=%d\n",RadRecLen,RadBufSize);
Head[0]=-8; Head[1]=RadBufSize/2; Head[2]=5; Head[3]=RadRecLen;
if (fwrite(&RadBufSize,4,1,Fp)<1) { SAttention("Error: Disk Full?"); return; }
if (fwrite(Head,2,4,Fp)<1) { SAttention("Error: Disk Full?"); return; }
if (fwrite(RadBuf,2,RadRecLen,Fp)<1) { SAttention("Error: Disk Full?"); return; }
if (fwrite(&RadBufSize,4,1,Fp)<1) { SAttention("Error: Disk Full?"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------*/
gint ZlsWrite(FILE *Fp,gushort *Buf,gint NEvt,gint NPar)
//Writes a block of Zls data
// This function is employed for rewriting list files (rewrite.c)  
// but not for data acquisition because 
//      (a) In data acquisition we are combining data from 2 crates
//      (b) For data acquisition there are threholds (LLD, ULD) for zero supression. Now we take LLD=0.
// Return 0 normally, 1 if disk full or other error
{
gint i,j,k,l,m,Ptr,CPtr,CBufSiz,HdrWds;
gushort CompBuf[MAX_DATABUF+2048];                                          //The compressed buffer can be larger than Buf
gushort BitMask[MAX_HDR];                                                //The event masks, 1 mask for every 16 parameters
static gchar Sign[4] = {"DAPS"};
                                                                                                                             
HdrWds=(NPar+15)/16;
for (i=0,Ptr=0,CPtr=0;i<NEvt;++i)                                                                //Loop over events in Buf
    {
    for (j=0;j<HdrWds;++j) BitMask[j]=0;
    for (j=0,m=0;j<NPar;++j)
        {
        k=j/16; l=j%16;
        if (Buf[Ptr+j] > 0) { BitMask[k]|=1<<l; CompBuf[CPtr+HdrWds+m]=Buf[Ptr+j]; ++m; }
        }
    for (j=0;j<HdrWds;++j) CompBuf[CPtr+j]=BitMask[j];
    Ptr+=NPar; CPtr+=HdrWds+m;
    }
CBufSiz=2*CPtr;
if (fwrite(Sign,4,1,Fp)<1) { SAttention("Error: Disk Full?"); return 1; }
if (fwrite(&CBufSiz,4,1,Fp)<1) { SAttention("Error: Disk Full?"); return 1; }
if (fwrite(CompBuf,2,CPtr,Fp)<CPtr) { SAttention("Error: Disk Full?"); return 1; }
return 0;
}
/*----------------------------------------------------------------------------------------------------------------------*/
