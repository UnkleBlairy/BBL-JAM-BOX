/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), FM-1 Groovebox */
/* ANALOG: the 808 machines, KICK SNARE TOM RIM CLAP COWBELL HAT CYMBAL (E7 = MACH), and AGE. The spec is
 * docs/808lab.html: each machine is its WebAudio graph for one hit (the v* voices there), here in fixed point with
 * the same ranges and curves:
 *   - oscillators are phase accumulators (sines from the table, squares from the phase's top bit: not band-limited),
 *   - the filters are the prototype's biquads (BiquadFilterNode's: its LP / HP Q is in dB, BP's is plain), their
 *     coefficients made once at the hit,
 *   - an envelope is the prototype's env(): from 0.0001 a straight line to its peak, then exponential to 0.0001
 *     (a factor every sample, from the decay time at the hit); a pitch sweep is an exponential ramp the same way.
 * Then the track chain of the prototype (applyFx): the hit's level, LEVEL x 1.1, a tanh shaper (DRIVE, GRIT), GRIT's
 * bits and low-pass. Each hit draws its AGE variations (hit(): DRIFT's pitch, decay and level, a late start, HISS's
 * noise through the hit's own decay). All the maths of a hit's setup is done at the note (64-bit, no floats); the
 * samples need no divide. A track plays one hit at a time (voice.c trk_note_on: a drum), a restart in place fades the
 * old one's last value out over AN_DECLICK samples.
 * Knobs: E0..E6 the machine's first seven, P_LN0 its LEVEL (the eighth), P_LN1 AGE (params.c, engines.c). */

/* AGE's three parts: their weights, as the prototype's sliders (0..127, 64 = 1.0). One knob, AGE, sets how much of
 * all three; these say how much of each. (The final numbers come later: change them here) */
#define AGE_DRIFT 64                  /* per-hit pitch (and a slow walk), decay and level, a late start up to 8 ms */
#define AGE_HISS 64                   /* a burst of high-passed noise with each hit, through the hit's decay */
#define AGE_GRIT 64                   /* the shaper driven harder, down to ~7 bits, a low-pass falling to ~3.6 kHz */

enum { AN_KICK, AN_SNARE, AN_TOM, AN_RIM, AN_CLAP, AN_COW, AN_HAT, AN_CYM, AN_N };
static const char *const N_AMACH[AN_N] = {"KICK", "SNARE", "TOM", "RIM", "CLAP", "COWB", "HAT", "CYM"};
/* each machine's eight knobs (the spec's MACH table) and their defaults; the eighth (LEVEL) is P_LN0 */
static const char *const AN_LABEL[AN_N][8] = {
    {"TUNE", "DECAY", "CLICK", "TONE", "SWEEP", "S.TIM", "DRIVE", "LEVEL"},
    {"TUNE", "TONE", "SNAP", "DECAY", "BODY", "B.DEC", "DRIVE", "LEVEL"},
    {"TUNE", "DECAY", "SWEEP", "CLICK", "TONE", "NOISE", "DRIVE", "LEVEL"},
    {"TUNE", "DECAY", "TONE", "CLICK", "RATIO", "Q", "DRIVE", "LEVEL"},
    {"TUNE", "DECAY", "SPRD", "TONE", "BURST", "ROOM", "DRIVE", "LEVEL"},
    {"TUNE", "DECAY", "TONE", "CLICK", "DETUN", "Q", "DRIVE", "LEVEL"},
    {"TUNE", "DECAY", "TONE", "METAL", "HPF", "SNAP", "DRIVE", "LEVEL"},
    {"TUNE", "DECAY", "TONE", "METAL", "SHIM", "BODY", "DRIVE", "LEVEL"},
};
static const uint8_t AN_DEF[AN_N][8] = {
    {55, 70, 45, 80, 50, 40, 15, 110}, {64, 70, 80, 60, 80, 60, 20, 105}, {50, 64, 20, 30, 90, 20, 10, 105},
    {64, 40, 70, 50, 64, 60, 15, 100}, {64, 60, 50, 50, 64, 40, 15, 100}, {64, 50, 64, 40, 46, 50, 10, 95},
    {64, 25, 64, 80, 60, 50, 10, 90}, {64, 80, 70, 80, 80, 40, 10, 85},
};
#define AN_AGE_DEF 40                 /* AGE at a machine's start (the prototype's) */
/* each machine's level out, after its sound (Q12, 4096 = as the prototype): the prototype's hats, cymbal and clap are
 * 10..19 dB under its kick, its master compressor evens that out; these put each at about its FM machine's level.
 * Only the level: the sound (its drive, GRIT) is the prototype's. 4096 for all = the prototype's own balance */
static const uint16_t AN_TRIM[AN_N] = {4096, 4096, 4096, 5120, 12800, 5120, 36000, 18400};
#define AN_NOTE 60                    /* the note a machine sounds at its TUNE; the keys play it up and down */

static const preset_t ANALOG_PRESETS[AN_N] = {             /* AN_DEF's first seven, MACH (LEVEL, AGE: machine_defaults) */
#define AP(n, a, b, c, d, e, f, g, m) {n, {a, b, c, d, e, f, g, m}, {0, 100, 0, 40}, 0, 0, FX(0, 0, 0, 0), PAT(0)}
    AP("KICK", 55, 70, 45, 80, 50, 40, 15, 0), AP("SNARE", 64, 70, 80, 60, 80, 60, 20, 1),
    AP("TOM", 50, 64, 20, 30, 90, 20, 10, 2), AP("RIM", 64, 40, 70, 50, 64, 60, 15, 3),
    AP("CLAP", 64, 60, 50, 50, 64, 40, 15, 4), AP("COWBELL", 64, 50, 64, 40, 46, 50, 10, 5),
    AP("HAT", 64, 25, 64, 80, 60, 50, 10, 6), AP("CYMBAL", 64, 80, 70, 80, 80, 40, 10, 7),
#undef AP
};

/* ---------------------------------------------------------------- the hit's maths (at the note: 64-bit, Q16) --- */
#define AQ(x) ((int64_t)((x) * 65536.0 + 0.5))       /* a constant as Q16 (folded by the compiler: no float code) */
#define AN_FLOOR 26844u                             /* 0.0001 in Q28: where the prototype's envelopes start and end */
#define AN_DECLICK 64u

/* 2^(x / 65536), Q16 (x up to 14 << 16): the whole part a shift, the rest a Taylor series of e^(f ln 2) in Q30 */
static int64_t an_exp2(int64_t x)
{
    int64_t i = x >> 16, f = (x & 0xFFFF) << 14, p;                     /* f: Q30 */
    p = 16377;                                                          /* ln2^n / n! in Q30, n = 7 .. 0 */
    p = 165394 + ((p * f) >> 30);
    p = 1431680 + ((p * f) >> 30);
    p = 10327387 + ((p * f) >> 30);
    p = 59597083 + ((p * f) >> 30);
    p = 257941248 + ((p * f) >> 30);
    p = 744261118 + ((p * f) >> 30);
    p = 1073741824 + ((p * f) >> 30);
    if (i >= 0)
        return i > 30 ? (int64_t)1 << 46 : (p << i) >> 14;
    return i < -40 ? 0 : (p >> -i) >> 14;
}
/* log2(v / 65536) in Q16 (v > 0): the exponent, then 16 bits of the mantissa by squaring */
static int64_t an_log2(int64_t v)
{
    int64_t r = 0, m;
    int32_t n = 0, b;
    if (v <= 0)
        return -((int64_t)40 << 16);
    while (v >= ((int64_t)2 << 16)) { v >>= 1; n++; }
    while (v < ((int64_t)1 << 16)) { v <<= 1; n--; }
    m = v << 14;                                                        /* Q30, 1 <= m < 2 */
    for (b = 15; b >= 0; b--) {
        m = (m * m) >> 30;
        if (m >= ((int64_t)2 << 30)) {
            m >>= 1;
            r |= (int64_t)1 << b;
        }
    }
    return ((int64_t)n << 16) + r;
}
/* n / d without a 64-bit divide (the target has none): shift and subtract, only over the bits the quotient has */
static int64_t an_div(int64_t n, int64_t d)
{
    uint64_t un = (uint64_t)(n < 0 ? -n : n), ud = (uint64_t)(d < 0 ? -d : d), q = 0;
    int32_t s = 0;
    if (!ud)
        return 0;
    while (ud < un && !(ud >> 62)) {
        ud <<= 1;
        s++;
    }
    for (; s >= 0; s--) {
        q <<= 1;
        if (un >= ud) {
            un -= ud;
            q |= 1u;
        }
        ud >>= 1;
    }
    return (n < 0) != (d < 0) ? -(int64_t)q : (int64_t)q;
}
static int64_t an_pow(int64_t b, int64_t e) { return b <= 0 ? 0 : an_exp2((an_log2(b) * e) >> 16); }   /* b^e, Q16 */
static int64_t an_mul(int64_t a, int64_t b) { return (a * b) >> 16; }
static int64_t an_x(int32_t p) { return (int64_t)((clamp(p, 0, 127) << 16) / 127); }   /* a knob 0..127 as 0..1 */
static uint32_t an_samples(int64_t sec) { int64_t n = (sec * FS) >> 16; return n < 1 ? 1u : (uint32_t)n; }
static uint32_t an_inc(int64_t hz)                                      /* Hz (Q16) -> phase increment (< FS / 2) */
{
    hz = clamp((int32_t)(hz >> 4), 0, (int32_t)(AQ(21000.0) >> 4));
    return (uint32_t)(((uint64_t)hz << 4) * 1558265u >> 20);
}

/* a decay's per-sample factor 1 - d (d Q32): falling by 2^-A (A Q16) over n samples, as 1 - e^-u (u = A ln2 / n) */
static uint32_t an_dec(int64_t a, uint32_t n)
{
    int64_t u = an_div(a * 1488522236, (int64_t)n << 16), d, t;            /* Q31 */
    if (u > ((int64_t)3 << 29))
        u = (int64_t)3 << 29;                                           /* (0.75: a fall in one or two samples) */
    t = u; d = u;
    t = (t * u) >> 31; d -= t >> 1;
    t = (t * u) >> 31; d += an_div(t, 6);
    t = (t * u) >> 31; d -= an_div(t, 24);
    t = (t * u) >> 31; d += an_div(t, 120);
    t = (t * u) >> 31; d -= an_div(t, 720);
    return d <= 0 ? 1u : (uint32_t)(d << 1);
}

/* ---------------------------------------------------------------- envelopes and sweeps --- */
typedef struct {
    uint32_t v;                  /* the level, Q28; 0 = done */
    int32_t step;                /* the attack's step */
    uint32_t na, n1, pre;        /* samples: of the attack, of the first decay (then d2), before it starts */
    uint32_t d, d2;              /* the decays' factors (Q32: v -= v d) */
} an_env_t;
/* the prototype's env(g, t, pk, dec, at): pk, the decay and the attack in seconds (Q16) */
static void an_env(an_env_t *e, int64_t pk, int64_t dec, int64_t at)
{
    int64_t p28 = pk << 12;
    e->pre = 0;
    e->n1 = 0;
    if (p28 <= (int64_t)AN_FLOOR) {
        e->v = 0;
        return;
    }
    e->v = AN_FLOOR;
    e->na = an_samples(at);
    e->step = (int32_t)an_div(p28 - (int64_t)AN_FLOOR, e->na);
    e->d = e->d2 = an_dec(an_log2(pk) + 870824, an_samples(dec));       /* (log2 of pk / 0.0001) */
}
/* the next sample of an envelope (Q28) */
static inline uint32_t an_env_tick(an_env_t *e)
{
    uint32_t v = e->v;
    if (!v)
        return 0;
    if (e->pre) {
        e->pre--;
        return 0;
    }
    if (e->na) {
        e->na--;
        e->v = v + (uint32_t)e->step;
        return v;
    }
    if (e->n1 && !--e->n1)
        e->d = e->d2;
    e->v = v - (uint32_t)(((uint64_t)v * e->d) >> 32);
    if (e->v < AN_FLOOR)
        e->v = 0;
    return v;
}
/* a pitch sweep: the factor (Q28) from r (Q16, >= 1) down to 1 over sec, then 1 */
typedef struct { uint32_t f, d, n; } an_swp_t;
static void an_sweep(an_swp_t *s, int64_t r, int64_t sec)
{
    s->n = an_samples(sec);
    s->f = (uint32_t)(clamp((int32_t)(r >> 4), 1 << 12, 7 << 12) << 16);   /* (r <= 7: Q28 in 32 bits) */
    s->d = an_dec(an_log2(r), s->n);
}
static inline uint32_t an_sweep_tick(an_swp_t *s)
{
    uint32_t f = s->f;
    if (s->n) {
        s->n--;
        s->f = s->n ? f - (uint32_t)(((uint64_t)f * s->d) >> 32) : 1u << 28;
    }
    return f;
}

/* ---------------------------------------------------------------- filters --- */
/* a biquad as BiquadFilterNode makes it (the Audio EQ Cookbook's LP, HP and constant-0-dB BP), from g = tan(pi f / FS) and
 * k = 1 / Q, so the coefficients need no cosine near 1: with D = 1 + k g + g^2, LP b0 = g^2 / D, HP 1 / D, BP k g / D,
 * a1 = 2 (1 - g^2) / D, a2 = (1 - k g + g^2) / D (b1, b2: +-2 b0, b0 / -b0). Direct form 1, three products a sample,
 * Q29, the output's history kept with 8 more bits (a low corner's poles sit near 1: the rounding stays far down) */
typedef struct { int32_t bl, bh, bb, c1, c2, x1, x2, y1, y2; } an_svf_t;
#define AN_QDB05 61870                                /* k of BiquadFilterNode's LP / HP Q in dB: 0.5 0.6 0.7 */
#define AN_QDB06 61162
#define AN_QDB07 60462
static int32_t an_kq(int64_t q) { return (int32_t)an_div((int64_t)1 << 32, q < 1024 ? 1024 : q); }   /* BP: k = 1 / Q */
static void an_svf_set(an_svf_t *f, int64_t hz, int32_t k)
{
    uint32_t inc = an_inc(hz);
    int64_t th = (int64_t)(((uint64_t)inc * 3373259426u) >> 32), g, g2, kg, r;   /* pi f / FS, Q30 */
    if (th < ((int64_t)1 << 28)) {                    /* tan (Q30): its series below 0.25, sin / cos above */
        int64_t t2 = (th * th) >> 30;
        g = th + an_div((th * t2) >> 30, 3) + an_div(((((th * t2) >> 30) * t2) >> 30) * 2, 15);
    } else {
        int64_t sn = sine_i(inc >> 1), c = sine_i((inc >> 1) + 0x40000000u);
        g = c > 0 ? an_div(sn << 30, c) : (int64_t)60 << 30;
    }
    g2 = (g * g) >> 30;
    kg = ((int64_t)k * g) >> 16;
    r = an_div((int64_t)1 << 59, ((int64_t)1 << 30) + kg + g2);                   /* 1 / D, Q29 */
    memset(f, 0, sizeof *f);
    f->bl = (int32_t)((g2 * r) >> 30);
    f->bh = (int32_t)r;
    f->bb = (int32_t)((kg * r) >> 30);
    f->c1 = (int32_t)((2 * (((int64_t)1 << 30) - g2) * r) >> 30);                 /* -a1, -a2 */
    f->c2 = (int32_t)(-(((((int64_t)1 << 30) - kg + g2) * r) >> 30));
}
static inline int32_t an_bq(an_svf_t *f, int32_t b, int32_t xs)
{
    int32_t y = (int32_t)((((int64_t)b * xs << 8) + (int64_t)f->c1 * f->y1 + (int64_t)f->c2 * f->y2) >> 29);
    f->y2 = f->y1;
    f->y1 = y;
    return y >> 8;
}
static inline int32_t an_lp(an_svf_t *f, int32_t x)
{
    int32_t xs = x + 2 * f->x1 + f->x2;
    f->x2 = f->x1;
    f->x1 = x;
    return an_bq(f, f->bl, xs);
}
static inline int32_t an_hp(an_svf_t *f, int32_t x)
{
    int32_t xs = x - 2 * f->x1 + f->x2;
    f->x2 = f->x1;
    f->x1 = x;
    return an_bq(f, f->bh, xs);
}
static inline int32_t an_bp(an_svf_t *f, int32_t x)                 /* 0 dB at the centre */
{
    int32_t xs = x - f->x2;
    f->x2 = f->x1;
    f->x1 = x;
    return an_bq(f, f->bb, xs);
}

/* tanh(i / 8), i = 0..64, Q15: the shaper (DRIVE, GRIT) */
static const int16_t AN_TANH[65] = {
    0, 4075, 8025, 11742, 15142, 18173, 20812, 23065, 24955, 26518, 27796, 28829, 29659, 30321, 30846, 31261, 31588,
    31845, 32047, 32205, 32328, 32425, 32500, 32559, 32605, 32641, 32669, 32690, 32707, 32720, 32731, 32739, 32745,
    32750, 32754, 32757, 32759, 32761, 32762, 32763, 32764, 32765, 32765, 32766, 32766, 32766, 32766, 32766, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767};
static inline int32_t an_tanh(int32_t u)                 /* tanh(u / 32768) * 32767, u up to 8.0 */
{
    int32_t a = u < 0 ? -u : u, i = a >> 12, y;
    y = i >= 64 ? 32767 : AN_TANH[i] + (((AN_TANH[i + 1] - AN_TANH[i]) * (a & 4095)) >> 12);
    return u < 0 ? -y : y;
}

/* ---------------------------------------------------------------- a hit --- */
typedef struct {
    uint8_t mach, owner, nb, b;  /* the machine; the voice it is (index + 1, 0 free); CLAP's bursts, the one playing */
    uint32_t wait;               /* DRIFT: samples before the hit starts */
    int32_t seed, hseed;         /* the noise, HISS's noise */
    int32_t dc, dcn;             /* the old hit's last value, fading out (a restart in place) */
    int32_t last;                /* the last value out (for that) */
    int32_t lf;                  /* DRIFT: the hit's level (Q16) */
    int32_t pitch16;             /* the note's pitch at the hit (the keys and the LFO move the oscillators from it) */
    uint32_t ph[6], inc[6];      /* the oscillators: phases, increments at the hit */
    int32_t g0, g1;              /* RIM: its squares' levels (Q15) */
    uint32_t bn, bc, bd, btail;  /* CLAP: samples a burst, into this one, the bursts' and the tail's decay */
    an_env_t e[4];               /* 0 the body, 1 2 the other parts (click, snap, noise, room), 3 HISS */
    an_swp_t sw;                 /* the pitch sweep */
    an_svf_t f[4], hf, gl;       /* the machine's filters, HISS's high-pass, GRIT's low-pass */
    int32_t gk;                  /* GRIT: the g gl, gq, gqi are for (-1: none yet) */
    uint32_t gq, gqi;            /* .. its step (Q16) and 2^32 / step */
} an_v_t;
static an_v_t an_v[NTRK][2] __attribute__((section(".pool")));   /* (a track plays one, the other fading out) */
static struct { int32_t walk, rng; } an_t[NTRK];

static inline int32_t an_noise(int32_t *s) { return (int32_t)noise32(s) >> 16; }   /* -1..1, Q15 */
static int64_t an_rnd(int32_t *s) { return (int64_t)(noise32(s) >> 16); }          /* 0..1, Q16 */
static int64_t an_bip(int32_t *s) { return an_rnd(s) * 2 - 65536; }               /* -1..1, Q16 */

static an_v_t *an_of(track_t *t, voice_t *v)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, vi = (uint32_t)(v - t->v) + 1u;
    return an_v[tr][0].owner == vi ? &an_v[tr][0] : an_v[tr][1].owner == vi ? &an_v[tr][1] : 0;
}

/* the voices: P the knobs (0..1, Q16), pf df DRIFT's pitch and decay factors; return the hit's length (s, Q16) */
static int64_t an_kick(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t base = an_mul(an_mul(AQ(20.0), an_pow(AQ(16.0), P[0])), pf), d;
    d = an_mul(AQ(0.05) + an_mul(an_pow(P[1], AQ(2.2)), AQ(3.0)), df);
    h->inc[0] = an_inc(base);
    an_sweep(&h->sw, AQ(1.0) + an_mul(P[4], AQ(5.0)), AQ(0.008) + an_mul(P[5], AQ(0.1)));
    an_env(&h->e[0], AQ(1.0), d, AQ(0.001));
    an_svf_set(&h->f[0], an_mul(AQ(80.0), an_pow(AQ(200.0), P[3])), AN_QDB05);
    if (P[2] > AQ(0.02)) {
        an_svf_set(&h->f[1], AQ(1500.0) + an_mul(P[3], AQ(3000.0)), an_kq(AQ(0.8)));
        an_env(&h->e[1], an_mul(P[2], AQ(1.5)), AQ(0.012), AQ(0.0005));
    }
    return d;
}
static int64_t an_snare(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t tf = an_mul(an_mul(AQ(0.35), an_pow(AQ(8.0), P[0])), pf), bdur, nd;
    bdur = an_mul(AQ(0.05) + an_mul(P[5], AQ(0.2)), df);
    h->inc[0] = an_inc(an_mul(AQ(180.0), tf));
    h->inc[1] = an_inc(an_mul(AQ(330.0), tf));
    an_sweep(&h->sw, AQ(1.25), AQ(0.02));
    an_env(&h->e[0], AQ(0.3) + an_mul(AQ(0.7), P[4]), bdur, AQ(0.001));   /* (the second sine at 0.6 of it) */
    nd = an_mul(AQ(0.03) + an_mul(an_pow(P[3], AQ(2.5)), AQ(1.6)), df);
    an_svf_set(&h->f[0], AQ(300.0) + an_mul(an_mul(P[1], P[1]), AQ(12000.0)), AN_QDB07);
    an_env(&h->e[1], an_mul(P[2], AQ(1.6)), nd, AQ(0.001));
    return bdur > nd ? bdur : nd;
}
static int64_t an_tom(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t base = an_mul(an_mul(AQ(40.0), an_pow(AQ(24.0), P[0])), pf), d;
    d = an_mul(AQ(0.04) + an_mul(an_mul(P[1], P[1]), AQ(3.0)), df);
    h->inc[0] = an_inc(base);
    an_sweep(&h->sw, AQ(1.0) + an_mul(P[2], AQ(6.0)), AQ(0.05));
    an_env(&h->e[0], AQ(1.0), d, AQ(0.001));
    an_svf_set(&h->f[0], an_mul(AQ(300.0), an_pow(AQ(30.0), P[4])), AN_QDB05);
    if (P[3] > AQ(0.02)) {
        an_svf_set(&h->f[1], AQ(1800.0), an_kq(AQ(1.0)));
        an_env(&h->e[1], an_mul(P[3], AQ(1.2)), AQ(0.01), AQ(0.0005));
    }
    if (P[5] > AQ(0.02)) {
        an_svf_set(&h->f[2], an_mul(base, AQ(2.2)), an_kq(AQ(1.2)));
        an_env(&h->e[2], an_mul(P[5], AQ(0.5)), an_mul(AQ(0.06), df), AQ(0.001));
    }
    return d;
}
static int64_t an_rim(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t tf = an_mul(an_mul(AQ(0.33), an_pow(AQ(10.0), P[0])), pf), d;
    d = an_mul(AQ(0.008) + an_mul(an_mul(P[1], P[1]), AQ(0.6)), df);
    h->inc[0] = an_inc(an_mul(AQ(455.0), tf));
    h->inc[1] = an_inc(an_mul(an_mul(AQ(1667.0), tf), AQ(0.7) + an_mul(P[4], AQ(0.7))));
    h->g0 = (int32_t)((AQ(1.0) - an_mul(P[2], AQ(0.8))) >> 1);
    h->g1 = (int32_t)((AQ(0.3) + an_mul(P[2], AQ(0.7))) >> 1);
    an_svf_set(&h->f[0], an_mul(AQ(1700.0), tf), an_kq(AQ(1.5) + an_mul(P[5], AQ(12.0))));
    an_env(&h->e[0], AQ(0.9), d, AQ(0.0005));
    if (P[3] > AQ(0.02)) {
        an_svf_set(&h->f[1], AQ(2500.0), AN_QDB07);
        an_env(&h->e[1], P[3], AQ(0.004), AQ(0.0003));
    }
    return d;
}
static int64_t an_clap(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t tf = an_mul(an_mul(AQ(0.35), an_pow(AQ(8.0), P[0])), pf), sp, tail;
    uint32_t nb = 2u + (uint32_t)((P[4] * 4 + 32768) >> 16);
    sp = AQ(0.003) + an_mul(an_mul(P[2], P[2]), AQ(0.08));
    tail = an_mul(AQ(0.03) + an_mul(an_pow(P[1], AQ(2.4)), AQ(1.6)), df);
    an_svf_set(&h->f[0], an_mul(AQ(1100.0), tf), an_kq(AQ(0.5) + an_mul(an_mul(P[3], P[3]), AQ(20.0))));
    h->nb = (uint8_t)nb;
    h->b = 0;
    h->bn = an_samples(sp);
    h->bc = 0;
    h->bd = an_dec(an_log2(AQ(20.0)), an_samples(an_mul(sp, AQ(0.8))));   /* 1 -> 0.05 over 80 % of a burst */
    h->btail = an_dec(870824, an_samples(tail));                           /* the last: 1 -> 0.0001 over the tail */
    h->e[0].v = 1u << 28;                                                   /* (the bursts' own envelope: an_clap_env) */
    if (P[5] > AQ(0.02)) {
        an_svf_set(&h->f[1], AQ(1800.0), AN_QDB05);
        an_env(&h->e[1], an_mul(P[5], AQ(0.35)), an_mul(AQ(0.2) + an_mul(P[5], AQ(0.6)), df), AQ(0.01));
        h->e[1].pre = (nb - 1u) * h->bn;                                    /* the room: from the last burst */
    }
    return tail + sp * nb;
}
static int64_t an_cow(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t tf = an_mul(an_mul(AQ(0.35), an_pow(AQ(8.0), P[0])), pf), d;
    d = an_mul(AQ(0.03) + an_mul(an_pow(P[1], AQ(2.2)), AQ(2.2)), df);
    h->inc[0] = an_inc(an_mul(AQ(540.0), tf));
    h->inc[1] = an_inc(an_mul(an_mul(AQ(540.0), tf), AQ(1.3) + an_mul(P[4], AQ(0.5))));
    an_svf_set(&h->f[0], an_mul(an_mul(AQ(800.0), tf), an_pow(AQ(6.0), P[2] - AQ(0.5))), an_kq(AQ(1.0) + an_mul(P[5], AQ(8.0))));
    an_env(&h->e[0], AQ(1.0), AQ(0.011), AQ(0.001));                       /* 0.0001 -> 1 in 1 ms -> 0.3 at 12 ms -> */
    h->e[0].d = an_dec(an_log2(AQ(1.0)) - an_log2(AQ(0.3)), an_samples(AQ(0.011)));   /* 0.0001 after d more */
    h->e[0].n1 = an_samples(AQ(0.011));
    h->e[0].d2 = an_dec(an_log2(AQ(0.3)) + 870824, an_samples(d));
    if (P[3] > AQ(0.02)) {
        an_svf_set(&h->f[1], AQ(3000.0), AN_QDB07);
        an_env(&h->e[1], P[3], AQ(0.004), AQ(0.0003));
    }
    return d + AQ(0.012);
}
static const int32_t AN_HR[6] = {65536, 97190, 117965, 166855, 172360, 255394};   /* the 808's six squares (HR, Q16) */
static void an_six(an_v_t *h, int64_t tf, int64_t metal)
{
    int64_t mt = AQ(0.1) + an_mul(metal, AQ(2.2));
    uint32_t i;
    for (i = 0; i < 6u; i++)
        h->inc[i] = an_inc(an_mul(an_mul(AQ(205.3), tf), AQ(1.0) + an_mul(AN_HR[i] - AQ(1.0), mt)));
}
static int64_t an_hat(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t tf = an_mul(an_mul(AQ(0.316), an_pow(AQ(10.0), P[0])), pf), d;
    d = an_mul(AQ(0.02) + an_mul(an_mul(an_mul(P[1], P[1]), P[1]), AQ(4.0)), df);
    an_six(h, tf, P[3]);
    an_svf_set(&h->f[0], AQ(2000.0) + an_mul(P[2], AQ(16000.0)), an_kq(AQ(0.9)));
    an_svf_set(&h->f[1], AQ(4000.0) + an_mul(P[4], AQ(5000.0)), AN_QDB07);
    an_env(&h->e[0], AQ(1.0), d, AQ(0.0005));
    if (P[5] > AQ(0.02)) {
        an_svf_set(&h->f[2], AQ(7000.0), AN_QDB07);
        an_env(&h->e[1], an_mul(P[5], AQ(0.35)), AQ(0.006), AQ(0.0003));
    }
    return d;
}
static int64_t an_cym(an_v_t *h, const int64_t *P, int64_t pf, int64_t df)
{
    int64_t tf = an_mul(an_mul(AQ(0.316), an_pow(AQ(10.0), P[0])), pf), d;
    d = an_mul(AQ(0.15) + an_mul(an_mul(P[1], P[1]), AQ(5.0)), df);
    an_six(h, tf, P[3]);
    an_svf_set(&h->f[0], AQ(2000.0) + an_mul(P[2], AQ(14000.0)), an_kq(AQ(0.6)));
    an_svf_set(&h->f[1], AQ(3500.0), AN_QDB07);
    an_svf_set(&h->f[2], AQ(3000.0) + an_mul(P[4], AQ(15000.0)), AN_QDB06);
    an_env(&h->e[0], AQ(0.9), d, AQ(0.002));
    if (P[5] > AQ(0.02)) {
        an_svf_set(&h->f[3], AQ(2500.0), AN_QDB06);
        an_env(&h->e[1], an_mul(P[5], AQ(0.8)), an_mul(d, AQ(1.2)), AQ(0.003));
    }
    return d;
}
static int64_t (*const AN_VOICE[AN_N])(an_v_t *, const int64_t *, int64_t, int64_t) = {
    an_kick, an_snare, an_tom, an_rim, an_clap, an_cow, an_hat, an_cym};

static void analog_note_on(track_t *t, voice_t *v)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, vi = (uint32_t)(v - t->v) + 1u, i;
    an_v_t *h = an_of(t, v);
    int32_t *rs = &an_t[tr].rng, dc = 0;
    int64_t P[8], a, w, hl, pf, df, lf, dd;
    if (h) {                                          /* a restart in place: the old hit's last value fades out */
        dc = h->last + ((h->dc * h->dcn) >> 6);
    } else {                                          /* a slot no sounding voice of the track has */
        an_v_t *o = &an_v[tr][0];
        h = o->owner && t->v[(o->owner - 1u) % NVOICE].active ? &an_v[tr][1] : o;
    }
    memset(h, 0, sizeof *h);
    h->owner = (uint8_t)vi;
    h->dc = dc;
    h->dcn = dc ? (int32_t)AN_DECLICK : 0;
    h->mach = (uint8_t)((uint32_t)t->p[P_E7] % AN_N);
    h->gk = -1;
    if (!*rs)
        *rs = (int32_t)(0x9E3779B9u * (tr + 1u));
    h->seed = (int32_t)(0x2545F491u ^ (vi * 0x9E3779B9u) ^ (uint32_t)noise32(rs));
    h->hseed = (int32_t)noise32(rs) | 1;
    for (i = 0; i < 7u; i++)
        P[i] = an_x(t->p[P_E0 + i]);
    P[7] = an_x(t->p[P_LN0]);
    /* hit(): AGE's DRIFT and HISS for this hit */
    a = an_x(t->p[P_LN1]);
    w = (int32_t)a * AGE_DRIFT * 2 / 127;                              /* a x DRIFT / 63.5 */
    hl = clamp((int32_t)a * AGE_HISS * 2 / 127, 0, (int32_t)AQ(1.5));
    an_t[tr].walk = clamp(an_t[tr].walk + (int32_t)an_mul(an_bip(rs), AQ(0.4)), -65536, 65536);
    pf = AQ(1.0) + an_mul(an_mul(an_t[tr].walk, AQ(0.025)) + an_mul(an_bip(rs), AQ(0.015)), w);
    df = clamp((int32_t)(AQ(1.0) + an_mul(an_mul(an_bip(rs), AQ(0.25)), w)), (int32_t)AQ(0.5), (int32_t)AQ(1.6));
    lf = clamp((int32_t)(AQ(1.0) + an_mul(an_mul(an_bip(rs), AQ(0.25)), w)), (int32_t)AQ(0.5), (int32_t)AQ(1.5));
    h->wait = (uint32_t)((an_mul(an_mul(an_rnd(rs), AQ(0.008)), w) * FS) >> 16);
    h->lf = (int32_t)lf;
    h->pitch16 = v->pitch16;
    pf = an_mul(pf, an_exp2(((v->pitch16 - AN_NOTE * 16) << 16) / 192));   /* the key played */
    dd = AN_VOICE[h->mach](h, P, pf, df);
    if (hl > AQ(0.01)) {
        an_svf_set(&h->hf, AQ(2500.0), AN_QDB07);
        an_env(&h->e[3], an_mul(hl, AQ(0.4)), dd, AQ(0.001));
    }
    v->s[0] = (int32_t)vi;
}

static int analog_done(track_t *t, voice_t *v)
{
    const an_v_t *h = an_of(t, v);
    return !h || (!h->wait && !h->e[0].v && !h->e[1].v && !h->e[2].v && !h->e[3].v && !h->dcn);
}

/* the prototype's CLAP envelope: each burst 1 -> 0.05 over 80 % of it, held, then 1 again; the last to 0.0001 */
static inline uint32_t an_clap_env(an_v_t *h)
{
    uint32_t v = h->e[0].v;
    if (!v)
        return 0;
    if (h->b + 1u < h->nb) {
        if (h->bc * 5u < h->bn * 4u)
            h->e[0].v = v - (uint32_t)(((uint64_t)v * h->bd) >> 32);
        if (++h->bc >= h->bn) {
            h->bc = 0;
            h->b++;
            h->e[0].v = 1u << 28;
        }
    } else {
        h->e[0].v = v - (uint32_t)(((uint64_t)v * h->btail) >> 32);
        if (h->e[0].v < AN_FLOOR)
            h->e[0].v = 0;
    }
    return v;
}

static inline int32_t an_g(int32_t s, uint32_t e) { return (int32_t)(((int64_t)s * (int32_t)(e >> 13)) >> 15); }   /* s x env */
static inline int32_t an_sq(uint32_t ph) { return ((int32_t)ph >> 31) | 1; }   /* a square, -1 / +1 */

static void analog_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    an_v_t *hp = an_of(t, v), hv, *h = &hv;           /* (the block on a copy: out[] can't alias it, it stays in registers) */
    const int16_t *p = t->p;
    uint32_t i, k, inc[6], q, qi, gr;
    int32_t gain, kk, ninv, velg = v->vel * 258, trim = (VOICE_FS * AN_TRIM[h->mach % AN_N]) >> 12;
    int64_t a, g, dr, pr;
    if (!hp)
        return;
    hv = *hp;
    /* the track chain for this block (applyFx): AGE's GRIT, DRIVE, LEVEL */
    a = an_x(p[P_LN1]);
    g = clamp((int32_t)a * AGE_GRIT * 2 / 127, 0, 65536);
    dr = an_x(p[P_E6]);
    kk = (int32_t)((AQ(1.0) + dr * 3 + g * 4) >> 4);                /* the shaper's drive, Q12 (1 .. 8) */
    ninv = (int32_t)((32767u << 14) / (uint32_t)an_tanh(kk << 3));    /* 1 / tanh(k), Q14 */
    gain = (int32_t)((((int64_t)h->lf * an_x(p[P_LN0])) >> 16) * AQ(1.1) >> 17);   /* lf x LEVEL x 1.1, Q15 */
    gr = g > AQ(0.02);
    if (gr && h->gk != (int32_t)g) {                  /* bits 16 - 9 g: a step of 2^(9 g) in Q15; the low-pass (when AGE moved) */
        int64_t st = an_exp2(g * 9);
        h->gk = (int32_t)g;
        h->gq = (uint32_t)st;
        h->gqi = 0xFFFFFFFFu / (uint32_t)st;
        an_svf_set(&h->gl, an_mul(AQ(20000.0), an_pow(AQ(0.18), g)), AN_QDB05);
    }
    q = h->gq;
    qi = h->gqi;
    pr = an_exp2(((m->pitch16 - h->pitch16) << 16) / 192);    /* the pitch moved since the hit (LFO, bend) */
    for (k = 0; k < 6u; k++)
        inc[k] = (uint32_t)(((uint64_t)h->inc[k] * (uint64_t)pr) >> 16);
    for (i = 0; i < n; i++) {
        int32_t s = 0, x, y;
        if (h->wait) {
            h->wait--;
            s = 0;
        } else {
            switch (h->mach) {
            case AN_KICK: {
                uint32_t f = an_sweep_tick(&h->sw);
                s = an_lp(&h->f[0], an_g(sine_i(h->ph[0]), an_env_tick(&h->e[0])));
                h->ph[0] += (uint32_t)(((uint64_t)inc[0] * f) >> 28);
                if (h->e[1].v)
                    s += an_g(an_bp(&h->f[1], an_noise(&h->seed)), an_env_tick(&h->e[1]));
                break;
            }
            case AN_SNARE: {
                uint32_t f = an_sweep_tick(&h->sw);
                s = an_g(sine_i(h->ph[0]) + ((sine_i(h->ph[1]) * 19661) >> 15), an_env_tick(&h->e[0]));
                h->ph[0] += (uint32_t)(((uint64_t)inc[0] * f) >> 28);
                h->ph[1] += (uint32_t)(((uint64_t)inc[1] * f) >> 28);
                if (h->e[1].v)
                    s += an_g(an_hp(&h->f[0], an_noise(&h->seed)), an_env_tick(&h->e[1]));
                break;
            }
            case AN_TOM: {
                uint32_t f = an_sweep_tick(&h->sw);
                s = an_lp(&h->f[0], an_g(sine_i(h->ph[0]), an_env_tick(&h->e[0])));
                h->ph[0] += (uint32_t)(((uint64_t)inc[0] * f) >> 28);
                if (h->e[1].v | h->e[2].v) {
                    int32_t nz = an_noise(&h->seed);
                    if (h->e[1].v)
                        s += an_g(an_bp(&h->f[1], nz), an_env_tick(&h->e[1]));
                    if (h->e[2].v)
                        s += an_g(an_bp(&h->f[2], nz), an_env_tick(&h->e[2]));
                }
                break;
            }
            case AN_RIM:
                s = an_g(an_bp(&h->f[0], an_sq(h->ph[0]) * h->g0 + an_sq(h->ph[1]) * h->g1), an_env_tick(&h->e[0]));
                h->ph[0] += inc[0];
                h->ph[1] += inc[1];
                if (h->e[1].v)
                    s += an_g(an_hp(&h->f[1], an_noise(&h->seed)), an_env_tick(&h->e[1]));
                break;
            case AN_CLAP: {
                int32_t nz = an_noise(&h->seed);
                s = an_g(an_bp(&h->f[0], nz), an_clap_env(h));
                if (h->e[1].v)
                    s += an_g(an_lp(&h->f[1], nz), an_env_tick(&h->e[1]));
                }
                break;
            case AN_COW:
                s = an_g(an_bp(&h->f[0], (an_sq(h->ph[0]) + an_sq(h->ph[1])) * 19660), an_env_tick(&h->e[0]));
                h->ph[0] += inc[0];
                h->ph[1] += inc[1];
                if (h->e[1].v)
                    s += an_g(an_hp(&h->f[1], an_noise(&h->seed)), an_env_tick(&h->e[1]));
                break;
            default: {                                /* HAT, CYMBAL: the six squares at 0.1 each */
                int32_t six = (an_sq(h->ph[0]) + an_sq(h->ph[1]) + an_sq(h->ph[2]) + an_sq(h->ph[3]) + an_sq(h->ph[4]) +
                               an_sq(h->ph[5])) * 3277;
                for (k = 0; k < 6u; k++)
                    h->ph[k] += inc[k];
                if (h->mach == AN_HAT) {
                    s = an_g(an_hp(&h->f[1], an_bp(&h->f[0], six)), an_env_tick(&h->e[0]));
                    if (h->e[1].v)
                        s += an_g(an_hp(&h->f[2], an_noise(&h->seed)), an_env_tick(&h->e[1]));
                } else {
                    s = an_g(an_lp(&h->f[2], an_hp(&h->f[1], an_bp(&h->f[0], six))), an_env_tick(&h->e[0]));
                    if (h->e[1].v)
                        s += an_g(an_lp(&h->f[3], six), an_env_tick(&h->e[1]));
                }
                break;
            }
            }
            if (h->e[3].v)                            /* HISS */
                s += an_g(an_hp(&h->hf, an_noise(&h->hseed)), an_env_tick(&h->e[3]));
        }
        /* the track chain: the hit's level x LEVEL x 1.1, the shaper's input (-1..1), tanh(k x) / tanh(k), GRIT */
        x = clamp((int32_t)(((int64_t)s * gain) >> 15), -32767, 32767);
        y = (int32_t)(((int64_t)an_tanh((x * kk) >> 12) * ninv) >> 14);
        if (gr) {
            y = (int32_t)(((((int64_t)y * qi + 32768) >> 16) * q) >> 16);
            y = an_lp(&h->gl, y);
        }
        y = mulq15(y, velg);
        h->last = y;
        if (h->dcn) {                                 /* the old hit's last value, fading out */
            y += (h->dc * h->dcn) >> 6;                   /* (/ AN_DECLICK) */
            h->dcn--;
        }
        out[i] += (int32_t)(((int64_t)mulq15(y, amp_at(m, i)) * trim) >> 15);   /* (voice_amp, with AN_TRIM) */
    }
    *hp = hv;
}

static const param_desc_t *analog_desc(const track_t *t, uint32_t k)
{
    static param_desc_t d[7];
    uint32_t mach = (uint32_t)t->p[P_E7] % AN_N;
    if (k >= 7u)
        return 0;
    d[k] = (param_desc_t){AN_LABEL[mach][k], F_INT, 0, 127, AN_DEF[mach][k], 0, 0};
    return &d[k];
}

static const engine_t ENG_ANALOG = {
    .name = "ANALOG",
    .page_title = {"OSC 1", "OSC 2"},
    .edit = {{"TUNE", F_INT, 0, 127, 55, 0, 0}, {"DECAY", F_INT, 0, 127, 70, 0, 0}, {"CLICK", F_INT, 0, 127, 45, 0, 0},
             {"TONE", F_INT, 0, 127, 80, 0, 0}, {"SWEEP", F_INT, 0, 127, 50, 0, 0}, {"S.TIM", F_INT, 0, 127, 40, 0, 0},
             {"DRIVE", F_INT, 0, 127, 15, 0, 0}, {"MACH", F_ENUM, 0, AN_N - 1, 0, N_AMACH, 0}},
    .presets = ANALOG_PRESETS,
    .npresets = AN_N,
    .note_on = analog_note_on,
    .render = analog_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .desc = analog_desc,
    .poly = 2,
    .keep = 0,
    .oneshot = 1,
    .ownenv = 1,
    .done = analog_done,
};
