#include "stm32f4xx.h"
#include <math.h>
#include "i2s_setup.h" 
#include "error_sig.h" 

/* Where data to be transferred to CODEC reside */
int16_t codecDmaTxBuf[CODEC_DMA_BUF_LEN * 2];
/* Where data from CODEC reside */
int16_t codecDmaRxBuf[CODEC_DMA_BUF_LEN * 2];
/* Which half of transmit buffer we are at currently */
int16_t *codecDmaTxPtr = NULL;
/* Which half of receive buffer we are at currently */
int16_t *codecDmaRxPtr = NULL;
/* Flag to check if processing is finished */
int processingDone = 1;
int numBufferUnderruns = 0;

/* This assumes i2s is configured as master */
int i2s_clock_setup(uint32_t sr)
{
    /* Disable PLLI2S */
    RCC->CR &= ((uint32_t)(~RCC_CR_PLLI2SON));

    /* PLLI2S clock used as I2S clock source */
    RCC->CFGR &= ~RCC_CFGR_I2SSRC;

    /* Configure PLLI2S */
    /* see stm32f4 reference manual, p. 894 */
    switch(sr) {
        case 44100:
            RCC->PLLI2SCFGR = (271 << 6) | (2 << 28);
            SPI3->I2SPR     = ((0x2 << 8) | 0x6); // 44.1Khz
            I2S3ext->I2SPR  = ((0x2 << 8) | 0x6);
            break;
        case 16000:
            RCC->PLLI2SCFGR = (213 << 6) | (2 << 28);
            SPI3->I2SPR     = ((0x2 << 8) | 13); // 16KHz
            I2S3ext->I2SPR  = ((0x2 << 8) | 13);
            break;
        case 32000:
            RCC->PLLI2SCFGR = (213 << 6) | (2 << 28);
            SPI3->I2SPR     = ((0x3 << 8) | 0x6); // 32Khz
            I2S3ext->I2SPR  = ((0x3 << 8) | 0x6);
            break;
        default:
            return -1; /* bad sampling rate */
    }

    /* Enable PLLI2S */
    RCC->CR |= ((uint32_t)RCC_CR_PLLI2SON);

    /* start indicator light incase program gets stuck here */
    error_sig_started_waiting();

    /* Wait till PLLI2S is ready */
    while((RCC->CR & RCC_CR_PLLI2SRDY) == 0);

    /* pll is ready, turn off error indication */
    error_sig_done_waiting();

    return 0;
}

/* takes desired sampling rate, throws error if bad sampling rate */
void i2s_dma_full_duplex_setup(uint32_t sr)
{
    /* Turn on GPIO clock for I2S3 pins */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    /* Configure GPIO */
    /* Configure PA15 to Alternate Function */
    GPIOA->MODER &= ~(0x3 << 30);
    GPIOA->MODER |= (0x2 << 30);
    /* Configure PC7, PC10-12 to Alternate Function */
    GPIOC->MODER &= ~((0x3 << 20) | (0x3 << 22) | (0x3 << 24) | (0x3 << 14));
    GPIOC->MODER |= (0x2 << 20) | (0x2 << 22) | (0x2 << 24) | (0x2 << 14);
    /* Set pins to high speed */
    GPIOA->OSPEEDR |= (0x3 << 30);
    GPIOC->OSPEEDR |= (0x3 << 20) | (0x3 << 22) | (0x3 << 24) | (0x3 << 14);
    /* Pins have no-pull up nor pull-down */
    GPIOA->PUPDR &= ~(0x3 << 30);
    GPIOC->PUPDR &= ~((0x3 << 20) | (0x3 << 22) | (0x3 << 24) | (0x3 << 14));
    /* A15 Alternate function 6 */
    GPIOA->AFR[1] &= ~(0xf << 28);
    GPIOA->AFR[1] |= (0x6 << 28);
    /* C7 Alternate function 6 */
    GPIOC->AFR[0] &= ~(0xf << 28);
    GPIOC->AFR[0] |= (0x6 << 28);
    /* C10,12, Alternate function 6, C11 alternate function 5 */
    GPIOC->AFR[1] &= ~((0xf << 8) | (0xf << 12) | (0xf << 16));
    GPIOC->AFR[1] |= ((0x6 << 8) | (0x5 << 12) | (0x6 << 16));

    /* Turn on DMA1 clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    /* Set up memory to peripheral DMA */
    /* Disable DMA peripheral */
    if (DMA1_Stream5->CR & DMA_SxCR_EN) {
        DMA1_Stream5->CR &= ~(DMA_SxCR_EN);
    }
    /* Wait until free */
    while (DMA1_Stream5->CR & DMA_SxCR_EN);
    /* Set peripheral address to SPI3 data register */
    DMA1_Stream5->PAR = (uint32_t)&(SPI3->DR);
    /* Set memory address to transmit buffer */
    DMA1_Stream5->M0AR = (uint32_t)codecDmaTxBuf;
    /* Inform DMA peripheral of buffer length. This is two times the defined
     * value because we trigger on HALF and FULL transfer */
    DMA1_Stream5->NDTR = (uint32_t)(CODEC_DMA_BUF_LEN * 2);
    /* Set up DMA control register: */
    /* Channel 0 */
    DMA1_Stream5->CR &= ~DMA_SxCR_CHSEL;
    /* Priority HIGH */
    DMA1_Stream5->CR &= ~DMA_SxCR_PL;
    DMA1_Stream5->CR |= 0x2 << 16;
    /* PBURST Single Transfer */
    DMA1_Stream5->CR &= ~DMA_SxCR_PBURST;
    /* MBURST Single Transfer */
    DMA1_Stream5->CR &= ~DMA_SxCR_MBURST;
    /* No Double Buffer Mode (we do this ourselves with the HALF and FULL
     * transfer) */
    DMA1_Stream5->CR &= ~DMA_SxCR_DBM;
    /* Memory datum size 16-bit */
    DMA1_Stream5->CR &= ~DMA_SxCR_MSIZE;
    DMA1_Stream5->CR |= 0x1 << 13;
    /* Peripheral datum size 16-bit */
    DMA1_Stream5->CR &= ~DMA_SxCR_PSIZE;
    DMA1_Stream5->CR |= 0x1 << 11;
    /* Memory incremented after each transfer */
    DMA1_Stream5->CR |= DMA_SxCR_MINC;
    /* No peripheral address increment */
    DMA1_Stream5->CR &= ~DMA_SxCR_PINC;
    /* Circular buffer mode */
    DMA1_Stream5->CR |= DMA_SxCR_CIRC;
    /* Memory to peripheral mode (this is the transmitting peripheral) */
    DMA1_Stream5->CR &= ~DMA_SxCR_DIR;
    DMA1_Stream5->CR |= 0x1 << 6;
    /* DMA is the flow controller (DMA will keep transferring items from memory
     * to peripheral until disabled) */
    DMA1_Stream5->CR &= ~DMA_SxCR_PFCTRL;
    /* Enable interrupt on transfer complete */
    DMA1_Stream5->CR |= DMA_SxCR_TCIE;
    /* Enable interrupt on transfer half complete */
    DMA1_Stream5->CR |= DMA_SxCR_HTIE;
    /* No interrupt on transfer error */
    DMA1_Stream5->CR &= ~DMA_SxCR_TEIE;
    /* No interrupt on direct mode error */
    DMA1_Stream5->CR &= ~DMA_SxCR_DMEIE;

    /* Set up peripheral to memory DMA */
    /* Disable DMA peripheral */
    if (DMA1_Stream0->CR & DMA_SxCR_EN) {
        DMA1_Stream0->CR &= ~(DMA_SxCR_EN);
    }
    /* Wait until free */
    while (DMA1_Stream0->CR & DMA_SxCR_EN);
    /* Set peripheral address to I2S3_ext data register */
    DMA1_Stream0->PAR = (uint32_t)&(I2S3ext->DR);
    /* Set memory address to receive buffer */
    DMA1_Stream0->M0AR = (uint32_t)codecDmaRxBuf;
    /* Inform DMA peripheral of buffer length. This is two times the defined
     * value because we trigger on HALF and FULL transfer */
    DMA1_Stream0->NDTR = (uint32_t)(CODEC_DMA_BUF_LEN * 2);
    /* Set up DMA control register: */
    /* Channel 3 */
    DMA1_Stream0->CR &= ~DMA_SxCR_CHSEL;
    DMA1_Stream0->CR |= 0x3 << 25;
    /* Priority HIGH */
    DMA1_Stream0->CR &= ~DMA_SxCR_PL;
    DMA1_Stream0->CR |= 0x2 << 16;
    /* PBURST Single Transfer */
    DMA1_Stream0->CR &= ~DMA_SxCR_PBURST;
    /* MBURST Single Transfer */
    DMA1_Stream0->CR &= ~DMA_SxCR_MBURST;
    /* No Double Buffer Mode (we do this ourselves with the HALF and FULL
     * transfer) */
    DMA1_Stream0->CR &= ~DMA_SxCR_DBM;
    /* Memory datum size 16-bit */
    DMA1_Stream0->CR &= ~DMA_SxCR_MSIZE;
    DMA1_Stream0->CR |= 0x1 << 13;
    /* Peripheral datum size 16-bit */
    DMA1_Stream0->CR &= ~DMA_SxCR_PSIZE;
    DMA1_Stream0->CR |= 0x1 << 11;
    /* Memory incremented after each transfer */
    DMA1_Stream0->CR |= DMA_SxCR_MINC;
    /* No peripheral address increment */
    DMA1_Stream0->CR &= ~DMA_SxCR_PINC;
    /* Circular buffer mode */
    DMA1_Stream0->CR |= DMA_SxCR_CIRC;
    /* Peripheral to memory mode (this is the receiving peripheral) */
    DMA1_Stream0->CR &= ~DMA_SxCR_DIR;
    /* DMA is the flow controller (DMA will keep transferring items from
     * peripheral to memory until disabled) */
    DMA1_Stream0->CR &= ~DMA_SxCR_PFCTRL;
    /* Enable interrupt on transfer complete */
    DMA1_Stream0->CR |= DMA_SxCR_TCIE;
    /* Enable interrupt on transfer half complete */
    DMA1_Stream0->CR |= DMA_SxCR_HTIE;
    /* No interrupt on transfer error */
    DMA1_Stream0->CR &= ~DMA_SxCR_TEIE;
    /* No interrupt on direct mode error */
    DMA1_Stream0->CR &= ~DMA_SxCR_DMEIE;

    /* Turn on I2S3 clock (SPI3) */
    RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
    /* set up clock dividers for desired sampling rate */
    if (i2s_clock_setup(sr)) {
        /* bad sampling rate, throw error and wait here */
        error_sig_started_waiting();
        while(1);
    }
    /* CKPOL = 0, I2SMOD = 1, I2SEN = 0 (don't enable yet), I2SSTD = 00
     * (Phillips), DATLEN = 00 (16-bit), CHLEN = 0 (16-bit) I2SCFGR = 10 (Master
     * transmit) */
    SPI3->I2SCFGR = 0xa00;
    /* TXDMAEN = 1 (Transmit buffer empty DMA request enable), other bits off */
    SPI3->CR2 = SPI_CR2_TXDMAEN ;
    /* Set up duplex instance the same as SPI3, except configure as slave
     * receive and trigger interrupt when receive buffer full */
    /* same as above but I2SCFG = 01 (slave receive) */
    I2S3ext->I2SCFGR = 0x900;
    /* RXDMAEN = 1 (Receive buffer not empty DMA request enable), other bits off */
    I2S3ext->CR2 = SPI_CR2_RXDMAEN ;

    /* Enable the DMA peripherals */
   
    /* clear possible Interrupt flags */
    DMA1->HIFCR |= 0x00000f80;
    DMA1_Stream5->CR |= DMA_SxCR_EN;
    /* clear possible Interrupt flags */
    DMA1->LIFCR &= 0x0000001f;
    DMA1_Stream0->CR |= DMA_SxCR_EN;

    /* Enable DMA interrupts */
    NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    /* Turn on I2S3 and its extended block */
    I2S3ext->I2SCFGR |= 0x400;
    SPI3->I2SCFGR |= 0x400;

    /* Wait for them to be enabled (to show they are ready) */
    while(!((DMA1_Stream5->CR & DMA_SxCR_EN) && (DMA1_Stream0->CR & DMA_SxCR_EN)));
}

void DMA1_Stream0_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(DMA1_Stream0_IRQn);
    /* If transfer complete on stream 0 (peripheral to memory), set current rx
     * pointer to half of the buffer */
    if (DMA1->LISR & DMA_LISR_TCIF0) {
        /* clear flag */
        DMA1->LIFCR = DMA_LIFCR_CTCIF0;
        codecDmaRxPtr = codecDmaRxBuf + CODEC_DMA_BUF_LEN;
    }
    /* If half of transfer complete on stream 0 (peripheral to memory), set current rx
     * pointer to beginning of the buffer */
    if (DMA1->LISR & DMA_LISR_HTIF0) {
        /* clear flag */
        DMA1->LIFCR = DMA_LIFCR_CHTIF0;
        codecDmaRxPtr = codecDmaRxBuf;
    }
}

void DMA1_Stream5_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
    /* If transfer complete on stream 5 (memory to peripheral), set current tx
     * pointer to half of the buffer */
    if (DMA1->HISR & DMA_HISR_TCIF5) {
        /* clear flag */
        DMA1->HIFCR = DMA_HIFCR_CTCIF5;
        if (processingDone) {
            processingDone = 0;
            codecDmaTxPtr = codecDmaTxBuf + CODEC_DMA_BUF_LEN;
        } else {
            numBufferUnderruns += 1;
        }
    }
    /* If half of transfer complete on stream 5 (memory to peripheral), set current tx
     * pointer to beginning of the buffer */
    if (DMA1->HISR & DMA_HISR_HTIF5) {
        /* clear flag */
        DMA1->HIFCR = DMA_HIFCR_CHTIF5;
        if (processingDone) {
            processingDone = 0;
            codecDmaTxPtr = codecDmaTxBuf;
        } else {
            numBufferUnderruns += 1;
        }
    }
}
