/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), BBL JAM BOX */
/* MONO: one analog-style voice in the vein of a Minimoog (not a copy of one). Two oscillators, a square sub, a drive
 * before the filter; the filter, its envelope and the ENV are DIGITAL's pages (FILTER / FILTER ENV / ENV).
 *   OSC 1: WAVE  the two oscillators' waves (SAW SQR TRI SIN in pairs, then each doubled), MIX  osc 1 .. osc 2,
 *          FINE  osc 2 -1 .. +1 semitone (centre: in tune), SEMI  osc 2 0 .. +24 semitones
 *   OSC 2: SUB  -1 / -2 / -3 octaves (a square), SUBVL  its level, PW  osc 1's shape (SQR: the pulse width,
 *          TRI: a wavefold, SIN: fewer and fewer levels to a square, SAW: a hidden oscillator it is hard synced to,
 *          1x .. 8x the note), SAT (P_LN0)  drive into a soft clip before the filter
 * Band-limited (polyBLEP) saws, pulses, sync. One voice (.poly 1; the machine is LEGATO).
 * The voice's state: ph[0] osc 1, ph[1] osc 2, ph[2] the sub; s[0..1] the filter, s[2..3] its envelope (as SAMPLER's),
 * s[4..5] osc 1 / 2's sync partner, s[6..7] their sync jump (the sample before the reset to the one after it). */
#define MONO_NW 10u
static const char *const N_MWAVE[MONO_NW] = {"SW+SQ", "SW+TR", "SW+SN", "SQ+TR", "SQ+SN", "TR+SN",
                                             "SW+SW", "SQ+SQ", "TR+TR", "SN+SN"};
enum { MW_SAW, MW_SQR, MW_TRI, MW_SIN };
static const uint8_t MONO_W[MONO_NW][2] = {{MW_SAW, MW_SQR}, {MW_SAW, MW_TRI}, {MW_SAW, MW_SIN}, {MW_SQR, MW_TRI},
                                          {MW_SQR, MW_SIN}, {MW_TRI, MW_SIN}, {MW_SAW, MW_SAW}, {MW_SQR, MW_SQR},
                                          {MW_TRI, MW_TRI}, {MW_SIN, MW_SIN}};
static const char *const N_MSUB[3] = {"-1", "-2", "-3"};
static const char *const N_MMACH[1] = {"MONO"};
static const param_desc_t MONO_SAT = {"SAT", F_INT, 0, 127, 0, 0, 0};   /* P_LN0 on a MONO track (params.c track_desc) */

/* osc 2's tuning: Q16 ratio to osc 1 for d 1/64 semitones (0 .. +24 semitones + -1 .. +1) */
static uint32_t mono_ratio(int32_t d)
{
    uint32_t base = pitch_inc(60u * 16u), a, b, x;
    int32_t p = 60 * 16 * 4 + d;
    a = pitch_inc((uint32_t)(p >> 2));
    b = pitch_inc((uint32_t)(p >> 2) + 1u);
    x = a + (uint32_t)(((uint64_t)(b - a) * (uint32_t)(p & 3)) >> 2);
    return (uint32_t)(((uint64_t)x << 16) / base);
}

/* one oscillator's sample: wave w at phase *ph (advanced by inc), pw 0..127 its shape; *sp, *jp its sync partner and
 * jump (SAW) */
static inline int32_t mono_osc(uint32_t w, uint32_t *ph, uint32_t inc, uint32_t pw, int32_t *sp, int32_t *jp)
{
    uint32_t p = *ph, y;
    int32_t o;
    switch (w) {
    case MW_SAW:
        if (!pw) {
            o = osc_saw(p, inc);
            break;
        }
        {                                               /* hard sync: the partner at 1x .. 8x, reset as osc wraps */
            uint32_t si = (uint32_t)(((uint64_t)inc * (65536u + pw * 7u * 65536u / 127u)) >> 16), s = (uint32_t)*sp;
            if (p > 0xFFFFFFFFu - inc) {               /* the wrap comes next sample: the partner's jump at it */
                uint32_t at = s + (uint32_t)(((uint64_t)si * (0xFFFFFFFFu - p)) / inc);
                *jp = -(int32_t)(at >> 16);             /* (from its value there down to the reset's -32768) */
            }
            o = ((int32_t)(s >> 16) - 32768) - blep(s, si);
            if (p < inc || p > 0xFFFFFFFFu - inc)      /* the reset's own step, band-limited */
                o += (blep(p, inc) * *jp) >> 16;
            s += si;
            if (p + inc < p)                           /* osc wraps: the partner starts again where it would be */
                s = (uint32_t)(((uint64_t)si * (p + inc)) / inc);
            *sp = (int32_t)s;
        }
        break;
    case MW_SQR:                                        /* PW: 50 % .. 5 % */
        o = osc_pulse(p, inc, 0x80000000u - pw * (0x73333333u / 127u));
        break;
    case MW_TRI:                                        /* PW: folded back into range, more and more */
        o = osc_tri(p);
        if (pw) {
            o = (o * (int32_t)(127u + pw * 3u)) / 127;  /* x1 .. x4 */
            while (o > 32767 || o < -32768)
                o = o > 32767 ? 65534 - o : -65536 - o;
        }
        break;
    default:                                            /* MW_SIN, PW: 16 bits .. 1 (a square) */
        o = osc_sine(p);
        if (pw) {
            uint32_t sh = 1u + pw * 14u / 127u, q = 1u << sh;    /* (a level every q: 2 .. 32768) */
            y = (uint32_t)(o + 32768);
            o = (int32_t)((y >> sh << sh) + (q >> 1)) - 32768;
            o = clamp(o, -32767, 32767);
        }
        break;
    }
    *ph = p + inc;
    return o;
}

static void mono_note_on(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    v->s[2] = p[P_FE_ATK] ? 0 : 1 << 24;               /* the filter envelope (as DIGITAL's) */
    v->s[3] = p[P_FE_ATK] ? 1 : 2;
}

static void mono_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, wv = (uint32_t)p[P_E0] % MONO_NW, w1 = MONO_W[wv][0], w2 = MONO_W[wv][1];
    uint32_t mix = (uint32_t)p[P_E1] & 127u, pw = (uint32_t)p[P_E6] & 127u, sub = (uint32_t)p[P_E4] % 3u;
    int32_t sublv = p[P_E5] & 127, sat = p[P_LN0] & 127;
    uint32_t inc1 = m->inc, inc2, incs;
    int32_t drv = 32768 + sat * sat * 4;                /* SAT: x1 .. ~x2 squared, so the top screams (Q15) */
    inc2 = (uint32_t)(((uint64_t)inc1 * mono_ratio(p[P_E3] * 64 + p[P_E2])) >> 16);   /* FINE -64..63: +-1 st */
    incs = inc1 >> (sub + 1u);
    /* the filter (DIGITAL's, with its envelope and DRIVE), as SAMPLER's */
    int32_t cut = (p[P_FLT_CUT] << 8) + m->cutoff + (p[P_FLT_KTR] * (v->pitch16 - 60 * 16) >> 4);
    {
        int32_t fe = v->s[2], fs = v->s[3], sus = (int32_t)p[P_FE_SUS] * (1 << 17);
        if (!v->gate)
            fs = 4;
        if (fs == 1) {
            fe += (int32_t)ENV_LIN[p[P_FE_ATK] & 127];
            if (fe >= (1 << 24)) { fe = 1 << 24; fs = 2; }
        } else if (fs == 2) {
            fe = p[P_FE_DEC] ? fe + mulq16(sus - fe, ENV_EXP[p[P_FE_DEC] & 127]) : sus;
        } else if (fs == 4) {
            fe -= mulq16(fe, ENV_EXP[p[P_FE_REL] & 127]);
        }
        fe = clamp(fe, 0, 1 << 24);
        v->s[2] = fe;
        v->s[3] = fs;
        cut += ((fe >> 9) * p[P_FE_AMT]) >> 6;
        if (v == &t->v[t->m_vi])
            t->m_fenv = fe >= (1 << 24) ? 32767 : fe >> 9;   /* (FENV, a matrix source: mod.c) */
    }
    int32_t drive = 32768 + p[P_FLT_DRV] * 512, fdrv = p[P_FLT_DRV], ic1 = v->s[0], ic2 = v->s[1];
    int32_t fk = ANALOG_K(p[P_FLT_RES]);
    uint32_t ft = (uint32_t)p[P_FTYPE];
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_FLT_RES]);
    for (i = 0; i < n; i++) {
        int32_t a = mono_osc(w1, &v->ph[0], inc1, pw, &v->s[4], &v->s[6]);
        int32_t b = mono_osc(w2, &v->ph[1], inc2, 0u, &v->s[5], &v->s[7]);   /* (PW: osc 1 only) */
        int32_t y = (a * (127 - (int32_t)mix) + b * (int32_t)mix) / 127;
        int32_t s = osc_pulse(v->ph[2], incs, 0x80000000u);
        v->ph[2] += incs;
        y = (y * 3 + ((s * sublv) >> 7) * 2) >> 2;     /* (osc 3/4, the sub up to 1/2: room before the clip) */
        if (sat)
            y = softclip((int32_t)(((int64_t)y * drv) >> 15));
        if (fdrv)
            y = softclip(((y >> 2) * (drive >> 2)) >> 11);
        y = digital_filter(&flt, fk, ft, y, &ic1, &ic2);
        out[i] += voice_amp(y, m, i);
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
}

static const preset_t MONO_PRESETS[1] = {   /* a plain saw: the filter open, nothing else */
    {"SAW", {0, 0, 0, 0, 0, 0, 0, 0}, {0, 64, 127, 20}, 0, 1, FX(0, 0, 0, 0), PAT(0)},
};

static const engine_t ENG_MONO = {
    .name = "MONO",
    .page_title = {"OSC 1", "OSC 2"},
    .edit = {{"WAVE", F_ENUM, 0, MONO_NW - 1, 0, N_MWAVE, 0}, {"MIX", F_INT, 0, 127, 0, 0, 0},
             {"FINE", F_BIPCT, -64, 63, 0, 0, 0}, {"SEMI", F_INT, 0, 24, 0, 0, 0},
             {"SUB", F_ENUM, 0, 2, 0, N_MSUB, 0}, {"SUBVL", F_INT, 0, 127, 0, 0, 0}, {"PW", F_INT, 0, 127, 0, 0, 0},
             {"MACH", F_ENUM, 0, 0, 0, N_MMACH, 0}},
    .presets = MONO_PRESETS,
    .npresets = 1,
    .note_on = mono_note_on,
    .render = mono_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .keep = 0x33u,                                      /* (a retrigger: the filter and the sync partners go on) */
};
