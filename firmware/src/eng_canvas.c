/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; CANVAS: the Felucca Canvas fork */
/* CANVAS: a granular performance instrument. The keys are grains: each grain key runs a stream of grains of its own
 * (its own POSition in the material, grain SIZE, DENSity and PiTCH), tapped on and left sounding (LATCH) or sounding
 * while held (HOLD); the streams blend, each fading in and out over FADE. Other keys hold glitches, or are controls.
 *
 * KEYS (P_E1), the layout:
 *   16+FX  the 16 white keys F3 .. G5 are grains 1..16; the 11 black keys are the glitches (CV_GL_*), held
 *   27     every key is a grain (1..27, F3 .. G5); glitches: Felucca's FX layer (FX held + a white key, perform.c)
 *   8+CTL  white keys 1..8 (F3 .. F4) grains 1..8; white keys 9..16 (G4 .. G5) the controls (CV_CT_*); black keys glitches
 * A key's note is 53 + the key (the engine maps the keys: no scale, no octave), so the sequencer, the arp and MIDI IN
 * (notes 53..79, others folded into that range) play the same grains, glitches and controls.
 *
 * Glitches (held): RPT16 RPT32 REVRS FREEZ TAPE and OCT+ OCT- are the FX layer's master effects (perform.c perf_press:
 * beat repeat 1/16 and 1/32, reverse, freeze, tape stop, the harmonizer up and down; they act on the whole mix, as when
 * played from the FX layer); SCATR (every grain at a random place in the material), JUMP (every grain a random leap:
 * an octave or a fifth either way), BURST (four times the grains, a quarter of the size), CRUSH (this track: bits and
 * sample rate, by CRSH) are this engine's own.
 * Controls (8+CTL): ALL OFF (every grain off), RANDOM (the sounding grains to new places), SCAN (while held: every grain's
 * position drifts on through the material, and stays where it got to), SMALL / LARGE (while held: grains a quarter / three
 * times the size), PIT- / PIT+ (while held: an octave down / up), LATCH (LATCH <-> HOLD).
 *
 * The grains' settings (4 values a key, 27 keys) are the track's: cv_slot[track][key] (POS SIZE DENS, PTCH + 24; 7 bits
 * each). Kept as the cells of the earlier CANVAS were: in a project, in the track's FM6 patch area (project.c: the patch
 * then loads as the init voice); in a user preset, in its FM6 patch entry (up_fm6.c); in the undo copy (ui.c undo.cv).
 * A factory preset's: canvas_load_layout. EDIT > GRAINS (ui_canvas.c): the keys as a map; the picked grain's settings
 * on KNOB 1..4 (pressing a grain key picks it; the PRESETS knob picks without playing).
 *
 * Material: the SAMPLE material as GRAIN plays it (IMA ADPCM in flash, decoded on demand from a seek index; eng_grain.c's
 * decoder and index). SRC (P_E0): MIX (eight of the built-in zones), PIANO / FLUTE / SAX (that set's zones), USR1..3 (the
 * slot's zones, up to 16). POS 0..127 runs over the zones in turn, each a share of it, from its start to its end.
 * The others (EDIT 1 / 2): MODE LATCH / HOLD; SPRY a random spread of each grain's place (and its timing); FADE a stream's
 * fade in / out (3 ms .. ~1 s); RAND a random detune a grain (quadratic, up to +-1 octave); TONE a one-pole low-pass;
 * CRSH the CRUSH glitch's depth. LFO -> SHP moves every grain's place, LFO -> PIT their pitch, LFO / ENV -> FLT the TONE.
 *
 * Sound: the streams render in the engine's post hook (once a block, with or without a voice): at most CV_NG grains a part,
 * Hann windowed, forward, each 1 / sqrt(overlap), the streams together 1 / sqrt(how many sound). A key's voice renders
 * nothing (ownenv): it only says the key is held (its gate), and ends at once for a tap that only toggles. */
#define CV_KEYS 27u              /* keys F3 .. G5 */
#define CV_NG 24                 /* grains per part */
#define CV_MAXZ 16u              /* zones of the material */
#define CV_NIDX GR_NIDX          /* seek index entries per part */
#define CV_NOTE0 53              /* key 0's note (F3) */

enum { CV_SRC_MIX, CV_SRC_PIANO, CV_SRC_FLUTE, CV_SRC_SAX, CV_SRC_USR1, CV_SRC_USR2, CV_SRC_USR3, CV_SRC_N };
#if FELUCCA_CANVAS_KEYS >= 0          /* the granular-only firmware: its own synthesized material (gen_canvas_material.py) */
static const char *const N_CV_SRC[CV_SRC_N] = {"MIX", "NATURE", "DREAM", "GLITCH", "USR1", "USR2", "USR3"};
#else
static const char *const N_CV_SRC[CV_SRC_N] = {"MIX", "PIANO", "FLUTE", "SAX", "USR1", "USR2", "USR3"};
#endif
static const uint8_t CV_SRC_SET[CV_SRC_N] = {0xFF, 0, 2, 3, SMP_NSETS, SMP_NSETS + 1, SMP_NSETS + 2};   /* SMP_ALL index */
#define CV_NZB ((uint32_t)NELEM(SMP_ZONES))  /* the built-in zones */
enum { CV_LAY_16, CV_LAY_27, CV_LAY_8, CV_LAY_N };
/* FELUCCA_CANVAS_KEYS (build.py): -1 every layout (KEYS picks; the default); 0 / 1 / 2 a firmware of that layout only
 * (16+FX, 27, 8+CTL): KEYS fixed to it, every preset in it */
#ifndef FELUCCA_CANVAS_KEYS
#define FELUCCA_CANVAS_KEYS (-1)
#endif
#if FELUCCA_CANVAS_KEYS >= 3
#error "FELUCCA_CANVAS_KEYS: -1 (all), 0 (16+FX), 1 (27) or 2 (8+CTL)"
#endif
#define CV_LAYP(x) (FELUCCA_CANVAS_KEYS >= 0 ? FELUCCA_CANVAS_KEYS : (x))   /* a preset's layout in this build */
static const char *const N_CV_LAY[CV_LAY_N] = {"16+FX", "27", "8+CTL"};
enum { CV_LATCH, CV_HOLD };
static const char *const N_CV_MODE[2] = {"LATCH", "HOLD"};

/* the glitches, black keys 1..11 (F#3 .. F#5) */
enum { CV_GL_R16, CV_GL_R32, CV_GL_REV, CV_GL_FRZ, CV_GL_TAPE, CV_GL_SCAT, CV_GL_JUMP, CV_GL_BURST, CV_GL_CRUSH,
       CV_GL_OUP, CV_GL_ODN, CV_GL_N };
static const char *const N_CV_GL[CV_GL_N] = {"RPT16", "RPT32", "REVRS", "FREEZ", "TAPE", "SCATR", "JUMP", "BURST",
                                             "CRUSH", "OCT+", "OCT-"};
/* perform.c's effect of a glitch (PF_* numbers, checked in ui_canvas.c), 0xFF = this engine's own */
static const uint8_t CV_GL_PF[CV_GL_N] = {1, 2, 3, 7, 6, 0xFF, 0xFF, 0xFF, 0xFF, 8, 9};
static void perf_press(uint32_t e, int down);          /* perform.c */
/* the controls, white keys 9..16 of 8+CTL */
enum { CV_CT_OFF, CV_CT_RAND, CV_CT_SCAN, CV_CT_SMALL, CV_CT_LARGE, CV_CT_DOWN, CV_CT_UP, CV_CT_LATCH, CV_CT_N };
static const char *const N_CV_CT[CV_CT_N] = {"ALL OFF", "RANDOM", "SCAN", "SMALL", "LARGE", "PIT-", "PIT+", "LATCH"};

/* what key k is in a layout */
enum { CV_R_NONE, CV_R_GRAIN, CV_R_GLITCH, CV_R_CTL };
static const int8_t CV_WHITE[12] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6};    /* from F: white place in the octave */
static const int8_t CV_BLACK[12] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, 4, -1};  /* .. black place */
static uint32_t cv_role(uint32_t lay, uint32_t k, uint32_t *idx)
{
    int32_t w = CV_WHITE[k % 12u], b = CV_BLACK[k % 12u];
    uint32_t o = k / 12u;
    *idx = 0;
    if (k >= CV_KEYS)
        return CV_R_NONE;
    if (lay == CV_LAY_27) {
        *idx = k;
        return CV_R_GRAIN;
    }
    if (b >= 0) {
        *idx = o * 5u + (uint32_t)b;
        return *idx < CV_GL_N ? CV_R_GLITCH : CV_R_NONE;
    }
    *idx = o * 7u + (uint32_t)w;
    if (lay == CV_LAY_8 && *idx >= 8u) {
        *idx -= 8u;
        return CV_R_CTL;
    }
    return CV_R_GRAIN;
}
/* the key of a note (53..79; other notes folded into it) */
static uint32_t cv_key_of(uint32_t note) { return (uint32_t)(((int32_t)note - CV_NOTE0) % 27 + 27) % 27u; }
/* the grains a layout has */
static uint32_t cv_ngrains(uint32_t lay) { return lay == CV_LAY_27 ? 27u : lay == CV_LAY_8 ? 8u : 16u; }

/* the grains' settings: per track and grain, POS SIZE DENS PTCH+24 (each 0..127) */
enum { CV_POS, CV_SIZE, CV_DENS, CV_PTCH, CV_NV };
static uint8_t cv_slot[NTRK][CV_KEYS][CV_NV] __attribute__((section(".pool")));
static uint32_t cv_tr(const track_t *t) { return (uint32_t)(t - trk) % NTRK; }
static int32_t cv_get(const track_t *t, uint32_t g, uint32_t v)
{
    int32_t x = cv_slot[cv_tr(t)][g % CV_KEYS][v % CV_NV];
    return v == CV_PTCH ? x - 24 : x;
}
static void cv_set(const track_t *t, uint32_t g, uint32_t v, int32_t x)
{
    x = v == CV_PTCH ? clamp(x, -24, 24) + 24 : clamp(x, 0, 127);
    cv_slot[cv_tr(t)][g % CV_KEYS][v % CV_NV] = (uint8_t)x;
}

typedef struct {
    const smp_zone_t *z;
    uint32_t pos;                /* the next sample to decode */
    int32_t frac;                /* Q16 between a and b */
    uint32_t step;               /* Q16 source samples per output sample */
    uint32_t wph, winc;          /* window phase: one turn = the grain */
    uint32_t left;               /* output samples to go */
    int32_t a, b;                /* the two source samples around the read position */
    int32_t pred, idx;           /* ADPCM state */
    int32_t gain;                /* Q15 */
    uint8_t on;                  /* 1 = sounding */
    uint8_t g;                   /* its grain key */
} cv_grain_t;

typedef struct {
    cv_grain_t gr[CV_NG];
    int16_t ipred[CV_NIDX];      /* seek index: the decoder state before sample k * GR_SEG of a zone */
    uint8_t iidx[CV_NIDX];
    const smp_zone_t *zp[CV_MAXZ];
    uint16_t zbase[CV_MAXZ], zcnt[CV_MAXZ], zdone[CV_MAXZ];
    uint32_t stamp;              /* what the index was built for (user slots: smp_user_gen) */
    uint8_t src;                 /* SRC + 1 the index holds, 0 = none */
    uint8_t nz;                  /* 0: no material (silent) */
    uint32_t latched;            /* bit g: grain g latched on */
    uint32_t held;               /* bit g: grain g's key held (HOLD), this block */
    uint32_t glitch, glprev;     /* bit i: glitch i held, this block / the block before */
    uint32_t ctl;                /* bit i: control i held */
    int32_t cnt[CV_KEYS];        /* samples to a grain's next grain */
    int32_t gain[CV_KEYS];       /* a grain's stream level, Q15 */
    int32_t scan;                /* SCAN's drift, POS units Q8 */
    int32_t y, ch, cn;           /* TONE's state; CRUSH's held sample and count */
    uint32_t pressed;            /* + 1 a key press (the page follows it) */
    uint8_t last;                /* the key pressed last */
} cv_part_t;
ENG_MEM_FITS("CANVAS", sizeof(cv_part_t));
static int32_t cv_rng[NPART];

static uint32_t cv_partno(const track_t *t) { return (uint32_t)(t - trk) % NPART; }
static cv_part_t *cv_of(const track_t *t) { return (cv_part_t *)ENG_MEM(cv_partno(t)); }
static inline uint32_t cv_rnd(const track_t *t) { return noise32(&cv_rng[cv_partno(t)]); }
static uint32_t cv_src(const int16_t *p) { return (uint32_t)clamp(p[P_E0], 0, CV_SRC_N - 1); }
static uint32_t cv_lay(const int16_t *p)
{
    return FELUCCA_CANVAS_KEYS >= 0 ? (uint32_t)FELUCCA_CANVAS_KEYS : (uint32_t)clamp(p[P_E1], 0, CV_LAY_N - 1);
}

/* the material SRC s has: how many zones, zone k; 0 zones = none (an empty user slot, a build without the samples) */
static uint32_t cv_nzones(uint32_t s)
{
    uint32_t si;
    if (s == CV_SRC_MIX)
        return CV_NZB < 8u ? CV_NZB : 8u;
    si = CV_SRC_SET[s % CV_SRC_N];
    if (smp_set_missing(si))
        return 0;
    si = si < SMP_NSETS ? SMP_SETS[si].nz : usr_nz[(si - SMP_NSETS) % SMP_USER_SLOTS];
    return si < CV_MAXZ ? si : CV_MAXZ;
}
static const smp_zone_t *cv_zone_of(uint32_t s, uint32_t k)
{
    uint32_t si;
    if (s == CV_SRC_MIX)                                /* spread over every built-in zone */
        return &SMP_ZONES[CV_NZB > 8u ? k * CV_NZB / 8u : k];
    si = CV_SRC_SET[s % CV_SRC_N];
    return si < SMP_NSETS ? &SMP_ZONES[SMP_SETS[si].z0 + k] : &usr_zone[(si - SMP_NSETS) % SMP_USER_SLOTS][k & 15u];
}
static uint32_t cv_stamp(uint32_t s) { return s >= CV_SRC_USR1 ? smp_user_gen : 0u; }

static void cv_reset(cv_part_t *P, uint32_t s)
{
    uint32_t i, tot = 0, nz = cv_nzones(s);
    P->src = (uint8_t)(s + 1u);
    P->stamp = cv_stamp(s);
    P->nz = 0;
    for (i = 0; i < CV_NG; i++)
        P->gr[i].on = 0;
    for (i = 0; i < nz; i++) {
        const smp_zone_t *z = cv_zone_of(s, i);
        uint32_t c = z->n >= 64u ? (z->n + GR_SEG - 1u) >> GR_SEG_LOG2 : 0u;
        if (tot + c > CV_NIDX)
            c = 0;                                      /* past the capacity: that zone stays silent */
        P->zp[i] = z;
        P->zbase[i] = (uint16_t)tot;
        P->zcnt[i] = (uint16_t)c;
        P->zdone[i] = c ? 1u : 0u;
        if (c) {
            P->ipred[tot] = 0;                          /* a zone starts at the state (0, 0) */
            P->iidx[tot] = 0;
        }
        tot += c;
    }
    P->nz = (uint8_t)nz;
}

/* POS (Q8, wrapping over 0..128) -> the zone and the sample there */
static uint32_t cv_place(const cv_part_t *P, int32_t posq8, uint32_t *zl)
{
    uint32_t u = (uint32_t)(((posq8 % (128 << 8)) + (128 << 8)) % (128 << 8)), x = u * P->nz;   /* 0 .. nz * 128 << 8 */
    *zl = (x >> 15) % P->nz;
    return (uint32_t)(((uint64_t)(x & 0x7FFFu) * P->zp[*zl]->n) >> 15);
}

/* one index entry: the zone the sounding grains need first (the least built of them), then the first not done */
static void cv_build(cv_part_t *P, const track_t *t)
{
    uint32_t i, zl = CV_MAXZ, k, e, pos, on = P->latched | P->held;
    int32_t pred, idx;
    for (i = 0; i < CV_KEYS && P->nz; i++) {
        uint32_t z;
        if (!((on >> i) & 1u))
            continue;
        cv_place(P, (cv_get(t, i, CV_POS) << 8) + P->scan, &z);
        if (P->zdone[z] < P->zcnt[z] && (zl == CV_MAXZ || P->zdone[z] < P->zdone[zl]))
            zl = z;
    }
    for (i = 0; i < P->nz && zl == CV_MAXZ; i++)
        if (P->zdone[i] < P->zcnt[i])
            zl = i;
    if (zl == CV_MAXZ)
        return;
    k = P->zdone[zl];
    e = P->zbase[zl] + k - 1u;
    pred = P->ipred[e];
    idx = P->iidx[e];
    pos = (k - 1u) << GR_SEG_LOG2;
    for (i = 0; i < GR_SEG; i++)
        gr_dec(P->zp[zl], pos + i, &pred, &idx);
    P->ipred[e + 1u] = (int16_t)pred;
    P->iidx[e + 1u] = (uint8_t)idx;
    P->zdone[zl] = (uint16_t)(k + 1u);
}

/* start a grain of grain key g; 0 = none could start yet (its place not indexed: again soon) */
static int cv_spawn(cv_part_t *P, const track_t *t, uint32_t g, uint32_t iv, int32_t lfo)
{
    const int16_t *p = t->p;
    cv_grain_t *gr = 0;
    const smp_zone_t *z;
    uint32_t i, zl, n, len, step, span, load, ov, r1 = cv_rnd(t), r2 = cv_rnd(t), pos, lim;
    int32_t d16, posq, sz = cv_get(t, g, CV_SIZE);
    for (i = 0; i < CV_NG && !gr; i++)
        if (!P->gr[i].on)
            gr = &P->gr[i];
    if (!gr)
        return 1;
    /* place: POS + SCAN's drift + LFO -> SHP (+-64), SCATR anywhere, SPRY's spread forward (127: ~a quarter of POS) */
    posq = (cv_get(t, g, CV_POS) << 8) + P->scan + ((lfo * p[P_LD_SHP]) >> 7);
    if ((P->glitch >> CV_GL_SCAT) & 1u)
        posq = (int32_t)(r1 & 0x7FFFu);
    posq += (int32_t)(((r2 & 0xFFFFu) * (uint32_t)p[P_E3]) >> 10);
    pos = cv_place(P, posq, &zl);
    z = P->zp[zl];
    n = z->n;
    if (n < 64u || !P->zcnt[zl])
        return 1;
    /* size: 441 * 2^(SIZE / 21) output samples (10 ms .. ~0.66 s); BURST / SMALL a quarter, LARGE three times */
    len = (441u * pow2_q16(sz * 64 / 7)) >> 16;
    if (((P->glitch >> CV_GL_BURST) & 1u) || ((P->ctl >> CV_CT_SMALL) & 1u))
        len >>= 2;
    if ((P->ctl >> CV_CT_LARGE) & 1u)
        len *= 3u;
    /* pitch: PTCH, PIT- / PIT+, JUMP's leap, RAND's detune, LFO -> PIT; at the zone's own speed */
    d16 = cv_get(t, g, CV_PTCH) * 16 + ((lfo * p[P_LD_PIT] * 3) >> 15);
    if ((P->ctl >> CV_CT_DOWN) & 1u)
        d16 -= 192;
    if ((P->ctl >> CV_CT_UP) & 1u)
        d16 += 192;
    if ((P->glitch >> CV_GL_JUMP) & 1u) {
        static const int16_t LEAP[4] = {-192, -112, 112, 192};
        d16 += LEAP[(r1 >> 20) & 3u];
    }
    {
        int32_t amt = p[P_E5] * p[P_E5] * 192 / (127 * 127);
        d16 += (((int32_t)(r2 >> 16)) - 32768) * amt >> 15;
    }
    step = (pow2_q16(clamp(d16, -1536, 576)) >> 8) * (z->rate >> 8);
    step = step > GR_STEP_MAX ? GR_STEP_MAX : step < 256u ? 256u : step;
    span = (len * (step >> 8)) >> 8;                    /* source samples the grain reads */
    if (span + 4u > n / 2u) {                           /* at most half the zone */
        span = n / 2u - 4u;
        len = (span << 8) / (step >> 8);
    }
    if (len < 16u)
        return 1;
    if (pos > n - 3u - span)                            /* fitted into the zone */
        pos = n - 3u - span;
    lim = (uint32_t)P->zdone[zl] << GR_SEG_LOG2;        /* (where the index has got to: a grain starts there or before) */
    if (pos >= lim) {
        if (lim < 2u * GR_SEG && pos > 4u * GR_SEG)
            return 0;                                   /* nowhere near yet: again soon */
        pos = lim - 1u;
    }
    for (i = 0, load = step; i < CV_NG; i++)            /* the part's decode load: a cap on the cost */
        if (P->gr[i].on)
            load += P->gr[i].step;
    if (load > GR_LOAD_MAX)
        return 1;
    gr->z = z;
    gr->step = step;
    gr->frac = 0;
    gr->wph = 0;
    gr->winc = 0xFFFFFFFFu / len;
    gr->left = len;
    gr->g = (uint8_t)g;
    ov = len / (iv ? iv : 1u);
    gr->gain = GR_NORM[ov < 1u ? 1u : ov > GR_NG ? GR_NG : ov];
    {                                                   /* the decoder at pos: the entry below, then up to 255 */
        uint32_t e = P->zbase[zl] + (pos >> GR_SEG_LOG2), q;
        int32_t pr = P->ipred[e], ix = P->iidx[e];
        for (q = pos & ~(GR_SEG - 1u); q < pos; q++)
            gr_dec(z, q, &pr, &ix);
        gr->pred = pr;
        gr->idx = ix;
    }
    gr->a = gr_dec(z, pos, &gr->pred, &gr->idx);
    gr->b = gr_dec(z, pos + 1u, &gr->pred, &gr->idx);
    gr->pos = pos + 2u;
    gr->on = 1;
    return 1;
}

/* one grain into acc, its window times its stream's level (g0 -> g1 over the block); 0 when it ended */
static int cv_run(cv_grain_t *g, int32_t *acc, uint32_t n, int32_t g0, int32_t g1)
{
    uint32_t i, m = n < g->left ? n : g->left, step = g->step, pos = g->pos;
    int32_t a = g->a, b = g->b, frac = g->frac, w0, w1, wq, dq, pred = g->pred, idx = g->idx;
    const smp_zone_t *z = g->z;
    w0 = mulq15(mulq15(gr_win(g->wph), g->gain), g0);
    g->wph += g->winc * m;
    g->left -= m;
    w1 = g->left ? mulq15(mulq15(gr_win(g->wph), g->gain), g0 + (g1 - g0) * (int32_t)m / (int32_t)n) : 0;
    wq = w0 * 1024;
    dq = m == CTL ? ((w1 - w0) * 1024) >> CTL_LOG2 : m ? (w1 - w0) * 1024 / (int32_t)m : 0;
    for (i = 0; i < m; i++) {
        int32_t s = a + (((b - a) * (frac >> 1)) >> 15);
        acc[i] += (s * (wq >> 10)) >> 15;
        wq += dq;
        frac += (int32_t)step;
        while (frac >= 65536) {
            frac -= 65536;
            a = b;
            b = pos < z->n ? gr_dec(z, pos++, &pred, &idx) : a;
        }
    }
    g->pred = pred;
    g->idx = idx;
    g->pos = pos;
    g->a = a;
    g->b = b;
    g->frac = frac;
    if (!g->left)
        g->on = 0;
    return g->left != 0;
}

/* a key's press (a voice starting on its note): a grain toggles (LATCH), a control acts; the page follows it */
static void canvas_note_on(track_t *t, voice_t *v)
{
    cv_part_t *P = cv_of(t);
    uint32_t k = cv_key_of(v->note), idx, role = cv_role(cv_lay(t->p), k, &idx), g;
    P->last = (uint8_t)k;
    P->pressed++;
    if (role == CV_R_GRAIN && t->p[P_E2] == CV_LATCH) {
        P->latched ^= 1u << idx;
        if ((P->latched >> idx) & 1u)
            P->cnt[idx] = 0;                            /* its first grain at once */
    } else if (role == CV_R_CTL) {
        if (idx == CV_CT_OFF) {
            P->latched = 0;
        } else if (idx == CV_CT_RAND) {                 /* the sounding grains to new places */
            for (g = 0; g < CV_KEYS; g++)
                if (((P->latched | P->held) >> g) & 1u)
                    cv_set(t, g, CV_POS, (int32_t)(cv_rnd(t) & 127u));
        } else if (idx == CV_CT_LATCH) {
            t->p[P_E2] = t->p[P_E2] == CV_LATCH ? CV_HOLD : CV_LATCH;
            if (t->p[P_E2] == CV_HOLD)
                P->latched = 0;
        }
    }
}

/* a key's voice lasts while it is held, except a tap that only toggles or acts */
static int canvas_done(track_t *t, voice_t *v)
{
    uint32_t idx, role = cv_role(cv_lay(t->p), cv_key_of(v->note), &idx);
    if (role == CV_R_NONE || (role == CV_R_GRAIN && t->p[P_E2] == CV_LATCH))
        return 1;
    if (role == CV_R_CTL && (idx == CV_CT_OFF || idx == CV_CT_RAND || idx == CV_CT_LATCH))
        return 1;
    return !v->gate;
}
static void canvas_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;     /* (the streams render in canvas_post) */
}

/* per part and block, before the voices: the material follows SRC, the held keys, the master glitches, an index entry */
static void canvas_block(track_t *t)
{
    cv_part_t *P = cv_of(t);
    uint32_t s = cv_src(t->p), i, lay = cv_lay(t->p), held = 0, gl = 0, ct = 0, ng = cv_ngrains(lay);
    if (!cv_rng[cv_partno(t)])
        cv_rng[cv_partno(t)] = 0x6C8E9CF5;
    if (P->src != s + 1u || P->stamp != cv_stamp(s))
        cv_reset(P, s);
    for (i = 0; i < NVOICE; i++) {                       /* the keys held now */
        const voice_t *v = &t->v[i];
        uint32_t idx, role;
        if (!v->active || !v->gate)
            continue;
        role = cv_role(lay, cv_key_of(v->note), &idx);
        if (role == CV_R_GRAIN && t->p[P_E2] == CV_HOLD)
            held |= 1u << idx;
        else if (role == CV_R_GLITCH)
            gl |= 1u << idx;
        else if (role == CV_R_CTL)
            ct |= 1u << idx;
    }
    for (i = 0; i < CV_KEYS; i++)
        if (((held & ~P->held) >> i) & 1u)
            P->cnt[i] = 0;                              /* a key held: its first grain at once */
    P->held = held;
    P->latched &= (ng < 32u ? (1u << ng) - 1u : ~0u);   /* (a layout with fewer grains: the others off) */
    P->ctl = ct;
    P->glitch = gl;
    for (i = 0; i < CV_GL_N; i++)                         /* the master glitches: pressed / let go (perform.c) */
        if (CV_GL_PF[i] != 0xFFu && (((gl ^ P->glprev) >> i) & 1u))
            perf_press(CV_GL_PF[i], (int)((gl >> i) & 1u));
    P->glprev = gl;
    if ((ct >> CV_CT_SCAN) & 1u)                         /* SCAN: the whole material in ~4 s (6 / 32768 a block) */
        P->scan = (P->scan + 6) % (128 << 8);
    cv_build(P, t);
}

/* per part and block, after the voices: the streams */
static uint32_t canvas_post(track_t *t, int32_t *out, uint32_t n)
{
    cv_part_t *P = cv_of(t);
    const int16_t *p = t->p;
    uint32_t i, g, on = P->latched | P->held, nsound = 0, ngr = 0, ng = cv_ngrains(cv_lay(p));
    int32_t acc[CTL], g0[CV_KEYS], lfo = mulq15(t->lfo_val, t->lfo_fade), fstep, lp, norm, y = P->y;
    if (n > CTL)
        n = CTL;
    if (!P->nz || P->src != cv_src(p) + 1u)
        return 0;                                       /* no material (an empty user slot): silent */
    /* the streams' levels: towards 1 (on) or 0 over FADE (3 ms .. ~1 s) */
    fstep = 32768 / (1 + p[P_E4] * p[P_E4] / 12);
    for (g = 0; g < ng; g++) {
        int32_t to = ((on >> g) & 1u) ? 32768 : 0, x = P->gain[g];
        g0[g] = x;
        x += to > x ? (to - x < fstep ? to - x : fstep) : -(x - to < fstep ? x - to : fstep);
        P->gain[g] = x;
        nsound += x > 0 || g0[g] > 0;
    }
    for (g = ng; g < CV_KEYS; g++)
        g0[g] = P->gain[g] = 0;
    for (i = 0; i < n; i++)
        acc[i] = 0;
    for (i = 0; i < CV_NG; i++)                           /* the grains sounding */
        if (P->gr[i].on) {
            uint32_t k = P->gr[i].g % CV_KEYS;
            cv_run(&P->gr[i], acc, n, g0[k], P->gain[k]);
            ngr++;
        }
    for (g = 0; g < ng; g++) {                           /* the next grain of each stream: DENS a second, 1..98 /s */
        uint32_t iv;
        if (!((on >> g) & 1u) && P->gain[g] <= 0)
            continue;
        iv = (44100u * pow2_q16(-cv_get(t, g, CV_DENS) * 10)) >> 16;
        if ((P->glitch >> CV_GL_BURST) & 1u)
            iv >>= 2;
        P->cnt[g] -= (int32_t)n;
        if (P->cnt[g] > 0 || !((on >> g) & 1u))
            continue;
        if (cv_spawn(P, t, g, iv, lfo)) {
            int32_t j = (((int32_t)(cv_rnd(t) & 0xFFFFu) - 32768) * p[P_E3]) / 254;   /* +-SPRY / 2, Q15 */
            P->cnt[g] += (int32_t)iv + mulq15((int32_t)iv, j);
            if (P->cnt[g] < 0)
                P->cnt[g] = 0;
        } else {
            P->cnt[g] = 4 * CTL;                        /* its place not indexed yet: again in four blocks */
        }
    }
    if (!ngr && !nsound && !y)
        return 0;
    /* the streams together: 1 / sqrt(how many sound); TONE (+ FLT modulation); CRUSH */
    norm = GR_NORM[nsound < 1u ? 1u : nsound > GR_NG ? GR_NG : nsound];
    lp = 4000 + ((clamp((p[P_E6] << 8) + ((lfo * p[P_LD_FLT]) >> 7), 0, 127 << 8) * 28767) >> 15);
    for (i = 0; i < n; i++) {
        int32_t s = mulq15(clamp(acc[i], -65535, 65535), norm);
        y += mulq15(s - y, lp);
        s = y;
        if ((P->glitch >> CV_GL_CRUSH) & 1u) {          /* CRUSH: hold 2..16 samples, 12..4 bits */
            int32_t hold = 2 + p[P_E7] * 14 / 127, bits = 12 - p[P_E7] * 8 / 127;
            if (--P->cn <= 0) {
                P->cn = hold;
                P->ch = (s >> (16 - bits)) << (16 - bits);
            }
            s = P->ch;
        }
        out[i] += (s * 26000) >> 14;
    }
    P->y = (y > -2 && y < 2) ? 0 : y;
    return ngr ? (ngr + 3u) / 4u : 1u;
}

#if FELUCCA_CANVAS_KEYS >= 0
/* the granular-only firmware's factory sounds: each a recipe for its grains (spread over the grains the layout has, so
 * 8, 16 or 27 keys cover the same sound) and its movement (the LFO, an INSERT). The material's four sounds are a
 * quarter of POS each: NATURE RAIN CREEK BIRDS NIGHT, DREAM GLASS DRIFT VOX LIQUID, GLITCH DATA CIRCUIT TAPE STATIC */
enum { CV_SC_NONE, CV_SC_PENTA, CV_SC_MINP, CV_SC_CHORD, CV_SC_OCT, CV_SC_FIFTH, CV_SC_WILD, CV_SC_N };
static const int8_t CV_SCALE[CV_SC_N][6] = {
    {0, 0, 0, 0, 0, 0}, {0, 2, 4, 7, 9, 12}, {0, 3, 5, 7, 10, 12}, {0, 4, 7, 11, 14, 19},
    {0, 12, -12, 0, 12, 24}, {0, 7, 12, 19, 24, -5}, {0, 6, -7, 13, 1, -11}};
typedef struct {
    uint8_t p0, p1;              /* POS: the first grain's, the last grain's (the rest between, in order) */
    uint8_t size, dens;          /* their middle values */
    uint8_t vsize, vdens, vpos;  /* how much each grain differs (a fixed scatter, every grain its own) */
    uint8_t scale;               /* CV_SC_*: the grains' pitches, low to high, through the scale */
    int8_t base;                 /* the lowest grain's pitch (semitones) */
    uint8_t lrate, lwave;        /* the LFO: RATE (0..127), WAVE (SIN TRI SAW SQR S&H) */
    int8_t lshp, lpit, lflt;     /* its depth to the place, the pitch, the TONE */
    uint8_t ins, ia;             /* INSERT type (OFF SOFT HARD FOLD FUZZ CRUSH PHASR FLANG CHOR) and its A */
} cv_recipe_t;
static const cv_recipe_t CV_RECIPE[] = {
    /*  p0   p1 size dens vs vd vp scale         base lrate lw shp pit flt  ins ia */
    {   2,  30, 96, 96, 20, 20, 6, CV_SC_NONE,   -5,  20, 0, 14,  0, 10,  0,  0},  /* RAINFALL */
    {  34,  62, 72, 108, 30, 14, 4, CV_SC_PENTA,  -7,  50, 4, 22,  0,  0,  0,  0},  /* CREEK */
    {  66,  94, 60, 72, 36, 30, 3, CV_SC_PENTA,   0,  40, 1,  8,  4,  0,  0,  0},  /* DAWN SONG */
    {  98, 126, 92, 84, 18, 24, 5, CV_SC_OCT,    -12, 30, 0, 10,  0, 16,  0,  0},  /* NIGHT POND */
    {   4, 124, 104, 92, 16, 16, 8, CV_SC_FIFTH, -12, 24, 2, 30,  0,  8,  0,  0},  /* WILDWOOD */
    {   8,  30, 110, 100, 12, 12, 3, CV_SC_PENTA, -12, 34, 0,  6,  2, 18,  8, 50},  /* GLASS GDN */
    {  36,  60, 122, 86, 4, 16, 4, CV_SC_CHORD,  -12, 20, 0, 18,  0, 24,  0,  0},  /* SLOW BLOOM */
    {  66,  94, 106, 96, 10, 10, 3, CV_SC_CHORD, -12, 88, 1,  4,  3,  0,  7, 40},  /* CHOIR */
    {  98, 126, 80, 100, 24, 18, 4, CV_SC_MINP,   -5, 40, 0, 20,  0, 30,  6, 60},  /* MELT */
    {   0, 127, 116, 90, 10, 20, 12, CV_SC_PENTA, -12, 60, 4, 26, 2, 12,  0,  0},  /* DREAMSCAPE */
    {   1,  31, 20, 120, 16, 8, 10, CV_SC_OCT,   -12, 80, 4, 30,  0,  0,  5, 70},  /* DATA RAIN */
    {  33,  63, 34, 110, 24, 16, 8, CV_SC_WILD,    0, 80, 3,  0,  6,  0,  3, 60},  /* BENT */
    {  66,  94, 90, 92, 14, 14, 3, CV_SC_CHORD,  -12, 50, 0,  6,  5,  0,  8, 70},  /* WOW+FLUTR */
    {  96, 127, 70, 92, 30, 20, 10, CV_SC_FIFTH, -12, 70, 4, 26,  0, 20,  4, 50},  /* SHORTWAVE */
    {   0, 127, 14, 124, 10, 6, 20, CV_SC_WILD,    0, 90, 4, 40,  8,  0,  5, 100}, /* MELTDOWN */
    {   0, 127, 60, 114, 40, 20, 14, CV_SC_MINP, -12, 60, 4, 40,  0, 20,  0,  0},  /* STORM */
    {  10, 118, 100, 96, 20, 16, 6, CV_SC_FIFTH,  0, 30, 1, 12,  2, 14,  8, 60},  /* BIOLUMIN */
};
static void canvas_load_layout(const track_t *t, uint32_t pi)
{
    const cv_recipe_t *r = &CV_RECIPE[pi % NELEM(CV_RECIPE)];
    uint32_t g, ng = cv_ngrains(cv_lay(t->p));
    for (g = 0; g < CV_KEYS; g++) {
        uint32_t gg = g % ng, h = ((gg + pi * 7u) * 2654435761u) >> 24;      /* 0..255, a fixed scatter */
        int32_t sc = (int32_t)(h & 63u) - 32;                             /* -32..31 */
        int32_t pos = (int32_t)r->p0 + (int32_t)((r->p1 - r->p0) * gg / (ng > 1u ? ng - 1u : 1u));
        uint32_t step = gg * 6u * (ng > 8u ? 2u : 3u) / ng;               /* through the scale: 2 or 3 octaves */
        int32_t ptch = r->base + CV_SCALE[r->scale][step % 6u] + 12 * (int32_t)(step / 6u) * (r->scale != CV_SC_NONE);
        pos += sc * r->vpos / 32;
        cv_set(t, g, CV_POS, pos);
        cv_set(t, g, CV_SIZE, (int32_t)r->size + sc * r->vsize / 32);
        cv_set(t, g, CV_DENS, (int32_t)r->dens + ((int32_t)((h >> 2) & 63u) - 32) * r->vdens / 32);
        cv_set(t, g, CV_PTCH, r->scale == CV_SC_NONE ? r->base * (gg < ng / 3u) : ptch);
    }
}
/* .. and its movement (a preset load only: a project or user preset brings its own) */
static void canvas_load_sound(track_t *t, uint32_t pi)
{
    const cv_recipe_t *r = &CV_RECIPE[pi % NELEM(CV_RECIPE)];
    t->p[P_LRATE] = r->lrate;
    t->p[P_LWAVE] = r->lwave;
    t->p[P_LD_SHP] = r->lshp;
    t->p[P_LD_PIT] = r->lpit;
    t->p[P_LD_FLT] = r->lflt;
    t->p[P_ITYPE] = r->ins;
    if (r->ins) {
        t->p[P_IA] = r->ia;
        t->p[P_IMIX] = 70;
    }
}
#else
/* the factory presets' grains (CV_PRESET: the shape; the others by a formula, every grain different) */
static const uint8_t CV_MAJ[7] = {0, 2, 4, 5, 7, 9, 11};
static void canvas_load_layout(const track_t *t, uint32_t pi)
{
    uint32_t g;
    for (g = 0; g < CV_KEYS; g++) {
        uint32_t h = (g * 2654435761u) >> 25;           /* 0..127, a fixed scatter */
        int32_t pos = (int32_t)(g * 127u / (CV_KEYS - 1u)), size = 70, dens = 80, ptch = 0;
        switch (pi % 6u) {
        case 0:                                         /* SPREAD: the material across the keys */
            break;
        case 1:                                         /* CLOUDS: long and dense, upper keys an octave up */
            size = 100, dens = 100, ptch = g >= 8u ? 12 : 0;
            break;
        case 2:                                         /* CHORDS: one place, the keys a major scale */
            pos = 30 + (int32_t)(g % 3u) * 2, size = 90, dens = 90, ptch = (int32_t)CV_MAJ[g % 7u] + 12 * (int32_t)(g / 7u) - 12;
            break;
        case 3:                                         /* GLITCH: short and fast, scattered, octaves */
            pos = (int32_t)h, size = 18, dens = 118, ptch = (int32_t)(h % 3u) * 12 - 12;
            break;
        case 4:                                         /* DRONE: very long, low, slow */
            pos = (int32_t)(g * 5u % 128u), size = 122, dens = 70, ptch = g % 3u == 0u ? -12 : g % 3u == 1u ? 0 : 7;
            break;
        default:                                        /* PERC: short grains from the starts of things */
            pos = (int32_t)((g * 37u) % 128u) & ~15, size = 28, dens = 30, ptch = 0;
            break;
        }
        cv_set(t, g, CV_POS, pos);
        cv_set(t, g, CV_SIZE, size);
        cv_set(t, g, CV_DENS, dens);
        cv_set(t, g, CV_PTCH, ptch);
    }
}
static void canvas_load_sound(track_t *t, uint32_t pi) { (void)t, (void)pi; }

#endif

/* the grains of track tr in an FM6 patch area (FM6_PACKED 7-bit bytes: projects, user presets): bytes 0..107 the 27 x 4
 * values, 120 121 the mark, 122 the version (2) */
#define CV_MARK0 0x43u           /* 'C' */
#define CV_MARK1 0x56u           /* 'V' */
#define CV_PKV 2u
static void canvas_pack(uint32_t tr, uint8_t *pk)
{
    uint32_t i;
    for (i = 0; i < FM6_PACKED; i++)
        pk[i] = 0;
    for (i = 0; i < CV_KEYS * CV_NV; i++)
        pk[i] = cv_slot[tr % NTRK][i / CV_NV][i % CV_NV] & 127u;
    pk[120] = CV_MARK0;
    pk[121] = CV_MARK1;
    pk[122] = CV_PKV;
}
/* .. and back: 1 = it held grains (else track tr's are left as they are) */
static int canvas_unpack(uint32_t tr, const uint8_t *pk)
{
    uint32_t i;
    if (pk[120] != CV_MARK0 || pk[121] != CV_MARK1 || pk[122] != CV_PKV)
        return 0;
    for (i = 0; i < CV_KEYS * CV_NV; i++)
        cv_slot[tr % NTRK][i / CV_NV][i % CV_NV] = (uint8_t)(i % CV_NV == CV_PTCH ? clamp(pk[i], 0, 48) : pk[i] & 127u);
    return 1;
}
_Static_assert(CV_KEYS * CV_NV <= 120u && FM6_PACKED >= 123u, "the grains in an FM6 patch area");

/* the keys play their own notes: key k = note 53 + k (no scale, no octave) */
static int32_t canvas_keys(const track_t *t, uint32_t k) { (void)t; return CV_NOTE0 + (int32_t)k; }

#if FELUCCA_CANVAS_KEYS >= 0
#define CVP(n, src, mode, spry, fade, rnd, tone, crsh, d, c, dl, rv) \
    {n, {src, FELUCCA_CANVAS_KEYS, mode, spry, fade, rnd, tone, crsh}, {0, 0, 127, 0}, 0, 0, FX(d, c, dl, rv), PAT(0)}
static const preset_t CANVAS_PRESETS[] = {
    /* name, SRC, MODE, SPRY, FADE, RAND, TONE, CRSH, sends DIST CHORUS DELAY REVERB; the grains and the movement:
     * CV_RECIPE, in this order (category.c: their categories) */
    CVP("RAINFALL", CV_SRC_PIANO, CV_LATCH, 40, 90, 8, 104, 60, 0, 20, 10, 90),
    CVP("CREEK", CV_SRC_PIANO, CV_LATCH, 30, 70, 6, 112, 60, 0, 10, 30, 70),
    CVP("DAWN SONG", CV_SRC_PIANO, CV_LATCH, 46, 60, 10, 120, 60, 0, 20, 55, 80),
    CVP("NIGHT POND", CV_SRC_PIANO, CV_LATCH, 30, 100, 4, 96, 60, 0, 30, 25, 100),
    CVP("WILDWOOD", CV_SRC_PIANO, CV_LATCH, 60, 110, 12, 108, 60, 0, 40, 30, 110),
    CVP("GLASS GDN", CV_SRC_FLUTE, CV_LATCH, 24, 90, 5, 118, 60, 0, 50, 45, 110),
    CVP("SLOW BLOOM", CV_SRC_FLUTE, CV_LATCH, 36, 124, 6, 100, 60, 0, 60, 20, 120),
    CVP("CHOIR", CV_SRC_FLUTE, CV_LATCH, 20, 100, 4, 108, 60, 0, 40, 30, 110),
    CVP("MELT", CV_SRC_FLUTE, CV_LATCH, 44, 80, 16, 104, 60, 10, 50, 40, 90),
    CVP("DREAMSCAPE", CV_SRC_FLUTE, CV_LATCH, 70, 118, 20, 110, 60, 0, 60, 40, 124),
    CVP("DATA RAIN", CV_SRC_SAX, CV_LATCH, 70, 20, 30, 127, 100, 10, 10, 50, 50),
    CVP("BENT", CV_SRC_SAX, CV_LATCH, 50, 30, 24, 124, 90, 20, 20, 40, 40),
    CVP("WOW+FLUTR", CV_SRC_SAX, CV_LATCH, 24, 80, 8, 100, 60, 10, 50, 45, 80),
    CVP("SHORTWAVE", CV_SRC_SAX, CV_LATCH, 60, 50, 18, 116, 80, 15, 20, 60, 70),
    CVP("MELTDOWN", CV_SRC_SAX, CV_HOLD, 90, 8, 44, 127, 127, 30, 10, 60, 40),
    CVP("STORM", CV_SRC_MIX, CV_LATCH, 90, 60, 26, 116, 90, 10, 30, 50, 100),
    CVP("BIOLUMIN", CV_SRC_MIX, CV_LATCH, 40, 100, 10, 112, 60, 0, 70, 50, 120),
};
_Static_assert(NELEM(CANVAS_PRESETS) == NELEM(CV_RECIPE), "a recipe each");
#else
static const preset_t CANVAS_PRESETS[] = {
    /* name, {SRC, KEYS, MODE, SPRY, FADE, RAND, TONE, CRSH}, {A D S R}, fenv, mono; the grains: canvas_load_layout */
    {"SPREAD", {CV_SRC_MIX, CV_LAYP(CV_LAY_16), CV_LATCH, 20, 40, 6, 120, 60}, {0, 0, 127, 0}, 0, 0, FX(0, 30, 25, 80), PAT(0)},
    {"CLOUDS", {CV_SRC_MIX, CV_LAYP(CV_LAY_16), CV_LATCH, 40, 70, 14, 100, 60}, {0, 0, 127, 0}, 0, 0, FX(0, 50, 20, 100), PAT(0)},
    {"CHORDS", {CV_SRC_PIANO, CV_LAYP(CV_LAY_27), CV_LATCH, 12, 50, 4, 115, 60}, {0, 0, 127, 0}, 0, 0, FX(0, 40, 25, 90), PAT(0)},
    {"GLITCH", {CV_SRC_MIX, CV_LAYP(CV_LAY_8), CV_HOLD, 30, 6, 30, 127, 90}, {0, 0, 127, 0}, 0, 0, FX(10, 10, 45, 40), PAT(0)},
    {"DRONE", {CV_SRC_FLUTE, CV_LAYP(CV_LAY_16), CV_LATCH, 50, 110, 10, 90, 60}, {0, 0, 127, 0}, 0, 0, FX(0, 60, 20, 110), PAT(0)},
    {"PERC", {CV_SRC_SAX, CV_LAYP(CV_LAY_16), CV_HOLD, 6, 4, 0, 127, 70}, {0, 0, 127, 0}, 0, 0, FX(0, 15, 40, 50), PAT(0)},
};
#endif

static const engine_t ENG_CANVAS = {
    .name = "CANVAS",
    .page_title = {"GRAN", "PLAY"},
    .edit = {
        {"SRC", F_ENUM, 0, CV_SRC_N - 1, CV_SRC_MIX, N_CV_SRC, 0},
#if FELUCCA_CANVAS_KEYS >= 0
        {"KEYS", F_ENUM, FELUCCA_CANVAS_KEYS, FELUCCA_CANVAS_KEYS, FELUCCA_CANVAS_KEYS, N_CV_LAY, 0},   /* (fixed) */
#else
        {"KEYS", F_ENUM, 0, CV_LAY_N - 1, CV_LAY_16, N_CV_LAY, 0},
#endif
        {"MODE", F_ENUM, 0, 1, CV_LATCH, N_CV_MODE, 0},
        {"SPRY", F_PCT, 0, 127, 20, 0, 0},
        {"FADE", F_PCT, 0, 127, 40, 0, 0},
        {"RAND", F_PCT, 0, 127, 6, 0, 0},
        {"TONE", F_PCT, 0, 127, 120, 0, 0},
        {"CRSH", F_PCT, 0, 127, 60, 0, 0},
    },
    .presets = CANVAS_PRESETS,
    .npresets = NELEM(CANVAS_PRESETS),
    .note_on = canvas_note_on,
    .render = canvas_render,
    .knob = {P_E3, P_E4, P_E6, P_E0},
    .block = canvas_block,
    .keys = canvas_keys,
    .ownenv = 1,
    .done = canvas_done,
    .mem = sizeof(cv_part_t),
    .post = canvas_post,
};
