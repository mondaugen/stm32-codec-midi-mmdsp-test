#ifndef NOTE_SCHEDULER_H
#define NOTE_SCHEDULER_H 
#include <stdint.h>
#include "mm_seq.h"
#include "mmpv_tesp.h" 

#define NOTE_SCHED_MAX_NUM_VOICES 8

typedef struct __NoteOnEvent NoteOnEvent;
typedef struct __NoteOffEvent NoteOffEvent;

struct __NoteOnEvent {
    MMEvent head;
    MMPolyManager *pvm;
    MMPvtespParams *params;
    MMSeq *sequencer; /* sequencer that will play this event and its note off */
    MMTime length; /* time between noteon and noteoff events */
};

struct __NoteOffEvent {
    MMEvent head;
    MMPolyManager *pvm;
    MMPvtespParams *params;
};

void NoteOffEvent_happen(MMEvent *event);
void NoteOnEvent_happen(MMEvent *event);
NoteOnEvent *NoteOnEvent_new(void);
NoteOffEvent *NoteOffEvent_new(void);
int get_next_free_voice_number(void);

#define NoteOffEvent_init(noe,_pvm,_params)\
    ((MMEvent*)(noe))->happen = NoteOffEvent_happen;\
    (noe)->pvm = _pvm;\
    (noe)->params = _params;

#define NoteOnEvent_init(noe,_pvm,_params,_seq,_len)\
    ((MMEvent*)(noe))->happen = NoteOnEvent_happen;\
    (noe)->pvm = _pvm;\
    (noe)->params = _params;\
    (noe)->sequencer = _seq;\
    (noe)->length = _len;

#endif /* NOTE_SCHEDULER_H */
