/* SAMPLER: plays the chunks the sample uploader wrote to the flash (docs/samplelab.html is the spec: its render() and
 * stretchBuf() are the reference). One machine, one voice a track (a new hit takes over, its old sound fades in 4 ms).
 *
 * The sample memory: flash 0xA0000..0xDBFFF (Felucca's old user sample slots), read in place through the XIP window.
 *   0xA0000  the table (4 KiB): u32 magic "FSM2", u16 version 1, u16 count (<= SMP_MAX), u32 data length, u32 data
 *            CRC32, 12 bytes 0, u32 table CRC32 (bytes 0..27, then the entries), then count entries of 24 bytes:
 *            char name[12] (NUL padded), u32 offset (in the data), u32 length (samples), u16 rate (Hz), u16 coding (0)
 *   0xA1000  the data: 8-bit mu-law (G.711), one byte a sample (the user chose twice the time over 16 bits)
 * All little endian. The web uploader builds the image (web/emu/index.html smpImage); the device checks it whole.
 *
 * Knobs: OSC 1 PITCH STRETCH START LEN, OSC 2 MODE SLOT LOFI LEVEL (P_LN0), the GATE page GATE (P_LN2). E7 = the
 * machine (one). The ENV and FILTER pages work as on the FM machines (the filter is DIGITAL's). */
#ifndef SMP_XIP                                     /* the emulator and the host tests: a RAM image of the flash */
#include "../hal/fm1_xip.h"
#define SMP_XIP(off) fm1_xip_ptr(off)
#endif
#define SMP_BASE 0xA0000u
#define SMP_DATA (SMP_BASE + 0x1000u)
#define SMP_CAP (0xDC000u - SMP_DATA)               /* 241664 bytes = samples: ~11 s at 22 kHz */
#define SMP_MAX 32u                                 /* chunks S01..S32 */
#define SMP_MAGIC 0x324D5346u                       /* "FSM2" */
#define SMP_NOTE 60                                 /* the note that plays a chunk at its own pitch */
#define SMP_DECLICK 176                             /* a hit taking over: the old sound fades over 4 ms */
#define SMP_GFADE 529                               /* GATE's end: 12 ms */
#define SMP_EFADE 48                                /* ONCE / REV: in and out over 48 samples (the prototype's) */
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

typedef struct {                 /* a track's voice (one: a hit takes over) */
    uint8_t owner;               /* the voice's index + 1 (0: none) */
    uint8_t mode, near, done, keyg, hold, hc, sh, gb;
    uint16_t rate;
    int32_t dir;                 /* 1, REV -1 */
    const uint8_t *d;            /* the window's first sample (REV: its last) */
    uint32_t gen, L, G, Ls, Lp, cf, cfw, per, finv, srq;
    uint64_t pos;                /* in the stretched window, Q16 */
    uint32_t cnt, gate, fo;      /* samples since the hit; GATE's length (0: the key's); ONCE's fade out left */
    int32_t y, last, dc, dcn, gf;
} smp_v_t;
static smp_v_t smp_v[NPART];
static uint32_t smp_rcp;                            /* 2^32 / (pitch_inc(SMP_NOTE) >> 8): pitch ratios without a divide */

static inline smp_v_t *smp_of(track_t *t, voice_t *v)
{
    smp_v_t *h = &smp_v[(uint32_t)(t - trk) % NPART];
    return h->owner == (uint32_t)(v - t->v) + 1u ? h : 0;
}

/* sample i of the window (mu-law) */
static inline int32_t smp_raw(const smp_v_t *h, uint32_t i) { return SMP_ULAW[h->d[(int32_t)i * h->dir]]; }
/* where grain j starts in the window: its place in the output / the stretch (the last ones stop at the end) */
static inline uint32_t smp_in(const smp_v_t *h, uint32_t j)
{
    uint32_t i = (uint32_t)(((uint64_t)(j << (h->gb - 1u)) * h->finv) >> 12);
    return i < h->L - h->G ? i : h->L - h->G;
}
/* sample o of the stretched window: two Hann grains half a grain apart, added (the prototype's stretchBuf) */
static inline int32_t smp_st(const smp_v_t *h, uint32_t o)
{
    uint32_t hb, j, k, wi;
    int32_t v;
    if (!h->G)
        return smp_raw(h, o);
    hb = h->gb - 1u;
    j = o >> hb;
    k = o & ((1u << hb) - 1u);
    wi = h->gb >= 8u ? k >> (h->gb - 8u) : k << (8u - h->gb);
    v = smp_raw(h, smp_in(h, j) + k) * SMP_HANN[wi];
    if (j)
        v += smp_raw(h, smp_in(h, j - 1u) + k + (1u << hb)) * SMP_HANN[wi + 128u];
    return v >> 15;
}
/* sample i of the loop: its start crossfaded with the end past it */
static inline int32_t smp_lv(const smp_v_t *h, uint32_t i)
{
    int32_t w;
    if (i >= h->cf)
        return smp_st(h, i);
    w = (int32_t)(i * h->cfw);
    return (smp_st(h, i) * w + smp_st(h, h->Lp + i) * (32768 - w)) >> 15;
}
/* sample i of what the mode plays (i < 2 periods) */
static inline int32_t smp_at(const smp_v_t *h, uint32_t i)
{
    switch (h->mode) {
    case SM_LOOP:
        return smp_lv(h, i >= h->Lp ? i - h->Lp : i);
    case SM_PING:
        i = i >= h->per ? i - h->per : i;
        return smp_lv(h, i < h->Lp ? i : h->per - 1u - i);
    default:
        return smp_st(h, i < h->Ls - 1u ? i : h->Ls - 1u);
    }
}

static void sampler_note_on(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    smp_v_t *h = &smp_v[(uint32_t)(t - trk) % NPART];
    uint32_t vi = (uint32_t)(v - t->v) + 1u, sl = (uint32_t)p[P_E5], n, a, b, s = (uint32_t)p[P_E1] & 127u;
    uint32_t lf = (uint32_t)p[P_E6] & 127u, g = (uint32_t)p[P_LN2], bpm = (uint32_t)song.g[G_BPM];
    int32_t dc = 0;
    const smp_chunk_t *c;
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
    a = (uint32_t)(p[P_E2] & 127) * (n - 1u) / 127u;  /* START, LEN (a length: the window moves with START) */
    b = a + (uint32_t)(p[P_E3] & 127) * (n - 1u) / 127u;
    b = b > n - 1u ? n - 1u : b;
    if (b - a < 64u) {
        b = a + 64u > n - 1u ? n - 1u : a + 64u;
        a = b >= 64u ? b - 64u : 0u;
    }
    h->L = b - a + 1u;
    h->mode = (uint8_t)((uint32_t)p[P_E4] & 3u);
    h->dir = h->mode == SM_REV ? -1 : 1;
    h->d = SMP_XIP(c->off + (h->mode == SM_REV ? b : a));
    h->rate = c->rate;
    h->srq = ((uint32_t)c->rate << 16) / (uint32_t)FS;
    if (s != 64u) {                                   /* STRETCH: grains of ~40 ms (a power of two), at most half the window */
        uint32_t gb = c->rate > 16000u ? 10u : 9u;
        while (gb > 4u && (1u << gb) > h->L / 2u)
            gb--;
        if ((1u << gb) <= h->L / 2u) {
            h->gb = (uint8_t)gb;
            h->G = 1u << gb;
            h->finv = (4096u * 4096u) / SMP_STRETCH[s];
            h->Ls = (uint32_t)(((uint64_t)h->L * SMP_STRETCH[s]) >> 12);
        }
    }
    if (!h->G)
        h->Ls = h->L;
    if (h->mode == SM_LOOP || h->mode == SM_PING) {   /* the end crossfades into the start (~256 samples) */
        h->cf = h->Ls / 4u < 256u ? h->Ls / 4u : 256u;
        h->Lp = h->Ls - h->cf;
        h->cfw = h->cf ? 32768u / h->cf : 0u;
        h->per = h->mode == SM_LOOP ? h->Lp : 2u * h->Lp;
        if (g)                                        /* GATE in steps (1/16s at the tempo) */
            h->gate = g * (FS * 60u / 4u) / (bpm ? bpm : 120u);
    }
    h->keyg = (uint8_t)!g;
    h->near = (uint8_t)(lf >= 16u);                   /* LOFI: nearest sample above ~12 % */
    h->sh = (uint8_t)(lf >= 2u ? 16u - (16u * 127u - 12u * lf + 63u) / 127u : 0u);   /* 16 .. 4 bits */
    h->hold = SMP_HOLD[lf];
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
    uint32_t i, step;
    int32_t lg = p[P_LN0] * 284;                      /* LEVEL: 0 .. 1.1 (the prototype's), Q15 */
    uint64_t end;
    if (!hp)
        return;
    hv = *hp;
    if (h->gen != smp_gen)
        h->done = 1;
    {                                                 /* PITCH (+-24 semitones) and the note: a resampling rate */
        int32_t p16 = clamp(m->pitch16 + (p[P_E0] - 64) * 6, 0, 2047);
        uint32_t r = (uint32_t)(((uint64_t)pitch_inc((uint32_t)p16) * smp_rcp) >> 24);   /* Q16 */
        step = (uint32_t)(((uint64_t)r * h->srq) >> 16);
    }
    end = h->mode == SM_ONCE || h->mode == SM_REV ? (uint64_t)(h->Ls - 1u) << 16 : (uint64_t)h->per << 16;
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
    }
    int32_t drive = 32768 + p[P_FLT_DRV] * 512, drv = p[P_FLT_DRV], ic1 = v->s[0], ic2 = v->s[1];
    int32_t fk = ANALOG_K(p[P_FLT_RES]);
    uint32_t ft = (uint32_t)p[P_FTYPE];
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_FLT_RES]);
    for (i = 0; i < n; i++) {
        int32_t y = 0;
        if (!h->done) {
            if (!h->hc) {                             /* LOFI's sample-hold: a new value every hold samples */
                uint32_t ix = (uint32_t)(h->pos >> 16);
                y = smp_at(h, ix);
                if (!h->near)
                    y += ((smp_at(h, ix + 1u) - y) * (int32_t)((uint32_t)h->pos >> 1 & 0x7FFFu)) >> 15;
                if (h->sh)
                    y = ((y + (1 << (h->sh - 1u))) >> h->sh) << h->sh;
                h->y = y;
            }
            if (++h->hc >= h->hold)
                h->hc = 0;
            y = h->y;
            h->pos += step;
            if (h->mode == SM_ONCE || h->mode == SM_REV) {
                if (h->cnt < SMP_EFADE)               /* in .. */
                    y = (y * (int32_t)h->cnt) / SMP_EFADE;
                if (h->pos >= end) {
                    h->done = 1;
                } else if (!h->fo && end - h->pos < (uint64_t)step * SMP_EFADE) {
                    h->fo = (uint32_t)((end - h->pos) >> 16) * 65536u / (step ? step : 1u) + 1u;   /* .. and out */
                    h->fo = h->fo > SMP_EFADE ? SMP_EFADE : h->fo;
                }
                if (h->fo) {
                    y = (y * (int32_t)h->fo) / SMP_EFADE;
                    if (h->fo > 1u)
                        h->fo--;
                }
            } else {
                while (h->pos >= end)
                    h->pos -= end;
                if (h->gate && h->cnt >= h->gate) {   /* GATE's end: out over 12 ms */
                    if (!h->gf)
                        h->gf = SMP_GFADE;
                    y = (y * h->gf) / SMP_GFADE;
                    if (!--h->gf)
                        h->done = 1;
                }
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
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .sampled = 1,
};
