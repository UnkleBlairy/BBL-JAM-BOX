/* SPDX-License-Identifier: GPL-3.0-only */
/* The two master effects of the groovebox (SIMPLE), on the master bus after the level and the FX layer:
 *   DJ    one knob: left of centre a resonant low-pass closing, right of centre a resonant high-pass opening, the
 *         middle no filter at all (song.g[G_MIDI], -64..63)
 *   COMB  a feedback comb filter with a damped loop: MIX, TUNE (the comb's pitch), FDBK, DAMP
 *         (song.g[G_SYNC], G_CRATE, G_CDEPTH, G_INFO: the ids of globals that were unused in this build)
 * The filter is perform.c's ZDF state-variable filter (PF_SVF's cutoffs) with its own resonance. Both fade in and out
 * (a share moving SL_SLOPE a sample), and cost nothing while they are off. */
#define MFX_CSLOPE 32                                   /* the comb's MIX glides: all of it in ~23 ms (a lock, a macro: no click) */
#define MFX_CB 1400u                                     /* the comb's longest line: 44100 / 32.7 Hz = 1349 samples */
static int16_t mfx_cb[2][MFX_CB] __attribute__((section(".pool")));
static struct {
    int32_t dj_a, idx;                                   /* the filter's share (Q15), the cutoff index now (Q8) */
    uint8_t hp;                                          /* its mode, changed only while it is out */
    int32_t z[4];                                        /* L ic1 ic2, R ic1 ic2 */
    int32_t cm, ci, cd, lp[2];                           /* the comb's share (Q15), its loop's input (Q15: fades in, so the first
                                                          * echo does not start mid-wave), its delay now (Q8), its loop's low-pass */
    uint32_t w;                                          /* the line's write place */
    uint8_t dirty;                                       /* the line holds something */
} mfx;
static uint16_t mfx_g[64];                               /* PF_SVF's g (Q14) per cutoff index, made at the first use */
static const uint16_t MFX_T2[13] = {32768, 30929, 29193, 27554, 26008, 24548, 23170, 21870, 20643, 19484, 18390, 17358, 16384};

/* a delay of the comb for TUNE t (0..127): 0.75 semitone a step up from 32.7 Hz, in 1/256 samples */
static int32_t mfx_delay(int32_t t)
{
    int32_t s = t * 12, semi = s >> 4, fr = s & 15, o = semi / 12, i = semi - o * 12;
    int32_t a = MFX_T2[i], b = MFX_T2[i + 1];
    int32_t d = (int32_t)(((int64_t)(1349 * 256) * (a + (((b - a) * fr) >> 4))) >> 15) >> o;
    return d < 4 * 256 ? 4 * 256 : d;
}

/* the loop's limiter: straight up to 16384, then bending smoothly towards 32767 (slope 1 at the knee) */
static int32_t mfx_soft(int32_t y)
{
    int32_t a = y < 0 ? -y : y, e;
    if (a <= 16384)
        return y;
    e = a - 16384;
    a = 16384 + (int32_t)(((int64_t)e * 16383) / (e + 16384));
    return y < 0 ? -a : a;
}

static void mfx_chunk(int32_t *bl, int32_t *br, uint32_t n)   /* (n <= MFX_CHUNK: the filter's coefficients once) */
{
    int32_t dj = song.g[G_MIDI], mix = song.g[G_SYNC];
    int32_t djt = (dj > 1 || dj < -1) ? 32768 : 0, ct = mix * 258;   /* (the comb: its wet share, gliding: mfx.cm) */
    int16_t c[3];
    int32_t k = 16384, td, ds = 0, gain, fbk, damp;
    uint32_t i;
    if (!djt && !mfx.dj_a && !ct && !mfx.cm)
        return;
    if (!mfx_g[0])
        for (i = 0; i < 64u; i++)
            mfx_g[i] = (uint16_t)(((uint32_t)PF_SVF[i][1] << 14) / (uint32_t)PF_SVF[i][0]);
    /* the filter: the mode while it is out, the cutoff glides, the coefficients for this block */
    if (!mfx.dj_a && djt)
        mfx.hp = dj > 0;
    if (mfx.hp != (dj > 0) && dj != 0 && mfx.dj_a)
        djt = 0;                                         /* across the middle: out, then the other way */
    {
        int32_t a = dj < 0 ? -dj : dj, tgt, fr, g, den, a1, a2, a3;
        a = a > 64 ? 64 : a;
        tgt = mfx.hp ? (4 << 8) + ((a * (50 - 4) * 256) >> 6) : (63 << 8) - ((a * (63 - 12) * 256) >> 6);
        if (!mfx.dj_a)
            mfx.idx = mfx.hp ? 4 << 8 : 63 << 8;
        if (!djt)
            tgt = mfx.idx;                               /* (fading out: the cutoff stays where it is) */
        mfx.idx += clamp(tgt - mfx.idx, -48, 48);       /* (a chunk of 32: 192 a 128-sample block, as before) */
        {                                                /* (the resonance follows the cutoff gliding, not the knob, finely) */
            int32_t num = mfx.hp ? mfx.idx - (4 << 8) : (63 << 8) - mfx.idx, den = mfx.hp ? (50 - 4) * 256 : (63 - 12) * 256;
            a = (clamp(num, 0, den) * 64 * 256) / den;   /* (Q8) */
        }
        i = (uint32_t)(mfx.idx >> 8);
        fr = mfx.idx & 255;
        g = mfx_g[i] + (((int32_t)mfx_g[i < 63u ? i + 1u : 63u] - mfx_g[i]) * fr >> 8);
        k = 16384 - ((a * (16384 - 5200)) >> 14);         /* the resonance: Q 1 in the middle, up to 3 at the ends (a: Q8) */
        den = 16384 + (int32_t)(((int64_t)g * (g + k)) >> 14);
        a1 = (int32_t)((1u << 28) / (uint32_t)den);
        a2 = (g * a1) >> 14;
        a3 = (g * a2) >> 14;
        c[0] = (int16_t)a1; c[1] = (int16_t)a2; c[2] = (int16_t)a3;
    }
    td = mfx_delay(song.g[G_CRATE]);
    fbk = song.g[G_CDEPTH] * 251;                        /* 0 .. 0.97 (Q15) */
    gain = 32768 - ((fbk * 22938) >> 15);                /* the wet's level: less as the ring grows */
    damp = 32768 - song.g[G_INFO] * 248;                 /* the loop's low-pass: 32768 open */
    (void)ds;
    for (i = 0; i < n; i++) {
        int32_t l = bl[i], r = br[i];
        mfx.dj_a += clamp(djt - mfx.dj_a, -MFX_CSLOPE, MFX_CSLOPE);   /* (~23 ms in / out: a lock, a macro, no click) */
        if (mfx.dj_a) {
            int32_t xl = clamp(l >> 3, -16383, 16383), xr = clamp(r >> 3, -16383, 16383), fl, fr2;
            int32_t *z = mfx.z, v3, v1, v2;
            v3 = xl - z[1]; v1 = (c[0] * z[0] + c[1] * v3) >> 14; v2 = z[1] + ((c[1] * z[0] + c[2] * v3) >> 14);
            z[0] = clamp(2 * v1 - z[0], -65535, 65535); z[1] = clamp(2 * v2 - z[1], -65535, 65535);
            fl = mfx.hp ? xl - ((k * v1) >> 14) - v2 : v2;
            z += 2;
            v3 = xr - z[1]; v1 = (c[0] * z[0] + c[1] * v3) >> 14; v2 = z[1] + ((c[1] * z[0] + c[2] * v3) >> 14);
            z[0] = clamp(2 * v1 - z[0], -65535, 65535); z[1] = clamp(2 * v2 - z[1], -65535, 65535);
            fr2 = mfx.hp ? xr - ((k * v1) >> 14) - v2 : v2;
            l = pf_mix(l, fl << 3, mfx.dj_a);
            r = pf_mix(r, fr2 << 3, mfx.dj_a);
        } else {
            mfx.z[0] = mfx.z[1] = mfx.z[2] = mfx.z[3] = 0;
        }
        mfx.cm += clamp(ct - mfx.cm, -MFX_CSLOPE, MFX_CSLOPE);
        mfx.ci += clamp((ct ? 32768 : 0) - mfx.ci, -MFX_CSLOPE, MFX_CSLOPE);
        mfx.cd += clamp(td - mfx.cd, -48, 48);
        if (mfx.cm) {
            int32_t ch, rd = (int32_t)(mfx.w << 8) - mfx.cd, x[2], wet[2], m = mfx.cm;
            uint32_t p0, p1, fq;
            if (rd < 0)                                  /* (round the line: a mask here read the wrong place, a click a lap) */
                rd += (int32_t)(MFX_CB << 8);
            p0 = (uint32_t)(rd >> 8) % MFX_CB;
            fq = (uint32_t)rd & 255u;
            p1 = (p0 + 1u) % MFX_CB;
            x[0] = clamp(l >> 2, -32767, 32767);
            x[1] = clamp(r >> 2, -32767, 32767);
            for (ch = 0; ch < 2; ch++) {
                int32_t d = mfx_cb[ch][p0] + (((int32_t)mfx_cb[ch][p1] - mfx_cb[ch][p0]) * (int32_t)fq >> 8), y;
                mfx.lp[ch] += (int32_t)(((int64_t)(d - mfx.lp[ch]) * damp) >> 15);
                int32_t fb = (int32_t)(((int64_t)mfx.lp[ch] * fbk) >> 15);
                y = mfx_soft(((x[ch] * mfx.ci) >> 15) + fb);   /* (a ringing loop rounds off: no hard clip) */
                mfx_cb[ch][mfx.w] = (int16_t)y;
                y = mfx.ci == 32768 ? y : mfx_soft(x[ch] + fb);
                wet[ch] = (int32_t)(((int64_t)y * gain) >> 15);
            }
            mfx.w = (mfx.w + 1u) % MFX_CB;
            mfx.dirty = 1;
            l = pf_mix(l, wet[0] << 2, m);
            r = pf_mix(r, wet[1] << 2, m);
        }
        bl[i] = l;
        br[i] = r;
    }
    if (!mfx.cm && !ct && mfx.dirty) {                 /* the comb is out: the line empty for the next time */
        for (i = 0; i < MFX_CB; i++)
            mfx_cb[0][i] = mfx_cb[1][i] = 0;
        mfx.lp[0] = mfx.lp[1] = 0;
        mfx.dirty = 0;
    }
}
#define MFX_CHUNK 32u
/* the master effects over a block, in chunks of 32 samples: the DJ filter's cutoff moves in small steps (no zipper) */
static __attribute__((noinline)) void mfx_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t o;
    for (o = 0; o < n; o += MFX_CHUNK)
        mfx_chunk(bl + o, br + o, n - o < MFX_CHUNK ? n - o : MFX_CHUNK);
}
