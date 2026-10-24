/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t a = 4 + t->p[pg->id[0]] * 50 / 127, d = 6 + t->p[pg->id[1]] * 50 / 127, r = 6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_line_t(x0, bot, x1, top, c, 2);                                  /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line_t(px, py, x1 + i, EGY(lvl), c, 2);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line_t(px, py, x3, EGY(sus), c, 2);                               /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line_t(px, py, x3 + i, EGY(sus * e >> 15), c, 2);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
#undef EGY
}

static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, uni = t->p[P_LPOL] != 0;
    uint32_t ph = (uint32_t)t->p[P_LPHASE] << 25;
    int32_t z = uni ? cy + a : cy;                   /* the zero line: POL UNI (LFO 2) draws 0..+1 over it */
    cv_rect(PANEL_X0, z, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {                  /* two cycles (lfo_wave only reads the track) */
        int32_t w = lfo_wave((track_t *)t, ph + (uint32_t)x * (0xFFFFFFFFu / (PANEL_W / 2u))), y;
        if (t->p[P_LWAVE] == 4)
            w = (int32_t)((x / 20 * 2654435761u) >> 16) - 32768;
        y = uni ? z - (w + 32768) * a / 32768 : cy - w * a / 32768;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}

/* step bar x of step i of a 16-step row: 4 groups, as the footer (9 px bars, 13 px apart, 4 px between groups),
 * the row centred: x 12 .. 228 */
static int32_t bar_x(uint32_t i) { return 12 + (int32_t)(i % 16u) * 13 + (int32_t)(i % 16u / 4u) * 4; }

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left the C rows named (S, DIM) and a keyboard strip (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows in the track's scale (SCL ROOT / SCALE; CHR: the white
 * keys) are tinted LANE, a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a 9 x 3 bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
#define PR_ROWS 21                                  /* semitones shown (1.75 octaves) */
#define PR_RH 5                                     /* px per row */
#define PR_Y0 9                                     /* the top row (canvas y) */
#define PR_X0 35                                    /* the first step column */
#define PR_CW 12                                    /* px per step */
#define PR_KX 24                                    /* the keyboard strip, 9 px */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, i, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_chn[k] && kb_trk[k] == song.sel)
            for (i = 0; i < kb_chn[k]; i++) {
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_chord[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || (ui_prefs & PREF_ANIM_OFF);
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)
            pr_split(x, PR_Y0, w, 1, c, hits);
        else if (y >= ybot)
            pr_split(x, ybot - 1, w, 1, c, hits);
        else if (acc)
            pr_split(x, y, w, PR_RH, c == T_ACCENT ? c : T_TEXT, hits);
        else
            pr_split(x, y + 1, w, PR_RH - 2, c, hits);
    }
}
/* SEQ > DETAIL (1.5.1, Discussion #199): the cursor's 16 steps as four lanes, the knobs' order top to bottom, each
 * named on the left (the lane of the knob just turned in the accent): CHANCE a bar as high as the chance, RATCH the
 * step's bar in its 2..4 parts (as the roll draws a ratchet), NUDGE a tick left / right of the step's middle, VEL a bar
 * as high as the velocity (an ACC step's full, TEXT). Off its default a value is bright (THEME, the cursor's ACCENT),
 * at it faint (RAISE: 100 %, x1, 0, a VEL never set); DIM where it does not play (a NUDGE with QUANTIZE ON or on a
 * ratchet, a VEL under ACC). A step that plays nothing: a dot (a value left on it DIM); past LEN nothing. The cursor
 * framed through the lanes, the step playing a line */
#define SD_X0 36                                    /* the first step column (12 px each) */
/* CHORD (SCL 2): the chord's name ("Cm7": the last one the track played; before any, the one on its ROOT from
 * C4) over the keys from the C below its lowest note (two octaves, three for a wide one): its notes THEME, its
 * root's ACCENT, the others RAISE. OFF or a kit: what to do instead */
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
        h = (h ^ (st->vel | (uint32_t)st->probability << 8)) * 16777619u;   /* (1.5.1: DETAIL, the marks) */
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}







/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
}

#ifndef FM6_CHART_HOOK
#define FM6_CHART_HOOK(kind, a, b) ((void)0)   /* ui_render.c (host tests): the chart's parts one by one (its lint) */
#endif
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
/* FM6's 32 algorithms as charts. FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every chart as drawn
 * (no route through a box or touching another route, no two boxes overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};


/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static void graph_fm(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint16_t graph_plen[4][NTRK];                  /* .. each track's pattern length (project_info: 1/48 quarters) */
static uint32_t graph_pden[4][NTRK];                  /* .. and its picture for SONG (project.c proj_density: 2 bits a cell) */
static uint8_t graph_pcell[4][NTRK];                  /* .. in this many cells (min(LEN, 16)) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++)
            mask |= (uint8_t)((project_info(i, graph_pname[i], graph_plen[i], graph_pden[i], graph_pcell[i]) != 0) << i);
        graph_pname_sig = fnv(fnv(2166136261u, graph_pname, sizeof graph_pname), graph_plen, sizeof graph_plen);
        graph_pname_sig = fnv(fnv(graph_pname_sig, graph_pden, sizeof graph_pden), graph_pcell, sizeof graph_pcell);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}

/* SONG's timeline (graph_song): the sections left to right, each as wide as it plays: REPS x its clock track's pattern
 * (song_chain.c chain_clock: the first track not silent there; project_info: that track's LEN x DIV in its slot, what
 * chain_tick counts; a slot not saved, or every track silent: one bar). A short song is stretched over the strip
 * (fitted); a section under SG_MINW gets SG_MINW and the rest is shared out again; when the sections would get under
 * SG_FITBAR px a bar the song scrolls at SG_BAR px a bar, the view following the cursor's section and, playing, the
 * playing one. + ADD's slot (count < CHAIN_ROWS) at the end. Content px: 0 .. the width. Four lanes, T1 at the top */
#define SG_X0 24                                       /* the strip: x 24 .. 234 (the lanes' cushions left of it) */
#define SG_W 210
#define SG_Y 3                                         /* the lanes: y 3 .. 88 */
#define SG_LH 20                                       /* a lane's height (its letter, the pattern's picture), SG_LG between two */
#define SG_LG 2
#define SG_H (NTRK * (SG_LH + SG_LG) - SG_LG)
#define SG_LY(k) (SG_Y + (int32_t)(k) * (SG_LH + SG_LG))   /* lane k's top */
#define SG_GAP 2                                       /* between two blocks (in a block's width) */
#define SG_MINW 16                                     /* a block's least width, its gap included: a letter in it */
#define SG_ADDW 20                                     /* + ADD's slot */
#define SG_BAR 6                                       /* scrolled: px a bar */
#define SG_FITBAR 3                                    /* fitted: at least this many px a bar */
#define SG_BARU 192u                                   /* a 4/4 bar in 1/48 quarters */
#define SG_RY (SG_Y + SG_H + 3)                        /* the ruler's line (92); its ticks and numbers under it */
typedef struct {
    uint32_t n, add, fit;                              /* the rows, + ADD shown, fitted (no scrolling) */
    uint32_t u0[CHAIN_ROWS + 1u];                      /* each row's start in 1/48 quarters; u0[n]: the song's length */
    int32_t x[CHAIN_ROWS + 2u];                        /* each row's left edge (content px); x[n]: + ADD's; x[n + 1]: the end */
} song_lay_t;
static struct { int32_t off; uint32_t frame; uint8_t init, cur, play, last; } sgv;   /* the view (scrolled) */
/* a * b / c without a 64-bit divide: a and c halved alike until a * b fits */
static uint32_t sg_muldiv(uint32_t a, uint32_t b, uint32_t c)
{
    while (b && a > 0xFFFFFFFFu / b) { a >>= 1; c >>= 1; }
    return c ? a * b / c : 0u;
}
static uint32_t song_unit(const chain_row_t *r)        /* one pass of a section: its clock track's (1/48 quarters) */
{
    uint32_t k = chain_clock(r), s = r->slot[k];
    return s < 4u && graph_project_used(s) && graph_plen[s][k] ? graph_plen[s][k] : SG_BARU;
}
static void song_layout(song_lay_t *L)
{
    uint32_t i, n = chain_config.count <= CHAIN_ROWS ? chain_config.count : CHAIN_ROWS, forced = 0, nf = 0, uf = 0, chg = 1;
    uint32_t wb;
    L->n = n;
    L->add = n < CHAIN_ROWS;
    L->u0[0] = 0;
    for (i = 0; i < n; i++)
        L->u0[i + 1u] = L->u0[i] + chain_config.row[i].repeat * song_unit(&chain_config.row[i]);
    wb = SG_W - (L->add ? SG_ADDW : 0u);
    while (chg) {                                      /* fitted: shared by length, SG_MINW the least */
        chg = 0; nf = 0; uf = 0;
        for (i = 0; i < n; i++)
            if ((forced >> i) & 1u) nf++;
            else uf += L->u0[i + 1u] - L->u0[i];
        if (nf * SG_MINW > wb)
            break;
        for (i = 0; i < n; i++)
            if (!((forced >> i) & 1u) && sg_muldiv(L->u0[i + 1u] - L->u0[i], wb - nf * SG_MINW, uf) < SG_MINW) {
                forced |= 1u << i;
                chg = 1;
            }
    }
    L->fit = nf * SG_MINW <= wb && (!uf || sg_muldiv(SG_BARU, wb - nf * SG_MINW, uf) >= SG_FITBAR);
    if (L->fit) {
        uint32_t nb = 0, ub = 0;
        for (i = 0; i <= n; i++) {
            L->x[i] = (int32_t)(nb * SG_MINW + (uf ? sg_muldiv(ub, wb - nf * SG_MINW, uf) : 0u));
            if (i < n && ((forced >> i) & 1u)) nb++;
            else if (i < n) ub += L->u0[i + 1u] - L->u0[i];
        }
    } else {
        L->x[0] = 0;
        for (i = 0; i < n; i++) {
            uint32_t w = ((L->u0[i + 1u] - L->u0[i]) * SG_BAR + SG_BARU / 2u) / SG_BARU;
            L->x[i + 1u] = L->x[i] + (int32_t)(w > SG_MINW ? w : SG_MINW);
        }
    }
    L->x[n + 1u] = L->x[n] + (L->add ? SG_ADDW : 0);
}
static int32_t song_ux(const song_lay_t *L, uint32_t u)   /* 1/48 quarters -> content px (the blocks' scale) */
{
    uint32_t i = 0;
    while (i + 1u < L->n && u >= L->u0[i + 1u])
        i++;
    if (!L->n)
        return 0;
    return L->x[i] + (int32_t)sg_muldiv(u - L->u0[i], (uint32_t)(L->x[i + 1u] - L->x[i]), L->u0[i + 1u] - L->u0[i]);
}
static uint32_t song_cur(void) { return ui.song_row < chain_config.count ? ui.song_row : chain_config.count; }
/* the playhead, content px: the playing section's repeats played and its clock track's step in its pattern (-1: none) */
static int32_t song_playx(const song_lay_t *L)
{
    uint32_t r = chain.row, len, idx, reps;
    const track_t *t = &trk[chain.clk % NTRK];
    if (!chain.running || r >= L->n)
        return -1;
    len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
    idx = t->seq_pos >= 0x7FFFFFFFu ? 0u : t->seq_idx % len;
    reps = chain.config.row[r].repeat ? chain.config.row[r].repeat : 1u;
    return L->x[r] + (int32_t)sg_muldiv((reps - (chain.remaining <= reps ? chain.remaining : reps)) * len + idx,
                                        (uint32_t)(L->x[r + 1u] - L->x[r] - SG_GAP), reps * len);
}
/* once a frame: the view toward the cursor's block and the playing one (both when they fit, else the one that moved
 * last), a block's margin round them; halving the way with MENU > ANIM ON, at once on a redraw forced, a gap in
 * the frames or ANIM OFF. Returns the view's left edge */
static int32_t song_follow(const song_lay_t *L)
{
    uint32_t c = song_cur(), p = chain.running && chain.row < L->n ? chain.row : 0xFFu;
    int32_t a = L->x[c], b = c < L->n ? L->x[c + 1u] - SG_GAP : L->x[c] + SG_ADDW, tgt = sgv.off, d, m, end;
    int fresh = !sgv.init || ui.frame != sgv.frame + 1u, snap = fresh || ui.force || (ui_prefs & PREF_ANIM_OFF);
    if (sgv.init && sgv.frame == ui.frame)
        return sgv.off;
    if (fresh) {                                     /* the page (re)entered: the playing row first */
        sgv.off = 0;
        sgv.last = p != 0xFFu;
    } else if (p != sgv.play) {
        sgv.last = 1;
    } else if (c != sgv.cur) {
        sgv.last = 0;
    }
    if (p != 0xFFu) {
        int32_t pa = L->x[p], pb = L->x[p + 1u] - SG_GAP;
        if ((pb > b ? pb : b) - (pa < a ? pa : a) <= SG_W) {
            a = pa < a ? pa : a;
            b = pb > b ? pb : b;
        } else if (sgv.last) {
            a = pa;
            b = pb;
        }
    }
    m = (SG_W - (b - a)) / 2;
    m = m > SG_MINW ? SG_MINW : m < 0 ? 0 : m;
    if (a - m < tgt) tgt = a - m;
    if (b + m > tgt + SG_W) tgt = b + m - SG_W;
    end = L->x[L->n + 1u] - SG_W;
    tgt = L->fit || end < 0 ? 0 : tgt < 0 ? 0 : tgt > end ? end : tgt;
    d = tgt - sgv.off;
    sgv.off = snap ? tgt : sgv.off + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0);
    sgv.cur = (uint8_t)c;
    sgv.play = (uint8_t)p;
    sgv.frame = ui.frame;
    sgv.init = 1;
    return sgv.off;
}

/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : g == GR_LFO ? SK_LFO : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer, the strip) */
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            n++;
            if (i == ui.page)
                k = n;
        }
    str_cpy(ti, pt ? pt : grid_on() && !detail_on() ? "GRID" : pg->title, 12);
    if (FELUCCA_SIMPLE && alt_mode && alt_ids(pg))     /* OCT+ held: the page's setup values */
        str_cpy(ti + str_len(ti), "+", 4);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_int(ti + str_len(ti), (int32_t)k);
        str_cpy(ti + str_len(ti), "/", 4);
        fmt_int(ti + str_len(ti), (int32_t)n);
    }
    if (pg->graph == GR_ROLL && !drum_track(t) && !detail_on()) {   /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
static void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, T_THEME, T_SURF);
}

static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home)
        return h ^ (ui.frame / 2u);                  /* scope: redraw every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number) */
        return h ^ 0x5A17u;
    if (pg->graph == GR_NONE || pg->graph == GR_ARP) h ^= ui.frame / 2u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u +
         ui.ppick * 1299709u;
    if (pg->graph == GR_CHORD) {                     /* the last chord played */
        h = (h ^ (chord_last[song.sel].root + 131u * chord_last[song.sel].mask)) * 16777619u;
        for (i = 0; i < CHORD_MAX; i++)
            h = (h ^ chord_last[song.sel].note[i]) * 16777619u;
        h ^= (uint32_t)t->engine * 389u;             /* (MONO and kits follow the sounding engine) */
    }
    h ^= pm_h.n ? 0x9E3779B1u * (pm_h.n + 1u) : 0u;
    if (pg->graph == GR_PERF) {
        uint32_t j;
        for (j = 0; j < PM_NM; j++)
            h = (h ^ (pm_mac[j].n | (j < pm_h.n ? (uint32_t)(pm_h.mac[j] + 1u) << 8 : 0u))) * 16777619u;
        h = (h ^ (pm_fade | pm_func << 4 | pm_arp << 8)) * 16777619u;
    }
    if (pg->graph == GR_SONG) {
        h ^= ui.song_row * 40503u + chain_config.count * 7919u + ui.song_trk * 3571u;
        h = fnv(h, chain_config.row, sizeof chain_config.row);
        h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u : 0u;
        for (i = 0; i < 4u; i++) h ^= (uint32_t)graph_project_used(i) << (24u + i);
        h += graph_pname_sig;
        {                                            /* the timeline: its view (following, ui_graph.c song_follow), the playhead */
            song_lay_t L;
            song_layout(&L);
            h = (h ^ (uint32_t)song_follow(&L)) * 16777619u;
            h = (h ^ (uint32_t)(song_playx(&L) + 1)) * 16777619u;
            h ^= (ui_prefs & PREF_LARGE) ? 0x2545F491u : 0u;
        }
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_slot + 1u) * 40503u;
    if (pg->graph == GR_SLCR && t->p[P_SLCR])        /* the SLICER's step playing */
        h ^= (sl[song.sel].idx + 1u) * 2654435761u;
    if (pg->graph == GR_SLOTS) {                     /* (a checksum over each slot) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)graph_project_used(i) << (20u + i);
        h += graph_pname_sig;
    }
    if (pg->scope == SC_ENGINE)
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->graph == GR_ROLL) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u;
        {                                            /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (detail_on())                             /* DETAIL: QUANTIZE (NUDGE's DIM), the lane named in the accent */
            h = (h ^ (0xD1u + (uint32_t)t->p[P_SQNT] * 31u + (ui.hot_t ? ui.hot_col + 1u : 0u) * 977u)) * 2654435761u;
        else if (!drum_track(t)) {                   /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
        }
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
                                       {"DIGITAL", "DGTL"},
                                       {"PHASE", "PHAS"}, {"LOFI", "LOFI"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px), in M as every list's (PRESETS, USER, PHRASES, PROJECT, AUTOMATION, MOD, the MENU): the selected
 * one a THEME bar with INK text; tag at x 14 (the widest, DRUM, to x 57), free text from x LIST_NX to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
#define LIST_TY CAP_IN(M, 16)                           /* a row's text: its capitals rows 2 .. 12 of the row */
#define LIST_NX 64
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + LIST_TY, &AF_M, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(LIST_NX, y + LIST_TY, &AF_M, name, sel ? T_INK : nc, bg, x1 - LIST_NX);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel note hint centred");
    cv_text_on(cv_keycap(HALF_UP(240 - kh_ink((uint32_t)id, s)), y + 1, (uint32_t)id, T_KEY, T_INK, T_SURF) + KH_GAP, y,
               &AF_S, s, fg, T_SURF);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)graph_h());
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)graph_h(), AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_SURF);
}

/* the AUTOMATION rows' columns (M): the kind at 14 (LOCK AUTO STEP PLAY, to x 54), the step / PLAY's ON at EV_SX, the
 * name / QUANTIZE's ON at EV_NX (QUANTIZE ends at x 86), the value right at 226 */
#define EV_SX 60
#define EV_NX 92
/* a row's value (M) and its unit (S, on the value's baseline, as a card's) ending at x 226; the value's left x */
static int32_t ev_value(int32_t y, const char *v, const char *unit, uint16_t fg, uint16_t bg)
{
    int32_t x = 226;
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    if (unit && unit[0]) {
        GFX_HOOK_ALIGN(0, 0, 0, y + LIST_TY + AF_M.asc, AL_B, "list row unit on its value's baseline");
        x = cv_text_r(x, y + LIST_TY + AF_M.asc - AF_S.asc, &AF_S, unit, fg, bg) - 2;
    }
    return cv_text_r(x, y + LIST_TY, &AF_M, v, fg, bg);
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot, uint32_t lvl,
                        const char *b, uint32_t lit)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : T_THEME;
    cv_begin(CARD_W, LG_H_GRAPH, T_BG);
    cv_rrect(0, 0, CARD_W, LG_H_GRAPH, 5, T_SURF, T_BG);
    cv_icon_on(4, 4, 16, trk_icon(c, sel), lit ? (sel ? T_TEXT : T_ACCENT) : sel ? T_ACCENT : T_MID, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_INK, T_SURF);
    else if (st)
        cv_keycap(53 - kc_w(KC_MUTE), 5, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    else
        cv_free_text(5, 22, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
    cv_rrect(5, TSS_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(c), LG_Y_GRAPH);
}
static void draw_tracks(void)
{
    uint32_t c, sk = strip_kind() == SK_TRK;
    if (ui.force) {                                  /* the BG beside the columns only, they cover the rest (#181: */
        draw_gaps(graph_y(), graph_h());             /* the whole panel cleared first flashed on every sound load and
                                                      * track change); LINE: divided as the cards above */
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
    }
    for (c = 0; c < NTRK; c++) {
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0, lit = (uint32_t)midi_lit(c);
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV, 4 MUTE (its badge) */
        int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint16_t vc = mute ? T_DIM : T_THEME;
        char b[16];
        t->peak = 0;
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u +
              lit * 15485863u;                         /* (1.5.5: MIDI IN lights the cushion) */
        if (!ui.force && sig == ts.col[c])
            continue;
        ts.col[c] = sig;
        if (sk) {
            track_strip(c, sel, st, mute, arm, hot, lvl, b, lit);
            continue;
        }
        cv_begin(CARD_W, H_GRAPH, T_BG);
        cv_rrect(0, 0, CARD_W, H_GRAPH, 5, T_SURF, T_BG);
        cv_icon_on(4, 5, 16, trk_icon(c, sel), lit ? (sel ? T_TEXT : T_ACCENT) : sel ? T_ACCENT : T_MID, T_SURF);
        if (st)
            GFX_HOOK_ALIGN(0, 5, 0, 21, AL_V, "mixer badge on the cushion's line");
        if (st == 1u || st == 2u)                    /* REC (recording) / ARM (armed, stopped) */
            cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 6, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                      T_INK, T_SURF);
        else if (st)                                 /* muted (K4 just turned: lit) */
            cv_keycap(53 - kc_w(KC_MUTE), 6, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        if (mute && arm)                             /* armed and muted: MUTE in place of the name */
            GFX_HOOK_ALIGN(0, 21 + AF_S_CAP_Y, 0, 21 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "mixer MUTE badge on the name's line");
        if (mute && arm)                             /* (on the name's capitals) */
            cv_keycap(5, 21 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        else
            cv_free_text(5, 21, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
        {   /* LEVEL: the knob, its dB inside (OFF at 0), and the meter of the output */
            char v[8];
            knob(KB_X, KB_Y, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, (int32_t)lvl, 0, 127, hot == 1u ? T_ACCENT : vc);
            if (lvl) {
                int32_t d = LEVEL_DB_X10[lvl];
                fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
                GFX_HOOK_ALIGN(KB_X, 0, KB_X + 2 * KNOB_BIG_R, 0, AL_H, "mixer level text centred across");
                cv_text_in(KB_X, KB_Y + KNOB_BIG_R + 5, 2 * KNOB_BIG_R, &AF_S, "dB", T_DIM, T_SURF);
            } else {
                str_cpy(v, "OFF", sizeof v);
            }
            GFX_HOOK_ALIGN(KB_X, KB_Y, KB_X + 2 * KNOB_BIG_R, KB_Y + 2 * KNOB_BIG_R, AL_HV, "mixer level value in its knob");
            cv_text_in(KB_X, KB_Y + CAP_IN(S, 2 * KNOB_BIG_R), 2 * KNOB_BIG_R, &AF_S, v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM, T_SURF);
            cv_rrect(45, TS_MY, 4, TS_MH, 2, T_RAISE, T_SURF);
            if (ts.meter[c])
                cv_rrect(45, TS_MY + TS_MH - 1 - ts.meter[c], 4, ts.meter[c] + 1, ts.meter[c] >= 4 ? 2 : 0, T_MID, T_RAISE);
        }
        {   /* PAN (from the centre) and the REV send: captions, knobs, values */
            char v[8];
            uint16_t pc = hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(15 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "PAN", T_DIM, T_SURF);   /* (by their ink) */
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(42 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "REV", T_DIM, T_SURF);
            knob(15 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, pan, -64, 63, pc);
            knob(42 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, rv, 0, 127, rc);
            v[0] = '+';                              /* the cards' numbers, without the % */
            fmt_int(pan > 0 ? v + 1 : v, pan * 100 / (pan > 0 ? 63 : 64));   /* (+100 at the top: param_format) */
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(15 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, pc, T_SURF);
            fmt_int(v, (rv * 100 + 63) / 127);
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(42 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, rc, T_SURF);
        }
        cv_blit((uint32_t)CARD_X(c), Y_GRAPH);
    }
}
/* HOME > TRACKS (1.3, Discussions #112 / #134: MENU > DISPLAY > HOME TRACKS): the four tracks as rows in the panel, one
 * under the other (29 px, 2 apart), in place of the scope; the cards above stay HOME's four (the selected track's). A row:
 * the track's cushion at the left; on its first line the engine's icon and the sound's name (MENU > LARGE: the name in M,
 * no icon), a MUTE badge at its right when muted; under it the 16 steps of the page of the track's pattern that plays
 * (stopped: the selected track's cursor page, the others' first): a note a bar (accented: taller, the accent), a TIE after
 * a note the bar carried on from the one before, DRUM hits a pad (two hits or more: two pads), an empty step or a REST a
 * stub (a TIE with nothing to carry: brighter); the step playing on a raised cell. LEN > 16: a pip per page right of the
 * steps (the page shown THEME). At the right the track's output meter. The selected track's row is the selection (THEME,
 * its ink INK, as a list's selected row); a muted track's steps DIM. Each row is its own canvas with its own signature
 * (ts.col, the MIXER's: the two never show together); a meter that moved alone redraws its slot only. LINE: the rows on
 * BG, 1 px rules between them */
#define TR_H 29                                      /* a row; rows TR_PITCH apart from Y_GRAPH, 3 .. 237 across */
#define TR_PITCH 31
#define TR_W 234
#define TR_NY(lg) ((lg) ? 2 : 4)                     /* the name line's capitals from row 4 (LARGE, M: 2) */
#define TR_SB 26                                     /* the steps: bars standing on row 25 */
#define TR_BW 9                                      /* a step's bar, 11 px apart, 4 more between groups of 4 */
#define TR_PX 216                                    /* the page pips */
#define TR_MX 226                                    /* the meter: 4 x 22 from row 4 */
#define TR_MY 4
#define TR_MH 22
static int32_t tr_bx(uint32_t i) { return 24 + (int32_t)i * 11 + (int32_t)(i / 4u) * 4; }
static void tr_meter(int32_t x, int32_t y, uint32_t c, uint16_t slot, uint16_t fill, uint16_t under)
{
    int32_t m = ts.meter[c] * TR_MH / (TS_MH - 2);
    cv_rrect(x, y, 4, TR_MH, 2, slot, under);
    if (m)
        cv_rrect(x, y + TR_MH - m, 4, m, m >= 4 ? 2 : 0, fill, slot);
}
static void home_track_row(uint32_t c)
{
    track_t *t = &trk[c];
    const step_t *s = seq_steps(t);
    uint32_t sel = c == song.sel, mute = t->p[P_MUTE] != 0, large = (ui_prefs & PREF_LARGE) != 0u, lit = (uint32_t)midi_lit(c);
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), np = (len + 15u) / 16u, pg, p0, ph, i, sig, carry = 0;
    int32_t y = Y_GRAPH + (int32_t)c * TR_PITCH, m, x, xr = 222, ch = large ? AF_M_CAP_H : AF_S_CAP_H;
    uint16_t bg = sel ? T_THEME : T_SURF, half = ux_mix(T_THEME, T_INK, 35);
    uint16_t cn = sel ? (mute ? ux_mix(T_THEME, T_INK, 55) : T_INK) : mute ? T_DIM : T_THEME;   /* a note */
    uint16_t ca = sel || mute ? cn : T_ACCENT, cs = sel ? half : T_RAISE;   /* accented; a stub */
    uint16_t cp = sel ? ux_mix(T_THEME, T_INK, 20) : T_RAISE, cpl = sel ? T_INK : mute ? T_MID : T_TEXT;   /* playing */
    char b[16];
    pg = song.playing ? t->seq_idx / 16u : sel ? ui.cursor / 16u : 0u;
    if (pg >= np)
        pg = np - 1u;
    p0 = pg * 16u;
    ph = song.playing && t->seq_idx / 16u == pg ? t->seq_idx % 16u : 0xFFu;
    trk_short_name(c, b);
    m = mute ? 0 : meter_px(t->peak);
    t->peak = 0;
    if (m < ts.meter[c] - 1)
        m = ts.meter[c] - 1;                         /* (falls as the MIXER's) */
    sig = str_hash(1u + sel + mute * 2u + large * 4u + t->eng_req * 8u, b) + steps_hash(t) * 31u + pg * 7919u +
          ph * 104729u + len * 1299709u + lit * 15485863u;   /* (1.5.5: MIDI IN lights the cushion) */
    if (!ui.force && sig == ts.col[c]) {
        if (m != ts.meter[c]) {                      /* the meter alone: its slot */
            ts.meter[c] = (uint8_t)m;
            cv_begin(4, TR_MH, bg);
            tr_meter(0, 0, c, cs, sel ? T_INK : T_MID, bg);
            cv_blit(3 + TR_MX, (uint32_t)(y + TR_MY));
        }
        return;
    }
    ts.col[c] = sig;
    ts.meter[c] = (uint8_t)m;
    cv_begin(TR_W, TR_H, T_BG);
    cv_rrect(0, 0, TR_W, TR_H, 5, bg, T_BG);
    cv_icon_mid(4, TR_H / 2, 16, trk_icon(c, 1), lit ? (sel ? half : T_ACCENT) : sel ? T_INK : mute ? T_DIM : T_MID, bg);
    if (mute) {                                      /* MUTE: its badge at the name line's end */
        xr -= kc_w(KC_MUTE);
        GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row badge on the name's line");
        cv_keycap(xr, TR_NY(large) + (ch - KC_H) / 2, KC_MUTE, sel ? T_INK : T_KEY, sel ? T_THEME : T_INK, bg);
        xr -= 6;
    }
    x = 24;
    if (FELUCCA_ICONS && !large) {                   /* the engine's icon on the name's capitals */
        uint32_t ei = engine_icon(ENGINES[t->eng_req % NENGINES]->name);
        if (ei != ICON_NONE)
            GFX_HOOK_ALIGN(0, TR_NY(large), 0, TR_NY(large) + ch, AL_V, "tracks row icon on the name's line");
        x += cv_icon_in(x, TR_NY(large), 0, ch, 12, ei, sel ? T_INK : T_MID, bg) + 5;
    }
    cv_free_text(x, TR_NY(large) - (large ? AF_M_CAP_Y : AF_S_CAP_Y), large ? &AF_M : &AF_S, b, sel ? T_INK : mute ? T_DIM : T_TEXT,
                 bg, xr - x);
    for (i = p0; i-- > 0u;)                          /* a TIE at the page's start: carried from a note before it? */
        if (s[i].time != ST_TIE) {
            carry = s[i].time == ST_NOTE && s[i].n;
            break;
        }
    for (i = 0; i < 16u && p0 + i < len; i++) {
        const step_t *st = &s[p0 + i];
        int32_t bx = tr_bx(i);
        uint16_t under = i == ph ? cp : bg;
        if (i == ph)                                 /* the step playing: a raised cell */
            cv_rrect(bx - 1, 15, TR_BW + 2, 13, 2, cp, bg);
        if (st->time == ST_TIE && carry) {           /* carried on: joined to the bar before */
            int32_t x0 = i ? tr_bx(i - 1u) + TR_BW : bx - 2;
            cv_rect(x0, TR_SB - 7, bx + 2 - x0, 7, i == ph ? cpl : cn);
            cv_rrect(bx, TR_SB - 7, TR_BW, 7, 2, i == ph ? cpl : cn, under);
        } else if (st->time == ST_NOTE && st->n) {   /* notes: a bar, accented taller */
            int acc = (st->flags & SF_ACCENT) != 0u;
            cv_rrect(bx, TR_SB - (acc ? 10 : 7), TR_BW, acc ? 10 : 7, 2, i == ph ? cpl : acc ? ca : cn, under);
            carry = 1;
            continue;
        } else if (st->time == ST_NOTE && st->hit) { /* DRUM hits: a pad, two for two hits or more */
            uint16_t pc = i == ph ? cpl : (st->acc & st->hit) ? ca : cn;
            cv_rrect(bx, TR_SB - 4, TR_BW, 4, 1, pc, under);
            if (st->hit & (st->hit - 1u))
                cv_rrect(bx, TR_SB - 9, TR_BW, 4, 1, pc, under);
        } else {                                     /* empty, a REST, a TIE carrying nothing */
            cv_rect(bx, TR_SB - 2, TR_BW, 2, i == ph ? (sel ? T_INK : T_MID) : st->time == ST_TIE ? (sel ? T_INK : T_MID) : cs);
        }
        carry = st->time == ST_TIE && carry;
    }
    if (np > 1u)                                     /* LEN > 16: the pages, top to bottom */
        for (i = 0; i < np; i++)
            cv_rect(TR_PX, TR_SB - 3 * (int32_t)(np - i) + 1, 3, 2, i == pg ? (sel ? T_INK : T_THEME) : cs);
    tr_meter(TR_MX, TR_MY, c, cs, sel ? T_INK : T_MID, bg);
    cv_blit(3, (uint32_t)y);
}
static void draw_home_tracks(void)
{
    uint32_t c;
    if (ui.force) {                                  /* the BG round the rows only, they cover the rest (#181) */
        lcd_fill(0, Y_GRAPH, 3, H_GRAPH, T_BG);
        lcd_fill(3 + TR_W, Y_GRAPH, 240 - 3 - TR_W, H_GRAPH, T_BG);
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
        for (c = 1; c < NTRK; c++) {                 /* the 2 rows between two rows; LINE: a rule on the first */
            uint32_t y = (uint32_t)(Y_GRAPH + (int32_t)c * TR_PITCH - 2);
            if (ux.style) {
                lcd_fill(3, y, TR_W, 1, T_RULE);
                GFX_HOOK_RULE(3, y, TR_W, 1);
            }
            lcd_fill(3, ux.style ? y + 1u : y, TR_W, ux.style ? 1u : 2u, T_BG);
        }
    }
    for (c = 0; c < NTRK; c++)
        home_track_row(c);
}
/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void graph_scope(uint16_t c)
{
    static int16_t snap[SCOPE_N] __attribute__((section(".pool")));
    uint32_t w = scope_w, i, trig = 0;
    int32_t cy = (int32_t)cv_h / 2, py = cy, x, peak = 1500, a = cv_h == H_GRAPH ? 46 : cy - 6;   /* (LARGE: the strip) */
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;    /* auto-scaled */
        if (x)
            cv_line_t(PANEL_X0 - 1 + x, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}

/* SONG is a playing order of the four stored project patterns. Letters match
 * PROJECT A..D; loading a project still restores its sound, SONG borrows steps. */
/* SONG as a timeline (song_layout), four tall lanes (T1 at the top, each led by its track's cushion, the selected
 * lane's the accent's) over a thin bar ruler: in a lane, the sections that play the same slot one after the other
 * make one block (RAISE, the slot's letter top-left; a slot not saved only outlined, DIM). In a block the picture of
 * the track's pattern in that slot (project.c proj_density, read with the slot: graph_pden): a bar a cell, as tall as
 * the cell is busy (a mean over its steps: rests 0, ties 1, notes / hits 2, accents 3; an empty cell a faint
 * baseline), the pattern looped over the section as it plays and a notch between the section's passes (its REPS). A
 * silent section ("-") is an empty lane with a faint line. The sections' boundaries are faint lines across the lanes
 * (seen between the blocks). The cell selected (KNOB 1 the section, KNOB 2 the lane) is THEME, its bars and letter
 * INK (a slot not saved: outlined THEME 2 px; "-": a THEME outline in the gap); + ADD's slot dashed after the last
 * section. Playing: the playhead across the lanes down to the ruler (ACCENT; INK on THEME), the playing section's
 * blocks filled up to it (RAISE -> ACCENT). The ruler: a tick a bar, longer every 4 with the bar's number. Under it
 * one line (M: the selected cell's detail): the selected cell's project name (NOT SAVED, SILENT; on + ADD the hint, S as
 * every key hint) and, right, the song's length in bars where it fits. MENU > LARGE: the letters in M too (the ruler's
 * numbers stay S) */
#define SG_PAD 3                                       /* a block's inner margin: the letter's, the picture's */
#define SG_PH (SG_LH - 2 * SG_PAD)                     /* the picture's height (14 px: 3 levels) */
#define SG_TX 5                                        /* the lanes' cushions (16 px) */
#define SG_IY 119                                      /* the line under the ruler: its capitals end on row 118 */
static void sg_dash(int32_t bx, int32_t bw, int32_t lo, int32_t hi, uint16_t c)   /* + ADD: dashed, clipped lo .. hi */
{
    int32_t k;
    for (k = 0; k < bw; k++)
        if (!((k >> 1) & 1) && bx + k >= lo && bx + k < hi) {
            cv_rect(bx + k, SG_Y, 1, 1, c);
            cv_rect(bx + k, SG_Y + SG_H - 1, 1, 1, c);
        }
    for (k = 0; k < SG_H; k++)
        if (!((k >> 1) & 1)) {
            if (bx >= lo) cv_rect(bx, SG_Y + k, 1, 1, c);
            if (bx + bw <= hi) cv_rect(bx + bw - 1, SG_Y + k, 1, 1, c);
        }
}
/* an outline of w x h at x, y, n px thick, clipped to lo .. hi (its ends only where not clipped) */
static void sg_frame(int32_t bx, int32_t bw, int32_t y, int32_t h, uint32_t n, int32_t lo, int32_t hi, uint16_t c)
{
    int32_t x0 = bx > lo ? bx : lo, x1 = bx + bw < hi ? bx + bw : hi;
    uint32_t k;
    if (x1 <= x0)
        return;
    for (k = 0; k < n; k++) {
        cv_rect(x0, y + (int32_t)k, x1 - x0, 1, c);
        cv_rect(x0, y + h - 1 - (int32_t)k, x1 - x0, 1, c);
        if (x0 == bx) cv_rect(bx + (int32_t)k, y, 1, h, c);
        if (x1 == bx + bw) cv_rect(bx + bw - 1 - (int32_t)k, y, 1, h, c);
    }
}
/* one lane's drawing state: the screen spans of the cell selected and of the playing section's part played */
typedef struct { int32_t c0, c1, p0, p1; uint16_t pf; } sg_fill_t;
static uint16_t sg_fill_at(const sg_fill_t *F, int32_t x)   /* the block's fill under column x */
{
    return x >= F->c0 && x < F->c1 ? T_THEME : x >= F->p0 && x < F->p1 ? F->pf : T_RAISE;
}
/* the slot's letter at x (MENU > LARGE: M), its capitals SG_PAD under the lane's top; 0: no room before x1 */
static int32_t sg_letter(int32_t x, int32_t x1, int32_t y, uint32_t s, uint16_t ink, uint16_t bg)
{
    const aafont_t *f = (ui_prefs & PREF_LARGE) ? &AF_M : &AF_S;
    char a[2] = {(char)('A' + s), 0};
    int32_t w = text_w(f, a);
    if (x + w + SG_PAD > x1)
        return 0;
    cv_text_on(x, y + SG_PAD - (f == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y), f, a, ink, bg);
    return w;
}
/* the picture of slot s's track k over sections i .. j - 1 (columns x0 .. x1 on screen), but for the letters'
 * columns (skip0 .. skip1, skip2 .. skip3) */
static void sg_picture(const song_lay_t *L, int32_t off, uint32_t k, uint32_t i, uint32_t j, uint32_t s, int32_t x0,
                       int32_t x1, const sg_fill_t *F, const int32_t *skip)
{
    int32_t y = SG_LY(k), yb = y + SG_PAD + SG_PH, x;
    uint32_t q, plen = graph_plen[s & 3u][k], n = graph_pcell[s & 3u][k], den = graph_pden[s & 3u][k];
    if (!plen || !n)
        return;
    for (q = i; q < j; q++) {
        int32_t sx = SG_X0 + L->x[q] - off, wt = L->x[q + 1u] - L->x[q] - SG_GAP, a, b;
        uint32_t reps = chain_config.row[q].repeat ? chain_config.row[q].repeat : 1u, unit = song_unit(&chain_config.row[q]);
        uint32_t tot = reps * unit, pc = 0xFFFFFFFFu, pl = 0xFFFFFFFFu, pp = 0xFFFFFFFFu, gaps, notch;
        if (wt < 1)
            continue;
        a = sx > x0 ? sx : x0;
        b = q + 1u == j ? sx + wt : sx + wt + SG_GAP;  /* (inside a run: on to the next section) */
        b = b < x1 ? b : x1;
        gaps = sg_muldiv((uint32_t)wt, plen, tot * n) >= 3u;   /* a cell 3 px or more: a gap between two */
        notch = reps > 1u && (uint32_t)wt / reps >= 4u;        /* the passes far enough apart: a notch between */
        for (x = sx; x < b; x++) {
            uint32_t r = (uint32_t)(x - sx), u = sg_muldiv(r < (uint32_t)wt ? r : (uint32_t)wt - 1u, tot, (uint32_t)wt);
            uint32_t pos = u % plen, cell = pos * n / plen, pass = u / unit, h;
            int edge = cell != pc || u / plen != pl, cut = notch && pass != pp && x > sx;
            uint16_t fill, bar;
            pc = cell;
            pl = u / plen;
            pp = pass;
            if (x < a || (x >= skip[0] && x < skip[1]) || (x >= skip[2] && x < skip[3]))
                continue;
            fill = sg_fill_at(F, x);
            bar = fill == T_THEME ? T_INK : T_THEME;
            if (cut) {                                 /* a pass begins: the notch */
                cv_rect(x, y + SG_PAD, 1, SG_PH, T_SURF);
                continue;
            }
            if (gaps && edge && x > sx)
                continue;
            h = (den >> (2u * cell)) & 3u;
            if (h)
                cv_rect(x, yb - (int32_t)(h * SG_PH / 3u), 1, (int32_t)(h * SG_PH / 3u), bar);
            else
                cv_rect(x, yb - 1, 1, 1, ux_mix(fill, bar, 30));
        }
    }
}
/* lane k's block of sections i .. j - 1 on slot s; c: the cursor's section when it is in this lane, else past it;
 * sp: the playhead on screen (-100: none) */
static void sg_block(const song_lay_t *L, int32_t off, uint32_t k, uint32_t i, uint32_t j, uint32_t s, uint32_t c,
                     int32_t sp)
{
    int32_t hi = SG_X0 + SG_W, y = SG_LY(k), bx = SG_X0 + L->x[i] - off, bw = L->x[j] - L->x[i] - SG_GAP, lx, w;
    int32_t x0 = bx > SG_X0 ? bx : SG_X0, x1 = bx + bw < hi ? bx + bw : hi, skip[4] = {0, 0, 0, 0};
    int saved = graph_project_used(s), cur = c >= i && c < j;
    sg_fill_t F = {0, 0, 0, 0, 0};
    uint32_t p = chain.running && chain.row >= i && chain.row < j ? chain.row : 0xFFu;
    if (x1 <= x0)
        return;
    if (!saved) {                                      /* not saved: an outline, the letter DIM */
        if (cur) {
            int32_t cx = SG_X0 + L->x[c] - off, cw = L->x[c + 1u] - L->x[c] - SG_GAP;
            sg_frame(bx, bw, y, SG_LH, 1, SG_X0, hi, T_DIM);
            sg_frame(cx, cw, y, SG_LH, 2, SG_X0, hi, T_THEME);
            sg_letter((cx > SG_X0 ? cx : SG_X0) + SG_PAD, x1, y, s, T_THEME, T_SURF);
            return;
        }
        sg_frame(bx, bw, y, SG_LH, 1, SG_X0, hi, T_DIM);
        sg_letter(x0 + SG_PAD, x1, y, s, T_DIM, T_SURF);
        return;
    }
    cv_rrect(x0, y, x1 - x0, SG_LH, x0 != bx || x1 != bx + bw ? 0 : 2, T_RAISE, T_SURF);
    if (p != 0xFFu && sp >= SG_X0) {                   /* playing here: the part played */
        int32_t a = SG_X0 + L->x[p] - off;
        F.pf = ux_mix(T_RAISE, T_ACCENT, 35);
        F.p0 = a > x0 ? a : x0;
        F.p1 = sp + 2 < x1 ? sp + 2 : x1;              /* (its right corners under the playhead) */
        if (F.p1 > F.p0) {                             /* (no cell of its own: the letter may cross its end) */
            int32_t e = F.p0 == bx && ux.style != ST_LINE && F.p1 - F.p0 > 2 ? 2 : 0;   /* the block's rounded end */
            cv_rect(F.p0 + e, y, F.p1 - F.p0 - e, SG_LH, F.pf);
            if (e) {
                cv_rect(F.p0, y + 2, 1, SG_LH - 4, F.pf);
                cv_rect(F.p0 + 1, y + 1, 1, SG_LH - 2, F.pf);
            }
        }
    }
    if (cur) {                                         /* the cell selected */
        int32_t cx = SG_X0 + L->x[c] - off, cw = L->x[c + 1u] - L->x[c] - SG_GAP;
        F.c0 = cx > SG_X0 ? cx : SG_X0;
        F.c1 = cx + cw < hi ? cx + cw : hi;
        if (F.c1 > F.c0)
            cv_rrect(F.c0, y, F.c1 - F.c0, SG_LH, j - i == 1u && F.c0 == cx && F.c1 == cx + cw ? 2 : 0, T_THEME, T_SURF);
    }
    lx = x0 + SG_PAD;                                  /* the letters: the block's, the cell's (when it starts later) */
    if (!(F.c1 > F.c0 && lx >= F.c0 && lx < F.c1) && (w = sg_letter(lx, F.c1 > F.c0 && F.c0 > lx ? F.c0 : x1, y, s, T_TEXT, sg_fill_at(&F, lx)))) {
        skip[0] = lx - 1;
        skip[1] = lx + w + 2;
    }
    if (F.c1 > F.c0 && (w = sg_letter(F.c0 + SG_PAD, F.c1, y, s, T_INK, T_THEME))) {
        skip[2] = F.c0 + SG_PAD - 1;
        skip[3] = F.c0 + SG_PAD + w + 2;
    }
    sg_picture(L, off, k, i, j, s, x0, x1, &F, skip);
}
/* Elektron-style dials (EDIT / FILTER family pages): four big knobs over the cards, centred on their cards, a 270 degree
 * arc that fills to the value, a pointer, and the parameter's short name under each. */
static int32_t dial_sin(int32_t deg)                     /* sin * 256, deg 0..359 */
{
    static const int16_t Q[7] = {0, 66, 128, 181, 222, 247, 256};
    int32_t neg = 0, i, f;
    if (deg >= 180) { deg -= 180; neg = 1; }
    if (deg > 90) deg = 180 - deg;
    i = deg / 15;
    f = deg % 15;
    i = Q[i] + (i < 6 ? (Q[i + 1] - Q[i]) * f / 15 : 0);
    return neg ? -i : i;
}
static void dial_pt(int32_t cx, int32_t cy, int32_t r, int32_t deg, int32_t *x, int32_t *y)
{
    deg %= 360;
    *x = cx + (r * dial_sin(deg) + 128) / 256;           /* 0 deg = up, clockwise */
    *y = cy - (r * dial_sin((deg + 90) % 360) + 128) / 256;
}
/* the pages drawn as dials: EDIT / FILTER family pages that have no graph of their own */
static int dial_page(void)
{
    const page_t *pg;
    const track_t *t;
    if (ui.home || large_kind() != LK_OFF)
        return 0;
    pg = cur_page();
    t = TSEL;
    if ((pg->graph != GR_NONE && pg->graph != GR_ADSR && pg->graph != GR_LFO) ||
        (pg->fam != FAM_EDIT && pg->fam != FAM_FLT && pg->fam != FAM_ENV && pg->fam != FAM_LFO && pg->fam != FAM_FX))
        return 0;
    (void)t;
    return 1;
}
static void graph_adsr(const track_t *t, uint16_t c);
static void graph_lfo(const track_t *t, uint16_t c);
/* PERF: row 1 the 8 macros (filled = something in it, held ones inverted), row 2 the 4 fade times then the 4 beat repeats; the
 * footer / header says the track, the held mode, FUNC and ARP */
static void graph_pm(void)
{
    static const char *const FADE_N[4] = {"1/16", "1/8", "BAR", "4BAR"};
    static const char *const REP_N[4] = {"1/8", "1/16", "1/32", "REV"};
    static const char *const FUNC_N[3] = {"HOLD", "CYCLE", "PING"};
    static const char *const ARP_N[5] = {"UP", "DOWN", "UPDN", "RAND", "ORDER"};
    uint32_t i, j;
    char nb[8], tb[24];
    for (i = 0; i < 16u; i++) {
        int32_t x = 4 + (int32_t)(i & 7u) * 29, y = 26 + (int32_t)(i >> 3) * 44;
        int held = 0, sel = 0;
        uint32_t n = 0;
        const char *top = 0, *cap = 0;
        if (i < PM_NM) {
            n = pm_mac[i].n;
            for (j = 0; j < pm_h.n; j++)
                held |= pm_h.mac[j] == i;
        } else if (i < PM_NM + 4u) {
            sel = pm_fade == i - PM_NM + 1u;
            top = FADE_N[i - PM_NM];
            cap = "FADE";
        } else {
            top = REP_N[i - PM_NM - 4u];
            cap = "RPT";
        }
        cv_rrect(x, y, 27, 38, 4, held || sel ? T_ACCENT : n ? T_RAISE : T_SURF, T_SURF);
        cv_frame(x, y, 27, 38, held || sel ? T_ACCENT : T_LINE);
        if (top) {
            cv_text_in(x, y + 6, 27, &AF_S, top, sel ? T_INK : T_ACCENT, sel ? T_ACCENT : T_SURF);
            cv_text_in(x, y + 24, 27, &AF_S, cap, sel ? T_INK : T_MID, sel ? T_ACCENT : T_SURF);
            continue;
        }
        fmt_int(nb, (int32_t)i + 1);
        cv_text_in(x, y + 3, 27, &AF_M, nb, held ? T_INK : n ? T_ACCENT : T_MID, held ? T_ACCENT : n ? T_RAISE : T_SURF);
        if (n) {
            fmt_int(nb, (int32_t)n);
            cv_text_in(x, y + 24, 27, &AF_S, nb, held ? T_INK : T_MID, held ? T_ACCENT : T_RAISE);
        }
    }
    tb[0] = 'T'; tb[1] = 'R'; tb[2] = 'K'; tb[3] = ' ';
    fmt_int(tb + 4, (int32_t)song.sel + 1);
    cv_text_in(4, 4, 80, &AF_M, tb, T_ACCENT, T_SURF);
    if (pm_h.n == 1u)
        cv_text_in(84, 8, 150, &AF_S, "TURN KNOBS  OCT-: CLEAR", T_MID, T_SURF);
    else if (pm_h.n == 2u)
        cv_text_in(84, 8, 150, &AF_S, "FADE BETWEEN", T_ACCENT, T_SURF);
    else if (pm_h.n > 2u)
        cv_text_in(84, 8, 150, &AF_S, "ARP", T_ACCENT, T_SURF);
    else {                                                  /* the two knobs: K1 FUNC, K2 ARP */
        uint32_t o = 0, k;
        const char *s = "F:";
        for (k = 0; s[k]; k++) tb[o++] = s[k];
        for (k = 0; FUNC_N[pm_func % 3u][k]; k++) tb[o++] = FUNC_N[pm_func % 3u][k];
        tb[o++] = ' '; tb[o++] = 'A'; tb[o++] = ':';
        for (k = 0; ARP_N[pm_arp % 5u][k]; k++) tb[o++] = ARP_N[pm_arp % 5u][k];
        tb[o] = 0;
        cv_text_in(84, 8, 150, &AF_S, tb, T_MID, T_SURF);
    }
}

static void graph_dials(const track_t *t, uint16_t c)
{
    uint32_t k;
    uint32_t g = cur_page()->graph;
    int gr = g == GR_ADSR || g == GR_LFO;                /* ENV / LFO pages: the dials above, the curve below */
    int32_t oy0 = cv_oy;
    int um = 0;                                          /* one value face for the page: M if any value is too wide for L */
    for (k = 0; k < 4u; k++) {
        int16_t *vq;
        const param_desc_t *dq = page_desc(cur_page(), k, &vq);
        char vb[12];
        const char *uq;
        if (!dq || !dq->label || dq->label[0] == '-')
            continue;
        param_format(dq, *vq, vb, &uq);
        if (text_w(&AF_L, vb) > 44)
            um = 1;
    }
    for (k = 0; k < 4u; k++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), k, &vp);
        int32_t cx = CARD_X(k) + CARD_W / 2, cy = gr ? 34 : 50, R = gr ? 23 : 26, a, x, y, ratio, on, i;
        int hot = k == ui.hot_col && ui.hot_t, inv = 0;
        uint16_t fc = hot ? T_ACCENT : c;
        const aafont_t *vf = &AF_L;
        const char *unit;
        char ab[6], val[12];
        if (!d || !d->label || d->label[0] == '-')
            continue;
        if (pm_altered(t, vp)) {                       /* PERF: a value this macro holds: inverted, boxed */
            cv_rrect(CARD_X(k), gr ? 2 : 18, CARD_W, gr ? 104 : 122, 5, T_ACCENT, T_SURF);
            fc = T_INK;
            inv = 1;
        }
        ratio = RATIO(d, enum_rank(d, *vp));
        if (ratio < 0) ratio = 0;
        on = ratio * 270 / 1000;
        for (a = 0; a <= 270; a += 3) {                  /* the track, then the value over it */
            dial_pt(cx, cy, R, 225 + a, &x, &y);
            cv_rect(x - 1, y - 1, 3, 3, a <= on ? fc : inv ? T_MID : T_RAISE);
        }
        dial_pt(cx, cy, R - 6, 225 + on, &x, &y);        /* the pointer */
        cv_line_t(cx, cy, x, y, inv ? T_INK : hot ? T_ACCENT : T_MID, 2);
        cv_rect(cx - 2, cy - 2, 5, 5, inv ? T_INK : hot ? T_ACCENT : T_MID);
        for (i = 0; i < 4 && d->label[i]; i++)           /* the short name, 4 letters */
            ab[i] = d->label[i];
        ab[i] = 0;
        cv_text_in(cx - CARD_W / 2, gr ? 62 : 92, CARD_W, &AF_M, ab, inv ? T_INK : hot ? T_ACCENT : T_MID, inv ? T_ACCENT : T_SURF);
        param_format(d, *vp, val, &unit);                /* the value under the name, the face that fits */
        if (gr || um) vf = &AF_M;
        if (text_w(vf, val) > CARD_W - 2) vf = &AF_S;
        cv_text_in(cx - CARD_W / 2, gr ? 84 : 124, CARD_W, vf, val, fc, inv ? T_ACCENT : T_SURF);
    }
    if (gr) {                                            /* the curve in the strip under the dials (y 112 .. 170) */
        cv_oy = oy0 + 112 + 3;
        graph_ht = 52;
        if (g == GR_ADSR) graph_adsr(t, c);
        else graph_lfo(t, c);
        graph_ht = 100;
        cv_oy = oy0;
    }
}

static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
    if (ui.home && home_tracks()) {                  /* MENU > HOME TRACKS (1.3) */
        draw_home_tracks();
        return;
    }
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    if (dial_page()) {                               /* the canvas holds 124 rows, the dials' panel is 170: two halves */
        uint32_t g = pg->graph, half = g == GR_ADSR || g == GR_LFO ? 112u : graph_h() / 2u, hi;
        for (hi = 0; hi < 2u; hi++) {
            cv_begin(240, hi ? graph_h() - half : half, T_BG);
            cv_oy = -(int32_t)(hi * half);
            cv_rrect(3, 0, 234, (int32_t)graph_h(), 5, T_SURF, T_BG);
            cv_bg = T_SURF;
            graph_dials(t, c);
            cv_oy = 0;
            cv_blit(0, graph_y() + hi * half);
        }
        return;
    }
    cv_begin(240, graph_h(), T_BG);
    cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (strip_kind() != SK_NONE) {                   /* MENU > LARGE: the strip */
        uint32_t k = strip_kind();
        cv_oy = k == SK_ADSR || k == SK_LFO ? 3 : 0;
        graph_ht = LG_H_GRAPH - 6;
        if (k == SK_SCOPE) graph_scope(c);
        else if (k == SK_ADSR) graph_adsr(t, c);
        else if (k == SK_LFO) graph_lfo(t, c);
        else graph_title();
        graph_ht = 100;
    } else if (ui.home) {
        cv_oy = 0;
        graph_scope(c);
    } else {
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_LFO:
            graph_lfo(t, c);
            break;
        case GR_PERF:
            cv_oy = 0;
            graph_pm();
            break;
        default:
            if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && t->eng_req % NENGINES == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
            else if (dial_page()) { cv_oy = 0; graph_dials(t, c); }
            else { cv_oy = 0; graph_scope(c); }
            break;
        }
    }
    cv_oy = 0;
    cv_blit(0, graph_y());
}
