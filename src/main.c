/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_conf.h" 
#include <stdlib.h>
#include <string.h> 
#include <math.h> 
#include "stm32f4xx.h" 
#include "leds.h" 
#include "main.h" 
#include "i2s_setup.h" 
#include "midi_lowlevel.h"

/* mmmidi includes */
#include "mm_midimsgbuilder.h"
#include "mm_midirouter_standard.h" 

/* mm_dsp includes */
#include "mm_bus.h"
#include "mm_sample.h"
#include "mm_sampleplayer.h"
#include "mm_sigchain.h"
#include "mm_sigproc.h"
#include "mm_wavtab.h"
#include "mm_sigconst.h"
#include "wavetables.h" 

#define MIDI_BOTTOM_NOTE 48
#define MIDI_TOP_NOTE    84
#define MIDI_NUM_NOTES   (MIDI_TOP_NOTE - MIDI_BOTTOM_NOTE)

#define BUS_NUM_CHANS 1
#define BUS_BLOCK_SIZE (CODEC_DMA_BUF_LEN / CODEC_NUM_CHANNELS)

//extern MMSample GrandPianoFileDataStart;
//extern MMSample GrandPianoFileDataEnd;

static MIDIMsgBuilder_State_t lastState;
static MIDIMsgBuilder midiMsgBuilder;
static MIDI_Router_Standard midiRouter;

MMSamplePlayerSigProc *spsps[MIDI_NUM_NOTES];

MMSample waveTableMidiNum = 59;

void MIDI_note_on_do(void *data, MIDIMsg *msg)
{
    if ((msg->data[1] < MIDI_TOP_NOTE) && (msg->data[1] >= MIDI_BOTTOM_NOTE)) {
        MMSamplePlayerSigProc *sp = 
            ((MMSamplePlayerSigProc**)data)[msg->data[1] - MIDI_BOTTOM_NOTE];
        ((MMSigProc*)sp)->state = MMSigProc_State_PLAYING;
        ((MMSamplePlayerSigProc*)sp)->index = 0;
        ((MMSamplePlayerSigProc*)sp)->rate = pow(2.,
            ((msg->data[1] - waveTableMidiNum) / 12.));
    }
    MIDIMsg_free(msg);
}

void MIDI_note_off_do(void *data, MIDIMsg *msg)
{
    if ((msg->data[1] < MIDI_TOP_NOTE) && (msg->data[1] >= MIDI_BOTTOM_NOTE)) {
        MMSamplePlayerSigProc *sp = 
            ((MMSamplePlayerSigProc**)data)[msg->data[1] - MIDI_BOTTOM_NOTE];
        ((MMSigProc*)sp)->state = MMSigProc_State_DONE;
    }
    MIDIMsg_free(msg);
}

int main(void)
{
    MMSample *sampleFileDataStart = WaveTable;
    MMSample *sampleFileDataEnd   = WaveTable + WAVTABLE_LENGTH_SAMPLES;
    size_t i;


    /* Enable LEDs so we can toggle them */
    LEDs_Init();

    codecDmaTxPtr = NULL;
    codecDmaRxPtr = NULL;
    
    /* Enable codec */
    i2s_dma_full_duplex_setup();

    /* Sample to write data to */
//    MMSample sample;

    /* The bus the signal chain is reading/writing */
    MMBus *outBus = MMBus_new(BUS_BLOCK_SIZE,BUS_NUM_CHANS);

    /* a signal chain to put the signal processors into */
    MMSigChain sigChain;
    MMSigChain_init(&sigChain);

    /* A constant that zeros the bus each iteration */
    MMSigConst sigConst;
    MMSigConst_init(&sigConst);
    MMSigConst_setOutBus(&sigConst,outBus);

    /* A sample player */
    MMSamplePlayer samplePlayer;
    MMSamplePlayer_init(&samplePlayer);
    samplePlayer.outBus = outBus;
    /* puts its place holder at the top of the sig chain */
    MMSigProc_insertAfter(&sigChain.sigProcs, &samplePlayer.placeHolder);

    /* put sig constant at the top of the sig chain */
    MMSigProc_insertBefore(&samplePlayer.placeHolder,&sigConst);

    /* initialize wavetables */
    waveTableMidiNum = WaveTable_midiNumber();
    WaveTable_init();

    /* Give access to samples of sound as wave table */
    MMWavTab samples;
    samples.data = sampleFileDataStart;
    samples.length = sampleFileDataEnd - sampleFileDataStart;

    /* Enable MIDI hardware */
    MIDI_low_level_setup();

    /* Initialize MIDI Message builder */
    MIDIMsgBuilder_init(&midiMsgBuilder);

    /* set up the MIDI router to trigger samples */
    MIDI_Router_Standard_init(&midiRouter);
    for (i = 0; i < MIDI_NUM_NOTES; i++) {
        spsps[i] = MMSamplePlayerSigProc_new();
        MMSamplePlayerSigProc_init(spsps[i]);
        spsps[i]->samples = &samples;
        spsps[i]->parent = &samplePlayer;
        spsps[i]->loop = 1;
        ((MMSigProc*)spsps[i])->state = MMSigProc_State_DONE;
        /* insert in signal chain */
        MMSigProc_insertAfter(&samplePlayer.placeHolder, spsps[i]);
    }
    MIDI_Router_addCB(&midiRouter.router, MIDIMSG_NOTE_ON, 1, MIDI_note_on_do, spsps);
    MIDI_Router_addCB(&midiRouter.router, MIDIMSG_NOTE_OFF, 1, MIDI_note_off_do, spsps);

    size_t curWaveTabIdx = 0;

    while (1) {
        while (!(codecDmaTxPtr && codecDmaRxPtr));/* wait for request to fill with data */
        MIDI_process_buffer(); /* process MIDI at most every audio block */
        MMSigProc_tick(&sigChain);
        size_t i;
        for (i = 0; i < CODEC_DMA_BUF_LEN; i += 2) {
            /* We know that outBus has the same block size as DMA and is only 1
             * channel */
//            codecDmaTxPtr[i] = FLOAT_TO_INT16(WaveTable[curWaveTabIdx]);
//            codecDmaTxPtr[i+1] = FLOAT_TO_INT16(outBus->data[curWaveTabIdx]);
            codecDmaTxPtr[i] = FLOAT_TO_INT16(outBus->data[i/2] * 0.1);
            codecDmaTxPtr[i+1] = FLOAT_TO_INT16(outBus->data[i/2] * 0.1);
//            curWaveTabIdx += 100;
//            if (curWaveTabIdx >= WAVTABLE_LENGTH_SAMPLES) {
//                curWaveTabIdx -= WAVTABLE_LENGTH_SAMPLES;
//            }
        }
        codecDmaTxPtr = NULL;
        codecDmaRxPtr = NULL;
    }
}

void do_stuff_with_msg(MIDIMsg *msg)
{
    MIDI_Router_handleMsg(&midiRouter.router, msg);
}

void MIDI_process_byte(char byte)
{
    switch (MIDIMsgBuilder_update(&midiMsgBuilder,byte)) {
        case MIDIMsgBuilder_State_WAIT_STATUS:
            break;
        case MIDIMsgBuilder_State_WAIT_DATA:
            break;
        case MIDIMsgBuilder_State_COMPLETE:
            do_stuff_with_msg(midiMsgBuilder.msg);
            MIDIMsgBuilder_init(&midiMsgBuilder); /* reset builder */
            break;
        default:
            break;
    }
    lastState = midiMsgBuilder.state;
}
