/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI input: LEDs, knobs and buttons, SEQ step entry, panel setup. */
/* ----------------------------------------------------------- LEDs --- */
/* The LED picture is built off-line and copied one byte per column: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker. */
static uint8_t led_pos[41];                        /* (col << 3) | row bit, 0xFF = none */

static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}

static void led_put(uint8_t *nl, uint32_t id, int on)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF && on)
        nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
}

/* PLAY's second, green LED: not in the key matrix (no key there), found on the hardware at column 8, row PA9 (bit 1) */
#define LED_PLAY_GREEN ((8u << 3) | 1u)
static void led_clear(uint8_t *nl, uint32_t id)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF)
        nl[q >> 3] &= (uint8_t)~(1u << (q & 7u));
}

static const uint8_t FAM_BTN[FAM_COUNT] = {B_HOME, B_ENV, B_LFO, B_EDIT, B_HOME, B_FX, B_GLO, B_SAVE,   /* (the FX and EDIT
                                                                     * buttons swapped: the FX button is the engine's
                                                                     * sound pages (OSC), the EDIT button the effects) */
                                           B_ARP, B_SEQ, B_HOME,    /* MIXER: HOME's (1.2); REC is transport; */
                                           B_SCL};                  /* FAM_SCL has no pages (HOME's: never tapped); the SCL
                                                                     * button opens FAM_FLT, the engine's FLT / TONE page */

static uint32_t cur_fam(void) { return ui.home ? FAM_HOME : cur_page()->fam; }

static int32_t accel(uint32_t role, int32_t s, int32_t range);
/* the OCT LEDs, bit 0 OCT- lit, bit 1 OCT+ lit, OCT_BREATH OCT+ breathing (can be pressed: dark .. ~60 %,
 * hal/fm1_input.h fm1_led_breath, #119). In the dialogs and on action pages OCT- (back) is lit and OCT+ breathes
 * while it would do something; the menu: on a value row OCT- and OCT+ both breathe while that way has a value to go to,
 * on CALIBRATION / ABOUT OCT+ breathing alone (opens), in ABOUT OCT- lit (back); elsewhere the octave shift */
#define OCT_BREATH 4u
#define OCT_BREATH_DN 8u                                /* OCT- breathing (the menu: the previous value) */
/* MOMENTARY (1.2, Discussions #72 / #73): LFO held, the knobs' changes are temporary. KNOB 1..4 on HOME and on the
 * sound and FX pages (a value of the selected track or a global sound / FX one, as edit_param sets it; MIXER's LEVEL
 * PAN REV MUTE too, #195; not on the action pages, the lists, STEP, AUTOMATION or SONG, where the knobs pick) change
 * their value as ever, and each value turned is put back as it was when LFO is let go: hold LFO, sweep the filter,
 * let go, it snaps back. OCT+ while
 * LFO is still held KEEPS them (the changes stay, nothing is put back; OCT+ breathes while something would go back;
 * LFO has no LED). LFO's tap (its page) is dropped once any knob turned with it; LFO alone still opens LFO. Temporary
 * turns are not recorded as automation (motion_capture) and take no undo; a sound or project load meanwhile (the
 * PRESETS knob, the editor, a song's section) puts that track's values (and the global ones) back first (mom_back: the
 * undo copy holds the sound as it was, the load's sound stays after). The menu, a dialog, NAME or a layer opening put
 * them back too. Up to MOM_N values at a time ("MOMENTARY FULL" past them: the turn does nothing). LFO held did
 * nothing (it is no layer; OCT- is the octave and a layer's UNDO, so not it), so no gesture changes: LFO + a knob
 * turned opened LFO's page on release before, and edited the page under it for good. The values: ui.c mom */
static int mom_down(void)                              /* SIMPLE: OCT+ held (OCT- keeps); else LFO held (OCT+ keeps) */
{
    return ((fm1_in.buttons >> panel.btn[FELUCCA_SIMPLE ? B_OCTUP : B_LFO]) & 1u) && !ui.confirm && !ui.uboot;
}
#define MOM_KEEP (FELUCCA_SIMPLE ? B_OCTDN : B_OCTUP)
/* KNOB k turned s with LFO held: 1 = taken (a momentary value, or refused full), 0 = not a momentary value (the turn
 * goes as ever: the column picks, steps, acts) */
static int mom_turn(uint32_t k, int32_t s)
{
    const page_t *pg = cur_page();
    const param_desc_t *d;
    int16_t *vp = 0;
    uint32_t i, g;
    const track_t *t = ui.home ? home_trk(k) : TSEL;     /* (HOME LEVELS: KNOB k is track k's) */
    if (ui.home) {
        d = home_param(k, &vp);
    } else if (pg->scope == SC_TRK && !lock_held()) {  /* MIXER (#195): the selected track's LEVEL PAN REV MUTE */
        static const uint8_t MIX_ID[4] = {P_LEVEL, P_PAN, P_REV, P_MUTE};   /* (tracks_edit's knobs) */
        vp = &TSEL->p[MIX_ID[k & 3u]];
        d = &TP[MIX_ID[k & 3u]];
    } else {
        if (lock_held() || !(pg->scope == SC_TRACK || pg->scope == SC_ENGINE || pg->scope == SC_GLOBAL) ||
            pg->graph == GR_EVENTS || pg->graph == GR_SONG || pg->graph == GR_BROWSE || pg->graph == GR_USER ||
            pg->graph == GR_PATS || (pg->graph == GR_SLICES && k < 2u) || (pg->graph == GR_MOD && k == 0u))
            return 0;
        d = page_desc(pg, k, &vp);
    }
    g = vp && vp >= song.g && vp < song.g + G_COUNT ? (uint32_t)(vp - song.g) : G_COUNT;
    if (!d || !vp || d->max == d->min ||
        !((vp >= t->p && vp < t->p + P_COUNT) || (g <= G_CDEPTH && g != G_CLOCK) || g == G_RTYPE))
        return 0;                                      /* (only the values a sound and the FX are: no CLK, no actions) */
    for (i = 0; i < mom.n && mom.vp[i] != vp; i++)
        ;
    if (i == mom.n) {                                  /* the first turn of this value: what it was */
        if (mom.n >= MOM_N) {
            ui_message("MOMENTARY FULL");
            return 1;
        }
        mom.vp[i] = vp;
        mom.v[i] = *vp;
        mom.n++;
        if (i == 0u)
            ui_message(FELUCCA_SIMPLE ? "TEMP  [OCT-] KEEP" : "MOMENTARY  [OCT+] KEEP");
    }
    *vp = (int16_t)param_turn(d, *vp, accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
    return 1;
}


static uint32_t oct_leds(void)
{
    if (ui.confirm)                                     /* a dialog: OCT- cancels, OCT+ (breathing) does it */
        return 1u | OCT_BREATH;
    if (mom.n && mom_down())                            /* MOMENTARY: OCT+ (SIMPLE: OCT-) keeps the changes */
        return FELUCCA_SIMPLE ? 2u | OCT_BREATH_DN : (song.octave < 0 ? 1u : 0u) | OCT_BREATH;
    return (song.octave < 0 ? 1u : 0u) | (song.octave > 0 ? 2u : 0u);
}


/* SIMPLE, the step keys (SEQ): the white keys light where the selected track has a step (the playing step inverted),
 * the black keys the selected track */
static int gbs_trigless(const track_t *t, uint32_t p)   /* step p: no note, a lock (a trigless lock) */
{
    return p < NSTEP && !(t->step[p].time == ST_NOTE && t->step[p].n) &&
           (t->step[p].time == ST_LOCK || ((motion_lock_steps(trk_index(t)) >> p) & 1u));
}
static uint32_t fxl_leds(void)                           /* the FX lane: its trigs (the play position flips one), key 7 */
{
    uint32_t k, m = 0, ph = song.playing ? clk_step & 15u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (key_black(k) ? p == NTRK : (((fxl.trig >> p) & 1u) ^ (uint32_t)(p == ph)) != 0u)
            m |= 1u << k;
    }
    return m;
}
static uint32_t sgrid_leds(uint32_t *br)                 /* (*br: the trigless steps, breathing) */
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, len = (uint32_t)t->p[P_SLEN];
    if (fxl_on())
        return fxl_leds();
    uint32_t ph = song.playing && t->seq_idx < len && t->seq_idx < 16u ? t->seq_idx : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), on;
        if (!key_black(k)) {
            on = p < len && t->step[p].time == ST_NOTE && t->step[p].n != 0u;
            on ^= (uint32_t)(p == ph);
            if (br && p < len && gbs_trigless(t, p))
                *br |= 1u << k;
        } else {
            on = p < NTRK ? p == song.sel : 0u;
        }
        m |= on << k;
    }
    return m;
}


/* the DRUM grid's keys that do something, bit k = key k (they glow with the idle LEDs, ui_leds): the page's steps
 * within LEN, the lane keys, ACC, the page keys while there is more than one page */
static uint32_t grid_glow(void)
{
    uint32_t k, m = 0, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        m |= (uint32_t)(!key_black(k) ? ui.bank * 16u + p < len : p < NLANE || p == GK_ACC || len > 16u) << k;
    }
    return m;
}

/* an ARP playing on any part flashes the ARP button on the beat, the bar's first beat longer: 1 lit, 0 dark,
 * 2 no ARP playing */
static uint32_t arp_led(void)
{
    uint32_t k, on = 0, b = beat_samples();
    for (k = 0; k < NPART; k++)
        on |= arp_am(&trk[k]) && trk[k].nheld;
    return !on ? 2u : beat_pos < (beat_n ? b / 6u : b / 2u);
}

/* Discussion #81: the keys of the notes MIDI IN (USB and TRS, routed by ROUT) holds on track t, bit k = key k: as
 * play_leds, where the keys play that note at the octave now, the lowest key that gives it; a note no key plays is
 * not shown. Read from midi_control.c's own state (midi_note_held: the notes each channel holds for the track, the
 * pedal's too, and the tones of MIDI chords): no state here; nothing to do while MIDI holds none of the track's */
static uint32_t midi_leds(const track_t *t)
{
    uint32_t k, note, m = 0, seen[4] = {0, 0, 0, 0};
    if (!midi_owners[trk_index(t)])
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        if (note > 127u || ((seen[note >> 5] >> (note & 31u)) & 1u))
            continue;                                   /* (silent, or a lower key gives it) */
        seen[note >> 5] |= 1u << (note & 31u);
        m |= (uint32_t)midi_note_held(t, note) << k;
    }
    return m;
}

/* the keys of the notes the selected track's sequencer and ARP sound now (#38), bit k = key k: where the keys
 * play that note (kb_map: the octave, TRN, QNT, an engine's own map), the lowest key that gives it (QNT SNAP
 * rounds the keys above down onto it); a note no key plays is not shown. A snapshot of the ISR's seq_notes /
 * arp_note (arp_ch): no state of its own, nothing to do while nothing sounds */
static uint32_t play_leds(void)
{
    const track_t *t = TSEL;
    uint8_t s[4 + NLANE + 4];
    uint32_t n = t->seq_n < 4u + NLANE ? t->seq_n : 4u + NLANE, i, k, note, used = 0, m = 0, hit;
    for (i = 0; i < n; i++)
        s[i] = t->seq_notes[i];
    if (t->arp_note)
        s[n++] = t->arp_note;
    for (i = 0; i < 3u; i++)                            /* (the ARP's CHORD) */
        if (t->arp_ch[i])
            s[n++] = t->arp_ch[i];
    for (k = 0; n && k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0, hit = 0; i < n; i++)
            if (s[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                hit = 1;
            }
        m |= hit << k;
    }
    return m | midi_leds(t);
}

/* Discussion #89: on SEQ > STEP (the piano roll; the DRUM grid has its own map) while stopped, the keys of the
 * notes stored in the cursor step, bit k = key k: as play_leds, where the keys play that note now (kb_map: the
 * octave, TRN, QNT), the lowest key that gives it; a note no key plays is not shown, nor a REST step's. Playing,
 * the keys show what plays (play_leds) instead. Read from the step: no state of its own */
static uint32_t step_leds(void)
{
    const track_t *t = TSEL;
    const step_t *st;
    uint32_t i, k, note, used = 0, m = 0;
    if (ui.home || ui.confirm || !song.seq_mode || cur_page()->graph != GR_ROLL || song.playing ||
        ui.cursor >= NSTEP)
        return 0;
    st = &t->step[ui.cursor];
    if (!st->n || st->time == ST_REST)
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0; i < st->n && i < 4u; i++)
            if (st->note[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                m |= 1u << k;
            }
    }
    return m;
}

/* 1: the keys show a map of their own (NAME, a layer's map: SCL's scale, FX; the DRUM grid, SLICES), lit or
 * dark; 0: the keys held and the notes playing, over the idle glow */
static int keys_own(void)
{
    return grid_on();
}

/* Discussion #127 (1.2): MENU > SCALE LEDS ON, the keys of the selected track's scale (ROOT, SCALE), bit k = key k, as
 * the keys play now: the note a key gives with TRN (QNT SNAP / SEQ: the key's own note; the others round down onto
 * the scale); QNT WHITE every white key (they walk the scale; the black ones are silent). *root: the keys of the
 * scale's root. None where the engine maps the keys itself (DRUM's GM map, the slices): 0 there, and OFF */
static uint32_t scale_leds(uint32_t *root)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t k, m = 0, r = 0, mask = scale_mask(t), note;
    int32_t pc;
    *root = 0;
    if (!(ui_rec_prefs & (PREF_SCALE_LEDS >> 8)))
        return 0;
    for (k = 0; k < 27u; k++) {
        if (e->keys && e->keys(t, k) >= 0)
            continue;
        if (t->p[P_QUANT] == QN_WHITE) {
            if ((note = kb_map(t, k)) > 127u)
                continue;                               /* (a black key: silent) */
            pc = (int32_t)note - KB_TRN(t);         /* (WHITE walks the scale before TRN) */
        } else {
            pc = 53 + (int32_t)k + KB_TRN(t);
        }
        pc = ((pc - t->p[P_ROOT]) % 12 + 12) % 12;
        m |= ((mask >> pc) & 1u) << k;
        r |= (uint32_t)(pc == 0) << k;
    }
    *root = r & m;
    return m;
}

static uint32_t pm_leds(void);
static uint32_t trk_keys(int playing);
static uint32_t par_leds(void);
static uint32_t kit_leds(void);
static void layer_kill(void);                         /* (ui_buttons.c) */
/* MENU > ANIM IDLE (1.2, Discussion #135): after IDLE_LEDS_MS without panel input (ui.c scrn.idle: a button or key
 * pressed or held, a knob turned; what SCREEN OFF counts) the LEDs play the idle animation (hal/fm1_led_anim.h
 * fm1_idle_level: a soft light drifting over the keys and back, the buttons breathing with it; the scan draws it,
 * hal/fm1_input.h fm1_led_idle_start). Only with nothing going on: stopped (no count-in, no song, nothing armed or
 * waiting to start), no layer (held or locked), menu, dialog, NAME or UPDATE countdown, no key held or lit by the
 * selected track (MIDI IN notes), not with ANIM OFF. Any input ends it at once and is not swallowed (as SCREEN OFF's
 * wake would be): a key or a button down ends it in the scan before its next frame (the press acts as ever), a knob
 * here (the turn acts as ever); so does the transport starting (MIDI Start, the editor) or anything above. 1 = it has
 * the LEDs (ui_leds writes none). A dialog or the count-in count as input, as for SCREEN OFF (ui.c scr_frame) */
#define IDLE_LEDS_MS 60000u
static int idle_leds(void)
{
    int due = (ui_idle & 1u) && !(ui_prefs & PREF_ANIM_OFF) && !song.playing && !seq_counting() &&
              !song.rec && !transport_req && !ui.confirm && !ui.uboot &&
              !(fm1_in.notes | fm1_in.buttons) && !panel_moved && !play_leds() && fm1_ms - scrn.idle >= IDLE_LEDS_MS;
    if (!due) {
        if (fm1_led_idle_on())
            fm1_led_anim_stop();
        return 0;
    }
    if (!fm1_led_anim_on())                            /* (not over the power-on sweep) */
        fm1_led_idle_start(settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO);
    return fm1_led_idle_on();
}
/* the key LEDs, bit k = key k: NAME's keys, the layer's map, the DRUM grid, else the keys held and the notes
 * the selected track's sequencer, ARP and MIDI IN play, on STEP while stopped the cursor step's notes (#89; and
 * on SLICES the keys of the selected slice; on SONG the key of the cell's slot, 1.2) */
static uint32_t key_leds(uint32_t *br)                 /* (*br: a layer's keys that breathe, layer_leds) */
{
    uint32_t c;
    if (br)
        *br = 0;
    c = gbsv_keys_on() ? gbsv_leds() : sgrid_on() ? sgrid_leds(br) : par_on() ? par_leds() | (fm1_in.notes & ~kb_layer) :
        kit_on() ? kit_leds() | (fm1_in.notes & ~kb_layer) :
                 (fm1_in.notes & ~kb_layer) | play_leds() | step_leds() | (pm_page() ? pm_leds() : 0u);
    if (FELUCCA_SIMPLE && kb_octup && !gbsv_keys_on())  /* OCT+ held (the mutes): the tracks not muted lit */
        c = (c & ~trk_keys(0)) | trk_keys(1);
    return c;
}

/* The LEDs: lit = active (the page's family, PLAY / REC running, the keys held or playing, a map's keys), the ARP
 * beat flashes, the ones that can be pressed breathe (#119: the open layer's button, OCT+ when it would act, a
 * layer's keys; dark .. ~60 % of lit and back, hal/fm1_input.h fm1_led_breath, in every mode: never a hard blink in
 * a dark room; DIM LO up to ~30 %, DIM HI OFF INV ~60 %; with LEDS OFF it is the only glow; INV leaves it as it is,
 * between the dark active ones and the lit idle ones), every other button and key glows dim (#35: the
 * buttons of the black FM-1 can be found in the dark; hal/fm1_input.h fm1_led_dim, a short pulse each frame).
 * MENU > LEDS: DIM HI (default) that glow, DIM LO a darker one (fm1_led_dim_level), OFF no glow (as 1.0); INV
 * turns it around, as the stock firmware: the idle ones fully lit, the active ones dark, no glow (the ARP beat:
 * lit / dark). The keys' own maps (keys_own) stay lit or dark in every mode: their dark keys read as dark; only the
 * DRUM grid's keys that do something (grid_glow) glow under it, so STEP on a DRUM track is never a dark
 * keyboard (the steps of an empty pattern); on it the first step of each four (grid_beats: 1 5 9 13 of a page, where
 * no hit shows) glows steadier and brighter, so a bar's beats can be counted: DIM HI / DIM LO the mid level
 * (hal/fm1_input.h fm1_led_mid: lit 1 frame in 8, DIM LO in 12, over the glow), OFF and INV the glow alone (no
 * other key glows there). 1.2, MENU > SCALE LEDS ON (scale_leds; not over a map of the keys' own, nor on a DRUM or
 * SLICE track): the keys of the selected track's scale glow and the others are dark, the root's at the mid level, in
 * DIM HI, DIM LO and OFF alike (asked for: OFF only takes the other glow away); INV turns it around as it does the
 * rest: the root's keys lit, the scale's at the mid level, the others glowing; the keys held and playing as ever
 * (lit, INV dark). Each picture is built off-line and copied one byte per column, the
 * glow first: an LED going from lit to dim never has a dark frame */
static void ui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, own[FM1_NCOL] = {0}, og[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0};
    uint8_t nm[FM1_NCOL] = {0}, sg[FM1_NCOL] = {0}, sm[FM1_NCOL] = {0}, iv[FM1_NCOL] = {0};
    uint32_t k, c, g, br, beat, sc, sr = 0;
    uint32_t fam = cur_fam(), mode = settings_leds, inv = leds_inv();   /* (1.5.6: INV LO 1, INV HI 2) */
    int keys_map = keys_own();
    static uint8_t ready;
    if (idle_leds() || fm1_led_anim_on())              /* the idle animation, the power-on sweep have them (main.c */
        return;                                         /* boot_leds) */
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[FAM_BTN[fam]], 1);
    if (sgrid_on())
        led_put(nl, panel.btn[B_SEQ], 1);               /* SIMPLE: SEQ lit while the keys are the steps, */
    led_put(nb, panel.btn[B_SEQ], FELUCCA_SIMPLE && ui.sgrid == 2u);   /* .. breathing while they are PARAM */
    if ((k = arp_led()) != 2u)
        led_put(nl, panel.btn[B_ARP], FAM_BTN[fam] == B_ARP ? !k : (int)k);   /* (on ARP's page: dark flashes) */
    led_put(nl, panel.btn[B_REC], song.rec != 0u);
    k = oct_leds();
    led_put(nl, panel.btn[B_OCTDN], (int)(k & 1u));
    led_put(nl, panel.btn[B_OCTUP], (int)((k >> 1) & 1u));
    led_put(nb, panel.btn[B_OCTUP], (int)(k & OCT_BREATH));
    led_put(nb, panel.btn[B_OCTDN], (k & OCT_BREATH_DN) != 0u);
    c = key_leds(&br);
    g = !keys_map ? 0u : grid_on() ? grid_glow() : 0u;   /* (key_leds: the grid's map) */
    beat = 0;
    sc = keys_map ? 0u : scale_leds(&sr);               /* SCALE LEDS: the scale's keys glow, the root's brighter */
    for (k = 0; k < 27u; k++) {
        uint32_t in = (sc >> k) & 1u, rt = (sr >> k) & 1u, idle = !((c >> k) & 1u);
        led_put(nm, 14u + k, (int)((beat >> k) & 1u));
        led_put(keys_map ? own : nl, 14u + k, (int)((c >> k) & 1u));
        led_put(keys_map ? og : nd, 14u + k, keys_map ? (int)((g >> k) & 1u) : !sc || (mode == LEDS_INV && rt));
        led_put(nb, 14u + k, (int)((br >> k) & 1u));
        if (!sc)
            continue;
        led_put(sg, 14u + k, mode == LEDS_INV ? !in && idle : (int)in);   /* (INV: the active ones dark) */
        led_put(sm, 14u + k, mode == LEDS_INV ? in && !rt && idle : (int)rt);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    if (song.playing || seq_counting())                 /* playing: PLAY's green, its own LED dark in every mode */
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++) {
        if (mode == LEDS_INV) {                         /* INV: the active ones dark, the rest lit (1.5.6, #212: */
            iv[c] = (uint8_t)(nd[c] & ~nl[c]);          /* INV LO / HI: at the steady mid level, below) */
            nl[c] = inv ? 0u : iv[c];
        }
        nd[c] = mode == LEDS_DIM || mode == LEDS_DIM_LO ? (uint8_t)(nd[c] | og[c]) : nm[c];   /* OFF, INV: no glow but */
        if (mode != LEDS_DIM && mode != LEDS_DIM_LO)    /* the grid's beats (the one level above dark there); DIM HI */
            nm[c] = inv ? iv[c] : 0u;                   /* / LO: the beats at the mid level over the glow; INV LO / HI:
                                                         * INV's lit ones there */
        nd[c] |= sg[c];                                 /* SCALE LEDS: in every mode (INV: inverted above) */
        nm[c] |= sm[c];
        nm[c] &= (uint8_t)~(nb[c] | own[c]);
        nl[c] |= own[c];
        nl[c] &= (uint8_t)~nb[c];                       /* breathing: neither lit (INV's idle) nor the glow */
        nd[c] &= (uint8_t)~nb[c];
    }
    if (song.playing || (seq_counting() && cin_pos < click_beat_len() / 4u))   /* counting in: green on each beat */
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(mode == LEDS_DIM_LO || inv == 1u);   /* (INV LO: the darker mid level, ~8 %; HI ~12.5 %) */
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = nb[c];
        fm1_led_mid[c] = nm[c];
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
}

/* ---------------------------------------------------------- input --- */
/* Predictable hardware response (#23): each decoded detent is one value step; a fast turn keeps its full signed
 * detent count. MENU > KNOB ACCEL ON (#52, OFF by default) multiplies a fast turn of a wide value (range > 32, not a
 * list of names; the FX and GLO layers' knobs too, #126) by 2..4, by up to 8 over a range above 64. The main loop
 * reads the knobs many times a frame (main.c), so a read holds one detent as a rule: the speed is the time per
 * detent, ACC_RATE / ms -> 25 ms x2, 16 ms x3, 12 ms x4, 10 ms x5 .. 6 ms or less x8 (a flick: 12 detents in 120 ms
 * move a -100..100 FX macro ~50). Only the longer of this read's and the previous read's time counts, and only while
 * the turn goes on (both under ACC_GAP ms) in one direction: a slow turn, the first two detents of a turn, a single
 * quick detent (a bounce) and a reversal are one step per detent, and the sign is always the detents'.
 * ui.enc_t[role]: bits 0..23 the ms of its last read, bit 24 its direction (+1), 25..31 its ms per detent (127 slow) */
#define ACC_GAP 40u
#define ACC_RATE 50u
static int32_t accel(uint32_t role, int32_t s, int32_t range)
{
    uint32_t now = fm1_ms & 0xFFFFFFu, st = ui.enc_t[role], up = s > 0, pi = st >> 25, a, i, m = 1;
    if ((!FELUCCA_SIMPLE && !(ui_prefs & PREF_ACCEL)) || range <= 32 || !s)
        return s;
    a = (uint32_t)(s < 0 ? -s : s);
    i = ((now - st) & 0xFFFFFFu) / a;                   /* ms per detent of this read */
    if (!st || ((st >> 24) & 1u) != up || i >= ACC_GAP)
        i = 127u;                                       /* a new turn, or reversed */
    else if (pi < ACC_GAP) {
        /* SIMPLE, always on: slow is one value a detent (fine), fast whips round: 1 + (cap - 1) * t^2, t the speed from 40 ms
         * a detent (0) to 4 ms (1); cap 24 over a wide range, 6 over a narrow one */
        uint32_t sl = i > pi ? i : pi ? pi : 1u;
        m = sl >= 28u ? 1u : sl >= 11u ? (range > 64 ? 3u : 2u) : (range > 64 ? 8u : 4u);   /* slow / medium / fast */
    }
    ui.enc_t[role] = now | up << 24 | (i ? i : 1u) << 25;
    return s * (int32_t)m;
}

/* MIXER page: KNOB 1 LEVEL, 2 PAN, 3 REV send, 4 MUTE of the selected track (right = ON, left = OFF: the
 * track itself is ALGORITHM's, on every page). Pattern length stays on SEQ. */
static void tracks_edit(uint32_t slot, int32_t steps)
{
    uint32_t tk = cur_page()->id[slot & 3u];          /* (the VOLUME pages: KNOB k is track id[k]) */
    track_t *t;
    int16_t *vp;
    const param_desc_t *d;
    if (tk >= NTRK)
        return;
    t = &trk[tk];
    vp = &t->p[P_LEVEL];
    d = &TP[P_LEVEL];
    *vp = (int16_t)clamp(*vp + accel(EN_K1 + slot, steps, d->max - d->min), d->min, d->max);
    motion_capture(t, (uint32_t)(vp - t->p), *vp);
    return;
    switch (slot) {
    case 3:
        t->p[P_MUTE] = (int16_t)(steps > 0);
        return;
    case 0:
        vp = &t->p[P_LEVEL];
        d = &TP[P_LEVEL];
        break;
    case 2:
        vp = &t->p[P_REV];
        d = &TP[P_REV];
        break;
    default:
        vp = &t->p[P_PAN];
        d = &TP[P_PAN];
        break;
    }
    *vp = (int16_t)clamp(*vp + accel(EN_K1 + slot, steps, d->max - d->min), d->min, d->max);
    motion_capture(t, (uint32_t)(vp - t->p), *vp);
}

/* SEQ > STEP's piano roll (not the DRUM grid, whose white keys stay its steps): where step recording writes */
static int roll_on(void)
{
    return !ui.home && !ui.confirm && song.seq_mode && cur_page()->graph == GR_ROLL && !drum_track(TSEL);
}

/* SIMPLE, SAVE: pressed, the SAVE screen: a white key saves the selected track's sound in that slot of its machine
 * (16 a machine), a black key the kit (11). Tapped (let go soon, nothing saved), the LOAD screen stays: a white key
 * loads that sound into the selected track, a black key that kit, and the screen goes so the keys play it; SAVE again
 * or a page button closes it. The keys are silent there (song.grid 5). What was saved / loaded flashes ~1 s (gbsv 3,
 * the keys play) */
static void gbsv_done(const char *what, uint32_t n, const char *how)   /* "SOUND 3" "LOADED", flashing */
{
    char nb[4];
    str_cpy(gbsv_msg[0], what, sizeof gbsv_msg[0]);
    fmt_int(nb, (int32_t)n + 1);
    str_cpy(gbsv_msg[0] + str_len(gbsv_msg[0]), nb, sizeof gbsv_msg[0] - str_len(gbsv_msg[0]));
    str_cpy(gbsv_msg[1], how, sizeof gbsv_msg[1]);
    gbsv_t = fm1_ms | 1u;
    if (gbsv == 2u)                                      /* (loaded: the keys play it) */
        gbsv = 0;
}
static uint32_t gbsv_t0;
static uint8_t gbsv_did;
static void gbsv_buttons(uint32_t pressed, int ok)
{
    static const uint8_t PAGE_B[] = {B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SEQ, B_ARP};
    uint32_t k, down = (fm1_in.buttons >> panel.btn[B_SAVE]) & 1u;
    if (!ok) {
        if (gbsv || gbsv_t) { gbsv = 0; gbsv_t = 0; ui.force = 1; }
        return;
    }
    if (gbsv_t && fm1_ms - gbsv_t > 1200u) {            /* the flash is over */
        gbsv_t = 0;
        ui.force = 1;
    }
    for (k = 0; k < NELEM(PAGE_B); k++)
        if ((gbsv || gbsv_t) && ((pressed >> panel.btn[PAGE_B[k]]) & 1u)) {
            gbsv = 0;
            gbsv_t = 0;
            ui.force = 1;
        }
    if ((pressed >> panel.btn[B_SAVE]) & 1u) {
        if (gbsv == 2u) {                                /* LOAD: SAVE closes it */
            gbsv = 0;
            gbsv_t0 = 0;
        } else {
            gbsv = 1;
            gbsv_t0 = fm1_ms | 1u;
            gbsv_did = 0;
            gbsv_t = 0;
        }
        ui.force = 1;
    }
    if (gbsv == 1u && !down) {                           /* SAVE let go: a tap opens LOAD */
        gbsv = !gbsv_did && fm1_ms - gbsv_t0 < 700u ? 2u : 0u;
        ui.force = 1;
    }
}
static void gbsv_keys(uint32_t pressed)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (!((pressed >> k) & 1u))
            continue;
        if (!key_black(k) && p < 16u) {
            if (gbsv == 1u ? gbs_save_sound(p) : gbs_load_sound(p))
                gbsv_done("SOUND ", p, gbsv == 1u ? (transport_busy() ? "SAVED AT STOP" : "SAVED") : "LOADED");
        } else if (key_black(k) && p < 11u) {
            if (gbsv == 1u ? gbk_save(p) : gbk_load(p))
                gbsv_done("KIT ", p, gbsv == 1u ? (transport_busy() ? "SAVED AT STOP" : "SAVED") : "LOADED");
        } else {
            continue;
        }
        gbsv_did = 1;
        ui.force = 1;
        if (!gbsv)
            break;                                       /* (loaded: the other keys play) */
    }
}
static uint32_t gbsv_leds(void)                          /* the slots in use: the machine's sounds, the kits */
{
    uint32_t k, m = 0, mach = song.sel;                  /* (the slots are the track's) */
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (key_black(k) ? gbk_used(p) : p < 16u && gbs_used(mach, p))
            m |= 1u << k;
    }
    return m;
}

/* PERF (song.grid 4): a black key picks the track; the white keys 1..8 are the macros (held: they apply, knob turns on any
 * page are kept in them, let go everything returns), 9..12 the fade time, 13..16 the beat repeat (seq.c keyboard_block). The
 * keys are silent here (seq.c keyboard_block) */
static void pm_keys(uint32_t pressed)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (!((pressed >> k) & 1u))
            continue;
        if (key_black(k)) {
            if (p < NTRK && !kb_octup)                  /* (OCT+ held: the track keys are the mutes, seq.c) */
                track_select(p);
        } else if (p < PM_NM) {
            pm_begin(p, k);
            ui.force = 1;
        } else if (p < PM_NM + 4u) {                    /* a fade time: stays; again for none */
            pm_fade = pm_fade == p - PM_NM + 1u ? 0u : (uint8_t)(p - PM_NM + 1u);
            ui.force = 1;
        }
    }
}
/* a macro lets go when its key does (any page); called every input pass, it also moves the fades */
static void pm_poll(void)
{
    uint32_t i, was = pm_h.n;
    for (i = pm_h.n; i > 0; i--)
        if (!((fm1_in.notes >> pm_h.key[i - 1u]) & 1u))
            pm_drop(i - 1u);
    pm_update();
    if (was && !pm_h.n && !pm_page()) {                 /* all let go: back to the macros */
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].graph == GR_PERF) {
                ui.home = 0;
                ui.page = (uint8_t)i;
                page_entered();
                break;
            }
    }
    if (was != pm_h.n)
        ui.force = 1;
}
static uint32_t trk_keys(int playing)               /* the track keys (black 1..6): all, or those whose track is not muted */
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (key_black(k) && key_place(k) < NTRK && (!playing || !trk[key_place(k)].p[P_MUTE]))
            m |= 1u << k;
    return m;
}
static uint32_t pm_leds(void)                       /* the macros with something in them, the fade time chosen, the repeats held */
{
    uint32_t k, m = trk_keys(1);                        /* (the track keys: lit while the track plays, not muted) */
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (key_black(k))
            continue;
        if (p < PM_NM ? pm_mac[p].n : p < PM_NM + 4u ? pm_fade == p - PM_NM + 1u : p < 16u && ((fm1_in.notes >> k) & 1u))
            m |= 1u << k;
    }
    return m;
}


/* REC tap on every page: arm / disarm recording on the selected track without navigating; arming while stopped
 * starts the transport too (live recording), except on STEP's piano roll (1.2, Discussion #133): there it arms step
 * recording and the transport stays stopped: the keys write the cursor step (seq_entry), PLAY then records live.
 * Armed and playing, STEP's keys record live (at the play head) instead of writing the cursor step */
static int rec_tap(void)                                /* 1: the arming changed */
{
    uint8_t bit = (uint8_t)(1u << song.sel);
    if (!ui.home && cur_page()->graph == GR_SONG && !(song.rec & bit)) {
        ui_message("[SEQ] TO RECORD");
        return 0;
    }
    song.rec ^= bit;
    ui.force = 1;                                     /* also refresh the status on MENU / ABOUT */
    if ((song.rec & bit) && !song.playing && !seq_counting() && roll_on()) {
        ui_message(ui_prefs2 & (PREF_CHORD_ADD >> 16) ? "KEYS ADD TO STEP" : "KEYS WRITE STEPS");   /* step recording:
                                                         * stopped, at the cursor (CHORD ENTRY ADD: adding) */
        return 1;
    }
    if ((song.rec & bit) && !song.playing && !seq_counting())
        transport_req = 1;
    if ((song.rec & bit) && grid_on())
        ui_message("LANE KEYS RECORD");               /* (the white keys stay the steps) */
    else if ((song.rec & bit) && !ui.home && cur_page()->graph == GR_ROLL)
        ui_message("KEYS RECORD LIVE");               /* (seq_entry pauses while armed and playing) */
    return 1;
}

/* live recording into the selected track now: the STEP page's key entry pauses meanwhile */
static int live_rec_sel(void)
{
    return ((song.rec >> song.sel) & 1u) && (song.playing || transport_req == 1u || seq_counting());
}






/* a global turned live (SIMPLE: one a lock can hold, motion.c MOTION_G): each track's base follows, so the next step
 * after a lock puts back this value, not the one from before the turn */
static void g_capture(const int16_t *vp)
{
    uint32_t id, k;
    if (!vp || vp < song.g || vp >= song.g + G_COUNT || (id = motion_gid((uint32_t)(vp - song.g))) == 0xFFu)
        return;
    for (k = 0; k < NTRK; k++)
        (void)motion_capture(&trk[k], id, *vp);
}

/* SIMPLE, the step keys: a white key toggles that step of the selected track (a note of the track's machine, seq.c
 * grid_note_of), a black key 1..4 selects the track (seq.c plays it). An empty step comes on at the press; a step that
 * is on goes when its key is let go, unless the SEQ page's knobs turned while it was held (its CHANCE, TRIG) */
static uint32_t gbs_off, gbs_kept;                       /* keys: its step goes when let go; .. turned while held: it stays */
static void gbs_step_off(step_t *st)                     /* the step and its TRIG (core.h GB_TRIG) */
{
    uint32_t i = (uint32_t)(st - TSEL->step);
    step_clear(st);
    (void)motion_clear_locks(TSEL, i);
    if (i < GB_TRIG)
        TSEL->step[i + GB_TRIG].probability = 0;
}
static void gbs_keep(void)                               /* the step keys held stay on when let go (they were edited) */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (((fm1_in.notes >> k) & 1u) && !key_black(k) && key_place(k) < GB_TRIG)
            gbs_kept |= 1u << k;
}
/* the steps held (bit p: step p), for the SEQ page's knobs */
static uint32_t gbs_held(void)
{
    uint32_t k, m = 0, len = fxl_on() ? 16u : (uint32_t)TSEL->p[P_SLEN];
    if (!sgrid_on())
        return 0;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (((fm1_in.notes >> k) & 1u) && !key_black(k) && p < len && p < GB_TRIG)
            m |= 1u << p;
    }
    return m;
}
static void sgrid_release(void)                          /* every input pass: the keys let go */
{
    uint32_t k, up = (gbs_off | gbs_kept) & ~fm1_in.notes;
    for (k = 0; k < 27u; k++)
        if ((up >> k) & 1u && ((gbs_off & ~gbs_kept) >> k) & 1u && key_place(k) < GB_TRIG) {
            if (fxl_on()) {
                fxl.trig &= (uint16_t)~(1u << key_place(k));
                fxl.lk[key_place(k)] = 0;
            } else {
                gbs_step_off(&TSEL->step[key_place(k)]);
            }
            ui.force = 1;
        }
    gbs_off &= ~up;
    gbs_kept &= ~up;
}
/* SIMPLE, PARAM (song.grid 6): what seq.c par_key needs, every input pass: the chosen knob's id and range, SEQ held */
static void par_poll(void)
{
    uint32_t id = par_on() ? lock_pid(cur_page(), par_k) : 0xFFu;
    kb_seqdn = (uint8_t)(par_on() && ((fm1_in.buttons >> panel.btn[B_SEQ]) & 1u));
    if (id == P_E7)
        id = 0xFFu;                                     /* (MACH: a machine wants its starting sound, machine_defaults) */
    if (id != 0xFFu) {
        const param_desc_t *d = id < P_COUNT ? track_desc(TSEL, id) : motion_desc(TSEL, id);
        int16_t lo = (int16_t)clamp(d->min, -64, 127), hi = (int16_t)clamp(d->max, -64, 127);   /* (what a lock holds) */
        if (par_id != id || par_lo != lo || par_hi != hi) {
            par_id = 0xFFu;                             /* (the audio ISR never sees a half-changed knob) */
            par_lo = lo;
            par_hi = hi;
        }
    }
    par_id = (uint8_t)id;
}
/* SIMPLE, SEQ held (not under a dialog, SAVE or PERF): + a knob = PARAM for that knob, + a white key = LOCKS (its release
 * is then no tap) */
static int seq_dn(void)
{
    return FELUCCA_SIMPLE && !ui.confirm && !gbsv_keys_on() && !pm_page() && !pm_down() &&
           ((fm1_in.buttons >> panel.btn[B_SEQ]) & 1u);
}
static void seq_white(uint32_t notes)
{
    uint32_t k;
    if (!seq_dn())
        return;
    for (k = 0; k < 27u; k++)
        if (((notes >> k) & 1u) && !key_black(k)) {
            layer_kill();
            if (ui.sgrid != 3u) {
                ui.sgrid = 3;                           /* (the key then plays as a LOCKS key, this same pass) */
                ui_message("LOCKS");
                ui.force = 1;
            }
            return;
        }
}
static int seq_knob(uint32_t k)
{
    uint32_t id;
    if (!seq_dn())
        return 0;
    layer_kill();
    id = !ui.home && lock_page_ok(ui.page) ? lock_pid(cur_page(), k) : 0xFFu;
    if (id == 0xFFu || id == P_E7)                      /* (a knob PARAM can't take: the turn does nothing) */
        return 1;
    par_k = (uint8_t)k;
    if (ui.sgrid != 2u)
        ui_message("PARAM");
    ui.sgrid = 2;
    ui.force = 1;
    return 1;
}
/* .. its black keys: the track (as SEQ's), SEQ held: black keys 1..4 pick the knob (SEQ's release is then no tap) */
static void par_keys(uint32_t pressed)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (!((pressed >> k) & 1u) || !key_black(k))
            continue;
        if (kb_seqdn) {
            layer_kill();
            if (p < 4u)
                par_k = (uint8_t)p;
            ui.force = 1;
        } else if (p < NTRK && !kb_octup) {
            track_select(p);
        }
    }
}
/* .. its keys' LEDs: the key of the knob's value, the selected track's key; SEQ held: the knob's key (black 1..4) */
static uint32_t par_leds(void)
{
    uint32_t k, m = 0, pos = 0xFFu;
    if (par_id != 0xFFu && par_hi > par_lo) {
        int32_t v = *motion_vp(TSEL, par_id);
        pos = (uint32_t)clamp(((v - par_lo) * 15 + (par_hi - par_lo) / 2) / (par_hi - par_lo), 0, 15);
    }
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (key_black(k) ? p == (kb_seqdn ? par_k : song.sel) : p == pos)
            m |= 1u << k;
    }
    return m;
}
static void sgrid_keys(uint32_t pressed)
{
    uint32_t k, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (!((pressed >> k) & 1u))
            continue;
        if (key_black(k)) {
            if (p <= NTRK && !kb_octup) {               /* (OCT+ held: the track keys are the mutes, seq.c) */
                gbs_off = gbs_kept = 0;                 /* (the steps held were the other track's) */
                fxl_sel = (uint8_t)(p == NTRK);         /* black key 7: the FX lane */
                if (p < NTRK)
                    track_select(p);
                ui.force = 1;
            }
            continue;
        }
        if (fxl_on()) {                                 /* the FX lane: a trig on at press, off when let go */
            if (p < 16u) {
                if ((fxl.trig >> p) & 1u) {
                    gbs_off |= 1u << k;
                } else {
                    fxl.lk[p] = 0;
                    fxl.trig |= (uint16_t)(1u << p);
                }
                ui.force = 1;
            }
            continue;
        }
        if (p >= len)
            continue;
        {
            step_t *st = &TSEL->step[p];
            if ((st->time == ST_NOTE && st->n) || gbs_trigless(TSEL, p)) {
                gbs_off |= 1u << k;                     /* (goes when let go: sgrid_release) */
            } else if (kb_octup) {                      /* OCT+ held: a trigless lock step (hold it and turn knobs) */
                gbs_step_off(st);
                st->time = ST_LOCK;
                mt_used = 1;                            /* (OCT+ was used: no setup values, no octave) */
                ui.force = 1;
            } else {
                gbs_step_off(st);
                st->time = ST_NOTE;
                st->n = (uint8_t)grid_chord(TSEL, st->note);
                st->vel = 100;
            }
        }
        cursor_set((int32_t)p);
    }
}

/* SIMPLE, the SEQ page: KNOB 1 LEN (1..16), 2 SPEED (x1/4 .. x4: DIV 1/4 .. 1/64), 3 CHANCE: the track's, or with
 * steps held their own (TRK: none, the track's), 4 TRIG: the held steps' (- 1/4 2/4 3/4 4/4: core.h GB_TRIG) */
static void gbseq_knob(uint32_t slot, int32_t steps)
{
    track_t *t = TSEL;
    uint32_t held = gbs_held(), p;
    int16_t was;
    if (slot >= 2u && held) {                            /* the held steps' own: they stay when let go */
        gbs_keep();
        for (p = 0; p < GB_TRIG; p++) {
            step_t *s = &t->step[p];
            if (!((held >> p) & 1u))
                continue;
            if (slot == 2u) {
                int32_t c = clamp(gb_own(s) + accel(EN_K1 + slot, steps, 101), -1, 100);
                s->probability = (uint8_t)(c < 0 ? 0 : c == 0 ? 101 : c);
            } else {
                int32_t g = clamp((int32_t)gb_trig(s) + (steps > 0 ? 1 : -1), 0, 4);
                s[GB_TRIG].probability = (uint8_t)(g ? COND_FIRST + GB_TRIG_C0 + (uint32_t)g - 1u : 0u);
            }
        }
        ui.force = 1;
        return;
    }
    if (slot == 3u) {
        ui_message("HOLD A STEP");
        return;
    }
    if (slot == 0u) {
        was = t->p[P_SLEN];
        t->p[P_SLEN] = (int16_t)clamp(was + (steps > 0 ? 1 : -1), 1, 16);
        pm_record(t, &t->p[P_SLEN], was);
    } else if (slot == 1u) {
        was = t->p[P_SDIV];
        t->p[P_SDIV] = GB_SPEED[clamp((int32_t)gb_speed(was) + (steps > 0 ? 1 : -1), 0, 4)];
        pm_record(t, &t->p[P_SDIV], was);
    } else {
        was = t->p[P_APROB];
        t->p[P_APROB] = (int16_t)clamp((was > 100 ? 100 : was) + accel(EN_K1 + slot, steps, 100), 0, 100);
        pm_record(t, &t->p[P_APROB], was);
    }
    ui.force = 1;
}

/* ------------------------------------------------- parameter locks --- */
/* 1.1 (Discussions #75, #53): on SEQ > STEP, hold a step and turn KNOB 1..4: the step gets a lock of the parameter
 * that knob edits on the sound page shown last (ui.plk_src: HOME's four, ENV, LFO, FX, EDIT 1 / 2, OP ENV, ..; the
 * cards show those four while a step is held, a locked value bright, the sound's own DIM). The step held: on the
 * DRUM grid its white keys (several at once: each gets the lock; a press on a hit takes it away only when let go,
 * and not at all when it was held for a lock), on the roll the step being entered (its note keys held). [EDIT]
 * tapped meanwhile clears the held steps' locks (EDIT alone still clears the step, its locks with it). Not while a
 * song plays (STOP TO EDIT). A lock is a motion record (motion.c): AUTOMATION's PLAY plays it, CLEAR clears it */
static uint64_t lock_held(void)
{
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)t->p[P_SLEN], keys = fm1_in.notes & ~kb_layer;
    uint64_t m = 0;
    if (sgrid_on()) {
        if (ui.home || !keys || !lock_page_ok(ui.page))
            return 0;
        ui.plk_src = (uint8_t)(ui.page + 1u);           /* (the page shown: lock_id) */
        if (fxl_on())
            len = 16u;                                  /* (the FX lane's 16) */
        for (k = 0; k < 27u; k++)
            if (((keys >> k) & 1u) && !key_black(k) && key_place(k) < len && key_place(k) < GB_TRIG)
                m |= (uint64_t)1 << key_place(k);
        return m;
    }
    if (ui.home || ui.confirm || !song.seq_mode || cur_page()->graph != GR_ROLL ||
        !keys || detail_on())                           /* (DETAIL: its knobs are the step's own fields, sd_turn) */
        return 0;
    if (!grid_on())                                     /* the roll: the step being entered */
        return ui.entry_open && !live_rec_sel() && ui.cursor < len ? (uint64_t)1 << ui.cursor : 0u;
    for (k = 0; k < 27u; k++)
        if (((keys >> k) & 1u) && !key_black(k) && ui.bank * 16u + key_place(k) < len)
            m |= (uint64_t)1 << (ui.bank * 16u + key_place(k));
    return m;
}


/* KNOB k turned s with the steps `held` held: each gets (or changes) its lock of lock_id(k), from its lock or the
 * sound's own value */
/* the FX lane's steps `held`: KNOB k's lock, the master's knobs only (from the lock, or the knob's own value) */
static void fxl_turn(uint32_t k, int32_t s, uint64_t held)
{
    uint32_t id = lock_id(k), j, i;
    const param_desc_t *d;
    if (id == 0xFFu || id < P_COUNT) {
        ui_message("FX: MASTER ONLY");
        return;
    }
    j = id - P_COUNT;
    d = &GP[MOTION_G[j]];
    s = accel(EN_K1 + k, s, d->max - d->min);
    for (i = 0; i < 16u; i++) {
        int32_t v;
        if (!((held >> i) & 1u))
            continue;
        v = (fxl.lk[i] >> j) & 1u ? fxl.v[i][j] : song.g[MOTION_G[j]];
        fxl.v[i][j] = (int8_t)param_turn(d, v, s);
        fxl.lk[i] |= (uint16_t)(1u << j);
        fxl.trig |= (uint16_t)(1u << i);
    }
}
static void lock_turn(uint32_t k, int32_t s, uint64_t held)
{
    track_t *t = TSEL;
    uint32_t id = lock_id(k), i;
    const param_desc_t *d;
    if (fxl_on()) {
        fxl_turn(k, s, held);
        return;
    }
    if (id == 0xFFu)
        return;                                         /* (an empty card: nothing to lock) */
    ui.plk_rm &= ~held;                                 /* (held for a lock: its hit stays) */
    d = id < P_COUNT ? track_desc(t, id) : motion_desc(t, id);   /* (SIMPLE: a global, motion.c) */
    s = accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min);
    motion_undo_take(t, 0x100u | k);                    /* (1.1.5: SAVE held takes the locks back) */
    for (i = 0; i < NSTEP; i++) {
        int16_t v;
        if (!((held >> i) & 1u))
            continue;
        if (!motion_lock_get(t, i, id, &v))
            v = motion_base_value(t, id);
        if (motion_set_lock(t, i, id, (int16_t)param_turn(d, v, s)) == 2)
            break;                                      /* (full: ui_notices says so) */
    }
    motion_undo_done(t);
}

/* SIMPLE, KIT (seq.c kit_*): the white keys held (their places, bit p) */
static uint32_t kit_held(void)
{
    uint32_t k, m = 0, keys = fm1_in.notes & ~kb_layer;
    if (!kit_on())
        return 0;
    for (k = 0; k < 27u; k++)
        if (((keys >> k) & 1u) && !key_black(k) && key_place(k) < 16u)
            m |= 1u << key_place(k);
    return m;
}
/* KNOB k turned s with the KIT keys `held` held: each key's lock of the page's knob changes (a new one starts from what
 * sounds; KIT_N a key), and sounds at once. 0: the knob locks nothing here (it edits as ever) */
static int kit_turn(uint32_t k, int32_t s, uint32_t held)
{
    track_t *t = TSEL;
    uint32_t tk = song.sel, id = lock_page_ok(ui.page) ? lock_pid(cur_page(), k) : 0xFFu, p, i;
    const param_desc_t *d;
    if (id == 0xFFu)
        return 0;
    d = id < P_COUNT ? track_desc(t, id) : motion_desc(t, id);
    s = accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min);
    for (p = 0; p < 16u; p++) {
        kit_key_t *kk = &kit[tk][p];
        if (!((held >> p) & 1u))
            continue;
        for (i = 0; i < kk->n && kk->id[i] != id; i++)
            ;
        if (i == kk->n) {
            if (kk->n == KIT_N) {
                ui_message("KEY LOCKS FULL");
                continue;
            }
            kk->id[kk->n] = (uint8_t)id;
            kk->v[kk->n++] = kit_cur[tk] == p + 1u ? *motion_vp(t, id) : motion_base_value(t, id);
        }
        kk->v[i] = (int16_t)param_turn(d, kk->v[i], s);
        if (kit_cur[tk] == p + 1u)
            kit_apply(t, p);                            /* (sounding: the new value now) */
    }
    return 1;
}
/* LOCKS: what knob vp's dial shows: a key held, its lock (1: inverted) or the sound's own; else the sound's own (a key's
 * lock may sound) */
static int kit_shown(int16_t *vp, int16_t *cur)
{
    uint32_t id, held = kit_held(), p = 0, i;
    if (vp >= TSEL->p && vp < TSEL->p + P_COUNT)
        id = (uint32_t)(vp - TSEL->p);
    else if (vp >= song.g && vp < song.g + G_COUNT && motion_gid((uint32_t)(vp - song.g)) != 0xFFu)
        id = motion_gid((uint32_t)(vp - song.g));
    else
        return 0;
    if (held) {
        const kit_key_t *kk;
        while (!((held >> p) & 1u))
            p++;
        kk = &kit[song.sel][p];
        for (i = 0; i < kk->n; i++)
            if (kk->id[i] == id) {
                *cur = kk->v[i];
                return 1;
            }
    }
    (void)kit_base(TSEL, id, cur);
    return 0;
}
/* LOCKS: a black key 1..6 picks the track */
static void kit_keys(uint32_t pressed)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (((pressed >> k) & 1u) && key_black(k) && key_place(k) < NTRK && !kb_octup)
            track_select(key_place(k));
}
/* .. its LEDs: the white keys that hold locks, the selected track's black key */
static uint32_t kit_leds(void)
{
    uint32_t k, m = 0, p;
    for (k = 0; k < 27u; k++) {
        p = key_place(k);
        if (key_black(k) ? p == song.sel : p < 16u && kit[song.sel][p].n)
            m |= 1u << k;
    }
    return m;
}

/* [EDIT] with the steps `held` held: their locks go */
static void lock_clear(uint64_t held)
{
    uint32_t i, n = 0;
    if (fxl_on()) {                                     /* the FX lane's held steps: their locks go (the trigs stay) */
        for (i = 0; i < 16u; i++)
            if ((held >> i) & 1u) {
                n += fxl.lk[i] != 0u;
                fxl.lk[i] = 0;
            }
        ui_message(n ? "LOCKS CLEARED" : "NO LOCKS");
        return;
    }
    ui.plk_rm &= ~held;
    if (!(motion_lock_steps(song.sel) & held)) {
        ui_message("NO LOCKS");
        return;
    }
    motion_undo_take(TSEL, 0);                          /* (1.1.5: SAVE held brings them back) */
    for (i = 0; i < NSTEP; i++)
        if ((held >> i) & 1u)
            n += motion_clear_locks(TSEL, i);
    motion_undo_done(TSEL);
    ui_message(n ? "LOCKS CLEARED" : "NO LOCKS");
}




static void edit_param(uint32_t slot, int32_t steps)
{
    int16_t *vp;
    const page_t *pg = cur_page();
    const param_desc_t *d;
    int32_t v;
    if (pg->scope == SC_TRK) {
        tracks_edit(slot, steps);
        return;
    }
    if (pg->graph == GR_GBSEQ) {
        gbseq_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_PERF) {                          /* KNOB 1 the release fade (none .. a bar), 2 the DJ filter, 3 the comb's MIX, 4 its TUNE */
        if (slot == 0u)
            pm_rel = (uint8_t)clamp((int32_t)pm_rel + (steps > 0 ? 1 : -1), 0, 16);
        else {                                           /* macros keep these too */
            static const uint8_t GK[3] = {G_MIDI, G_SYNC, G_CRATE};
            int16_t *gp = &song.g[GK[slot - 1u]], was = *gp;
            const param_desc_t *gd = &GP[GK[slot - 1u]];
            *gp = (int16_t)param_turn(gd, *gp, accel(EN_K1 + slot, steps, gd->max - gd->min));
            pm_record(TSEL, gp, was);
            g_capture(gp);
        }
        ui.force = 1;
        return;
    }
    d = page_desc(pg, slot, &vp);
    if (!d || !vp || d->max == d->min)
        return;
    if (FELUCCA_SIMPLE && kit_cur[song.sel]) {          /* LOCKS: a key's lock sounds: the knob is the sound's own */
        if (vp >= TSEL->p && vp < TSEL->p + P_COUNT)
            kit_drop(TSEL, (uint32_t)(vp - TSEL->p));
        else if (vp >= song.g && vp < song.g + G_COUNT && motion_gid((uint32_t)(vp - song.g)) != 0xFFu)
            kit_drop(TSEL, motion_gid((uint32_t)(vp - song.g)));
    }
    v = param_turn(d, *vp, accel(EN_K1 + slot, steps, d->fmt == F_ENUM ? 0 : d->max - d->min));
    {
        int16_t was = *vp;
        *vp = (int16_t)v;
        pm_record(TSEL, vp, was);
    }
    if (vp == &TSEL->p[P_E7])                            /* MACH: the machine's starting sound */
        machine_defaults(TSEL);
    if (vp >= TSEL->p && vp < TSEL->p + P_COUNT)        /* (a track value: not a global on the FX page) */
        motion_capture(TSEL, (uint32_t)(vp - TSEL->p), *vp);
    else
        g_capture(vp);
}


/* OCT- / OCT+ where they answer (the dialogs, the menu, action pages): on release, and only a press
 * that began there; both down together (UPDATE MODE, main.c) is no tap. Bit 0 OCT-, bit 1 OCT+ */
static uint32_t oct_taps(uint32_t pressed, int here)
{
    static uint8_t down, chord;
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP];
    uint32_t now = ((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1, tap;
    if (here)
        down |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    if (now == 3u)
        chord = 1;
    tap = down & ~now;
    down &= (uint8_t)now;
    if (chord) {
        tap = 0;
        chord = now != 0u;
    }
    return tap;
}




/* HOME / REC / SAVE: tap on release, hold 0.7 s fires once. t0 = press time | 1,
 * bit 1 = fired (or swallowed: then the release is no tap either) */
enum { BT_NONE, BT_TAP, BT_HOLD };
static uint32_t btn_hold(uint32_t *t0, uint32_t label, uint32_t now, int hold_ok)
{
    uint32_t tap;
    if ((fm1_in.buttons >> panel.btn[label]) & 1u) {
        if (!*t0)
            *t0 = (now | 1u) & ~2u;
        else if (hold_ok && !(*t0 & 2u) && now - (*t0 & ~3u) > 700u * 1000u * FM1_TICKS_PER_US) {
            *t0 |= 2u;
            return BT_HOLD;
        }
        return BT_NONE;
    }
    tap = *t0 && !(*t0 & 2u);
    *t0 = 0;
    return tap ? BT_TAP : BT_NONE;
}

static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

/* what the keys are (seq.c song.grid): 5 the SAVE screen's, 1 the steps, 4 PERF's macros, 6 PARAM, 7 KIT, 0 notes */
static uint32_t keys_mode(void)
{
    return gbsv_keys_on() ? 5u : sgrid_on() ? 1u : pm_page() || pm_down() ? 4u : par_on() ? 6u : kit_on() ? 7u : 0u;
}

/* the page buttons: ui_buttons.c (included after this file) */
static void layer_arm(uint32_t pressed, uint32_t now);
static uint32_t layer_gesture(void);
static void layer_tap(uint32_t l);

/* a page button let go (they act on release; a layer's own button: layer_gesture). 1: it acted (EDIT on STEP, USER,
 * PROJECT, AUTOMATION) instead of opening a page */
static int page_tap(uint32_t b)
{
    uint32_t f;
    if (FELUCCA_SIMPLE && b == B_SEQ && ui.sgrid == 1u) {   /* SIMPLE: a tap in SEQ mode: KEY (the page stays) */
        ui.sgrid = 0;
        ui_message("KEY");
        return 1;
    }
    if (FELUCCA_SIMPLE && b == B_SEQ && (ui.home || cur_page()->graph != GR_GBSEQ)) {   /* .. otherwise from anywhere: the SEQ page, its keys on */
        ui.fam_last[FAM_SEQ] = 0;
        ui.home = 0;
        open_family(FAM_SEQ);
        ui.sgrid = 1;
        ui.force = 1;
        return 1;
    }
    if (FELUCCA_SIMPLE && b == B_SEQ) {                  /* .. on the SEQ page: the step keys off / on */
        ui.sgrid = 1;                                   /* .. on the SEQ page: KEY, PARAM, LOCKS -> SEQ */
        ui_message("SEQ");
        return 1;
    }
    for (f = FAM_HOME + 1u; f < FAM_COUNT; f++)        /* (GLO SEQ SAVE ..: HOME is ui_input's, home_tap) */
        if (FAM_BTN[f] == b && b != B_HOME) {
            open_family(f);
            return 0;
        }
    return 0;
}


/* 1.5.5 (#208): MIDI IN notes since the last frame (seq.c midi_seen): their tracks light (midi_lit), and with MENU > MIDI >
 * AUTO SELECT ON a track that is not selected becomes the selected one, but not while the selected track has had notes
 * within MF_HOLD_MS (two tracks played together, a chord across tracks: the selection stays) nor within MF_HOLD_MS of the
 * last such switch (no flicker), nor with MIDI IN SEL (every note is the selected track's), the menu or a layer up, nor
 * while the selected track is being edited (mf_busy). The lowest such track if several. 1: it switched */
#define MF_HOLD_MS 250u
/* every pass's start (the menu, NAME, SCREEN OFF too): the notes since the last pass light their tracks and are this
 * pass's (mf_seen: SCREEN WAKE +MIDI, AUTO SELECT); a pass that returns before midi_follow drops them (no AUTO SELECT
 * later from a note played in the menu, NAME or the dark) */
static void midi_take(void)
{
    uint32_t k, now = fm1_ms | 1u;
    fm1_irq_off();
    mf_seen = midi_seen;
    midi_seen = 0;
    fm1_irq_on();
    for (k = 0; k < NTRK; k++)
        if ((mf_seen >> k) & 1u)
            midi_lit_ms[k] = now;
}
/* an edit of the selected track in progress: a step held for a lock (STEP, the DRUM grid), a step being entered on the
 * roll, a grid hit waiting for its key's release, a dialog */
static int mf_busy(void)
{
    return lock_held() || (ui.entry_open && (fm1_in.notes & ~kb_layer)) || ui.plk_rm || ui.confirm;
}
static int midi_follow(void)
{
    static uint32_t sw_ms;                              /* the last switch, fm1_ms | 1 (0 none) */
    uint32_t seen = mf_seen, k, now = fm1_ms | 1u, cand, lit;
    mf_seen = 0;
    if (!seen)
        return 0;
    lit = midi_lit_ms[song.sel];                        /* (the selected track's last note: midi_take lit it now if */
                                                        /* this pass has one, and then nothing switches) */
    cand = seen & ~(1u << song.sel);
    k = 0;
    if (cand && !((seen >> song.sel) & 1u) && (ui_rec_prefs & (PREF_MIDI_FOLLOW >> 8)) && song.g[G_ROUTE] != ROUTE_SEL &&
        !mf_busy() && (!lit || now - lit >= MF_HOLD_MS) &&
        (!sw_ms || now - sw_ms >= MF_HOLD_MS)) {
        while (!((cand >> k) & 1u))
            k++;
        track_select(k);
        sw_ms = now;
        k = 1;
    } else
        k = 0;
    return (int)k;
}

/* messages of things that happened elsewhere (a load, the editor, MIDI in): after this frame's own */
static void ui_notices(void)
{
    static uint32_t midi_t, midi_last;
    if (motion_full) { motion_full = 0; ui_message("AUTOMATION FULL"); }
    if (midi_follow())                                  /* (AUTO SELECT took the track: no "MIDI IN -> Tn") */
        midi_hint = 0;
    if (midi_hint) {                                    /* MIDI notes into a track that is not selected */
        uint32_t h = midi_hint;
        midi_hint = 0;
        if (!ui.msg_t && (h != midi_last || fm1_ms - midi_t > 4000u)) {
            char b[4] = {'T', (char)('0' + h), 0, 0};
            ui_say("MIDI IN -> ", b);
            midi_last = h;
            midi_t = fm1_ms;
        }
    }
}

static void ui_input(void)
{
    uint32_t pressed = fm1_input_edges(0), notes = fm1_input_note_edges(), now = fm1_ticks(), id, b, k;
    uint32_t home, rec, save;
    uint32_t oct;
    uint32_t lytap;
    int32_t s;
    midi_take();                                        /* (MIDI IN notes: lights, SCREEN WAKE, AUTO SELECT's) */
    midi_pc_poll();                                     /* MIDI Program Change */
    fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;   /* (MENU > USB LEVEL: fx.c, audio.c) */
    scope_mix = (ui_prefs2 & (PREF_SCOPE_MIX >> 16)) != 0u;   /* (MENU > SCOPE: fx.c, audio.c) */
    follow_poll();                                      /* (1.6, #242: SEQ's roll follows the play position) */
    if (scr_input(pressed, notes))                      /* MENU > SCREEN OFF: dark, or the wake gesture: swallowed */
        return;                                         /* (before the buttons' timers: no tap or hold after it) */
    if (mom.n && !mom_down()) {                         /* MOMENTARY: LFO let go (or the menu, a layer ..): back */
        mom_restore();
        ui_message("BACK");
    }
    if (mom.n && ((pressed >> panel.btn[MOM_KEEP]) & 1u)) {   /* .. OCT+ with LFO held (SIMPLE: OCT- with OCT+): kept */
        mom.n = 0;
        pressed &= ~(1u << panel.btn[MOM_KEEP]);
        ui_message("KEPT");
    }
    home = btn_hold(&ui.home_t0, B_HOME, now, 1);
    rec = btn_hold(&ui.rec_t0, B_REC, now, 0);          /* (a tap; held, the REC layer: ui_layer.c) */
    save = btn_hold(&ui.save_t0, B_SAVE, now, !ui.confirm);
    layer_arm(pressed, now);
    if (((pressed >> panel.btn[B_REC]) & 1u) && ui.ly == LAYER_REC)
        ui.rec_t0 |= 2u;                                /* (REC is a page button: its tap is layer_tap's) */
    oct = oct_taps(pressed, ui.confirm != 0);
    lytap = layer_gesture();
    seq_white(notes);
    par_poll();
    song.grid = (uint8_t)keys_mode();                 /* (a dialog: the keys play again) */
    if (rec == BT_TAP && !ui.confirm)
        rec_tap();
    (void)save;
    gbsv_buttons(pressed, !ui.confirm);   /* SIMPLE: SAVE is the sounds' and kits' screen */
    if (ui.confirm) {                                   /* OCT- cancels, OCT+ does it; nothing else reacts */
        if (oct & 2u) {
            uint32_t kind = ui.confirm;
            ui.confirm = 0;
            ui.force = 1;
            if (kind == CF_CLEAR_MOTION) {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                { load_begin(t, UNDO_PAT); motion_clear(t); load_end(t); ui_message("AUTOMATION CLEARED"); }
            } else if (kind == CF_INIT_SOUND) {
                { set_engine(TSEL->eng_req); ui_message("SOUND INIT"); }
            } else if (kind == CF_CLEAN_SOUND) {
                { clean_engine(); ui_message("CLEAN INIT"); }
            } else {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                load_begin(t, UNDO_PAT);
                track_defaults_steps(t);
                load_end(t);
                t->nheld = 0;                           /* and the latched arp chord */
                t->arp_phys = 0;
                if (kind == CF_CLEAR_TRK) {
                    char b[12] = "1 CLEARED";
                    b[0] = (char)('1' + ui.confirm_trk);
                    ui_say("TRACK ", b);
                } else {
                    ui_message("PATTERN CLEARED");
                }
            }
        } else if (oct & 1u) {
            ui.confirm = 0;
            ui.force = 1;
        }
        ui.pg_down = 0;
        enc_drop();
        return;
    }
    if (lytap)                                          /* a layer's button acts on release (held: the layer; SEQ's */
        layer_tap(lytap);                               /* on every page since 1.2, its tap the SEQ pages) */
    if (home == BT_TAP)                                 /* HOME acts on release: a hold opens the menu */
        home_tap();
    cursor_fix();                                       /* LEN may have changed (knob, editor, load) */
    for (id = 0; id < 14u; id++) {
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        switch (b) {
        case B_PLAY:
            if (song.playing || seq_counting())   /* (a count-in: PLAY stops it) */
                transport_req = 2;
            else
                transport_req = 1;
            break;
        case B_SEQ:
        case B_REC:                                     /* tap: above; held, its layer (ui_layer.c) */
        case B_SAVE:
        case B_FX:                                      /* the layers' buttons: ui_layer.c */
        case B_GLO:
        case B_SCL:
        case B_EDIT:
        case B_ENV:
        case B_HOME:
            break;
        case B_OCTDN:
        case B_OCTUP: {
            if (FELUCCA_SIMPLE && b == B_OCTDN && pm_h.n == 1u) {   /* a macro key held (alone): OCT- empties it */
                pm_clear();
                ui_message("MACRO CLEARED");
                break;
            }
            if (FELUCCA_SIMPLE && b == B_OCTDN && kit_held()) {   /* KIT keys held: OCT- empties them */
                uint32_t h = kit_held(), p;
                for (p = 0; p < 16u; p++)
                    if ((h >> p) & 1u) {
                        if (kit_cur[song.sel] == p + 1u)
                            kit_undo(TSEL);
                        kit[song.sel][p].n = 0;
                    }
                ui_message("KEY LOCKS CLEARED");
                break;
            }
            if (FELUCCA_SIMPLE && b == B_OCTUP && !sgrid_on())
                break;                                   /* SIMPLE: OCT+ is a tap (the octave, on release) or a hold (the setup values) */
            if (FELUCCA_SIMPLE && sgrid_on()) {          /* the step keys: OCT- clears the track's steps, OCT- + OCT+ every track's */
                uint32_t other = b == B_OCTDN ? B_OCTUP : B_OCTDN, a, i2, all = (fm1_in.buttons >> panel.btn[other]) & 1u;
                if (b == B_OCTDN && gbs_held()) {       /* steps held: OCT- clears their locks (the steps stay) */
                    gbs_keep();
                    lock_clear(gbs_held());
                    break;
                }
                if ((b == B_OCTDN || all) && (all || fxl_on())) {   /* (the FX lane: OCT- clears it; with OCT+ it too) */
                    memset(&fxl, 0, sizeof fxl);
                    if (!all) {
                        ui_message("FX CLEARED");
                        break;
                    }
                }
                if (b == B_OCTDN || all) {
                    for (a = all ? 0u : song.sel; a < (all ? NTRK : song.sel + 1u); a++) {
                        for (i2 = 0; i2 < NSTEP; i2++)
                            step_clear(&trk[a].step[i2]);
                        motion_clear(&trk[a]);          /* (its locks and automation too) */
                    }
                    ui_message(all ? "ALL TRACKS CLEARED" : "TRACK CLEARED");
                }
                break;
            }
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) == both)
                song.octave = 0;
            else
                song.octave += b == B_OCTDN ? (song.octave > -3 ? -1 : 0) : (song.octave < 3 ? 1 : 0);
            break;
        }
        default:                                        /* page buttons (GLO SCL ENV LFO EDIT ARP): when let go */
            ui.pg_down |= (uint16_t)(1u << id);
            break;
        }
    }
    b = ui.pg_down & ~fm1_in.buttons;
    ui.pg_down &= (uint16_t)~b;
    for (id = 0; b; id++, b >>= 1)
        if (b & 1u)
            page_tap(panel_btn_of(id));
    {   /* OCT+: held 250 ms = the page's setup values (alt_mode); let go sooner = the octave up (OCT- held: neither) */
        static uint32_t ou_t0;
        static uint8_t ou_dn, ou_used;
        uint32_t dn = (fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u;
        if (dn && !ou_dn) {
            ou_t0 = fm1_ms;
            ou_used = (uint8_t)((fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u);
        } else if (dn && ((fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u)) {
            ou_used = 1;
        }
        if (!dn && ou_dn && !ou_used && fm1_ms - ou_t0 < 250u &&
            !ui.confirm && !sgrid_on() && !par_on())
            song.octave += song.octave < 3 ? 1 : 0;
        kb_octup = (uint8_t)dn;                         /* (seq.c: OCT+ held + a track key = mute) */
        if (mt_used || mom.n) {
            ou_used = 1;                               /* (a mute, a temporary change: not the octave up) */
            mt_used = 0;
        }
        ou_dn = (uint8_t)dn;
        alt_mode = 0;                                   /* (SIMPLE: no second sets) */
    }
    par_poll();
    song.grid = (uint8_t)keys_mode();                 /* (seq.c: the keys are the grid's) */
    if (song.grid != 7u)                               /* (out of KIT: the keys' locks sounding go back) */
        for (k = 0; k < NTRK; k++)
            if (kit_cur[k])
                kit_undo(&trk[k]);
    pm_poll();
    sgrid_release();
    gb_store_poll();
    if (song.grid == 5u) {
        gbsv_keys(notes);
    } else if (song.grid == 4u) {
        pm_keys(notes);
    } else if (song.grid == 6u) {
        par_keys(notes);
    } else if (song.grid == 7u) {
        kit_keys(notes);
    } else if (song.grid && sgrid_on()) {
        sgrid_keys(notes);
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {              /* PRESETS: the selected track's machine (FM, then the 808s) */
        int32_t m = clamp((int32_t)trk_mach(TSEL) + (s > 0 ? 1 : -1), 0, (int32_t)NMACH - 1);
        if ((uint32_t)m != trk_mach(TSEL))
            machine_set(TSEL, (uint32_t)m);
    }
    if ((s = panel_enc(EN_ALGO)) != 0) {                /* ALGORITHM: the selected track's volume, on every page */
        int16_t was = TSEL->p[P_LEVEL];
        TSEL->p[P_LEVEL] = (int16_t)param_turn(&TP[P_LEVEL], was, accel(EN_ALGO, s, TP[P_LEVEL].max - TP[P_LEVEL].min));
        pm_record(TSEL, &TSEL->p[P_LEVEL], was);       /* (a macro holds it too) */
        motion_capture(TSEL, P_LEVEL, TSEL->p[P_LEVEL]);
        ui.force = 1;
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {              /* SELECT: the tempo */
        int16_t was = song.g[G_BPM];
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_SELECT, s, 200), GP[G_BPM].min, GP[G_BPM].max);
        pm_record(TSEL, &song.g[G_BPM], was);           /* (a macro holds the tempo too) */
        ui.bpm_t = 40;                                  /* the header's BPM lights up */
    }
    for (k = 0; k < 4u; k++) {
        const page_t *pg = cur_page();
        int16_t *hv;
        uint64_t held;
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        if (seq_knob(k))                                /* SEQ held + KNOB k: PARAM for it */
            continue;
        if (mom_down())                                 /* (LFO + a knob, MOMENTARY or not: LFO's release opens no page) */
            ui.pg_down &= (uint16_t)~(1u << panel.btn[B_LFO]);
        if (kit_held() && kit_turn(k, s, kit_held())) {   /* KIT: a key held, KNOB k sets its lock */
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
            continue;
        }
        if ((held = lock_held()) != 0u) {               /* a step held on STEP: KNOB k locks (parameter locks) */
            gbs_keep();                                 /* (SIMPLE: the step stays when let go) */
            lock_turn(k, s, held);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
            continue;
        }
        if (mom_down() && mom_turn(k, s)) {             /* MOMENTARY: LFO held (no automation, no undo) */
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
            continue;
        }
        if (pg->scope == SC_TRK || page_desc(pg, k, &hv)) {   /* (not an empty column) */
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40;
        }
        edit_param(k, s);
    }
    ui_notices();
}

/* ---------------------------------------------------- panel setup --- */
/* 30 s without input: give up and keep the old table (a stuck key cannot hang the boot). A piano key cancels at
 * any time: the keys are not part of what is taught, so one works whatever the buttons' table says */
#define SETUP_IDLE_MS 30000u
#define SETUP_GIVE_UP(t0) (fm1_ms - (t0) > SETUP_IDLE_MS || fm1_in.notes != 0u)
static void setup_title(void)
{
    lcd_fill(0, 0, 240, 240, T_BG);
    {   /* the title with its icon (the menu row's), centred together; M from y 8 as before */
        const char *t = "HARDWARE CALIBRATION";
        int32_t x = (240 - (16 + 6 + text_w(&AF_M, t))) / 2;
        cv_begin(240, 24, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "calibration title centred");
        GFX_HOOK_ALIGN(0, HEAD_MY + AF_M_CAP_Y, 0, HEAD_MY + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS,
                       "header icon on its title's line");
        cv_icon_mid(x, 12, 16, ICON_X_DOCTOR, T_THEME, T_BG);
        cv_text(x + 22, HEAD_MY, &AF_M, t, T_TEXT);
        cv_blit(0, 5);
    }
    draw_text_box(0, 32, 240, &AF_S, "TEACH EACH BUTTON AND KNOB", T_MID, 1);
    lcd_fill(16, 56, 208, 1, T_LINE);
    draw_text_box(0, 206, 240, &AF_S, "ANY KEY: CANCEL", T_DIM, 1);
}
static void setup_show(const char *what, const char *name)     /* "PRESS" / "TURN RIGHT", the control */
{
    draw_text_box(0, 80, 240, &AF_S, what, T_MID, 1);
    draw_text_box(0, 100, 240, &AF_L, name, T_THEME, 1);
}
static void panel_setup(void)
{
    uint32_t i, used = 0, t0 = fm1_ms;
    const panel_t old = panel;
    setup_title();
    while (fm1_in.buttons) {                             /* wait for OCT-/OCT+ release */
        fm1_wdt_feed();
        if (SETUP_GIVE_UP(t0))
            goto timeout;
    }
    fm1_input_edges(0);
    for (i = 0; i < NB; i++) {
        uint32_t p = 0, id;
        setup_show("PRESS", B_NAME[i]);
        t0 = fm1_ms;
        while (!(p & ~used)) {
            fm1_wdt_feed();
            p |= fm1_input_edges(0);
            if (SETUP_GIVE_UP(t0))
                goto timeout;
        }
        for (id = 0; id < 14u; id++)
            if (((p & ~used) >> id) & 1u)
                break;
        panel.btn[i] = (uint8_t)id;
        used |= 1u << id;
    }
    used = 0;
    for (i = 0; i < NE; i++) {
        uint32_t e;
        int32_t st = 0;
        setup_show("TURN RIGHT", E_NAME[i]);
        for (e = 0; e < 7u; e++)
            fm1_enc_take(e);
        t0 = fm1_ms;
        for (;;) {
            fm1_wdt_feed();
            if (SETUP_GIVE_UP(t0))
                goto timeout;
            for (e = 0; e < 7u; e++)
                if (!((used >> e) & 1u) && (st = fm1_enc_take(e)) != 0)
                    break;
            if (e < 7u)
                break;
        }
        panel.enc[i] = (uint8_t)e;
        panel.dir[i] = (int8_t)(st > 0 ? 1 : -1);
        used |= 1u << e;
        fm1_delay_ms(300);
        fm1_enc_take(e);
    }
    panel.magic = PANEL_MAGIC;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    return;
timeout:
    panel = old;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    ui_message("SETUP CANCELLED");
}
