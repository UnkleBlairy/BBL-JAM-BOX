/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), BBL JAM BOX */
/* SAMPLER: plays the chunks the sample uploader wrote to the flash (docs/samplelab.html is the spec: its render() and
 * stretchBuf() are the reference). One machine, one voice a track (a new hit takes over, its old sound fades in 4 ms).
 *
 * The sample memory: flash 0xA0000..0xD9FFF (Felucca's old user sample slots), read in place through the XIP window.
 *   0xA0000  the table (4 KiB): u32 magic "FSM2", u16 version 1, u16 count (<= SMP_MAX), u32 data length, u32 data
 *            CRC32, 12 bytes 0, u32 table CRC32 (bytes 0..27, then the entries), then count entries of 24 bytes:
 *            char name[12] (NUL padded), u32 offset (in the data), u32 length (samples), u16 rate (Hz), u16 coding (0)
 *   0xA1000  the data: 8-bit mu-law (G.711), one byte a sample (the user chose twice the time over 16 bits)
 * All little endian. The web uploader builds the image (web/emu/index.html smpImage); the device checks it whole.
 *
 * Knobs: OSC 1 PITCH STRETCH START LEN, OSC 2 MODE SLOT LOFI LEVEL (P_LN0), the GATE page GATE (P_LN2) SCRUB (P_DIST:
 * the old distortion's id, 0 in every sampler sound saved before). E7 = the machine (one). The ENV and FILTER pages work
 * as on the FM machines (the filter is DIGITAL's).
 *
 * SCRUB, a tape head (0 off, 1..127 = where it sits in the whole chunk; START LEN MODE STRETCH GATE don't apply). The
 * head chases the knob with some weight (smp_scrub_block), its speed is the speed the tape is read at: fast = high,
 * backwards = reversed, still = silent. The note scales that (a tape of the same length read at another pitch): two Hann
 * grains read at speed x note, each restarted at the head, so at middle C they ARE the tape. Moving the knob starts SCRUB's
 * voice by itself (sampler_block), at the last note played; it ends when the head has been still a moment. A key or trig
 * always plays the sample as normal, from its start, at once. The knob moving then takes over with no jump: the head
 * starts where the sound has got to, at its speed, and travels to the knob (smp_scrub_catch); where they meet the knob
 * has it. So a step's SCRUB lock only parks the head; a trigless lock moves the tape. */
#ifndef SMP_XIP                                     /* the emulator and the host tests: a RAM image of the flash */
#include "../hal/fm1_xip.h"
#define SMP_XIP(off) fm1_xip_ptr(off)
#endif
#define SMP_BASE 0xA0000u
#define SMP_DATA (SMP_BASE + 0x1000u)
#define SMP_TOP 0xDA000u                            /* (0xDA000..0xDBFFF: the last jam's second part, storage.c OBJ_JAM2) */
#define SMP_CAP (SMP_TOP - SMP_DATA)                /* 233472 bytes = samples: ~10.6 s at 22 kHz */
#define SMP_MAX 32u                                 /* chunks S01..S32 */
#define SMP_MAGIC 0x324D5346u                       /* "FSM2" */
#define SMP_NOTE 60                                 /* the note that plays a chunk at its own pitch */
#define SMP_DECLICK 176                             /* a hit taking over: the old sound fades over 4 ms */
#define SMP_GFADE 529                               /* GATE's end: 12 ms */
#define SMP_EFADE 48                                /* ONCE / REV: in and out over 48 samples (the prototype's) */
#define SMP_ST_MIN 882                              /* SCRUB's weight, samples: 20 ms at the least (a fast hand) .. */
#define SMP_ST_MAX 11025                            /* .. 250 ms (a slow sweep: smooths the 127 knob steps into a glide) */
#define SMP_SR_MAX (8 << 12)                        /* the tape's fastest: 8 samples of the chunk a sample out */
#define SMP_SC_TAU 1764                              /* the catch-up's weight: 40 ms; at most ~1/2 s (at twice the chunk's own speed or more) */
#define SMP_SG_SLEW 186                             /* its gain's fastest move, a sample (~4 ms end to end) */
#define GB_WAVE 224                                 /* the screen's waveform: columns (gb_store.c smp_peaks) */

static const char *const N_SMODE[4] = {"ONCE", "LOOP", "PING", "REV"};
enum { SM_ONCE, SM_LOOP, SM_PING, SM_REV };

/* tables (python, from the prototype's formulas): mu-law to 16 bits; STRETCH 4^((v-64)/64) in Q12; LOFI's
 * sample-hold round(10^(v/127)); a 256-point Hann window (i + .5), Q15: w[i] + w[i + 128] = 1 */
static const int16_t SMP_ULAW[256] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956, -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412, -11900, -11388, -10876, -10364, -9852, -9340, -8828, -8316,
    -7932, -7676, -7420, -7164, -6908, -6652, -6396, -6140, -5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092,
    -3900, -3772, -3644, -3516, -3388, -3260, -3132, -3004, -2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980,
    -1884, -1820, -1756, -1692, -1628, -1564, -1500, -1436, -1372, -1308, -1244, -1180, -1116, -1052, -988, -924,
    -876, -844, -812, -780, -748, -716, -684, -652, -620, -588, -556, -524, -492, -460, -428, -396,
    -372, -356, -340, -324, -308, -292, -276, -260, -244, -228, -212, -196, -180, -164, -148, -132,
    -120, -112, -104, -96, -88, -80, -72, -64, -56, -48, -40, -32, -24, -16, -8, 0,
    32124, 31100, 30076, 29052, 28028, 27004, 25980, 24956, 23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764,
    15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412, 11900, 11388, 10876, 10364, 9852, 9340, 8828, 8316,
    7932, 7676, 7420, 7164, 6908, 6652, 6396, 6140, 5884, 5628, 5372, 5116, 4860, 4604, 4348, 4092,
    3900, 3772, 3644, 3516, 3388, 3260, 3132, 3004, 2876, 2748, 2620, 2492, 2364, 2236, 2108, 1980,
    1884, 1820, 1756, 1692, 1628, 1564, 1500, 1436, 1372, 1308, 1244, 1180, 1116, 1052, 988, 924,
    876, 844, 812, 780, 748, 716, 684, 652, 620, 588, 556, 524, 492, 460, 428, 396,
    372, 356, 340, 324, 308, 292, 276, 260, 244, 228, 212, 196, 180, 164, 148, 132,
    120, 112, 104, 96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0};
static const uint16_t SMP_STRETCH[128] = {
    1024, 1046, 1069, 1093, 1117, 1141, 1166, 1192, 1218, 1244, 1272, 1300, 1328, 1357, 1387, 1417,
    1448, 1480, 1512, 1545, 1579, 1614, 1649, 1685, 1722, 1760, 1798, 1838, 1878, 1919, 1961, 2004,
    2048, 2093, 2139, 2186, 2233, 2282, 2332, 2383, 2435, 2489, 2543, 2599, 2656, 2714, 2774, 2834,
    2896, 2960, 3025, 3091, 3158, 3228, 3298, 3371, 3444, 3520, 3597, 3676, 3756, 3838, 3922, 4008,
    4096, 4186, 4277, 4371, 4467, 4565, 4664, 4767, 4871, 4978, 5087, 5198, 5312, 5428, 5547, 5668,
    5793, 5919, 6049, 6182, 6317, 6455, 6597, 6741, 6889, 7039, 7194, 7351, 7512, 7677, 7845, 8016,
    8192, 8371, 8555, 8742, 8933, 9129, 9329, 9533, 9742, 9955, 10173, 10396, 10624, 10856, 11094, 11337,
    11585, 11839, 12098, 12363, 12634, 12910, 13193, 13482, 13777, 14079, 14387, 14702, 15024, 15353, 15689, 16033};
static const uint8_t SMP_HOLD[128] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10};
static const uint16_t SMP_HANN[256] = {
    1, 11, 31, 60, 100, 149, 208, 277, 355, 443, 541, 648, 765, 891, 1027, 1171,
    1325, 1488, 1660, 1841, 2030, 2229, 2435, 2650, 2874, 3105, 3345, 3592, 3847, 4110, 4380, 4657,
    4942, 5233, 5531, 5835, 6146, 6463, 6786, 7115, 7449, 7789, 8134, 8484, 8838, 9197, 9561, 9929,
    10300, 10675, 11054, 11436, 11820, 12208, 12598, 12990, 13385, 13781, 14179, 14578, 14978, 15379, 15780, 16182,
    16585, 16987, 17388, 17789, 18189, 18588, 18986, 19382, 19777, 20169, 20559, 20947, 21331, 21713, 22092, 22467,
    22838, 23206, 23570, 23929, 24283, 24633, 24978, 25318, 25652, 25981, 26304, 26621, 26932, 27236, 27534, 27825,
    28110, 28387, 28657, 28920, 29175, 29422, 29662, 29893, 30117, 30332, 30538, 30737, 30926, 31107, 31279, 31442,
    31596, 31740, 31876, 32002, 32119, 32226, 32324, 32412, 32490, 32559, 32618, 32667, 32707, 32736, 32756, 32766,
    32766, 32756, 32736, 32707, 32667, 32618, 32559, 32490, 32412, 32324, 32226, 32119, 32002, 31876, 31740, 31596,
    31442, 31279, 31107, 30926, 30737, 30538, 30332, 30117, 29893, 29662, 29422, 29175, 28920, 28657, 28387, 28110,
    27825, 27534, 27236, 26932, 26621, 26304, 25981, 25652, 25318, 24978, 24633, 24283, 23929, 23570, 23206, 22838,
    22467, 22092, 21713, 21331, 20947, 20559, 20169, 19777, 19382, 18986, 18588, 18189, 17789, 17388, 16987, 16585,
    16182, 15780, 15379, 14978, 14578, 14179, 13781, 13385, 12990, 12598, 12208, 11820, 11436, 11054, 10675, 10300,
    9929, 9561, 9197, 8838, 8484, 8134, 7789, 7449, 7115, 6786, 6463, 6146, 5835, 5531, 5233, 4942,
    4657, 4380, 4110, 3847, 3592, 3345, 3105, 2874, 2650, 2435, 2229, 2030, 1841, 1660, 1488, 1325,
    1171, 1027, 891, 765, 648, 541, 443, 355, 277, 208, 149, 100, 60, 31, 11, 1};

/* the table (the main loop's smp_scan, gb_store.c, fills it at boot and after an upload) */
typedef struct {
    uint32_t off, len;           /* flash offset of its first sample, samples */
    uint16_t rate;               /* Hz */
    char name[13];
} smp_chunk_t;
static smp_chunk_t smp_tab[SMP_MAX];
static uint8_t smp_n;                               /* the chunks there are */
static volatile uint32_t smp_gen;                   /* + 1 a scan: a voice playing the old table ends */

typedef struct { uint32_t L, Lp, cf, cfw, per; } smp_g_t;   /* a window's loop: its length, the loop (L less the
                                                             * crossfade cf, its gain step cfw), the period */
typedef struct {                 /* a track's voice (one: a hit takes over) */
    uint8_t owner;               /* the voice's index + 1 (0: none) */
    uint8_t scr;                 /* SCRUB: the tape head plays this voice (smp_s) */
    uint8_t mode, done, keyg, hc, gb, tail;   /* gb: log2 of the grain; tail: ONCE / REV past the end, the last grain fades */
    int32_t dir;                 /* 1, REV -1 */
    const uint8_t *d, *dp, *dq;  /* the window's first sample (REV: its last) for the grain playing, the one before, and
                                  * the next (START / LEN moved: grains take it as they start, so their overlap fades it) */
    const uint8_t *base;         /* the chunk's first sample */
    smp_g_t g[3];                /* the loops of d, dp, dq: a grain keeps its window's, START / LEN moving it or not */
    uint32_t gen, srq;
    uint32_t off, n;             /* the chunk: its first sample (flash), its length */
    int16_t sk, lk;              /* the START and LEN the window was made from (smp_win: they move it live) */
    int32_t cin, pin;            /* the source position the grain playing started at, and the one before it, Q8 */
    uint32_t gpos;               /* where in the grain playing, Q16 (a grain is read for half its length, then the next) */
    uint32_t cnt, gate;          /* samples since the hit; GATE's length (0: the key's) */
    int32_t y, last, dc, dcn, gf;
} smp_v_t;
static smp_v_t smp_v[NPART];
typedef struct {                 /* a track's tape head: outlives the voices, so a new note doesn't move it */
    uint8_t on, want, note;      /* on: the head is valid; want: this note is SCRUB's own; note: the last played (0: none) */
    uint8_t pv, sv;              /* the value seen last (sampler_block, smp_scrub_block) */
    uint8_t catch, fresh;        /* catch: the head is on its way to the knob; fresh: the note taking over is yet to start */
    int32_t cv;                  /* the catch-up's speed, Q12 samples a sample */
    uint32_t off;                /* the chunk the head is on */
    int32_t s1, s2, head;        /* the chase's two stages and the head, Q12 samples */
    int32_t vel, rate;           /* the head's speed; the reading speed (with the note), Q12 samples a sample */
    int32_t ra, rb;              /* the grains' read positions, Q12 */
    int32_t gain, gt;            /* Q15: the sound's, what the head's speed asks */
    uint32_t ph;                 /* the grains' phase, 0..1023 */
    uint32_t since, tau, idle;   /* samples: since the knob's last step, the chase's time, the head still */
} smp_s_t;
static smp_s_t smp_s[NPART];
static uint32_t smp_rcp;                            /* 2^32 / (pitch_inc(SMP_NOTE) >> 8): pitch ratios without a divide */

static inline smp_v_t *smp_of(track_t *t, voice_t *v)
{
    smp_v_t *h = &smp_v[(uint32_t)(t - trk) % NPART];
    return h->owner == (uint32_t)(v - t->v) + 1u ? h : 0;
}

/* sample i of the window (mu-law) */
static inline int32_t smp_raw(const smp_v_t *h, const uint8_t *d, uint32_t i)
{
    int32_t k = (int32_t)(d - h->base) + (int32_t)i * h->dir;   /* (a grain of a window since moved: inside the chunk) */
    return k >= 0 && k < (int32_t)h->n ? SMP_ULAW[h->base[k]] : 0;
}
/* sample i of the loop (i < Lp): its start crossfaded with the end past it */
static inline int32_t smp_lv(const smp_v_t *h, const smp_g_t *g, const uint8_t *d, uint32_t i)
{
    int32_t w;
    if (i >= g->cf)
        return smp_raw(h, d, i);
    w = (int32_t)(i * g->cfw);
    return (smp_raw(h, d, i) * w + smp_raw(h, d, g->Lp + i) * (32768 - w)) >> 15;
}
/* sample i (>= 0) of what the mode plays: ONCE / REV the window then silence, LOOP over and over, PING there and back */
static inline int32_t smp_src(const smp_v_t *h, const smp_g_t *g, const uint8_t *d, uint32_t i)
{
    switch (h->mode) {
    case SM_LOOP:
        while (i >= g->Lp)
            i -= g->Lp;
        return smp_lv(h, g, d, i);
    case SM_PING:
        while (i >= g->per)
            i -= g->per;
        return smp_lv(h, g, d, i < g->Lp ? i : g->per - 1u - i);
    default:
        return i < g->L ? smp_raw(h, d, i) : 0;
    }
}
/* the stretch (the prototype's stretchBuf, streaming): two Hann grains half a grain apart, added; k into the grain
 * started at c, the one before (pv) at its second half. STRETCH 1.00x: they line up, the sound as it is. The next grain
 * starts half a grain / STRETCH further on (sampler_render): STRETCH moves while it plays */
static inline int32_t smp_gr(const smp_v_t *h, int32_t c, const uint8_t *dc, const smp_g_t *gc, int32_t pv,
                             const uint8_t *dv, const smp_g_t *gv, uint32_t k, int on)   /* (dc gc, dv gv: the windows
                                                                                          * the two grains read) */
{
    uint32_t hb = h->gb - 1u, wi = h->gb >= 8u ? k >> (h->gb - 8u) : k << (8u - h->gb);
    int32_t v = smp_src(h, gv, dv, (uint32_t)((pv >> 8) + (int32_t)k + (1 << hb))) * SMP_HANN[wi + 128u];
    if (on)
        v += smp_src(h, gc, dc, (uint32_t)((c >> 8) + (int32_t)k)) * SMP_HANN[wi];
    return v >> 15;
}

/* the window of a chunk of n samples: LEN its size (START doesn't change it), START where it starts; pushed into the
 * chunk's end, the window's end stays there and START goes on, the window shrinking (to `least` samples, at the very
 * end). -> its first sample (a), its length (L). The screen's waveform too (ui_gb.c) */
static void smp_window(uint32_t n, int32_t st, int32_t ln, uint32_t *a, uint32_t *L, uint32_t least)
{
    uint32_t l = 1u + (uint32_t)(ln & 127) * (n - 1u) / 127u, s = (uint32_t)(st & 127) * (n - 1u) / 127u;
    least = least > n ? n : least;
    s = s > n - least ? n - least : s;
    l = l > n - s ? n - s : l;                          /* (against the end: shorter) */
    *L = l < least ? least : l;
    *a = s;
}
/* LOOP / PING: the end's crossfade into the start (~256 samples) and the period, for the window's length */
static void smp_len_set(smp_v_t *h, smp_g_t *g)
{
    if (h->mode == SM_LOOP || h->mode == SM_PING) {
        g->cf = g->L / 4u < 256u ? g->L / 4u : 256u;
        g->Lp = g->L - g->cf;
        g->cfw = 32768u / g->cf;
        g->per = h->mode == SM_LOOP ? g->Lp : 2u * g->Lp;
    }
}
/* START and LEN (live, sampler_render each block: knob, lock, LFO, macro): the window moves / resizes under the
 * playhead, which keeps its place in it (a scrub); the next grain takes it (dq, g[2]) */
static void smp_win(smp_v_t *h, int32_t st, int32_t ln)
{
    uint32_t a, L;
    smp_window(h->n, st, ln, &a, &L, 65u);
    h->sk = (int16_t)st;
    h->lk = (int16_t)ln;
    h->g[2].L = L;
    h->dq = SMP_XIP(h->off + (h->mode == SM_REV ? a + L - 1u : a));
    smp_len_set(h, &h->g[2]);
}

/* one sample of the chunk at a Q12 position (held at its ends: no click) */
static inline int32_t smp_tap(const smp_v_t *h, int32_t pos, uint32_t near)
{
    uint32_t i = (uint32_t)pos >> 12;
    int32_t a = SMP_ULAW[h->base[i]];
    if (near || i + 1u >= h->n)
        return a;
    return a + (((SMP_ULAW[h->base[i + 1u]] - a) * (pos & 4095)) >> 12);
}
/* SCRUB's block: the chase to the knob, the speeds it gives. r: the note's ratio, Q16 */
static void smp_scrub_block(smp_v_t *h, smp_s_t *s, int32_t val, uint32_t n, uint32_t r)
{
    int32_t tg = (int32_t)((uint32_t)(val > 0 ? val - 1 : 0) * (h->n - 1u) / 126u) << 12, a, av, thr;
    if (val && val != s->sv) {                        /* the knob moved: its pace sets the weight */
        uint32_t t0 = s->since * 3u / 2u;
        t0 = t0 < SMP_ST_MIN ? SMP_ST_MIN : t0 > SMP_ST_MAX ? SMP_ST_MAX : t0;
        s->tau = s->since > SMP_ST_MAX ? SMP_ST_MIN : (s->tau + t0) / 2u;   /* (from rest: at once) */
        s->since = 0;
        s->sv = (uint8_t)val;
    }
    if (s->since <= SMP_ST_MAX)
        s->since += n;
    if (val && s->catch) {                            /* on its way to the knob: at a steady speed, with weight */
        int32_t d = tg - s->s1, st = s->cv * (int32_t)n;
        a = (int32_t)((n << 15) / SMP_SC_TAU);
        if (d <= st && d >= -st) {
            s->s1 = tg;
            s->catch = 0;                             /* they've met: the knob has it */
            s->tau = SMP_SC_TAU;
        } else
            s->s1 += d > 0 ? st : -st;
        s->s2 += (int32_t)(((int64_t)(s->s1 - s->s2) * a) >> 15);
    } else if (val) {
        a = (int32_t)((n << 15) / s->tau);
        s->s1 += (int32_t)(((int64_t)(tg - s->s1) * a) >> 15);
        s->s2 += (int32_t)(((int64_t)(s->s1 - s->s2) * a) >> 15);
    }                                                 /* (off: it stays, the sound fades) */
    s->vel = (s->s2 - s->head) / (int32_t)n;
    {
        int64_t rt = ((int64_t)s->vel * r) >> 16;
        s->rate = rt > SMP_SR_MAX ? SMP_SR_MAX : rt < -SMP_SR_MAX ? -SMP_SR_MAX : (int32_t)rt;
    }
    av = s->vel < 0 ? -s->vel : s->vel;               /* the sound: full from 1/8 of the chunk's own speed, down to none */
    thr = (int32_t)(h->srq >> 7);
    thr = thr < 1 ? 1 : thr;
    s->gt = !val ? 0 : av >= thr ? 32767 : av * 32767 / thr;
    if (s->gain || s->gt || s->vel)
        s->idle = 0;
    else if ((s->idle += n) > FS / 3u)
        h->done = 1;                                  /* still for a moment: the voice ends (a knob move starts another) */
    if (!val && !s->gain)
        h->done = 1;
}
/* a sample playing normally, the knob moved: the head takes over where the sound has got to, at its speed, bound for
 * the knob. Called before the takeover's note */
static void smp_scrub_catch(const smp_v_t *h, smp_s_t *s, int32_t val)
{
    const smp_g_t *g = &h->g[0];
    int32_t i = (h->cin >> 8) + (int32_t)(h->gpos >> 16), pos, v0, dist;
    int32_t tg = (int32_t)((uint32_t)(val - 1) * (h->n - 1u) / 126u) << 12, least = (int32_t)(h->srq >> 3);
    i = i < 0 ? 0 : i;
    if (h->mode == SM_LOOP) {
        while (i >= (int32_t)g->Lp)
            i -= (int32_t)g->Lp;
    } else if (h->mode == SM_PING) {
        while (i >= (int32_t)g->per)
            i -= (int32_t)g->per;
        i = i < (int32_t)g->Lp ? i : (int32_t)g->per - 1 - i;
    }
    pos = clamp((int32_t)(h->d - h->base) + i * h->dir, 0, (int32_t)h->n - 1);
    v0 = h->dir * (int32_t)(h->srq >> 4);             /* Q16 to Q12: the chunk's own speed */
    s->on = 1;
    s->off = h->off;
    s->head = s->s2 = s->ra = s->rb = pos << 12;
    s->s1 = s->head + v0 * SMP_SC_TAU;                /* (the second stage then runs at that speed) */
    s->vel = v0;
    s->gain = s->gt = 32767;
    s->ph = 0;
    s->sv = (uint8_t)val;
    s->since = 0;
    s->tau = SMP_SC_TAU;
    dist = tg - s->head;
    dist = (dist < 0 ? -dist : dist) / (int32_t)(FS / 2u);
    s->cv = clamp(dist, least, SMP_SR_MAX);
    s->catch = s->fresh = 1;
}
/* SCRUB's sample */
static inline int32_t smp_scrub(smp_v_t *h, smp_s_t *s, uint32_t near, uint32_t hold, uint32_t sh)
{
    int32_t top = (int32_t)((h->n - 1u) << 12), d, y;
    s->head += s->vel;
    s->ra += s->rate;
    s->rb += s->rate;
    s->ph = (s->ph + 1u) & 1023u;
    if (s->ph == 0u)                                  /* a grain's window is at 0 here: it starts again at the head */
        s->ra = s->head;
    else if (s->ph == 512u)
        s->rb = s->head;
    s->ra = clamp(s->ra, 0, top);
    s->rb = clamp(s->rb, 0, top);
    if (!h->hc) {                                     /* LOFI's sample-hold, its bits */
        uint32_t wi = s->ph >> 2;
        y = (smp_tap(h, s->ra, near) * (int32_t)SMP_HANN[wi] + smp_tap(h, s->rb, near) * (int32_t)SMP_HANN[(wi + 128u) & 255u]) >> 15;
        if (sh)
            y = ((y + (1 << (sh - 1u))) >> sh) << sh;
        h->y = y;
    }
    if (++h->hc >= hold)
        h->hc = 0;
    d = s->gt - s->gain;
    s->gain += d > SMP_SG_SLEW ? SMP_SG_SLEW : d < -SMP_SG_SLEW ? -SMP_SG_SLEW : d;
    return (h->y * s->gain) >> 15;
}

static void sampler_note_on(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    smp_v_t *h = &smp_v[(uint32_t)(t - trk) % NPART];
    uint32_t vi = (uint32_t)(v - t->v) + 1u, sl = (uint32_t)p[P_E5], n;
    uint32_t g = (uint32_t)p[P_LN2], bpm = (uint32_t)song.g[G_BPM];
    int32_t dc = 0;
    const smp_chunk_t *c;
    smp_s_t *s = &smp_s[(uint32_t)(t - trk) % NPART];
    uint32_t scr = s->want;
    int32_t val = clamp(p[P_DIST], 0, 127);
    if (!scr) {
        s->note = v->note;                            /* (the last played: SCRUB's pitch when it starts by itself) */
        s->pv = (uint8_t)val;                         /* (a value this note brought, its lock: no tape move) */
    }
    if (h->owner && !h->done && h->owner <= NVOICE && t->v[h->owner - 1u].active) {
        dc = h->last + ((h->dc * h->dcn) / SMP_DECLICK);   /* a takeover: the old sound fades out */
        if (h->owner != vi)
            v->s[0] = v->s[1] = 0;
    } else
        v->s[0] = v->s[1] = 0;                        /* the filter (a takeover keeps it: no click) */
    v->s[2] = p[P_FE_ATK] ? 0 : 1 << 24;              /* the filter envelope (as DIGITAL's) */
    v->s[3] = p[P_FE_ATK] ? 1 : 2;
    if (!smp_rcp)
        smp_rcp = 0xFFFFFFFFu / (pitch_inc(SMP_NOTE * 16) >> 8);
    memset(h, 0, sizeof *h);
    h->owner = (uint8_t)vi;
    h->dc = dc;
    h->dcn = dc ? SMP_DECLICK : 0;
    h->gen = smp_gen;
    if (sl >= smp_n) {                                /* an empty slot: silence */
        h->done = 1;
        return;
    }
    c = &smp_tab[sl];
    n = c->len;
    h->mode = (uint8_t)((uint32_t)p[P_E4] & 3u);
    h->dir = h->mode == SM_REV ? -1 : 1;
    h->off = c->off;
    h->n = n;
    h->base = SMP_XIP(c->off);
    smp_win(h, p[P_E2], p[P_E3]);                     /* START, LEN */
    h->d = h->dp = h->dq;
    h->g[0] = h->g[1] = h->g[2];
    h->srq = ((uint32_t)c->rate << 16) / (uint32_t)FS;
    {                                                 /* grains of ~40 ms (a power of two), at most half the window */
        uint32_t gb = c->rate > 16000u ? 10u : 9u;
        while ((1u << gb) > h->g[0].L / 2u)
            gb--;                                     /* (L >= 65: 32 and up) */
        h->gb = (uint8_t)gb;
        h->pin = -(1 << (gb - 1u + 8u));              /* the grain "before" lines up: no fade in */
    }
    if ((h->mode == SM_LOOP || h->mode == SM_PING) && g)   /* GATE in steps (1/16s at the tempo) */
        h->gate = g * (FS * 60u / 4u) / (bpm ? bpm : 120u);
    h->keyg = (uint8_t)!g;
    if (scr) {
        h->scr = 1;
        h->keyg = 0;                                  /* (the voice ends when the tape stops, not with the key) */
    }
    if (val || scr) {                                 /* a normal note parks the head at the knob; SCRUB's own keeps it */
        if (!scr || !s->on || s->off != c->off) {     /* (.. unless it's not on this chunk) */
            val = val ? val : 1;
            s->catch = 0;
            s->vel = s->rate = s->gain = s->gt = 0;
            s->ph = 0;
            s->s1 = s->s2 = s->head = s->ra = s->rb = (int32_t)((uint32_t)(val - 1) * (n - 1u) / 126u) << 12;
            s->on = 1;
            s->off = c->off;
            s->sv = (uint8_t)val;
            s->tau = SMP_ST_MIN;
            s->since = SMP_ST_MAX + 1u;
        }
        s->idle = 0;
        if (s->fresh) {                               /* taking over a sample: only the difference fades */
            h->dc = dc - smp_tap(h, s->head, 0);
            h->dcn = SMP_DECLICK;
            s->fresh = 0;
        }
    }
}

/* the amplitude hook: the voice ends when the sound does; GATE in steps: the key's release doesn't end it */
static int32_t sampler_amp(track_t *t, voice_t *v, int32_t adsr)
{
    smp_v_t *h = smp_of(t, v);
    if (!h || h->done || h->gen != smp_gen) {
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    if (!h->keyg && v->stage == 3)
        v->stage = 2;
    return adsr;
}

static void sampler_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    smp_v_t *hp = smp_of(t, v), hv, *h = &hv;
    const int16_t *p = t->p;
    uint32_t i, step, lf = (uint32_t)p[P_E6] & 127u, hq, adv, r;
    smp_s_t *s = &smp_s[(uint32_t)(t - trk) % NPART];
    uint32_t near = lf >= 16u, hold = SMP_HOLD[lf];  /* LOFI (live): nearest sample above ~12 %, the sample-hold, */
    uint32_t sh = lf >= 2u ? 16u - (16u * 127u - 12u * lf + 63u) / 127u : 0u;   /* 16 .. 4 bits */
    int32_t lg = p[P_LN0] * 284;                      /* LEVEL: 0 .. 1.1 (the prototype's), Q15 */
    if (!hp)
        return;
    hv = *hp;
    if (h->gen != smp_gen)
        h->done = 1;
    if ((p[P_E2] != h->sk || p[P_E3] != h->lk) && !h->done)   /* START, LEN (live: knob, lock, LFO, macro) */
        smp_win(h, p[P_E2], p[P_E3]);
    {                                                 /* the note (with PITCH, voice.c): a resampling rate */
        int32_t p16 = m->pitch16;
        r = (uint32_t)(((uint64_t)pitch_inc((uint32_t)p16) * smp_rcp) >> 24);   /* Q16 */
        step = (uint32_t)(((uint64_t)r * h->srq) >> 16);
    }
    hq = 1u << (h->gb - 1u + 16u);                    /* half a grain, Q16 */
    adv = (1u << (h->gb - 1u + 20u)) / SMP_STRETCH[(uint32_t)p[P_E1] & 127u];   /* STRETCH (live): the next grain, Q8 */
    if (h->scr && !h->done)
        smp_scrub_block(h, s, clamp(p[P_DIST], 0, 127), n, r);
    /* the filter (DIGITAL's, with its envelope and DRIVE) */
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
    int32_t drive = 32768 + p[P_FLT_DRV] * 512, drv = p[P_FLT_DRV], ic1 = v->s[0], ic2 = v->s[1];
    int32_t fk = ANALOG_K(p[P_FLT_RES]);
    uint32_t ft = (uint32_t)p[P_FTYPE];
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_FLT_RES]);
    for (i = 0; i < n; i++) {
        int32_t y = 0;
        if (h->done) {
        } else if (h->scr) {
            y = smp_scrub(h, s, near, hold, sh);
        } else {
            if (!h->hc) {                             /* LOFI's sample-hold: a new value every hold samples */
                uint32_t k = h->gpos >> 16;
                y = smp_gr(h, h->cin, h->d, &h->g[0], h->pin, h->dp, &h->g[1], k, !h->tail);
                if (!near) {
                    int32_t y2 = k + 1u < hq >> 16 ? smp_gr(h, h->cin, h->d, &h->g[0], h->pin, h->dp, &h->g[1], k + 1u, !h->tail)
                               : h->tail ? 0                  /* (the last grain's end: nothing follows it) */
                                         : smp_gr(h, h->cin + (int32_t)adv, h->dq, &h->g[2], h->cin, h->d, &h->g[0], 0, 1);
                    y += ((y2 - y) * (int32_t)(h->gpos >> 1 & 0x7FFFu)) >> 15;
                }
                if (sh)
                    y = ((y + (1 << (sh - 1u))) >> sh) << sh;
                h->y = y;
            }
            if (++h->hc >= hold)
                h->hc = 0;
            y = h->y;
            h->gpos += step;
            while (h->gpos >= hq) {                   /* the next grain */
                h->gpos -= hq;
                if (h->tail) {
                    h->done = 1;
                    break;
                }
                h->pin = h->cin;
                h->cin += (int32_t)adv;
                h->dp = h->d;                         /* (the window as it is now, for the new grain) */
                h->d = h->dq;
                h->g[1] = h->g[0];
                h->g[0] = h->g[2];
                if (h->mode == SM_ONCE || h->mode == SM_REV) {
                    if (h->cin >> 8 > (int32_t)(h->g[0].L - (2u << (h->gb - 1u))))   /* past the end: the last grain fades out */
                        h->tail = 1;
                } else {
                    while (h->pin >> 8 >= (int32_t)h->g[1].per)   /* (each grain in its own loop; LEN made */
                        h->pin -= (int32_t)(h->g[1].per << 8);           /* shorter: maybe more than one period) */
                    while (h->cin >> 8 >= (int32_t)h->g[0].per)
                        h->cin -= (int32_t)(h->g[0].per << 8);
                }
            }
            if (h->mode == SM_ONCE || h->mode == SM_REV) {
                if (h->cnt < SMP_EFADE)               /* in over 48 samples */
                    y = (y * (int32_t)h->cnt) / SMP_EFADE;
            } else if (h->gate && h->cnt >= h->gate) {   /* GATE's end: out over 12 ms */
                if (!h->gf)
                    h->gf = SMP_GFADE;
                y = (y * h->gf) / SMP_GFADE;
                if (!--h->gf)
                    h->done = 1;
            }
            h->cnt++;
        }
        if (h->dcn) {                                 /* a takeover: the old sound's last value fades */
            y += (h->dc * h->dcn) / SMP_DECLICK;
            h->dcn--;
        }
        h->last = y;
        y = (y * lg) >> 15;
        if (drv)
            y = softclip(((y >> 2) * (drive >> 2)) >> 11);
        y = digital_filter(&flt, fk, ft, y, &ic1, &ic2);
        out[i] += voice_amp(y, m, i);
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
    *hp = hv;
}

static void trk_note_on(track_t *t, uint32_t note, uint32_t vel);   /* (voice.c) */
/* the knob moved: SCRUB needs no key, it starts its own voice (or takes over a sample playing normally); at 0, off.
 * Each block, voices or none (track_render), the value as the matrix leaves it */
static void sampler_block(track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NPART;
    smp_v_t *h = &smp_v[k];
    smp_s_t *s = &smp_s[k];
    int32_t val = clamp(t->p[P_DIST], 0, 127);
    if (val == s->pv)
        return;
    s->pv = (uint8_t)val;
    if (!val) {
        s->on = 0;                                    /* (the voice fades, smp_scrub_block; next time from the knob) */
        return;
    }
    if (!(h->scr && !h->done && h->gen == smp_gen && h->owner && h->owner <= NVOICE && t->v[h->owner - 1u].active &&
          t->v[h->owner - 1u].stage != 4u)) {
        if (!h->scr && !h->done && h->gen == smp_gen && h->owner && h->owner <= NVOICE && t->v[h->owner - 1u].active &&
            t->v[h->owner - 1u].stage != 4u)
            smp_scrub_catch(h, s, val);               /* a sample playing: the head picks it up where it is */
        s->want = 1;
        trk_note_on(t, s->note ? s->note : SMP_NOTE, 100);
        s->want = 0;
    }
}

static const char *const N_SMACH[1] = {"SAMPLE"};
static const preset_t SAMPLER_PRESETS[1] = {
    {"SAMPLE", {64, 64, 0, 127, 0, 0, 0, 0}, {0, 60, 127, 20}, 0, 0, FX(0, 0, 0, 0), PAT(0)},
};
static const engine_t ENG_SAMPLER = {
    .name = "SAMPLER",
    .page_title = {"OSC 1", "OSC 2"},
    .edit = {{"PITCH", F_INT, 0, 127, 64, 0, 0}, {"STRCH", F_INT, 0, 127, 64, 0, 0}, {"START", F_INT, 0, 127, 0, 0, 0},
             {"LEN", F_INT, 0, 127, 127, 0, 0}, {"MODE", F_ENUM, 0, 3, 0, N_SMODE, 0}, {"SLOT", F_INT, 0, SMP_MAX - 1, 0, 0, 0},
             {"LOFI", F_INT, 0, 127, 0, 0, 0}, {"MACH", F_ENUM, 0, 0, 0, N_SMACH, 0}},
    .presets = SAMPLER_PRESETS,
    .npresets = 1,
    .note_on = sampler_note_on,
    .render = sampler_render,
    .amp = sampler_amp,
    .block = sampler_block,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .sampled = 1,
};
