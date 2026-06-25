!user_INGA.F 20-04-08. Gain matching and Addbacks are done here
!To correct gain drifts there should be 1 cal file for each run 
!located in the cal directory with name run_name.cal
!Note the 1st line of USERSUB: no of clovers and threshold (KeV)
!With NCLOVERS=24, 96 gain calibrated peseudos are generated
!Followed by 24 Addbacks. Define these as USER type pseudos
#include "constants.h"
!-----------------------------------------------------------------------
SUBROUTINE BSTART()
END
!-----------------------------------------------------------------------
SUBROUTINE FSTART(RUN_NAME)
COMMON/CALIB/A(150),B(150),C(150),D(150)
CHARACTER(*) RUN_NAME
CHARACTER CAL_FILE*60,UNITS*10

CAL_FILE='cal/'//RUN_NAME//'.cal'
WRITE(6,"('Run Name ',A,' Calibration File:',A)")RUN_NAME,CAL_FILE
OPEN(1,FILE=CAL_FILE,IOSTAT=IERR,STATUS='OLD')
IF (IERR.NE.0) THEN 
   WRITE(6,*)' File not found:',CAL_FILE
ELSE
  WRITE(6,*)' File opened:',CAL_FILE
  READ(1,"(///)")
  DO I=1,113; READ(1,*)J,A(I),B(I),C(I),D(I),UNITS; ENDDO
  CLOSE(1)
ENDIF
END
!-----------------------------------------------------------------------
FUNCTION LCLIP(X,LIMITLO,LIMITHI)         !NOTE: Please clip all pseudos
I=NINT(X)                         !Convert real value to nearest integer
IF (I.LT.LIMITLO) I=LIMITLO     !Ensure we are not below the lower limit
IF (I.GT.LIMITHI) I=LIMITHI     !Ensure we are not above the upper limit
LCLIP=I
END
!-----------------------------------------------------------------------
SUBROUTINE USERSUB(NADC,NSEUDO,ADC)
INTEGER*2 ADC(MAX_TOTAL_PAR)
COMMON/CALIB/A(150),B(150),C(150),D(150)
INTEGER CLOV_PARA(24)
DATA CLOV_PARA/3,8,13,18,23,28,33,38,44,49,54,59,64,69,74,79,85,90,95,100,105,110,115,120/

NCLOVERS=22; THRESH=50.                              !THRESH is in KeV
IV=0
DO ICLOV=1,NCLOVERS
  IF ((ICLOV.NE.4).AND.(ICLOV.NE.6).AND.(ICLOV.NE.9).AND.(ICLOV.NE.15)) THEN
     IV=IV+1
     Z=O
     DO ICRYS=1,4
        M=4*(ICLOV-1)+ICRYS
        N=CLOV_PARA(ICLOV)+ICRYS-1
        X=ADC(N)+RAND()
        Y=A(N)+B(N)*X+C(N)*X*X+D(N)*SQRT(X)
        IF (Y.GT.THRESH) Z=Z+Y
     ENDDO
  ADC(NADC+IV)=LCLIP(Z,0,4095)                !Add backs
  ENDIF
ENDDO
END
!-----------------------------------------------------------------------
SUBROUTINE FSTOP()
RETURN
END
!-----------------------------------------------------------------------
SUBROUTINE BSTOP()
RETURN
END
!-----------------------------------------------------------------------
