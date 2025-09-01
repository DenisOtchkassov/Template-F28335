
#include "DSP28x_Project.h"   


//User Knobs
#define EPWM1_TBPRD 2000u      
#define EPWM1_CMPA 1000u      

//ADC channel selections 
#define ADC_SEQ1_CONV00 3u
#define ADC_SEQ1_CONV01 2u

//Prototypes
static void init_gpio_scope_marker(void);
static void init_epwm1_pwm_and_soca(void);
static void init_adc_epwm_triggered(void);
__interrupt void adc_isr(void);

//Globals
volatile Uint16 ConversionCount = 0;
volatile Uint16 Voltage1[10];
volatile Uint16 Voltage2[10];

float test = 0;

void main(void)
{

    InitSysCtrl();
    init_gpio_scope_marker();

    //Disable interrupts and initialize the PIE control and vector table.
    DINT;
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

    //Remap PIE vectors we use 
    EALLOW;
    PieVectTable.ADCINT = &adc_isr;     
    EDIS;

    //Initialize peripherals we use:
    init_epwm1_pwm_and_soca();
    init_adc_epwm_triggered();

    //Enable interrupts (PIE + CPU)
    PieCtrlRegs.PIEIER1.bit.INTx6 = 1;
    IER |= M_INT1;
    EINT;
    ERTM;

    //Idle loop
    for(;;)
    {
        
        asm(" NOP");
    }
}

//GPIO6 scope marker
static void init_gpio_scope_marker(void)
{
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 0;   
    GpioCtrlRegs.GPADIR.bit.GPIO6 = 1;   
    EDIS;
}

//ePWM1 setup
static void init_epwm1_pwm_and_soca(void)
{
    
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;   
    EDIS;

    //Route EPWM1 pins
    InitEPwm1Gpio();

    
    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UP;   
    EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;    
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = TB_DIV2;       
    EPwm1Regs.TBCTL.bit.CLKDIV = TB_DIV2;       
    EPwm1Regs.TBPRD = EPWM1_TBPRD;   
    EPwm1Regs.TBCTR = 0x0000;        

    //Shadowing & loading
    EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;  
    EPwm1Regs.CMPA.half.CMPA       = EPWM1_CMPA;   

    //Action-Qualifier
    EPwm1Regs.AQCTLA.bit.ZRO = AQ_SET;   
    EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR; 

    //Generate SOCA on CMPA up-countt
    EPwm1Regs.ETSEL.bit.SOCASEL = ET_CTRU_CMPA; 
    EPwm1Regs.ETPS.bit.SOCAPRD = ET_1ST;
    EPwm1Regs.ETSEL.bit.SOCAEN = 1;

    //Start TB clocks
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;
}

//ADC setup
static void init_adc_epwm_triggered(void)
{
    
    EALLOW;
    #if (CPU_FRQ_150MHZ)
        #define ADC_MODCLK 0x3     
    #elif (CPU_FRQ_100MHZ)
        #define ADC_MODCLK 0x2
    #else
        #define ADC_MODCLK 0x3    
    #endif
        SysCtrlRegs.HISPCP.all = ADC_MODCLK; 
    EDIS;

    InitAdc();                  

    //Two conversions on SEQ1
    AdcRegs.ADCMAXCONV.all = 0x0001;                 
    AdcRegs.ADCCHSELSEQ1.bit.CONV00 = ADC_SEQ1_CONV00;
    AdcRegs.ADCCHSELSEQ1.bit.CONV01 = ADC_SEQ1_CONV01;

    //Trigger SEQ1 from ePWM1 SOCA and enable interrupt every EOS
    AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1;
    AdcRegs.ADCTRL2.bit.INT_ENA_SEQ1 = 1;
}

//ADC ISR
__interrupt void adc_isr(void)
{
    GpioDataRegs.GPATOGGLE.bit.GPIO6 = 1;

    Voltage1[ConversionCount] = AdcRegs.ADCRESULT0 >> 4;
    Voltage2[ConversionCount] = AdcRegs.ADCRESULT1 >> 4;

    if (ConversionCount == 9) ConversionCount = 0;
    else ConversionCount++;

    //Reset SEQ1 and clear interrupt flags
    AdcRegs.ADCTRL2.bit.RST_SEQ1 = 1;
    AdcRegs.ADCST.bit.INT_SEQ1_CLR = 1;

    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
