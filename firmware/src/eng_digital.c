/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DIGITAL: four-operator FM, the groovebox's engine: the machines SYN BD SD HH CYM PERC (E7 = MACH).
 * Four sine operators, eight classic 4-operator algorithms (ALG = P_E0, EDIT 1 KNOB 1),
 * op 4 with feedback, one modulation INDEX shaped by a modulator envelope.
 * Phase modulation wraps naturally in the 32-bit phase. */
/* ------------------------------------------------- DIGITAL's tables --- */
static const char *const N_FMALG[] = {"1", "2", "3", "4", "5", "6", "7", "8"};
static const char *const N_MACH[] = {"SYN", "BD", "SD", "HH", "CYM", "PERC"};
static const char *const N_RATIO[] = {".5", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "14", "16"};

/* the factory presets (the stores' preset numbers of engine 1 index this table) */
static const preset_t DIGITAL_PRESETS[] = {
    /* ALG R2 R3 R4 INDEX MODDEC FDBK - */
    {"INIT", {-56, 8, 8, 8, 70, 50, 0, 0}, {0, 70, 60, 40}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
    {"BELL", {8, 64, 8, 32, 90, 70, 0, 0}, {0, 95, 0, 85}, 0, 0, FX(0, 0, 0, 0), PAT(7)},
    {"BASS", {-56, 8, 8, 8, 48, 40, 8, 0}, {0, 60, 70, 25}, 0, 1, FX(0, 0, 0, 0), PAT(2)},
    {"BRASS", {-56, 8, 8, 8, 70, 40, 20, 0}, {40, 70, 100, 40}, 0, 0, FX(0, 0, 0, 0), PAT(6)},
    {"ORGAN", {56, 16, 24, 32, 0, 0, 0, 0}, {2, 60, 127, 20}, 0, 0, FX(0, 0, 0, 0), PAT(6)},
    {"PAD", {24, 16, 8, 24, 40, 90, 10, 0}, {80, 90, 110, 95}, 0, 0, FX(0, 0, 0, 0), PAT(5)},
    {"MARIMBA", {8, 32, 8, 8, 60, 30, 0, 0}, {0, 80, 0, 60}, 0, 0, FX(0, 0, 0, 0), PAT(3)},
    {"FUNK KEY", {-8, 8, 24, 40, 90, 25, 20, 0}, {0, 45, 30, 30}, 0, 0, FX(0, 0, 0, 0), PAT(6)},
    /* the FM drum machines (E7 = MACH): E4..E6 are their macros (eng_digital.c MACH_D) */
    {"BD", {0, 64, 60, 0, 0, 0, 0, 1}, {0, 100, 0, 40}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
    {"SD", {-24, 70, 71, 16, 35, 0, 0, 2}, {0, 84, 0, 40}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
    {"CH", {-44, 95, 90, 29, 103, 0, 0, 3}, {0, 58, 0, 30}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
    {"OH", {-44, 95, 90, 29, 103, 0, 0, 3}, {0, 95, 0, 60}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
    {"CYM", {0, 117, 70, 42, 47, 0, 0, 4}, {0, 108, 0, 70}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
    {"TOM", {0, 8, 50, 70, 30, 0, 0, 5}, {0, 92, 0, 40}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
};
#define FM4_NPRESETS 14u
_Static_assert(NELEM(DIGITAL_PRESETS) == FM4_NPRESETS, "DIGITAL presets");

/* EDIT: the ranges are the stores' (a value of engine 1 is clamped to them before it converts) */
/* SIMPLE: E0 ALG / TUNE -64..63 (SYN: (E0 + 64) / 16 = the algorithm 0..7), E1..E3 the ratios in 1/8 steps (8 = 1.0, up to
 * 15.9; the drum machines' macros), E4..E6 0..127, E7 the machine */
#define FM4_EDIT(l0, l1, l2, l3, l4, l5, l6, f0, f1, n0, n1) {                                              \
        {l0, F_INT, -64, 63, 0, 0, 0}, {l1, F_INT, 0, 127, 8, 0, 0}, {l2, F_INT, 0, 127, 8, 0, 0},         \
        {l3, F_INT, 0, 127, 8, 0, 0}, {l4, F_PCT, 0, 127, 60, 0, 0}, {l5, f1, 0, 127, 60, 0, 0},            \
        {l6, F_PCT, 0, 127, 0, 0, 0}, {"MACH", F_ENUM, 0, 5, 0, N_MACH, 0}}


/* the filter: a TPT state-variable filter (dsp.c tsvf_*), LP / BP / HP (TYPE 0 1 2) */
#define ANALOG_K(res) (8192 - (res) * 7600 / 127)     /* the damping, Q12 */
static inline int32_t analog_tap(const tsvf_t *c, int32_t k, int32_t in, int32_t *ic1, int32_t *ic2, uint32_t ft)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    if (!ft)
        return v2;
    return ft == 1u ? v1 : in - ((k * v1) >> 12) - v2;
}

static const uint16_t RATIO_Q8[15] = {128, 256, 512, 768, 1024, 1280, 1536, 1792, 2048, 2304, 2560, 2816,
                                      3072, 3584, 4096};

/* SIMPLE: a ratio / macro value 0..127 is a frequency ratio in 1/8 steps (8 = 1.0; never 0: fm_ratio_inc divides), the
 * algorithm is (E0 + 64) / 16, TUNE (E0, -64..63) moves a drum machine 3/16 semitone a step (+-12 st, a table of 2^(k/12) Q8) */
static inline uint32_t fm_rq(int32_t v) { return (uint32_t)(v < 1 ? 1 : v) * 32u; }
#define DIG_ALG(p) ((((uint32_t)(p)[P_E0] + 64u) >> 4) & 7u)
#ifdef FELUCCA_LAB                                    /* web/ lab build only: the machines' inner constants as live values (web_emu/wasm_main.c);
                                                       * the defaults are the firmware's own numbers, so the firmware itself is unchanged */
static int32_t lab_hid[64] = {28921, 199, 1, 16, 0, 8, 0, 0, 86, 0, 53,
                                 44, 29, 416, 16384, 19133, 258, 22000, 13520, 240, 0, 1, 0, 1, 1, 0, 98,
                                 370, 493, 2, 1, 26125, 258, 1, 1500, 200, 1, 1, 1, 2, 0, 0, 1, 3};
#define LAB(i, d) (lab_hid[i])
#else
#define LAB(i, d) (d)
#endif
static inline int32_t lab_pow(int32_t e, int32_t c) { return c <= 0 ? e : c == 1 ? mulq15(e, e) : mulq15(mulq15(e, e), e); }
static const uint16_t SEMI_Q8[26] = {128, 136, 144, 152, 161, 171, 181, 192, 203, 215, 228, 242, 256, 271, 287, 304, 323, 342, 362, 384, 406, 431, 456, 483, 512, 542};
static uint32_t tune_q8(int32_t v)
{
    int32_t t = v * 3, s = (t >> 4) + 12, f = t & 15;
    s = clamp(s, 0, 24);
    return (uint32_t)(SEMI_Q8[s] + (((int32_t)SEMI_Q8[s + 1] - SEMI_Q8[s]) * f >> 4));
}

static int32_t digital_env[NPART][NVOICE][4];
static uint8_t digital_stage[NPART][NVOICE][4];

static void digital_note_on(track_t *t, voice_t *v)
{
    uint32_t tr = (uint32_t)(t - trk), vi = (uint32_t)(v - t->v), op;
    for (op = 0; op < 4u; op++) {
        digital_env[tr][vi][op] = t->p[P_FM1_ATK + op * 5u] ? 0 : 1 << 24;
        digital_stage[tr][vi][op] = t->p[P_FM1_ATK + op * 5u] ? 1 : 2;
    }
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
    v->s[7] = 0;                 /* op 4 phase */
    v->s[0] = v->s[1] = 0;       /* the filter */
    v->s[2] = t->p[P_FE_ATK] ? 0 : 1 << 24;   /* the filter envelope: level, Q24 */
    v->s[3] = t->p[P_FE_ATK] ? 1 : 2;         /* .. its stage: 1 attack, 2 decay to END */
    v->s[4] = 1 << 24;           /* modulator envelope, Q24 */
    v->s[5] = v->s[6] = 0;       /* feedback history */
    if (t->p[P_E7] > 0) {        /* a drum machine: s[6] its noise, s[7] its fast sweep envelope */
        v->s[6] = (int32_t)(0x2545F491u ^ (vi * 0x9E3779B9u));
        v->s[7] = 1 << 24;
        if (t->p[P_E7] == 1)
            v->ph[0] = (uint32_t)LAB(6, 0) << 24;            /* BD: the sine's start phase (0 = a rising zero crossing) */
        else if (t->p[P_E7] == 2)
            v->ph[0] = (uint32_t)LAB(22, 0) << 24;           /* SD: the tone's start phase */
    }
}

/* operator increment = carrier * ratio (Q8) without 32-bit overflow; kept below Nyquist */
static inline uint32_t fm_ratio_inc(uint32_t inc, uint32_t r)
{
    uint32_t lim = 0x73000000u / r;                      /* (inc >> 8) * r must stay < 0x73000000 */
    return (inc >> 8) > lim ? 0x73000000u : (inc >> 8) * r;
}

static inline uint32_t digital_mod(int32_t x, int32_t idx) { return (uint32_t)(x * idx) * 2065u; }

/* the filter after the operators: ANALOG's (eng_analog.c: Simper's SVF, LP / BP / HP, drive, soft knee) */
static inline int32_t digital_filter(const tsvf_t *f, int32_t fk, uint32_t ft, int32_t s, int32_t *ic1, int32_t *ic2)
{
    int32_t y = ft ? analog_tap(f, fk, s >> 1, ic1, ic2, ft) : tsvf_lp(f, s >> 1, ic1, ic2);
    int32_t a = y < 0 ? -y : y;
    if (a > 16000) {
        a = 16000 + (softclip((a - 16000) * 2) >> 1);
        y = y < 0 ? -a : a;
    }
    return y << 1;
}

static __attribute__((noinline)) void digital_render_legacy(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t alg = DIG_ALG(p), i;
    uint32_t i1 = m->inc;
    uint32_t i2 = fm_ratio_inc(m->inc, fm_rq(clamp(p[P_E1] + ((m->envq15 * p[P_ED_FX]) >> 14), 0, 127))), i3 = fm_ratio_inc(m->inc, fm_rq(p[P_E2]));
    uint32_t i4 = fm_ratio_inc(m->inc, fm_rq(p[P_E3]));
    int32_t me, idx, fb = p[P_E6], fb1, fb2;                 /* fb1, fb2: op 4 feedback history */
    uint32_t ph0, ph1, ph2, ph4;
    uint32_t mach = FELUCCA_SIMPLE ? (uint32_t)p[P_E7] % 6u : 0u, mi0 = 0, mi1 = 0, mi2 = 0;   /* 0 = SYN, 1 BD 2 SD 3 HH 4 CYM 5 PERC */
    int32_t mT = 32767, mA = 0, mB = 0, mC = 0, mD = 0, mE = 0, mF = 0, mG = 0, mH = 0, mR = 1, mHold = 0;
    int32_t cut = (p[P_FLT_CUT] << 8) + m->cutoff + (p[P_FLT_KTR] * (v->pitch16 - 60 * 16) >> 4);
    {   /* the filter envelope (ATK DEC SUS REL, AMT deep, per block): 0 .. 1 in Q24 */
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
    }
    int32_t drive = 32768 + p[P_FLT_DRV] * 512, drv = p[P_FLT_DRV], ic1 = v->s[0], ic2 = v->s[1];
    int32_t fk = ANALOG_K(p[P_FLT_RES]);
    uint32_t ft = (uint32_t)p[P_FTYPE];
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_FLT_RES]);
    /* modulator envelope: decays (MODDEC) towards 25 %, per block */
    if (!mach)
        v->s[4] += mulq16((1 << 22) - v->s[4], ENV_EXP[p[P_E5] & 127]);
    me = v->s[4] >> 9;                                         /* Q15 */
    idx = (p[P_E4] * me) >> 15;                                 /* 0..127 */
    idx = clamp(idx + ((m->shape - (64 << 8)) >> 8) + (v->vel - 96) / 4, 0, 127);
    ph0 = v->ph[0];                                             /* state in locals: out[] may alias v->s[] */
    ph1 = v->ph[1];
    ph2 = v->ph[2];
    ph4 = (uint32_t)v->s[7];
    fb1 = v->s[5];
    fb2 = v->s[6];
    if (mach) {   /* the machine's block setup: s[4] its main envelope (rate from E2 / E3), ph4 a fast sweep, both Q24 decaying to 0 */
        int32_t en = v->s[4], sw = (int32_t)ph4, a1 = p[P_E1], a2 = p[P_E2], a3 = p[P_E3], e4 = p[P_E4];
        int32_t a4s = e4;
        uint32_t bi = fm_ratio_inc(m->inc, tune_q8(p[P_E0]));    /* TUNE */
        en -= mulq16(en, ENV_EXP[(mach == 5u ? a3 : a2) & 127]);
        sw -= mulq16(sw, ENV_EXP[mach == 2u ? clamp(LAB(11, 44) + ((a4s * LAB(26, 98)) >> 8), 0, 127) : mach == 1u ? LAB(8, 86) : 44]);   /* (SD: its pitch drop lasts ~17 ms, long enough to hear) */
        v->s[4] = en;
        ph4 = (uint32_t)sw;
        mA = en >> 9;                                                /* Q15 */
        mB = sw >> 9;
        mE = e4;
        mF = p[P_E5] ? 32 + p[P_E5] * 2 : 0;                         /* FOLD: a wavefolder after the voice */
        mG = clamp((p[P_E6] * 13 + 63) / 127, 0, 13);                /* CRSH: bits dropped, from the first notch */
        mR = 1 + (p[P_E6] >> (mach == 3u || mach == 4u ? 3 : 4));                                     /* .. and the sample rate held (1..8 samples) */
        if (mach == 1) {                                             /* BD: TUNE PNCH SWPT CLIK | FM FOLD CRSH */
            mi0 = fm_ratio_inc(bi, 256 + ((lab_pow(mA, LAB(4, 0)) * a1 * LAB(3, 16)) >> 15));   /* PNCH: the pitch starts up to 8x high, falls with SWPT */
            mC = mulq15(lab_pow(LAB(9, 0) ? mB : mA, LAB(2, 1)), a3 * LAB(1, 199));   /* CLIK: noise, gain follows the envelope squared */
            mD = (e4 * LAB(5, 8)) >> 5;                                 /* FM: the sine's feedback index */
        } else if (mach == 2) {                                      /* SD: TUNE NOIS NDEC O2 | SWP FOLD CRSH */
            mi0 = fm_ratio_inc(bi, 256 + ((mB * e4 * LAB(12, 29)) >> 17));          /* SWP: up to 3x at the hit, falling to TUNE */
            mi1 = fm_ratio_inc(mi0, fm_rq(a3));                         /* O2: the second sine, a ratio of the first (1/8 steps) */
            mC = a1 * LAB(16, 258);
            mD = LAB(18, 13520);                                       /* the noise's high-pass corner (fixed) */
            mH = mulq15(lab_pow(mB, LAB(21, 1)), LAB(17, 22000));
            mT = 32767 - mulq15(32767 - mA, clamp(LAB(25, 0) * 258, 0, 32767));                        /* the crack: a few ms of noise at the hit */
        } else if (mach <= 4u) {                                     /* HH / CYM: TUNE METL TONE R2 | R3 FOLD CRSH */
            mi0 = bi;
            mi1 = fm_ratio_inc(bi, 64u + (uint32_t)a3 * 8u);              /* R2, R3: osc 2 and 3 as ratios of osc 1, (64 + 8 R) / 256 */
            mi2 = fm_ratio_inc(bi, 64u + (uint32_t)e4 * 8u);
            mC = a1 * LAB(32, 258);
            mD = LAB(34, 1500) + a2 * LAB(35, 200);
            mE = LAB(43, mach == 3u ? 3 : 0);                            /* the fixed FM of osc 2 (a hat 3, a cymbal 0) */
            mT = 32767 - mulq15(32767 - mA, clamp(LAB(41, 0) * 258, 0, 32767));
        } else {                                                     /* PERC: TUNE RATIO IDX IDEC | SWP FOLD CRSH */
            mi0 = fm_ratio_inc(bi, 256 + ((mB * e4) >> 12));
            mi1 = fm_ratio_inc(bi, fm_rq(a1));
            mC = (a2 * mA) >> 15;
        }
    }
    for (i = 0; i < n; i++) {
        int32_t o1, o2, o3, o4, s;
        if (mach) {
            int32_t nz = ((int32_t)(noise32(&fb2) >> 17) - 16384) * 2;
            if (mach == 1) {
                o1 = sine_i(ph0 + digital_mod(fb1, mD));
                fb1 = o1;
                s = mulq15(LAB(10, 53) ? softclip((o1 * (32 + LAB(10, 53))) >> 5) : o1, LAB(0, 28921)) + mulq15(nz, mC);
                ph0 += mi0;
            } else if (mach == 2) {
                fb1 += mulq15(nz - fb1, mD);                         /* high-passed noise */
                o1 = mulq15(sine_i(ph0) + ((sine_i(ph1) * LAB(14, 16384)) >> 15), LAB(15, 19133));
                if (LAB(23, 1)) o1 = softclip((o1 * (32 + LAB(23, 1))) >> 5);
                if (LAB(25, 0)) o1 = mulq15(o1, mT);
                s = mulq15(o1, 32767 - (mC >> LAB(24, 1))) + mulq15(mulq15(nz - fb1, lab_pow(mA, LAB(20, 0))), mC) + mulq15(nz - fb1, mH);
                ph0 += mi0;
                ph1 += mi1;
            } else if (mach <= 4u) {
                int32_t sq = ((ph0 >> 31) ^ ((ph1 + digital_mod(sine_i(ph0), mE)) >> 31) ^ (LAB(37, 1) ? (ph2 >> 31) : 0) ^ ((ph0 * (uint32_t)LAB(38, mach == 3u ? 1 : 5)) >> 31)) ? LAB(31, mach == 3u ? 26125 : 16000) : -LAB(31, mach == 3u ? 26125 : 16000);
                int32_t mx = mulq15(LAB(41, 0) ? mulq15(sq, mT) : sq, mC) + mulq15(nz, 32767 - (mC >> LAB(33, 1)));
                fb1 += mulq15(mx - fb1, mD);
                s = (mx - fb1) * LAB(39, 2);
                ph0 += mi0;
                ph1 += mi1;
                ph2 += mi2;
            } else {
                s = sine_i(ph0 + digital_mod(sine_i(ph1), mC));
                ph0 += mi0;
                ph1 += mi1;
            }
            if (mF)
                s = sine_i((uint32_t)((s * mF) >> 5) << 16);
            if (mR > 1) {
                if ((i % (uint32_t)mR) == 0)
                    mHold = s;
                s = mHold;
            }
            if (mG)
                s = (s >> mG) << mG;
        } else {
        o4 = sine_i((ph4 + digital_mod((fb1 + fb2) >> 1, fb)));
        fb2 = fb1;
        fb1 = o4;
        switch (alg) {
        case 0:
            o3 = sine_i((ph2 + digital_mod(o4, idx)));
            o2 = sine_i((ph1 + digital_mod(o3, idx)));
            s = sine_i((ph0 + digital_mod(o2, idx)));
            break;
        case 1:
            o3 = sine_i(ph2);
            o2 = sine_i((ph1 + digital_mod((o3 + o4) >> 1, idx)));
            s = sine_i((ph0 + digital_mod(o2, idx)));
            break;
        case 2:
            o3 = sine_i(ph2);
            o2 = sine_i((ph1 + digital_mod(o3, idx)));
            s = sine_i((ph0 + digital_mod((o2 + o4) >> 1, idx)));
            break;
        case 3:
            o3 = sine_i((ph2 + digital_mod(o4, idx)));
            o2 = sine_i(ph1);
            s = sine_i((ph0 + digital_mod((o2 + o3) >> 1, idx)));
            break;
        case 4:
            o3 = sine_i((ph2 + digital_mod(o4, idx)));
            o2 = sine_i(ph1);
            o1 = sine_i((ph0 + digital_mod(o2, idx)));
            s = (o1 + o3) >> 1;
            break;
        case 5:
            o3 = sine_i((ph2 + digital_mod(o4, idx)));
            o2 = sine_i((ph1 + digital_mod(o4, idx)));
            o1 = sine_i((ph0 + digital_mod(o4, idx)));
            s = mulq15(o1 + o2 + o3, 10923);           /* / 3 without a divide */
            break;
        case 6:
            o3 = sine_i((ph2 + digital_mod(o4, idx)));
            o2 = sine_i(ph1);
            o1 = sine_i(ph0);
            s = mulq15(o1 + o2 + o3, 10923);           /* / 3 without a divide */
            break;
        default:
            o3 = sine_i(ph2);
            o2 = sine_i(ph1);
            o1 = sine_i(ph0);
            s = (o1 + o2 + o3 + o4) >> 2;
            break;
        }
        ph0 += i1;
        ph1 += i2;
        ph2 += i3;
        ph4 += i4;
        }
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        s = digital_filter(&flt, fk, ft, s, &ic1, &ic2);
        out[i] += voice_amp(s, m, i);   /* full-scale sines: 6 dB below the others */
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[7] = (int32_t)ph4;
    v->s[5] = fb1;
    v->s[6] = fb2;
    v->s[0] = ic1;
    v->s[1] = ic2;
}

/* Independent operator envelopes use a shared 640-byte runtime table, never
 * retained state. Transparent defaults retain the original DSP path exactly. */
static int digital_custom(const track_t *t)
{
    uint32_t op;
    (void)t; (void)op;
    return 0;                                   /* the operator envelopes are gone: ids 61..64 are the filter's */
    for (op = 0; op < 4u; op++) {
        const int16_t *p = &t->p[P_FM1_ATK + op * 5u];
        if (p[0] || p[1] || p[2] != 127 || p[3] || p[4] != 127) return 1;
    }
    return 0;
}

static void digital_op_tick(track_t *t, voice_t *v, int32_t gain[4])
{
    uint32_t tr = (uint32_t)(t - trk), vi = (uint32_t)(v - t->v), op;
    for (op = 0; op < 4u; op++) {
        const int16_t *p = &t->p[P_FM1_ATK + op * 5u];
        int32_t e = digital_env[tr][vi][op], sus = p[2] == 127 ? 1 << 24 : (int32_t)p[2] * (1 << 17);
        uint8_t st = digital_stage[tr][vi][op];
        if (!p[0] && !p[1] && p[2] == 127 && !p[3]) {
            /* Flat operator envelope: LEVEL edits retain the master release. */
            digital_env[tr][vi][op] = 1 << 24;
            gain[op] = (int32_t)p[4] * 258;                 /* 127 -> 32766: no divide */
            continue;
        }
        if (!v->gate) st = 3;
        if (st == 1) {
            e += (int32_t)ENV_LIN[p[0] & 127];
            if (e >= (1 << 24)) { e = 1 << 24; st = 2; }
        } else if (st == 2) {
            if (!p[1]) e = p[2] == 127 ? 1 << 24 : sus;
            else e += mulq16(sus - e, ENV_EXP[p[1] & 127]);
        } else if (st == 3) {
            if (!p[3]) e = 0;
            else e -= mulq16(e, ENV_EXP[p[3] & 127]);
        }
        e = clamp(e, 0, 1 << 24);
        digital_env[tr][vi][op] = e;
        digital_stage[tr][vi][op] = st;
        gain[op] = ((e >> 9) * ((int32_t)p[4] * 258)) >> 15;
    }
}

static __attribute__((noinline)) void digital_render_custom(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t gains[4];
    digital_op_tick(t, v, gains);
    uint32_t alg = DIG_ALG(p), i;
    uint32_t i1 = m->inc;
    uint32_t i2 = fm_ratio_inc(m->inc, fm_rq(clamp(p[P_E1] + ((m->envq15 * p[P_ED_FX]) >> 14), 0, 127))), i3 = fm_ratio_inc(m->inc, fm_rq(p[P_E2]));
    uint32_t i4 = fm_ratio_inc(m->inc, fm_rq(p[P_E3]));
    int32_t me, idx, fb = p[P_E6], fb1, fb2;                 /* fb1, fb2: op 4 feedback history */
    uint32_t ph0, ph1, ph2, ph4;
    /* modulator envelope: decays (MODDEC) towards 25 %, per block */
    v->s[4] += mulq16((1 << 22) - v->s[4], ENV_EXP[p[P_E5] & 127]);
    me = v->s[4] >> 9;                                         /* Q15 */
    idx = (p[P_E4] * me) >> 15;                                 /* 0..127 */
    idx = clamp(idx + ((m->cutoff + m->shape - (64 << 8)) >> 8) + (v->vel - 96) / 4, 0, 127);
    ph0 = v->ph[0];                                             /* state in locals: out[] may alias v->s[] */
    ph1 = v->ph[1];
    ph2 = v->ph[2];
    ph4 = (uint32_t)v->s[7];
    fb1 = v->s[5];
    fb2 = v->s[6];
    for (i = 0; i < n; i++) {
        int32_t o1, o2, o3, o4, s;
        o4 = mulq15(sine_i((ph4 + digital_mod((fb1 + fb2) >> 1, fb))), gains[3]);
        fb2 = fb1;
        fb1 = o4;
        switch (alg) {
        case 0:
            o3 = mulq15(sine_i((ph2 + digital_mod(o4, idx))), gains[2]);
            o2 = mulq15(sine_i((ph1 + digital_mod(o3, idx))), gains[1]);
            s = mulq15(sine_i((ph0 + digital_mod(o2, idx))), gains[0]);
            break;
        case 1:
            o3 = mulq15(sine_i(ph2), gains[2]);
            o2 = mulq15(sine_i((ph1 + digital_mod((o3 + o4) >> 1, idx))), gains[1]);
            s = mulq15(sine_i((ph0 + digital_mod(o2, idx))), gains[0]);
            break;
        case 2:
            o3 = mulq15(sine_i(ph2), gains[2]);
            o2 = mulq15(sine_i((ph1 + digital_mod(o3, idx))), gains[1]);
            s = mulq15(sine_i((ph0 + digital_mod((o2 + o4) >> 1, idx))), gains[0]);
            break;
        case 3:
            o3 = mulq15(sine_i((ph2 + digital_mod(o4, idx))), gains[2]);
            o2 = mulq15(sine_i(ph1), gains[1]);
            s = mulq15(sine_i((ph0 + digital_mod((o2 + o3) >> 1, idx))), gains[0]);
            break;
        case 4:
            o3 = mulq15(sine_i((ph2 + digital_mod(o4, idx))), gains[2]);
            o2 = mulq15(sine_i(ph1), gains[1]);
            o1 = mulq15(sine_i((ph0 + digital_mod(o2, idx))), gains[0]);
            s = (o1 + o3) >> 1;
            break;
        case 5:
            o3 = mulq15(sine_i((ph2 + digital_mod(o4, idx))), gains[2]);
            o2 = mulq15(sine_i((ph1 + digital_mod(o4, idx))), gains[1]);
            o1 = mulq15(sine_i((ph0 + digital_mod(o4, idx))), gains[0]);
            s = mulq15(o1 + o2 + o3, 10923);           /* / 3 without a divide */
            break;
        case 6:
            o3 = mulq15(sine_i((ph2 + digital_mod(o4, idx))), gains[2]);
            o2 = mulq15(sine_i(ph1), gains[1]);
            o1 = mulq15(sine_i(ph0), gains[0]);
            s = mulq15(o1 + o2 + o3, 10923);           /* / 3 without a divide */
            break;
        default:
            o3 = mulq15(sine_i(ph2), gains[2]);
            o2 = mulq15(sine_i(ph1), gains[1]);
            o1 = mulq15(sine_i(ph0), gains[0]);
            s = (o1 + o2 + o3 + o4) >> 2;
            break;
        }
        ph0 += i1;
        ph1 += i2;
        ph2 += i3;
        ph4 += i4;
        out[i] += voice_amp(s, m, i);   /* full-scale sines: 6 dB below the others */
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[7] = (int32_t)ph4;
    v->s[5] = fb1;
    v->s[6] = fb2;
}

static void digital_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (digital_custom(t)) digital_render_custom(t, v, out, n, m);
    else digital_render_legacy(t, v, out, n, m);
}

/* the macros of the drum machines (E7 = MACH): the same ranges and defaults as EDIT, other names */
#define MD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define MD_T MD("TUNE", F_INT, -64, 63, 0)
#define MD_X(l) MD(l, F_PCT, 0, 127, 8)
static const param_desc_t MACH_D[5][7] = {
    {MD_T, MD_X("PNCH"), MD_X("SWPT"), MD_X("CLIK"), MD("FM", F_PCT, 0, 127, 60), MD("FOLD", F_PCT, 0, 127, 60), MD("CRSH", F_PCT, 0, 127, 0)},
    {MD_T, MD_X("NOIS"), MD_X("NDEC"), MD("O2", F_INT, 0, 127, 16), MD("SWP", F_PCT, 0, 127, 35), MD("FOLD", F_PCT, 0, 127, 0), MD("CRSH", F_PCT, 0, 127, 0)},
    {MD_T, MD_X("METL"), MD_X("TONE"), MD("R2", F_INT, 0, 127, 29), MD("R3", F_INT, 0, 127, 103), MD("FOLD", F_PCT, 0, 127, 0), MD("CRSH", F_PCT, 0, 127, 0)},
    {MD_T, MD_X("METL"), MD_X("TONE"), MD("R2", F_INT, 0, 127, 42), MD("R3", F_INT, 0, 127, 47), MD("FOLD", F_PCT, 0, 127, 0), MD("CRSH", F_PCT, 0, 127, 0)},
    {MD_T, MD("RATO", F_INT, 0, 127, 8), MD_X("IDX"), MD_X("IDEC"), MD("SWP", F_PCT, 0, 127, 60), MD("FOLD", F_PCT, 0, 127, 60), MD("CRSH", F_PCT, 0, 127, 0)},
};
static const param_desc_t *digital_desc(const track_t *t, uint32_t k)
{
    uint32_t mach = (uint32_t)t->p[P_E7] % 6u;
    return mach && k < 7u ? &MACH_D[mach - 1u][k] : 0;
}

static const engine_t ENG_DIGITAL = {
    .name = "DIGITAL",
    .page_title = {"OPS", "MOD"},
    .edit = FM4_EDIT("ALG", "R2", "R3", "R4", "IDX", "MDEC", "FB", F_ENUM, F_TIME, N_FMALG, N_RATIO),
    .presets = DIGITAL_PRESETS,
    .npresets = NELEM(DIGITAL_PRESETS),
    .note_on = digital_note_on,
    .render = digital_render,
    .knob = {P_E7, P_E0, P_E1, P_E2},
    .desc = digital_desc,
    .poly = 4,                   /* the 4-voice poly machine; the shared budget of 8 still holds */
    .keep = 0x63,                /* the filter and feedback history; the modulator envelope and (drums) the pitch sweep restart on a
                                  * hit that finds its voice still sounding (the same drum twice in a row) */
};

