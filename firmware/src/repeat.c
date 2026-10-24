/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The PERF page's beat repeats (seq.c keyboard_block: its last four white keys): REPEAT 1/8, 1/16, 1/32 and REVERSE
 * (1/8), on the master after its level and before the DJ filter and COMB (fx.c perf_master). A key held records that
 * much of the mix, from the next 1/16 (at once while stopped), and loops it (REVERSE: backwards) while held; a shorter
 * REPEAT of what is recorded switches with a cross-fade, another one fades the old out first. Let go: the loop fades
 * out over PERF's FADE (perf_rel_sl, ui.c), at least 2.9 ms. The last key pressed of those held plays.
 * The loop: one stereo buffer of 8192 frames at 22.05 kHz (371 ms); a REPEAT 1/8 or REVERSE longer than that at the
 * tempo (below 81 BPM) does nothing. Idle: nothing runs, the output is bit-identical.
 * (Felucca's FX layer effects were here: perform.c.) */
enum { PF_R8, PF_R16, PF_R32, PF_REV, PF_N };
#define PF_BIT(e) (1u << (e))
#define PF_Q (PF_BIT(PF_R8) | PF_BIT(PF_R16) | PF_BIT(PF_R32) | PF_BIT(PF_REV))   /* they start on the 1/16 */
#define SL_RAMP_LOG2 7
#define SL_RAMP (1 << SL_RAMP_LOG2)           /* the ramps: 128 samples, 2.9 ms */
#define SL_SLOPE (32768 / SL_RAMP)
#define PB_FRAMES 8192u                       /* stereo frames in pb_buf: 371 ms at 22.05 kHz */
#define PB_MAX (2u * PB_FRAMES)               /* the longest loop, 44.1 kHz samples */
enum { BM_NONE, BM_LOOP };

static volatile uint32_t kb_layer;    /* ISR: the keys held that play no note (a repeat key, a mute key), a bit per key */
static volatile uint32_t perf_held;   /* ISR: the repeats held (PF_*) */
static volatile int32_t perf_rel_sl;  /* main (ui.c, PERF's FADE): the loop's share falls this much a sample when let go (0: SL_SLOPE) */
static volatile uint32_t perf_act;    /* ISR: .. running (from its 1/16 on) */
static uint32_t perf_ord[PF_N], perf_seq;   /* press order: the last pressed plays */

/* the ZDF state-variable filter at cutoff index i (30 Hz * 2^(i / 7), 0..63: up to 15.4 kHz), k = 1:
 * a1 a2 a3 in Q14 (tools: g = tan(pi fc / fs), a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2) */
static const int16_t PF_SVF[64][3] = {
    {16349, 35, 0}, {16345, 39, 0}, {16341, 43, 0}, {16337, 47, 0}, {16332, 52, 0}, {16327, 57, 0}, {16321, 63, 0},
    {16314, 70, 0}, {16307, 77, 0}, {16299, 85, 0}, {16290, 94, 1}, {16280, 103, 1}, {16269, 114, 1},
    {16257, 126, 1}, {16244, 139, 1}, {16229, 153, 1}, {16213, 169, 2}, {16196, 186, 2}, {16176, 205, 3},
    {16154, 227, 3}, {16130, 250, 4}, {16104, 275, 5}, {16075, 303, 6}, {16043, 334, 7}, {16007, 368, 8},
    {15968, 406, 10}, {15925, 447, 13}, {15877, 492, 15}, {15824, 541, 19}, {15766, 596, 22}, {15702, 655, 27},
    {15631, 720, 33}, {15553, 791, 40}, {15467, 869, 49}, {15372, 953, 59}, {15267, 1046, 72}, {15151, 1146, 87},
    {15024, 1255, 105}, {14883, 1374, 127}, {14729, 1502, 153}, {14559, 1640, 185}, {14372, 1790, 223},
    {14166, 1950, 268}, {13940, 2121, 323}, {13692, 2304, 388}, {13420, 2499, 465}, {13122, 2704, 557},
    {12797, 2921, 667}, {12441, 3147, 796}, {12053, 3382, 949}, {11631, 3624, 1129}, {11173, 3870, 1341},
    {10677, 4119, 1589}, {10140, 4365, 1879}, {9562, 4605, 2218}, {8940, 4832, 2612}, {8274, 5040, 3070},
    {7563, 5219, 3602}, {6807, 5359, 4219}, {6006, 5444, 4934}, {5163, 5456, 5766}, {4282, 5369, 6733},
    {3372, 5149, 7863}, {2449, 4745, 9190},
};

static struct {
    uint32_t ph, P, split;             /* samples since play; the 1/16; this block: where a 1/16 starts */
    uint32_t act, act0;                /* running after / before split */
    uint8_t busy, sync;                /* sync: a 1/16 starts with the next block */
    uint8_t mode, src, next, rev;      /* BM_*, the repeat it plays (PF_N none), one waiting for the fade, REVERSE */
    uint8_t state, odd, xrev;          /* 0 recording, 1 playing; the sample of a pair; the old loop's */
    uint32_t len, wr, rp;              /* loop length (samples), frames written, read (samples into the loop) */
    uint32_t xrp, xlen, xn;            /* the loop it switched from, faded over xn samples */
    int32_t w, tw, hl, hr;             /* the loop's share and its target, Q15; the first of a pair */
} pf = {.src = PF_N, .next = PF_N};
static int16_t pb_buf[2u * PB_FRAMES] __attribute__((section(".pool")));   /* the loop (32 KB) */

/* the loop length of a repeat at the tempo, 44.1 kHz samples */
static const uint8_t PF_DEN[PF_N] = {2, 4, 8, 2};
static uint32_t perf_len(uint32_t e) { return e < PF_N ? beat_samples() / PF_DEN[e] : 0u; }
/* the repeats that can run at the tempo: one longer than the loop cannot */
static uint32_t perf_avail(void)
{
    uint32_t e, m = ~0u, b = beat_samples();
    for (e = PF_R8; e < PF_N; e++)
        if (b > PB_MAX * PF_DEN[e])
            m &= ~PF_BIT(e);
    return m;
}

/* seq.c keyboard_block: a repeat key down / up (e: PF_*, PF_N = none) */
static void perf_press(uint32_t e, int down)
{
    if (e >= PF_N)
        return;
    if (down) {
        perf_held |= PF_BIT(e);
        perf_ord[e] = ++perf_seq;
    } else {
        perf_held &= ~PF_BIT(e);
    }
}

static void perf_start(void) { pf.ph = 0; pf.sync = 1; }      /* seq_start: a 1/16 starts with the transport */

/* the repeat to play: the last pressed of those running */
static uint32_t perf_pick(uint32_t a)
{
    uint32_t e, best = PF_N;
    for (e = 0; e < PF_N; e++)
        if (((a >> e) & 1u) && (best == PF_N || perf_ord[e] - perf_ord[best] < 0x80000000u))
            best = e;
    return best;
}

static void perf_buf_start(uint32_t e)
{
    pf.src = (uint8_t)e;
    pf.next = PF_N;
    pf.wr = pf.rp = 0;
    pf.odd = 0;
    pf.xn = 0;
    pf.mode = BM_LOOP;
    pf.state = 0;
    pf.len = perf_len(e);
    pf.rev = e == PF_REV;
}

/* the repeat to play becomes e (at a 1/16): a shorter one of what is recorded switches with a cross-fade, anything
 * else fades the old one out first */
static void perf_buf_select(uint32_t e)
{
    if (e == pf.src && pf.next == PF_N)
        return;
    if (e != PF_N && pf.mode == BM_LOOP && pf.state && e <= PF_REV && pf.src != PF_N &&
        perf_len(e) <= 2u * pf.wr) {
        pf.xrp = pf.rp;
        pf.xlen = pf.len;
        pf.xrev = pf.rev;
        pf.xn = SL_RAMP;
        pf.src = (uint8_t)e;
        pf.next = PF_N;
        pf.len = perf_len(e);
        pf.rev = e == PF_REV;
        pf.rp = 0;
        return;
    }
    if (pf.mode == BM_NONE || !pf.w) {
        if (e == PF_N) {
            pf.src = PF_N;
            pf.next = PF_N;
        } else {
            perf_buf_start(e);
        }
        return;
    }
    pf.src = PF_N;                                  /* fade out, then the next one (perf_seg) */
    pf.next = (uint8_t)e;
}

/* each block, after the keys (seq.c) and before the parts: what runs, the 1/16. 1: perf_block has work */
static __attribute__((noinline)) int perf_begin(uint32_t n)
{
    uint32_t held = perf_held & perf_avail();
    uint32_t q, ph0, bnd;
    if (!held && !pf.busy) {                        /* idle: the clock only */
        pf.ph += n;
        pf.sync = 0;
        perf_act = 0;
        return 0;
    }
    pf.P = beat_samples() >> 2;
    if (pf.sync) {
        pf.ph = 0;
        pf.sync = 0;
    }
    ph0 = pf.ph % pf.P;                             /* the 1/16 in this block (n: none) */
    bnd = ph0 ? (pf.P - ph0 < n ? pf.P - ph0 : n) : 0u;
    pf.ph += n;
    pf.act &= held;                                 /* let go: at once */
    pf.act0 = pf.act;
    q = held & PF_Q & ~pf.act;
    pf.split = n;
    if (q && (!song.playing || bnd < n)) {
        pf.split = song.playing ? bnd : 0u;
        pf.act |= q;
    }
    perf_act = pf.act;
    pf.busy = 1;
    return 1;
}

/* x towards y by w (Q15, 32768 = y) */
static inline int32_t pf_mix(int32_t x, int32_t y, int32_t w)
{
    return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);
}

/* the loop: frame f of it, both channels (Q15) */
static inline void pb_put(uint32_t f, int32_t l, int32_t r)
{
    int16_t *p = pb_buf + 2u * (f & (PB_FRAMES - 1u));
    p[0] = (int16_t)clamp(l, -32768, 32767);
    p[1] = (int16_t)clamp(r, -32768, 32767);
}
/* sample s of the frames recorded from 0 (n of them): between two frames on an odd one */
static inline void pb_at(uint32_t s, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 1 < n ? s >> 1 : n - 1u;    /* (an odd loop's last sample: the frame before) */
    const int16_t *p = pb_buf + 2u * (f & (PB_FRAMES - 1u));
    const int16_t *q;                               /* a frame is the mean of samples 2f, 2f + 1: sample s */
    if (s & 1u)                                     /* lies a quarter frame from it, towards f + 1 or f - 1 */
        q = f + 1u < n ? p + 2 : p;
    else
        q = f ? p - 2 : p;
    *l = (3 * p[0] + q[0]) >> 1;
    *r = (3 * p[1] + q[1]) >> 1;
}
/* REPEAT / REVERSE at sample rp of a loop of len, windowed at both ends */
static inline void pb_loop(uint32_t rp, uint32_t len, int rev, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t e = len - 1u - rp;
    pb_at(rev ? e : rp, n, l, r);
    if (rp < e)
        e = rp;
    if (e < (uint32_t)SL_RAMP) {
        *l = (*l * (int32_t)e) >> SL_RAMP_LOG2;
        *r = (*r * (int32_t)e) >> SL_RAMP_LOG2;
    }
}

/* record the pair into frame f (the mean, >> 1: headroom) */
static inline uint32_t pb_rec(uint32_t f, int32_t l, int32_t r)
{
    if (!pf.odd) {
        pf.hl = l;
        pf.hr = r;
        pf.odd = 1;
        return f;
    }
    pf.odd = 0;
    pb_put(f, (pf.hl + l) >> 2, (pf.hr + r) >> 2);
    return f + 1u;
}

/* the loop on one sample: the share pf.w of what it plays, the rest live */
static inline void perf_buf(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, yl = 0, yr = 0, on = 0;
    if (!pf.state) {
        pf.wr = pb_rec(pf.wr, l, r);
        if (++pf.rp >= pf.len) {                    /* (rp: samples recorded) */
            pf.state = 1;
            pf.rp = 0;
        }
    } else {
        pb_loop(pf.rp, pf.len, pf.rev, pf.wr, &yl, &yr);
        if (pf.xn) {                                /* the loop it switched from, fading out */
            int32_t ol, orr, g = (int32_t)pf.xn;
            pb_loop(pf.xrp, pf.xlen, pf.xrev, pf.wr, &ol, &orr);
            yl += (ol * g) >> SL_RAMP_LOG2;
            yr += (orr * g) >> SL_RAMP_LOG2;
            pf.xn--;
            if (++pf.xrp >= pf.xlen)
                pf.xrp = 0;
        }
        if (++pf.rp >= pf.len)
            pf.rp = 0;
        on = 1;
    }
    pf.tw = on && pf.src != PF_N ? 32768 : 0;
    {
        int32_t dn = pf.src == PF_N && pf.next == PF_N && perf_rel_sl > 0 && perf_rel_sl < SL_SLOPE ? perf_rel_sl : SL_SLOPE;
        pf.w += clamp(pf.tw - pf.w, -dn, SL_SLOPE);
    }
    if (pf.w) {
        *pl = pf_mix(l, yl, pf.w);
        *pr = pf_mix(r, yr, pf.w);
    } else if (pf.src == PF_N) {                    /* faded out: the next one, or none */
        if (pf.next != PF_N)
            perf_buf_start(pf.next);
        else
            pf.mode = BM_NONE;
    }
}

static __attribute__((noinline)) void perf_switch(uint32_t a) { perf_buf_select(perf_pick(a)); }

/* after the master level: the repeat on l / r (stereo, Q15) */
static __attribute__((noinline)) void perf_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, a = pf.act0;
    if (pf.split)
        perf_switch(a);
    for (i = 0; i < n; i++) {
        int32_t l = bl[i], r = br[i];
        if (i == pf.split) {
            a = pf.act;
            perf_switch(a);
        }
        if (pf.mode != BM_NONE)
            perf_buf(&l, &r);
        bl[i] = l;
        br[i] = r;
    }
    pf.busy = pf.act || pf.mode != BM_NONE || pf.w;
}
