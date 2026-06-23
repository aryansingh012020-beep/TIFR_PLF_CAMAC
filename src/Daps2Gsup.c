/*       Daps2Gsup.c:: A.Chatterjee 13-09-00. Update: 13-09-00           *
 * Converts a zero-suppressed list mode file in the old DAPS format      *
 * to the group suppressed GSUP format, explained below:                 *
 . Block signature: 'GSUP' (4-bytes) and Block length in bytes (4-bytes) 
 . For eack block, an integral number of events
 . For each event, first comes a group bit-pattern, with 1 signifying that 
   at least one parameter in a group of 4 parameters is non-'zero'. This 
   requires int((int((n+3)/4)+15)/16) bytes (n is the number of parameters)
 . This is followed by only non-'zero' data with extra bits: 
               2^15         2^14      2^13      2^12 ... 2^0 
            Last Para           Para No.          ADC-data
    Here, the ADC data is confined to the lower 13-bits, bits 2^14 and
    2^13 give the parameter number (0-3) within the group of 4 and bit
    2^15 is set for the last non-'zero' parameter.                       */ 

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define PARMAX  2000
#define HDRMAX (PARMAX+15)/16
#define OUTSZ  65535
#define GRPBUFSZ  65535
#define GRPHDRMAX ((PARMAX+3)/4+15)/16

FILE *DFile,*GFile;
char Messg[80];
int TotEvts,Bufs,BytsRead,FileBytes,FireMin,FireMax,BytsWrit;
float FireAvg;

void Finish(void);
/*---------------------------------------------------------------------------*/
int main(int argc,char *argv[])
{
char DapsFName[60],GsupFName[60],DapsSign[5],GsupSign[5];
struct stat StatBuf;
int NoOfParams,HdrWds,GrpHdrWds,NSkip,I,WdsRead,WdsInBuf,NBuf,
    BitSum,Evt,J,Bit,WrdPtr,Grps,HiFire,Ix;
long BytsInGrpBuf,FilePtr1,FilePtr2;
unsigned short Buf[OUTSZ],EvtHdr[HDRMAX],Para[PARMAX],GrpHdr[GRPHDRMAX];
int Ll,Ul,Lld[PARMAX],Uld[PARMAX],Option,ParNo;
FILE *ThreshFile;

sprintf(GsupSign,"GSUP");
if (argc!=3) {printf("Usage: Daps2Gsup <DAPS File> <GSup File>\n"); exit(0);}
strcpy(DapsFName,argv[1]); strcpy(GsupFName,argv[2]);
printf("Converting %s to %s\n",DapsFName,GsupFName);
stat(DapsFName,&StatBuf); FileBytes=StatBuf.st_size;
printf("File: %s, Size: %d Bytes\n",DapsFName,FileBytes);
if ((DFile=fopen(DapsFName,"r"))==NULL) 
   { printf("%s: No such file!\n",DapsFName); exit(0); }
printf("How many parameters(e.g. 960)?"); scanf("%d",&NoOfParams);
if (NoOfParams<1) { printf("Invalid input\n"); exit(0); }
if (NoOfParams>PARMAX) 
   { printf("%d: Cant handle that much!\n",PARMAX); exit(0); }
HdrWds=(NoOfParams+15)/16;
if (HdrWds>HDRMAX)
   { printf("Unexpected error HdrWds %d > %d\n",HdrWds,HDRMAX); exit(0); }
Grps=(NoOfParams+3)/4; 
GrpHdrWds=((NoOfParams+3)/4+15)/16;
if (GrpHdrWds>GRPHDRMAX)
   { 
   printf("Unexpected error GrpHdrWds %d > %d\n",GrpHdrWds,GRPHDRMAX); 
   exit(0); }
printf("Option for applying thresholds\n");
printf("1=None, 2=same thresholds for all paras, 3=read from thresh.dat\n");
printf("Enter 1,2 or 3..."); scanf("%d",&Option);
switch (Option)
   {
   case 1:
      for (I=0;I<NoOfParams;I++) { Lld[I]=0; Uld[I]=8192; } 
      break;
   case 2:
      printf("LLD? (only data >LLD accepted)"); scanf("%d",&Ll);
      for (I=0;I<NoOfParams;I++) Lld[I]=Ll;
      printf("ULD? (only daty <ULD accepted)"); scanf("%d",&Ul);
      for (I=0;I<NoOfParams;I++) Uld[I]=Ul;
      break;
   case 3:
      printf("Reading LLD and ULD values from thresh.dat\n");
      printf("Data in free format like this:\n");
      printf("1  100   8190\n");
      printf("2  75    8175\n");
      printf(".... upto last parameter\n");
      if ((ThreshFile=fopen("thresh.dat","r"))==NULL) 
         { printf("ERROR: File thresh.dat not found!\n"); exit(0); }
      for (I=0;I<NoOfParams;I++)
          {
          fscanf(ThreshFile,"%d %d %d",&ParNo,&Lld[I],&Uld[I]);
          if (ParNo != I+1)
             {
             printf("ERROR reading thresholds for Para No: %d\n",I+1);
             printf("Please check the file: thresh.dat\n");
             }
          }
      fclose(ThreshFile);
      break;
   default:
      printf("Unkown option\n"); exit(1);
   }
printf("How many z-buffs to skip?"); scanf("%d",&NSkip);
/*-------Begin Skip buffers-----*/
for (I=0;I<NSkip;I++)
   {
   printf("Skipping Buffer: %d\n",I+1); 
   WdsRead=fread(DapsSign,1,4,DFile);                  /*Read 4 byte signature*/
   DapsSign[4]='\0';                                        /*Terminate string*/
   if (strcmp(DapsSign,"DAPS"))
      { sprintf(Messg,"Signature %s is not DAPS",DapsSign); Finish(); }
   fread(Buf,2,2,DFile); WdsInBuf=(65536L*Buf[1]+Buf[0])/2;
   if (WdsInBuf>OUTSZ)
      { sprintf(Messg,"Decompressed data overflows buffer"); Finish(); }
   fread(Buf,2,WdsInBuf,DFile);                                   /*Read Block*/
   }
/*-------End Skip buffers-----*/
printf("How many buffers to read(0=Complete file)?"); scanf("%d",&NBuf);
if ((GFile=fopen(GsupFName,"w"))==NULL)
   { printf("Could not open: %s\n",GsupFName); exit(0); }
Bufs=0; BytsRead=0; FireMin=PARMAX; FireMax=0; FireAvg=0.0; TotEvts=0;
BytsWrit=0;
while (1)                                          /*Loop over buffers to read*/
   {
   WdsRead=fread(DapsSign,1,4,DFile);                  /*Read 4-byte signature*/
   if (WdsRead<4) 
      { sprintf(Messg,"EOF while looking for signature"); Finish(); }
   DapsSign[4]='\0';                                        /*Terminate string*/
   BytsRead+=4;
   if (strcmp(DapsSign,"DAPS")) 
      { sprintf(Messg,"Signature %s is not DAPS",DapsSign); Finish(); }
   fwrite(GsupSign,1,4,GFile);                        /*Write 4-byte signature*/
   BytsWrit+=4;
   WdsRead=fread(Buf,2,2,DFile); WdsInBuf=(65536L*Buf[1]+Buf[0])/2;
   BytsRead+=2*WdsRead;
   if (WdsInBuf>OUTSZ)
      { sprintf(Messg,"Decompressed data overflows buffer"); Finish(); }
   BytsInGrpBuf=0;       /*At this stage I dont know the value of BytsInGrpBuf*/
   FilePtr1=ftell(GFile);    /*To overwrite WdsInGrp at this point in the file*/
   fwrite(&BytsInGrpBuf,4,1,GFile); BytsWrit+=4;
   WdsRead=fread(Buf,2,WdsInBuf,DFile); BytsRead+=2*WdsRead;
   if (WdsRead<WdsInBuf)
      { sprintf(Messg,"EOF before complete Z-buf read"); Finish(); }
   Evt=0; WrdPtr=0; 
   while (WrdPtr<WdsInBuf)                   /*Loop over events in this buffer*/
      {
      for (I=0;I<HdrWds;I++) EvtHdr[I]=Buf[WrdPtr+I];
      WrdPtr+=HdrWds;
                  /*Calculate BitSum and Para values*/
      BitSum=0;
      for (J=0;J<HdrWds;J++) 
         {
         for (I=0;I<16;I++) 
             {
             Bit=(EvtHdr[J] >> I) & 1;
             BitSum+=Bit;
             if (Bit==1) { Para[I+16*J]=Buf[WrdPtr]; WrdPtr++; }
             else          Para[I+16*J]=0;
             }
         }
                /*Okay, BitSum and Para values are calculated*/
      if (BitSum>NoOfParams) 
         {
         sprintf(Messg,"BitSum(%d) > NoOfParams(%d)",BitSum,NoOfParams);
         Finish();
         }
                 /*Statistics calculations*/
      FireAvg=FireAvg+(float)BitSum; TotEvts++;
      if (BitSum<FireMin) FireMin=BitSum;
      if (BitSum>FireMax) FireMax=BitSum;
              /*Let us calculate the GrpHdr*/
      for (Ix=0;Ix<GrpHdrWds;Ix++) GrpHdr[Ix]=0; 
      for (I=0;I<Grps;I++)
          {
          Ix=I/16;
          for (J=0,Bit=0;J<4;J++) 
              {
              if ( (Para[4*I+J]>Lld[4*I+J]) && (Para[4*I+J]<Uld[4*I+J]) )
                 { Bit=1; break; }  /*When any 1 det fires, Bit=1*/
              }
          GrpHdr[Ix]+=Bit<<(I-16*Ix);
          }
                 /*Write the GrpHdr*/
      BytsWrit+=fwrite(GrpHdr,2,GrpHdrWds,GFile);          
      BytsInGrpBuf+=2*GrpHdrWds;
                /*Write the non-zero events along with higher bits*/
      for (I=0;I<Grps;I++)
          {
          Ix=I/16;
          if ( ( (GrpHdr[Ix]>>(I-16*Ix)) & 1) >0)
             /*At least 1 para in group is non-zero*/
             {
             for (J=0;J<4;J++)                              /*Find HiFire*/
                 if ((Para[4*I+J]>Lld[4*I+J]) && (Para[4*I+J]<Uld[4*I+J])) 
                    HiFire=J;
             for (J=0;J<4;J++)
                 {
                 if ((Para[4*I+J]>Lld[4*I+J]) && (Para[4*I+J]<Uld[4*I+J]))
                    {
                    Para[4*I+J]+=(J<<13); /*Put para # (0..3) in bits 13,14*/
                    if (J==HiFire) Para[4*I+J]+=(1<<15);
                    fwrite(&Para[4*I+J],2,1,GFile); BytsWrit+=2;
                    BytsInGrpBuf+=2;
                    }
                 }
             }
          }
      Evt++;
      }                                  /*End loop over events in this buffer*/
   FilePtr2=ftell(GFile); fseek(GFile,FilePtr1,SEEK_SET); 
   fwrite(&BytsInGrpBuf,4,1,GFile); fseek(GFile,FilePtr2,SEEK_SET);
   if ( !((Bufs+1) % 10) ) printf("Buffers: %d\n",Bufs+1);
   if ((Bufs>NBuf) && (NBuf>0) )
      { sprintf(Messg,"Read the requested buffers"); Finish(); }
   Bufs++;
   }                                                   /*End loop over buffers*/
return 1;
}
/*---------------------------------------------------------------------------*/
void Finish(void)
/*Cleans up after EOF is reached or ERROR is encountered*/
{
fclose(DFile);
fclose(GFile);
printf("FINISH:: %s\n",Messg);
if (TotEvts>0) FireAvg=FireAvg/TotEvts;
printf("Valid buffers processed=%d\n",Bufs+1);
printf("Bytes read from file: %d, out of: %d\n",BytsRead,FileBytes);
printf(" ---Statistics from the buffers read--- \n");
printf("Firing Parameters: Minimum    Average   Maximum\n");
printf("%24d %8.3f %9d\n",FireMin,FireAvg,FireMax);
exit(0);
}
/*---------------------------------------------------------------------------*/
