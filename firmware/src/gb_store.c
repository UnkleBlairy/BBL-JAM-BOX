/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), BBL JAM BOX; based on Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* gb_store.c - the groovebox's sounds and kits (FELUCCA_SIMPLE). Patterns are never saved: it is for jamming.
 *   SOUND  16 slots a pool (96; engines.c gb_pool: KICK SNARE CYMBAL PERC SYNTH SAMPLE): the selected track's machine
 *          picks the pool; a sound keeps its machine (loading it switches the track's). Its sequencer and mixer
 *          settings left out. (Before: 16 a track, GB_MAGIC; moved into the pools once, gb_pool_move)
 *   KIT    11 slots: the six tracks' sounds, the macros, the PERF fade, the FX and COMB globals (not the tempo,
 *          the DJ filter, MIDI or the clock), the LOCKS keys (GB_KLK bytes of them: gbk_lk_pack)
 * Seven storage objects (storage.c OBJ_GB0..): three of 32 sounds (two machines each), four of 3 kits. They live in
 * Felucca's old projects', user presets' and autosave's sectors (st_sector). Kits of the four-track build (four to
 * an object, GB_MAGIC) load into these (gb_kit_old: tracks 5 and 6 as they are). A RAM mirror of each
 * (.pool) is what loads read; a save changes the mirror at once and goes to flash when nothing plays (an erase
 * stops the audio: project.c autosave_quiet), one object a pass. A slot keeps P_COUNT with it, so a later build
 * with other parameters maps the values by count (core.h params_by_count). */
#define GB_MACH 6u                                       /* sound pools (two to a storage object) */
_Static_assert(GB_NPOOL <= GB_MACH, "more pools: more sound objects (GB_SOBJ, storage.c)");
#define GB_SLOTS 16u
#define GB_KITS 11u
#define GB_SOBJ 3u                                       /* sound objects (two machines each) */
#define GB_KOBJ 4u                                       /* kit objects (3 kits each) */
#define GB_KPER 3u
#define GB_MAGIC 0x31534247u                             /* "GBS1": the sounds; the four-track build's kits */
#define GB_KMAGIC 0x324B4247u                            /* "GBK2": the six-track kits */
#define GB_PMAGIC 0x33534247u                            /* "GBS3": the sounds by pool */
#define GB_K3MAGIC 0x334B4247u                           /* "GBK3": the kits with their LOCKS keys */
#define GB_KLK 264u                                      /* a kit's LOCKS keys: a count, then each key with locks: track << 4 |
                                                          * key, n, n ids, n values (int16) */
#define GB_KG 27u                                        /* the globals a kit has room for (G_COUNT when kits began; the FX
                                                          * LFOs' WAVEs after them go in rsv: gbk_wave) */
typedef struct { uint8_t used, np; int8_t p[P_COUNT]; } gb_snd_t;   /* values - 64 (all of them fit in -64..127: project.c);
                                                                    * used: 1 a DIGITAL sound (all before the 808s), 2 ANALOG,
                                                                    * 3 SAMPLER */
typedef struct { uint32_t magic; uint16_t np, kind; gb_snd_t s[2u * GB_SLOTS]; } gb_sobj_t;
typedef struct {
    uint8_t used, rel, fade, rsv;
    gb_snd_t s[NTRK];
    perf_mac_t mac[PM_NM];
    int16_t g[GB_KG];
    uint8_t lk[GB_KLK];
} gb_kit_t;
typedef struct { uint32_t magic; uint16_t np, kind; gb_kit_t k[GB_KPER]; } gb_kobj_t;
/* the end of a kit's lk: each track's step note / chord (seq.c lastc, lastn): GB_NT0 GB_NT1, then per track how many
 * (0: none, middle C / the machine's) and 4 notes. The LOCKS keys stop before it (GB_KLK_LK); a kit saved before it
 * has no mark there: none */
#define GB_NTB (2u + NTRK * 5u)
#define GB_KLK_LK (GB_KLK - GB_NTB)
#define GB_NT0 0x5Au
#define GB_NT1 0xA5u
/* the kits before the LOCKS keys (GB_KMAGIC): the same up to lk; read once, written as gb_kobj_t */
typedef struct {
    uint8_t used, rel, fade, rsv;
    gb_snd_t s[NTRK];
    perf_mac_t mac[PM_NM];
    int16_t g[GB_KG];
} gb_kit2_t;
typedef struct { uint32_t magic; uint16_t np, kind; gb_kit2_t k[GB_KPER]; } gb_kobj2_t;
/* the four-track build's kit objects (OBJ_GB0 + 3 .. 5, four kits each): read once, then written as gb_kobj_t */
typedef struct {
    uint8_t used, rel, fade, rsv;
    gb_snd_t s[4];
    perf_mac_t mac[PM_NM];
    int16_t g[GB_KG];
} gb_kit_old_t;
typedef struct { uint32_t magic; uint16_t np, kind; gb_kit_old_t k[4]; } gb_kobj_old_t;
_Static_assert(sizeof(gb_kobj_old_t) == 3136u, "the four-track build's kit objects");
_Static_assert(sizeof(gb_sobj_t) <= 3840u, "32 sounds in a storage object");
_Static_assert(sizeof(gb_kobj_t) <= 3840u, "3 kits in a storage object");
_Static_assert(GB_KOBJ * GB_KPER >= GB_KITS, "every kit has a place");
static gb_sobj_t gb_so[GB_SOBJ] __attribute__((section(".pool")));
static gb_kobj_t gb_ko[GB_KOBJ] __attribute__((section(".pool")));
static uint8_t gb_dirty, gb_ready;                       /* bit o: object o (sounds 0..2, kits 3..6) to write */

/* a sound's own values: not its sequencer's (LEN DIV SWING GATE QUANTIZE CHANCE) nor the track's MUTE */
static int gb_sound_id(uint32_t id)
{
    return id != P_SLEN && id != P_SDIV && id != P_SSWING && id != P_SGATE && id != P_SQNT && id != P_APROB && id != P_MUTE;
}
/* the globals a kit holds: the delay, the reverb, the COMB, swing */
static const uint8_t GB_KIT_G[] = {G_SWING, G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX, G_RSIZE, G_RDAMP, G_RTYPE,
                                   G_SYNC, G_CRATE, G_CDEPTH, G_INFO};
/* .. and the FX track's LFOs (not AMP: the level, as the tempo and the DJ): RATE DEST AMT in g, the WAVEs in rsv
 * (1 + their three in base 6; 0 a kit from before them: the LFOs stay as they are) */
static const uint8_t GB_KIT_FXL[] = {G_F1RATE, G_F1DEST, G_F1AMT, G_F2RATE, G_F2DEST, G_F2AMT, G_F3RATE, G_F3DEST, G_F3AMT};
_Static_assert(G_F3AMT < GB_KG && G_AMP < GB_KG && G_F1WAVE >= GB_KG, "the kit's g: the old ids");

static gb_kit_t *gb_kit(uint32_t k);
/* old kit object o (four kits of four tracks) into the mirrors (gb_store_init: only while no new kit object is in
 * flash); written back as new kits at the next quiet moment (gb_store_poll). Its macros' globals (track 4 then) become
 * PM_G */
static void gb_kit_old_load(uint32_t o)
{
    static gb_kobj_old_t old __attribute__((section(".pool")));
    uint32_t i, j, k;
    if (st_load(OBJ_GB0 + GB_SOBJ + o, &old, sizeof old) != (int)sizeof old || old.magic != GB_MAGIC)
        return;
    for (i = 0; i < 4u; i++) {
        const gb_kit_old_t *ok = &old.k[i];
        gb_kit_t *kt;
        k = o * 4u + i;
        if (k >= GB_KITS || !ok->used)
            continue;
        kt = gb_kit(k);
        memset(kt, 0, sizeof *kt);
        kt->used = 1;
        kt->rel = ok->rel;
        kt->fade = ok->fade;
        for (j = 0; j < 4u; j++)
            kt->s[j] = ok->s[j];
        memcpy(kt->mac, ok->mac, sizeof kt->mac);
        for (j = 0; j < PM_NM; j++) {
            uint32_t n;
            for (n = 0; n < PM_N; n++)
                if (kt->mac[j].trk[n] == 4u)
                    kt->mac[j].trk[n] = PM_G;
        }
        memcpy(kt->g, ok->g, sizeof kt->g);
        gb_ko[k / GB_KPER].magic = GB_K3MAGIC;
        gb_ko[k / GB_KPER].np = old.np;
        gb_dirty |= (uint8_t)(1u << (GB_SOBJ + k / GB_KPER));
    }
}

static gb_snd_t *gb_slot(uint32_t mach, uint32_t slot);
/* the sounds of the per-track build (GB_MAGIC objects, now in gb_so) into their machines' pools, in order (a full pool
 * keeps its first 16); written back as pool objects at the next quiet moment */
static void gb_pool_move(void)
{
    static gb_snd_t old[GB_SOBJ * 2u * GB_SLOTS] __attribute__((section(".pool")));
    uint8_t fill[GB_MACH] = {0};
    uint32_t o, i, n = 0;
    for (o = 0; o < GB_SOBJ; o++) {
        if (gb_so[o].magic == GB_MAGIC)
            for (i = 0; i < 2u * GB_SLOTS; i++)
                if (gb_so[o].s[i].used)
                    old[n++] = gb_so[o].s[i];
        memset(&gb_so[o], 0, sizeof gb_so[o]);
        gb_so[o].magic = GB_PMAGIC;
        gb_so[o].np = (uint16_t)P_COUNT;
        gb_dirty |= (uint8_t)(1u << o);
    }
    for (i = 0; i < n; i++) {
        int16_t in[P_COUNT], def[P_COUNT], out[P_COUNT];
        uint32_t j, np = old[i].np > P_COUNT ? P_COUNT : old[i].np, eng, m, pl;
        for (j = 0; j < P_COUNT; j++)
            def[j] = TP[j].def;
        for (j = 0; j < np; j++)
            in[j] = (int16_t)(old[i].p[j] + 64);
        params_by_count(out, in, np, def);
        eng = old[i].used == 2u ? ENGI_ANALOG : old[i].used == 3u ? ENGI_SAMPLER : old[i].used == 4u ? ENGI_MONO : ENGI_DIGITAL;
        for (m = 0; m < NMACH && !(MACHINES[m].engine == eng && MACHINES[m].sub == (uint32_t)out[P_E7]); m++)
            ;
        pl = gb_pool(m < NMACH ? m : 0u);
        if (fill[pl] < GB_SLOTS)
            *gb_slot(pl, fill[pl]++) = old[i];
    }
}

static void gb_store_init(void)
{
    uint32_t o, old = 0;
    if (gb_ready)
        return;
    gb_ready = 1;
    for (o = 0; o < GB_SOBJ; o++) {
#if FELUCCA_FLASH
        if (st_load(OBJ_GB0 + o, &gb_so[o], sizeof gb_so[o]) == (int)sizeof gb_so[o] &&
            (gb_so[o].magic == GB_PMAGIC || gb_so[o].magic == GB_MAGIC)) {
            old |= gb_so[o].magic == GB_MAGIC;
            continue;
        }
#endif
        memset(&gb_so[o], 0, sizeof gb_so[o]);
    }
    if (old)                                             /* (the per-track build's: into the pools, once) */
        gb_pool_move();
    for (o = 0; o < GB_KOBJ; o++) {
#if FELUCCA_FLASH
        static gb_kobj2_t k2 __attribute__((section(".pool")));
        if (st_load(OBJ_GB0 + GB_SOBJ + o, &gb_ko[o], sizeof gb_ko[o]) == (int)sizeof gb_ko[o] && gb_ko[o].magic == GB_K3MAGIC)
            continue;
        if (st_load(OBJ_GB0 + GB_SOBJ + o, &k2, sizeof k2) == (int)sizeof k2 && k2.magic == GB_KMAGIC) {
            uint32_t i;                                  /* (kits before the LOCKS keys: none in them) */
            memset(&gb_ko[o], 0, sizeof gb_ko[o]);
            gb_ko[o].magic = GB_K3MAGIC;
            gb_ko[o].np = k2.np;
            for (i = 0; i < GB_KPER; i++)
                memcpy(&gb_ko[o].k[i], &k2.k[i], sizeof k2.k[i]);
            gb_dirty |= (uint8_t)(1u << (GB_SOBJ + o));
            continue;
        }
#endif
        memset(&gb_ko[o], 0, sizeof gb_ko[o]);
    }
#if FELUCCA_FLASH
    for (o = 0; o < GB_KOBJ && gb_ko[o].magic != GB_K3MAGIC; o++)
        ;
    if (o == GB_KOBJ)                                    /* no six-track kits yet: the four-track build's, once */
        for (o = 0; o < 3u; o++)
            gb_kit_old_load(o);
#endif
}

/* every input pass: a changed object to flash, when an erase would not be heard */
static void gb_store_poll(void)
{
#if FELUCCA_FLASH
    uint32_t o;
    if (!gb_dirty || !autosave_quiet())
        return;
    for (o = 0; o < GB_SOBJ + GB_KOBJ; o++)
        if ((gb_dirty >> o) & 1u) {
            const void *src = o < GB_SOBJ ? (const void *)&gb_so[o] : (const void *)&gb_ko[o - GB_SOBJ];
            uint32_t len = o < GB_SOBJ ? sizeof gb_so[0] : sizeof gb_ko[0];
            if (st_save(OBJ_GB0 + o, src, len) != 0)
                ui_message("SAVE FAILED");
            gb_dirty &= (uint8_t)~(1u << o);
            return;                                      /* (one a pass) */
        }
#else
    gb_dirty = 0;
#endif
}

#define GB_SN_MARK 0x5A
static void gb_snd_take(gb_snd_t *s, const track_t *t)
{
    uint32_t i;
    s->used = eng_idx(t->eng_req) == ENGI_ANALOG ? 2u : eng_idx(t->eng_req) == ENGI_SAMPLER ? 3u : eng_idx(t->eng_req) == ENGI_MONO ? 4u : 1u;
    s->np = (uint8_t)P_COUNT;
    for (i = 0; i < P_COUNT; i++)
        s->p[i] = (int8_t)(clamp(motion_base_value(t, i), -64, 127) - 64);   /* (the sound's own, not a lock playing) */
    /* the track's step note / chord (seq.c lastc, lastn), in bytes a sound doesn't use (gb_sound_id). MUTE's byte marks it:
     * a sound saved before holds -64 or -63 there */
    i = (uint32_t)(t - trk) % NTRK;
    s->p[P_MUTE] = GB_SN_MARK;
    s->p[P_SLEN] = (int8_t)(lastc[i] > 4u ? 4u : lastc[i]);
    s->p[P_SDIV] = (int8_t)lastn[i][0];
    s->p[P_SSWING] = (int8_t)lastn[i][1];
    s->p[P_SGATE] = (int8_t)lastn[i][2];
    s->p[P_SQNT] = (int8_t)lastn[i][3];
}
/* a sound into track t: its own values; its sequencer and mixer settings stay */
static void gb_snd_put(const gb_snd_t *s, track_t *t)
{
    int16_t in[P_COUNT], def[P_COUNT], out[P_COUNT];
    uint32_t i, np = s->np > P_COUNT ? P_COUNT : s->np;
    for (i = 0; i < P_COUNT; i++)
        def[i] = t->p[i];
    for (i = 0; i < np; i++)
        in[i] = (int16_t)(s->p[i] + 64);
    params_by_count(out, in, np, def);
    motion_restore(t);                                   /* (the locks playing put back first: motion_rebase) */
    fm1_irq_off();                                       /* (the ISR sees the old engine or the new one, never a mix) */
    t->eng_req = s->used == 2u ? ENGI_ANALOG : s->used == 3u ? ENGI_SAMPLER : s->used == 4u ? ENGI_MONO : ENGI_DIGITAL;   /* its engine first: the values fit its knobs */
    t->p[P_E7] = out[P_E7];
    for (i = 0; i < P_COUNT; i++)
        if (gb_sound_id(i))
            t->p[i] = (int16_t)param_fit(track_desc(t, i), out[i]);
    fm1_irq_on();
    motion_rebase(t);
}

static uint32_t gb_mach_of(const track_t *t) { return gb_pool(trk_mach(t)); }   /* (its machine's pool) */
static gb_snd_t *gb_slot(uint32_t mach, uint32_t slot)
{
    mach %= GB_MACH;
    return &gb_so[mach / 2u].s[(mach & 1u) * GB_SLOTS + slot % GB_SLOTS];
}
static gb_kit_t *gb_kit(uint32_t k) { return &gb_ko[(k % GB_KITS) / GB_KPER].k[(k % GB_KITS) % GB_KPER]; }

static int gbs_used(uint32_t mach, uint32_t slot) { gb_store_init(); return gb_slot(mach, slot)->used; }
static int gbk_used(uint32_t k) { gb_store_init(); return k < GB_KITS && gb_kit(k)->used; }

static int gbs_save_sound(uint32_t slot)
{
    uint32_t m = gb_mach_of(TSEL);
    gb_store_init();
    gb_snd_take(gb_slot(m, slot), TSEL);
    gb_so[m / 2u].magic = GB_PMAGIC;
    gb_so[m / 2u].np = (uint16_t)P_COUNT;
    gb_dirty |= (uint8_t)(1u << (m / 2u));
    return 1;
}
static int gbs_load_sound(uint32_t slot)
{
    const gb_snd_t *s;
    gb_store_init();
    s = gb_slot(gb_mach_of(TSEL), slot);
    if (!s->used) {
        ui_message("EMPTY SLOT");
        return 0;
    }
    gb_snd_put(s, TSEL);
    {                                                    /* its step note / chord; a sound from before: middle C */
        uint32_t i = song.sel % NTRK;
        int has = P_MUTE < s->np && s->p[P_MUTE] == GB_SN_MARK && s->p[P_SLEN] >= 0 && s->p[P_SLEN] <= 4;
        lastc[i] = (uint8_t)(has ? s->p[P_SLEN] : 0);
        if (has) {
            lastn[i][0] = (uint8_t)s->p[P_SDIV];
            lastn[i][1] = (uint8_t)s->p[P_SSWING];
            lastn[i][2] = (uint8_t)s->p[P_SGATE];
            lastn[i][3] = (uint8_t)s->p[P_SQNT];
        }
    }
    ui.force = 1;
    return 1;
}
/* the LOCKS keys (seq.c kit) into a kit's lk: 0 when they did not all fit (the first ones kept) */
static int gbk_lk_pack(uint8_t *lk)
{
    uint32_t k, o = 1, n = 0;
    memset(lk, 0, GB_KLK);
    for (k = 0; k < NTRK * 16u; k++) {
        const kit_key_t *kk = &kit[k / 16u][k % 16u];
        uint32_t c = kk->n > KIT_N ? KIT_N : kk->n;
        if (!c || k % 16u >= KIT_KEYS)
            continue;
        if (o + 2u + 3u * c > GB_KLK_LK || n == 255u) {   /* (the step notes after them) */
            lk[0] = (uint8_t)n;
            return 0;
        }
        lk[o++] = (uint8_t)((k / 16u) << 4 | (k % 16u));
        lk[o++] = (uint8_t)c;
        memcpy(lk + o, kk->id, c);
        o += c;
        memcpy(lk + o, kk->v, 2u * c);
        o += 2u * c;
        n++;
    }
    lk[0] = (uint8_t)n;
    return 1;
}
/* a kit's lk back into the LOCKS keys (all six tracks': a kit with none empties them) */
static void gbk_lk_unpack(const uint8_t *lk)
{
    uint32_t i, o = 1, k;
    for (k = 0; k < NTRK; k++)
        if (kit_cur[k])
            kit_undo(&trk[k]);                           /* (a key's locks sounding: put back first) */
    memset(kit, 0, sizeof kit);
    for (i = 0; i < lk[0]; i++) {
        uint32_t at = lk[o], c = lk[o + 1u];
        kit_key_t *kk;
        if (c > KIT_N || o + 2u + 3u * c > GB_KLK)
            break;
        kk = (at >> 4) < NTRK ? &kit[at >> 4][at & 15u] : 0;
        if (kk) {
            kk->n = (uint8_t)c;
            memcpy(kk->id, lk + o + 2u, c);
            memcpy(kk->v, lk + o + 2u + c, 2u * c);
        }
        o += 2u + 3u * c;
    }
}
static int gbk_save(uint32_t k)
{
    gb_kit_t *kt;
    uint32_t i;
    if (k >= GB_KITS)
        return 0;
    gb_store_init();
    kt = gb_kit(k);
    memset(kt, 0, sizeof *kt);
    kt->used = 1;
    for (i = 0; i < NTRK; i++)
        gb_snd_take(&kt->s[i], &trk[i]);
    memcpy(kt->mac, pm_mac, sizeof kt->mac);
    kt->rel = pm_rel;
    kt->fade = pm_fade;
    for (i = 0; i < GB_KG; i++)
        kt->g[i] = song.g[i];
    kt->rsv = (uint8_t)(1u + (uint32_t)song.g[G_F1WAVE] % 6u + 6u * ((uint32_t)song.g[G_F2WAVE] % 6u) +
                        36u * ((uint32_t)song.g[G_F3WAVE] % 6u));
    if (!gbk_lk_pack(kt->lk))
        ui_message("KEY LOCKS PART SAVED");
    kt->lk[GB_KLK_LK] = GB_NT0;                          /* the step notes (the keys' last note / chord) */
    kt->lk[GB_KLK_LK + 1u] = GB_NT1;
    for (i = 0; i < NTRK; i++) {
        uint8_t *n = &kt->lk[GB_KLK_LK + 2u + 5u * i];
        n[0] = lastc[i] > 4u ? 4u : lastc[i];
        memcpy(n + 1, lastn[i], 4);
    }
    gb_ko[k / GB_KPER].magic = GB_K3MAGIC;
    gb_ko[k / GB_KPER].np = (uint16_t)P_COUNT;
    gb_dirty |= (uint8_t)(1u << (GB_SOBJ + k / GB_KPER));
    return 1;
}
static int gbk_load(uint32_t k)
{
    const gb_kit_t *kt;
    uint32_t i;
    if (k >= GB_KITS)
        return 0;
    gb_store_init();
    kt = gb_kit(k);
    if (!kt->used) {
        ui_message("EMPTY KIT");
        return 0;
    }
    if (pm_h.n) {
        ui_message("LET GO OF THE MACROS");
        return 0;
    }
    for (i = 0; i < NTRK; i++)
        if (kt->s[i].used)                               /* (a kit of the four-track build: tracks 5 and 6 stay) */
            gb_snd_put(&kt->s[i], &trk[i]);
    memcpy(pm_mac, kt->mac, sizeof pm_mac);
    for (i = 0; i < PM_NM; i++)                          /* (a kit of another build: what this one can hold) */
        if (pm_mac[i].n > PM_N)
            pm_mac[i].n = 0;
    pm_rel = kt->rel > 16u ? 16u : kt->rel;
    pm_fade = kt->fade > 4u ? 0u : kt->fade;
    for (i = 0; i < NELEM(GB_KIT_G); i++)
        song.g[GB_KIT_G[i]] = (int16_t)clamp(kt->g[GB_KIT_G[i]], GP[GB_KIT_G[i]].min, GP[GB_KIT_G[i]].max);
    if (kt->rsv) {                                       /* (a kit with the FX track's LFOs) */
        uint32_t w = kt->rsv - 1u;
        for (i = 0; i < NELEM(GB_KIT_FXL); i++)
            song.g[GB_KIT_FXL[i]] = (int16_t)clamp(kt->g[GB_KIT_FXL[i]], GP[GB_KIT_FXL[i]].min, GP[GB_KIT_FXL[i]].max);
        song.g[G_F1WAVE] = (int16_t)(w % 6u);
        song.g[G_F2WAVE] = (int16_t)(w / 6u % 6u);
        song.g[G_F3WAVE] = (int16_t)(w / 36u % 6u);
    }
    gbk_lk_unpack(kt->lk);
    for (i = 0; i < NTRK; i++)                           /* the step notes as saved (none: middle C, the machine's) */
        if (kt->s[i].used) {
            const uint8_t *n = &kt->lk[GB_KLK_LK + 2u + 5u * i];
            int has = kt->lk[GB_KLK_LK] == GB_NT0 && kt->lk[GB_KLK_LK + 1u] == GB_NT1 && n[0] <= 4u;
            lastc[i] = (uint8_t)(has ? n[0] : 0u);
            if (has)
                memcpy(lastn[i], n + 1, 4);
        }
    ui.force = 1;
    return 1;
}

/* ---- the sample memory (eng_sampler.c has the layout): its table, checked whole (a bad table or data: no chunks) */
static inline uint32_t smp_u32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint32_t smp_u16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static int smp_scan_ok;                            /* the last scan found a good table (smp_usb.c END) */
static void smp_scan(void)                         /* at boot (persist_boot) and after an upload */
{
    static smp_chunk_t tb[SMP_MAX];
    const uint8_t *h = SMP_XIP(SMP_BASE), *e;
    uint32_t cnt = 0, dlen = 0, i, k, ok;
    ok = smp_u32(h) == SMP_MAGIC && smp_u16(h + 4) == 1u && (cnt = smp_u16(h + 6)) <= SMP_MAX &&
         (dlen = smp_u32(h + 8)) <= SMP_CAP;
    if (ok)
        ok = ~st_crc32_add(st_crc32_add(0xFFFFFFFFu, h, 28u), h + 32, cnt * 24u) == smp_u32(h + 28);
    for (i = 0; ok && i < cnt; i++) {
        uint32_t off, len, rate;
        e = h + 32 + i * 24u;
        off = smp_u32(e + 12);
        len = smp_u32(e + 16);
        rate = smp_u16(e + 20);
        ok = len >= 64u && off <= dlen && len <= dlen - off && rate >= 4000u && rate <= 48000u && !smp_u16(e + 22);
        for (k = 0; k < 12u; k++)
            tb[i].name[k] = e[k] >= ' ' && e[k] < 127 ? (char)e[k] : (e[k] ? '?' : 0);
        tb[i].name[12] = 0;
        tb[i].off = SMP_DATA + off;
        tb[i].len = len;
        tb[i].rate = (uint16_t)rate;
    }
    if (ok)
        ok = st_crc32(SMP_XIP(SMP_DATA), dlen) == smp_u32(h + 12);
    fm1_irq_off();
    for (i = 0; ok && i < cnt; i++)
        smp_tab[i] = tb[i];
    smp_n = (uint8_t)(ok ? cnt : 0u);
    smp_scan_ok = (int)ok;
    smp_gen++;
    fm1_irq_on();
    ui.force = 1;
}

/* the screen's waveform of a chunk: GB_WAVE columns of its loudest sample (0..255), kept for the chunk last asked */
static uint8_t smp_pk[GB_WAVE];
static uint32_t smp_pk_of = 0xFFFFFFFFu;          /* (slot | gen << 8) */
/* the zoomed waveform (ui_gb.c, START / LEN turned): GB_WAVE peaks of chunk sl's samples x0 .. x0 + w (x0 may be < 0:
 * before the chunk, none), in a buffer of its own */
static uint8_t smp_zpk[GB_WAVE];
static const uint8_t *smp_zpeaks(uint32_t sl, int32_t x0, uint32_t w)
{
    uint32_t x, i;
    const smp_chunk_t *c;
    const uint8_t *d;
    if (sl >= smp_n)
        return 0;
    c = &smp_tab[sl];
    d = SMP_XIP(c->off);
    for (x = 0; x < GB_WAVE; x++) {
        int32_t a = x0 + (int32_t)(x * w / GB_WAVE), b = x0 + (int32_t)((x + 1u) * w / GB_WAVE), m = 0;
        b = b > a ? b : a + 1;
        for (i = (uint32_t)(a < 0 ? 0 : a); (int32_t)i < b && i < c->len; i++) {
            int32_t s = SMP_ULAW[d[i]];
            s = s < 0 ? -s : s;
            m = s > m ? s : m;
        }
        smp_zpk[x] = (uint8_t)(m >> 7);
    }
    return smp_zpk;
}

static const uint8_t *smp_peaks(uint32_t sl)
{
    uint32_t key = sl | smp_gen << 8, x, i;
    if (sl >= smp_n)
        return 0;
    if (smp_pk_of != key) {
        const smp_chunk_t *c = &smp_tab[sl];
        const uint8_t *d = SMP_XIP(c->off);
        for (x = 0; x < GB_WAVE; x++) {
            uint32_t a = x * c->len / GB_WAVE, b = (x + 1u) * c->len / GB_WAVE;   /* (< 2^26: 32 bits) */
            int32_t m = 0;
            b = b > a ? b : a + 1u;
            for (i = a; i < b && i < c->len; i++) {
                int32_t s = SMP_ULAW[d[i]];
                s = s < 0 ? -s : s;
                m = s > m ? s : m;
            }
            smp_pk[x] = (uint8_t)(m >> 7);
        }
        smp_pk_of = key;
    }
    return smp_pk;
}

/* ------------------------------------------------------------ LAST JAM --- */
/* SIMPLE: what you were doing, kept over power-off: every track's machine, sound and sequencer settings, its steps
 * (all 64), the locks and automation (motion.c), the LOCKS keys (seq.c kit), the macros and the PERF fade, the FX lane,
 * the tempo and the kit's globals (not the DJ filter: it starts open), the selected track. Written a second after it all
 * goes quiet (persist.c autosave_quiet: stopped, no key, nothing sounding; an erase stops the audio ~40 ms), when it
 * changed. Two storage objects: OBJ_AUTOSAVE, then OBJ_JAM2 only for a jam over one (each starts with the same
 * generation; two that differ were cut by a power-off between them: nothing restored). Restored at power-on
 * ("LAST JAM"). A stream of sections, each with its sizes, so a later build skips what it can't read */
#define JAM_MAGIC 0x314D414Au                            /* "JAM1" */
#define JAM_PART (ST_PAYLOAD_MAX - 4u)                   /* a part's stream bytes (after its generation) */
#define JAM_CAP (2u * JAM_PART)
static uint8_t jam_buf[JAM_CAP] __attribute__((section(".pool")));    /* the stream just made / read */
static uint8_t jam_old[JAM_CAP] __attribute__((section(".pool")));    /* .. as flash holds it */
static uint8_t jam_img[ST_PAYLOAD_MAX] __attribute__((section(".pool")));
static uint32_t jam_n, jam_oldn, jam_gen, jam_q0, jam_t;
static uint8_t jam_pend;                                 /* bit o: part o of the snapshot in jam_buf to write */

typedef struct { uint8_t *p; uint32_t n, cap, bad; } jam_io_t;
static void jam_w(jam_io_t *w, const void *src, uint32_t len)
{
    if (w->n + len > w->cap) {
        w->bad = 1;                                      /* (too big: the rest left out, what came first kept) */
        return;
    }
    memcpy(w->p + w->n, src, len);
    w->n += len;
}
static void jam_w8(jam_io_t *w, uint32_t v) { uint8_t b = (uint8_t)v; jam_w(w, &b, 1); }
static void jam_w16(jam_io_t *w, uint32_t v) { uint16_t b = (uint16_t)v; jam_w(w, &b, 2); }
static int jam_r(jam_io_t *r, void *dst, uint32_t len)
{
    if (r->bad || r->n + len > r->cap) {
        r->bad = 1;
        return 0;
    }
    if (dst)
        memcpy(dst, r->p + r->n, len);
    r->n += len;
    return 1;
}
static uint32_t jam_r8(jam_io_t *r) { uint8_t b = 0; jam_r(r, &b, 1); return b; }
static uint32_t jam_r16(jam_io_t *r) { uint16_t b = 0; jam_r(r, &b, 2); return b; }

static int jam_step_used(const step_t *s)
{
    return s->n || s->hit || s->acc || s->probability || s->flags || s->time == ST_TIE || s->time == ST_LOCK;
}
static uint32_t jam_make(uint8_t *out)                  /* the stream: its length */
{
    jam_io_t w = {out, 0, JAM_CAP, 0};
    uint32_t k, i, n;
    jam_w(&w, &(uint32_t){JAM_MAGIC}, 4);
    jam_w8(&w, song.sel);
    jam_w8(&w, pm_rel);
    jam_w8(&w, pm_fade);
    jam_w8(&w, G_COUNT);
    jam_w(&w, song.g, sizeof song.g);
    jam_w16(&w, sizeof pm_mac);
    jam_w(&w, pm_mac, sizeof pm_mac);
    jam_w16(&w, sizeof fxl);
    jam_w(&w, &fxl, sizeof fxl);
    jam_w8(&w, motion.count);
    jam_w8(&w, motion.on);
    jam_w(&w, motion.event, motion.count * sizeof motion.event[0]);
    jam_w8(&w, P_COUNT);
    jam_w8(&w, sizeof(step_t));
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        jam_w8(&w, t->eng_req);
        jam_w(&w, t->p, sizeof t->p);
        for (i = n = 0; i < NSTEP; i++)
            n += (uint32_t)jam_step_used(&t->step[i]);
        jam_w8(&w, n);
        for (i = 0; i < NSTEP; i++)
            if (jam_step_used(&t->step[i])) {
                jam_w8(&w, i);
                jam_w(&w, &t->step[i], sizeof(step_t));
            }
    }
    for (k = n = 0; k < NTRK * 16u; k++)                 /* the LOCKS keys with locks */
        n += kit[k / 16u][k % 16u].n != 0u;
    jam_w8(&w, n);
    for (k = 0; k < NTRK * 16u; k++) {
        const kit_key_t *kk = &kit[k / 16u][k % 16u];
        if (!kk->n)
            continue;
        jam_w8(&w, k);
        jam_w8(&w, kk->n);
        jam_w(&w, kk->id, kk->n);
        jam_w(&w, kk->v, kk->n * sizeof kk->v[0]);
    }
    if (!w.bad) {                                        /* ACID (seq.c acid; older jams end before it) */
        jam_w16(&w, sizeof acid);
        jam_w(&w, acid, sizeof acid);
    }
    if (!w.bad) {                                        /* the step notes (seq.c lastc, lastn) */
        jam_w16(&w, sizeof lastc + sizeof lastn);
        jam_w(&w, lastc, sizeof lastc);
        jam_w(&w, lastn, sizeof lastn);
    }
    return w.n;
}

/* the stream back: 1 when it was one */
static int jam_apply(uint8_t *in, uint32_t len)
{
    jam_io_t r = {in, 0, len, 0};
    uint32_t magic = 0, sel, rel, fade, ng, sz, np, ssz, k, i, n;
    static const uint8_t JAM_G[] = {G_BPM, G_SWING, G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX, G_RSIZE, G_RDAMP, G_RTYPE,
                                    G_SYNC, G_CRATE, G_CDEPTH, G_INFO, G_AMP, G_F1RATE, G_F1WAVE, G_F1DEST, G_F1AMT,
                                    G_F2RATE, G_F2WAVE, G_F2DEST, G_F2AMT, G_F3RATE, G_F3WAVE, G_F3DEST, G_F3AMT, G_PAN};
                                    /* (the tempo + GB_KIT_G, AMP and the FX track's LFOs: not the DJ) */
    int16_t g[G_COUNT];
    jam_r(&r, &magic, 4);
    if (magic != JAM_MAGIC)
        return 0;
    sel = jam_r8(&r);
    rel = jam_r8(&r);
    fade = jam_r8(&r);
    ng = jam_r8(&r);
    memcpy(g, song.g, sizeof g);
    for (i = 0; i < ng; i++) {
        int16_t v = 0;
        jam_r(&r, &v, 2);
        if (i < G_COUNT)
            g[i] = v;
    }
    for (i = 0; i < NELEM(JAM_G); i++)
        song.g[JAM_G[i]] = (int16_t)clamp(g[JAM_G[i]], GP[JAM_G[i]].min, GP[JAM_G[i]].max);
    sz = jam_r16(&r);
    if (sz == sizeof pm_mac) {
        jam_r(&r, pm_mac, sz);
        for (i = 0; i < PM_NM; i++)
            if (pm_mac[i].n > PM_N)
                pm_mac[i].n = 0;
    } else {
        jam_r(&r, 0, sz);
    }
    pm_rel = (uint8_t)(rel > 16u ? 16u : rel);
    pm_fade = (uint8_t)(fade > 4u ? 0u : fade);
    sz = jam_r16(&r);
    if (sz == 2u + 32u + 16u * 11u || sz == 2u + 32u + 16u * 15u) {   /* (a jam of the 16-step lane: 11 locks a step, or 15) */
        uint8_t ob[2u + 32u + 16u * 15u];
        uint32_t ng = (sz - 34u) / 16u;
        uint16_t t16;
        memset(&fxl, 0, sizeof fxl);
        fxl.len = 16;
        jam_r(&r, ob, sz);
        memcpy(&t16, ob, 2);
        fxl.trig = t16;
        memcpy(fxl.lk, ob + 2, 32);
        for (i = 0; i < 16u; i++)
            memcpy(fxl.v[i], ob + 34u + ng * i, ng);
    } else if (sz == 8u + 2u + 2u * FXN + FXN * 15u + 6u) {   /* (64 steps, 15 locks a step: before PAN) */
        static uint8_t ob[8u + 2u + 2u * FXN + FXN * 15u + 6u];
        jam_r(&r, ob, sz);
        memset(&fxl, 0, sizeof fxl);
        memcpy(&fxl.trig, ob, 8);
        memcpy(&fxl.len, ob + 8, 2);
        memcpy(fxl.lk, ob + 10, 2u * FXN);
        for (i = 0; i < FXN; i++)
            memcpy(fxl.v[i], ob + 10u + 2u * FXN + 15u * i, 15);
    } else {
        jam_r(&r, sz == sizeof fxl ? (void *)&fxl : 0, sz);
    }
    n = jam_r8(&r);
    motion.on = (uint8_t)jam_r8(&r);
    motion.count = 0;
    for (i = 0; i < n; i++) {
        motion_event_t e;
        if (!jam_r(&r, &e, sizeof e))
            break;
        if ((e.place >> 6) < NTRK && motion.count < MOTION_MAX)
            motion.event[motion.count++] = e;
    }
    np = jam_r8(&r);
    ssz = jam_r8(&r);
    for (k = 0; k < NTRK && !r.bad; k++) {
        track_t *t = &trk[k];
        int16_t in[128], def[P_COUNT], out[P_COUNT];
        uint32_t eng = jam_r8(&r);
        for (i = 0; i < np && i < 128u; i++)
            in[i] = (int16_t)jam_r16(&r);
        for (i = 0; i < P_COUNT; i++)
            def[i] = t->p[i];
        params_by_count(out, in, np > P_COUNT ? P_COUNT : np, def);
        fm1_irq_off();                                   /* (the ISR sees the old engine or the new one, never a mix) */
        t->eng_req = (uint8_t)(eng < NENGINES ? eng : t->eng_req);
        t->p[P_E7] = out[P_E7];
        for (i = 0; i < P_COUNT; i++)
            t->p[i] = (int16_t)param_fit(track_desc(t, i), out[i]);
        fm1_irq_on();
        for (i = 0; i < NSTEP; i++)
            step_clear(&t->step[i]);
        n = jam_r8(&r);
        for (i = 0; i < n; i++) {
            uint32_t at = jam_r8(&r);
            step_t s;
            memset(&s, 0, sizeof s);
            jam_r(&r, ssz == sizeof s ? (void *)&s : 0, ssz);
            if (at < NSTEP && ssz == sizeof s)
                t->step[at] = s;
        }
    }
    memset(kit, 0, sizeof kit);
    n = jam_r8(&r);
    for (i = 0; i < n && !r.bad; i++) {
        uint32_t at = jam_r8(&r), c = jam_r8(&r);
        kit_key_t kk;
        memset(&kk, 0, sizeof kk);
        if (c > KIT_N)
            break;
        jam_r(&r, kk.id, c);
        jam_r(&r, kk.v, c * sizeof kk.v[0]);
        kk.n = (uint8_t)c;
        if (at < NTRK * 16u && !r.bad)
            kit[at / 16u][at % 16u] = kk;
    }
    for (k = 0; k < NTRK; k++)
        acid_reset(&acid[k]);
    if (!r.bad && r.n + 2u <= r.cap) {
        sz = jam_r16(&r);
        jam_r(&r, sz == sizeof acid ? (void *)acid : 0, sz);
    }
    memset(lastc, 0, sizeof lastc);
    if (!r.bad && r.n + 2u <= r.cap) {                   /* the step notes */
        sz = jam_r16(&r);
        if (sz == sizeof lastc + sizeof lastn) {
            jam_r(&r, lastc, sizeof lastc);
            jam_r(&r, lastn, sizeof lastn);
            for (k = 0; k < NTRK; k++)
                lastc[k] = (uint8_t)(lastc[k] > 4u ? 0u : lastc[k]);
        } else {
            jam_r(&r, 0, sz);
        }
    }
    for (k = 0; k < NTRK; k++) {                         /* (the first try's SKIP ACCENT SCALE ROOT: gone) */
        acid[k].scale = acid[k].root = 0;
        for (i = 0; i < ACID_N; i++) {
            acid[k].st[i].flags &= AF_SLIDE;
            acid[k].st[i].stime = acid[k].st[i].stime ? acid[k].st[i].stime : 40u;
        }
    }
    for (k = 0; k < NTRK; k++)
        motion_rebase(&trk[k]);
    song.sel = (uint8_t)(sel < NTRK ? sel : 0u);
    return 1;
}

/* power-on: the last jam back (main.c, after felucca_init) */
static void jam_boot(void)
{
#if FELUCCA_FLASH
    uint32_t g0, len;
    int n0, n1;
    if (!flash_ok)
        return;
    n0 = st_load(OBJ_AUTOSAVE, jam_img, ST_PAYLOAD_MAX);
    if (n0 < 8)
        return;
    memcpy(&g0, jam_img, 4);
    len = (uint32_t)n0 - 4u;
    memcpy(jam_buf, jam_img + 4, len);
    if (len == JAM_PART) {                               /* a full first part: the rest is in the second */
        uint32_t g1;
        n1 = st_load(OBJ_JAM2, jam_img, ST_PAYLOAD_MAX);
        if (n1 >= 4) {
            memcpy(&g1, jam_img, 4);
            if (g1 != g0)
                return;                                  /* (cut between the two: neither is trusted) */
            memcpy(jam_buf + len, jam_img + 4, (uint32_t)n1 - 4u);
            len += (uint32_t)n1 - 4u;
        }
    }
    jam_gen = g0;
    memcpy(jam_old, jam_buf, len);
    jam_oldn = len;
    if (jam_apply(jam_buf, len))
        ui_message("LAST JAM");
#endif
}

/* every input pass: the jam to flash a second after it all went quiet, when it changed (one object a pass) */
static void jam_poll(void)
{
#if FELUCCA_FLASH
    uint32_t o;
    if (!flash_ok)
        return;
    if (!autosave_quiet()) {
        jam_q0 = 0;
        return;
    }
    if (!jam_q0)
        jam_q0 = fm1_ms | 1u;
    if ((int32_t)(fm1_ms - jam_q0) < 1000)
        return;
    if (!jam_pend) {
        if (jam_t && (int32_t)(fm1_ms - jam_t) < 1000)
            return;                                      /* (looked a second ago) */
        jam_t = fm1_ms | 1u;
        jam_n = jam_make(jam_buf);
        if (jam_n == jam_oldn && !memcmp(jam_buf, jam_old, jam_n))
            return;
        jam_gen++;
        jam_pend = jam_n > JAM_PART ? 3u : 1u;           /* (the second part first: the first is the commit) */
    }
    o = (jam_pend & 2u) ? 1u : 0u;
    memcpy(jam_img, &jam_gen, 4);
    {
        uint32_t off = o * JAM_PART, part = jam_n - off > JAM_PART ? JAM_PART : jam_n - off;
        memcpy(jam_img + 4, jam_buf + off, part);
        if (st_save(o ? OBJ_JAM2 : OBJ_AUTOSAVE, jam_img, 4u + part) != 0) {
            jam_pend = 0;                                /* (tried again at the next look) */
            return;
        }
    }
    jam_pend &= (uint8_t)~(1u << o);
    if (!jam_pend) {
        memcpy(jam_old, jam_buf, jam_n);
        jam_oldn = jam_n;
    }
#endif
}
