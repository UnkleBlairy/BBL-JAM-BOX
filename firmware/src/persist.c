/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), BBL JAM BOX; based on Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The device's own settings in flash (OBJ_SETTINGS): the panel's calibration (panel_t), the palette, HOLD and LEDS.
 * The layout is Felucca's PER4 (the bytes the groovebox does not use are kept as they were saved), so a unit that
 * ran Felucca keeps its calibration. No music is kept here: a jam starts from scratch (the sounds and kits: gb_store.c). */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;   /* (unused: kept as saved) */
} persist_t;
#define PERSIST_MAGIC 0x50455234u

/* the stored settings into the device; 0 = none (another layout, torn) */
static int settings_import(persist_t *p, int n)
{
    if (n != (int)sizeof *p || p->magic != PERSIST_MAGIC)
        return 0;
    p->palette = palette_to_stored(palette_from_stored(p->palette));
    settings.magic = SETTINGS_MAGIC;
    settings.palette = palette_from_stored(p->palette);
    settings.lowcut = p->lowcut;
    settings.zoom = p->zoom;
    settings_hold = (uint8_t)hold_from_stored(p->bold);
    settings_leds = (uint8_t)leds_from_stored(p->zoom);
    if (p->panel.magic == PANEL_MAGIC)
        panel = p->panel;
    return 1;
}

/* the device's settings into p (the last one imported or saved: its unused bytes stay) */
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom = leds_to_stored(settings.zoom, settings_leds);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
}

#if FELUCCA_FLASH
static persist_t persist_saved __attribute__((section(".pool")));
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint32_t persist_retry_ms;
#endif

static void smp_scan(void);
static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;       /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();
    if (settings_import(&p, st_load(OBJ_SETTINGS, &p, sizeof p)))
        persist_saved = p;
    smp_scan();                                         /* the sampler's chunks (gb_store.c) */
#endif
}

/* nothing going on that an erase (the audio stopped ~40 ms) would be heard in, or would race */
static int autosave_quiet(void)
{
    uint32_t k, i;
    if (transport_busy() || fm1_in.notes || fm1_in.buttons)
        return 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            if (trk[k].v[i].active)
                return 0;
    return 1;
}

/* main loop: a requested settings save, when nothing plays */
static void settings_poll(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (!persist_pending || !flash_ok || !autosave_quiet() ||
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
    p = persist_saved;
    settings_export(&p);
    if (!memcmp(&p, &persist_saved, sizeof p)) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0) {
        persist_saved = p;
        persist_pending = 0;
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
    }
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}

#if FELUCCA_FLASH
_Static_assert(sizeof(persist_t) <= ST_PAYLOAD_MAX, "settings do not fit one flash sector");
#endif
