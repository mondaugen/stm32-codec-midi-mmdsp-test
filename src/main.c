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
#include "mm_trapenvedsampleplayer.h"
#include "mm_sigchain.h"
#include "mm_sigproc.h"
#include "mm_wavtab.h"
#include "mm_sigconst.h"
#include "wavetables.h" 

#define MIDI_BOTTOM_NOTE 48
#define MIDI_TOP_NOTE    60
#define MIDI_NUM_NOTES   (MIDI_TOP_NOTE - MIDI_BOTTOM_NOTE)

#define BUS_NUM_CHANS 1
#define BUS_BLOCK_SIZE (CODEC_DMA_BUF_LEN / CODEC_NUM_CHANNELS)

#define ATTACK_TIME 2 
#define RELEASE_TIME 2 

//extern MMSample GrandPianoFileDataStart;
//extern MMSample GrandPianoFileDataEnd;

static MIDIMsgBuilder_State_t lastState;
static MIDIMsgBuilder midiMsgBuilder;
static MIDI_Router_Standard midiRouter;

MMTrapEnvedSamplePlayer spsps[MIDI_NUM_NOTES];

void MIDI_note_on_do(void *data, MIDIMsg *msg)
{
    if ((msg->data[1] < MIDI_TOP_NOTE) && (msg->data[1] >= MIDI_BOTTOM_NOTE)) {
        MMTrapEnvedSamplePlayer *tesp = (MMTrapEnvedSamplePlayer*)data 
            + msg->data[1] - MIDI_BOTTOM_NOTE;
        MMEnvedSamplePlayer_getSamplePlayerSigProc(tesp).interp =
            MMInterpMethod_CUBIC;
        MMEnvedSamplePlayer_getSamplePlayerSigProc(tesp).index = 0;
        MMEnvedSamplePlayer_getSamplePlayerSigProc(tesp).rate =
            pow(2., (msg->data[1] - 69.) / 10.) * 440.0 / WAVTABLE_FREQ;
        MMSigProc_setState(
                &MMEnvedSamplePlayer_getSamplePlayerSigProc(tesp),
                MMSigProc_State_PLAYING);
        MMTrapezoidEnv_init(&MMTrapEnvedSamplePlayer_getTrapezoidEnv(tesp),
            0, (MMSample)msg->data[2] / 127., ATTACK_TIME, RELEASE_TIME);
        MMEnvelope_startAttack(&MMTrapEnvedSamplePlayer_getTrapezoidEnv(tesp));
    }
    MIDIMsg_free(msg);
}

void MIDI_note_off_do(void *data, MIDIMsg *msg)
{
    if ((msg->data[1] < MIDI_TOP_NOTE) && (msg->data[1] >= MIDI_BOTTOM_NOTE)) {
        MMTrapEnvedSamplePlayer *tesp = (MMTrapEnvedSamplePlayer*)data 
            + msg->data[1] - MIDI_BOTTOM_NOTE;
        MMEnvelope_startRelease(
                &MMTrapEnvedSamplePlayer_getTrapezoidEnv(tesp));
    }
    MIDIMsg_free(msg);
}

int main(void)
{
    MMSample *sampleFileDataStart = WaveTable;
    size_t i;


    /* Enable LEDs so we can toggle them */
    LEDs_Init();

    codecDmaTxPtr = NULL;
    codecDmaRxPtr = NULL;
    
    /* Enable codec */
    i2s_dma_full_duplex_setup();

    /* The bus the signal chain is reading/writing */
    MMBus *outBus = MMBus_new(BUS_BLOCK_SIZE,BUS_NUM_CHANS);

    /* a signal chain to put the signal processors into */
    MMSigChain sigChain;
    MMSigChain_init(&sigChain);

    /* A constant that zeros the bus each iteration */
    MMSigConst sigConst;
    MMSigConst_init(&sigConst,outBus,0,MMSigConst_doSum_FALSE);

    /* put sig constant at the top of the sig chain */
    MMSigProc_insertAfter(&sigChain.sigProcs,&sigConst);

    /* initialize wavetables */
    WaveTable_init();

    /* Give access to samples of sound as wave table */
    MMWavTab samples;
    samples.data = sampleFileDataStart;
    samples.length = WAVTABLE_LENGTH_SAMPLES; // sampleFileDataEnd - sampleFileDataStart;

    /* Enable MIDI hardware */
    MIDI_low_level_setup();

    /* Initialize MIDI Message builder */
    MIDIMsgBuilder_init(&midiMsgBuilder);

    /* set up the MIDI router to trigger samples */
    MIDI_Router_Standard_init(&midiRouter);
    for (i = 0; i < MIDI_NUM_NOTES; i++) {
        MMTrapEnvedSamplePlayer_init(&spsps[i], outBus, BUS_BLOCK_SIZE, 
                1. / (MMSample)CODEC_SAMPLE_RATE);
        MMEnvedSamplePlayer_getSamplePlayerSigProc(&spsps[i]).samples = &samples;
        MMEnvedSamplePlayer_getSamplePlayerSigProc(&spsps[i]).loop = 1;
        MMSigProc_setState(
            &MMEnvedSamplePlayer_getSamplePlayerSigProc(&spsps[i]),
            MMSigProc_State_DONE);
        MMSigProc_setDoneAction(
            &MMEnvedSamplePlayer_getSamplePlayerSigProc(&spsps[i]),
            MMSigProc_DoneAction_NONE);
        MMTrapezoidEnv_init(&MMTrapEnvedSamplePlayer_getTrapezoidEnv(&spsps[i]),
            0, 1, ATTACK_TIME, RELEASE_TIME);
        /* insert in signal chain after sig const*/
        MMSigProc_insertAfter(&sigConst, &spsps[i]);
    }
    MIDI_Router_addCB(&midiRouter.router, MIDIMSG_NOTE_ON, 1, MIDI_note_on_do, spsps);
    MIDI_Router_addCB(&midiRouter.router, MIDIMSG_NOTE_OFF, 1, MIDI_note_off_do, spsps);

    while (1) {
        while (!(codecDmaTxPtr && codecDmaRxPtr));
        MIDI_process_buffer(); /* process MIDI at most every audio block */
        MMSigProc_tick(&sigChain);
        size_t i;
        for (i = 0; i < CODEC_DMA_BUF_LEN; i += 2) {
            codecDmaTxPtr[i] = FLOAT_TO_INT16(outBus->data[i/2] * 0.01);
            codecDmaTxPtr[i+1] = FLOAT_TO_INT16(outBus->data[i/2] * 0.01);
        }
        codecDmaTxPtr = NULL;
        codecDmaRxPtr = NULL;
        processingDone = 1;
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
