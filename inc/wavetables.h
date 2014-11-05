#ifndef WAVETABLES_H
#define WAVETABLES_H 
#include "mm_sample.h" 
#include "i2s_setup.h"

#define WAVTABLE_LENGTH_SAMPLES 8192 

#define WAVTABLE_NUM_PARTIALS   2

extern MMSample WaveTable[];

MMSample WaveTable_midiNumber(void);
void WaveTable_init(void);

#endif /* WAVETABLES_H */
