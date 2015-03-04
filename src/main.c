/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_conf.h" 
#include <stdlib.h>
#include <string.h> 
#include <math.h> 
#include "stm32f4xx.h" 
#include "leds.h" 
#include "main.h" 
#include "error_sig.h" 
#include "i2s_setup.h" 
#include "midi_lowlevel.h"
#include "fmc.h" 
#include "note_scheduler.h" 
#include "midi_callbacks.h"

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
#include "mm_wavtab_recorder.h"

#define MIDI_BOTTOM_NOTE 48
#define MIDI_TOP_NOTE    60
#define MIDI_NUM_NOTES   12

#define BUS_NUM_CHANS 1
#define BUS_BLOCK_SIZE (CODEC_DMA_BUF_LEN / CODEC_NUM_CHANNELS)
#define BLOCKS_PER_SEC (CODEC_SAMPLE_RATE / CODEC_DMA_BUF_LEN) 

#define ATTACK_TIME 0.01 
#define RELEASE_TIME 0.01
#define SUSTAIN_TIME 2.0 
#define SHORT_RELEASE_TIME 0.01
#define NOTE_LENGTH_SEC 5
#define NOTE_LENGTH_TICKS (NOTE_LENGTH_SEC * BLOCKS_PER_SEC) 
#define INITIAL_NOTE_DELAY_SEC 10
#define INITIAL_NOTE_DELAY_TICKS (INITIAL_NOTE_DELAY_SEC * BLOCKS_PER_SEC)
#define EVENT_PERIOD_SEC 2
#define EVENT_PERIOD_TICKS (EVENT_PERIOD_SEC * BLOCKS_PER_SEC) 

//extern MMSample GrandPianoFileDataStart;
//extern MMSample GrandPianoFileDataEnd;

static MIDIMsgBuilder_State_t lastState;
static MIDIMsgBuilder midiMsgBuilder;
static MIDI_Router_Standard midiRouter;

MMTrapEnvedSamplePlayer spsps[MIDI_NUM_NOTES];

static MMPolyManager *pvm;

static MMWavTab samples;

static MMWavTabRecorder wtr;

static MMSample amplitude = 0.9;
static MMSample attackTime = ATTACK_TIME;
static MMSample releaseTime = RELEASE_TIME;
static MMSample sustainTime = SUSTAIN_TIME;
static MMSample playbackRate = 1.0;

static int  eventPeriod = EVENT_PERIOD_TICKS;
static int  eventLength = NOTE_LENGTH_TICKS;

SynthEnvironment synthEnvironment = {
    spsps,
    &amplitude,
    &attackTime,
    &releaseTime,
    &sustainTime,
    &samples,
    &playbackRate
};


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
    params->rate = playbackRate;
    params->rateSource = MMPvtespRateSource_RATE;
    MMPolyManager_noteOn(pvm, (void*)params, 
            MMPolyManagerSteal_FALSE, MMPolyManagerRetrigger_FALSE);
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

void MIDI_cc_do(void *data, MIDIMsg *msg)
{
    if (msg->data[2]) {
        /* start recording */
        ((MMWavTabRecorder*)data)->currentIndex = 0;
        ((MMWavTabRecorder*)data)->state = MMWavTabRecorderState_RECORDING;
    } else {
        ((MMWavTabRecorder*)data)->state = MMWavTabRecorderState_STOPPED;
    }
}

void MIDI_cc_rate_control(void *data, MIDIMsg *msg)
{
    *((MMSample*)data) = ((MMSample)msg->data[2] - 60.0) / 60. + 1.;
}

void MIDI_cc_period_control(void *data, MIDIMsg *msg)
{
    *((int*)data) = msg->data[2] * 2 + 1;
}

void MIDI_cc_length_control(void *data, MIDIMsg *msg)
{
    *((int*)data) = msg->data[2] * 3 + 1;
}

int main(void)
{
    MMSample *sampleFileDataStart = WaveTable;
    size_t i;

    /* Enable signalling routines for errors */
    error_sig_init();

    /* Enable external SRAM */
    FMC_Config();

    codecDmaTxPtr = NULL;
    codecDmaRxPtr = NULL;

    /* The bus the signal chain is reading */
    MMBus *inBus = MMBus_new(BUS_BLOCK_SIZE,BUS_NUM_CHANS);

    /* The bus the signal chain is writing */
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
    MMArray_set_data(&samples, WaveTable);
    MMArray_set_length(&samples, WAVTABLE_LENGTH_SAMPLES); 
    /* Set with this samplerate so it plays at normal speed when midi note 69
     * received */
    samples.samplerate = 440 * WAVTABLE_LENGTH_SAMPLES;//CODEC_SAMPLE_RATE;

    /* Allow MMWavTabRecorder to record into samples */
    MMWavTabRecorder_init(&wtr);
    wtr.buffer = &samples;
    wtr.inputBus = inBus;
    wtr.currentIndex = 0;
    wtr.state = MMWavTabRecorderState_RECORDING;

    /* Put MMWavTabRecorder at the top of the signal chain */
    MMSigProc_insertAfter(&sigChain.sigProcs,&wtr);

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
    MIDI_Router_addCB(&midiRouter.router, MIDIMSG_NOTE_ON, 1, MIDI_note_on_autorelease_do,
                        &synthEnvironment);
    MIDI_CC_CB_Router_addCB(&midiRouter.cbRouters[0],2,MIDI_cc_do,&wtr);
    MIDI_CC_CB_Router_addCB(&midiRouter.cbRouters[0],3,MIDI_cc_rate_control,&playbackRate);
    MIDI_CC_CB_Router_addCB(&midiRouter.cbRouters[0],4,MIDI_cc_period_control,&eventPeriod);
    MIDI_CC_CB_Router_addCB(&midiRouter.cbRouters[0],5,MIDI_cc_length_control,&eventLength);

    /* set up note scheduler */
    MMSeq *sequence = MMSeq_new();
    MMSeq_init(sequence, 0);

    /* Enable codec */
    i2s_dma_full_duplex_setup(CODEC_SAMPLE_RATE);

    while (1) {
        while (!(codecDmaTxPtr && codecDmaRxPtr));
        //if ((MMSeq_getCurrentTime(sequence) % eventPeriod) == 0) {
        //    /* Make event */
        //    NoteOnEvent *noe = NoteOnEvent_new();
        //    MMPvtespParams *params = MMPvtespParams_new();
        //    params->paramType = MMPvtespParamType_NOTEON;
        //    params->note = get_next_free_voice_number();
        //    params->amplitude = 1.0;
        //    params->interpolation = MMInterpMethod_CUBIC;
        //    params->index = 0;
        //    params->attackTime = ATTACK_TIME;
        //    params->releaseTime = RELEASE_TIME; 
        //    params->samples = &samples;
        //    params->loop = 1;
        //    params->rate = playbackRate;
        //    params->rateSource = MMPvtespRateSource_RATE;
        //    NoteOnEvent_init(noe,pvm,params,sequence,eventLength);
        //    /* Schedule event to happen now */
        //    MMSeq_scheduleEvent(sequence,(MMEvent*)noe,MMSeq_getCurrentTime(sequence));
        //}
        ///* Do scheduled events and tick */
        //MMSeq_doAllCurrentEvents(sequence);
        //MMSeq_tick(sequence);
        MIDI_process_buffer(); /* process MIDI at most every audio block */
        MMSigProc_tick(&sigChain);
        size_t i;
        for (i = 0; i < CODEC_DMA_BUF_LEN; i += 2) {
            /* write out data */
            codecDmaTxPtr[i] = FLOAT_TO_INT16(outBus->data[i/2] * 0.01);
            codecDmaTxPtr[i+1] = FLOAT_TO_INT16(outBus->data[i/2] * 0.01);
            /* read in data */
            inBus->data[i/2] = INT16_TO_FLOAT(codecDmaRxPtr[i+1]);
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
