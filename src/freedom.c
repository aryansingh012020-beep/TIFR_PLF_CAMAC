#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include "common.h"
#include "lamps.h"

//External globals
extern struct Setup Setup;
extern enum ReWriteRequest ReWriteRequest;
extern glong BytesWritten;
//Function Prototypes
void expand(datapackptr inbuf, datapackptr outbuf);
extern datapack buf,readbuf;
void ProcessEvent(gshort *Para,gboolean Option);
void ZlsError(void);
void Attention(gint XPos,gchar *Messg);
/*---------------------------------------------------------------------------------------------------------*/
gint process_event_block(datapackptr rdbuf,gboolean OptionReWrite,gushort *OutBuf,gint *NewEvts)
{
gint i,k,offset,NewBufPtr;
gshort Para[MAX_TOTAL_PAR];
gboolean SkipEvent;

expand(rdbuf,&buf); NewBufPtr=0;
for (k=0;k<buf.header.number_of_units;++k)
    {
    offset=k*buf.header.unitsize;
    for (i=0;i<buf.header.unitsize;++i) Para[i]=buf.data[offset+i];
    ProcessEvent(Para,OptionReWrite);
    if (OptionReWrite)                                      //Make parameter block available for re-writing list file
       {
       for (i=0,SkipEvent=TRUE;i<NTOT;++i) if ((Setup.ReWrite.Select[i]) && (Para[i]>0)) { SkipEvent=FALSE; break; }
       if (!SkipEvent)                                               //Skip event if all relevant parameters are zero
          {
          ++(*NewEvts);
          if (NewBufPtr+Setup.ReWrite.NPar > MAX_DATABUF)
             {
             ReWriteRequest=StopAllReWrite;
             Attention(0,"Error...Please increase MAX_DATABUF in lamps.h\nand recompile lamps");
             //g_print("NewBufPtr+Setup.ReWrite.NPar=%d MAX_DATABUF=%d",NewBufPtr+Setup.ReWrite.NPar,MAX_DATABUF);
             return 1;
             }
          for (i=0;i<NTOT;++i) if (Setup.ReWrite.Select[i]) OutBuf[NewBufPtr++]=Para[i];
          }
       }
    } 
//g_print("process_event_block::*NewEvts=%d\n",*NewEvts);
return buf.header.unitsize*buf.header.number_of_units;
}
/*---------------------------------------------------------------------------------------------------------*/
gint FreedomRead(FILE *ifp,glong *ZBufs,glong *EvtsRead,glong *BytsRead,
                 gboolean OptionReWrite,gushort *OutBuf,gint *NewEvts)
//Reads a block of Freedom/Candle data
//Return 0 normally, 1 on EOF
//    When OptionReWrite=TRUE we wish to rewrite files. Now:
//    1. Spectra building is avoided. The call to ProcessEvent calculates pseudos but does not increment spectra
//    2. A copy of the de-coded buffer with only selected parameters (and pseudos) is placed in OutBuf
//    3. Events where all relevant parameters (i.e selected for rewrite) are zero are filtered out
//    4. The number of events in OutBuf is stored in NewEvts
{
glong x;

*NewEvts=0;
if (!(x=fread(&readbuf.header,sizeof(dataheader),1,ifp))) return 1;
*BytsRead+=x*sizeof(dataheader);
if (readbuf.header.block == end_of_file) return 1;
if (!(x=fread(&readbuf.data,readbuf.header.size_in_bytes,1,ifp))) return 1;
*BytsRead+=x*readbuf.header.size_in_bytes;
readbuf.size=sizeof(dataheader)+readbuf.header.size_in_bytes;
expand(&readbuf,&buf);

if (buf.header.block == event) 
   { 
   (*ZBufs)++; 
   (*EvtsRead)+=process_event_block(&readbuf,OptionReWrite,OutBuf,NewEvts); 
   }
return 0;
}
/*---------------------------------------------------------------------------------------------------------*/
gint FreedomSkip(FILE *ifp)                                              
//Skips 1 buffer. Returns 0 if an event buffer was successfully skipped,
//                Returns 1 if EOF was encountered 
//                Returns 2 if the buffer skipped was not and event buffer 
{
if (!fread(&readbuf.header,sizeof(dataheader),1,ifp)) return 1;
if (readbuf.header.block == end_of_file) return 1;
if (!fread(&readbuf.data,readbuf.header.size_in_bytes,1,ifp)) return 1;
readbuf.size=sizeof(dataheader)+readbuf.header.size_in_bytes;
expand(&readbuf,&buf);

if (buf.header.block == event) return 0; else return 2;
}
/*---------------------------------------------------------------------------------------------------------*/
void expand(datapackptr inbuf,datapackptr outbuf)
{
gint nwords=inbuf->header.number_of_units * inbuf->header.unitsize;
gshort *dstart,*wrtptr,*rdptr,*patptr,mask;
gint i,numpats;

if (inbuf->header.block != event)
   { memcpy(outbuf,inbuf,inbuf->size); return; }
memset(outbuf,0,sizeof(datapack));
numpats=nwords/16; if (nwords % 16) ++numpats;

rdptr=inbuf->data + numpats;
patptr=inbuf->data;
dstart=wrtptr=outbuf->data;
mask=1;

for (i=0;i<nwords;++i)
    {
    if (mask & *patptr) *wrtptr++=*rdptr++;
    else *wrtptr++=0;
    mask <<= 1;
    if (!mask) { mask=1; ++patptr; }
    }

outbuf->header.block=inbuf->header.block;
outbuf->header.number_of_units=inbuf->header.number_of_units;
outbuf->header.unitsize=inbuf->header.unitsize;
outbuf->header.compstatus=0;
outbuf->header.size_in_bytes=2*inbuf->header.number_of_units*inbuf->header.unitsize;
}
/*---------------------------------------------------------------------------------------------------------*/
void compress(datapackptr inbuf,datapackptr outbuf)
{
gint evsize=inbuf->header.unitsize;
gint nwords=inbuf->header.number_of_units*evsize;
gshort *dstart,*wrtptr,*rdptr,*patptr;
gint i,numpats;
gushort bitpat;

if (inbuf->header.block != event) { memcpy(outbuf,inbuf,inbuf->size); return; }
memset(outbuf,0,sizeof(datapack));
numpats=nwords/16; if (nwords % 16) ++numpats;
rdptr=inbuf->data; dstart=patptr=outbuf->data; wrtptr=outbuf->data+numpats; bitpat=1;

for (i=0;i<nwords;++i)
    {
    /* mask the unused higher bits of data. data equal to mask is set to zero. */
    unsigned short mask = 0x1fff; /* 13 bit ADC mask to treat overflow as zero */
    *rdptr &= mask;
    if (*rdptr == mask) *rdptr=0;
    if (*rdptr) { *wrtptr++ = *rdptr; *patptr |= bitpat; }
    ++rdptr; bitpat <<= 1;
    if (!bitpat) { bitpat = 1; ++patptr; }
    }
outbuf->header.block=event; outbuf->header.number_of_units=inbuf->header.number_of_units;
outbuf->header.unitsize=inbuf->header.unitsize; outbuf->header.compstatus=1;
outbuf->header.size_in_bytes=2*(int)(wrtptr-dstart);
}
/*---------------------------------------------------------------------------------------------------------*/
datapack buf,wrbuf;
gboolean writeToFile(FILE *Fp)
{
compress(&buf,&wrbuf);
wrbuf.size=wrbuf.header.size_in_bytes+sizeof(dataheader);
if (fwrite(&wrbuf,wrbuf.size,1,Fp) != 1) return FALSE;
BytesWritten+=wrbuf.size;
return TRUE;
}
/*---------------------------------------------------------------------------------------------------------*/
void queue_text(enum blocktype block,gchar *txt,FILE *Fp)
{
int numbytes;

memset(&buf,sizeof(buf),0);
numbytes=strlen(txt)+1;
buf.header.block=block;
buf.header.unitsize=numbytes;
buf.header.number_of_units=1;
buf.header.size_in_bytes=numbytes;
memcpy(buf.data,txt,numbytes);
buf.size=numbytes+sizeof(dataheader);
writeToFile(Fp);
}
/*---------------------------------------------------------------------------------------------------------*/
gboolean writeEventBlock(gint eventSize,gint ev_per_block,gushort data[],FILE *Fp)
{
int numbytes;

memset(&buf,sizeof(buf),0);
buf.header.block=event;
buf.header.unitsize=eventSize;
buf.header.number_of_units=ev_per_block;
numbytes=ev_per_block*eventSize*2;
buf.header.size_in_bytes=numbytes;
buf.size=numbytes+sizeof(dataheader);

memcpy(buf.data,data,sizeof(buf.data));
return writeToFile(Fp);
}
/*---------------------------------------------------------------------------------------------------------*/
void writeNames(int noOfAdcs,int noOfSingles,int noOfScalers,FILE *Fp)
{
int i,numbytes;
char name[10],*p;

memset(&buf,sizeof(buf),0);
p=(char*)buf.data;
sprintf(p,"READ %d\nCONTROL %d\nINIT %d\nSCALER %d\nSINGLE %d\n",noOfAdcs,0,0,noOfScalers,noOfSingles);
numbytes=strlen(p)+1;
for (i=0;i<noOfAdcs;++i) { sprintf(name,"ADC-%02d\n",i+1); numbytes+=strlen(name); strcat(p,name); }
buf.header.block=names;
buf.header.unitsize=numbytes;
buf.header.number_of_units=1;
buf.header.size_in_bytes=numbytes;
buf.size=numbytes+sizeof(dataheader);

writeToFile(Fp);
}
/*---------------------------------------------------------------------------------------------------------*/
void writeEndOfFile(FILE *Fp)
{
dataheader last;

last.block=end_of_file;
last.unitsize=0;
if (fwrite(&last,sizeof(dataheader),1,Fp) != 1) ZlsError();
}
/*---------------------------------------------------------------------------------------------------------*/
