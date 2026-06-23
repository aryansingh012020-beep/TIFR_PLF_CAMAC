#include <stdio.h>
#include <string.h>
#include <time.h>
#include "common.h"
//#include "dserv.h"

void expand(datapackptr inbuf, datapackptr outbuf);

datapack buf,readbuf;


void process_hgram_block(datapackptr buf)
{
	printf("HISTOGRAM:\nSize = %d\n",buf->header.unitsize);
}


int process_event_block(datapackptr rdbuf)
{
int i,k;

expand(rdbuf,&buf);


/*
printf("eventsize = %d. events per block = %d compressed size = %d words\n",
rdbuf->header.unitsize, rdbuf->header.number_of_units, rdbuf->header.size_in_bytes / 2); 
*/

/*
for(k = 0; k < buf.header.number_of_units; ++k)
	{
	int offset = k * buf.header.unitsize;
	if(!k)
		fprintf(stdout,"%d: ",k);	
	for(i = 0; i < buf.header.unitsize; ++i)
		fprintf(stdout,"%4d ",buf.data[offset + i]);
	fprintf(stdout,"\n");
	}
*/

return buf.header.unitsize * buf.header.number_of_units;
}



int main(int argc, char *argv[])
{
FILE 	*ifp;
char	filename[50];
int	num_ev_blocks = 0;
int	num_events = 0;
time_t	init,fin;

if(argc == 2)
	strcpy(filename,argv[1]);
else
	{
	fprintf(stderr,"Usage: exam filename. using 'ajith.001'\n");
	strcpy(filename,"ajith.001");
	}
	
ifp = fopen(filename,"rb"); 
if(!ifp) 
	{
	fprintf(stderr,"File %s not found\n",argv[1]);
	exit(1);
	}


time(&init);
while(!feof(ifp))
	{
	fread(&readbuf.header,sizeof(dataheader),1,ifp);
	if(readbuf.header.block == end_of_file)
		{
		time(&fin);
		
		printf("END OF FILE: %ld %ld %ld\n",init, fin, fin-init);
		printf("Total Number of Event Blocks = %d %10.3f\n",
			num_ev_blocks, ((float) num_ev_blocks) / (fin-init) );
		break;
		}	
	fread(&readbuf.data, readbuf.header.size_in_bytes, 1, ifp);
	readbuf.size = sizeof(dataheader) + readbuf.header.size_in_bytes;
	expand(&readbuf,&buf);

	switch(buf.header.block)
		{
	case event:
		++num_ev_blocks;
		num_events += process_event_block(&readbuf);
	break;
	
	case hgram:
		process_hgram_block(&readbuf);
	break;
	
	case scaler:
		printf("SCALER:\n%s\n",(char *) readbuf.data);
		break;

	case start:
		printf("START:\n%s\n",(char *) readbuf.data);
		break;
	case stop:
		printf("STOP:\n%s\n",(char *) readbuf.data);
		break;
	case pause:
		printf("PAUSE:\n%s\n",(char *) readbuf.data);
		break;
	case resume:
		printf("RESUME:\n%s\n",(char *) readbuf.data);
		break;
	case names:
		printf("NAMES:\n%s\n",(char *) readbuf.data);
		break;
	case user:
		printf("USER:\n%s\n",(char *) readbuf.data);
	break;
		
	default:
		printf("Unknown Blocktype %d\n",readbuf.header.block);		
		}
	}


return 0;
}


void expand(datapackptr inbuf, datapackptr outbuf)
{
int	nwords = inbuf->header.number_of_units * inbuf->header.unitsize;
shptr 	dstart,wrtptr,rdptr,patptr;
int	i, numpats;
unsigned short mask;

if(inbuf->header.block != event)
	{
	memcpy(outbuf,inbuf,inbuf->size);
	return;
	}
	
memset(outbuf, 0, sizeof(datapack));

numpats = nwords / 16; if(nwords % 16) ++numpats;

rdptr = inbuf->data + numpats;
patptr = inbuf->data;
dstart = wrtptr = outbuf->data;
mask = 1;

for(i = 0; i < nwords; ++i)
	{
	if(mask & *patptr)
		*wrtptr++ = *rdptr++;
	else
		*wrtptr++ = 0;
	mask <<= 1;
	if(!mask)
		{
		mask = 1;
		++patptr;
		}
	}

outbuf->header.block  = inbuf->header.block;
outbuf->header.number_of_units = inbuf->header.number_of_units;
outbuf->header.unitsize = inbuf->header.unitsize;
outbuf->header.compstatus = 0;
outbuf->header.size_in_bytes = 2 * inbuf->header.number_of_units * inbuf->header.unitsize;
}
