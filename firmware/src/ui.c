/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA user interface.
 * Flat: SURF cards and panels on the palette's background, no rules, one type family (Inter Tight, three sizes), tracks
 * named by their numbers 1..4 on a cushion (icons.c trk_icon). Four columns <-> KNOB 1..4. Rendering is lazy:
 * every element remembers what it last drew and is redrawn only on change. */
static int project_save(uint32_t slot);
static void panel_setup(void);
static void project_load(uint32_t slot);
static int project_used(uint32_t slot);
static int up_used(uint32_t k);              /* user presets: upreset.c */
static int up_load(uint32_t k);
static uint32_t up_count(void);
static uint32_t up_nth(uint32_t n);
static uint32_t up_rank(uint32_t slot);
static uint32_t up_engine(uint32_t k);
static void up_name(uint32_t k, char *b);
static void up_slot_label(char *b, uint32_t k);
static void up_ui(uint32_t op, uint32_t k);
static int up_has_pat(uint32_t k);               /* user presets that hold a pattern */
static uint32_t up_pat_count(void);
static uint32_t up_pat_nth(uint32_t n);
static uint32_t up_pat_rank(uint32_t slot);
static void up_pat_load(track_t *t, uint32_t k);
static void up_auto_name(char *b, uint32_t e, uint32_t k);   /* naming (ui_name.c) */
static void up_ui_named(uint32_t op, uint32_t k, const char *name, uint32_t cat);
static uint32_t up_cat(uint32_t k);
static int project_save_as(uint32_t slot, const char *name);
static int project_name(uint32_t slot, char *b);
static int project_info(uint32_t slot, char *b, uint16_t *len48, uint32_t *den, uint8_t *n);
static int project_rename(uint32_t slot, const char *name);
static int project_erase(uint32_t slot);
static void project_cur_name(char *b);
static uint32_t up_gen;                      /* bumped on every user bank change (redraws) */
static uint8_t ui_pref_bytes[32];            /* Felucca's MENU settings: all at their defaults (0) in the groovebox */
/* MENU's two-valued settings (ui_menu.c MENU_FLAGS), a bit each in a byte no engine uses (as ui_layer.c layer_seen):
 * saved with the settings; 0 in older ones = every setting's default (append-only: a new setting takes a new bit,
 * its default is 0) */
#define ui_prefs (ui_pref_bytes[30])
#define PREF_LATCH 1u                          /* MENU > FX LATCH ON (#40) */
#define PREF_ANIM_OFF 2u                       /* MENU > ANIM OFF (#46): values snap (no rolling digits, no glide) */
#define PREF_ACCEL 4u                          /* MENU > KNOB ACCEL ON (#52): fast turns of wide values x2..x8 */
#define PREF_USB_FIXED 8u                      /* MENU > USB LEVEL FIXED: USB audio at the full level, MASTER after */
#define PREF_BPM_LOCK 16u                      /* MENU > BPM LOCK ON (#58): SELECT sets the tempo only with GLO held */
#define PREF_LARGE 32u                         /* MENU > LARGE ON (#15, Discussion #80): big knob labels and values
                                                * (ui_draw.c large_kind); clear in every older setting = OFF */
#define PREF_SERIAL_OFF 64u                    /* MENU > USB SERIAL OFF (#67): no serial console, the device enumerates as
                                                * audio + MIDI only (usb.c usb_cdc_switch); clear in every older setting = ON */
#define PREF_RESTORE_OFF 128u                  /* MENU > RESTORE LAST OFF (1.2, Discussion #130): no autosave written, none
                                                * restored at power-on (project.c); clear in every older setting = ON */
#define fx_latch ui_prefs
/* MENU > AUDIO CLICK, CLICK LEVEL, COUNT-IN (1.1, Discussion #131; click.c, seq.c): two bits each in another byte no
 * engine uses, saved with the settings: bits 0-1 CLICK (OFF REC ON), 2-3 CLICK LEVEL (stored 0 MID, 1 LOW, 2 HIGH),
 * 4-5 COUNT-IN (OFF, 1 BAR, 2 BARS); 0 in older settings = OFF / MID / OFF, so nothing clicks or counts in that did
 * not; 3 (unknown) reads as 0 (settings_persist.c puts it back). Bit 6 (1.2): MENU > SCALE LEDS ON (Discussion #127,
 * ui_input.c scale_leds; 0 in older settings = OFF). Bit 7 (1.5.5, #208): MENU > MIDI > AUTO SELECT ON */
#define ui_rec_prefs (ui_pref_bytes[28])
#define PREF_REC 0xFF00u                       /* (menu_items.c MENU_FLAGS: a bit of ui_rec_prefs, << 8) */
#define PREF_SCALE_LEDS (0x40u << 8)
#define PREF_MIDI_FOLLOW (0x80u << 8)          /* 1.5.5 (#208): MENU > MIDI > AUTO SELECT ON, bit 7 of ui_rec_prefs (0 in
                                                * every older setting = OFF; ui_input.c midi_follow) */
/* 1.2: three more two-valued MENU settings, bits of another byte no engine uses, saved with the settings; 0 in older
 * settings = today's behaviour (append-only, as ui_prefs): bit 0 DISPLAY > SCOPE MIX (Discussion #165, fx.c scope_mix),
 * bit 1 CONTROL > STEP PREVIEW ON (Discussion #169, ui_input.c step_preview), bit 2 CONTROL > CHORD ENTRY ADD (#155,
 * ui_input.c seq_entry). 1.5: bit 3 SYSTEM > HELP ON (Discussion #156, ui_help.c). 1.5.5: bit 4 CONTROL > SELECT PAGE
 * (Discussion #188, ui_input.c: SELECT steps the pages; 0 = BPM, as before); bit 5 MIDI > SCREEN WAKE PANEL (scr_input:
 * only the panel wakes SCREEN OFF, as up to 1.5.1; 0, every older setting, = +MIDI, the default: a note-on too).
 * 1.5.6: bit 6 SELECT's second bit (#84: sel_mode). Bit 7 free */
#define ui_prefs2 (ui_pref_bytes[24])
#define PREF_X 0xFF0000u                       /* (menu_items.c MENU_FLAGS: a bit of ui_prefs2, << 16) */
#define PREF_SCOPE_MIX (0x01u << 16)
#define PREF_PREVIEW (0x02u << 16)
#define PREF_CHORD_ADD (0x04u << 16)
#define PREF_HELP (0x08u << 16)
#define PREF_SEL_PAGE (0x10u << 16)
#define PREF_WAKE_PANEL (0x20u << 16)
#define PREF_SEL_HI (0x40u << 16)
#define PREF_CPU2_ON (0x80u << 16)              /* MENU > SYSTEM > 2ND CORE ON (BETA): the second core from the next
                                                 * power-on (main.c); clear = OFF, one core as 1.6 (every older setting:
                                                 * 1.6 never set bit 7). It turns itself OFF when core 1 misreads
                                                 * (hal/fm1_cpu1.h). (The 1.6.1 dual-core beta build had it the other
                                                 * way round, 0 AUTO / 1 OFF) */
/* MENU > CONTROL > SELECT: 0 BPM, 1 PAGE (1.5.5, #188), 2 MODW, 3 VEL (1.5.6, #84: ui_input.c sel_play), bit 4 and bit 6 of
 * ui_prefs2 (0 in every older setting = BPM; 1.5.5's PAGE keeps its bit; firmware before 1.5.6 reads MODW as BPM and
 * VEL as PAGE (its bit 4), before 1.5.5 both as BPM) */
enum { SEL_BPM, SEL_PAGE, SEL_MODW, SEL_VEL, SEL_N };
/* 1.3 (Discussions #112, #134): MENU > DISPLAY > HOME, what the HOME page shows under its four cards: 0 SCOPE (the
 * oscilloscope, as before), 1 TRACKS (the four tracks as rows: ui_graph.c draw_home_tracks), 2 LEVELS (1.5, #134: the
 * rows of TRACKS, and KNOB 1..4 the LEVEL of T1..T4 in place of the engine's four: home_param). A byte of its own no
 * engine uses, saved with the settings; 0 in older settings = SCOPE; an unknown value (a later firmware's view) reads as
 * SCOPE and is kept as saved (1.3 / 1.4 read LEVELS so) */
#define ui_home_view (ui_pref_bytes[26])
/* 1.5.6 (#212): MENU > DISPLAY > LEDS INV LO / INV HI: INV with the idle LEDs at the steady mid level (ui_input.c
 * ui_leds) instead of lit. The mode stays INV (settings_leds, panel.c: so firmware before 1.5.6 reads plain INV) and
 * this byte no engine uses holds the level: 1 LO, 2 HI; 0 (every older setting) or another value = INV as before.
 * Saved with the settings; read only while LEDS is INV (menu_items.c leds_row) */
#define ui_leds_inv (ui_pref_bytes[25])
static uint32_t leds_inv(void) { return settings_leds == LEDS_INV && ui_leds_inv - 1u < 2u ? ui_leds_inv : 0u; }
enum { HV_SCOPE, HV_TRACKS, HV_LEVELS, HV_COUNT };
static int home_levels(void) { return ui_home_view == HV_LEVELS; }
enum { RP_CLICK, RP_LEVEL, RP_COUNTIN };
/* MENU > STYLE (ui_menu.c): ST_FLAT ST_LINE (gfx.c) in another byte no engine uses, saved with the settings;
 * 0 in older ones = FLAT; 2, the retired PIXEL (1.0.1), reads as LINE; anything else unknown as FLAT (settings_persist.c). gfx.c draws from its copy, ux.style
 * (ui_draw.c style_apply) */
#define ui_style (ui_pref_bytes[29])
static void draw_gaps(uint32_t y, uint32_t h);        /* (ui_draw.c) */
static int dial_page(void);                           /* (ui_graph.c) EDIT / FILTER pages drawn as four big dials */

static uint8_t sync_reload;                  /* engine / preset / project / user preset loaded: editor RELOAD push */

#define ACC T_THEME                /* values, curves */
#define VAL(c) ((c) == ui.hot_col && ui.hot_t ? T_ACCENT : T_THEME)   /* the knob just turned: accent */
#define RATIO(d, v) ((d)->max > (d)->min ? ((int32_t)(v) - (d)->min) * 1000 / ((d)->max - (d)->min) : -1)
/* layout: header 0..24, four cards 28..72 (57 px at x 3 + 59 c),
 * the panel 76..198 (a SURF area; the graphs live in it), footer 202..240; BG between them */
#define Y_HEAD 0
#define H_HEAD 24
#define Y_LABEL 28                    /* the cards */
#define Y_SEP_END 72
#define CARD_W 57
#define CARD_H 44
#define CARD_X(c) (3 + 59 * (int32_t)(c))
#define Y_GRAPH 76                    /* the panel */
#define H_GRAPH 122
#define Y_FOOT 202
#define H_FOOT 38
/* MENU > LARGE (large_kind below): on the value pages the cards are tall (28..132) and the panel a strip
 * (136..198); elsewhere (lists, rolls, layers) the layout above, its card labels in M */
#define LG_CARD_H 104
#define LG_Y_GRAPH 136
#define LG_H_GRAPH 62

static struct {
    uint8_t home;
    uint8_t page;                /* index into PAGES */
    uint8_t fam_last[FAM_COUNT]; /* last page used per family */
    uint8_t bank;                /* SEQ: 16-step bank (follows the cursor) */
    uint8_t cursor;              /* SEQ: step being edited (STEP page KNOB 1 moves it) */
    uint8_t entry_open;          /* SEQ: keys held since the first press of this entry */
    uint8_t sgrid;               /* SIMPLE, what the keys are (SEQ taps on the SEQ page): 1 SEQ the step keys (white keys =
                                  * steps, black keys 1..6 = tracks), 2 PARAM (ui_input.c par_*), 0 KEY the keyboard */
    uint8_t lane;                /* SEQ > STEP on a DRUM track (the grid): the lane the keys and KNOB 3 / 4 edit */
    uint8_t plk_src;             /* parameter locks (ui_input.c lock_*): the sound page whose KNOB 1..4 a step held
                                  * locks, PAGES index + 1; 0 = HOME's four */
    uint8_t plk_lane, plk_trk;   /* .. the grid: the lane and the track of the hits plk_rm takes away */
    uint64_t plk_rm, plk_acc;    /* .. the grid's step keys pressed on a hit (bit = step): it goes when the key is let
                                  * go, unless the key was held for a lock (its accent: plk_acc) */
    uint8_t hot_col, hot_t;      /* column whose knob was just turned (drawn white) */
    uint8_t menu;                /* 0 off, 1 list, 2 about + credits, 3 info (1.2) (HOME held) */
    uint8_t menu_sel;            /* MENU: the row (menu_items.c MI_*; its tab MI_TAB), kept while the device runs */
    uint8_t menu_row[5];         /* MENU: the row last picked in each tab, from its first (ALGORITHM comes back to it) */
    uint16_t menu_scroll;        /* continuous ABOUT + CREDITS position, pixels */
    uint32_t menu_sig, home_t0;  /* HOME press time (btn_hold) */
    uint8_t force;               /* full redraw pending */
    uint8_t msg_t;               /* transient message frames */
    uint8_t help_t;              /* 1.5, MENU > HELP ON: frames the page's hint stays in the footer (ui_help.c) */
    uint8_t bpm_t;               /* frames the BPM stays highlighted after a SELECT turn */
    uint8_t act;                 /* action pages: the column whose action OCT+ does, + 1; 0 = none (act_col) */
    uint8_t tl_clean;            /* SAVE > TOOLS: KNOB 2 turned on past INIT, CLEAN INIT picked (1.5.5, #190) */
    uint32_t rec_t0;             /* REC press time: a tap arms (btn_hold, no hold: held, REC opens its layer) */
    uint32_t save_t0;           /* SAVE press time (btn_hold: held = UNDO) */
    uint8_t confirm;             /* the OCT- / OCT+ dialog: CF_*, 0 = none */
    uint8_t confirm_trk;         /* the track it clears, the slot it overwrites */
    uint8_t uslot;               /* SAVE > USER: the selected user preset slot */
    uint8_t ppick;               /* SAVE > PHRASES: the pattern picked (pat_count list index) */
    uint8_t song_row;            /* SONG: the section selected, count selects + ADD */
    uint8_t song_trk;            /* SONG: the lane (track) selected, 0..NTRK-1 */
    uint16_t ev_row;             /* SEQ > AUTOMATION (ui_events.c ev_*): the row selected (0 PLAY / CLEAR, 1 QUANTIZE, the
                                  * last + ADD; 1.2: up to EV_ROWS, past 255) */
    uint8_t ev_step, ev_id;      /* .. + ADD: the step and the parameter (or EV_CHANCE, EV_RATCH, EV_NUDGE, EV_VEL) it
                                  * adds on */
    uint16_t ev_keep;            /* .. the CHANCE / RATCH / NUDGE row being edited (its EVC code, 0 none): listed at its default */
    uint8_t ev_vel;              /* .. 1.5: the VEL rows shown (+ ADD VEL's SHOW / HIDE; off when the page is entered) */
    uint8_t uboot;               /* main.c: seconds left before UPDATE MODE (OCT- + OCT+ held), 0 = none */
    uint32_t ly_t0;              /* the layer button's press time | 1, LY_* bits (ui_layer.c layer_gesture) */
    uint8_t ly;                  /* the layer whose button is down (LAYER_*), 0 = none */
    uint8_t layer;               /* the layer whose map is shown (LAYER_*), 0 = none */
    uint8_t lock;                /* #83: the layer locked open by a double tap (LAYER_*), 0 = none (ui_layer.c) */
    uint16_t pg_down;            /* page buttons down (panel ids) that act when let go */
    uint32_t layer_sig;          /* drawn-state cache of the map */
    char msg[24];
    char msg2[24];               /* a second message, shown when the first is over */
    uint32_t enc_t[NE];
    /* drawn-state cache */
    char col[4][32];
    uint32_t graph_sig, head_sig, foot_sig, frame;
    /* rolling digits (ui_draw.c roll_*): the four card values and the header BPM */
    struct {
        char from[7];            /* the value rolling out; "" = idle */
        uint8_t t0;              /* (ui.frame) of the value's last change */
        int8_t dir;              /* +1: it went up (the old digit leaves upward), -1: down */
        uint8_t sig;             /* a card: what its value is of (label, unit, track, engine, palette) */
    } roll[5];
    int16_t roll_bpm;            /* the BPM last drawn */
    struct {                     /* MIDI LEARN (midi_learn.c; here, not loose globals: the audio ISR's data keeps its
                                  * layout and its cost, tests/target_budget.py) */
        uint8_t on;              /* MIDI LEARN is on */
        uint8_t pick, trk, id;   /* a parameter picked (1): its track and P_* id */
        volatile uint8_t io[2];  /* midi_control.c ml_arm, ml_heard */
    } ml;
    volatile uint8_t mpc[NTRK];  /* MIDI Program Change per track, 0x80 | program, 0 none (midi_control.c mpc_io: here
                                  * for the same reason as ml.io; midi_pc_poll) */
} ui;

enum { CF_NONE, CF_CLEAR_SEQ, CF_CLEAR_TRK, CF_OVR_PROJ, CF_OVR_USER, CF_LOAD_PAT,
       CF_DEL_ROW, CF_CLEAR_SONG, CF_INIT_SOUND, CF_CLEAR_MOTION, CF_ERASE_USER, CF_ERASE_PROJ, CF_CLEAN_SOUND };
                                  /* ui.confirm: TOOLS' clears; SAVE over a used slot; a pattern over the user's steps;
                                   * USER ERASE; PROJECT ERASE (1.5.5, #199); CLEAN INIT (1.5.5, #190) */

/* MENU > DISPLAY > SCREEN OFF (1.1.5): after a while with no input on the panel the screen goes dark (the backlight
 * stays on: lcd.c lcd_power)
 * (a picture held for hours can stay faintly on the panel); the sound, the sequencer, MIDI and USB go on, the LEDs
 * stay as they are. NEVER (the default since 1.1.5.1), 5, 15, 30, 60 minutes, kept in a byte no engine uses, saved
 * with the settings (SCR_CODE). 1.1.5 stored value ^ 3, 0 (its default and every older setting) being 30 MIN; 1.1.5.1
 * reads that 0 as NEVER (the new default: the panel went dark for good on 1.1.5, see lcd.c lcd_power) and keeps the
 * other codes, 30 MIN now 4. Unknown codes read as NEVER.
 * Input is the panel's: a button or key held or pressed, a knob turned (not the editor or MASTER, an analogue pot);
 * 1.5.5: with MENU > MIDI > SCREEN WAKE +MIDI (the default; PANEL: the panel only, as before) a MIDI IN note-on too,
 * USB or TRS, on any channel that plays a track (seq.c midi_seen: the ISR only sets its bit), not clock, CC or SysEx.
 * The press, key or turn that wakes it is swallowed: no note (seq.c kb_asleep), no button, no value, until everything
 * is let go and the knobs rest for SCR_EAT_MS; a MIDI note that wakes it is not: it plays, as every MIDI note does.
 * A dialog, the count-in and the UPDATE MODE countdown keep it on (and wake it); the crash screen, UPDATE and UBOOT wake it at once (lcd_wake_now).
 * Off: lcd_power(0) (DISPOFF, SLPIN) and nothing is drawn. Waking, a frame at a time (never a wait in
 * the loop): 120 ms after the SLPIN, SLPOUT (lcd_power(1)); 120 ms after that one frame redraws everything
 * (ui.force), then DISPON (lcd_power(2)) */
#define ui_scr (ui_pref_bytes[27])
/* MENU > ANIM IDLE (1.2, Discussion #135; menu_items.c, ui_input.c idle_leds): bit 0, the LEDs' idle animation, in a
 * byte no engine uses, kept with the settings; 0 in every older setting = off. The other bits: 0 (room for a delay) */
#define ui_idle (ui_pref_bytes[10])
#define SCR_DEF 0u                                     /* NEVER (1.1.5.1; 1.1.5: 30 MIN) */
static const uint8_t SCR_CODE[5] = {0, 2, 1, 4, 7};    /* the stored byte of NEVER 5 15 30 60 MIN (1.1.5: 3 2 1 0 7) */
static const uint8_t SCR_DEC[8] = {0, 2, 1, 0, 3, 0, 0, 4};   /* .. and back (3: 1.1.5's NEVER; 5, 6 unknown) */
#define SCR_EAT_MS 300u
static const uint8_t SCR_MIN[5] = {0, 5, 15, 30, 60};  /* NEVER, minutes */
enum { SCR_ON, SCR_OFF, SCR_WAKE, SCR_SLPOUT, SCR_SHOW };
static struct {
    uint8_t st;                  /* SCR_*: SCR_WAKE asked, SCR_SLPOUT sent, SCR_SHOW the frame that redraws */
    uint8_t eat;                 /* the wake gesture: panel input swallowed (ui_input) */
    uint32_t idle;               /* fm1_ms of the last panel input (or blocker) */
    uint32_t t;                  /* fm1_ms of the last panel command (SLPIN, SLPOUT) */
    uint32_t eat_t;              /* fm1_ms of the last swallowed knob detent */
} scrn;
static uint32_t scr_get(void)                          /* the MENU's value: 0 NEVER .. 4 60 MIN */
{
    return ui_scr < NELEM(SCR_DEC) ? SCR_DEC[ui_scr] : SCR_DEF;
}
static void scr_wake(void)
{
    if (scrn.st == SCR_OFF)
        scrn.st = SCR_WAKE;
    scrn.eat = 1;
}
static void scr_wake_now(void)                         /* main.c: the crash screen, UPDATE, UBOOT */
{
    if (scrn.st != SCR_ON)
        lcd_wake_now();
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}
static uint8_t mf_seen;          /* 1.5.5 (#208): the MIDI IN notes of this pass (seq.c midi_seen, taken at every pass's
                                  * start: ui_input.c midi_take), bit per track; AUTO SELECT's (midi_follow) */
/* ui_input, every pass: 1 = the pass is swallowed (the screen dark or waking, or the wake gesture going on) */
static int scr_input(uint32_t pressed, uint32_t notes)
{
    uint32_t held = fm1_in.buttons | fm1_in.notes, k;
    int32_t moved = 0;
    if (pressed | notes | held | panel_moved)
        scrn.idle = fm1_ms;
    panel_moved = 0;
    if (mf_seen && !(ui_prefs2 & (PREF_WAKE_PANEL >> 16))) {     /* +MIDI: a note-on since the last pass keeps it on,
                                                                 * wakes it; nothing swallowed (the note has played) */
        scrn.idle = fm1_ms;
        if (scrn.st == SCR_OFF)
            scrn.st = SCR_WAKE;
    }
    if (scrn.st == SCR_ON && !scrn.eat)
        return 0;
    for (k = 0; k < NE; k++)                           /* (every knob's turns go) */
        moved |= panel_enc(k);
    panel_moved = 0;
    if (moved)
        scrn.eat_t = scrn.idle = fm1_ms;
    if (pressed | notes | held | (uint32_t)moved)
        scr_wake();
    if (scrn.st != SCR_ON || held || (int32_t)(fm1_ms - scrn.eat_t) < (int32_t)SCR_EAT_MS)
        return 1;
    scrn.eat = 0;                                       /* all let go, the knobs at rest: input again */
    kb_asleep = 0;
    return 0;
}
/* ui_draw, every frame: 1 = draw it */
static int scr_frame(void)
{
    uint32_t lim = SCR_MIN[scr_get()] * 60000u;
    if (ui.confirm || ui.uboot || seq_counting()) {    /* (never dark under these) */
        scrn.idle = fm1_ms;
        if (scrn.st == SCR_OFF)
            scr_wake();
    }
    switch (scrn.st) {
    case SCR_ON:
        if (!lim || (int32_t)(fm1_ms - scrn.idle) < (int32_t)lim)   /* (signed: robust to a wrap) */
            return 1;
        lcd_power(0);
        scrn.st = SCR_OFF;
        scrn.t = fm1_ms;
        scrn.eat = 1;
        kb_asleep = 1;
        break;
    case SCR_WAKE:
        if (fm1_ms - scrn.t >= 120u) {
            lcd_power(1);
            scrn.st = SCR_SLPOUT;
            scrn.t = fm1_ms;
        }
        break;
    case SCR_SLPOUT:
        if (fm1_ms - scrn.t >= 120u) {
            scrn.st = SCR_SHOW;
            ui.force = 1;
            return 1;
        }
        break;
    case SCR_SHOW:
        return 1;
    }
    ui.msg_t = 0;                                      /* (dark: what would have shown is over; a clean page back) */
    ui.msg2[0] = 0;
    ui.hot_t = ui.bpm_t = 0;
    return 0;
}
static void scr_shown(void)                            /* after the frame: the redrawn page on */
{
    if (scrn.st != SCR_SHOW)
        return;
    lcd_power(2);
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}

static const page_t *page_over;   /* a quick layer's own four knobs (ui_layer.c), while it edits or draws them */
static const page_t *cur_page(void) { return page_over ? page_over : &PAGES[ui.page]; }

/* MENU > LARGE (#15, Discussion #80: what KNOB 1..4 do, in bigger type). Per page type:
 *   LK_TALL  HOME and the value pages (EDIT ENV LFO MOD FX SLICER SCL CHORD ARP VOICE, MIXER, CLOCK,
 *            TOOLS): tall cards (a K1..K4 keycap, the icon, the label in M, the value in L,
 *            in M or S when L is too wide or lacks a glyph for any value of the knob (#251: one size per knob,
 *            ui_draw.c span_face), the unit under it, the gauge), the panel a strip: HOME's
 *            scope, ENV's ADSR, LFO's wave, MIXER's four tracks (name, state, meter) small; the
 *            page's title and number in L on the others (their charts cannot be read that small);
 *   LK_LABEL the list and graph pages (PRESETS, USER, PROJECT, PHRASES, SONG, the piano roll and the drum grid,
 *            AUTOMATION, SLICES), HOME with MENU > HOME TRACKS (1.3) and the quick layers' maps: the layout as it
 *            is, the card labels in M;
 *   LK_OFF   LARGE off; the menu (its own LARGE: ui_menu.c), the dialogs and NAME keep their own layout in every case. */
enum { LK_OFF, LK_LABEL, LK_TALL };
static int large_face_has(const char *s)                /* every glyph of s in the sparse L face */
{
    for (; *s; s++)
        if (glyph_at(&AF_L, fold(&AF_L, (uint8_t)*s)) < 0)
            return 0;
    return 1;
}

/* the quick layers (ui_layer.c): a button held, the keys and KNOB 1..4 are its shortcuts, its map over the page */
enum { LAYER_NONE, LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT, LAYER_SEQ, LAYER_REC, LAYER_ENV, LAYER_N };

/* FM operator pages belong to DIGITAL; they never appear on other instruments (without FELUCCA_FM4: never). SLICES:
 * a SLICE track's (ui_slice.c); LANES / LANES 2 a DRUM track's (its lane levels); FILTER an ANALOG track's; OPERATOR, OP ENV, OPERATOR 2 an FM6
 * track's (its patch's operators, ui_fm6op.c) */
static int page_visible(uint32_t i)
{
    return i < NPAGES && (PAGES[i].scope != SC_ENGINE || page_id_an(&PAGES[i], PAGES[i].id[0]) != 0xFFu);
}                                                     /* (SIMPLE: AGE only on an ANALOG track) */

static uint32_t page_first(uint32_t fam)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == fam)
            return i;
    return 0;
}

/* transient message in the top bar, right of the transport and the BPM: a + b */
static void ui_say(const char *a, const char *b)
{
    uint32_t n;
    str_cpy(ui.msg, a, sizeof ui.msg);
    n = str_len(ui.msg);
    str_cpy(ui.msg + n, b, sizeof ui.msg - n);
    ui.msg_t = 40;
    ui.msg2[0] = 0;
}

static void ui_message(const char *s) { ui_say(s, ""); }
/* a message led by an icon: its first byte (ui_draw.c draw_head draws the icon, then the words) */
#define MSG_NOFILE "\x01"                      /* the no-file icon (ICON_X_NOFILE) */
/* a missing sample (ui_input.c sample_notice): NO SAMPLE (SAMPLE NOT FOUND, the SLICES page's, is 4 px too wide
 * for the header's 130 px after the icon) */
#define MSG_NO_SAMPLE MSG_NOFILE "NO SAMPLE"

/* Main loop only, with interrupts enabled. PLAY may be consumed between reads;
 * take one coherent snapshot before a flash operation or song preparation. */
static int transport_busy(void)
{
    int busy;
    fm1_irq_off();
    busy = song.playing || transport_req == 1u || seq_counting();   /* (a count-in: as playing) */
    fm1_irq_on();
    return busy;
}


/* parameter locks: a page whose KNOB 1..4 can lock (a track's sound: one of its parameters can be motion; not MOD's
 * slots), the parameter KNOB k locks (ui.plk_src's page, HOME's four), 0xFF = none */
/* page pg's KNOB k as a lock's id (OCT+'s other four on LFO, COMB; SIMPLE: a global as motion.c's), 0xFF none */
static uint32_t lock_pid(const page_t *pg, uint32_t k)
{
    uint32_t id = page_id_an(pg, pg->id[k & 3u]);
    if (FELUCCA_SIMPLE && alt_mode && alt_ids(pg))
        id = alt_ids(pg)[k & 3u];
    if (id == 0xFFu)
        return 0xFFu;
    if ((id & 0x80u) || pg->scope == SC_GLOBAL)
        id = motion_gid(id & 0x7Fu);
    return id < MOTION_N && motion_param(id) ? id : 0xFFu;
}
static int lock_page_ok(uint32_t i)
{
    uint32_t k;
    if (i >= NPAGES || PAGES[i].graph == GR_MOD || PAGES[i].graph == GR_PERF ||
        (PAGES[i].scope != SC_TRACK && PAGES[i].scope != SC_ENGINE && !(FELUCCA_SIMPLE && PAGES[i].scope == SC_GLOBAL)))
        return 0;
    for (k = 0; k < 4u; k++)
        if (lock_pid(&PAGES[i], k) != 0xFFu)
            return 1;
    return 0;
}
static uint32_t lock_id(uint32_t k)
{
    uint32_t id = ui.plk_src && lock_page_ok(ui.plk_src - 1u) ? lock_pid(&PAGES[ui.plk_src - 1u], k)
                                                               : ENGINES[TSEL->eng_req % NENGINES]->knob[k & 3u];
    return id < MOTION_N && motion_param(id) ? id : 0xFFu;
}

/* 1.5 (Discussion #156): MENU > HELP ON: a page entered (here, go_home) shows its hint in the footer for ~2 s
 * (ui_help.c, ui_draw.c draw_foot) */
#define HELP_FRAMES 120u                       /* (~2 s at the UI's ~60 frames a second) */
static int help_on(void) { return (ui_prefs2 & (PREF_HELP >> 16)) != 0u; }
/* 1.5.5 (#208): a MIDI IN note into track k lights its number cushion for MLIT_MS (the header's for the selected track,
 * HOME TRACKS / LEVELS, the MIXER); midi_lit_ms[k]: fm1_ms | 1 of the last (ui_input.c midi_take), 0 none */
#define MLIT_MS 90u
static uint32_t midi_lit_ms[NTRK];
static int midi_lit(uint32_t k) { return midi_lit_ms[k % NTRK] && (fm1_ms | 1u) - midi_lit_ms[k % NTRK] < MLIT_MS; }
static void help_start(void) { ui.help_t = (uint8_t)(help_on() ? HELP_FRAMES : 0u); }
static void page_entered(void)
{
    const page_t *pg = cur_page();
    if (!ui.home && lock_page_ok(ui.page))
        ui.plk_src = (uint8_t)(ui.page + 1u);
    song.seq_mode = !ui.home && pg->fam == FAM_SEQ;
    if (FELUCCA_SIMPLE && !ui.home && pg->graph == GR_PERF)
        ui.sgrid = 0;                            /* SIMPLE: out of PERF the keys are the keyboard */
    ui.entry_open = 0;
    ui.hot_t = 0;                                /* clear the previous page's emphasis */
    ui.act = pg->graph == GR_USER ? 4u : 0u;     /* the save screen is ready for OCT+ */
    ui.tl_clean = 0;
    help_start();
    ui.force = 1;
}


static void step_clear(step_t *st)
{
    st->n = 0;
    st->time = ST_REST;
    st->flags = 0;
    st->vel = 0;
    st->hit = st->acc = 0;
    st->probability = 0;
}


/* ------------------------------------------------------- the DRUM grid --- */
/* SEQ > STEP on a DRUM track is the grid: 8 lanes x the 16 steps of a page. The white keys are the
 * steps of the page shown (a tap toggles the selected lane there), black keys 1..8 select the lane (and play
 * it), black key 9 held is ACC (white keys toggle accents, their LEDs show them), black keys 10 / 11 the page
 * down / up. KNOB 1 STEP, 2 LANE, 3 HIT, 4 ACC edit the cursor step. A sound load never converts the
 * steps: the grid shows a step's notes on their lanes (eng_drum.c step_lanes) and an edit makes the lane its
 * own (grid_own). Live recording on a DRUM track writes hits (seq.c rec_note) */
static int grid_on(void) { return !ui.home && !ui.confirm && cur_page()->graph == GR_ROLL && drum_track(TSEL); }
/* SEQ > DETAIL (1.5.1, Discussion #199; params.c, between STEP and AUTOMATION): a step page as STEP (the cursor, the
 * roll's keys, the grid's lane and page keys), its knobs the cursor step's CHANCE RATCH NUDGE VEL (ui_input.c sd_turn),
 * its panel their lanes (ui_graph.c graph_detail). No state of its own: the page is what says so */
static int detail_on(void)
{
    return !ui.home && !ui.confirm && cur_page()->scope == SC_STEP && str_eq(cur_page()->title, "DETAIL");
}

/* SIMPLE: the step keys (SEQ tapped): the page stays where it is; the keys are the 16 steps of the selected track (white)
 * and the 4 tracks (black: they select and play it) */
static int pm_page(void);
static int pm_down(void);
static uint8_t gbsv;                                  /* SIMPLE, the SAVE screen: 0 off, 1 SAVE (SAVE held), 2 LOAD (ui_input.c) */
static char gbsv_msg[2][16];                          /* .. what was saved / loaded, flashing from gbsv_t (fm1_ms | 1, 0 none) */
static uint32_t gbsv_t;
static int gbsv_keys_on(void) { return gbsv == 1u || gbsv == 2u; }   /* the keys are the slots' */
static uint8_t fxl_sel;                               /* SIMPLE: the FX lane is selected (black key 7, SEQ mode) */
static int sgrid_on(void) { return FELUCCA_SIMPLE && ui.sgrid == 1u && !ui.confirm && !pm_page() && !pm_down() && !gbsv_keys_on(); }   /* (PERF and a macro held: their own keys) */

/* ------------------------------------------------- performance macros --- */
/* GLO is the performance page. Its black keys pick the track; its 16 white keys are three groups:
 *   keys 1..8   the macros: punch-in settings. Hold one and turn knobs, on any page: each parameter turned is kept in that
 *               macro (up to PM_N) and sounds while the key is held; let go, everything returns. OCT- with it held empties it.
 *   keys 9..12  the fade time, 1/16, 1/8, 1 bar, 4 bars: it stays chosen (press it again for none). None: a macro is
 *               instant. The time is the tempo's (the sequencer's 1/16 is a quarter of a beat).
 *   keys 13..16 the master beat repeat: REPEAT 1/8, 1/16, 1/32 and REVERSE of the FX layer (perform.c), held (seq.c).
 * How many macro keys are held says what happens (the FUNC knob, KNOB 1: HOLD, CYCLE, PINGPONG):
 *   one     fades in, from the sound as it was to the macro's values, over the fade time
 *   two     fades from the first pressed to the second
 *   three+  steps through the held macros, one per fade time (the ARP knob, KNOB 2: UP DOWN UPDN RANDOM ORDER)
 * HOLD goes there once and stays; CYCLE goes there and starts again; PINGPONG goes there and back. Let go and every
 * parameter jumps back to what it was. */
#define PM_N 8u                                          /* parameters in a macro */
#define PM_NM 8u                                         /* macros */
#define PM_B 32u                                         /* parameters moving at once (the held macros' together) */
typedef struct { uint8_t n; uint8_t trk[PM_N], id[PM_N]; int16_t val[PM_N]; } perf_mac_t;
static perf_mac_t pm_mac[PM_NM];                         /* one set, global: a macro can move parameters on any tracks */
static struct {
    uint8_t n;                                           /* macro keys held */
    uint8_t mac[PM_NM], key[PM_NM];                      /* .. in the order pressed: the macro, its key */
    uint32_t t0;                                         /* fm1_ms when the held set last changed */
    uint8_t nb, btrk[PM_B], bid[PM_B], bedit[PM_B];      /* the parameters moving, edited (turned while held: left alone) */
    int16_t bval[PM_B];                                  /* .. and what they were */
} pm_h;
static uint8_t pm_fade, pm_func = 2, pm_arp = 2;         /* SIMPLE: always PINGPONG and UPDN (no knobs for them) */
static uint8_t pm_rel;                                   /* the release fade in 1/16s, 0 none .. 16 a bar: a let-go macro (and a beat repeat) glides out */
static struct { uint8_t n, trk[32], id[32]; int16_t from[32], to[32]; uint32_t t0, dur; } pm_rl;   /* the parameters gliding back */
static uint8_t pm_ui_a = 255, pm_ui_c = 255;             /* what the screen shows: the macro fading from, the one fading to / playing in the arp */
static int16_t pm_ui_u;                                  /* .. how far (0..1024) */                 /* 0 none, 1..4 the fade time; FUNC 0..2; ARP 0..4 */
static int pm_page(void) { return !ui.home && !ui.confirm && cur_page()->graph == GR_PERF; }
static int pm_down(void) { return pm_h.n != 0u; }   /* a macro key held (on any page) */
static int fxl_on(void) { return fxl_sel && sgrid_on(); }   /* the step keys are the FX lane's (seq.c fxl) */
/* SIMPLE, PARAM keys on: on a page whose knobs lock, the white keys set the chosen knob (par_k) and play (seq.c par_key) */
static uint8_t par_k;
static int par_on(void)
{
    return FELUCCA_SIMPLE && ui.sgrid == 2u && !ui.confirm && !ui.home && !pm_page() && !pm_down() && !gbsv_keys_on() &&
           lock_page_ok(ui.page);
}
/* the macro's parameter: track tr < NTRK, or tr == PM_G: a global (BPM, SWING, the master's) */
#define PM_G 0xFFu
static int16_t *pm_ptr(uint32_t tr, uint32_t id)
{
    return tr >= NTRK ? &song.g[id % G_COUNT] : &trk[tr].p[id];
}
static int pm_find(const perf_mac_t *m, uint32_t tr, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < m->n; i++)
        if (m->trk[i] == tr && m->id[i] == id)
            return (int)i;
    return -1;
}
static int pm_bfind(uint32_t tr, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < pm_h.nb; i++)
        if (pm_h.btrk[i] == tr && pm_h.bid[i] == id)
            return (int)i;
    return -1;
}
/* the value vp of track t is moving under a held macro: its dial shows inverted (ui_graph.c graph_dials) */
static int pm_altered(const track_t *t, const int16_t *vp)
{
    if (!pm_h.n || !vp)
        return 0;
    if (vp >= song.g && vp < song.g + G_COUNT)
        return pm_bfind(PM_G, (uint32_t)(vp - song.g)) >= 0;
    if (vp < t->p || vp >= t->p + P_COUNT)
        return 0;
    return pm_bfind((uint32_t)(t - trk), (uint32_t)(vp - t->p)) >= 0;
}
/* parameter b of the moving ones takes the value of macro m (-1: what it was) */
static int32_t pm_mv(int m, uint32_t b)
{
    if (m >= 0) {
        int i = pm_find(&pm_mac[m], pm_h.btrk[b], pm_h.bid[b]);
        if (i >= 0)
            return pm_mac[m].val[i];
    }
    return pm_h.bval[b];
}
static void pm_put(uint32_t b, int32_t v)
{
    int16_t *vp = pm_ptr(pm_h.btrk[b], pm_h.bid[b]);
    if (*vp != v)
        *vp = (int16_t)v;
}
static int pm_badd(uint32_t tr, uint32_t id, int16_t was)
{
    int b = pm_bfind(tr, id);
    if (b >= 0)
        return b;
    if (pm_h.nb >= PM_B)
        return -1;
    b = (int)pm_h.nb++;
    pm_h.btrk[b] = (uint8_t)tr;
    pm_h.bid[b] = (uint8_t)id;
    pm_h.bedit[b] = 0;
    pm_h.bval[b] = was;
    return b;
}
static uint32_t pm_step_ms(void) { return beat_samples() * 250u / (uint32_t)FS; }   /* a 1/16, ms */
static uint32_t pm_time(void)                           /* the fade time, ms; none (0) is instant for one macro */
{
    static const uint8_t ST[4] = {1, 2, 16, 64};
    return pm_fade ? pm_step_ms() * ST[(pm_fade - 1u) & 3u] : 0u;
}
static uint32_t pm_rel_ms(void)                         /* the release time, ms (0: none) */
{
    return pm_step_ms() * pm_rel;
}
static void pm_rl_rebase(void)                           /* the glide starts again from where each value is */
{
    uint32_t i;
    for (i = 0; i < pm_rl.n; i++)
        pm_rl.from[i] = *pm_ptr(pm_rl.trk[i], pm_rl.id[i]);
    pm_rl.t0 = fm1_ms;
}
static void pm_rl_snap(uint32_t tr, uint32_t id)         /* a macro takes a parameter that is gliding back: it is back */
{
    uint32_t i;
    for (i = 0; i < pm_rl.n; i++)
        if (pm_rl.trk[i] == tr && pm_rl.id[i] == id) {
            *pm_ptr(tr, id) = pm_rl.to[i];
            pm_rl.n--;
            pm_rl.trk[i] = pm_rl.trk[pm_rl.n]; pm_rl.id[i] = pm_rl.id[pm_rl.n];
            pm_rl.from[i] = pm_rl.from[pm_rl.n]; pm_rl.to[i] = pm_rl.to[pm_rl.n];
            return;
        }
}
static void pm_rl_step(void)
{
    uint32_t i, el = fm1_ms - pm_rl.t0;
    if (!pm_rl.n)
        return;
    for (i = 0; i < pm_rl.n; i++)
        *pm_ptr(pm_rl.trk[i], pm_rl.id[i]) = (int16_t)(el >= pm_rl.dur ? pm_rl.to[i]
            : pm_rl.from[i] + (int32_t)(((int32_t)pm_rl.to[i] - pm_rl.from[i]) * (int32_t)(el * 1024u / pm_rl.dur) >> 10));
    if (el >= pm_rl.dur)
        pm_rl.n = 0;
}
/* the held macros in the order the ARP wants, into seq[]; returns how many */
static uint32_t pm_order(uint8_t *seq)
{
    uint32_t i, j, n = pm_h.n, c = 0;
    uint8_t tmp[PM_NM];
    for (i = 0; i < n; i++)
        tmp[i] = pm_h.mac[i];
    if (pm_arp != 4u)                                    /* sorted up, unless the order pressed */
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && tmp[j - 1u] > tmp[j]; j--) {
                uint8_t x = tmp[j];
                tmp[j] = tmp[j - 1u];
                tmp[j - 1u] = x;
            }
    if (pm_arp == 1u)
        for (i = 0; i < n; i++)
            seq[c++] = tmp[n - 1u - i];
    else {
        for (i = 0; i < n; i++)
            seq[c++] = tmp[i];
        if (pm_arp == 2u)
            for (i = n >= 2u ? n - 2u : 0u; i > 0; i--)
                seq[c++] = tmp[i];
    }
    return c;
}
/* the moving parameters at this moment: called every input pass */
static void pm_update(void)
{
    uint32_t el, T, i, n = pm_h.n;
    pm_rl_step();
    perf_rel_sl = pm_rel ? (int32_t)(32768u / (pm_rel_ms() * (uint32_t)FS / 1000u + 1u) + 1u) : 0;
    if (!n) {
        pm_ui_c = pm_ui_a = 255;
        return;
    }
    el = fm1_ms - pm_h.t0;
    T = pm_time();
    if (n <= 2u) {                                       /* a fade: from A (what it was, or the first) to B */
        int a = n == 2u ? (int)pm_h.mac[0] : -1, c = (int)pm_h.mac[n - 1u];
        int32_t u;                                       /* 0..1024 */
        if (!T)
            u = n == 2u ? 0 : 1024;                      /* none: one macro is at once; two with no time hold the first */
        else if (pm_func == 0u)
            u = el >= T ? 1024 : (int32_t)(el * 1024u / T);
        else if (pm_func == 1u)
            u = (int32_t)((el % T) * 1024u / T);
        else {
            uint32_t ph = el % (2u * T);
            u = (int32_t)((ph < T ? ph : 2u * T - ph) * 1024u / T);
        }
        pm_ui_a = (uint8_t)a;
        pm_ui_c = (uint8_t)c;
        pm_ui_u = (int16_t)u;
        for (i = 0; i < pm_h.nb; i++) {
            int32_t va, vb;
            if (pm_h.bedit[i])
                continue;
            va = pm_mv(a, i);
            vb = pm_mv(c, i);
            pm_put(i, va + (((vb - va) * u) >> 10));
        }
    } else {                                             /* an arpeggio: a macro each fade time */
        uint8_t seq[2u * PM_NM];
        uint32_t cnt = pm_order(seq), step = el / (T ? T : pm_step_ms() * 2u), idx;
        if (pm_arp == 3u) {                              /* RANDOM: a hash of the step */
            uint32_t h = step * 2654435761u + 0x9E3779B9u;
            h ^= h >> 15;
            h *= 2246822519u;
            h ^= h >> 13;
            idx = (h >> 8) % cnt;
        } else if (pm_func == 0u)
            idx = step < cnt ? step : cnt - 1u;
        else if (pm_func == 1u)
            idx = step % cnt;
        else {
            uint32_t ph = cnt > 1u ? step % (2u * cnt - 2u) : 0u;
            idx = ph < cnt ? ph : 2u * cnt - 2u - ph;
        }
        pm_ui_a = 255;
        pm_ui_c = seq[idx];
        pm_ui_u = (int16_t)(((el % (T ? T : pm_step_ms() * 2u)) * 1024u) / (T ? T : pm_step_ms() * 2u));
        for (i = 0; i < pm_h.nb; i++)
            if (!pm_h.bedit[i])
                pm_put(i, pm_mv((int)seq[idx], i));
    }
}
/* a macro key goes down */
static void pm_begin(uint32_t mac, uint32_t key)
{
    perf_mac_t *m;
    uint32_t i;
    if (pm_h.n >= PM_NM || mac >= PM_NM)
        return;
    for (i = 0; i < pm_h.n; i++)
        if (pm_h.mac[i] == mac)
            return;
    m = &pm_mac[mac];
    pm_h.mac[pm_h.n] = (uint8_t)mac;
    pm_h.key[pm_h.n] = (uint8_t)key;
    pm_h.n++;
    for (i = 0; i < m->n; i++) {
        pm_rl_snap(m->trk[i], m->id[i]);
        pm_badd(m->trk[i], m->id[i], *pm_ptr(m->trk[i], m->id[i]));
    }
    pm_h.t0 = fm1_ms;
    pm_update();
}
/* every parameter back to what it was */
static void pm_restore(uint32_t only_unused)
{
    uint32_t i, j, w = 0, rel = pm_rel_ms();
    if (rel)
        pm_rl_rebase();
    for (i = 0; i < pm_h.nb; i++) {
        int used = 0;
        if (only_unused)
            for (j = 0; j < pm_h.n && !used; j++)
                used = pm_find(&pm_mac[pm_h.mac[j]], pm_h.btrk[i], pm_h.bid[i]) >= 0;
        if (used) {                                      /* kept: moved down to the next place */
            pm_h.btrk[w] = pm_h.btrk[i]; pm_h.bid[w] = pm_h.bid[i]; pm_h.bedit[w] = pm_h.bedit[i]; pm_h.bval[w] = pm_h.bval[i];
            w++;
        } else if (rel && pm_rl.n < 32u && *pm_ptr(pm_h.btrk[i], pm_h.bid[i]) != pm_h.bval[i]) {   /* glides back */
            uint32_t r = pm_rl.n++;
            pm_rl.trk[r] = pm_h.btrk[i]; pm_rl.id[r] = pm_h.bid[i];
            pm_rl.from[r] = *pm_ptr(pm_h.btrk[i], pm_h.bid[i]); pm_rl.to[r] = pm_h.bval[i];
        } else
            *pm_ptr(pm_h.btrk[i], pm_h.bid[i]) = pm_h.bval[i];
    }
    if (rel) {
        pm_rl.t0 = fm1_ms;
        pm_rl.dur = rel;
    }
    pm_h.nb = (uint8_t)w;
}
/* a macro key comes up (the i-th held) */
static void pm_drop(uint32_t i)
{
    uint32_t j;
    for (j = i; j + 1u < pm_h.n; j++) {
        pm_h.mac[j] = pm_h.mac[j + 1u];
        pm_h.key[j] = pm_h.key[j + 1u];
    }
    pm_h.n--;
    pm_restore(pm_h.n ? 1u : 0u);                        /* what no held macro holds any more goes back; all of it with the last */
    if (!pm_h.n)
        pm_h.nb = 0;
    pm_h.t0 = fm1_ms;
}
static void pm_clear(void)                               /* OCT-: the macro held (alone) is emptied; its key stays down */
{
    if (pm_h.n != 1u)
        return;
    pm_restore(0u);
    pm_mac[pm_h.mac[0]].n = 0;
    pm_h.nb = 0;
}
/* the globals a macro can hold: the tempo and swing, the delay and the reverb, the master DJ filter and comb (not the actions, the clock mode, the names) */
static int pm_global_ok(uint32_t id)
{
    return id == G_BPM || id == G_SWING || (id >= G_DTIME && id <= G_CDEPTH) || id == G_MIDI || id == G_SYNC || id == G_INFO ||
           id == G_RTYPE;
}
/* a knob turned the value vp of track t from `old`: with one macro key down it is kept in that macro, and left alone by the fade */
static void pm_record(track_t *t, int16_t *vp, int16_t old)
{
    perf_mac_t *m;
    uint32_t id, tr = (uint32_t)(t - trk);
    int i, b;
    if (pm_h.n != 1u || *vp == old)
        return;
    if (vp >= song.g && vp < song.g + G_COUNT) {         /* a global: only the tempo and the swing */
        id = (uint32_t)(vp - song.g);
        tr = PM_G;
        if (!pm_global_ok(id))
            return;
    } else {
        if (vp < t->p || vp >= t->p + P_COUNT)
            return;
        id = (uint32_t)(vp - t->p);
        if (!motion_param(id) && !(id >= P_M1SRC && id <= P_M4AMT) && id != P_FTYPE && id != P_ESYNC)   /* (a macro can hold the LFO's DEST and AMT and the filter TYPE too) */
            return;
    }
    m = &pm_mac[pm_h.mac[0]];
    i = pm_find(m, tr, id);
    if (i < 0) {
        if (m->n >= PM_N) {
            ui_message("MACRO FULL");
            return;
        }
        i = (int)m->n++;
        m->trk[i] = (uint8_t)tr;
        m->id[i] = (uint8_t)id;
    }
    b = pm_badd(tr, id, old);
    if (b >= 0)
        pm_h.bedit[b] = 1;
    m->val[i] = *vp;
}





/* SEQ cursor: wraps inside the pattern length, the bank follows, a step entry ends */
static void cursor_set(int32_t c)
{
    int32_t len = TSEL->p[P_SLEN] > 0 ? TSEL->p[P_SLEN] : 1;
    ui.cursor = (uint8_t)((c % len + len) % len);
    ui.bank = (uint8_t)(ui.cursor / 16u);
    ui.entry_open = 0;
}

static void cursor_fix(void)                           /* LEN got shorter: onto the last step */
{
    if (ui.cursor >= (uint32_t)TSEL->p[P_SLEN])
        cursor_set(TSEL->p[P_SLEN] - 1);
}


/* SEQ > STEP / DETAIL follows the play position (1.6, #242, Discussion #238): a melodic pattern longer than the 16
 * steps on the screen shows the bank the playhead is in while the transport plays (ui.bank only: the cursor, the
 * pattern and the saved project are untouched; DRUM's grid keeps its bank, its keys edit what the grid shows). The
 * roll is left alone ("held") from the moment a knob moves the cursor or edits the cursor's step, or step entry has the
 * keys: the bank goes back to the cursor's, so what is being edited stays on the screen. Following starts again after
 * FOLLOW_MS without that, or once the playhead has gone one pass round the pattern, whichever is first. The transport
 * stopped, another track, or the roll left: nothing held any more */
#define FOLLOW_MS 3000u
static struct {
    uint32_t t;                                        /* fm1_ms of the last touch of the cursor's step */
    uint16_t prev, steps;                              /* the playhead at the last poll, the steps it has gone since */
    uint8_t hold;
} follow;

static void follow_hold(void)
{
    follow.hold = 1;
    follow.t = fm1_ms;
    follow.prev = TSEL->seq_idx;
    follow.steps = 0;
    ui.bank = (uint8_t)(ui.cursor / 16u);
}

/* every pass of ui_input() */
static void follow_poll(void)
{
    uint32_t len = (uint32_t)TSEL->p[P_SLEN], want;
    if (!song.playing || ui.home || ui.confirm || cur_page()->graph != GR_ROLL || drum_track(TSEL)) {
        follow.hold = 0;
        return;
    }
    if (len <= 16u)
        return;
    if (ui.entry_open)                                 /* step entry has the keys: stay at the cursor */
        follow_hold();
    if (follow.hold) {
        uint32_t idx = TSEL->seq_idx;
        follow.steps = (uint16_t)(follow.steps + (idx + len - follow.prev) % len);
        follow.prev = (uint16_t)idx;
        if (fm1_ms - follow.t < FOLLOW_MS && follow.steps < len)
            return;
        follow.hold = 0;
    }
    want = TSEL->seq_idx / 16u;
    if (want * 16u < len && want != ui.bank)
        ui.bank = (uint8_t)want;
}


/* a page button tapped (1.2, Discussion #153: SEQ STEP <-> AUTOMATION, GLO SONG (its only page: GLOBAL became HOME's
 * CLOCK, SYSTEM went to the MENU), SAVE USER -> PRESETS -> PHRASES -> PROJECT -> TOOLS (1.5, #173: PRESETS second; up
 * to 1.4.1 the last); HOME: home_tap). From another
 * family the family's page shown last (SAVE: USER); one that is not the family's any more (a page that moved, as GLOBAL
 * and SYSTEM in 1.2), or not shown for this track, gives the family's first */
static void open_family(uint32_t fam)
{
    if (!ui.home && cur_page()->fam == fam) {          /* same button again: next page */
        uint32_t n, i = ui.page;
        for (n = 0; n < NPAGES; n++) {
            i = (i + 1u) % NPAGES;
            if (PAGES[i].fam == fam && page_visible(i)) break;
        }
        ui.page = (uint8_t)i;
    } else if (fam == FAM_SAVE) {
        uint32_t i;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].graph == GR_USER) break;
        if (i < NPAGES)                               /* (SIMPLE has none) */
            ui.page = (uint8_t)i;                     /* SAVE enters the sound save screen directly */
    } else {
        uint32_t i = ui.fam_last[fam];
        ui.page = (uint8_t)(i && i < NPAGES && PAGES[i].fam == fam && page_visible(i) ? i : page_first(fam));
    }
    ui.fam_last[fam] = ui.page;
    ui.home = 0;
    page_entered();
}

static void go_home(void)
{
    ui.home = 1;
    ui.plk_src = 0;                              /* (parameter locks: HOME's four) */
    ui.entry_open = 0;
    ui.hot_t = 0;
    song.seq_mode = 0;
    ui.force = 1;
    help_start();
}

/* HOME tapped (1.2): HOME -> MIXER (up to 1.1.5 GLO's first page) -> CLOCK (up to 1.1.5 GLO > GLOBAL) -> HOME; from any
 * other page HOME. HOME's pages are FAM_TRK's, in PAGES order, always from the first (none remembered). Held: the menu
 * (ui_input) */
static void home_tap(void)
{
    uint32_t i = ui.page + 1u;
    (void)i;
    if (!ui.home && cur_page()->fam == FAM_TRK) {
        open_family(FAM_TRK);                         /* (the next, CLOCK wraps to VOLUME 1-4) */
    } else {
        ui.home = 0;
        ui.fam_last[FAM_TRK] = 0;
        open_family(FAM_TRK);
    }
    return;
    if (ui.home) {
        ui.fam_last[FAM_TRK] = 0;                     /* (open_family: the family's first, the MIXER) */
        open_family(FAM_TRK);
    } else if (cur_page()->fam == FAM_TRK && i < NPAGES && PAGES[i].fam == FAM_TRK) {
        open_family(FAM_TRK);                         /* (on HOME's page again: the next one) */
    } else {
        go_home();
    }
}



/* One-step UNDO of a load. A sound load (a factory or user preset, an engine jump, TOOLS INIT, the
 * editor's PRESET / G_ENGSEL / UP_LOAD) changes the sound only; a pattern load (SAVE > PHRASES) changes
 * the steps and the pattern parameters only. Each first copies the track as it was; SAVE held 0.7 s
 * swaps back what the loads changed (held again: the loads again), so steps recorded after a sound
 * load, or a sound edited after a pattern load, stay as they are. One copy for all tracks: the last
 * load wins. Loads in a row on one track with nothing changed in between (the PRESETS knob through the
 * list, one pattern after the other, an editor audition) keep the copy from before the first, so the
 * undo goes back past the whole browse. Not snapshotted: power-on, projects.
 * 1.1.5: a lock or automation edit by hand (a step held and a knob turned, a step's locks cleared, SEQ > AUTOMATION)
 * takes the copy too (UNDO_MOT: SAVE held puts the track's motion back, the sound and the steps stay; motion_undo_take).
 * A sound load keeps the track's motion on the parameters every engine has (load_end, motion.c motion_sound_loaded);
 * a pattern load drops it all (the steps it was on are gone) */
enum { UNDO_SOUND = 1, UNDO_PAT = 2, UNDO_MOT = 4 };
static struct {
    uint8_t trk;                 /* track + 1, 0 = nothing to undo */
    uint8_t keep;                /* the track is as the last load left it (undo.after): a next load keeps the copy */
    uint8_t what;                /* UNDO_SOUND | UNDO_PAT: what the loads since the copy changed (undo_swap) */
    uint8_t eng, preset, user, patn;
    int16_t p[P_COUNT];
    step_t step[NSTEP];
    motion_store_t motion_backup; /* one track only, swaps with the shared event pool on undo */
    uint32_t after;              /* track_sig right after the last load */
    uint32_t pat;                /* pat_sig[] of the copy */
    uint32_t from_h;             /* pat_from_h[], pat_from[] of the copy (the header's slot letter) */
    uint8_t from;
    uint32_t t_ms;               /* time of the last load (the editor's SETs after it belong to it) */
} undo __attribute__((section(".pool")));   /* (main loop only: off the audio code's .bss) */
static uint8_t undo_depth;       /* loads nest (an engine jump loads its first preset): the outer one counts;
                                  * felucca_init / project_load raise it to take no copy at all */
static uint32_t pat_sig[NTRK];   /* steps_sig of the pattern the last pattern load put into each track: such
                                  * steps, untouched, are replaced by the next pattern without asking */
static uint8_t pat_last[NTRK];   /* that pattern's list index + 1, 0 = none */
/* the header's slot letter (ui_draw.c head_slot): the PROJECT slot each track's working pattern came from (the last
 * PROJECT load or save: 1..4 = A..D; 0 none: power-on, a PHRASES pattern, the editor's restore) and pat_from_sig() of
 * it then: an edit since shows the letter with a mark */
static uint8_t pat_from[NTRK];
static uint32_t pat_from_h[NTRK];
static uint8_t load_from;        /* a sound load (the outer one): the engine before it (load_end) */
static uint8_t load_mo;          /* .. the track's motion paused while the sound changes: bit 0 set, bit 1 it was ON */

static uint32_t fnv(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
static uint32_t steps_sig(const track_t *t) { return fnv(2166136261u, t->step, sizeof t->step); }
static uint32_t track_sig(const track_t *t)      /* the sound (an FM6 track's patch too), the steps */
{
    uint8_t id[3] = {t->eng_req, t->preset, t->user};
    uint32_t h = fnv(fnv(steps_sig(t), t->p, sizeof t->p), id, 3), k = trk_index(t);
    for (uint32_t j = 0; j < motion.count; j++)
        if ((motion.event[j].place >> 6) == k) h = fnv(h, &motion.event[j], sizeof motion.event[j]);
    return h ^ ((motion.on >> k) & 1u);
}
static uint32_t motion_sig(const track_t *t)     /* the track's motion only (the sound moves while it plays) */
{
    uint32_t h = 2166136261u, k = trk_index(t);
    for (uint32_t j = 0; j < motion.count; j++)
        if ((motion.event[j].place >> 6) == k) h = fnv(h, &motion.event[j], sizeof motion.event[j]);
    return h ^ ((motion.on >> k) & 1u);
}
/* the track's pattern as a song plays it from a slot: the steps, LEN DIV SWING GATE, QUANTIZE, the automation (the
 * editable ones: while a song plays the header shows the section's slot instead) */
static uint32_t pat_from_sig(const track_t *t)
{
    int16_t q = t->p[P_SQNT];
    uint32_t h = fnv(fnv(steps_sig(t), &t->p[P_SLEN], 4u * sizeof t->p[0]), &q, sizeof q);
    return h ^ motion_sig(t);
}

/* load_begin, the copy taken: a pattern load drops the track's motion (its steps go); a sound load pauses it (PLAY
 * OFF while the sound changes under it) and load_end keeps what still means the same (motion_sound_loaded) */
static void load_motion_hold(track_t *t, uint32_t what)
{
    if (what & UNDO_PAT) {
        motion_reset(t);
        load_mo = 0;
        return;
    }
    load_from = t->eng_req;
    load_mo = (uint8_t)(1u | (motion_enabled(t) ? 2u : 0u));
    motion_set_enabled(t, 0);
}

/* MOMENTARY (ui_input.c mom_turn: LFO held, the knobs' changes go back when it is let go): the values turned, where
 * they are (a track's p[], song.g[]) and what they were. In the main loop's pool (no .bss) */
#define MOM_N 8u
static struct {
    uint8_t n;                                    /* values held, 0 = none */
    int16_t *vp[MOM_N];
    int16_t v[MOM_N];
} mom __attribute__((section(".pool")));
static void mom_restore(void)                     /* LFO let go: every value back */
{
    while (mom.n) {
        mom.n--;
        *mom.vp[mom.n] = mom.v[mom.n];
    }
}
static void mom_back(const track_t *t)            /* a sound load into t (0: a project): its values back first */
{
    uint32_t i, k = 0;
    for (i = 0; i < mom.n; i++)
        if (!t || (mom.vp[i] >= t->p && mom.vp[i] < t->p + P_COUNT) || (mom.vp[i] >= song.g && mom.vp[i] < song.g + G_COUNT)) {
            *mom.vp[i] = mom.v[i];                /* (a global: back too, before the load's undo copy) */
        } else {
            mom.vp[k] = mom.vp[i];
            mom.v[k++] = mom.v[i];
        }
    mom.n = (uint8_t)k;
}
static void load_begin(track_t *t, uint32_t what)
{
    uint32_t i = trk_index(t);
    if (what & UNDO_SOUND) {
        mom_back(t);                              /* (MOMENTARY: the sound as it was, then the load) */
    }
    if (undo_depth++)
        return;
    motion_restore(t);
    if (undo.keep && undo.trk == i + 1u && !(undo.what & UNDO_MOT) && track_sig(t) == undo.after) {
        undo.what |= (uint8_t)what;               /* browsing on: the copy from before the first load stays */
        load_motion_hold(t, what);
        return;
    }
    undo.trk = (uint8_t)(i + 1u);
    undo.what = (uint8_t)what;
    undo.eng = t->eng_req;
    undo.preset = t->preset;
    undo.user = t->user;
    memcpy(undo.p, t->p, sizeof undo.p);
    memcpy(undo.step, t->step, sizeof undo.step);
    undo.pat = pat_sig[i];
    undo.from = pat_from[i];
    undo.from_h = pat_from_h[i];
    undo.patn = pat_last[i];
    motion_snapshot_track(t, &undo.motion_backup);
    load_motion_hold(t, what);
}

static void load_end(track_t *t)
{
    if (--undo_depth)
        return;
    if (load_mo & 1u) {                           /* a sound load: the motion on the common parameters stays */
        (void)motion_sound_loaded(t, load_from, t->eng_req);
        if ((load_mo & 2u) && motion_count(t))
            motion_set_enabled(t, 1);
        load_mo = 0;
    }
    motion_rebase(t);
    undo.after = track_sig(t);
    undo.keep = 1;
    undo.t_ms = fm1_ms;
}


/* 1.1.5: a lock / automation edit by hand on track t is about to happen: the undo copy of the track as it is (as
 * ui_tools.c tl_undo_take, UNDO_MOT: the swap puts back its motion only). A knob turned on (the same tag, not 0:
 * the same knob of the same gesture), on the same track, nothing else changed and less than 1.5 s apart, shares the
 * copy from before its first detent; tag 0 (an add, a delete, a clear, a kind) always takes its own.
 * motion_undo_done after each */
static uint32_t undo_mot_tag;
/* what an edit's copy compares (undo.after): the motion; with the steps (UNDO_PAT: step_undo_take) the steps too */
static uint32_t edit_sig(const track_t *t) { return undo.what & UNDO_PAT ? motion_sig(t) ^ steps_sig(t) : motion_sig(t); }
static void edit_undo_take(track_t *t, uint32_t tag, uint32_t what)
{
    uint32_t i = trk_index(t);
    if (tag && tag == undo_mot_tag && undo.keep && undo.trk == i + 1u && undo.what == what &&
        edit_sig(t) == undo.after && fm1_ms - undo.t_ms < 1500u)
        return;
    undo_mot_tag = tag;
    undo.trk = (uint8_t)(i + 1u);
    undo.what = (uint8_t)what;
    undo.eng = t->eng_req;
    undo.preset = t->preset;
    undo.user = t->user;
    memcpy(undo.p, t->p, sizeof undo.p);
    memcpy(undo.step, t->step, sizeof undo.step);
    undo.pat = pat_sig[i];
    undo.from = pat_from[i];
    undo.from_h = pat_from_h[i];
    undo.patn = pat_last[i];
    motion_snapshot_track(t, &undo.motion_backup);
    undo.keep = 0;
}
static void motion_undo_take(track_t *t, uint32_t tag) { edit_undo_take(t, tag, UNDO_MOT); }
static void motion_undo_done(track_t *t)
{
    undo.after = edit_sig(t);
    undo.keep = 1;
    undo.t_ms = fm1_ms;
}

static int param_kept(uint32_t i);




static void track_defaults_steps(track_t *t)
{
    uint32_t i;
    motion_reset(t);
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
}







/* the track's settings, not the sound's: what a sound load (factory or user preset, an engine jump,
 * TOOLS INIT) leaves alone. The mix (LEVEL, PAN, MUTE: the TRACKS faders), the arpeggiator (ARP, ARP 2),
 * the scale and key map (SCL), the pattern parameters (LEN, DIV, SWING, GATE; 1.2: QUANTIZE) and the SLICER
 * insert, which chops whatever the track plays in time with its sequencer */
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_CHRD || i == P_VOIC || i == P_SQNT;
}

/* a retired preset kept as an alias, so stored preset numbers stay valid: SAMPLE 1, once TRANH, is PIANO
 * (tools/gen_samples.py SMP_SET_ORIG). It loads as the original; browsing skips it. -> the preset k stands for.
 * (SAMPLE 4, once PERC, is past SMP_NPRESETS: apply_preset_to loads it as DRUM) */
static uint32_t preset_orig(const engine_t *e, uint32_t k)
{
    (void)e;
    return k;
}




static void set_engine_of(track_t *t, uint32_t ei);
/* preset pi of the engine the track asked for: the sound only (not the steps, not param_kept) */
/* MACH turned: the machine's own starting sound (its factory preset's values and envelope), the rest of the track kept */
static void machine_defaults(track_t *t)
{
    const machine_t *m = trk_machine(t);
    const engine_t *e = ENGINES[m->engine];
    const preset_t *pr = &e->presets[m->preset % e->npresets];
    uint32_t i;
    for (i = 0; i < 7u; i++)
        t->p[P_E0 + i] = (int16_t)pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    if (m->engine == ENGI_ANALOG) {                   /* an 808: its LEVEL (the eighth knob), AGE */
        t->p[P_LN0] = AN_DEF[m->sub % AN_N][7];
        t->p[P_LN1] = 0;                              /* AGE: clean at the start (the user's choice) */
    } else if (m->engine == ENGI_SAMPLER) {           /* the sampler: LEVEL, GATE, track n's slot n */
        t->p[P_LN0] = 100;
        t->p[P_LN2] = TP[P_LN2].def;
        t->p[P_E5] = (int16_t)(trk_index(t) % SMP_MAX);
    }
}
/* SIMPLE: track t becomes machine mach (the PRESETS knob): its engine (the old one's voices fade, voice.c
 * engine_block) and its starting sound; the rest of the track (filter, sends, LFO, sequence) stays */
static void machine_set(track_t *t, uint32_t mach)
{
    const machine_t *m = &MACHINES[mach % NMACH];
    fm1_irq_off();                                    /* (the ISR sees the old engine or the new one, never a mix) */
    t->eng_req = m->engine;
    t->p[P_E7] = m->sub;
    machine_defaults(t);
    fm1_irq_on();
    ui.force = 1;
}
static void apply_preset_to(track_t *t, uint32_t pi)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i;
    load_begin(t, UNDO_SOUND);
    panic_req |= (uint8_t)(1u << trk_index(t));       /* MONO/POLY may change: release what sounds */
    t->user = 0;
    if (t == TSEL)
        sync_reload = 1;
    if (!e->npresets) {
        load_end(t);
        return;
    }
    pi = preset_orig(e, pi % e->npresets);
    t->preset = (uint8_t)pi;
    for (i = 0; i < P_E0; i++)                        /* the rest of the sound to its defaults: a preset */
        if (!param_kept(i))                           /* sounds the same after any edit */
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = (int16_t)e->presets[pi].e[i];
    t->p[P_ATK] = e->presets[pi].env[0];
    t->p[P_DEC] = e->presets[pi].env[1];
    t->p[P_SUS] = e->presets[pi].env[2];
    t->p[P_REL] = e->presets[pi].env[3];
    t->p[P_ED_FLT] = e->presets[pi].fenv;
    t->p[P_VOICE] = e->presets[pi].mono ? V_LEGATO : V_POLY;   /* mono presets keep the legato feel */
    {   /* the sends */
        static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
        const preset_t *pr = &e->presets[pi];
        for (i = 0; i < 4u; i++)
            t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
    }
    load_end(t);
}

/* the engine's defaults and its first preset. With the audio IRQ off: the ISR sees the old engine with
 * its values or the new one with its own (voice.c engine_block), never one with the other's */
static void set_engine_of(track_t *t, uint32_t ei)
{
    const engine_t *e = ENGINES[ei % NENGINES];
    uint32_t i;
    load_begin(t, UNDO_SOUND);
    fm1_irq_off();
    t->eng_req = (uint8_t)(ei % NENGINES);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    apply_preset_to(t, 0);
    fm1_irq_on();
    load_end(t);
}

static void set_engine(uint32_t ei) { set_engine_of(TSEL, ei); }

/* CLEAN INIT (1.5.5, #190): the engine's basic sound, nothing added: E1..E8 at the engine's own defaults (not its first
 * preset's), every other sound value at its default (TP: no FX sends DIST CHO DLY REV, INSERT OFF, no LFO or ENV DEST
 * amounts, the MOD slots off, no GLIDE, POLY), the ENV neutral (ATK 0, SUS 100 %, a short REL; DRUM: its kit's own, so
 * the hits ring as they should); FM6 the init voice (one sine carrier, ALGORITHM 1; SLOT OWN). What INIT keeps stays
 * (param_kept: LEVEL PAN MUTE, ARP, SCL, the sequence, SLICER, the chord keys); the steps stay. One undo, as INIT
 * (SAVE held). INIT itself is unchanged (the engine's defaults and its first preset) */
#define CLEAN_REL 40                                   /* ~18 ms */
static void clean_engine_of(track_t *t)
{
    uint32_t ei = t->eng_req % NENGINES, i;
    const engine_t *e = ENGINES[ei];
    load_begin(t, UNDO_SOUND);
    set_engine_of(t, ei);                             /* (nested in this load: one undo copy, from before) */
    fm1_irq_off();
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i))
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = TP[P_SUS].max;
    t->p[P_REL] = CLEAN_REL;
    fm1_irq_on();
    load_end(t);
}
static void clean_engine(void) { clean_engine_of(TSEL); }

/* MIDI Program Change (1.5.1, #179 by renebohne): program n on track t, as the PRESETS list numbers the engine's sounds
 * from 0: its n-th preset, the aliases skipped (preset_orig), loaded as from there (the track's own settings kept, one
 * undo for a run of them). DRUM: its n-th KIT, the retired variants skipped (enum_orig); FM6: its presets (F1..F8),
 * then the voice bank's B1..B32 through SLOT as the knob loads them (fm6_poll; the rest of the sound stays), an empty
 * one ignored. A program past the last is ignored, not wrapped: a number is always the same sound */
static volatile uint8_t *const mpc_io = ui.mpc;
static void midi_pc_load(track_t *t, uint32_t n)
{
    if (n < ENGINES[t->eng_req % NENGINES]->npresets)
        apply_preset_to(t, n);
}
static void midi_pc_poll(void)                       /* main loop (ui_input) */
{
    uint32_t k, v;
    for (k = 0; k < NTRK; k++) {
        if (!ui.mpc[k])
            continue;
        fm1_irq_off();                               /* (the ISR may note another meanwhile) */
        v = ui.mpc[k];
        ui.mpc[k] = 0;
        fm1_irq_on();
        midi_pc_load(&trk[k], v & 127u);
        ui.force = 1;
    }
}

static void track_defaults(track_t *t)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    track_defaults_steps(t);
}


/* HOME: the track KNOB k edits (MENU > HOME LEVELS, 1.5: track k; else the selected one) and what: the engine's four
 * main parameters, or (LEVELS) the track's LEVEL, as the GLO layer's knobs */
static track_t *home_trk(uint32_t k) { return home_levels() ? &trk[k & 3u] : TSEL; }
static const param_desc_t *home_param(uint32_t k, int16_t **vp)
{
    uint32_t id = home_levels() ? (uint32_t)P_LEVEL : ENGINES[TSEL->eng_req % NENGINES]->knob[k & 3u];
    *vp = &home_trk(k)->p[id];
    return track_desc(home_trk(k), id);
}

/* select track i (KNOB 1 on TRACKS, the editor): its sound, pages and pattern from now on */
static void track_select(uint32_t i)
{
    if (i >= NTRK || i == song.sel)
        return;
    song.sel = (uint8_t)i;
    if (FELUCCA_SIMPLE && song.rec)                  /* recording follows the track: change tracks, keep recording */
        song.rec = (uint8_t)(1u << i);
    ui.entry_open = 0;
    ui.hot_t = 0;
    ui.ev_keep = 0;                              /* (AUTOMATION: a row kept at its default was the other track's) */
    ui.cursor = 0;
    ui.bank = 0;
    follow.hold = 0;                             /* (1.6, #242: the roll follows the new track's playhead) */
    sync_reload = 1;
    ui.force = 1;
}


