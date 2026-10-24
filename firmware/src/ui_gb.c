/* ui_gb.c - the groovebox look (FELUCCA_SIMPLE): black and one colour, big text, four circle-and-line dials a page. The
 * colour is the selected track's machine's (SYN BD SD HH CYM PERC: GB_COL, to be set from the app later); on the
 * MIXER each dial wears its own track's. One screen = the page's title and the BPM, four dials with their short
 * name over and the value under, the page dots and the machine's name. The PERF page is 16 tiles. The screen is first
 * described (gb_view_t), drawn from that, and only when the description changed. Pages: EDIT, FILTER, ENV, LFO, FX,
 * PERF, MIXER, CLOCK; the rest keep the old look. */
#if FELUCCA_SIMPLE
#define GB_STRIP 60                           /* canvas rows a strip */
#define GB_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static uint16_t gb_col[4] = {GB_RGB(255, 90, 54), GB_RGB(255, 214, 10), GB_RGB(46, 196, 255), GB_RGB(179, 136, 255)};   /* a colour a TRACK */
static const char *const GB_PILL[6] = {"SYNTH", "KICK", "SNARE", "HATS", "CYMBAL", "PERC"};

typedef struct {
    uint8_t perf, mixer, mach, sel, page, npages, mask, fade, rpt, hn, sv, flash;   /* SAVE / LOAD: 1 2; flash: 1 lit, 2 dark */
    char msg[2][16];
    int16_t bpm;
    uint8_t tm[4];                            /* each track's machine */
    uint8_t mac_n[8], mac_held[8];
    int16_t ratio[4];
    uint8_t alt[4];
    int16_t dj, cmix, ctune;                 /* PERF: the master DJ filter and comb */
    uint8_t fill[8], rel, bpm_alt;           /* PERF: each held macro's tile filled 0..26 rows; the release fade; the tempo is held by a macro */                           /* a macro holds this value: its dial is drawn inverted */
    char title[10], lab[4][8], val[4][12];
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
static int gb_page(void)
{
    const page_t *pg;
    if (ui.home || ui.layer || ui.menu || ui.confirm || name_on())
        return 0;
    pg = cur_page();
    if ((gbsv || gbsv_t) && !ui.menu && !ui.confirm)
        return 1;
    return pg->graph == GR_PERF || pg->graph == GR_TRK || pg->graph == GR_GBSEQ || pg->fam == FAM_TRK || (pg->fam == FAM_GLO && pg->scope == SC_GLOBAL) || dial_page();
}
static uint32_t gb_mach(uint32_t k) { return (uint32_t)trk[k & 3u].p[P_E7] % 6u; }

static void gb_fill(gb_view_t *v)
{
    const page_t *pg = cur_page();
    uint32_t k, i, n = 0, at = 0;
    uint64_t lk;
    char *p = (char *)v;
    for (i = 0; i < sizeof *v; i++)
        p[i] = 0;
    v->perf = pg->graph == GR_PERF;
    v->mixer = pg->graph == GR_TRK;
    v->sel = (uint8_t)song.sel;
    v->bpm = song.g[G_BPM];
    v->bpm_alt = (uint8_t)(pm_altered(TSEL, &song.g[G_BPM]) != 0);
    str_cpy(v->title, pg->title, sizeof v->title);
    for (k = 0; k < 4u; k++)
        v->tm[k] = (uint8_t)gb_mach(k);
    v->mach = v->tm[song.sel & 3u];
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
        v->rpt = (uint8_t)((perf_held | perf_latched) & 15u);
        return;
    }
    if (v->mixer) {                           /* the four tracks' levels */
        for (k = 0; k < 4u; k++) {
            const char *nm = GB_PILL[v->tm[k] % 6u];
            const track_t *t = &trk[k];
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
        str_cpy(v->title, "VOLUME", sizeof v->title);
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
        if (lk && vp >= TSEL->p && vp < TSEL->p + P_COUNT) {   /* a step held: its lock (inverted), else the sound's own */
            uint32_t id = (uint32_t)(vp - TSEL->p), st = 0;
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
        }
        param_format(d, cur, v->val[k], &u);
        if (str_eq(v->val[k], "CYM"))                 /* (too wide beside a negative value) */
            str_cpy(v->val[k], "CY", sizeof v->val[k]);
        r = RATIO(d, enum_rank(d, cur));
        v->ratio[k] = (int16_t)(r < 0 ? 0 : r);
        v->mask |= (uint8_t)(1u << k);
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

#define GB_W 0xFFFFu                                  /* the middle of a screen: white */
static void gb_scene(const gb_view_t *v, uint16_t bgc)
{
    const uint16_t c = GB_W;                      /* all ink white, the track's colour behind it */
    uint32_t k;
    char bb[8];
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
        cv_text_r(232, 14, &AF_M, GB_PILL[v->mach % 6u], c, bgc);
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
        cv_text_on(8, 4, &AF_L, v->hn > 2u ? "ARP" : "PERF", c, bgc);
        for (k = 0; k < 8u; k++) {
            int32_t tx = 8 + (int32_t)(k & 3u) * 56, ty = 44 + (int32_t)(k >> 2) * 38;
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
            gb_pill(tx, 124, 52, 28, GB_W, on);
            cv_text_in(tx, 130, 52, &AF_M, FN[k], on ? bgc : GB_W, on ? GB_W : bgc);
            gb_pill(tx, 158, 52, 28, GB_W, rp);
            cv_text_in(tx, 164, 52, &AF_M, RN[k], rp ? bgc : GB_W, rp ? GB_W : bgc);
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
            cv_text_on(8, 188, &AF_M, fb, c, bgc);
            if (v->dj > 1 || v->dj < -1) {                     /* the DJ filter: LP to the left, HP to the right, how far */
                int32_t a2 = v->dj < 0 ? -v->dj : v->dj;
                fb[0] = v->dj < 0 ? 'L' : 'H'; fb[1] = 'P'; fb[2] = ' ';
                fmt_int(nn, a2 * 100 / 64);
                for (q = 0; nn[q]; q++) fb[3u + q] = nn[q];
                fb[3u + q] = 0;
            } else {
                str_cpy(fb, "FILT -", sizeof fb);
            }
            cv_text_r(232, 188, &AF_M, fb, c, bgc);
            str_cpy(fb, "COMB ", sizeof fb);
            fmt_int(nn, v->cmix * 100 / 127);
            for (q = 0; nn[q]; q++) fb[5u + q] = nn[q];
            fb[5u + q] = 0;
            cv_text_on(8, 211, &AF_M, fb, c, bgc);
            str_cpy(fb, "TUNE ", sizeof fb);
            fmt_int(nn, v->ctune * 100 / 127);
            for (q = 0; nn[q]; q++) fb[5u + q] = nn[q];
            fb[5u + q] = 0;
            cv_text_r(232, 211, &AF_M, fb, c, bgc);
        }
        return;
    }
    cv_text_on(8, 6, &AF_L, v->title, c, bgc);                      /* the title, the BPM at the right */
    fmt_int(bb, v->bpm);
    if (v->bpm_alt) {                                             /* the tempo held by a macro: inverted */
        int32_t w = text_w(&AF_M, bb) + 12;
        cv_rrect(232 - w + 6, 8, w, 22, 6, GB_W, bgc);
        cv_text_r(232, 12, &AF_M, bb, bgc, GB_W);
    } else
        cv_text_r(232, 12, &AF_M, bb, c, bgc);
    for (k = 0; k < 4u; k++) {                                    /* the dials */
        int32_t cx = 8 + (int32_t)k * 56 + 28, cy = 108, a, px, py, on;
        uint16_t dc = GB_W, lc = GB_W, bk = bgc;
        const aafont_t *vf = &AF_L;
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
        if (v->mixer && k == v->sel)                              /* the selected track: a second ring */
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
        cv_text_in(cx - 27, vf == &AF_L ? 142 : 150, 54, vf, v->val[k], dc, bk);
    }
    for (k = 0; k < v->npages; k++)                               /* the page dots */
        if (k == v->page)
            cv_rrect(8 + (int32_t)k * 20, 206, 16, 8, 4, c, bgc);
        else {
            cv_rrect(8 + (int32_t)k * 20, 206, 8, 8, 4, c, bgc);
            cv_rrect(10 + (int32_t)k * 20, 208, 4, 4, 2, bgc, c);
        }
    {                                                             /* the machine's name, bottom right: an oval */
        const char *nm = GB_PILL[v->mach % 6u];
        int32_t w = (text_w(&AF_M, nm) + 17) & ~1;
        gb_pill(232 - w, 198, w, 24, c, 0);
        cv_text_on(232 - w + 8, 201, &AF_M, nm, c, bgc);
    }
}

static void gb_draw(void)
{
    uint32_t i, s, mono, light;
    uint16_t c;
    const uint8_t *a, *b;
    gb_fill(&gb_v);
    c = gb_col[gb_v.sel & 3u];
    a = (const uint8_t *)&gb_v;
    b = (const uint8_t *)&gb_last;
    for (i = 0; i < sizeof gb_v && a[i] == b[i]; i++)
        ;
    if (!ui.force && gb_have && i == sizeof gb_v)
        return;
    gb_last = gb_v;
    gb_have = 1;
    mono = ux.mono;                               /* the text ramps in colour on black whatever the palette (restored below) */
    light = ux.light;
    if (mono || light) {
        ux.mono = 0;
        ux.light = 0;
        ux.gen++;
    }
    for (s = 0; s < 240u / GB_STRIP; s++) {
        cv_begin(240, GB_STRIP, c);
        cv_oy = -(int32_t)(s * GB_STRIP);
        gb_scene(&gb_v, c);
        cv_oy = 0;
        cv_blit(0, s * GB_STRIP);
    }
    if (mono || light) {
        ux.mono = (typeof(ux.mono))mono;
        ux.light = (typeof(ux.light))light;
        ux.gen++;
    }
}
#endif
