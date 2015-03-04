#include "midi_callbacks.h" 
#include "note_scheduler.h"
#include "mm_trapenvedsampleplayer.h"

void autorelease_on_done(MMEnvedSamplePlayer * esp)
{
    yield_params_to_allocator((void*)&voiceStates,
            (void *)&(MMEnvedSamplePlayer_getSamplePlayerSigProc(esp).note));
}

void MIDI_note_on_autorelease_do(void *data, MIDIMsg *msg)
{
    /* this actually ignores all the information received over MIDI, which is
     * fine for now. */
    int voiceNum = get_next_free_voice_number();
    SynthEnvironment *se = (SynthEnvironment*)data;
    if (voiceNum == -1) {
        MIDIMsg_free(msg);
        return;
    }
    ((MMEnvedSamplePlayer*)&se->spsps[voiceNum])->onDone = autorelease_on_done;
    MMTrapEnvedSamplePlayer_noteOn_Rate(
            &se->spsps[voiceNum],
            voiceNum,
            *(se->amplitude),
            MMInterpMethod_CUBIC,
            0,
            *(se->attackTime),
            *(se->releaseTime),
            *(se->sustainTime),
            se->samples,
            1,
            *(se->rate));
}
