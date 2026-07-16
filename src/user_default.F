! (1) Edit user.F as required. A safe copy is kept as user_safe.F
! (2) Type make
#include "constants.h"
!-----------------------------------------------------------------------
SUBROUTINE BSTART()
! Initialization routine called before a batch of files is analysed 
! READ, WRITE and PAUSE statements are possible.
! This routine will not be called during online acquisition (see FSTART)
!COMMON/B/VAR1,VAR2,VAR3
! (you can change the common statement as required)

!WRITE(6,*)'Please enter the values of VAR1,VAR2,VAR3...'
!READ(5,*) VAR1,VAR2,VAR3
!WRITE(6,*) VAR1,VAR2,VAR3
!PAUSE

RETURN
END
!-----------------------------------------------------------------------
SUBROUTINE FSTART()
! Initialization routine called for every zls file
! Also called everytime acquisition is started
!COMMON/F/A,B,C

!WRITE(6,*) 'Acquisition Started'
RETURN
END
!-----------------------------------------------------------------------
FUNCTION LCLIP(X,LIMITLO,LIMITHI)         !NOTE: Please clip all pseudos
I=NINT(X)                         !Convert real value to nearest integer
IF (I.LT.LIMITLO) I=LIMITLO     !Ensure we are not below the lower limit
IF (I.GT.LIMITHI) I=LIMITHI     !Ensure we are not above the upper limit
LCLIP=I; RETURN 
END
!-----------------------------------------------------------------------
SUBROUTINE USERSUB(NADC,NSEUDO,ADC)
INTEGER*2 ADC(MAX_TOTAL_PAR)
! User routine to calculate extra pseudo parameters. The above 2 lines
! should not be modified. NADC=number of real ADCs,
! NSEUDO=total number of virtual ADCs including those defined here.
! ADC(1)..ADC(NADC) are real ADCs, do not change their values.
! ADC(NADC+1).. are virtual ADCs. Do not change values of standard pseudos.
! Define the values of the 'user' type pseudos here.
! Here is a simple example:
!COMMON/B/VAR1,VAR2,VAR3
!COMMON/F/A,B,C
!X=VAR1+VAR2*ADC(1)+VAR3*ADC(1)*ADC(2)
!ADC(NADC+1)=LCLIP(X,0,8191) ! Important

RETURN
END
!-----------------------------------------------------------------------
SUBROUTINE FSTOP()
! Called after the analysis of every zls file
! Also called everytime online analysis is stopped

!WRITE(6,*) 'Acquisition Stopped'
RETURN
END
!-----------------------------------------------------------------------
SUBROUTINE BSTOP()
! Called at the end of analysis of a batch of files 

RETURN
END
!-----------------------------------------------------------------------
