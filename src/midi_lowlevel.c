#include "midi_lowlevel.h"
#include "leds.h" 

char midiBuffer[MIDI_BUF_SIZE];
int MIDIlastIndex = 0;
int MIDITimeToProcessBuffer = 0;

static void UART8_Enable_Rx(void)
{
    GPIO_InitTypeDef  GPIO_USART_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    DMA_InitTypeDef   DMA_InitStruct;

    /* Enable Clocks */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART8, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

    /* Set up GPIO for alternate function */
    GPIO_PinAFConfig(GPIOE,GPIO_PinSource0,GPIO_AF_UART8);

    /* Configure GPIO to transmit */
    GPIO_USART_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_USART_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_USART_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_USART_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOE, &GPIO_USART_InitStruct);

    /* Configure USART */
    USART_InitStruct.USART_BaudRate = MIDI_BAUD_RATE;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_Mode = USART_Mode_Rx;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(UART8, &USART_InitStruct);

    /* Enable UART */
    USART_Cmd(UART8, ENABLE);

    /* no DMA interrupts */
//    DMA_ITConfig(DMA1_Stream6, DMA_IT_TC, ENABLE);
//    NVIC_EnableIRQ(DMA1_Stream6_IRQn);


    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_5;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(UART8->DR));
    DMA_InitStruct.DMA_Memory0BaseAddr = (uint32_t)midiBuffer;
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStruct.DMA_BufferSize = MIDI_BUF_SIZE;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream6, &DMA_InitStruct);
    DMA_Cmd(DMA1_Stream6, ENABLE);

    /* Connect UART to DMA */
    USART_DMACmd(UART8, USART_DMAReq_Rx, ENABLE);

    /* wait for DMA to be enabled */
    while (DMA_GetCmdStatus(DMA1_Stream6) != ENABLE);

}

static void Timer_setup(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimerInitStruct;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimerInitStruct.TIM_Prescaler = 0;
    TIM_TimerInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimerInitStruct.TIM_Period = MIDI_TIMER_PERIOD_TICKS; /* Tick once per millisecond */
    TIM_TimerInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &TIM_TimerInitStruct);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void MIDI_low_level_setup(void)
{
    UART8_Enable_Rx();
    Timer_setup();
}
    
void MIDI_process_buffer(void)
{
    if (!MIDITimeToProcessBuffer) {
        return;
    }
    MIDITimeToProcessBuffer = 0;
    int length = MIDI_DMA_STRUCT->NDTR;
    int topIndex = MIDI_BUF_SIZE - length;
    if (topIndex < MIDIlastIndex) {
        topIndex += MIDI_BUF_SIZE;
    }
    int numItems = topIndex - MIDIlastIndex;
    while (numItems--) {
        MIDI_process_byte(midiBuffer[MIDIlastIndex]);
        /* increase last index so the function starts at the correct point next interrupt */
        MIDIlastIndex = (MIDIlastIndex + 1) % MIDI_BUF_SIZE; 
    }
}

void DMA1_Stream6_IRQHandler (void)
{
    if (DMA_GetITStatus(DMA1_Stream6, DMA_IT_TCIF6)
            && DMA_GetFlagStatus(DMA1_Stream6, DMA_FLAG_TCIF6)) {
//        LEDs_greenToggle();
        DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);
        DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6);
    }
    NVIC_ClearPendingIRQ(DMA1_Stream6_IRQn);
}

void TIM2_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    if (TIM_GetITStatus(TIM2, TIM_IT_Update)) {
        MIDI_TIMER_INTERRUPT();
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}
