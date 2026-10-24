/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: the header, the four knob cards, the footer (steps + engine / preset / page)
 * and the frame; the panel between the cards and the footer is ui_graph.c. The layout:
 * flat SURF cards and panels on BG,
 * rounded corners, no rules (MENU > STYLE LINE: no SURF, 1 px rules between the areas instead: draw_gaps), colours from the theme tokens only (gfx.c T_*).
 * Type, by role: M (15 px) what is read: values, titles, messages, every list's rows, the
 * selected item's detail; S (12 px) what describes it: labels, units, key hints (a keycap's word), the footer, map
 * and layer cells, graph annotations, document text; L (28 px) the one big readout of a screen. Where M cannot fit
 * (a card value with its unit wider than the card, a dialog question or a header message too long) it is set in S;
 * free text (names) is ellipsised at its size. */
static void draw_menu(void);
static int name_on(void);                              /* NAME (ui_name.c) */
static uint64_t lock_held(void);                       /* parameter locks: the steps held on STEP (ui_input.c) */
static void name_draw(void);

/* --------------------------------------------------------- drawing --- */
#define COL_W CARD_W                                  /* a card: 57 x 44 at x 3 + 59 c, y 28 */
#define COL_H CARD_H

static int32_t batt_level(void)                         /* thresholds 531 / 561 / 591 on ADC ch3 */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* A USB host powers us; there is no separate charger status line.
 * 4 means external power (a steady bolt inside the battery), otherwise its level. */
static int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 4;
    return batt_level();
}

/* REC: filled, the selected track armed; outlined, another track armed */
static void draw_rec_mark(int32_t x, uint16_t bg)
{
    if (song.rec)
        cv_icon_mid(x, H_HEAD / 2, 16, (song.rec >> song.sel) & 1u ? ICON_X_REC : ICON_X_REC_O, T_REC, bg);
}

/* the battery: a 16 px Fukiai icon. The stock thresholds give 0..3 bars of 3; the font has 0..4 bars of 4:
 * each level takes the nearest share, 0 -> battery_0 (empty), 1 (1/3) -> battery_1 (1/4), 2 (2/3) ->
 * battery_3 (3/4), 3 (full) -> battery_4 (battery_2 is not used); USB power: battery_charging.
 * Colours as the drawn battery had them: one bar left the accent, empty MID (its outline), else THEME */
static uint32_t batt_icon(int32_t lvl)
{
    static const uint8_t I[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
    return I[clamp(lvl, 0, 4)];
}
static void draw_battery(int32_t bx)
{
    int32_t lvl = batt_shown();
    cv_icon_mid(bx, H_HEAD / 2, 24, batt_icon(lvl), lvl == 1 ? T_ACCENT : lvl == 0 ? T_MID : T_THEME, T_BG);   /* 24 px: the glyph is wide and short */
}

/* ---------------------------------------------------- rolling digits --- */
/* A number that changes rolls its changed digits like a slot machine; only the drawing: the value, the gauge,
 * the hot colour and the sound change at once. Up: the old digit leaves upward and the new one comes from below;
 * down: the reverse. ROLL_FRAMES UI frames (ui.frame, ~135 ms) on an integer ease-out (ROLL_EASE: cubic, over
 * half-way in a quarter of the time, no overshoot); the ones digit first, each place one frame later, all ending
 * together on a frame equal to the static render. Clipped to the value strip, which alone is redrawn while it
 * rolls (a card: rows ROLL_Y.., the header: the BPM's columns). Characters other than digits never roll.
 * A change during a roll retargets it: it restarts from the value it was going to. Shown at once (no roll):
 * a change within ROLL_SNAP frames of the last one (a fast turn reads better as plain numbers), another
 * shape (length, sign, a non-digit, a name or an enum, a value set in S), another label or unit, a track /
 * engine / palette change (ui.roll[].sig), ui.force (a page change), a card's first draw; MENU > ANIM OFF (#46). */
#define ROLL_FRAMES 9u
#define ROLL_SNAP 3u                                    /* frames (~45 ms) */
#define ROLL_BPM 4u                                     /* ui.roll[]: the four cards, then the header BPM */
#define ROLL_Y 18                                       /* a card's value strip: rows 18..34, its figures (21..31) in the middle, */
#define ROLL_H 17                                       /* the label (MONO's chip: to row 17) above it, the gauge (38) below */
static const uint8_t ROLL_EASE[17] = {0, 45, 84, 118, 147, 172, 193, 210, 223, 234, 242, 247, 251, 253, 254, 255, 255};

static int roll_digit(char c) { return c >= '0' && c <= '9'; }
/* +1 / -1: b rolls in from a (same length, digits where a has digits, the rest equal and one of + - .),
 * the sign of b - a; 0: b is shown at once */
static int roll_dir(const char *a, const char *b)
{
    uint32_t i;
    int d = 0, neg = 0;
    for (i = 0; a[i] || b[i]; i++) {
        if (i + 1u >= sizeof ui.roll[0].from || roll_digit(a[i]) != roll_digit(b[i]))
            return 0;
        if (!roll_digit(a[i]) && (a[i] != b[i] || (a[i] != '-' && a[i] != '+' && a[i] != '.')))
            return 0;
        neg |= a[i] == '-';
        if (!d && a[i] != b[i])
            d = b[i] > a[i] ? 1 : -1;
    }
    return neg ? -d : d;
}
/* field k now shows b instead of a: roll (a retarget mid-roll), or snap */
static void roll_note(uint32_t k, const char *a, const char *b, int snap)
{
    uint32_t el = (uint8_t)(ui.frame - ui.roll[k].t0);
    int d = roll_dir(a, b);
    ui.roll[k].t0 = (uint8_t)ui.frame;
    ui.roll[k].from[0] = 0;
    if (snap || !d || el < ROLL_SNAP || (ui_prefs & PREF_ANIM_OFF))
        return;
    str_cpy(ui.roll[k].from, a, sizeof ui.roll[k].from);
    ui.roll[k].dir = (int8_t)d;
}
/* s (M) at x, y with field k's roll: each character at its pen in s (the digits are tabular and never kerned,
 * so the old value has the same pens); a rolling digit has moved o of the strip's h rows (ROLL_EASE, one frame
 * later per place from the right), clipped to the strip. The roll's last frame clears it: the static text. */
static int32_t roll_text(uint32_t k, int32_t x, int32_t y, const char *s, uint16_t fg)
{
    const aafont_t *f = &AF_M;
    uint32_t e = (uint8_t)(ui.frame - ui.roll[k].t0) + 1u, i, p = 0;
    const char *a = ui.roll[k].from;
    int32_t cy = k < ROLL_BPM ? ROLL_Y : 0, h = k < ROLL_BPM ? ROLL_H : H_HEAD;
    uint16_t bg = k < ROLL_BPM ? T_SURF : T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + cv_oy);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + cv_oy + h);
    cv_scroll = 1;                                      /* (the lint: rolling digits are cut on purpose) */
    for (i = 0; s[i]; i++) {
        int32_t px, ny = y;
        t[i] = 0;
        px = x + text_w(f, t);                          /* (its pen: no digit kerns) */
        t[i] = s[i];
        p -= (uint32_t)roll_digit(s[i]);
        if (s[i] != a[i]) {                             /* p: the places right of it */
            int32_t o = 0, dir = ui.roll[k].dir;
            if (e > p)                                  /* (e < ROLL_FRAMES: the LUT index stays in 1..16) */
                o = (int32_t)((ROLL_EASE[((e - p) * 32u / (ROLL_FRAMES - p) + 1u) >> 1] * (uint32_t)h + 128u) >> 8);
            ch[0] = a[i];
            cv_text_on(px, y - dir * o, f, ch, fg, bg);
            ny = y + dir * (h - o);
        }
        ch[0] = s[i];
        cv_text_on(px, ny, f, ch, fg, bg);
    }
    cv_cy0 = 0;
    cv_cy1 = (int16_t)cv_h;
    cv_scroll = 0;
    return x + text_w(f, s);
}

/* the header (y 0..24), three calm slots (1.2):
 *   left    the selected track's cushion (ink 4..20), the PROJECT slot letter of its pattern ("1 A", head_slot), the REC
 *           mark attached (filled: it is armed; outlined: another). A message centred hides the letter (its room)
 *   centre  the play state (stop, play, the song's disc), the metronome and the BPM (BPM LOCK ON: a lock right of
 *           it), the group's ink centred on x 120 (head_geo). The metronome swings on the click's beats (seq.c click_beat_now): mirrored
 *           on the bar's 2nd and 4th, the accent's on its 1st; stopped, still and MID
 *   right   the octave, or the song row while a song plays (ending at 208), and the battery (ink 216..236; its bolt
 *           says USB power: no separate USB icon)
 * A message or a layer's label replaces the centre and the octave, centred on x 120, up to
 * 166 px; while the BPM is being changed (a SELECT turn: bpm_t) or in the GLO layer the centre stays and the message
 * goes right of the BPM (over the right slot). Every element's ink centred on row 12 (H_HEAD / 2; ui_test.c
 * test_head_centres): icons by cv_icon_mid, text by its capitals' band (CAP_IN; the odd row left over above it, as
 * everywhere): M (BPM, the song row, the octave, a message: capitals rows 6..16) from y 2, S (OCT, a key hint: rows 7..15)
 * from y 4 */
#define HEAD_SY CAP_IN(S, H_HEAD)
#define HEAD_MY CAP_IN(M, H_HEAD)
#define HEAD_TRK_X 4                                   /* the track cushion's cell */
#define HEAD_REC_X 20                                  /* the REC mark's cell (ink 23..33), after the slot letter if any */
#define HEAD_SLOT_X 23                                 /* the slot letter's pen (ui_draw.c head_slot) */
#define HEAD_BAT_X 214                                 /* the battery's cell */
#define HEAD_GRP_R 208                                 /* the octave / song row ends here */
#define HEAD_MSG_W 166                                 /* a message centred on x 120: 37..203 (after the REC mark) */
#define HEAD_GAP 4                                     /* ink to ink: the metronome -> the BPM -> the lock */
#define HEAD_PLAY_GAP 6                                /* the play state -> the metronome */
/* the header's centre (1.5.6): the play state, the metronome and the BPM (BPM LOCK ON: its lock) as one group, its
 * ink (gx0 .. gx1, past it) centred on x 120 (before: the BPM's alone, the rest hanging off to the left). The play
 * state's slot (px0 .. px1) is as wide as the wider of stop and play, so the group stays put on PLAY / STOP (the
 * narrower ink right-aligned in it, its gap to the metronome kept; a song's disc reaches a few px further left). The
 * digits are placed by as many 0s as the BPM has digits (tabular figures: the same advance), so nothing moves while
 * the tempo changes, only with a digit more or less (or BPM LOCK turned). bx: the pen of the BPM, b: its ink; the
 * metronome's ink (mx0 .. mx1, past it); the BPM's rolling strip (sx0 .. sx1: between the metronome's cell, either
 * way round, and the lock's) */
typedef struct { int32_t bx, b[4], mx0, mx1, sx0, sx1, px0, px1, gx0, gx1; } head_geo_t;
static void head_geo(head_geo_t *g, const char *bpm)
{
    int32_t ib[4], lb[4], pb[4], c = 16, pw, lw = 0;
    uint32_t sz = 16;
    char z[8] = "0000000";
    z[str_len(bpm) < 7u ? str_len(bpm) : 7u] = 0;
    text_ink(&AF_M, z, g->b);
    icon_ink(16, ICON_TEMPO, ib);
    if (icon_cell(&sz, ICON_TEMPO) >= 0)
        c = (int32_t)sz;
    icon_ink(16, ICON_X_STOP, pb);                     /* the play state's slot: the wider of stop and play */
    pw = pb[2] - pb[0];
    icon_ink(16, ICON_X_PLAY, pb);
    if (pb[2] - pb[0] > pw)
        pw = pb[2] - pb[0];
    icon_ink(16, ICON_X_LOCK, lb);
    if (ui_prefs & PREF_BPM_LOCK)
        lw = HEAD_GAP + lb[2] - lb[0];
    g->gx0 = HALF_UP(240 - (pw + HEAD_PLAY_GAP + (ib[2] - ib[0]) + HEAD_GAP + (g->b[2] - g->b[0]) + lw));
    g->px0 = g->gx0;
    g->px1 = g->px0 + pw;
    g->mx0 = g->px1 + HEAD_PLAY_GAP;
    g->mx1 = g->mx0 + (ib[2] - ib[0]);
    g->bx = g->mx1 + HEAD_GAP - g->b[0];
    g->gx1 = g->bx + g->b[2] + lw;
    g->sx0 = g->mx1 + (ib[0] > c - ib[2] ? ib[0] : c - ib[2]);
    g->sx1 = g->bx + g->b[2] + HEAD_GAP - lb[0];
}
/* the metronome with its ink at x0 (beat: click_beat_now; the 2nd and 4th mirrored, the same ink box; the 1st the
 * accent's). Mirrored by cv_flip: the cell's alpha drawn right to left, no second glyph */
static void head_metro(int32_t x0, int32_t beat)
{
    int32_t ib[4], c = 16;
    uint32_t sz = 16;
    icon_ink(16, ICON_TEMPO, ib);
    if (icon_cell(&sz, ICON_TEMPO) >= 0)
        c = (int32_t)sz;
    cv_flip = (uint8_t)(beat > 0 && (beat & 1));
    cv_icon_mid(x0 - (cv_flip ? c - ib[2] : ib[0]), H_HEAD / 2, 16, ICON_TEMPO, beat == 0 ? T_ACCENT : T_MID, T_BG);
    cv_flip = 0;
}
/* the width cv_free_hint(x, .., s, .., maxw) draws (its word, keycap and the ellipsised rest) */
static int32_t head_hint_w(const char *s, int32_t maxw)
{
    uint32_t n, i;
    int32_t id = kc_tag(s, &n), w = 0;
    char b[48];
    for (i = 0; id < 0 && i < 7u && s[i] && s[i] != ' '; i++)
        ;
    if (id < 0 && i && s[i] == ' ' && (id = kc_tag(s + i + 1u, &n)) >= 0) {
        char wd[8] = {0};
        uint32_t j;
        for (j = 0; j < i; j++)
            wd[j] = s[j];
        w = text_w(&AF_S, wd) + KH_GAP;
        s += i + 1u;
    }
    if (id >= 0 && kc_w((uint32_t)id) + KH_GAP < maxw - w) {
        w += kc_w((uint32_t)id) + KH_GAP;
        s += n;
        while (*s == ' ')
            s++;
        if (!*s)
            return w - KH_GAP;
    }
    text_fit(b, sizeof b, s, &AF_S, maxw - w);
    return w + text_w(&AF_S, b);
}
/* a key hint: a keycap first, or after the first word ("[SAVE] HOLD TO UNDO", "HOLD [GLO] QUICK"; cv_free_hint) */
static int head_is_hint(const char *s)
{
    uint32_t n, i;
    if (kc_tag(s, &n) >= 0)
        return 1;
    for (i = 0; i < 7u && s[i] && s[i] != ' '; i++)
        ;
    return i && s[i] == ' ' && kc_tag(s + i + 1u, &n) >= 0;
}
/* a message / a layer's label at x, or centred on x 120 (x < 0), at most maxw px with its
 * icon (ui.c MSG_NOFILE: the accent's) and a locked layer's lock. A message in M (as the panel's titles), a key hint
 * (a layer's label, "[SAVE] HOLD TO UNDO") in S as every keycap's word; a message M cannot hold in maxw (right of the
 * BPM; a long name) in S, ellipsised there if it must be */
static void head_msg(int32_t x, int32_t maxw)
{
    const char *m = ui.msg_t ? ui.msg : "";
    int lock = 0;
    int icon = m[0] == MSG_NOFILE[0];
    int32_t lw = lock ? 15 : 0, iw = icon ? 16 + KH_GAP : 0;   /* the lock: its cell 6 px on, its ink to 15 */
    int big = !head_is_hint(m + icon) && text_w(&AF_M, m + icon) <= maxw - lw - iw;
    if (x < 0)
        x = 120 - HALF_UP(iw + (big ? text_w(&AF_M, m + icon) : head_hint_w(m + icon, maxw - lw - iw)) + lw);
    if (icon)
        x += cv_icon_mid(x, H_HEAD / 2, 16, ICON_X_NOFILE, T_ACCENT, T_BG) + KH_GAP;
    if (big) {
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_MY, &AF_M, m + icon, T_TEXT);
    } else {
        x = cv_free_hint(x, HEAD_SY, m + icon, T_TEXT, T_BG, maxw - lw - iw);   /* (may start with a keycap) */
    }
    if (lock)
        cv_icon_mid(x + 6, H_HEAD / 2, 16, ICON_X_LOCK, T_THEME, T_BG);
}
/* the slot letter after the track's cushion (1.2): the PROJECT slot of the pattern the selected track plays or edits.
 * A song playing: its section's slot for the track ("-" silent there); else the slot the working pattern came from
 * (ui.c pat_from: the last PROJECT load or save), with a mark ("A*") once it was edited since (its steps, LEN DIV SWING
 * GATE, QUANTIZE or automation: ui.c pat_from_sig); none at power-on, after a PHRASES pattern or the editor's restore.
 * b: the letter (b[1] '*' or 0); 0 = none */
static int head_slot(char *b)
{
    uint32_t k = song.sel % NTRK, s;
    b[1] = b[2] = 0;
    if (chain.running) {
        s = chain.lane[k];
        b[0] = s < 4u ? (char)('A' + s) : '-';
        return 1;
    }
    if (!pat_from[k] || pat_from[k] > 4u)
        return 0;
    b[0] = (char)('A' + pat_from[k] - 1u);
    if (pat_from_sig(TSEL) != pat_from_h[k])
        b[1] = '*';
    return 1;
}
/* the octave, or the song row while a song plays, its right end at HEAD_GRP_R */
static void head_group(void)
{
    char b[8];
    int32_t x;
    if (chain.running) {
        fmt_int(b, (int32_t)chain.row + 1);
        x = HEAD_GRP_R - (16 + 4 + text_w(&AF_M, b));
        x += cv_icon_mid(x, H_HEAD / 2, 16, ICON_X_SONG, T_MID, T_BG);   /* SONG: the disc */
    } else {
        str_cpy(b, song.octave > 0 ? "+" : "", sizeof b);
        fmt_int(b + str_len(b), song.octave);
        x = HEAD_GRP_R - (text_w(&AF_S, "OCT") + 4 + text_w(&AF_M, b));
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_SY, &AF_S, "OCT", T_MID);
    }
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    cv_text(x + 4, HEAD_MY, &AF_M, b, T_THEME);
}
static void draw_head(void)
{
    static int16_t metro_x = -1;                       /* the metronome's ink as drawn (-1: hidden), its beat */
    static int8_t metro_beat;
    char b[16];
    head_geo_t g;
    int32_t beat = -1;                                 /* the metronome stands still (its swing removed in 1.1.5) */
    uint32_t rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u;   /* 2 the selected track armed, 1 another */
    char sl[3];
    int slot = head_slot(sl);
    uint32_t sig = (uint32_t)song.playing * 3u + rec * 5u + (uint32_t)(song.octave + 8) * 11u + song.sel * 13131u +
                   (ui.msg_t ? str_hash(7u, ui.msg) : 0u) + (uint32_t)song.g[G_BPM] * 101u + (ui.bpm_t != 0) * 31u +
                   (uint32_t)seq_counting() * 7u + (ui_prefs & PREF_BPM_LOCK) * 4099u +
                   (uint32_t)batt_shown() * 7777u + (chain.running ? (chain.row + 1u) * 104729u : 0u) +
                   (slot ? str_hash(11u, sl) : 0u) + (uint32_t)midi_lit(song.sel) * 15485863u;   /* (1.5.5) */
    int msg = ui.msg_t;
    int centre = !msg || ui.bpm_t == LAYER_GLO;   /* the BPM being changed: it stays, the message right */
    int32_t rx = HEAD_REC_X;
    if (song.g[G_BPM] != ui.roll_bpm) {
        char a[8];
        fmt_int(a, ui.roll_bpm);
        fmt_int(b, song.g[G_BPM]);
        roll_note(ROLL_BPM, a, b, ui.force);
        ui.roll_bpm = song.g[G_BPM];
    } else if (ui.force) {
        ui.roll[ROLL_BPM].from[0] = 0;
    }
    if (!centre)                                       /* (a message over the BPM: no roll there to finish; CLOCK's */
        ui.roll[ROLL_BPM].from[0] = 0;                 /* BPM knob turned while one shows) */
    fmt_int(b, song.g[G_BPM]);
    head_geo(&g, b);
    if (!ui.force && sig == ui.head_sig) {
        if (ui.roll[ROLL_BPM].from[0] && centre) {     /* rolling: the BPM's strip only */
            cv_begin((uint32_t)(g.sx1 - g.sx0), H_HEAD, T_BG);
            roll_text(ROLL_BPM, g.bx - g.sx0, HEAD_MY, b, ui.bpm_t ? T_ACCENT : T_THEME);
            cv_blit((uint32_t)g.sx0, Y_HEAD);
        }
        if (metro_x >= 0 && beat != metro_beat) {      /* a beat: the metronome's cell only (either way round) */
            int32_t cx = g.mx0 - (g.sx0 - g.mx1);
            metro_beat = (int8_t)beat;
            cv_begin((uint32_t)(g.sx0 - cx), H_HEAD, T_BG);
            head_metro(g.mx0 - cx, beat);
            cv_blit((uint32_t)cx, Y_HEAD);
        }
        return;
    }
    ui.head_sig = sig;
    metro_x = -1;
    cv_begin(240, H_HEAD, T_BG);
    cv_icon_mid(HEAD_TRK_X, H_HEAD / 2, 16, trk_icon(song.sel, 1), midi_lit(song.sel) ? T_TEXT : T_ACCENT, T_BG);   /* (1.5.5:
                                                       * a MIDI IN note into it, #208: lit) */
    if (slot && (centre || !msg)) {                    /* the slot letter (not under a message centred: its room) */
        char l[2] = {sl[0], 0};
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        rx = cv_text(HEAD_SLOT_X, HEAD_MY, &AF_M, l, sl[0] == '-' ? T_MID : T_THEME);
        if (sl[1])                                     /* edited since: the mark, up by the letter's top */
            rx = cv_text(rx, HEAD_MY, &AF_M, "*", T_MID);
        rx += 1;                                       /* (the REC mark's cell after it, its ink 3 px on) */
    }
    draw_rec_mark(rx, T_BG);
    if (centre) {
        int32_t ib[4], lx = g.bx + g.b[2];             /* lx: the right end of the BPM (and its lock) */
        uint32_t id = song.playing ? (chain.running ? ICON_X_SONG : ICON_X_PLAY) : seq_counting() ? ICON_X_PLAY : ICON_X_STOP;
        icon_ink(16, id, ib);                          /* the play state: the song's disc, play (counting in: the
                                                        * accent's, about to play), stop */
        cv_icon_mid(g.px1 - ib[2], H_HEAD / 2, 16, id, /* (right-aligned in its slot) */
                    song.playing ? T_THEME : seq_counting() ? T_ACCENT : T_MID, T_BG);
        head_metro(g.mx0, beat);
        metro_x = (int16_t)g.mx0;
        metro_beat = (int8_t)beat;
        if (!ui.roll[ROLL_BPM].from[0])
            GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        roll_text(ROLL_BPM, g.bx, HEAD_MY, b, ui.bpm_t ? T_ACCENT : T_THEME);
        if (ui_prefs & PREF_BPM_LOCK) {                /* BPM LOCK ON (#58): the lock right of the BPM */
            icon_ink(16, ICON_X_LOCK, ib);
            cv_icon_mid(lx + HEAD_GAP - ib[0], H_HEAD / 2, 16, ICON_X_LOCK, T_MID, T_BG);
            lx += HEAD_GAP + ib[2] - ib[0];
        }
        if (msg) {                                     /* right of the BPM, over the right slot */
            int32_t x = lx + 9 > 142 ? lx + 9 : 142;
            head_msg(x, 236 - x);
        }
    } else {
        head_msg(-1, HEAD_MSG_W);
    }
    if (!msg && (chain.running || song.octave))
        head_group();
    if (!(msg && centre))
        draw_battery(HEAD_BAT_X);
    cv_blit(0, Y_HEAD);
}
/* LINE: 1 px dividers in the BG gaps between the strips, not around them: under the header, above and
 * below the panel, and between the four columns over rows y .. y + h (the cards; the mixer's strips too) */
#define RULE_X0 CARD_X(0)
#define RULE_W (CARD_X(3) + CARD_W - CARD_X(0))

#ifndef CARD_HOOK
#define CARD_HOOK(v, u) ((void)0)                       /* host: a card value set in S (M too wide with its unit) */
#endif
/* MOTION on the cards (#63: values that move on their own): the selected track's motion-driven parameters
 * (motion.c motion_mask: PLAY ON and an event for the id), once per draw_columns; card_mot_next is set just
 * before a draw_column of a track parameter (card_mot_of) and taken by it; card_mot: the cards drawn with it. */
static uint32_t card_mot_mask[(MOTION_N + 31u) / 32u];
static uint8_t card_mot_next, card_mot;

/* #251, MENU > LARGE: a tall card sets its value in one face over everything its knob can show (L where every value
 * fits L, else M, else S), so turning it never changes the value's size. card_span (what the knob shows: fn's text
 * of each value lo..hi) is set just before a draw_column (card_span_of / card_span_by) and taken by it; none: the
 * value's own face. The face is worked out when what the card is of changes (span_face's key), not every frame. */
typedef struct card_span {
    void (*fn)(const struct card_span *sp, int32_t v, char *val);   /* value v as the card shows it (val: 16) */
    const void *p;                                       /* fn's: a param_desc_t, a name list .. */
    int32_t lo, hi, ctx;
} card_span_t;
static card_span_t card_span;

/* MENU > LARGE, a tall card (ui.c LK_TALL; 57 x LG_CARD_H): which knob it is (a "K1" pill in M, the keycap's colours,
 * the accent while it turns, DIM when the knob does nothing here) and the icon at the right of it; the label in M
 * centred (S when M is too wide); the value centred in L, its figures twice the height of M (M, then S, when L is too
 * wide or the sparse L face lacks a glyph: gen_aa_font.py L_CHARS; #251: for any value the knob shows, sp: one face
 * for all of them, span_face); the unit (S) centred under it; the gauge. The value snaps (no rolling digits). MONO,
 * the knob just turned: the label inverted as on the small cards. */
#ifndef LARGE_HOOK
#define LARGE_HOOK(s, why) ((void)0)                    /* host: a tall card's value and its face (0 L, 1 M: a glyph not
                                                         * in L, 2 M: L too wide, 3 S) */
#endif
#define LG_MY 4                                         /* the K pill: rows 4..21 */
#define LG_MH 18
#define LG_LY 24                                        /* the label's line: M capitals rows 28..38 */
#define LG_VB 49                                        /* the value's band: L capitals rows 49..69 */
#define LG_UY 73                                        /* the unit's line (S capitals rows 76..84) */
#define LG_GY 92                                        /* the gauge, 3 px */








/* The power-on splash (main.c fm1_main, web/emu felucca_web.c; 1.1.5): a RAISE square centred on the screen, square
 * corners (in every style; LINE too: RAISE from the palette's SURF), in it in AF_M, the ink SPL_PAD in from the left:
 * at the top the name and the version, the first line's capitals' top SPL_PAD down; at the bottom the maker and
 * "with community", the last line's capitals' bottom (its baseline) SPL_PAD up from the square's bottom. Both
 * margins are measured to the capitals (the descender of "community" hangs into the bottom one, as off a baseline).
 * The square (176 x 176) is more than the canvas (CV_MAX, 240 x 124): it is filled straight on the screen and its
 * two text bands drawn on it as canvases, the top one from the square's top edge, the bottom one to its bottom edge.
 * In the saved palette (settings_init before it) */
#define SPL_SQ 176                                  /* the square (even: centred exactly) */
#define SPL_X0 ((240u - SPL_SQ) / 2u)               /* its top-left on the screen (x and y) */
#define SPL_PAD 16                                  /* the text's inset: left (its ink), top and bottom (the capitals) */
#define SPL_PITCH 20                                /* line to line */
static const char *const SPLASH_LINES[] = {"Felucca", FELUCCA_VERSION, "H\xFCgelton Instruments", "with community"};
/* a band of two lines, h tall, at y on the screen: the first line's top at ly; the two lines' capitals declared
 * against al0..al1 (mode: AL_V the band centred there, AL_B the last baseline on al1) */
static void splash_band(uint32_t y, uint32_t h, int32_t ly, const char *const *l, uint32_t mode, int32_t al0,
                        int32_t al1, const char *tag)
{
    cv_begin(SPL_SQ, h, T_RAISE);
    GFX_HOOK_ALIGN(0, al0, 0, al1, mode | AL_N(2), tag);
    cv_text_on(SPL_PAD, ly, &AF_M, l[0], T_TEXT, T_RAISE);
    cv_text_on(SPL_PAD, ly + SPL_PITCH, &AF_M, l[1], T_TEXT, T_RAISE);
    cv_blit(SPL_X0, y);
}
static void draw_splash(void)
{
    int32_t h = SPL_PAD - AF_M_CAP_Y + SPL_PITCH + AF_M.h;     /* a band: its two lines, the descenders in */
    int32_t lb = h - SPL_PAD - AF_M_CAP_H - AF_M_CAP_Y;         /* the bottom band's last line: its top */
    lcd_fill(0, 0, 240, 240, T_BG);
    lcd_fill(SPL_X0, SPL_X0, SPL_SQ, SPL_SQ, T_RAISE);
    splash_band(SPL_X0, (uint32_t)h, SPL_PAD - AF_M_CAP_Y, SPLASH_LINES, AL_V, SPL_PAD,
                SPL_PAD + SPL_PITCH + AF_M_CAP_H, "splash name + version SPL_PAD from the top");
    splash_band(SPL_X0 + SPL_SQ - (uint32_t)h, (uint32_t)h, lb - SPL_PITCH, SPLASH_LINES + 2, AL_B, 0, h - SPL_PAD,
                "splash maker's baseline SPL_PAD from the bottom");
    lcd_sync();
}

/* UPDATE MODE countdown (main.c: OCT- + OCT+ held): over everything, the menu and the dialogs too */
static void draw_uboot(void)
{
    static uint8_t shown;
    char d[4] = {(char)('0' + ui.uboot % 10u), 0, 0, 0};
    if (!ui.force && shown == ui.uboot)
        return;
    shown = ui.uboot;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    draw_text_box(0, 76, 240, &AF_M, "UPDATE MODE IN", T_TEXT, 1);
    draw_text_box(0, 102, 240, &AF_L, d, T_THEME, 1);
    {   /* the two keycaps held, and what letting go does */
        int32_t w = kc_w(KC_OCTDN) + 3 + kh_ink(KC_OCTUP, "LET GO TO CANCEL"), x = HALF_UP(240 - w);
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(3), "update-mode keys centred");
        x = cv_keycap(x, 1, KC_OCTDN, T_KEY, T_INK, T_BG) + 3;
        cv_key_hint(x, 1, KC_OCTUP, "LET GO TO CANCEL", 1, T_BG);
        cv_blit(0, 149);
        lcd_sync();
    }
    ui.force = 0;
}

/* the count-in (seq.c, MENU > COUNT-IN; 1.1, Discussion #131): over the page until the sequencer starts. Its bar's four
 * beats counted down, 4 3 2 1, in four cells (the beat now filled THEME, those gone dim), with 2 BARS which bar it is;
 * PLAY stops it. Drawn again on each beat (seq.c's state; the header stays live) */
#define CI_Y 88                                       /* the cells: 4 x 48 px, 8 px apart, 72 tall */
#define CI_H 72

/* the OCT- / OCT+ dialog: what it does (ui.confirm, ui.confirm_trk) on two lines, on a surface */
#define DLG_X 16
#define DLG_Y 62
#define DLG_W 208
#define DLG_H 116
static void confirm_text(char *a, char *b)
{
    uint32_t k = ui.confirm_trk;
    b[0] = 0;
    switch (ui.confirm) {
    case CF_CLEAR_TRK:
        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);
        break;
    case CF_INIT_SOUND:
        str_cpy(a, "INITIALIZE SOUND?", 24);
        break;
    case CF_CLEAN_SOUND:                             /* 1.5.5 (#190): the basic sound, nothing added */
        str_cpy(a, "CLEAN INIT SOUND?", 24);
        str_cpy(b, "NO FX, NO MOD", 24);
        break;
    case CF_CLEAR_MOTION:
        str_cpy(a, "CLEAR T1 AUTOMATION?", 24); a[7] = (char)('1' + k % NTRK);
        break;
    default:
        str_cpy(a, "CLEAR T1 SEQUENCE?", 24);     /* the header (T1..T4) is hidden */
        a[7] = (char)('1' + k % NTRK);
        break;
    }
}
static void draw_confirm(void)
{
    char a[24], b[24];
    const aafont_t *tf = &AF_M;
    confirm_text(a, b);
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    cv_begin(DLG_W, DLG_H, T_BG);                     /* a SURF card: warning, the question, the detail, two buttons */
    if (ux.style)                                     /* LINE: no card, a 1 px rule round it */
        cv_rrect(0, 0, DLG_W, DLG_H, 8, T_RULE, T_BG);
    cv_rrect(!!ux.style, !!ux.style, DLG_W - 2 * !!ux.style, DLG_H - 2 * !!ux.style, 8 - !!ux.style, T_SURF,
             ux.style ? T_RULE : T_BG);
    GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog icon centred");
    cv_icon_in(0, 12, DLG_W, 0, 16, ui.confirm == CF_CLEAR_MOTION ? ICON_X_MOTION_DEL : ICON_X_WARN, T_ACCENT, T_SURF);
    if (text_w(tf, a) > DLG_W - 16)
        tf = &AF_S;
    GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog question centred");
    cv_text_in(0, 34, DLG_W, tf, a, T_TEXT, T_SURF);
    if (b[0]) {
        char f[32];
        text_fit(f, sizeof f, b, &AF_M, DLG_W - 16);    /* a name: free text (M: what is selected, as the question) */
        GFX_HOOK_ALIGN(0, 0, DLG_W, 0, AL_H, "dialog detail centred");
        cv_text_flags(ink_in(&AF_M, f, DLG_W), 55, &AF_M, f, T_MID, T_SURF, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    }
    cv_rrect(10, 80, 90, 26, 6, T_RAISE, T_SURF);     /* OCT-: NO */
    GFX_HOOK_ALIGN(10, 80, 100, 106, AL_HV | AL_N(2), "dialog button NO");
    cv_key_hint(10 + HALF_UP(90 - kh_ink(KC_OCTDN, "NO")), 80 + HALF_UP(26 - KC_H), KC_OCTDN, "NO", 1, T_RAISE);   /* (ink centred) */
    cv_rrect(108, 80, 90, 26, 6, T_THEME, T_SURF);    /* OCT+: YES (on the THEME fill: an INK keycap, THEME label) */
    {
        int32_t x = 108 + HALF_UP(90 - kh_ink(KC_OCTUP, "YES"));
        GFX_HOOK_ALIGN(108, 80, 198, 106, AL_HV | AL_N(2), "dialog button YES");
        x = cv_keycap(x, 80 + HALF_UP(26 - KC_H), KC_OCTUP, T_INK, T_THEME, T_THEME);
        cv_text_on(x + KH_GAP, 80 + HALF_UP(26 - KC_H) - 1, &AF_S, "YES", T_INK, T_THEME);
    }
    cv_blit(DLG_X, DLG_Y);
}

/* the stored STYLE (ui_style: the menu, a settings import, a restore) into gfx.c's copy: the tokens again, all redrawn */
static void style_apply(void)
{
    if (ui_style > ST_LINE)                             /* 2, the retired PIXEL: LINE; unknown: FLAT */
        ui_style = ui_style == 2u ? ST_LINE : ST_FLAT;
    if (ux.style != ui_style) {
        ux.style = ui_style;
        palette_apply();
        ui.force = 1;
    }
}

static void ui_draw_page(uint32_t counting);
static void ui_draw(void)
{
    static uint8_t counting;
    style_apply();
    ui.frame++;
    if (counting != (uint8_t)seq_counting()) {          /* a count-in began or ended: the page drawn again after it */
        counting = (uint8_t)seq_counting();
        ui.force = 1;
    }
    if (!scr_frame())                                   /* MENU > SCREEN OFF: dark (or waking), nothing drawn */
        return;
    ui_draw_page(counting);
    scr_shown();
}
static void ui_draw_page(uint32_t counting)
{
    (void)counting;
    if (ui.uboot) {
        draw_uboot();
        draw_head();
        return;
    }
    if (ui.confirm) {                                   /* the OCT- / OCT+ dialog */
        if (ui.force) {
            draw_confirm();
            ui.force = 0;
        }
        draw_head();
        return;
    }
    if (ui.home) {                                       /* no HOME page: the first EDIT page instead */
        open_family(FAM_EDIT);
        ui.force = 1;
    }
    cursor_fix();
    gb_draw();                                           /* ui_gb.c: the whole screen, in one colour */
    if (ui.hot_t)
        ui.hot_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    ui.force = 0;
}
