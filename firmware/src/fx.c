/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Effects: per-track DIST and INSERT (1.5), then sends into three
 * shared buses (chorus, tempo delay, reverb). Mono buses, stereo dry mix. */
#define DLY_LEN 65536u           /* 1.49 s: 1/4 fits from 41 BPM, 1/4D from 61, 1/2 from 81 */
static int16_t dly_buf[DLY_LEN] __attribute__((section(".pool")));
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[2] = {556, 441};
static int16_t rev_comb[1116 + 1188 + 1277 + 1356] __attribute__((section(".pool")));
static union {                          /* ROOM's allpasses; SPRING's allpass chain (int32: no clamps) */
    int16_t ap[556 + 441];
    int32_t sp[(556 + 441) / 2];
} rev_u __attribute__((section(".pool")));
#define rev_ap (rev_u.ap)
static struct {
    uint32_t dly_w;
    int32_t dly_cur;                     /* SIMPLE: the delay time now (Q8 samples), gliding to the knob: the tape / BBD repitch */
    int32_t dly_lp, dly_lp2, dly_hp;     /* the delay's loop: two low-passes, a high-pass */
    uint32_t dly_ph;                     /* .. the tap's slow wobble (a BBD's clock drift) */
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint8_t rtype;                       /* the reverb model running (G_RTYPE: 0 ROOM, 1 SPRING, 2 HALL) */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop
                                             * length (Q8, glides) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
} fx;

/* SPRING (G_RTYPE 1): one spring of a spring tank, mono like the other buses, in the ROOM's own buffers (no
 * RAM of its own): the input and the loop's return -> a low cut (~110 Hz: a spring carries little bass) ->
 * SP_N stretched first-order allpasses, (a + z^-4) / (1 + a z^-4) (after Valimaki, Parker and Abel: below
 * fs / 8 = 5.5 kHz the group delay rises with frequency, the chirp; each pass round the loop adds more of
 * it: the "boing", the drips) -> the loop's delay line (rev_comb, SP_LEN) -> back through a one-pole
 * low-pass (DAMP) and the decay gain (SIZE). The output: the spring's far end, half way along the loop
 * (the first sound 15 .. 30 ms after the send: the tank's own pre-delay; a slow wobble of a sample or two
 * on it), plus a second, quieter pickup at three quarters (a shorter spring beside it: denser). SIZE sets
 * the loop's length (30 .. 60 ms) and its decay; DAMP the loop's low-pass. The allpasses' states: 4
 * samples each (int32), in rev_ap's memory. Changing the model fades the old one's block out and clears both buffers. */
#define SP_LEN 4096u                     /* the loop's line in rev_comb (4937 samples) */
#define SP_MASK (SP_LEN - 1u)
#define SP_N 10u                         /* allpass stages */
#define SP_A 2867                        /* their coefficient, Q12 (0.7: Q12 keeps (x - o) * a in 32 bits up to
                                          * |x - o| < 749000, far past any peak the chain reaches) */
_Static_assert(sizeof rev_comb / 2u >= SP_LEN && sizeof rev_u.sp / 4u >= 4u * (SP_N + 1u), "SPRING in ROOM's buffers");

/* HALL (G_RTYPE 2, 1.2): a smooth, dense, stereo reverb, again in ROOM's buffers (its few states and one block
 * of work in the pool, no .bss). ROOM's 4 combs each ring on their own (the metallic tone); here every echo
 * goes round 8 lines that all feed each other, and they run at half the rate (22.05 kHz: the same memory holds
 * twice the time, so twice the resonances, at half the work; the tail stops near 10 kHz, as a hall's air
 * does). The send (every other sample of it, low-passed 1 2 1; ROOM's level) -> a low cut (~110 Hz: no boom
 * in a long tail) -> 2 allpass diffusers (rev_ap: a drum's click smeared into a burst) -> into the 8 lines
 * (rev_comb, mutually prime lengths, 21 .. 34 ms). Lines 1, 2, 4, 7 are read through slow sines (+-HL_MOD
 * samples, between samples; 0.37 or 0.74 Hz, each in its own phase): the resonances drift instead of ringing.
 * Lines 0, 3, 5, 6, read at their ends, go through a one-pole low-pass (DAMP: the mixing takes every echo
 * through them); every
 * line through its gain (from its length: all decay alike; RT60 below the damping 0.5 s at SIZE 0, 4 s at
 * 90, 9 s at 127; ~2.4 s heard at 90 with DAMP 60); then the 8 x 8 mixing: a conference matrix (Paley,
 * order 8: every line feeds the 7 others, +-1 each, none itself: no flutter at a line's own length), with
 * the send added. Stereo: left lines 0 3 4 -7, right 1 2 5 6 (sums spread over the matrix's rows); the wet
 * bus (mono) gets left + right, rev_side left - right, which mix_block adds to the left and takes from the
 * right (rev_side_mix). Back to 44.1 kHz between samples. Rounding: a loud line's gain rounds to the nearest
 * (the tail decays as it should), a quiet one's towards zero (it ends in exact silence: no limit cycle), and
 * the low-passes keep their steps' remainders (no offset). */
#define HL_NL 8u                         /* lines */
#define HL_NAP 2u                        /* diffusers */
#define HL_NC (CTL / 2u)                 /* a block at half the rate */
#define HL_MOD 8                         /* the taps' swing, samples (read 2 .. 18 samples short of the end) */
#define HL_LPM 0x69u                     /* the lines with a low-pass */
#define HL_KD(d) (32767 - (d) * 180)     /* its coefficient (Q15) for DAMP d: none at 0 .. ~1 kHz at 127 */
#define HL_LOUD 32                       /* a line this loud (two samples' sum) rounds its gain to the nearest */
#define HL_OUT 4                         /* the outputs' gain: the level of ROOM's tail */
static const uint16_t HL_LEN[HL_NL] = {467, 509, 547, 593, 631, 673, 709, 743};   /* (+1 each: the mirror) */
static const uint16_t HL_AP[HL_NAP] = {131, 307};
static const int16_t HL_APG[HL_NAP] = {11469, 10240};               /* Q14: 0.7, 0.625 */
_Static_assert(467 + 509 + 547 + 593 + 631 + 673 + 709 + 743 + HL_NL <= sizeof rev_comb / 2u &&
               131 + 307 <= sizeof rev_u.ap / 2u, "HALL in ROOM's buffers");
_Static_assert(CTL % 2u == 0u && HL_NC + 2u * HL_MOD + 2u < 467u && HL_NC <= 131u, "HALL: one wrap a block at most");
static struct {                          /* HALL's state (in the pool: no .bss) */
    uint16_t i[HL_NL], d[HL_NAP];        /* the lines' write indices, the diffusers' */
    int32_t lp[HL_NL], le[HL_NL];        /* the low-passes and their steps' remainders */
    int32_t x, m, s, hp, he;             /* the send's last sample, its low cut and that step's remainder; the
                                          * last mid and side out (half rate) */
    int16_t g[HL_NL], sz;                /* the lines' gains / sqrt(7) (Q14), for SIZE sz - 1 (0: none yet) */
    uint32_t ph;                         /* the taps' drift */
    uint8_t side;                        /* sd holds this block's stereo difference (HALL running) */
    int32_t sd[CTL];                     /* the block's left minus right (mix_block: rev_side_mix) */
    int32_t bx[HL_NC], l[HL_NC], r[HL_NC];   /* the block at half the rate: the diffused send, left, right */
    int16_t y[HL_NL][HL_NC];             /* the lines' ends, then what goes back into them */
} hl __attribute__((section(".pool")));
#define rev_side (hl.sd)


/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* MENU > USB LEVEL FIXED (for #42: record over USB with the speaker turned down): the mix goes to master_out at the
 * full MASTER level, USB audio takes that (audio.c uac_tap), and only then does MASTER scale what the DAC gets
 * (usb_fixed_dac). MASTER (0, the default): MASTER before master_out, as always (USB follows the knob) */
static volatile uint8_t fx_usb_fixed;
#define MASTER_FULL 4096                /* main.c: the MASTER knob's top, Q12 */
/* the MASTER pot (ADC 0..1023, smoothed in main.c) to its level, square law: 0 .. 4088 (Q12) */
static inline uint32_t master_of_pot(uint32_t k10) { return (k10 * k10) >> 8; }
static __attribute__((noinline)) void usb_fixed_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i;
    int32_t m = (int32_t)song.master_q12;
    for (i = 0; i < 2u * n; i++)
        out[i] = (out[i] * m) >> 12;                /* (|out| <= 32767 after the soft clip: fits) */
}
static volatile uint8_t fx_lowcut;     /* settings: 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
                                        * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}

/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its
 * own envelope (a level-following trapezoid: odd harmonics), then band-passed ~220 Hz..1 kHz and added.
 * The low-pass has 4 poles (#42: with 2, the trapezoid rebuilt the 300 .. 600 Hz of the mix itself, late,
 * and cancelled up to 6 dB of it; now under 0.5 dB) */
static int32_t sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl;
static inline int32_t spk_bass(int32_t m)
{
    int32_t a, t, u;
    sb_lp1 += ((m - sb_lp1) * 692) >> 15;
    sb_lp2 += ((sb_lp1 - sb_lp2) * 692) >> 15;
    sb_lp3 += ((sb_lp2 - sb_lp3) * 692) >> 15;
    sb_lp4 += ((sb_lp3 - sb_lp4) * 692) >> 15;
    a = sb_lp4 < 0 ? -sb_lp4 : sb_lp4;
    if (a > sb_env)
        sb_env += (a - sb_env) >> 2;
    else if (sb_env > 0)
        sb_env -= (sb_env >> 11) + 1;
    t = clamp(sb_lp4 * 8, -sb_env, sb_env);
    sb_h1 += (t - sb_h1) >> 5;
    u = t - sb_h1;
    sb_h2 += (u - sb_h2) >> 5;
    u -= sb_h2;
    sb_hl += (u - sb_hl) >> 3;
    return sb_hl + (sb_hl >> 1);      /* x1.5 (#180: x3 peaked at 1.5 .. 3.5 x a full-scale kick, the limiter */
}                                     /* pulled the mix down up to 16 dB on each hit and the buzz took over) */

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {                  /* two one-pole high-passes, error feedback as dc_block (the */
        uint32_t sh = fx_lowcut == 2u ? 5u : 6u;    /* rounded step stopped at |x - lc| < 32: an offset) */
        int32_t b = fx_lowcut == 2u ? spk_bass((*l + *r) >> 1) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + b;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + b;
    }
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 12) + 1;
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    uint32_t bpm = song.g[G_BPM] >= 40 ? (uint32_t)song.g[G_BPM] : 120u;   /* (BPM 0 for a moment, #282: no divide by 0) */
    return song.g[G_CLOCK] && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / bpm;
}
static uint32_t div_samples(uint32_t div)
{
    uint32_t quarter = beat_samples();
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : div == 10u ? quarter / 16u : quarter / DIV_DEN[div % 6u];   /* (10: 1/64, SIMPLE's SEQ) */
}

#include "repeat.c"                                  /* PERF's beat repeats (the master) */
#include "mfx.c"                                     /* the groovebox's master DJ filter and comb */
#include "click.c"                                   /* the metronome's click (after the master: audio.c) */

/* the delay's TIME (params.c N_DTIME): N_DIV's ten (div_samples), then 1.5.1's dotted 1/8D 1/16D 1/4D (10, 11, 12: 3/4,
 * 3/8, 3/2 of a quarter); at most DLY_LEN - 1 samples (1.486 s): 1/4D clamps at 60 BPM and below (1/2 below 81) */
static const uint8_t DIV_DOT[3] = {2, 3, 1};           /* 3 quarters >> this */
/* SIMPLE: the delay runs free of the tempo: TIME 0..127 is 20 ms .. 1.45 s, exponential (6.2 octaves). -> samples, Q8 */
static const uint32_t EXP2_Q15[17] = {32768, 34219, 35734, 37316, 38968, 40693, 42495, 44376, 46341, 48393, 50535, 52773, 55109, 57549, 60097, 62757, 65536};
static int32_t delay_q8(void)
{
    uint32_t e = (uint32_t)song.g[G_DTIME] * 3189u, oct = e >> 16, fr = (e >> 12) & 15u, sub = e & 4095u;
    uint32_t m = EXP2_Q15[fr] + (((uint32_t)EXP2_Q15[fr + 1u] - EXP2_Q15[fr]) * sub >> 12);
    uint32_t s = ((880u << oct) * m) >> 15;
    return (int32_t)(s >= DLY_LEN - 2u ? DLY_LEN - 2u : s) << 8;
}
static uint32_t delay_samples(void)
{
    uint32_t d = (uint32_t)song.g[G_DTIME];
    uint32_t s = d - 10u < 3u ? (beat_samples() * 3u) >> DIV_DOT[d - 10u] : div_samples(d);
    return s < 16u ? 16u : s >= DLY_LEN ? DLY_LEN - 1u : s;
}


#define QV_GAIN 22                                       /* QUAD's outputs: about ROOM's tail */
/* QUAD (SIMPLE: the one reverb): a 90s rack reverb, a plate tank of the Dattorro kind (Griesinger's figure-of-eight:
 * four input diffusers, then two halves each a modulated allpass, a delay, a damping low-pass, the decay, an allpass, a
 * delay, crossing into the other half) run at a quarter of the rate (11 kHz: the band stops ~5 kHz), filtered in and
 * out so it is clean. GRIT (G_RTYPE, 0..127, squared: only near the top) opens those filters (the old converters'
 * aliasing), holds the samples instead of joining them and takes the bits down to 8. Stereo: taps of both halves (rev_side). TIME 127 is FREEZE: the input
 * fades out, the decay is 1 and the damping opens: the tail holds as long as it is left there. In ROOM's buffers
 * (rev_comb, rev_ap), as SPRING and HALL; a model change clears them */
#define QV_R 4u                                          /* the rate's divider */
#define QV_NC (CTL / QV_R)
enum { QV_D0, QV_D1, QV_D2, QV_D3, QV_MAL, QV_DL1, QV_APL, QV_DL2, QV_MAR, QV_DR1, QV_APR, QV_DR2, QV_NL };
static const uint16_t QV_LEN[QV_NL] = {37, 27, 95, 70, 169 + 8, 1122, 454, 937, 229 + 8, 1063, 669, 797};
static const uint8_t QV_IN_AP[QV_NL] = {0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0};   /* 1: in rev_ap, 0: in rev_comb */
_Static_assert(37 + 27 + 95 + 70 + 1122 + 937 + 1063 + 669 + 797 <= sizeof rev_comb / 2u &&
               169 + 8 + 454 + 229 + 8 <= sizeof rev_u.ap / 2u, "QUAD in ROOM's buffers");
static struct {
    uint16_t w[QV_NL];                                   /* each line's write place */
    int32_t bw, dl, dr, le, re;                          /* the input's low-pass; the damping low-passes; the halves' ends */
    int32_t pl, pr, acc[2];                              /* the last outputs (back to 44.1 kHz between them) */
    int32_t fi[2], fo[4];                                /* the filters in and out (two poles each, a side) */
    uint32_t ph;                                         /* the modulation */
    int32_t frz;                                         /* FREEZE's share (Q15) */
} qv __attribute__((section(".pool")));
/* QUAD's products: 64-bit (no overflow), rounded to the nearest (a tail dies out to 0, no offset left) */
static inline int32_t qmul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + 16384) >> 15); }
static int16_t *qv_buf(uint32_t k)                       /* line k's memory */
{
    uint32_t j, oc = 0, oa = 0;
    for (j = 0; j < k; j++) {
        if (QV_IN_AP[j])
            oa += QV_LEN[j];
        else
            oc += QV_LEN[j];
    }
    return QV_IN_AP[k] ? rev_ap + oa : rev_comb + oc;
}
static int16_t *qv_b[QV_NL];
static const uint16_t QV_DEC[127] = {   /* TIME 0..126 -> the decay (Q15): RT60 0.3 s * e^(TIME / 36.6), ~3.5 s at 90 */
    1997, 2154, 2317, 2489, 2668, 2854, 3048, 3249, 3458, 3674, 3897, 4127, 4364, 4607, 4857, 5113,
    5376, 5644, 5918, 6197, 6481, 6770, 7064, 7362, 7664, 7970, 8279, 8592, 8907, 9225, 9545, 9868,
    10192, 10517, 10844, 11172, 11501, 11830, 12159, 12488, 12816, 13144, 13472, 13798, 14123, 14447, 14769, 15090,
    15408, 15724, 16038, 16350, 16659, 16965, 17269, 17569, 17867, 18161, 18452, 18739, 19024, 19304, 19581, 19855,
    20124, 20390, 20653, 20911, 21165, 21416, 21663, 21905, 22144, 22379, 22610, 22837, 23060, 23280, 23495, 23706,
    23914, 24118, 24318, 24514, 24706, 24895, 25080, 25261, 25439, 25613, 25783, 25950, 26114, 26274, 26430, 26584,
    26734, 26881, 27025, 27165, 27303, 27437, 27569, 27697, 27823, 27946, 28066, 28183, 28298, 28410, 28519, 28626,
    28731, 28833, 28932, 29029, 29124, 29217, 29307, 29395, 29481, 29565, 29647, 29727, 29806, 29882, 29956};
static int32_t qv_tap(uint32_t k, uint32_t back)        /* line k, `back` samples before the next write (1..LEN) */
{
    uint32_t L = QV_LEN[k];
    return qv_b[k][(qv.w[k] + L - back) % L];
}
static void qv_put(uint32_t k, int32_t v)
{
    qv_b[k][qv.w[k]] = (int16_t)clamp(v, -32767, 32767);
    if (++qv.w[k] >= QV_LEN[k])
        qv.w[k] = 0;
}
static int32_t qv_ap(uint32_t k, int32_t x, int32_t g)  /* an allpass of line k's length, coefficient g (Q15) */
{
    int32_t d = qv_tap(k, QV_LEN[k]), v = x - qmul(d, g);
    qv_put(k, v);
    return d + qmul(v, g);
}
static int32_t qv_mod(uint32_t k, int32_t x, int32_t g, int32_t ofs)   /* .. with its delay moving +-4 (ofs Q8) */
{
    uint32_t L = QV_LEN[k], D = ((L - 8u) << 8) + (uint32_t)(ofs + (4 << 8)), di = D >> 8, fr = D & 255u;
    int32_t a = qv_tap(k, di), b = qv_tap(k, di + 1u), d = a + (((b - a) * (int32_t)fr) >> 8), v = x - qmul(d, g);
    qv_put(k, v);
    return d + qmul(v, g);
}
static __attribute__((noinline)) void rev_quad(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, j, k;
    int32_t sz = song.g[G_RSIZE], freeze = sz >= 127, dec = QV_DEC[clamp(sz, 0, 126)];
    int32_t dmp = 32767 - song.g[G_RDAMP] * 220, pl = qv.pl, pr = qv.pr;
    int32_t gr = clamp(song.g[G_RTYPE], 0, 127), q = gr * gr / 127;   /* GRIT, squared: clean until near the top */
    int32_t kin = 20700 + ((32767 - 20700) * q) / 127, kout = 22000 + ((32767 - 22000) * q) / 127;   /* ~7 / 8 kHz .. open */
    int32_t hold = q > 64 ? (q - 64) * 520 : 0, bits = 16 - (q * 8) / 127;   /* (hold Q15: 0 .. ~1; 16 .. 8 bits) */
    (void)n;
    if (!qv_b[0])
        for (k = 0; k < QV_NL; k++)
            qv_b[k] = qv_buf(k);
    for (i = 0; i < QV_NC; i++) {
        int32_t x = 0, a, b, l, r, tri, gin, dk, dp;
        for (j = 0; j < QV_R; j++) {                     /* down to 11 kHz: two poles, then the four averaged */
            int32_t xi = qmul(clamp(rev_in[QV_R * i + j], -262143, 262143), 2580);   /* (ROOM's level) */
            qv.fi[0] += qmul(xi - qv.fi[0], kin);
            qv.fi[1] += qmul(qv.fi[0] - qv.fi[1], kin);
            x += qv.fi[1];
        }
        x >>= 2;
        qv.frz += clamp((freeze ? 32768 : 0) - qv.frz, -512, 512);   /* (~23 ms) */
        gin = 32768 - qv.frz;
        dk = dec + (int32_t)(((int64_t)(32768 - dec) * qv.frz) >> 15);
        dp = dmp + (int32_t)(((int64_t)(32767 - dmp) * qv.frz) >> 15);
        x = (int32_t)(((int64_t)x * gin) >> 15);
        qv.bw += qmul(x - qv.bw, 20000);                 /* the input's band (the rate does the rest) */
        a = qv_ap(QV_D0, qv.bw, 24576);
        a = qv_ap(QV_D1, a, 24576);
        a = qv_ap(QV_D2, a, 20480);
        a = qv_ap(QV_D3, a, 20480);
        qv.ph += 272700u;                                /* ~0.7 Hz */
        tri = (int32_t)(qv.ph >> 23);                    /* (0 .. 511: up, then down) */
        tri = (tri < 256 ? tri : 511 - tri) * 8 - (4 << 8);   /* (-4 .. +4 samples, Q8) */
        tri = (int32_t)(((int64_t)tri * gin) >> 15);     /* (FREEZE: still, so nothing smooths the tail away) */
        /* the left half, fed by the right's end; the right, by the left's */
        l = qv_mod(QV_MAL, a + qmul(qv.re, dk), -22938, tri);
        b = qv_tap(QV_DL1, QV_LEN[QV_DL1]);
        qv_put(QV_DL1, l);
        qv.dl += qmul(b - qv.dl, dp);
        b = qv_ap(QV_APL, qmul(qv.dl, dk), 16384);
        l = qv_tap(QV_DL2, QV_LEN[QV_DL2]);
        qv_put(QV_DL2, b);
        r = qv_mod(QV_MAR, a + qmul(qv.le, dk), -22938, -tri);
        b = qv_tap(QV_DR1, QV_LEN[QV_DR1]);
        qv_put(QV_DR1, r);
        qv.dr += qmul(b - qv.dr, dp);
        b = qv_ap(QV_APR, qmul(qv.dr, dk), 16384);
        r = qv_tap(QV_DR2, QV_LEN[QV_DR2]);
        qv_put(QV_DR2, b);
        qv.le = l;
        qv.re = r;
        /* the taps: both halves to each side, then GRIT's bits */
        l = qv_tap(QV_DL1, 67) + qv_tap(QV_DL1, 749) - qv_tap(QV_APL, 400) + qv_tap(QV_DL2, 503) - qv_tap(QV_DR1, 501) -
            qv_tap(QV_APR, 47) - qv_tap(QV_DR2, 269);
        r = qv_tap(QV_DR1, 89) + qv_tap(QV_DR1, 914) - qv_tap(QV_APR, 309) + qv_tap(QV_DR2, 674) - qv_tap(QV_DL1, 532) -
            qv_tap(QV_APL, 84) - qv_tap(QV_DL2, 30);
        l = qmul(l, 19661);
        r = qmul(r, 19661);
        if (bits < 16) {
            int32_t h = 1 << (15 - bits);
            l = (l + h) & ~(2 * h - 1);
            r = (r + h) & ~(2 * h - 1);
        }
        for (j = 0; j < QV_R; j++) {                     /* back to 44.1 kHz, between the two (GRIT: held), one pole */
            int32_t lo = pl + (((l - pl) * (int32_t)(j + 1u)) >> 2), ro = pr + (((r - pr) * (int32_t)(j + 1u)) >> 2);
            lo += qmul(pl - lo, hold);
            ro += qmul(pr - ro, hold);
            qv.fo[0] += qmul(lo - qv.fo[0], kout);
            qv.fo[1] += qmul(qv.fo[0] - qv.fo[1], kout);
            qv.fo[2] += qmul(ro - qv.fo[2], kout);
            qv.fo[3] += qmul(qv.fo[2] - qv.fo[3], kout);
            lo = qv.fo[1];
            ro = qv.fo[3];
            out[QV_R * i + j] += ((lo + ro) >> 1) * QV_GAIN;
            rev_side[QV_R * i + j] = ((lo - ro) >> 1) * QV_GAIN;
        }
        pl = l;
        pr = r;
    }
    qv.pl = pl;
    qv.pr = pr;
    hl.side = 1;
}


#if FELUCCA_CPU2
/* the dry mix and the sends, as each core adds its parts into them (after Jangada: mx[1] is the second core's,
 * mix_block), and a part's own block (b; mx[0].b also the fade of a reverb model change, after the parts) */
typedef struct { int32_t l[CTL], r[CTL], d[CTL], v[CTL], b[CTL]; } mixsum_t;
static mixsum_t mx[2];
#define mix_l (mx[0].l)
#define mix_r (mx[0].r)
#define send_d (mx[0].d)
#define send_r (mx[0].v)
#define part_buf (mx[0].b)
#define SM_DECL , mixsum_t *sm                          /* mix_part / mix_spread: the sums they add into */
#define SM_ARG(x) , (x)
#define SM(f) (sm->f)
#else
/* one core: the arrays of 1.6, the same code (SM(l) = mix_l, ...; send_d .. mix_r are declared below) */
static int32_t part_buf[CTL];                            /* a part's block (mix_part); the fade of a model change */
#define SM_DECL
#define SM_ARG(x)
#define SM(f) SM1_##f
#define SM1_l mix_l
#define SM1_r mix_r
#define SM1_d send_d
#define SM1_v send_r
#define SM1_b part_buf
#endif


/* process the two buses (the delay, the reverb) for one block; sends in, wet stereo-equal out */
static void fx_buses(const int32_t *dly_in, const int32_t *rev_in, int32_t *wet,
                     uint32_t n)
{
    uint32_t i, dl = delay_samples();
    int32_t fb = song.g[G_DFDBK] * 260, col = 2000 + song.g[G_DCOLOR] * 240, hpk = 700;
    int32_t dtarget = delay_q8();
    {   /* TONE: 64 the dub echo, below it the repeats go darker (a low-pass), above it thinner (a high-pass) */
        int32_t tn = song.g[G_DCOLOR] - 64;
        col = tn < 0 ? 14000 + tn * 180 : 14000 + tn * 127;
        hpk = tn > 0 ? 700 + tn * 130 : 700;
    }
#define SIMPLE_DT(dl) (fx.dly_cur)
    int32_t dmix = song.g[G_DMIX] * 258;
    int32_t rt;
    for (i = 0; i < n; i++) {
        int32_t y = 0, x;
        /* delay: a dub-style BBD echo. The tap wobbles slowly (a clock that drifts: +-20 samples at 0.8 Hz), every repeat goes
         * through two low-passes (COLR) and a low cut, and the loop saturates (tanh), so the echoes darken, thin out and
         * thicken as they pile up, and a high FDBK swells into self-oscillation instead of running away */
        {
            int32_t dm, pos, f, x0, x1, lp;
            uint32_t ri;
            {   /* free running: the time glides to the knob (a one-pole, ~90 ms), so sweeping TIME bends the echoes' pitch */
                int32_t d = dtarget - fx.dly_cur, step = d >> 12;
                if (!fx.dly_cur)
                    d = step = dtarget, fx.dly_cur = 0;
                else if (d > 0 && !step)
                    step = 1;
                step = step > 128 ? 128 : step < -128 ? -128 : step;   /* the pitch bends at most an octave down / half up */
                fx.dly_cur += step;
                dl = (uint32_t)fx.dly_cur >> 8;
            }
            dm = (int32_t)(dl >> 2) < 20 ? (int32_t)(dl >> 2) : 20;
            fx.dly_ph += 77900u;
            pos = (int32_t)(SIMPLE_DT(dl)) + ((osc_sine(fx.dly_ph) * dm) >> 7);
            ri = (uint32_t)pos >> 8;
            f = pos & 255;
            x0 = dly_buf[(fx.dly_w - ri) & (DLY_LEN - 1u)];
            x1 = dly_buf[(fx.dly_w - ri - 1u) & (DLY_LEN - 1u)];
            x = x0 + (((x1 - x0) * f) >> 8);
            fx.dly_lp += mulq15(x - fx.dly_lp, col);
            fx.dly_lp2 += mulq15(fx.dly_lp - fx.dly_lp2, col);
            fx.dly_hp += mulq15(fx.dly_lp2 - fx.dly_hp, hpk);
            lp = fx.dly_lp2 - fx.dly_hp;
            dly_buf[fx.dly_w & (DLY_LEN - 1u)] =
                (int16_t)clamp((dly_in[i] >> 1) + softclip(mulq15(lp, fb) * 5 >> 2), -32768, 32767);
            x = lp;
        }
        fx.dly_w++;
        y += mulq15(x << 1, dmix);
        wet[i] = y;
    }
    hl.side = 0;
    (void)rt;
    rev_quad(rev_in, wet, n);                           /* SIMPLE: the one reverb */
}

/* after the buses: HALL's stereo difference to the left and from the right of the dry mix (the wet bus adds the
 * same to both) */
static __attribute__((noinline)) void rev_side_mix(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        l[i] += rev_side[i];
        r[i] -= rev_side[i];
    }
}

/* one block of the whole mix (shared with hostsim.c): events -> each part (with its modulation matrix)
 * -> dist -> INSERT -> SLICER -> level / pan / sends -> buses -> master; out: stereo Q15 */
static void events_block(uint32_t n);                    /* seq.c */
#if FELUCCA_CPU2
static int32_t wet[CTL];                                 /* (the dry mix and the sends: mx[0], above) */
#else
static int32_t send_d[CTL], send_r[CTL], wet[CTL], mix_l[CTL], mix_r[CTL];
#endif

/* SPREAD (#148): a POLY or UNISON part whose SPRD is above 0. Its voices start left and right of PAN in turn (voice.c
 * voice_start; UNISON's detuned voices alternate), at PAN -/+ SPRD / 2 (SPRD 127: hard left and right), with the pan
 * law PAN has (the far side down, the near one as it is). track_render gives every voice (b, as always) and the left
 * ones (spread_buf); here d = left - right, the side signal, so for a pair of pans L = b (gla + glb) / 2 + d (gla - glb)
 * / 2 (R alike): the mono sum is b's, the level of a centre pan less by SPRD / 256 at most. b goes through DIST, the
 * SLICER and the FX layer's mute key as always; d gets the SLICER's gate and the mute's ramp (slicer_track_sd,
 * perf_mute_sd) and passes DIST by (the spread of a distorted chord is its voices' clean difference). The sends
 * take b, mono, as before. SPRD 0, MONO or LEGATO: mix_part's own loop, bit for bit as before. The cost: a block's
 * clear and sum of spread_buf, the side and a wider mix loop, per part; per voice none (it picks its buffer) */
static __attribute__((noinline)) void mix_spread(track_t *t, int32_t *b, uint32_t n SM_DECL)
{
    int32_t *d = SPREAD_BUF(t);
    uint32_t i;
    int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127], pan = t->p[P_PAN], s = (t->p[P_SPRD] + 1) >> 1;
    int32_t pa = clamp(pan - s, -64, 63), pb = clamp(pan + s, -64, 63);
    int32_t gla = 4096 - (pa > 0 ? pa * 64 : 0), gra = 4096 + (pa < 0 ? pa * 64 : 0);
    int32_t glb = 4096 - (pb > 0 ? pb * 64 : 0), grb = 4096 + (pb < 0 ? pb * 64 : 0);
    int32_t glm = (gla + glb) >> 1, grm = (gra + grb) >> 1, gld = (gla - glb) >> 1, grd = (gra - grb) >> 1;
    int32_t dl = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
    int32_t xmax = dl;
    xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);
    for (i = 0; i < n; i++)
        d[i] = (d[i] << 1) - b[i];                      /* left - right */
    for (i = 0; i < n; i++) {
        int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x, y = ((d[i] >> 2) * lvl) >> 10;
        int32_t xs = clamp(x, -xmax, xmax);
        if (a > pk)
            pk = a;
        if (dl)
            SM(d)[i] += mulq15(xs, dl);
        if (r)
            SM(v)[i] += mulq15(xs, r);
        SM(l)[i] += ((x * glm) >> 12) + ((y * gld) >> 12);
        SM(r)[i] += ((x * grm) >> 12) + ((y * grd) >> 12);
    }
    t->peak = pk;
}

/* a part's cost for the second core's split (mix_split, after Jangada's): its voices x its engine's cost per
 * voice (ENG_VCOST: the host's instructions per sample, tests/regress.c CPU at 8 notes held less the idle mix, over the
 * engine's voices), and the DIST, INSERT and sends while it sounds or its tail runs. ENGINES[] order; DIGITAL as FM6
 * when built, nothing when retired. An estimate: it only steers the split, the sound does not depend on it */
#if FELUCCA_CPU2
static uint16_t part_cost[NTRK];                        /* the last block's, per part */
static const uint16_t ENG_VCOST[NENGINES] = {181, 181};
#endif

/* one synth part into the dry mix and the sends of sm (mx[0]; mx[1] on the second core); a part with no voice sounding costs
 * the LFO tick and a cleared buffer only (after the DIST tail has run out) */
static void mix_part(track_t *t, uint32_t n SM_DECL)
{
    int32_t *b = SM(b);
    uint32_t i;
#if FELUCCA_CPU2
    uint32_t nr;
#endif
    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */
    T_SPREAD(t) = t->p[P_SPRD] > 0 && (trk_vmode(t) == V_POLY || trk_vmode(t) == V_UNISON);   /* (SPREAD, mix_spread) */
#if FELUCCA_CPU2
    nr = track_render(t, b, n);
    part_cost[(uint32_t)(t - trk) % NTRK] = (uint16_t)(nr * ENG_VCOST[t->engine % NENGINES] + (nr ? 60u : 0u));
    if (nr)
#else
    if (track_render(t, b, n))
#endif
        ;
    else {                                              /* no voice sounding: nothing to mix */
        if (MOD(t).on)
            mod_end(t);
        return;
    }
    if (T_SPREAD(t))
        mix_spread(t, b, n SM_ARG(sm));
    else {
    }
    if (!T_SPREAD(t)) {                                /* (the mix's values read after the calls: none is held over
                                                         * them, no spill in the loop: the ISR's cost as before 1.5) */
        int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127], pan = t->p[P_PAN];
        int32_t gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
        int32_t d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        int32_t xmax = d;
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        for (i = 0; i < n; i++) {
            int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (d)
                SM(d)[i] += mulq15(xs, d);
            if (r)
                SM(v)[i] += mulq15(xs, r);
            SM(l)[i] += (x * gl) >> 12;
            SM(r)[i] += (x * gr) >> 12;
        }
        t->peak = pk;
    }
    if (MOD(t).on)
        mod_end(t);                                     /* the stored values back */
}

/* the HOME oscilloscope (ui_graph.c graph_scope): every other sample of the left channel. MENU > DISPLAY > SCOPE (1.2,
 * Discussion #165): OUT (0, the default) what goes out, after MASTER (audio.c audio_block, before the click); MIX the
 * mix before MASTER (and before the FX layer, SPEAKER EQ and the limiter), at the level MASTER at its top gives, so its
 * size does not follow the volume. The click is in neither */
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
static volatile uint8_t scope_mix;
static __attribute__((noinline)) void scope_take_mix(uint32_t n)
{
    uint32_t i;
    for (i = 1; i < n; i += 2u)                         /* (((x >> 2) * MASTER_FULL) >> 10 is x) */
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(mix_l[i] + wet[i], -32767, 32767);
}

/* the master with the FX layer's effects between its level and master_out (perform.c) */
static __attribute__((noinline)) void perf_master(int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++) {
        mix_l[i] = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        mix_r[i] = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
    }
    perf_block(mix_l, mix_r, n);
    mfx_block(mix_l, mix_r, n);
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}

/* ---- the second core (hal/fm1_cpu1.h; after Jangada's, github.com/zednaked/jangada, GPL-3.0) renders some of
 * the parts of each block into its own sums (mx[1]) while this core renders the others; then this core adds them in
 * and goes on (the LFOs' blocks, the FX layer, the buses, the master) as with one core. The sums are integers, so the
 * order they add up in changes nothing: the output is bit for bit one core's. A part's render touches its own state
 * only (its track_t with its SPREAD, ENV SYNC and LFO phase, its voices, its matrix: mod.c MOD, its SPREAD buffer,
 * its engine's memory) and what nobody writes meanwhile (song, the tables); what parts share is done before the
 * split (events_block: the notes, voice stealing; the FX layer's perf_begin) or after it (the LFOs' S&H draw from the
 * shared generator, part by part: voice.c track_lfo_end). tests/dualcore_test.c checks it, under ThreadSanitizer too.
 * Without a second core (the host, the browser, MENU > 2ND CORE OFF, one that did not answer or stopped) C1_RUN takes
 * nothing and this core renders them all */
#if FELUCCA_CPU2
#ifndef C1_RUN
#define C1_ON() 0                                       /* a second core is answering */
#define C1_RUN(fn, arg) 0                               /* hand it fn(arg): 1 = taken */
#define C1_WAIT() 0                                     /* wait for it: 0 = done */
#endif
#ifndef C1_SPLIT
#define C1_SPLIT() mix_split()
#endif
static uint32_t c1_blocks;                              /* blocks the second core took parts of (console) */

static void c1_parts(uint32_t arg)                      /* arg: the parts (bit k = part k) | n << 8 */
{
    uint32_t i, n = arg >> 8;
    mixsum_t *sm = &mx[1];
    for (i = 0; i < n; i++)
        sm->l[i] = sm->r[i] = sm->d[i] = sm->v[i] = 0;
    for (i = 0; i < NPART; i++)
        if (arg >> i & 1u)
            mix_part(&trk[i], n, sm);
}

/* which parts go to the second core: the split of the last block's costs that leaves the busier core the least; none
 * when that saves less than handing them over costs (C1_HAND) */
#define C1_HAND 40u
static uint32_t mix_split(void)
{
    uint32_t m, i, best = 0, all = 0, lo;
    for (i = 0; i < NPART; i++)
        all += part_cost[i];
    lo = all;
    for (m = 1; m < (1u << NPART) - 1u; m++) {
        uint32_t a = 0, w;
        for (i = 0; i < NPART; i++)
            if (m >> i & 1u)
                a += part_cost[i];
        w = (a > all - a ? a : all - a) + C1_HAND;
        if (w < lo) {
            lo = w;
            best = m;
        }
    }
    return best;
}
#endif

static void mix_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    int perf;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++)
        send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
    events_block(n);
    perf = perf_begin(n);                               /* the FX hold layer at work (perform.c) */
#if FELUCCA_CPU2
    uint32_t m = C1_ON() ? C1_SPLIT() & ((1u << NPART) - 1u) : 0u;
    if (m && !C1_RUN(c1_parts, m | n << 8))
        m = 0;
    c1_blocks += m != 0;
    for (i = 0; i < NPART; i++)
        if (!(m >> i & 1u))
            mix_part(&trk[i], n, &mx[0]);
    if (m) {
        const mixsum_t *sm = &mx[1];
        if (C1_WAIT())                                  /* it stopped answering (held from now on): its parts here */
            c1_parts(m | n << 8);
        for (i = 0; i < n; i++) {
            mix_l[i] += sm->l[i];
            mix_r[i] += sm->r[i];
            send_d[i] += sm->d[i];
            send_r[i] += sm->v[i];
        }
    }
    for (i = 0; i < NPART; i++)                         /* the LFOs' blocks, part by part (voice.c) */
        track_lfo_end(&trk[i]);
#else
    for (i = 0; i < NPART; i++)                         /* (one core: as 1.6) */
        mix_part(&trk[i], n);
#endif
    mod_g_begin();                                      /* (SIMPLE: the LFOs on the master's: mod.c) */
    fx_buses(send_d, send_r, wet, n);
    if (hl.side)
        rev_side_mix(mix_l, mix_r, n);                  /* HALL: stereo */
    if (scope_mix)
        scope_take_mix(n);
    if (perf) {
        perf_master(out, n);
        mod_g_end();
        return;
    }
    for (i = 0; i < n; i++) {
        mix_l[i] = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        mix_r[i] = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
    }
    mfx_block(mix_l, mix_r, n);
    mod_g_end();
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
    return;
    for (i = 0; i < n; i++) {
        int32_t l = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        int32_t r = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
