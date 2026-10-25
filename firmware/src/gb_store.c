/* SPDX-License-Identifier: GPL-3.0-only */
/* gb_store.c - the groovebox's sounds and kits (FELUCCA_SIMPLE). Patterns are never saved: it is for jamming.
 *   SOUND  16 slots a track (96): a track's sound (any machine, FM or 808), its sequencer and mixer settings left out.
 *          (Up to the 808s the slots were a machine's: SYN BD SD HH CYM PERC's are now tracks 1..6's)
 *   KIT    11 slots: the six tracks' sounds, the macros, the PERF fade, the FX and COMB globals (not the tempo,
 *          the DJ filter, MIDI or the clock)
 * Seven storage objects (storage.c OBJ_GB0..): three of 32 sounds (two machines each), four of 3 kits. They live in
 * Felucca's old projects', user presets' and autosave's sectors (st_sector). Kits of the four-track build (four to
 * an object, GB_MAGIC) load into these (gb_kit_old: tracks 5 and 6 as they are). A RAM mirror of each
 * (.pool) is what loads read; a save changes the mirror at once and goes to flash when nothing plays (an erase
 * stops the audio: project.c autosave_quiet), one object a pass. A slot keeps P_COUNT with it, so a later build
 * with other parameters maps the values by count (core.h params_by_count). */
#define GB_MACH 6u                                       /* tracks with sound slots (two to a storage object) */
_Static_assert(NTRK <= GB_MACH, "more tracks: more sound objects (GB_SOBJ, storage.c)");
#define GB_SLOTS 16u
#define GB_KITS 11u
#define GB_SOBJ 3u                                       /* sound objects (two machines each) */
#define GB_KOBJ 4u                                       /* kit objects (3 kits each) */
#define GB_KPER 3u
#define GB_MAGIC 0x31534247u                             /* "GBS1": the sounds; the four-track build's kits */
#define GB_KMAGIC 0x324B4247u                            /* "GBK2": the six-track kits */
typedef struct { uint8_t used, np; int8_t p[P_COUNT]; } gb_snd_t;   /* values - 64 (all of them fit in -64..127: project.c);
                                                                    * used: 1 a DIGITAL sound (all before the 808s), 2 ANALOG,
                                                                    * 3 SAMPLER */
typedef struct { uint32_t magic; uint16_t np, kind; gb_snd_t s[2u * GB_SLOTS]; } gb_sobj_t;
typedef struct {
    uint8_t used, rel, fade, rsv;
    gb_snd_t s[NTRK];
    perf_mac_t mac[PM_NM];
    int16_t g[G_COUNT];
} gb_kit_t;
typedef struct { uint32_t magic; uint16_t np, kind; gb_kit_t k[GB_KPER]; } gb_kobj_t;
/* the four-track build's kit objects (OBJ_GB0 + 3 .. 5, four kits each): read once, then written as gb_kobj_t */
typedef struct {
    uint8_t used, rel, fade, rsv;
    gb_snd_t s[4];
    perf_mac_t mac[PM_NM];
    int16_t g[G_COUNT];
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
        gb_ko[k / GB_KPER].magic = GB_KMAGIC;
        gb_ko[k / GB_KPER].np = old.np;
        gb_dirty |= (uint8_t)(1u << (GB_SOBJ + k / GB_KPER));
    }
}

static void gb_store_init(void)
{
    uint32_t o;
    if (gb_ready)
        return;
    gb_ready = 1;
    for (o = 0; o < GB_SOBJ; o++) {
#if FELUCCA_FLASH
        if (st_load(OBJ_GB0 + o, &gb_so[o], sizeof gb_so[o]) == (int)sizeof gb_so[o] && gb_so[o].magic == GB_MAGIC)
            continue;
#endif
        memset(&gb_so[o], 0, sizeof gb_so[o]);
    }
    for (o = 0; o < GB_KOBJ; o++) {
#if FELUCCA_FLASH
        if (st_load(OBJ_GB0 + GB_SOBJ + o, &gb_ko[o], sizeof gb_ko[o]) == (int)sizeof gb_ko[o] && gb_ko[o].magic == GB_KMAGIC)
            continue;
#endif
        memset(&gb_ko[o], 0, sizeof gb_ko[o]);
    }
#if FELUCCA_FLASH
    for (o = 0; o < GB_KOBJ && gb_ko[o].magic != GB_KMAGIC; o++)
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

static void gb_snd_take(gb_snd_t *s, const track_t *t)
{
    uint32_t i;
    s->used = eng_idx(t->eng_req) == ENGI_ANALOG ? 2u : eng_idx(t->eng_req) == ENGI_SAMPLER ? 3u : 1u;
    s->np = (uint8_t)P_COUNT;
    for (i = 0; i < P_COUNT; i++)
        s->p[i] = (int8_t)(clamp(motion_base_value(t, i), -64, 127) - 64);   /* (the sound's own, not a lock playing) */
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
    t->eng_req = s->used == 2u ? ENGI_ANALOG : s->used == 3u ? ENGI_SAMPLER : ENGI_DIGITAL;   /* its engine first: the values fit its knobs */
    t->p[P_E7] = out[P_E7];
    for (i = 0; i < P_COUNT; i++)
        if (gb_sound_id(i))
            t->p[i] = (int16_t)param_fit(track_desc(t, i), out[i]);
    fm1_irq_on();
    motion_rebase(t);
}

static uint32_t gb_mach_of(const track_t *t) { return (uint32_t)(t - trk) % GB_MACH; }   /* (its track's slots) */
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
    gb_so[m / 2u].magic = GB_MAGIC;
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
    ui.force = 1;
    return 1;
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
    for (i = 0; i < G_COUNT; i++)
        kt->g[i] = song.g[i];
    gb_ko[k / GB_KPER].magic = GB_KMAGIC;
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
    ui.force = 1;
    return 1;
}

/* ---- the sample memory (eng_sampler.c has the layout): its table, checked whole (a bad table or data: no chunks) */
static inline uint32_t smp_u32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint32_t smp_u16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
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
    smp_gen++;
    fm1_irq_on();
    ui.force = 1;
}

/* the screen's waveform of a chunk: GB_WAVE columns of its loudest sample (0..255), kept for the chunk last asked */
static uint8_t smp_pk[GB_WAVE];
static uint32_t smp_pk_of = 0xFFFFFFFFu;          /* (slot | gen << 8) */
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
