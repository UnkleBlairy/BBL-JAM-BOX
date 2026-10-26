/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), BBL JAM BOX */
/* smp_usb.c - the sample uploader's transfer into the sample memory (eng_sampler.c has its layout), over the update's
 * SysEx frames (ota.c: F0 pack7(00 59 cmd len24 body chk) F7, both ways). Every request gets one reply, the same cmd:
 *   0x60 BEGIN  {}                          -> {rc, cap u32}              a session starts: no chunk plays from here
 *   0x61 SUM    {off u32}                   -> {rc, off u32, crc u32}     CRC-32 of the 4 KiB sector at off (the host
 *                                                                          skips sectors that already hold its bytes)
 *   0x62 WRITE  {off u32, up to 512 bytes}  -> {rc, off u32}              off in the region, a multiple of 256; a
 *                                                                          sector's first block (off % 4096 == 0) erases
 *                                                                          it first; read back after the write
 *   0x63 END    {}                          -> {rc, count, data len u32}  the table checked (smp_scan); session over
 * rc: 0 ok, 1 arguments, 2 erase, 3 write, 4 read back, 5 the table or the data don't check.
 * off counts from the region's start (flash SMP_BASE). The host writes the data first and the table sector (off 0)
 * last, so an upload cut short leaves no table: the memory reads as empty, never half old, half new. A block that
 * fails is sent again from its sector's start. A session ends at END, or 10 s after the last request (then the
 * table is read as the flash has it). The audio is silent while it runs (an erase stops it ~40 ms). */
#define SMP_REGION (0xDC000u - SMP_BASE)
#define SMP_SESS_MS 10000u
enum { SMP_BEGIN = 0x60, SMP_SUM, SMP_WRITE, SMP_END };

/* one request (ota_dec, d bytes decoded) -> 1 when it ended the session */
static int smp_handle(const uint8_t *m, uint32_t d)
{
    uint32_t cmd = m[2], n = d - 7u, off = n >= 4u ? ota_rd32(m + 6) : 0xFFFFFFFFu, rc = 0;
    const uint8_t *b = m + 10;
    uint8_t r[12];
    uint32_t i;
    for (i = 0; i < sizeof r; i++)
        r[i] = 0;
    switch (cmd) {
    case SMP_BEGIN:
        ota_wr32(r + 1, SMP_CAP);
        ota_send_msg(cmd, r, 5);
        return 0;
    case SMP_SUM:
        if (n != 4u || off >= SMP_REGION || (off & 0xFFFu))
            rc = 1;
        else
            ota_wr32(r + 5, st_crc32(SMP_XIP(SMP_BASE + off), 0x1000u));
        r[0] = (uint8_t)rc;
        ota_wr32(r + 1, off);
        ota_send_msg(cmd, r, 9);
        return 0;
    case SMP_WRITE:
        n -= 4u;
        if (d < 12u || n > 512u || (off & 0xFFu) || off >= SMP_REGION || n > SMP_REGION - off)
            rc = 1;
        else if (!(off & 0xFFFu) && st_erase(SMP_BASE + off))
            rc = 2;
        else if (st_prog(SMP_BASE + off, b, n))
            rc = 3;
        else if (memcmp(SMP_XIP(SMP_BASE + off), b, n))
            rc = 4;
        r[0] = (uint8_t)rc;
        ota_wr32(r + 1, off);
        ota_send_msg(cmd, r, 5);
        return 0;
    default:                                        /* END */
        smp_scan();
        r[0] = smp_scan_ok ? 0 : 5;
        r[1] = smp_n;
        ota_wr32(r + 2, smp_scan_ok ? smp_u32(SMP_XIP(SMP_BASE + 8)) : 0u);
        ota_send_msg(cmd, r, 6);
        return 1;
    }
}

static void smp_show(uint32_t pct)                  /* the screen while samples load */
{
    char b[16];
    uint32_t k = 0;
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(0, 92, 240, &AF_M, "SAMPLES", T_TEXT, 1);
    str_cpy(b, "LOADING ", sizeof b);
    while (b[k])
        k++;
    if (pct >= 100u)
        b[k++] = '1';
    if (pct >= 10u)
        b[k++] = (char)('0' + pct / 10u % 10u);
    b[k++] = (char)('0' + pct % 10u);
    b[k++] = '%';
    b[k] = 0;
    draw_text_box(0, 124, 240, &AF_S, b, T_THEME, 1);
}

/* main loop (ota_service): a BEGIN came: the whole session here, then back to the groovebox */
static void smp_session(uint32_t d)
{
    uint32_t last, shown = 0xFFu, done;
    panic_req = (1u << NTRK) - 1u;                  /* every track's voices out */
    fm1_irq_off();
    if (song.playing)
        seq_stop();
    smp_n = 0;                                      /* nothing plays the flash while it changes */
    smp_gen++;
    fm1_irq_on();
    scr_wake_now();
    smp_show(0);
    done = smp_handle(ota_dec, d);
    for (last = ota_now_ms(); !done && ota_now_ms() - last < SMP_SESS_MS;) {
        ota_idle();
        d = ota_take();
        if (!d)
            continue;
        last = ota_now_ms();
        if (d == 7u && ota_dec[2] == 0x11) {        /* (an updater's handshake) */
            ota_reply_identity();
            continue;
        }
        if (ota_dec[2] < SMP_BEGIN || ota_dec[2] > SMP_END)
            continue;
        if (ota_dec[2] == SMP_WRITE && d >= 11u) {  /* progress: how far into the region */
            uint32_t pct = (ota_rd32(ota_dec + 6) >> 12) * 100u / (SMP_REGION >> 12);
            if (pct / 5u != shown) {
                shown = pct / 5u;
                smp_show(pct);
            }
        }
        done = smp_handle(ota_dec, d);
    }
    if (!done)
        smp_scan();                                 /* (the host went away: what the flash holds) */
    lcd_fill(0, 0, 240, 240, T_BG);
    ui_message(smp_n ? "SAMPLES LOADED" : "NO SAMPLES");
    ui.force = 1;
}
