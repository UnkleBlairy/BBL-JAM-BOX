/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The engine table and the tracks' sounds at power-on. The groovebox has one engine, DIGITAL (eng_digital.c). */
#include "dsp.c"                /* the shared DSP blocks (MIT, from Felucca-Engines) */
#include "eng_digital.c"        /* DIGITAL: four-operator FM */
#define ICON_MUTE GBI_MUTE        /* (ui_icons.h, Felucca's parameter icons, has these three names) */
#define ICON_DELAY GBI_DELAY
#define ICON_FADE GBI_FADE
#include "icons_gb.h"           /* the screen's 16 x 16 icons (the Pixel Slots editor) */
#include "icons_808.h"          /* .. and docs/808lab.html's (tools/gen_icons_808.py) */
#undef ICON_MUTE
#undef ICON_DELAY
#undef ICON_FADE

/* the icons by number (the screen's description holds the number: ui_gb.c gb_view_t) */
enum { IC_NONE, IC_SYN, IC_KICK, IC_SNARE, IC_HATS, IC_CYMBAL, IC_PERC, IC_TOM, IC_RIM, IC_CLAP, IC_COWBELL, IC_AGE,
       IC_LP, IC_BP, IC_HP, IC_DJ, IC_COMB, IC_SINE, IC_TRI, IC_SAW, IC_SQUARE, IC_RANDOM, IC_DELAY, IC_VERB, IC_MUTE,
       IC_FADE, IC_REPEAT, IC_REVERSE, IC_MACRO, IC_COUNT };
static const uint16_t *const GB_ICONS[IC_COUNT] = {
    0, ICON_SYN, ICON_KICK, ICON_SNARE, ICON_HATS, ICON_CYMBAL, ICON_PERC, ICON_TOM, ICON_RIM, ICON_CLAP, ICON_COWBELL,
    ICON_AGE, ICON_LOW_PASS, ICON_BAND_PASS, ICON_HIGH_PASS, ICON_DJ_FILTER, ICON_COMB, ICON_SINE, ICON_TRI, ICON_SAW,
    ICON_SQUARE, ICON_RANDOM, GBI_DELAY, ICON_VERB, GBI_MUTE, GBI_FADE, ICON_REPEAT, ICON_REVERSE, ICON_MACRO};

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
    uint8_t icon;                /* its screen icon (IC_*) */
} machine_t;
static const machine_t MACHINES[] = {
    {"SYNTH", ENGI_DIGITAL, 0, 0, 48, 0, IC_SYN},
    {"KICK", ENGI_DIGITAL, 1, 1, 36, 8, IC_KICK},
    {"SNARE", ENGI_DIGITAL, 2, 1, 50, 9, IC_SNARE},
    {"HATS", ENGI_DIGITAL, 3, 1, 84, 10, IC_HATS},
    {"CYMBAL", ENGI_DIGITAL, 4, 1, 72, 12, IC_CYMBAL},
    {"PERC", ENGI_DIGITAL, 5, 1, 48, 13, IC_PERC},
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
