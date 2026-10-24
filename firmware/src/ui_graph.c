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


static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}







/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17

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
/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint16_t graph_plen[4][NTRK];                  /* .. each track's pattern length (project_info: 1/48 quarters) */
static uint32_t graph_pden[4][NTRK];                  /* .. and its picture for SONG (project.c proj_density: 2 bits a cell) */
static uint8_t graph_pcell[4][NTRK];                  /* .. in this many cells (min(LEN, 16)) */
static uint32_t graph_pname_sig;

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

/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_TRK, SK_TITLE };


/* a list row (17 px), in M as every list's (PRESETS, USER, PHRASES, PROJECT, AUTOMATION, MOD, the MENU): the selected
 * one a THEME bar with INK text; tag at x 14 (the widest, DRUM, to x 57), free text from x LIST_NX to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
#define LIST_TY CAP_IN(M, 16)                           /* a row's text: its capitals rows 2 .. 12 of the row */
#define LIST_NX 64
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */

/* the AUTOMATION rows' columns (M): the kind at 14 (LOCK AUTO STEP PLAY, to x 54), the step / PLAY's ON at EV_SX, the
 * name / QUANTIZE's ON at EV_NX (QUANTIZE ends at x 86), the value right at 226 */
#define EV_SX 60
#define EV_NX 92
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


/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384



/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
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
/* one lane's drawing state: the screen spans of the cell selected and of the playing section's part played */
typedef struct { int32_t c0, c1, p0, p1; uint16_t pf; } sg_fill_t;
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


