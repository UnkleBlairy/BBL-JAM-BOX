/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The engine table and the tracks' sounds at power-on. The groovebox has one engine, DIGITAL (eng_digital.c). */
#include "dsp.c"                /* the shared DSP blocks (MIT, from Felucca-Engines) */
#include "eng_digital.c"        /* DIGITAL: four-operator FM */

/* the stores hold engine numbers: DIGITAL is 1 (ENGI_DIGITAL); any other number plays DIGITAL too */
static const engine_t *const ENGINES[NENGINES] = {&ENG_DIGITAL, &ENG_DIGITAL};

/* a track's engine number as an index (a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* The machines: what a track can be (the screen's oval, the SAVE slots, the drum or synth behaviour). A machine is an
 * engine (ENGINES[]) and that engine's own machine number (DIGITAL: its MACH, P_E7). A new engine: its eng_*.c included
 * above, its entry in ENGINES, its machines here (gb_store.c has room for GB_MACH of them) */
typedef struct {
    const char *pill;            /* the screen's oval */
    uint8_t engine, sub;         /* its engine (ENGINES[] index) and the engine's machine number */
    uint8_t drum;                /* a drum: the ENV is attack-decay (no sustain, no release) */
    uint8_t note;                /* the note the step keys play (a synth: its last chord, this one before any) */
    uint8_t preset;              /* its starting sound: the engine's preset */
} machine_t;
static const machine_t MACHINES[] = {
    {"SYNTH", ENGI_DIGITAL, 0, 0, 48, 0},
    {"KICK", ENGI_DIGITAL, 1, 1, 36, 8},
    {"SNARE", ENGI_DIGITAL, 2, 1, 50, 9},
    {"HATS", ENGI_DIGITAL, 3, 1, 84, 10},
    {"CYMBAL", ENGI_DIGITAL, 4, 1, 72, 12},
    {"PERC", ENGI_DIGITAL, 5, 1, 48, 13},
};
#define NMACH (sizeof MACHINES / sizeof MACHINES[0])
/* the machine track t is (its engine and machine number; none of them: the first) */
static inline uint32_t trk_mach(const track_t *t)
{
    uint32_t m;
    for (m = 0; m < NMACH; m++)
        if (ENGINES[MACHINES[m].engine] == ENGINES[eng_idx(t->eng_req)] && MACHINES[m].sub == (uint32_t)t->p[P_E7])
            return m;
    return 0;
}
static inline const machine_t *trk_machine(const track_t *t) { return &MACHINES[trk_mach(t)]; }

/* the tracks at power-on: DIGITAL's presets (eng_digital.c DIGITAL_PRESETS) BD SD CH OH, TOM (PERC), INIT (SYN) */
static const uint8_t TRK_DEF[NPART] = {8, 9, 10, 11, 13, 0};
