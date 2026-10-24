/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The engine table and the tracks' sounds at power-on. The groovebox has one engine, DIGITAL (eng_digital.c). */
#include "dsp.c"                /* the shared DSP blocks (MIT, from Felucca-Engines) */
#include "eng_digital.c"        /* DIGITAL: four-operator FM */

/* the stores hold engine numbers: DIGITAL is 1 (ENGI_DIGITAL); any other number plays DIGITAL too */
static const engine_t *const ENGINES[NENGINES] = {&ENG_DIGITAL, &ENG_DIGITAL};

/* a track's engine number as an index (a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* the tracks at power-on: DIGITAL's presets (eng_digital.c DIGITAL_PRESETS) BD SD CH OH */
static const uint8_t TRK_DEF[NPART] = {8, 9, 10, 11};
