#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

int main(argc,argv)
int argc;
char *argv[];
{
int Fp,Nt,WdsPerLine,LinesPerScreen,Line,Word,i,Loc,WrDsk,Done,BytesRequired,
    Lines,AtEOF,Loc2,Subs;
long FileBytes,FileWds;
off_t Offset;
char FileName[20],Key[10],Dsk[10],OutFileName[20],YorN[10];
unsigned short Data[200];
struct stat StatBuf;
FILE *Dfp;

void ZeroBuf(unsigned short *Data,int N);

if (argc>2) 
   { printf("read16 error: too many arguments\n"); return 1; }
if (argc==1) { printf("File Name?"); scanf("%s",FileName); }
else         strcpy(FileName,argv[1]);
printf("Substitute 16708,21328 by DAPS?(y/n)"); scanf("%s",YorN);
if (YorN[0]=='y' || YorN[0]=='Y') Subs=1; else Subs=0;
Fp=open(FileName,O_RDONLY);
fstat(Fp,&StatBuf);
FileBytes=StatBuf.st_size; FileWds=FileBytes/2;
printf("%s contains %ld words (%ld bytes)\n",FileName,FileWds,FileBytes);
LinesPerScreen=22; WrDsk=0;
printf("Words/Line for display(max 8)?"); scanf("%d",&WdsPerLine);
if (WdsPerLine>8) WdsPerLine=8;
printf("Do you also want the output in a file?(y/n)");
scanf("%s",Dsk);
if (Dsk[0]=='y' || Dsk[0]=='Y')
   {
   printf("Output File Name?"); scanf("%s",OutFileName);
   WrDsk=1; Dfp=fopen(OutFileName,"w");
   }
Done=0; Loc=0; AtEOF=0;
while (!Done)
    {
    BytesRequired=2*WdsPerLine*LinesPerScreen;
    ZeroBuf(Data,BytesRequired/2);
    Nt=read(Fp,Data,BytesRequired); Lines=LinesPerScreen;
    if (Nt<BytesRequired) { AtEOF=1; Lines=Nt/2/WdsPerLine+1; }
    i=0;
    for (Line=0;Line<Lines;Line++)
        {
        if (WdsPerLine>1)
           {
           Loc2=Loc+WdsPerLine-1;
           if (Loc2>FileWds-1) Loc2=FileWds-1;
           printf("(%5d-%5d):",Loc,Loc2);
           if (WrDsk) fprintf(Dfp,"(%5d-%5d):",Loc,Loc+WdsPerLine-1);
           }
        else 
           {
           printf("%5d",Loc);
           if (WrDsk) fprintf(Dfp,"%5d",Loc);
           }
        for (Word=0;Word<WdsPerLine;Word++) 
            {
            if (Subs) 
               {
               if (Data[i]==16708) printf("    DA"); else if (Data[i]==21328) printf(" PS");
               else printf("%6d",Data[i]);
               }
            else  printf("%6d",Data[i]);
            if (WrDsk) fprintf(Dfp,"%6d",Data[i]);
            i++; Loc++;
            if (Loc>FileWds-1) break;
            }
        printf("\n");
        if (WrDsk) fprintf(Dfp,"\n");
        }
    if (AtEOF) printf("We have reached EOF\n");
    printf("Command?(c=Continue, m=Move to Position, e=Exit)"); 
    scanf("%s",Key);
    switch (tolower(Key[0]))
       {
       case 'c': if (AtEOF) Done=1; break;
       case 'm': AtEOF=0;
                 printf("Position:"); scanf("%ld",&Loc);
                 if (Loc>(FileWds-WdsPerLine)) Loc=FileWds-WdsPerLine;
                 Offset=2*Loc; lseek(Fp,Offset,SEEK_SET); break;
       case 'e': Done=1; break;
       default : printf("What?\n"); break; 
       }
    }
close(Fp);
if (WrDsk) fclose(Dfp);  
}
/*-------------------------------------------------------------------------*/
void ZeroBuf(unsigned short *Data,int N)
{
int i;

for (i=0;i<N;i++) Data[i]=0;
}
/*-------------------------------------------------------------------------*/
