#include <stddef.h>
#include <string.h>
#include <math.h> 
#include "wavetables.h"

MMSample WaveTable[WAVTABLE_LENGTH_SAMPLES];

MMSample WaveTable_midiNumber(void)
{
    return 12. * log2(CODEC_SAMPLE_RATE / (WAVTABLE_LENGTH_SAMPLES * 440.0)) + 69.;
}

void WaveTable_init(void)
{
    size_t i,j;
    memset(WaveTable,0,sizeof(MMSample) * WAVTABLE_LENGTH_SAMPLES);
    for (i = 0; i < WAVTABLE_LENGTH_SAMPLES; i++) {
        for (j = 0; j < WAVTABLE_NUM_PARTIALS; j++) {
            WaveTable[i] += sin((MMSample)i / (MMSample)WAVTABLE_LENGTH_SAMPLES 
                    * (j + 1) * M_PI * 2.) / (MMSample)(j + 1);
        }
    }
}

