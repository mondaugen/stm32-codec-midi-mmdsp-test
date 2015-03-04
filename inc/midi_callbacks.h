#ifndef MIDI_CALLBACKS_H
#define MIDI_CALLBACKS_H 
#include "mm_midimsgbuilder.h" 
#include "mm_sample.h" 
#include "mm_trapenvedsampleplayer.h" 
#include "mm_wavtab.h" 

typedef struct __SynthEnvironment SynthEnvironment;

struct __SynthEnvironment {
            MMTrapEnvedSamplePlayer *spsps;
            MMSample *amplitude;
            MMSample *attackTime;
            MMSample *releaseTime;
            MMSample *sustainTime;
            MMWavTab *samples;
            MMSample *rate;
};

void MIDI_note_on_autorelease_do(void *data, MIDIMsg *msg);

#endif /* MIDI_CALLBACKS_H */
