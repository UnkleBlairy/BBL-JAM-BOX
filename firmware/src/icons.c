/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Icons: 4-bit alpha cells of the Fukiai icon font (MIT), Huffman-coded (cv_alpha_hc), 12 px (parameters, lists) and 16 px
 * (header, dialogs, menu): web/fukiai.ttf -> build/gen/ui_icons.h
 * (tools/gen_aa_icons.py; names from assets/icons.json). Which icon a parameter gets is decided
 * here, by its label. FELUCCA_ICONS=0 turns the parameter icons off (labels get their full width back). */
#include "ui_icons.h"
#ifndef FELUCCA_ICONS
#define FELUCCA_ICONS 1
#endif
#define ICON_CELL 12

/* track k's indicator (1.0.2): not an icon of the font (its numerals alias at 12 to 16 px) but the digit 1..4 in
 * Inter Tight on a cushion, a rounded square in the keycap's language, drawn by cv_icon_on: the cushion in the
 * icon's ink (the selected track's: the accent), the digit in what lies under it; rounded in every STYLE, as the
 * keycaps are. (filled: kept for the callers; every cushion is filled) */
#define ICON_TRK 0xF0u                                /* + k (above ICON_COUNT, below ICON_AUTO) */
static uint32_t trk_icon(uint32_t k, int filled) { (void)filled; return ICON_TRK + k % NTRK; }
static int32_t cv_trk(int32_t x, int32_t y, uint32_t size, uint32_t k, uint16_t fill, uint16_t under)
{
    const aafont_t *f = size > 16u ? &AF_M : &AF_S;
    char s[2] = {(char)('1' + k), 0};
    const aag_t *g = &f->g[glyph(f, (uint8_t)s[0])];
    int32_t in = size > 16u ? 2 : 0, w = (int32_t)size - 2 * in;
    int32_t nud = k == 0u ? 1 : 0;   /* the 1's ink centred looks right of centre (its flag): nudged left 1 px (2 looked left) */
    cv_rrect(x + in, y + in, w, w, -(w / 4), fill, under);
    GFX_HOOK_ALIGN(x + in - nud, y + in, x + in + w - nud, y + in + w, AL_HV, "track cushion digit");   /* (the 1: measured against its nudge) */
    cv_text_on(x + in + (w - g->bw) / 2 - g->bx - nud, y + in + (w - g->bh) / 2 - g->by, f, s, under, fill);   /* its ink centred */
    GFX_HOOK_ITEM(x + in, y + in, x + in + w, y + in + w);
    return (int32_t)size;
}

/* icon id's cell number at *size 12, 16 or 24 px (missing at 16 or 24: its 12 px cell, *size 12), -1: none */
static int32_t icon_cell(uint32_t *size, uint32_t id)
{
    const uint8_t *idx = *size == 24u ? AI24_IDX : *size == 16u ? AI16_IDX : AI12_IDX;
    if (id >= ICON_COUNT)
        return -1;
    if (idx[id] == 0xFFu) {
        if (*size == 12u || AI12_IDX[id] == 0xFFu)
            return -1;
        *size = 12u;
        idx = AI12_IDX;
    }
    return idx[id];
}
/* the ink box of icon id drawn at (0, 0) in a size px cell: b = x0 y0 x1 y1 (past the ink), from the generator's
 * table (tools/gen_aa_icons.py AI*_INK); a cushion: the cushion; nothing: the cell */
static void icon_ink(uint32_t size, uint32_t id, int32_t *b)
{
    uint32_t sz = size, v, i;
    int32_t k = id - ICON_TRK < NTRK ? -1 : icon_cell(&sz, id), in = id - ICON_TRK < NTRK && size > 16u ? 2 : 0;
    if (k < 0) {
        b[0] = b[1] = in;
        b[2] = b[3] = (int32_t)size - in;
        return;
    }
    if (sz == 24u) {
        v = AI24_INK[k];                            /* x0 y0 x1 y1: a byte each */
    } else {                                        /* 12 16: a nibble each, x1-1 y1-1: as the 24's */
        v = sz == 16u ? AI16_INK[k] : AI12_INK[k];
        v = ((v & 15u) | (v & 0xF0u) << 4 | (v & 0xF00u) << 8 | (v & 0xF000u) << 12) + 0x01010000u;
    }
    for (i = 0; i < 4u; i++)
        b[i] = (int32_t)(v >> (8u * i) & 255u);
}

/* icon id at size 12, 16 or 24 px (24: the header battery and USB, the FX map; one missing: its 12 px cell),
 * fg on bg; returns its width */
static int32_t cv_icon_on(int32_t x, int32_t y, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    int32_t k;
    if (id - ICON_TRK < NTRK)
        return cv_trk(x, y, size, id - ICON_TRK, fg, bg);
    if ((k = icon_cell(&size, id)) < 0)
        return 0;
    cv_alpha_hc(x, y, size, size, size == 24u ? AI24_DATA + AI24_OFF[k] : size == 16u ? AI16_DATA + AI16_OFF[k] :
                AI12_DATA + AI12_OFF[k], AI_HC, ramp(fg, bg));
    GFX_HOOK_ICON(x, y, size, id);
    GFX_HOOK_TEXT(x, y + cv_oy, x + (int32_t)size, y + cv_oy + (int32_t)size, "icon", 4u);
    return (int32_t)size;
}
/* the same, its ink centred across x .. x + w (w 0: the cell at x) and up and down in y .. y + h (h 0: the cell at y);
 * a pixel left over goes left / up */
static int32_t cv_icon_in(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    int32_t b[4];
    icon_ink(size, id, b);
    if (w)
        x += HALF_UP(w - (b[2] - b[0])) - b[0];
    if (h)
        y += HALF_UP(h - (b[3] - b[1])) - b[1];
    return cv_icon_on(x, y, size, id, fg, bg);
}
/* the same, its ink centred on the line cy (between rows cy - 1 and cy: the middle of rows 0 .. 2 cy - 1, the odd row
 * left over going up; header icons: each glyph sits differently in its cell) */
static int32_t cv_icon_mid(int32_t x, int32_t cy, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    GFX_HOOK_ALIGN(0, 0, 0, 2 * cy, AL_V | AL_PASS, "header icon on its middle");
    return cv_icon_in(x, 0, 0, 2 * cy, size, id, fg, bg);
}
#define ICON_NONE 0xFFu                   /* no icon (empty column) */
#define ICON_AUTO 0xFEu                   /* draw_column: look the label up */
#define ICON_GAP 2                        /* px between the icon and the label */

#if FELUCCA_ICONS
typedef struct {
    const char *label;
    uint8_t icon;
} icon_map_t;

/* label -> icon. Labels shared by several parameters (RATE, WAVE, SET) are
 * told apart by descriptor in param_icon(). Unknown labels get ICON_GENERIC. */
static const icon_map_t ICON_MAP[] = {
    /* track: ENV, LFO, destinations */
    {"LVL", ICON_LEVEL}, {"ATK", ICON_ATTACK}, {"DEC", ICON_DECAY}, {"SUS", ICON_SUSTAIN},
    {"REL", ICON_RELEASE}, {"FLT", ICON_CUTOFF}, {"PIT", ICON_PITCH}, {"SHP", ICON_SHAPE},
    {"FX", ICON_MOD}, {"RATE", ICON_RATE}, {"WAVE", ICON_WAVE}, {"PHS", ICON_PHASE},
    {"FADE", ICON_FADE}, {"AMP", ICON_LEVEL},
    /* arp, scale, pattern */
    {"MODE", ICON_ARP}, {"OCT", ICON_OCTAVE}, {"GATE", ICON_GATE}, {"SWG", ICON_SWING},
    {"PROB", ICON_PROB}, {"HOLD", ICON_HOLD}, {"ORD", ICON_ORDER}, {"ROOT", ICON_PITCH},
    {"SCL", ICON_SCALE}, {"QNT", ICON_QUANTIZE}, {"TRN", ICON_TRANSPOSE}, {"LEN", ICON_LENGTH},
    {"DIV", ICON_DIVISION}, {"VOIC", ICON_OCTAVE},   /* (CHRD: below, PHYS's) */
    /* fx sends, voice */
    {"DST", ICON_DIST}, {"CHO", ICON_CHORUS}, {"DLY", ICON_DELAY}, {"REV", ICON_REVERB},
    {"VCE", ICON_VOICE}, {"GLD", ICON_GLIDE}, {"GLMOD", ICON_GLIDE}, {"PRIO", ICON_ORDER},
    {"ALLOC", ICON_VOICE}, {"DTUNE", ICON_DETUNE}, {"PAN", ICON_PAN}, {"MUTE", ICON_MUTE},
    /* global */
    {"BPM", ICON_TEMPO}, {"CLK", ICON_TEMPO}, {"TUNE", ICON_TUNE}, {"TIME", ICON_TIME},
    {"FDBK", ICON_FEEDBACK}, {"COLR", ICON_TONE}, {"MIX", ICON_MIX}, {"SIZE", ICON_SIZE},
    {"DAMP", ICON_DAMP}, {"TYPE", ICON_REVERB}, {"CRT", ICON_RATE}, {"CDP", ICON_MOD}, {"MIDI", ICON_MIDI},
    {"SYNC", ICON_TEMPO}, {"ROUT", ICON_MIX}, {"CPU", ICON_CHIP}, {"SLOT", ICON_SAVE},
    {"LOAD", ICON_LOAD}, {"SAVE", ICON_SAVE}, {"ENG", ICON_WAVE}, {"CLRSQ", ICON_CLEAR},
    {"INIT", ICON_CLEAR}, {"ERASE", ICON_CLEAR}, {"CH", ICON_MIDI}, {"LEVEL", ICON_LEVEL},
    /* engines (eng_*.c edit[] labels) */
    {"DTN", ICON_DETUNE}, {"NOIS", ICON_NOISE}, {"CUT", ICON_CUTOFF}, {"RES", ICON_RESO},
    {"DRV", ICON_DRIVE}, {"KTR", ICON_KEYTRACK}, {"ALG", ICON_ALGORITHM}, {"R2", ICON_RATIO},
    {"R3", ICON_RATIO}, {"R4", ICON_RATIO}, {"IDX", ICON_MOD}, {"MDEC", ICON_DECAY},
    {"FB", ICON_FEEDBACK}, {"CHIP", ICON_CHIP}, {"DUTY", ICON_PULSE}, {"CRSH", ICON_BITS},
    {"SWP", ICON_SWEEP}, {"VIB", ICON_VIBRATO}, {"ARP", ICON_ARP}, {"TONE", ICON_TONE},
    {"SET", ICON_SAMPLE}, {"BITS", ICON_BITS}, {"LOOP", ICON_LOOP}, {"WAVE2", ICON_WAVE},
    {"DCW", ICON_SHAPE}, {"ENV", ICON_ENV}, {"LINE", ICON_MIX}, {"SUB", ICON_SUB},
    {"FOLD", ICON_FOLD}, {"WAV#", ICON_WAVE}, {"DCY", ICON_DECAY}, {"INT2", ICON_TRANSPOSE},
    {"INT3", ICON_TRANSPOSE}, {"PW", ICON_PULSE},
    {"REG", ICON_DRAWBAR}, {"BODY", ICON_LEVEL}, {"TOP", ICON_TONE}, {"PERC", ICON_DECAY},
    {"CLICK", ICON_ATTACK}, {"ROTR", ICON_VIBRATO},
    {"SRC", ICON_SAMPLE}, {"START", ICON_STEPS}, {"PTCH", ICON_PITCH}, {"DCAY", ICON_DECAY},
    {"POS", ICON_PHASE}, {"DENS", ICON_GRAIN}, {"SPRD", ICON_NOISE},
    {"MODEL", ICON_PHYS}, {"STRC", ICON_SHAPE}, {"BRIT", ICON_TONE}, {"BOW", ICON_SUSTAIN}, {"EXC", ICON_MIX},   /* PHYS */
    {"HARM", ICON_RATIO}, {"BEND", ICON_SWEEP}, {"CHRD", ICON_SCALE}, {"SYMP", ICON_MIX}, {"DECY", ICON_DECAY},
    {"SNAP", ICON_NOISE}, {"KIT", ICON_SAMPLE}, {"KICK", ICON_ATTACK},       /* PHYS: MEMB, SYMP; DRUM */
    {"VOWL", ICON_VOICE}, {"VOWL2", ICON_VOICE}, {"TALK", ICON_SWEEP}, {"SHIFT", ICON_TRANSPOSE},
    {"BUZZ", ICON_PULSE}, {"BRTH", ICON_NOISE}, {"Q", ICON_RESO}, {"RAND", ICON_PROB},
    {"FREQ", ICON_CUTOFF}, {"TRK", ICON_KEYTRACK}, {"DRFT", ICON_SWEEP},   /* NOISE (COLR, DENS, CRSH: above) */
    {"MLVL", ICON_MOD}, {"MRAT", ICON_RATIO}, {"MEG", ICON_DECAY}, {"VMOD", ICON_ACCENT}, {"DTUN", ICON_DETUNE},
    {"SLOT", ICON_LOAD},                                                    /* FM6 (ALG, FB: above) */
    /* fixed columns drawn by ui_draw.c (STEP page, preset browser) */
    {"NOTE", ICON_PITCH}, {"STEP", ICON_STEPS}, {"FLAG", ICON_ACCENT}, {"ACC", ICON_ACCENT}, {"LANE", ICON_DRUM}, {"HIT", ICON_GATE}, {"SLD", ICON_SLIDE},
    {"TRACK", ICON_MIX},                  /* TRACKS page (LEVEL, LEN, PAN: above) */
    {"SLCR", ICON_SLICE}, {"PAT", ICON_STEPS}, {"DEPTH", ICON_MIX},   /* SLICER page (RATE: param_icon) */
};


/* WAVE / WAVE2 set to a shape: that shape's icon (value names of the engines and the LFO) */
static const icon_map_t WAVE_ICON[] = {
    {"SIN", ICON_W_SIN}, {"TRI", ICON_W_TRI}, {"SAW", ICON_W_SAW}, {"SQR", ICON_W_SQR},
    {"PLS", ICON_W_PLS}, {"PWM", ICON_W_PWM}, {"S&H", ICON_W_SH}, {"NOIS", ICON_NOISE},
    {"DSIN", ICON_W_DSIN}, {"SPLS", ICON_W_SPLS}, {"RSAW", ICON_W_RSAW}, {"RTRI", ICON_W_RTRI},
    {"RTRP", ICON_W_RTRP},
};






#else
static uint32_t icon_for_label(const char *l) { (void)l; return ICON_NONE; }
static uint32_t param_icon(const param_desc_t *d, int32_t v) { (void)d; (void)v; return ICON_NONE; }
static uint32_t engine_icon(const char *name) { (void)name; return ICON_NONE; }
static uint32_t mod_src_icon(int32_t s) { (void)s; return ICON_NONE; }
static uint32_t mod_dst_icon(const track_t *t, int32_t d) { (void)t; (void)d; return ICON_NONE; }
static uint32_t motion_icon(void) { return ICON_NONE; }
static uint32_t page_icon(const page_t *pg) { (void)pg; return ICON_NONE; }
#endif
