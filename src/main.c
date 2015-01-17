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
#include "mm_poly_voice_manage.h"
#include "mmpv_tesp.h" 

#define MIDI_BOTTOM_NOTE 48
#define MIDI_TOP_NOTE    60
#define MIDI_NUM_NOTES   8

#define BUS_NUM_CHANS 1
#define BUS_BLOCK_SIZE (CODEC_DMA_BUF_LEN / CODEC_NUM_CHANNELS)

#define ATTACK_TIME 0.01 
#define RELEASE_TIME 0.5 
#define SHORT_RELEASE_TIME 0.2

//extern MMSample GrandPianoFileDataStart;
//extern MMSample GrandPianoFileDataEnd;

static MIDIMsgBuilder_State_t lastState;
static MIDIMsgBuilder midiMsgBuilder;
static MIDI_Router_Standard midiRouter;

static MMTrapEnvedSamplePlayer spsps[MIDI_NUM_NOTES];

static MMPolyManager *pvm;

static MMWavTab samples;

void MIDI_note_on_do(void *data, MIDIMsg *msg)
{
    MMPvtespParams *params = MMPvtespParams_new();
    params->paramType = MMPvtespParamType_NOTEON;
    params->note = (MMSample)msg->data[1];
    params->amplitude = (MMSample)msg->data[2] / 127.;
    params->interpolation = MMInterpMethod_CUBIC;
    params->index = 0;
    params->attackTime = ATTACK_TIME;
    /* this is the time a note that is stolen will take to decay */
    params->releaseTime = SHORT_RELEASE_TIME; 
    params->samples = &samples;
    params->loop = 1;
    MMPolyManager_noteOn(pvm, (void*)params, MMPolyManagerSteal_TRUE);
    MIDIMsg_free(msg);
}

void MIDI_note_off_do(void *data, MIDIMsg *msg)
{
    MMPvtespParams *params = MMPvtespParams_new();
    params->paramType = MMPvtespParamType_NOTEOFF;
    params->note = (MMSample)msg->data[1];
    params->amplitude = (MMSample)msg->data[2] / 127.;
    params->releaseTime = RELEASE_TIME;
    MMPolyManager_noteOff(pvm, (void*)params);
    MIDIMsg_free(msg);
}

int main(void)
{
    MMSample *sampleFileDataStart = WaveTable;
    size_t i;

    /* Enable signalling routines for errors */
    error_sig_init();

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
    samples.data = sampleFileDataStart;
    samples.length = WAVTABLE_LENGTH_SAMPLES; // sampleFileDataEnd - sampleFileDataStart;

    /* Make poly voice manager */
    pvm = MMPolyManager_new(MIDI_NUM_NOTES);

    /* Enable MIDI hardware */
    MIDI_low_level_setup();

    /* Initialize MIDI Message builder */
    MIDIMsgBuilder_init(&midiMsgBuilder);

    /* set up the MIDI router to trigger samples */
    MIDI_Router_Standard_init(&midiRouter);
    for (i = 0; i < MIDI_NUM_NOTES; i++) {
        /* Initialize sample player */
        MMTrapEnvedSamplePlayer_init(&spsps[i], outBus, BUS_BLOCK_SIZE, 
                1. / (MMSample)CODEC_SAMPLE_RATE);
        /* Make new poly voice and add it to the poly voice manager */
        MMPolyManager_addVoice(pvm, i, (MMPolyVoice*)MMPvtesp_new(&spsps[i])); 
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
