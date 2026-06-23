#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include "lamps.h"

#define ACCEPT_ALL       0
#define REJECT_CS_EVENTS 1
#define REJECT_PU_EVENTS 2
#define REJECT_CS_PU     3
#define ONLY_CS_EVENTS   4
#define ONLY_PU_EVENTS   5

/*External globals*/
extern struct Setup Setup;
/*Function prototypes*/
void ProcessEvent(gshort *Para,gboolean Option);
/*------------------------------------------------------------------------------------------------------------------------*/
void Exogam(gushort *Buf,glong *ZBufs,glong *EvtsRead,glong *BytsRead)
{
gchar BlockType[9];
gint i,j,NEvents,Evt,Ptr,SubEventLength,Address,Data;
gshort Para[MAX_PAR+MAX_PSEUDO];

(*BytsRead)+=16384;
memcpy(BlockType,Buf,8); BlockType[8]='\0';
if (!strcmp(BlockType," EBYEDAT"))
   {
   (*ZBufs)++;
   NEvents=Buf[11];
   *EvtsRead+=NEvents;
   for (Evt=0,Ptr=16;Evt<NEvents;Evt++)
       {
       for (i=0;i<Setup.Parameter.NPar;i++) Para[i]=0;
       SubEventLength=Buf[Ptr+6]; //g_print("SubEventLength=%d\n",SubEventLength);
       Ptr+=7;
       for (i=0;i<(SubEventLength-2)/2;i++)
           {
           Address=Buf[Ptr] & 0x3FFF;                                         //Mask upper 2 bits: compton rej and pileup
           Data=Buf[Ptr+1];
           for (j=0;j<Setup.Parameter.NPar;j++) 
               {
               if (Address==Setup.Parameter.ExogamID[j]) 
                  { 
                  Para[j]=MIN(Data,Setup.Parameter.Chan[j]-1); 
                  if      ((Setup.Parameter.ExoOpt[j]==REJECT_CS_EVENTS) && ((Buf[Ptr]>>14) & 1) )                Para[j]=0;
                  else if ((Setup.Parameter.ExoOpt[j]==REJECT_PU_EVENTS) && (Buf[Ptr]>>15) )                      Para[j]=0;
                  else if ((Setup.Parameter.ExoOpt[j]==REJECT_CS_PU) && (((Buf[Ptr]>>14) &1) || (Buf[Ptr]>>15)) ) Para[j]=0;
                  else if ((Setup.Parameter.ExoOpt[j]==ONLY_CS_EVENTS) && !((Buf[Ptr]>>14) & 1) )                 Para[j]=0;
                  else if ((Setup.Parameter.ExoOpt[j]==ONLY_PU_EVENTS) && !(Buf[Ptr]>>15) )                       Para[j]=0;
                  }
               }
           Ptr+=2;
           }
       ProcessEvent(Para,FALSE);
       }
   }
}
/*------------------------------------------------------------------------------------------------------------------------*/

