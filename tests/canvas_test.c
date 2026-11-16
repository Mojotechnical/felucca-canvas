/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; CANVAS: the Felucca Canvas fork */
/* CANVAS engine test (src/eng_canvas.c: the keyed granular instrument) through hostsim.c, as noise_test.c.
 *   build/host/canvas_test [DEMODIR]          (run_tests.sh: build/canvas_demo)
 * 1. the keys: every layout's roles (16+FX, 27, 8+CTL), notes folded onto the 27 keys.
 * 2. the grains' store: 27 x 4 values per track, packed into an FM6 patch area and back; a patch is no grains.
 * 3. every preset: three grains on sound (RMS), no clipping, no DC; off again: silence after FADE, no voice left.
 * 4. LATCH: a tap turns a grain on (its voice ends at once) and it sounds on; a tap again fades it out.
 *    HOLD: a grain sounds while its key is held and fades out after.
 * 5. a grain's own POS: every grain of it starts at its place (SPRY 0); PTCH +12 doubles the step; DENS more grains.
 * 6. glitches: SCATR spreads the places, JUMP leaps the pitch, BURST more and shorter grains, CRUSH holds samples;
 *    the master ones press and let go perform.c's effects (RPT16, FREEZ) with the key.
 * 7. controls (8+CTL): ALL OFF, RANDOM, SCAN, PIT+, LATCH.
 * 8. a fresh part (empty index): sound within 10 ms; SRC on an empty user slot: silent.
 * 9. determinism; the cost of 16 grains at their heaviest against GRAIN's heaviest preset.
 * 10. demos into DEMODIR: each preset performed (grains latched in turn, glitches held). */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define CANVAS_COST_RATIO 2.5      /* 16 streams at once against GRAIN's 3 voices */
#define ENGI_GRAIN_IDX 8u

static int bad;
static void check(const char *what, int ok)
{
    printf("canvas: %-84s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void setup(uint32_t pi)
{
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    eng_mem_owner[0] = 0;                              /* (a fresh part: its memory zeroed by the claim) */
    host_preset(&trk[0], ENGI_CANVAS, pi);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    perf_held = 0;
}
static uint32_t key_note(uint32_t k) { return 53u + k; }
/* the key of grain / glitch / control i in layout lay */
static uint32_t key_of(uint32_t lay, uint32_t role, uint32_t i)
{
    uint32_t k, idx;
    for (k = 0; k < CV_KEYS; k++)
        if (cv_role(lay, k, &idx) == role && idx == i)
            return k;
    return 0xFF;
}

/* track 0's sum for n samples (y may be 0): peak, rms, mean */
static void render(uint32_t n, double *y, double *peak, double *rms, double *mean)
{
    static int32_t b[CTL];
    uint32_t i, k;
    double pk = 0, s2 = 0, s1 = 0;
    for (i = 0; i < n; i += CTL) {
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++) {
            double v = b[k] / 32768.0;
            if (y) y[i + k] = v;
            pk = fabs(v) > pk ? fabs(v) : pk;
            s2 += v * v;
            s1 += v;
        }
    }
    if (peak) *peak = pk;
    if (rms) *rms = sqrt(s2 / n);
    if (mean) *mean = s1 / n;
}
static void tap(uint32_t k)
{
    trk_note_on(&trk[0], key_note(k), 100);
    render(CTL * 4u, 0, 0, 0, 0);
    trk_note_off(&trk[0], key_note(k));
    render(CTL * 4u, 0, 0, 0, 0);
}
static int voices_active(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[0].v[i].active;
    return (int)n;
}

/* the grains started over blocks: count, places (min / max), the steps' mean, lengths' mean */
typedef struct { uint32_t n, min, max, lenmin, lenmax; double step; } gstat_t;
static void watch(uint32_t blocks, gstat_t *st)
{
    static int32_t b[CTL];
    cv_part_t *P = cv_of(&trk[0]);
    uint32_t i, k;
    memset(st, 0, sizeof *st);
    st->min = st->lenmin = 0xFFFFFFFFu;
    for (i = 0; i < blocks; i++) {
        uint8_t before[CV_NG];
        for (k = 0; k < CV_NG; k++)
            before[k] = P->gr[k].on;
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CV_NG; k++) {
            cv_grain_t *g = &P->gr[k];
            if (g->on && !before[k]) {
                uint32_t used = (uint32_t)(((uint64_t)(g->wph / (g->winc ? g->winc : 1u)) * g->step) >> 16);
                uint32_t start = g->pos - 2u - used, len = (uint32_t)(0xFFFFFFFFu / (g->winc ? g->winc : 1u));
                st->n++;
                st->min = start < st->min ? start : st->min;
                st->max = start > st->max ? start : st->max;
                st->lenmin = len < st->lenmin ? len : st->lenmin;
                st->lenmax = len > st->lenmax ? len : st->lenmax;
                st->step += g->step;
            }
        }
    }
    if (st->n)
        st->step /= st->n;
}

int main(int argc, char **argv)
{
    const char *demo = argc > 1 ? argv[1] : 0;
    uint32_t pi, k, idx, g;
    static double y[FS * 4];
    double pk, rms, mean;
    char buf[200];

    /* 1. the keys */
    {
        int ok = 1;
        uint32_t ng = 0, ngl = 0, nct = 0;
        for (k = 0; k < CV_KEYS; k++) {                /* 16+FX: whites grains 0..15 in order, blacks glitches 0..10 */
            uint32_t r = cv_role(CV_LAY_16, k, &idx);
            if (r == CV_R_GRAIN)
                ok &= idx == ng++;
            else
                ok &= r == CV_R_GLITCH && idx == ngl++;
        }
        ok &= ng == 16u && ngl == 11u;
        for (k = 0; k < CV_KEYS; k++)
            ok &= cv_role(CV_LAY_27, k, &idx) == CV_R_GRAIN && idx == k;
        ng = ngl = 0;
        for (k = 0; k < CV_KEYS; k++) {
            uint32_t r = cv_role(CV_LAY_8, k, &idx);
            ng += r == CV_R_GRAIN && idx < 8u;
            ngl += r == CV_R_GLITCH;
            nct += r == CV_R_CTL && idx < CV_CT_N;
        }
        ok &= ng == 8u && ngl == 11u && nct == 8u && key_of(CV_LAY_8, CV_R_CTL, 0) == 14u;   /* ALL OFF on G4 */
        ok &= cv_key_of(53) == 0 && cv_key_of(79) == 26u && cv_key_of(80) == 0 && cv_key_of(52) == 26u;
        check("keys: 16+FX 16 grains + 11 glitches, 27 all grains, 8+CTL 8 + 8 controls + 11 glitches; notes folded", ok);
    }

    /* 2. the store */
    {
        uint8_t pk8[FM6_PACKED];
        int ok = 1;
        uint32_t i, maxb = 0;
        setup(0);
        for (g = 0; g < CV_KEYS; g++) {
            cv_set(&trk[1], g, CV_POS, (int32_t)(g * 4u));
            cv_set(&trk[1], g, CV_SIZE, (int32_t)(127u - g));
            cv_set(&trk[1], g, CV_DENS, (int32_t)(g * 3u));
            cv_set(&trk[1], g, CV_PTCH, (int32_t)g - 13);
        }
        canvas_pack(1, pk8);
        for (i = 0; i < FM6_PACKED; i++)
            maxb = pk8[i] > maxb ? pk8[i] : maxb;
        memset(cv_slot[1], 0, sizeof cv_slot[1]);
        ok &= maxb <= 127u && canvas_unpack(1, pk8);
        for (g = 0; g < CV_KEYS; g++)
            ok &= cv_get(&trk[1], g, CV_POS) == (int32_t)(g * 4u) && cv_get(&trk[1], g, CV_SIZE) == (int32_t)(127u - g) &&
                  cv_get(&trk[1], g, CV_DENS) == (int32_t)(g * 3u) && cv_get(&trk[1], g, CV_PTCH) == (int32_t)g - 13;
        memcpy(pk8, FM6_INIT, FM6_PACKED);
        ok &= !canvas_unpack(1, pk8) && cv_get(&trk[1], 3, CV_POS) == 12;
        cv_set(&trk[1], 0, CV_PTCH, 99);
        ok &= cv_get(&trk[1], 0, CV_PTCH) == 24;
        check("store: 27 x POS SIZE DENS PTCH a track, through an FM6 area (7-bit) and back; a patch is none; PTCH +-24", ok);
    }

    /* 3. every preset */
    for (pi = 0; pi < ENG_CANVAS.npresets; pi++) {
        uint32_t lay, keys[3], hold;
        double pk2;
        setup(pi);
        lay = cv_lay(trk[0].p);
        hold = trk[0].p[P_E2] == CV_HOLD;
        for (k = 0; k < 3u; k++) {
            keys[k] = key_of(lay, CV_R_GRAIN, k * 3u);
            trk_note_on(&trk[0], key_note(keys[k]), 100);
            if (!hold)
                trk_note_off(&trk[0], key_note(keys[k]));
        }
        render(FS / 2u, 0, 0, 0, 0);
        render(FS, y, &pk, &rms, &mean);
        for (k = 0; k < 3u; k++) {
            if (hold) {
                trk_note_off(&trk[0], key_note(keys[k]));
            } else {
                trk_note_on(&trk[0], key_note(keys[k]), 100);
                trk_note_off(&trk[0], key_note(keys[k]));
            }
        }
        render(FS * 3u, 0, 0, 0, 0);
        render(FS / 4u, 0, &pk2, 0, 0);
        snprintf(buf, sizeof buf, "%-7s three grains: rms %.4f, peak %.3f < 2, |dc| %.5f; off: silent (%.5f), no voice left",
                 ENG_CANVAS.presets[pi].name, rms, pk, fabs(mean), pk2);
        check(buf, rms > 0.004 && pk < 2.0 && fabs(mean) < 0.01 && pk2 == 0.0 && !voices_active());
    }

    /* 4. LATCH and HOLD */
    {
        cv_part_t *P;
        double a, b2, c;
        setup(0);                                      /* SPREAD: LATCH */
        P = cv_of(&trk[0]);
        tap(0);
        render(FS / 2u, 0, 0, &a, 0);
        render(FS / 2u, 0, 0, &b2, 0);
        snprintf(buf, sizeof buf, "LATCH: a tap turns grain 1 on, its voice gone at once (%d), it sounds on (rms %.4f, %.4f)",
                 voices_active(), a, b2);
        check(buf, (P->latched & 1u) && !voices_active() && a > 0.003 && b2 > 0.003);
        tap(0);
        render(FS * 2u, 0, 0, 0, 0);
        render(FS / 4u, 0, &c, 0, 0);
        check("LATCH: a tap again fades it out to silence", !(P->latched & 1u) && c == 0.0);
        setup(0);
        trk[0].p[P_E2] = CV_HOLD;
        P = cv_of(&trk[0]);
        trk_note_on(&trk[0], key_note(0), 100);
        render(FS / 2u, 0, 0, &a, 0);
        trk_note_off(&trk[0], key_note(0));
        render(FS * 2u, 0, 0, 0, 0);
        render(FS / 4u, 0, &c, 0, 0);
        snprintf(buf, sizeof buf, "HOLD: grain 1 sounds while held (rms %.4f), silent after its fade, not latched", a);
        check(buf, a > 0.003 && c == 0.0 && !P->latched);
    }

    /* 5. a grain's own settings */
    {
        gstat_t st0, st1;
        cv_part_t *P;
        uint32_t zl, want;
        setup(0);
        trk[0].p[P_E3] = 0;                            /* SPRY 0 */
        trk[0].p[P_E5] = 0;                            /* RAND 0 */
        cv_set(&trk[0], 2, CV_POS, 70);
        cv_set(&trk[0], 2, CV_SIZE, 20);
        cv_set(&trk[0], 2, CV_DENS, 60);
        cv_set(&trk[0], 2, CV_PTCH, 0);
        tap(key_of(CV_LAY_16, CV_R_GRAIN, 2));
        render(FS / 2u, 0, 0, 0, 0);
        P = cv_of(&trk[0]);
        want = cv_place(P, 70 << 8, &zl);
        watch(FS / CTL, &st0);
        snprintf(buf, sizeof buf, "POS 70: %u grains, all from its place %u (got %u..%u)", st0.n, want, st0.min, st0.max);
        check(buf, st0.n > 5u && st0.min + 1u >= want && st0.max <= want + 1u);
        cv_set(&trk[0], 2, CV_PTCH, 12);
        watch(FS / CTL, &st1);
        snprintf(buf, sizeof buf, "PTCH +12: step %.0f -> %.0f (x2)", st0.step, st1.step);
        check(buf, fabs(st1.step - 2 * st0.step) < 600);
        cv_set(&trk[0], 2, CV_PTCH, 0);
        cv_set(&trk[0], 2, CV_DENS, 110);
        watch(FS / CTL, &st1);
        snprintf(buf, sizeof buf, "DENS 60 -> 110: %u -> %u grains a second", st0.n, st1.n);
        check(buf, st1.n > 3u * st0.n);
    }

    /* 6. glitches */
    {
        gstat_t a0, a1;
        cv_part_t *P;
        uint32_t gk, i, runs = 0, same = 0;
        setup(0);
        trk[0].p[P_E3] = 0;
        trk[0].p[P_E5] = 0;
        cv_set(&trk[0], 0, CV_POS, 40);
        cv_set(&trk[0], 0, CV_DENS, 100);
        cv_set(&trk[0], 0, CV_SIZE, 40);
        tap(key_of(CV_LAY_16, CV_R_GRAIN, 0));
        render(FS / 2u, 0, 0, 0, 0);
        P = cv_of(&trk[0]);
        watch(FS / 2u / CTL, &a0);
        gk = key_of(CV_LAY_16, CV_R_GLITCH, CV_GL_SCAT);
        trk_note_on(&trk[0], key_note(gk), 100);
        watch(FS / 2u / CTL, &a1);
        trk_note_off(&trk[0], key_note(gk));
        snprintf(buf, sizeof buf, "SCATR: places spread %u..%u (without: %u..%u)", a1.min, a1.max, a0.min, a0.max);
        check(buf, a1.max - a1.min > 10u * (a0.max - a0.min + 100u));
        render(CTL * 4u, 0, 0, 0, 0);
        gk = key_of(CV_LAY_16, CV_R_GLITCH, CV_GL_JUMP);
        trk_note_on(&trk[0], key_note(gk), 100);
        {
            gstat_t j;
            uint32_t lo = 0xFFFFFFFFu, hi = 0, b2;
            for (b2 = 0; b2 < 40u; b2++) {
                watch(FS / 40u / CTL, &j);
                if (j.n) {
                    lo = (uint32_t)j.step < lo ? (uint32_t)j.step : lo;
                    hi = (uint32_t)j.step > hi ? (uint32_t)j.step : hi;
                }
            }
            snprintf(buf, sizeof buf, "JUMP: the grains' steps leap %u..%u (plain %.0f)", lo, hi, a0.step);
            check(buf, hi > 1.4 * a0.step && lo < 0.75 * a0.step);
        }
        trk_note_off(&trk[0], key_note(gk));
        render(CTL * 4u, 0, 0, 0, 0);
        gk = key_of(CV_LAY_16, CV_R_GLITCH, CV_GL_BURST);
        trk_note_on(&trk[0], key_note(gk), 100);
        watch(FS / 2u / CTL, &a1);
        trk_note_off(&trk[0], key_note(gk));
        snprintf(buf, sizeof buf, "BURST: %u -> %u grains, length %u -> %u", a0.n, a1.n, a0.lenmax, a1.lenmax);
        check(buf, a1.n > 2u * a0.n && a1.lenmax * 3u < a0.lenmax);
        render(CTL * 4u, 0, 0, 0, 0);
        gk = key_of(CV_LAY_16, CV_R_GLITCH, CV_GL_CRUSH);
        trk[0].p[P_E7] = 127;
        trk_note_on(&trk[0], key_note(gk), 100);
        render(CTL * 2u, 0, 0, 0, 0);
        render(FS / 4u, y, 0, 0, 0);
        trk_note_off(&trk[0], key_note(gk));
        for (i = 1; i < FS / 4u; i++, runs++)
            same += y[i] == y[i - 1];
        snprintf(buf, sizeof buf, "CRUSH 127: samples held (%.0f %% repeat the one before)", 100.0 * same / runs);
        check(buf, same * 10u > runs * 8u);
        render(CTL * 4u, 0, 0, 0, 0);
        {
            int ok;
            uint32_t r16 = key_of(CV_LAY_16, CV_R_GLITCH, CV_GL_R16), frz = key_of(CV_LAY_16, CV_R_GLITCH, CV_GL_FRZ);
            trk_note_on(&trk[0], key_note(r16), 100);
            render(CTL * 2u, 0, 0, 0, 0);
            ok = (perf_held >> PF_R16) & 1u;
            trk_note_on(&trk[0], key_note(frz), 100);
            render(CTL * 2u, 0, 0, 0, 0);
            ok &= ((perf_held >> PF_FRZ) & 1u) && ((perf_held >> PF_R16) & 1u);
            trk_note_off(&trk[0], key_note(r16));
            render(CTL * 2u, 0, 0, 0, 0);
            ok &= !((perf_held >> PF_R16) & 1u) && ((perf_held >> PF_FRZ) & 1u);
            trk_note_off(&trk[0], key_note(frz));
            render(CTL * 2u, 0, 0, 0, 0);
            ok &= !perf_held;
            check("RPT16 / FREEZ: the FX layer's effects held with their keys, let go with them", ok);
        }
        (void)P;
    }

    /* 7. controls (8+CTL) */
    {
        cv_part_t *P;
        int ok;
        int32_t p0 = 0;
        gstat_t s0, s1;
        setup(3);                                      /* GLITCH: 8+CTL, HOLD */
        trk[0].p[P_E2] = CV_LATCH;
        trk[0].p[P_E5] = 0;
        P = cv_of(&trk[0]);
        tap(key_of(CV_LAY_8, CV_R_GRAIN, 0));
        tap(key_of(CV_LAY_8, CV_R_GRAIN, 5));
        ok = P->latched == ((1u << 0) | (1u << 5));
        tap(key_of(CV_LAY_8, CV_R_CTL, CV_CT_OFF));
        ok &= !P->latched;
        check("ALL OFF: every grain off", ok);
        tap(key_of(CV_LAY_8, CV_R_GRAIN, 1));
        for (k = 0, ok = 0; k < 4u; k++) {            /* (a random draw may land on the same place: a few tries) */
            p0 = cv_get(&trk[0], 1, CV_POS);
            tap(key_of(CV_LAY_8, CV_R_CTL, CV_CT_RAND));
            ok |= cv_get(&trk[0], 1, CV_POS) != p0;
        }
        check("RANDOM: the sounding grain to a new place", ok && cv_get(&trk[0], 0, CV_POS) == cv_get(&trk[0], 0, CV_POS));
        render(FS / 4u, 0, 0, 0, 0);
        watch(FS / 4u / CTL, &s0);
        trk_note_on(&trk[0], key_note(key_of(CV_LAY_8, CV_R_CTL, CV_CT_UP)), 100);
        watch(FS / 4u / CTL, &s1);
        trk_note_off(&trk[0], key_note(key_of(CV_LAY_8, CV_R_CTL, CV_CT_UP)));
        snprintf(buf, sizeof buf, "PIT+ held: step %.0f -> %.0f (an octave up)", s0.step, s1.step);
        check(buf, fabs(s1.step - 2 * s0.step) < 0.1 * s0.step);
        trk_note_on(&trk[0], key_note(key_of(CV_LAY_8, CV_R_CTL, CV_CT_SCAN)), 100);
        render(FS, 0, 0, 0, 0);
        trk_note_off(&trk[0], key_note(key_of(CV_LAY_8, CV_R_CTL, CV_CT_SCAN)));
        p0 = P->scan;
        render(FS / 2u, 0, 0, 0, 0);
        snprintf(buf, sizeof buf, "SCAN held 1 s: the places drift %d / 32768 of the material, and stay", (int)p0);
        check(buf, p0 > 6500 && p0 < 9500 && P->scan == p0);   /* (the whole material in ~4 s) */
        tap(key_of(CV_LAY_8, CV_R_CTL, CV_CT_LATCH));
        check("LATCH (the control): LATCH -> HOLD, the latched grains off", trk[0].p[P_E2] == CV_HOLD && !P->latched);
    }

    /* 8. a fresh part; an empty SRC */
    {
        double pk2;
        setup(0);
        tap(0);
        render(FS / 100u, 0, &pk, 0, 0);
        setup(0);
        trk[0].p[P_E0] = CV_SRC_USR2;
        tap(0);
        render(FS / 2u, 0, &pk2, 0, 0);
        snprintf(buf, sizeof buf, "fresh part: sound within 10 ms (peak %.3f); SRC USR2 empty: silent (%.4f)", pk, pk2);
        check(buf, pk > 0.005 && pk2 == 0.0);
    }

    /* 9. determinism, cost */
    {
        static double a[FS / 2], b2[FS / 2];
        uint32_t i;
        int same = 1;
        setup(1);
        cv_rng[0] = 0x6C8E9CF5;
        tap(0); tap(4);
        render(FS / 2u, a, 0, 0, 0);
        setup(1);
        cv_rng[0] = 0x6C8E9CF5;
        tap(0); tap(4);
        render(FS / 2u, b2, 0, 0, 0);
        for (i = 0; i < FS / 2u; i++)
            same &= a[i] == b2[i];
        check("the same taps from silence render the same samples again (same random state)", same);
    }
    {
        static const uint8_t ch[4] = {48, 55, 60, 67};
        uint64_t t0, tc = ~0ull, tg[16];
        uint32_t rep, gp, gworst = 0, ng = ENGINES[ENGI_GRAIN_IDX]->npresets;
        double cpu_c, cpu_g;
        for (gp = 0; gp < 16u; gp++)
            tg[gp] = ~0ull;
        for (rep = 0; rep < 3u; rep++) {
            setup(1);
            for (g = 0; g < 16u; g++) {
                cv_set(&trk[0], g, CV_SIZE, 127);
                cv_set(&trk[0], g, CV_DENS, 127);
                tap(key_of(CV_LAY_16, CV_R_GRAIN, g));
            }
            render(FS, 0, 0, 0, 0);
            t0 = now_ns();
            render(FS * 2u, 0, 0, 0, 0);
            t0 = now_ns() - t0;
            tc = t0 < tc ? t0 : tc;
            for (gp = 0; gp < ng && gp < 16u; gp++) {
                memset(trk, 0, sizeof trk);
                host_tracks_init();
                host_preset(&trk[0], ENGI_GRAIN_IDX, gp);
                for (k = 0; k < 4u; k++)
                    trk_note_on(&trk[0], ch[k], 100);
                render(FS, 0, 0, 0, 0);
                t0 = now_ns();
                render(FS * 2u, 0, 0, 0, 0);
                t0 = now_ns() - t0;
                tg[gp] = t0 < tg[gp] ? t0 : tg[gp];
            }
        }
        for (gp = 1; gp < ng && gp < 16u; gp++)
            if (tg[gp] > tg[gworst])
                gworst = gp;
        cpu_c = (double)tc / (FS * 2u);
        cpu_g = (double)tg[gworst] / (FS * 2u);
        snprintf(buf, sizeof buf, "cost: 16 grains at their heaviest %.1f ns/sample, GRAIN %s %.1f: x%.2f <= %.1f",
                 cpu_c, ENGINES[ENGI_GRAIN_IDX]->presets[gworst].name, cpu_g, cpu_c / cpu_g, CANVAS_COST_RATIO);
        check(buf, cpu_c <= cpu_g * CANVAS_COST_RATIO);
    }

    /* 10. demos: each preset performed through the whole mix (the FX layer's effects included) */
    if (demo) {
        for (pi = 0; pi < ENG_CANVAS.npresets; pi++) {
            FILE *f;
            uint32_t i, n = FS * 12u, lay, hold, step = 0xFFFFu;
            static int32_t o[2 * CTL];
            snprintf(buf, sizeof buf, "%s/canvas_%u_%s.wav", demo, pi + 1u, ENG_CANVAS.presets[pi].name);
            if (!(f = fopen(buf, "wb")))
                break;
            memset(trk, 0, sizeof trk);
            host_tracks_init();
            eng_mem_owner[0] = 0;
            host_preset(&trk[0], ENGI_CANVAS, pi);
            perf_held = 0;
            lay = cv_lay(trk[0].p);
            hold = trk[0].p[P_E2] == CV_HOLD;
            wav_hdr(f, n);
            for (i = 0; i < n; i += CTL) {
                uint32_t j, t = i / (FS / 2u);         /* a half second at a time */
                if (i == 0u || t != step) {
                    uint32_t gk = key_of(lay, CV_R_GRAIN, (t * 5u) % cv_ngrains(lay));
                    uint32_t glk = key_of(lay, CV_R_GLITCH, (t / 3u) % CV_GL_N);
                    step = t;
                    if (hold) {                        /* HOLD: a grain a half second, a glitch now and then */
                        for (k = 0; k < CV_KEYS; k++)
                            trk_note_off(&trk[0], key_note(k));
                        if (t < 22u)
                            trk_note_on(&trk[0], key_note(gk), 100);
                        if (t % 4u == 3u && glk != 0xFFu)
                            trk_note_on(&trk[0], key_note(glk), 100);
                    } else {                           /* LATCH: a grain on every second, off again later */
                        if (t % 2u == 0u && t < 16u) {
                            trk_note_on(&trk[0], key_note(gk), 100);
                            trk_note_off(&trk[0], key_note(gk));
                        }
                        if (t == 13u || t == 14u) {    /* a glitch held for a half second each */
                            uint32_t gg = key_of(lay, CV_R_GLITCH, t == 13u ? CV_GL_SCAT : CV_GL_R16);
                            if (gg != 0xFFu)
                                trk_note_on(&trk[0], key_note(gg), 100);
                        }
                        if (t == 14u || t == 15u)
                            for (k = 0; k < CV_KEYS; k++) {
                                uint32_t r = cv_role(lay, k, &idx);
                                if (r == CV_R_GLITCH)
                                    trk_note_off(&trk[0], key_note(k));
                            }
                        if (t == 18u)
                            cv_of(&trk[0])->latched = 0;   /* (all off: the fades ring out) */
                    }
                }
                mix_block(o, CTL);
                for (j = 0; j < CTL; j++)
                    wav_put(f, o[2 * j], o[2 * j + 1]);
            }
            for (k = 0; k < CV_KEYS; k++)
                trk_note_off(&trk[0], key_note(k));
            fclose(f);
        }
        printf("canvas: demos in %s\n", demo);
    }
    printf(bad ? "canvas: %d FAILED\n" : "canvas: all ok\n", bad);
    return bad ? 1 : 0;
}
