#include <stdint.h>

#include "note_scheduler.h" 
#include "mm_seq.h"
#include "mmpv_tesp.h" 

typedef struct __NoteOnEvent NoteOnEvent;
typedef struct __NoteOffEvent NoteOffEvent;

struct __NoteOnEvent {
    MMEvent head;
    MMPolyManager *pvm;
    MMPvtespParams *params;
    MMSeq *sequencer; /* sequencer that will play this event and its note off */
    MMTime length; /* time between noteon and noteoff events */
};

NoteOnEvent *NoteOnEvent_new(void)
{
    return (NoteOnEvent*)malloc(sizeof(NoteOnEvent));
}

#define NoteOnEvent_init(noe,_pvm,_params,_seq,_len)\
    ((MMEvent*)(noe))->happen = NoteOnEvent_happen;\
    (noe)->pvm = _pvm;\
    (noe)->params = _params;\
    (noe)->sequencer = _seq;\
    (noe)->length = _len;

struct __NoteOffEvent {
    MMEvent head;
    MMPolyManager *pvm;
    MMPvtespParams *params;
};

NoteOffEvent *NoteOffEvent_new(void)
{
    return (NoteOffEvent*)malloc(sizeof(NoteOffEvent));
}

#define NoteOffEvent_init(noe,_pvm,_params)\
    ((MMEvent*)(noe))->happen = NoteOffEvent_happen;\
    (noe)->pvm = _pvm;\
    (noe)->params = _params;

uint32_t voiceStates = 0xffffffff; /* Store the voice states for up to 32 voices */

#define set_voice_busy(vs,n) (vs) &= ~(1 << (n))
#define set_voice_free(vs,n) (vs) |= (1 << (n))

void yield_params_to_allocator(void *allocator, void *params)
{
    /* mmpv_tesp will give a value of type (MMSample*)
     * to yield to the allocator */
    set_voice_free(*((uint32_t*)allocator),(uint32_t)*((MMSample*)params));
}

inline int get_next_free_voice_number(void)
{
    int n;
    for (n = 0; n < NOTE_SCHED_MAX_NUM_VOICES; n++) {
        if ((voiceStates >> n) & 0x1) {
            return n;
        }
    }
    return -1; /* no voices free */
}

void NoteOffEvent_happen(MMEvent *event)
{
    MMPvtespParams *params = ((NoteOffEvent*)event)->params;
    MMPolyManager *pvm     = ((NoteOffEvent*)event)->pvm;
    MMPolyManager_noteOff(pvm, (void*)params);
    free(event);
}

void NoteOnEvent_happen(MMEvent *event)
{
    MMPvtespParams *params = ((NoteOnEvent*)event)->params;
    MMPolyManager *pvm     = ((NoteOnEvent*)event)->pvm;
    int voiceNumber;
    if ((voiceNumber = get_next_free_voice_number()) == -1) {
        /* no free voices, free params, free event and abort */
        free(params);
        free(event);
        return;
    }
    /* set that voice as busy */
    set_voice_busy(voiceStates,voiceNumber);
    params->note = voiceNumber; /* use note to keep track of voice */
    /* Indicate how values should be yielded to the allocator */
    ((MMPolyVoiceParams*)params)->yield_params_to_allocator = yield_params_to_allocator;
    /* Indicate the allocator to which values should be yielded */
    ((MMPolyVoiceParams*)params)->allocator = (void*)&voiceStates;
    /* make note off parameters and initialize */
    MMPvtespParams *noteOffParams = MMPvtespParams_new();
    noteOffParams->paramType = MMPvtespParamType_NOTEOFF;
    noteOffParams->note = voiceNumber;
    noteOffParams->amplitude = params->amplitude; /* do we need this? */
    noteOffParams->releaseTime =  params->releaseTime;
    /* Indicate how values should be yielded to the allocator */
    ((MMPolyVoiceParams*)noteOffParams)->yield_params_to_allocator = yield_params_to_allocator;
    /* Indicate the allocator to which values should be yielded */
    ((MMPolyVoiceParams*)noteOffParams)->allocator = (void*)&voiceStates;
    /* make note off parameters and initialize */
    /* make note off event */
    NoteOffEvent *noff = NoteOffEvent_new();
    /* init with parameters and the poly voice manager */
    NoteOffEvent_init(noff,pvm,noteOffParams);
    /* schedule it */ 
    MMSeq_scheduleEvent(((NoteOnEvent*)event)->sequencer,
            (MMEvent*)noff,
            MMSeq_getCurrentTime(((NoteOnEvent*)event)->sequencer)
                + ((NoteOnEvent*)event)->length);
    /* now do the note on */
    MMPolyManager_noteOn(pvm, (void*)params, 
            MMPolyManagerSteal_FALSE, MMPolyManagerRetrigger_FALSE);
    /* no need to free the params, MMPolyManager will do that 
     * at the right time */
    /* free the event though */
    free(event);
}
