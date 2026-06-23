/* File from ET trimmed down heavily */ 
#define LPBUFSIZE		96000   //32000*NoOfCrates
#define DATABUFSIZE		(LPBUFSIZE + LPBUFSIZE/16)
enum blocktype { event,start,stop,pauseNSC,resume,hgram,scaler,user,names,calibs,show,error,end_of_file,datarate };
typedef struct dataheader
        { enum blocktype block; int unitsize; int number_of_units; int compstatus; int size_in_bytes; }
        dataheader,*dataheadptr;
typedef struct data { dataheader header; short data[DATABUFSIZE]; int empty; int size; } datapack,*datapackptr;
