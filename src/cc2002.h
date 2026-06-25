#define isa_base            0x300	
#define NONE                    0
#define ADDRESS_ERROR           1
#define INTERRUPT_ERROR         2
#define TRANSACTION_ERROR       3
#define TIMEOUT            300000
enum IocCode {
             IOC_OPEN = 0,
             IOC_CLOSE = -1,
             IOC_VTRANSPD_USER=0x8000,               //Codes may be reserved upto 32K
             IOC_VTRANSPD_INIT1,                     //init address and reset crate 1
             IOC_VTRANSPD_INIT2,                     //init address and reset crate 2
             IOC_VTRANSPD_SERVE1,
             IOC_VTRANSPD_SERVE2,
             IOC_VTRANSPD_WRITE1,                        //Set to write on first crate
             IOC_VTRANSPD_WRITE2,                       //Set to write on second crate
             IOC_VTRANSPD_READ,                                  //Actually do nothing
             IOC_VTRANSPD_TEST,   //ioctl will return error code of previous operation
             IOC_VTRANSPD_PEEK1,                          //Peek Transputer in Crate 1
             IOC_VTRANSPD_POKE1,                          //Poke Transputer in Crate 1
             IOC_VTRANSPD_PEEK2,                          //Peek Transputer in Crate 2
             IOC_VTRANSPD_POKE2                           //Poke Transputer in Crate 2
             };
typedef	struct { int error; int a,b,c,d,e; } StatusInfo;

//Commands for acqisition init and acqisition control
#define INIT_SETUP       1
#define START_ACQN       2
#define DATA_XFER        3
#define STOP_ACQN        4
#define PAUSE_ACQN       5
#define RESUME_ACQN      6
#define PROBE_XPUTER     9
#define TEST_DATA_XFER  10

enum command {SETZ, SETI, CLRI, CLRC, NAF, ENDI, DLY, DATA};                      //Enumeration of camac command to mtmps
typedef struct {unsigned short PkAddr; unsigned short PkVal; } Initinfo;                 //For peek and poke testing only
