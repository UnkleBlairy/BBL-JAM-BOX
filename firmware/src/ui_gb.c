/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), BBL JAM BOX */
/* ui_gb.c - the groovebox look (FELUCCA_SIMPLE): black and one colour, big text, four circle-and-line dials a page. The
 * colour is the selected track's machine's (SYN BD SD HH CYM PERC: GB_COL, to be set from the app later); on the
 * MIXER each dial wears its own track's. One screen = the page's title and the BPM, four dials with their short
 * name over and the value under, the page dots and the machine's name. The PERF page is 16 tiles. The screen is first
 * described (gb_view_t), drawn from that, and only when the description changed. Pages: EDIT, FILTER, ENV, LFO, FX,
 * PERF, MIXER, CLOCK; the rest keep the old look. */
#define GB_STRIP 60                           /* canvas rows a strip */
#define GB_W 0xFFFFu                                  /* the ink: white */
#define GB_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static uint16_t gb_col[NTRK] = {GB_RGB(255, 90, 54), GB_RGB(255, 214, 10), GB_RGB(46, 196, 255), GB_RGB(179, 136, 255),
                                GB_RGB(64, 214, 120), GB_RGB(255, 112, 196)};   /* a colour a TRACK */

typedef struct {
    uint8_t perf, mixer, mach, sel, ring, page, npages, mask, fade, rpt, hn, sv, flash;   /* ring: the VOLUME dial of the selected track (4 none) */   /* SAVE / LOAD: 1 2; flash: 1 lit, 2 dark */
    char msg[2][16];
    int16_t bpm;
    uint8_t tm[NTRK];                         /* each track's machine */
    uint8_t mac_n[8], mac_held[8];
    int16_t ratio[4];
    uint8_t alt[4];
    int16_t dj, cmix, ctune;                 /* PERF: the master DJ filter and comb */
    uint8_t fill[8], rel, bpm_alt;           /* PERF: each held macro's tile filled 0..26 rows; the release fade; the tempo is held by a macro */                           /* a macro holds this value: its dial is drawn inverted */
    uint8_t kmode, pk, fx;                   /* the keys: 1 SEQ 2 PARAM 3 KEY 4 KIT; PARAM's knob + 1 (0: the keys play); the FX lane */
    uint8_t rec;                             /* the selected track is armed (REC): a red REC by the BPM, 1 + its glow 0..15 */
    uint8_t thit, tmute;                     /* the track squares (the bottom): bit k track k just hit, is muted */
    uint16_t brd;                            /* the loop border: how far round (1..960 px, the selected track's loop); 0 stopped */
    uint8_t icon, picon, hit, an;            /* the header's icon and the machine pill's (IC_*, engines.c); a drum just hit; an 808 */
    uint8_t vic[4];                          /* a dial's value as an icon (SAMPLER's MODE), 0 = its text */
    uint8_t wv, wa, wb;                      /* SAMPLER's waveform: 1 NO SAMPLE, 2 the chunk (window wa .. wb of GB_WAVE) */
    uint16_t wkey;                           /* .. which chunk (its peaks) */
    uint8_t zoom;                            /* START / LEN just turned (a lock too): the waveform zoomed on 1 START, 2 the end */
    uint32_t za, zb;                         /* .. the window's first and last sample */
    char title[12], lab[4][8], val[4][12];
} gb_view_t;
static gb_view_t gb_v, gb_last;
static uint8_t gb_have;

static int name_on(void);
static uint32_t gbs_held(void);                    /* (ui_input.c) the steps held */
static uint32_t gbsv_leds(void);                   /* (ui_input.c) the SAVE screen's keys */
static int gbs_used(uint32_t mach, uint32_t slot); /* (gb_store.c) the sounds and kits */
static int gbk_used(uint32_t k);
static int gbs_save_sound(uint32_t slot);
static int gbs_load_sound(uint32_t slot);
static int gbk_save(uint32_t k);
static int gbk_load(uint32_t k);
static void gb_store_poll(void);
static uint64_t lock_held(void);                   /* (ui_input.c) .. on a sound page: its knobs lock */
static int kit_shown(int16_t *vp, int16_t *cur);   /* (ui_input.c) LOCKS: a dial's value */
static const uint8_t *smp_peaks(uint32_t sl);      /* (gb_store.c) SAMPLER: a chunk's waveform */
static const uint8_t *smp_zpeaks(uint32_t sl, int32_t x0, uint32_t w);   /* .. zoomed */
/* the SEQ page (ui_input.c gbseq_knob) */
static const uint8_t GB_SPEED[5] = {0, 1, 2, 3, 10};
static uint32_t gb_speed(int32_t div)                    /* SPEED 0..4 of a DIV (another DIV: x1) */
{
    uint32_t i;
    for (i = 0; i < 5u; i++)
        if (GB_SPEED[i] == div)
            return i;
    return 2u;
}
static int32_t gb_own(const step_t *s)                   /* a step's own CHANCE, -1 none (the track's) */
{
    return s->probability && s->probability < COND_FIRST ? (int32_t)step_chance(s) : -1;
}
static uint32_t gb_trig(const step_t *s)                 /* a step's TRIG 0..4 (- 1/4 2/4 3/4 4/4) */
{
    uint32_t c = step_cond(s + GB_TRIG);
    return c > GB_TRIG_C0 && c <= GB_TRIG_C0 + 4u ? c - GB_TRIG_C0 : 0u;
}
static uint32_t gb_mach(uint32_t k) { return trk_mach(&trk[k % NTRK]); }

/* the page's header icon: the machine's on its sound pages, the filter's type, the LFO's wave, the effect's (0: none) */
static uint32_t gb_page_icon(const page_t *pg, uint32_t mach)
{
    const track_t *t = TSEL;
    if (pg->graph == GR_PERF)
        return IC_MACRO;
    if (str_eq(pg->title, "COMB"))
        return IC_COMB;
    if (str_eq(pg->title, "DJ"))
        return IC_DJ;
    if (str_eq(pg->title, "AGE"))                       /* (SAMPLER's GATE: the machine's) */
        return eng_idx(t->eng_req) == ENGI_SAMPLER ? MACHINES[mach % NMACH].icon : IC_AGE;
    if (str_eq(pg->title, "DELAY"))
        return IC_DELAY;
    if (str_eq(pg->title, "VERB"))
        return IC_VERB;
    if (pg->fam == FAM_FLT)
        return IC_LP + (uint32_t)t->p[P_FTYPE] % 3u;
    if (pg->fam == FAM_LFO)
        return IC_SINE + (uint32_t)t->p[P_LWAVE] % 5u;
    if (pg->fam == FAM_TRK)                             /* VOLUME, CLOCK: all the tracks', none */
        return IC_NONE;
    return MACHINES[mach % NMACH].icon;
}

/* SAMPLER's values as the prototype shows them: PITCH in semitones, STRETCH x, MODE an icon, SLOT S01, GATE steps.
 * -> the icon (IC_*) when the value is one, else 0 (b rewritten or kept) */
static uint32_t smp_format(uint32_t id, int32_t cur, char *b)
{
    char nb[8];
    uint32_t o = 0, q;
    if (id == P_E0) {                                 /* (cur - 64) * 24 / 64 = (cur - 64) * 3 / 8, one decimal */
        int32_t t10 = (cur - 64) * 30 / 8, a = t10 < 0 ? -t10 : t10;
        b[o++] = t10 < 0 ? '-' : '+';
        fmt_int(nb, a / 10);
        for (q = 0; nb[q]; q++) b[o++] = nb[q];
        b[o++] = '.';
        b[o++] = (char)('0' + a % 10);
        b[o] = 0;
    } else if (id == P_E1) {                          /* 4^((cur - 64) / 64), two decimals */
        uint32_t x = ((uint32_t)SMP_STRETCH[cur & 127] * 100u + 2048u) >> 12;
        fmt_int(nb, (int32_t)(x / 100u));
        for (q = 0; nb[q]; q++) b[o++] = nb[q];
        b[o++] = '.';
        b[o++] = (char)('0' + x / 10u % 10u);
        b[o++] = (char)('0' + x % 10u);
        b[o++] = 'x';
        b[o] = 0;
    } else if (id == P_E4) {
        b[0] = 0;
        return IC_ONCE + (uint32_t)(cur & 3);
    } else if (id == P_E5) {
        b[0] = 'S';
        b[1] = (char)('0' + (cur + 1) / 10);
        b[2] = (char)('0' + (cur + 1) % 10);
        b[3] = 0;
    } else if (id == P_LN2) {                         /* 0 the key's length, else 1/16 .. BAR as FADE shows them */
        uint32_t n = (uint32_t)cur, d = 16u;
        if (!n) {
            str_cpy(b, "KEY", 8);
        } else if (n >= 16u) {
            str_cpy(b, "BAR", 8);
        } else {
            while (!(n & 1u)) { n >>= 1; d >>= 1; }
            fmt_int(nb, (int32_t)n);
            for (q = 0; nb[q]; q++) b[o++] = nb[q];
            b[o++] = '/';
            fmt_int(nb, (int32_t)d);
            for (q = 0; nb[q]; q++) b[o++] = nb[q];
            b[o] = 0;
        }
    }
    return 0;
}

static void gb_fill(gb_view_t *v)
{
    const page_t *pg = cur_page();
    uint32_t k, i, n = 0, at = 0, smp;
    uint64_t lk;
    char *p = (char *)v;
    for (i = 0; i < sizeof *v; i++)
        p[i] = 0;
    v->perf = pg->graph == GR_PERF;
    v->mixer = pg->graph == GR_TRK;
    v->sel = (uint8_t)song.sel;
    v->bpm = song.g[G_BPM];
    v->bpm_alt = (uint8_t)(pm_altered(TSEL, &song.g[G_BPM]) != 0);
    if ((song.rec >> song.sel) & 1u) {           /* REC: playing, it breathes twice a bar (one breath = 2 beats) */
        uint32_t bs = beat_samples(), ph = ((beat_n & 1u) * bs + (beat_pos < bs ? beat_pos : bs)) * 32u / (2u * bs + 1u);
        v->rec = (uint8_t)(1u + (song.playing ? (ph < 16u ? 15u - ph : ph - 16u) : 15u));
    }
    str_cpy(v->title, pg->title, sizeof v->title);
    v->kmode = (uint8_t)(ui.sgrid == 1u ? 1u : ui.sgrid == 2u ? 2u : ui.sgrid == 3u ? 4u : 3u);
    v->pk = (uint8_t)(par_on() && par_id != 0xFFu ? par_k + 1u : 0u);
    v->fx = (uint8_t)fxl_on();
    for (k = 0; k < NTRK; k++)
        v->tm[k] = (uint8_t)gb_mach(k);
    v->mach = v->tm[song.sel % NTRK];
    v->picon = v->fx ? IC_NONE : MACHINES[v->mach % NMACH].icon;
    v->an = (uint8_t)(!v->fx && MACHINES[v->mach % NMACH].engine == ENGI_ANALOG);
    v->icon = (uint8_t)gb_page_icon(pg, v->mach);
    for (k = 0; k < NTRK; k++) {                  /* the track squares: a hit (~90 ms), a mute */
        uint32_t h = trk_hit_ms[k];
        if (h && fm1_ms - h < 90u)
            v->thit |= (uint8_t)(1u << k);
        if (trk[k].p[P_MUTE])
            v->tmute |= (uint8_t)(1u << k);
    }
    if (song.playing) {                           /* the loop border: the selected track's place in its loop */
        const track_t *t = &trk[song.sel % NTRK];
        uint32_t len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u, idx = t->seq_idx % len;
        uint32_t cl = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), idx), fs = 0;
        if (t->seq_pos < cl)
            fs = t->seq_pos / (cl / 256u + 1u);
        v->brd = (uint16_t)(1u + (idx * 256u + fs) * 960u / (len * 256u));
    }
    {                                             /* a drum's hit: the header icon flashes ~90 ms */
        uint32_t h = trk_hit_ms[song.sel % NTRK];
        v->hit = (uint8_t)(MACHINES[v->mach % NMACH].drum && h && fm1_ms - h < 90u);
    }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            if (i == ui.page)
                at = n;
            n++;
        }
    v->page = (uint8_t)at;
    v->npages = (uint8_t)n;
    if (gbsv || gbsv_t) {                         /* SAVE? / LOAD?, or what was saved / loaded, flashing */
        v->sv = gbsv ? gbsv : 1u;
        if (gbsv_t) {
            v->flash = (uint8_t)(((fm1_ms - gbsv_t) / 150u) & 1u ? 2u : 1u);
            memcpy(v->msg, gbsv_msg, sizeof v->msg);
        }
        return;
    }
    if (v->perf) {
        for (i = 0; i < PM_NM; i++) {
            uint32_t j;
            v->mac_n[i] = (uint8_t)pm_mac[i].n;
            for (j = 0; j < pm_h.n; j++)
                if (pm_h.mac[j] == i)
                    v->mac_held[i] = 1;
        }
        v->fade = (uint8_t)pm_fade;
        v->hn = (uint8_t)pm_h.n;
        v->rel = pm_rel;
        v->dj = song.g[G_MIDI];
        v->cmix = song.g[G_SYNC];
        v->ctune = song.g[G_CRATE];
        if (pm_h.n && pm_ui_c < PM_NM) {
            uint32_t f = (uint32_t)pm_ui_u * 26u / 1024u;
            if (f > 26u)
                f = 26u;
            if (pm_h.n == 2u && pm_ui_a < PM_NM)
                v->fill[pm_ui_a] = (uint8_t)(26u - f);
            v->fill[pm_ui_c] = (uint8_t)f;
        }
        v->rpt = (uint8_t)(perf_held & 15u);
        return;
    }
    if (v->mixer) {                           /* the page's tracks' levels (VOLUME 1-4, 5-6) */
        v->ring = 4;
        for (k = 0; k < 4u; k++) {
            uint32_t tk = pg->id[k];
            const char *nm;
            const track_t *t;
            if (tk >= NTRK)
                continue;
            nm = MACHINES[v->tm[tk] % NMACH].pill;
            t = &trk[tk];
            if (tk == song.sel)
                v->ring = (uint8_t)k;
            const param_desc_t *d = &TP[P_LEVEL];
            const char *u;
            for (i = 0; i < 4u && nm[i]; i++)
                v->lab[k][i] = nm[i];
            if (t->p[P_MUTE])
                str_cpy(v->val[k], "MUTE", sizeof v->val[k]);
            else
                param_format(d, t->p[P_LEVEL], v->val[k], &u);
            v->ratio[k] = (int16_t)(RATIO(d, t->p[P_LEVEL]) < 0 ? 0 : RATIO(d, t->p[P_LEVEL]));
            v->mask |= (uint8_t)(1u << k);
        }
        return;
    }
    if (pg->graph == GR_GBSEQ) {                  /* LEN SPEED CHANCE TRIG; a step held: STEP n, its own CHANCE and TRIG */
        static const char *const SPN[5] = {"x1/4", "x1/2", "x1", "x2", "x4"};
        static const char *const TRN[5] = {"-", "1/4", "2/4", "3/4", "4/4"};
        static const char *const LB[4] = {"LEN", "SPEED", "CHANCE", "TRIG"};
        const track_t *t = TSEL;
        uint32_t held = gbs_held(), p = 0, sp = gb_speed(t->p[P_SDIV]);
        int32_t ch = t->p[P_APROB] > 100 ? 100 : t->p[P_APROB], tr = -1;
        while (held && !((held >> p) & 1u))
            p++;
        if (held) {
            char nb[4];
            fmt_int(nb, (int32_t)p + 1);
            str_cpy(v->title, "STEP ", sizeof v->title);
            str_cpy(v->title + 5, nb, sizeof v->title - 5u);
            ch = gb_own(&t->step[p]);
            tr = (int32_t)gb_trig(&t->step[p]);
        }
        for (k = 0; k < 4u; k++) {
            str_cpy(v->lab[k], LB[k], sizeof v->lab[k]);
            v->mask |= (uint8_t)(1u << k);
        }
        fmt_int(v->val[0], t->p[P_SLEN]);
        v->ratio[0] = (int16_t)((t->p[P_SLEN] - 1) * 1000 / 15);
        str_cpy(v->val[1], SPN[sp], sizeof v->val[1]);
        v->ratio[1] = (int16_t)(sp * 250u);
        if (ch < 0) {
            str_cpy(v->val[2], "TRK", sizeof v->val[2]);
            v->ratio[2] = 0;
        } else {
            fmt_int(v->val[2], ch);
            for (i = 0; v->val[2][i]; i++)
                ;
            v->val[2][i] = '%';
            v->val[2][i + 1u] = 0;
            v->ratio[2] = (int16_t)(ch * 10);
        }
        str_cpy(v->val[3], TRN[tr < 0 ? 0 : tr], sizeof v->val[3]);
        v->ratio[3] = (int16_t)((tr < 0 ? 0 : tr) * 250);
        return;
    }
    lk = lock_held();
    smp = !v->fx && eng_idx(TSEL->eng_req) == ENGI_SAMPLER && pg->scope == SC_ENGINE;
    if (smp && str_eq(pg->title, "AGE"))
        str_cpy(v->title, "GATE", sizeof v->title);
    else if (smp) {                                   /* the chunk's waveform, START .. START + LEN bright */
        const track_t *t = TSEL;
        int16_t st = t->p[P_E2], ln = t->p[P_E3];
        uint32_t sl = (uint32_t)t->p[P_E5], a, b, s0 = 0;
        if (lk) {                                     /* a step held: its START / LEN locks */
            while (!((lk >> s0) & 1u))
                s0++;
            (void)motion_lock_get(t, s0, P_E2, &st);
            (void)motion_lock_get(t, s0, P_E3, &ln);
        }
        a = (uint32_t)(st & 127) * (GB_WAVE - 1u) / 127u;
        b = a + (uint32_t)(ln & 127) * (GB_WAVE - 1u) / 127u;
        if (sl < smp_n && ui.hot_t && pg->id[ui.hot_col & 3u] == P_E2 + (ui.hot_col == 3u)
            && (ui.hot_col == 2u || ui.hot_col == 3u)) {
            uint32_t n = smp_tab[sl].len;             /* the window in samples (eng_sampler.c sampler_note_on) */
            v->zoom = (uint8_t)(ui.hot_col - 1u);
            v->za = (uint32_t)(st & 127) * (n - 1u) / 127u;
            v->zb = v->za + (uint32_t)(ln & 127) * (n - 1u) / 127u;
            v->zb = v->zb > n - 1u ? n - 1u : v->zb;
        }
        v->wv = smp_peaks(sl) ? 2u : 1u;
        v->wa = (uint8_t)a;
        v->wb = (uint8_t)(b > GB_WAVE - 1u ? GB_WAVE - 1u : b);
        v->wkey = (uint16_t)(sl | smp_gen << 6);
    }
    for (k = 0; k < 4u; k++) {
        int16_t *vp, cur;
        const param_desc_t *d = page_desc(pg, k, &vp);
        const char *u;
        int32_t r;
        if (!d || !d->label || d->label[0] == '-')
            continue;
        for (i = 0; i < 7u && d->label[i]; i++)
            v->lab[k][i] = d->label[i];
        cur = *vp;
        v->alt[k] = (uint8_t)(pm_altered(TSEL, vp) != 0);
        if (lk && v->fx) {                            /* the FX lane's step held: its lock of a master knob (inverted) */
            uint32_t g = vp >= song.g && vp < song.g + G_COUNT ? motion_gid((uint32_t)(vp - song.g)) : 0xFFu, st = 0;
            while (!((lk >> st) & 1u))
                st++;
            v->alt[k] = 0;
            if (g != 0xFFu && ((fxl.lk[st] >> (g - P_COUNT)) & 1u)) {
                cur = fxl.v[st][g - P_COUNT];
                v->alt[k] = 1;
            }
        } else if (lk && ((vp >= TSEL->p && vp < TSEL->p + P_COUNT) ||
                   (vp >= song.g && vp < song.g + G_COUNT && motion_gid((uint32_t)(vp - song.g)) != 0xFFu))) {
            /* a step held: its lock (inverted), else the sound's own */
            uint32_t id = vp >= TSEL->p && vp < TSEL->p + P_COUNT ? (uint32_t)(vp - TSEL->p) : motion_gid((uint32_t)(vp - song.g)), st = 0;
            int16_t lv;
            while (!((lk >> st) & 1u))
                st++;
            v->alt[k] = 0;
            if (motion_lock_get(TSEL, st, id, &lv)) {
                cur = lv;
                v->alt[k] = 1;
            } else if (motion_param(id)) {
                cur = motion_base_value(TSEL, id);
            }
        } else if (FELUCCA_SIMPLE && !v->fx && kit_on()) {   /* LOCKS: a key held, its lock (inverted); else the sound's own */
            int16_t kv = (int16_t)cur;
            if (kit_shown(vp, &kv))
                v->alt[k] = 1;
            cur = kv;
        }
        param_format(d, cur, v->val[k], &u);
        if (smp && vp >= TSEL->p && vp < TSEL->p + P_COUNT)
            v->vic[k] = (uint8_t)smp_format((uint32_t)(vp - TSEL->p), cur, v->val[k]);
        if (vp == &song.g[G_RSIZE] && cur >= 127)       /* the reverb's TIME at the top: FREEZE (fx.c rev_quad) */
            str_cpy(v->val[k], "FREEZE", sizeof v->val[k]);
        if (str_eq(v->val[k], "CYM"))                 /* (too wide beside a negative value) */
            str_cpy(v->val[k], "CY", sizeof v->val[k]);
        r = RATIO(d, enum_rank(d, cur));
        v->ratio[k] = (int16_t)(r < 0 ? 0 : r);
        v->mask |= (uint8_t)(1u << k);
    }
    if (str_eq(pg->title, "CLOCK")) {                 /* the audio's CPU load (read only): the room the sounds have */
        uint32_t c = song.cpu_q8 * 100u / 256u;
        c = c > 100u ? 100u : c;
        str_cpy(v->lab[3], "CPU", sizeof v->lab[3]);
        fmt_int(v->val[3], (int32_t)c);
        for (i = 0; v->val[3][i]; i++)
            ;
        v->val[3][i] = '%';
        v->val[3][i + 1u] = 0;
        v->ratio[3] = (int16_t)(c * 10u);
        v->mask |= 8u;
    }
}

/* an oval (a pill) made of rows: on = filled, else a 2 px outline. (cv_rrect rounds at most 8 px; these are up to 17) */
static uint16_t gb_bg;                                      /* the screen's background (the track's colour) */
static uint32_t gb_isqrt(uint32_t n)
{
    uint32_t r = 0;
    while ((r + 1u) * (r + 1u) <= n)
        r++;
    return r;
}
static int32_t gb_ymin = -32768;                            /* gb_oval: only the rows from here down (a tile filling up) */
static void gb_oval(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t col)
{
    int32_t r = h / 2, j;
    for (j = 0; j < h; j++) {
        if (y + j < gb_ymin)
            continue;
        int32_t d2 = 2 * j + 1 - h;                              /* the row's centre from the middle, in half pixels */
        int32_t dx = (int32_t)gb_isqrt((uint32_t)(r * r * 4 - d2 * d2) + 0u) ;
        int32_t in = r - (dx + 1) / 2;
        cv_rect(x + in, y + j, w - 2 * in, 1, col);
    }
}
static void gb_pill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, int on)
{
    gb_oval(x, y, w, h, c);
    if (!on)
        gb_oval(x + 2, y + 2, w - 4, h - 4, gb_bg);
}

static void gb_clip(int32_t y0, int32_t y1)                  /* text only in the screen rows y0 .. y1 - 1 */
{
    int32_t a = y0 + cv_oy, b = y1 + cv_oy;
    cv_cy0 = (int16_t)(a < 0 ? 0 : a);
    cv_cy1 = (int16_t)(b > GB_STRIP ? GB_STRIP : b < a ? a : b);
}
static void gb_unclip(void) { cv_cy0 = 0; cv_cy1 = GB_STRIP; }

/* a 16 x 16 icon (IC_*) at x, y, each pixel s x s, in col: a run of lit pixels a rectangle (cv_rect keeps to the strip) */
static void gb_icon(int32_t x, int32_t y, int32_t s, uint32_t icon, uint16_t col)
{
    const uint16_t *ic = icon < IC_COUNT ? GB_ICONS[icon] : 0;
    int32_t j, i, r;
    if (!ic)
        return;
    for (j = 0; j < 16; j++) {
        if (y + (j + 1) * s + cv_oy <= 0 || y + j * s + cv_oy >= GB_STRIP)
            continue;
        for (i = 0; i < 16; i = r) {
            for (r = i; r < 16 && ((ic[j] << r) & 0x8000u); r++)
                ;
            if (r > i)
                cv_rect(x + i * s, y + j * s, (r - i) * s, s, col);
            else
                r++;
        }
    }
}

/* the header: the page's icon at 2x (inverted while a drum hits), the title beside it and the page's n/N, the BPM at
 * the right. The title loses the n/N, then steps down a size, so it never runs into the BPM */
static void gb_header(const gb_view_t *v, const char *title, uint16_t bgc)
{
    char bb[8], tg[8];
    int32_t tx = v->icon ? 48 : 8, bw, room, tw, gw = 0;
    const aafont_t *tf = &AF_L;
    if (v->icon) {
        if (v->hit) {
            cv_rrect(4, 4, 40, 36, 6, GB_W, bgc);
            gb_icon(8, 6, 2, v->icon, bgc);
        } else
            gb_icon(8, 6, 2, v->icon, GB_W);
    }
    fmt_int(bb, v->bpm);
    bw = text_w(&AF_L, bb) + (v->bpm_alt ? 12 : 0);              /* (the title's size) */
    if (v->bpm_alt) {                                             /* the tempo held by a macro: inverted */
        cv_rrect(232 - bw + 6, 4, bw, 34, 6, GB_W, bgc);
        cv_text_r(232, 6, &AF_L, bb, bgc, GB_W);
    } else
        cv_text_r(232, 6, &AF_L, bb, GB_W, bgc);
    if (v->rec) {                                                 /* armed: a red REC left of the BPM */
        int32_t rw = text_w(&AF_S, "REC") + 12, rx = 232 - bw - 6 - rw;
        cv_rrect(rx - 1, 9, rw + 2, 24, 7, GB_W, bgc);
        uint16_t rc = (uint16_t)((8u + 23u * (v->rec - 1u) / 15u) << 11);   /* dark red .. red */
        cv_rrect(rx, 10, rw, 22, 6, rc, GB_W);
        cv_text_on(rx + 6, 15, &AF_S, "REC", GB_W, rc);
        bw += rw + 6;
    }
    room = 232 - bw - 8 - tx;
    tg[0] = 0;
    if (v->npages > 1u) {
        tg[0] = (char)('1' + v->page);
        tg[1] = '/';
        tg[2] = (char)('0' + v->npages);
        tg[3] = 0;
        gw = text_w(&AF_S, tg) + 8;
    }
    if (!large_face_has(title) || text_w(tf, title) > room)    /* big with its n/N, else big alone, else smaller */
        tf = &AF_M;
    tw = text_w(tf, title);
    if (tw + gw > room)
        tg[0] = 0;
    cv_text_on(tx, tf == &AF_L ? 6 : 13, tf, title, GB_W, bgc);
    if (tg[0])
        cv_text_on(tx + tw + 8, tf == &AF_L ? 20 : 16, &AF_S, tg, GB_W, bgc);
}

/* white mixed into the background: a (0..255) of white */
static uint16_t gb_mix(uint16_t bg, uint32_t a)
{
    uint32_t r = bg >> 11, g = (bg >> 5) & 63u, b = bg & 31u;
    r += ((31u - r) * a) >> 8;
    g += ((63u - g) * a) >> 8;
    b += ((31u - b) * a) >> 8;
    return (uint16_t)(r << 11 | g << 5 | b);
}

static void gb_scene(const gb_view_t *v, uint16_t bgc)
{
    const uint16_t c = GB_W;                      /* all ink white, the track's colour behind it */
    uint32_t k;
    cv_bg = bgc;
    gb_bg = bgc;
    if (v->sv) {                                              /* SAVE? / LOAD?: what the keys do; done: it flashes */
        if (v->flash) {
            if (v->flash == 1u) {
                cv_text_in(0, 80, 240, &AF_L, v->msg[0], c, bgc);
                cv_text_in(0, 124, 240, &AF_L, v->msg[1], c, bgc);
            }
            return;
        }
        cv_text_on(8, 4, &AF_L, v->sv == 1u ? "SAVE?" : "LOAD?", c, bgc);
        cv_text_r(232, 14, &AF_M, MACHINES[v->mach % NMACH].pill, c, bgc);
        cv_text_on(8, 84, &AF_L, "SOUND", c, bgc);
        cv_text_on(8, 116, &AF_M, "WHITE KEY", c, bgc);
        cv_text_on(8, 156, &AF_L, "KIT", c, bgc);
        cv_text_on(8, 188, &AF_M, "BLACK KEY", c, bgc);
        return;
    }
    if (v->perf) {
        static const char *const FN[4] = {"1/16", "1/8", "BAR", "4BAR"};
        static const char *const RN[4] = {"1/8", "1/16", "1/32", "REV"};
        char fb[24];
        uint32_t o = 0;
        gb_header(v, v->hn > 2u ? "ARP" : "PERF", bgc);
        for (k = 0; k < 8u; k++) {
            int32_t tx = 8 + (int32_t)(k & 3u) * 56, ty = 42 + (int32_t)(k >> 2) * 37;
            int on = v->mac_held[k];
            char nb[4];
            nb[0] = 'M'; nb[1] = (char)('1' + k); nb[2] = 0;
            if (on) {                                         /* held: a bold outline, the inside fills as its cycle runs */
                int32_t lv = ty + 30 - (int32_t)v->fill[k];
                gb_oval(tx, ty, 52, 34, GB_W);
                gb_oval(tx + 4, ty + 4, 44, 26, bgc);
                gb_ymin = lv;
                gb_oval(tx + 4, ty + 4, 44, 26, GB_W);
                gb_ymin = -32768;
                gb_clip(ty, lv);
                cv_text_in(tx, ty + 7, 52, &AF_M, nb, GB_W, bgc);
                gb_clip(lv, ty + 34);
                cv_text_in(tx, ty + 7, 52, &AF_M, nb, bgc, GB_W);
                gb_unclip();
            } else {
                gb_pill(tx, ty, 52, 34, GB_W, 0);
                cv_text_in(tx, ty + 7, 52, &AF_M, nb, GB_W, bgc);
                if (v->mac_n[k])
                    cv_rect(tx + 24, ty + 27, 4, 3, GB_W);
            }
        }
        for (k = 0; k < 4u; k++) {
            int32_t tx = 8 + (int32_t)k * 56;
            int on = v->fade == k + 1u, rp = (v->rpt >> k) & 1u;
            gb_pill(tx, 117, 52, 26, GB_W, on);
            cv_text_in(tx, 122, 52, &AF_M, FN[k], on ? bgc : GB_W, on ? GB_W : bgc);
            gb_pill(tx, 147, 52, 26, GB_W, rp);
            if (k == 3u)                                      /* REV: its icon */
                gb_icon(tx + 18, 152, 1, IC_REVERSE, rp ? bgc : GB_W);
            else
                cv_text_in(tx, 152, 52, &AF_M, RN[k], rp ? bgc : GB_W, rp ? GB_W : bgc);
        }
        {                                                 /* the four knobs: FADE  FILT / COMB  TUNE */
            char nn[8];
            uint32_t q;
            if (!v->rel) {
                str_cpy(fb, "FADE -", sizeof fb);
            } else if (v->rel >= 16u) {
                str_cpy(fb, "FADE BAR", sizeof fb);
            } else {                                          /* the fraction of a bar, reduced: 1/16 1/8 3/16 1/4 .. */
                uint32_t n = v->rel, d = 16u;
                while (!(n & 1u)) { n >>= 1; d >>= 1; }
                str_cpy(fb, "FADE ", sizeof fb);
                o = 5u;
                fmt_int(nn, (int32_t)n);
                for (q = 0; nn[q]; q++) fb[o++] = nn[q];
                fb[o++] = '/';
                fmt_int(nn, (int32_t)d);
                for (q = 0; nn[q]; q++) fb[o++] = nn[q];
                fb[o] = 0;
            }
            cv_text_on(8, 176, &AF_M, fb, c, bgc);
            if (v->dj > 1 || v->dj < -1) {                     /* the DJ filter: LP to the left, HP to the right, how far */
                int32_t a2 = v->dj < 0 ? -v->dj : v->dj;
                fb[0] = v->dj < 0 ? 'L' : 'H'; fb[1] = 'P'; fb[2] = ' ';
                fmt_int(nn, a2 * 100 / 64);
                for (q = 0; nn[q]; q++) fb[3u + q] = nn[q];
                fb[3u + q] = 0;
            } else {
                str_cpy(fb, "FILT -", sizeof fb);
            }
            cv_text_r(232, 176, &AF_M, fb, c, bgc);
            str_cpy(fb, "COMB ", sizeof fb);
            fmt_int(nn, v->cmix * 100 / 127);
            for (q = 0; nn[q]; q++) fb[5u + q] = nn[q];
            fb[5u + q] = 0;
            cv_text_on(8, 197, &AF_M, fb, c, bgc);
            str_cpy(fb, "TUNE ", sizeof fb);
            fmt_int(nn, v->ctune * 100 / 127);
            for (q = 0; nn[q]; q++) fb[5u + q] = nn[q];
            fb[5u + q] = 0;
            cv_text_r(232, 197, &AF_M, fb, c, bgc);
        }
        return;
    }
    gb_header(v, v->title, bgc);
    for (k = 0; k < 4u; k++) {                                    /* the dials */
        int32_t cx = 8 + (int32_t)k * 56 + 28, cy = 108, a, px, py, on;
        uint16_t dc = GB_W, lc = GB_W, bk = bgc;
        const aafont_t *vf = &AF_L;
        if (v->zoom)                                              /* (the zoomed waveform in their place) */
            break;
        if (!(v->mask & (1u << k)))
            continue;
        if (v->alt[k]) {                                          /* held by a macro: a white block, the ink in the track's colour */
            cv_rrect(cx - 27, 55, 54, 121, 6, GB_W, bgc);
            dc = lc = bgc;
            bk = GB_W;
        }
        cv_text_in(cx - 28, 60, 56, &AF_M, v->lab[k], lc, bk);
        for (a = 0; a < 360; a++) {
            dial_pt(cx, cy, 22, a, &px, &py);
            cv_rect(px - 1, py - 1, 3, 3, dc);
        }
        if (v->mixer && k == v->ring)                             /* the selected track: a second ring */
            for (a = 0; a < 360; a += 2) {
                dial_pt(cx, cy, 28, a, &px, &py);
                cv_rect(px, py, 2, 2, dc);
            }
        on = v->ratio[k] * 270 / 1000;
        dial_pt(cx, cy, 17, 225 + on, &px, &py);
        cv_line_t(cx, cy, px, py, dc, 3);
        if (text_w(vf, v->val[k]) > 54 || !large_face_has(v->val[k]))
            vf = &AF_M;
        if (text_w(vf, v->val[k]) > 54)
            vf = &AF_S;
        if (v->vic[k])                                            /* (SAMPLER's MODE: its icon) */
            gb_icon(cx - 16, 140, 2, v->vic[k], dc);
        else
            cv_text_in(cx - 27, vf == &AF_L ? 142 : 150, 54, vf, v->val[k], dc, bk);
        if (v->pk == k + 1u)                                      /* PARAM: the knob the white keys set, underlined */
            cv_rrect(cx - 18, 182, 36, 5, 2, GB_W, bgc);
    }
    if (v->wv == 1u)                                              /* SAMPLER: the chunk's waveform, or none */
        cv_text_in(0, 172, 240, &AF_M, "NO SAMPLE", c, bgc);
    else if (v->wv == 2u && v->zoom) {                           /* zoomed: ~1/6 of the chunk round START or the end */
        uint32_t sl = v->wkey & 63u, n = sl < smp_n ? smp_tab[sl].len : 0u, w = n / 6u, f = v->zoom == 1u ? v->za : v->zb;
        int32_t x, y0 = 64, hh = 104, x0, ma, mb, wa;
        const uint8_t *pk;
        uint16_t dim = gb_mix(bgc, 90);
        char zb[20];
        w = w < GB_WAVE ? GB_WAVE : w;
        x0 = (int32_t)f - (int32_t)(w / 2u);
        pk = smp_zpeaks(sl, x0, w);
        ma = ((int32_t)v->za - x0) * (int32_t)GB_WAVE / (int32_t)w;   /* START, the end: x (< 2^26: 32 bits) */
        mb = ((int32_t)v->zb - x0) * (int32_t)GB_WAVE / (int32_t)w;
        for (x = 0; pk && x < (int32_t)GB_WAVE; x++) {
            int32_t h2 = (int32_t)pk[x] * hh / 512;
            h2 = h2 < 1 ? 1 : h2;
            cv_rect(8 + x, y0 + hh / 2 - h2, 1, h2 * 2, x >= ma && x <= mb ? c : dim);
        }
        for (wa = 0; wa < 2; wa++) {                              /* the brackets: START [ and the end ] (the one turned: bold) */
            int32_t mx = wa ? mb : ma, dx = wa ? -4 : 1, bw = wa + 1 == v->zoom ? 3 : 1;
            if (mx < 0 || mx >= (int32_t)GB_WAVE)
                continue;
            cv_rect(8 + mx - bw / 2, y0 - 4, bw, hh + 8, c);
            cv_rect(8 + mx + dx, y0 - 4, 4, 2, c);
            cv_rect(8 + mx + dx, y0 + hh + 2, 4, 2, c);
        }
        str_cpy(zb, v->lab[v->zoom == 1u ? 2 : 3], sizeof zb);
        {
            uint32_t q = 0;
            while (zb[q])
                q++;
            zb[q++] = ' ';
            str_cpy(zb + q, v->val[v->zoom == 1u ? 2 : 3], sizeof zb - q);
        }
        cv_text_in(0, 178, 240, &AF_M, zb, c, bgc);
    } else if (v->wv == 2u) {
        const uint8_t *pk = smp_peaks(v->wkey & 63u);
        uint16_t dim = gb_mix(bgc, 90);                           /* outside the window: the ink at ~35 % */
        int32_t x, y0 = 170, hh = 20;
        for (x = 0; pk && x < (int32_t)GB_WAVE; x++) {
            int32_t h2 = (int32_t)pk[x] * hh / 512;
            h2 = h2 < 1 ? 1 : h2;
            cv_rect(8 + x, y0 + hh / 2 - h2, 1, h2 * 2, x >= v->wa && x <= v->wb ? c : dim);
        }
        cv_rect(8 + v->wa, y0 - 2, 1, hh + 4, c);                 /* the window's ends */
        cv_rect(8 + v->wb, y0 - 2, 1, hh + 4, c);
        cv_rect(8 + v->wa, y0 - 2, 3, 1, c);
        cv_rect(8 + v->wb - 2, y0 - 2, 3, 1, c);
    }
    const char *nm = v->fx ? "FX" : MACHINES[v->mach % NMACH].pill;
    int32_t pw = (text_w(&AF_M, nm) + 17 + (v->picon ? 20 : 0)) & ~1;
    {                                                             /* what the keys are: left of the pill */
        static const char *const KM[5] = {"", "SEQ", "PARAM", "KEY", "LOCKS"};
        const char *km = KM[v->kmode % 5u];
        int32_t x0 = 8, x1 = 232 - pw - 4;
        const aafont_t *kf = text_w(&AF_M, km) <= x1 - x0 ? &AF_M : &AF_S;
        cv_text_on(x0, kf == &AF_M ? 197 : 199, kf, km, c, bgc);
    }
    gb_pill(232 - pw, 194, pw, 24, c, v->an);                      /* the machine, bottom right: an oval (an 808: filled), */
    if (v->picon)                                                 /* its icon, its name */
        gb_icon(232 - pw + 8, 198, 1, v->picon, v->an ? bgc : c);
    cv_text_on(232 - pw + 8 + (v->picon ? 20 : 0), 197, &AF_M, nm, v->an ? bgc : c, v->an ? c : bgc);
}

/* the overlay, over every page: the loop border (3 px round the edge, clockwise from the top left, white as far as the
 * loop has got) and the six track squares at the bottom (the track's colour; white on a hit; muted: dark with an X; the
 * selected one: a thicker edge). direct: straight onto the panel (only these changed), else into the canvas strip */
static void gb_ovr(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c, int direct)
{
    if (!w || !h)
        return;
    if (direct)
        lcd_fill(x, y, w, h, c);
    else
        cv_rect((int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h, c);
}
static void gb_overlay(const gb_view_t *v, uint16_t bgc, int direct)
{
    uint32_t f[4], i, k, q = v->brd;
    for (i = 0; i < 4u; i++)
        f[i] = q > i * 240u ? (q - i * 240u < 240u ? q - i * 240u : 240u) : 0u;
    gb_ovr(f[0], 0, 240 - f[0], 3, bgc, direct);  /* the rest of the way first, then the white (the corners) */
    gb_ovr(237, f[1], 3, 240 - f[1], bgc, direct);
    gb_ovr(0, 237, 240 - f[2], 3, bgc, direct);
    gb_ovr(0, 0, 3, 240 - f[3], bgc, direct);
    gb_ovr(0, 0, f[0], 3, GB_W, direct);
    gb_ovr(237, 0, 3, f[1], GB_W, direct);
    gb_ovr(240 - f[2], 237, f[2], 3, GB_W, direct);
    gb_ovr(0, 240 - f[3], 3, f[3], GB_W, direct);
    for (k = 0; k < NTRK; k++) {
        uint32_t x = 62u + k * 20u, y = 220u, e = k == v->sel ? 3u : 1u, m = (v->tmute >> k) & 1u;
        uint16_t in = (v->thit >> k) & 1u ? GB_W : m ? 0u : gb_col[k];
        gb_ovr(x, y, 16, 16, GB_W, direct);
        gb_ovr(x + e, y + e, 16 - 2u * e, 16 - 2u * e, in, direct);
        if (m)
            for (i = 3; i < 13u; i++) {           /* the X (2 px) */
                gb_ovr(x + i, y + i, 2, 1, GB_W, direct);
                gb_ovr(x + 14u - i, y + i, 2, 1, GB_W, direct);
            }
    }
}

static void gb_draw(void)
{
    uint32_t i, s, mono, light, nstrip;
    uint16_t c;
    const uint8_t *a, *b;
    gb_fill(&gb_v);
    c = gb_v.fx ? GB_RGB(120, 128, 140) : gb_col[gb_v.sel % NTRK];   /* (the FX lane: grey) */
    a = (const uint8_t *)&gb_v;
    b = (const uint8_t *)&gb_last;
    for (i = 0; i < sizeof gb_v && a[i] == b[i]; i++)
        ;
    if (!ui.force && gb_have && i == sizeof gb_v)
        return;
    {                                             /* only the border or a square's hit changed: those, straight on */
        gb_view_t t = gb_v;
        t.thit = gb_last.thit;
        t.brd = gb_last.brd;
        if (!ui.force && gb_have && !memcmp(&t, &gb_last, sizeof t)) {
            gb_last = gb_v;
            gb_overlay(&gb_v, c, 1);
            return;
        }
    }
    {                                             /* only the drum flash or REC's glow changed: just the header's strip */
        gb_view_t t = gb_v;
        t.hit = gb_last.hit;
        t.thit = gb_last.thit;
        t.brd = gb_last.brd;
        if (t.rec && gb_last.rec)
            t.rec = gb_last.rec;
        nstrip = !ui.force && gb_have && !memcmp(&t, &gb_last, sizeof t) ? 1u : 240u / GB_STRIP;
    }
    gb_last = gb_v;
    gb_have = 1;
    mono = ux.mono;                               /* the text ramps in colour on black whatever the palette (restored below) */
    light = ux.light;
    if (mono || light) {
        ux.mono = 0;
        ux.light = 0;
        ux.gen++;
    }
    for (s = 0; s < nstrip; s++) {
        cv_begin(240, GB_STRIP, c);
        cv_oy = -(int32_t)(s * GB_STRIP);
        gb_scene(&gb_v, c);
        gb_overlay(&gb_v, c, 0);
        cv_oy = 0;
        cv_blit(0, s * GB_STRIP);
    }
    if (nstrip == 1u)                             /* (the header's strip only: the rest of the overlay may have moved) */
        gb_overlay(&gb_v, c, 1);
    if (mono || light) {
        ux.mono = (typeof(ux.mono))mono;
        ux.light = (typeof(ux.light))light;
        ux.gen++;
    }
}
