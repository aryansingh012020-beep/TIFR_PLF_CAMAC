#include <stdio.h>
#include <stdlib.h>

int main(void)
{
int Bytes;
unsigned short *Spec;
int I,IMax,Size;

while (1)
   {
   printf("Size?(-1 terminates)");
   scanf("%d",&Size); if (Size<0) break;
   IMax=Size*Size; Bytes=2*IMax;
   printf("IMax=%d (%d bytes)\n",IMax,Bytes);
   if ((Spec=malloc((size_t)Bytes)) == NULL) printf("Malloc failed!\n"); 
   else
      {
      IMax=Bytes/2;
      for (I=0;I<IMax;I++) Spec[I]=205; Spec[IMax-1]=137;
      for (I=0;I<IMax;I+=IMax/5) printf("Spec[%d]=%d\n",I,Spec[I]);
      printf("Spec[%d]=%d\n",IMax-1,Spec[IMax-1]);
      free(Spec);
      }
   }
}
