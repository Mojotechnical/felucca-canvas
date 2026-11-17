/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; CANVAS: the Felucca Canvas fork */
/* EDIT > GRAINS (a CANVAS track only, after EDIT 2): the keys of eng_canvas.c as a map, and the picked grain's settings.
 *   KNOB 1..4    the picked grain's POS SIZE DENS PTCH
 *   PRESETS      picks the next / previous grain (without playing it)
 *   KEYS         play as everywhere (grains, glitches, controls: the layout, KEYS); pressing a grain key picks it, so
 *                hold a grain key and turn the knobs to shape it while it sounds
 * The fractal (above the map, ui_graph.c graph_canvas): a Julia set the grains steer (canvas_fractal_step): their POS and
 * levels pick where on the Mandelbrot set's edge its constant c sits, so every set of grains has its own shape and a fade
 * morphs it;
 * the glitches warp it (SCATR CRUSH jitter, FREEZ stops it, REVRS runs it back, JUMP BURST pulse its zoom); with nothing
 * sounding it drifts slowly, dimmer. Coloured from the palette (BG -> SEL -> THEME -> ACCENT -> TEXT), so it follows it.
 * The map: the black keys over the white ones, as on the FM-1; a grain on (latched or held) THEME, sounding in its fade
 * MID, a glitch or control held ACCENT (idle: a dim stub), the picked grain ringed; under it the picked grain or the key
 * pressed last, and SRC. The keys' LEDs (any page): the grains that are on. */
#if FELUCCA_CANVAS
_Static_assert(PF_R16 == 1 && PF_R32 == 2 && PF_REV == 3 && PF_TAPE == 6 && PF_FRZ == 7 && PF_OUP == 8 && PF_ODN == 9,
               "eng_canvas.c CV_GL_PF: perform.c's effect numbers");
static struct {
    uint8_t sel;                 /* the picked grain */
    uint8_t last;                /* the key pressed last (+ 1, 0 = none yet) */
    uint32_t seen;               /* cv_part_t.pressed as last followed */
    int32_t t;                   /* the fractal's time (1/1024 turns of its drift), once a frame */
    int32_t cr, ci;              /* its constant c, Q12, easing towards what the grains ask */
    int32_t zoom;                /* Q12 (4096 = the whole set) */
    uint8_t lit;                 /* 1: something sounds (full colour) */
    uint8_t crush;               /* CRUSH held: coarse cells */
} cvs;

static cv_part_t *canvas_part(void);

/* sin of a in 1/1024 turns, Q12 (a parabola per half turn: smooth enough for a drift) */
static int32_t cv_sin(int32_t a)
{
    int32_t x = a & 1023, s = 1;
    if (x >= 512) {
        x -= 512;
        s = -1;
    }
    return s * (x * (512 - x) / 16);                     /* 4 x (512 - x) / 512^2, Q12: x (512 - x) / 16 */
}
static int32_t cv_cos(int32_t a) { return cv_sin(a + 256); }

/* once a frame: the fractal follows the grains (c, the zoom, the time). c runs along the edge of the Mandelbrot set's
 * main cardioid, c = r (e^{i th} / 2 - e^{2 i th} / 4), where every Julia set is intricate: the grains sounding pick th
 * (their POS, weighted by their levels; a slow drift on top), how many sound pick r (1.0 alone, fine filaments,
 * up to 1.044 with many, scattering into dust) */
static void canvas_fractal_step(void)
{
    cv_part_t *P = canvas_part();
    int32_t sum = 0, acc = 0, dt = 2, z = 4096, th, r, jr = 0, ji = 0, tr, ti, nsnd = 0;
    uint32_t g, on;
    cvs.lit = 0;
    cvs.crush = 0;
    if (P) {
        on = P->latched | P->held;
        for (g = 0; g < CV_KEYS; g++) {
            int32_t lv = P->gain[g] >> 3;                 /* Q12 */
            if (lv <= 0)
                continue;
            acc += cv_get(TSEL, g, CV_POS) * 8 * lv / 4096;   /* POS 0..127 -> a turn (1024) */
            sum += lv;
            nsnd++;
        }
        cvs.lit = on || sum > 0;
        dt = cvs.lit ? 4 + nsnd * 2 : 2;
        if ((P->glitch >> CV_GL_FRZ) & 1u)
            dt = 0;
        if ((P->glitch >> CV_GL_REV) & 1u)
            dt = -dt;
        if (P->glitch & ((1u << CV_GL_JUMP) | (1u << CV_GL_BURST) | (1u << CV_GL_R16) | (1u << CV_GL_R32)))
            z = 4096 - (cv_sin(cvs.t * 8) > 0 ? 1100 : 0);   /* a pulse */
        if ((P->glitch >> CV_GL_TAPE) & 1u)
            z = 4096 + 1400;
        if (P->glitch & ((1u << CV_GL_SCAT) | (1u << CV_GL_CRUSH))) {
            uint32_t q = (uint32_t)cvs.t * 2654435761u;
            jr = (int32_t)((q >> 8) & 255u) - 128;
            ji = (int32_t)((q >> 20) & 255u) - 128;
        }
        cvs.crush = (P->glitch >> CV_GL_CRUSH) & 1u;
    }
    cvs.t += dt;
    th = (sum > 0 ? acc * 4096 / sum : 0) + 300 + cvs.t / 4;   /* (300: at rest, a spiral) */
    r = 4100 + (nsnd > 8 ? 8 : nsnd) * 22;                /* 1.0 .. 1.044, Q12 */
    tr = (cv_cos(th) / 2 - cv_cos(2 * th) / 4) * r / 4096 + jr;
    ti = (cv_sin(th) / 2 - cv_sin(2 * th) / 4) * r / 4096 + ji;
    if (!cvs.cr && !cvs.ci) {
        cvs.cr = tr;
        cvs.ci = ti;
        cvs.zoom = z;
    }
    cvs.cr += (tr - cvs.cr) / 6;                          /* eased: a fade morphs it */
    cvs.ci += (ti - cvs.ci) / 6;
    cvs.zoom += (z - cvs.zoom) / 3;
}

static int canvas_page_ok(void) { return TSEL->eng_req % NENGINES == ENGI_CANVAS; }
static int canvas_page_on(void) { return !ui.home && cur_page()->graph == GR_CANVAS && canvas_page_ok(); }

/* the selected part's engine memory, when it holds CANVAS's state (0: another engine's, or not switched yet) */
static cv_part_t *canvas_part(void)
{
    uint32_t p = (uint32_t)(TSEL - trk) % NPART;
    return TSEL->engine % NENGINES == ENGI_CANVAS && eng_mem_owner[p] == ENGI_CANVAS + 1u ? cv_of(TSEL) : 0;
}
static uint32_t canvas_ng(void) { return cv_ngrains(cv_lay(TSEL->p)); }
static uint32_t canvas_sel(void) { return cvs.sel < canvas_ng() ? cvs.sel : 0u; }

/* once a frame: a key pressed (the keys, the sequencer, MIDI) -> a grain key becomes the picked one */
static void canvas_follow(void)
{
    cv_part_t *P = canvas_part();
    uint32_t idx;
    if (!P || P->pressed == cvs.seen)
        return;
    cvs.seen = P->pressed;
    cvs.last = (uint8_t)(P->last + 1u);
    if (cv_role(cv_lay(TSEL->p), P->last, &idx) == CV_R_GRAIN)
        cvs.sel = (uint8_t)idx;
}

/* KNOB 1..4: the picked grain's POS SIZE DENS PTCH */
static void canvas_knob(uint32_t slot, int32_t steps)
{
    uint32_t g = canvas_sel(), v = slot & 3u;
    cv_set(TSEL, g, v, cv_get(TSEL, g, v) + steps);
}
/* the PRESETS knob: the next / previous grain */
static void canvas_pick(int32_t steps)
{
    int32_t n = (int32_t)canvas_ng();
    cvs.sel = (uint8_t)((((int32_t)canvas_sel() + steps) % n + n) % n);
    cvs.last = 0;
}

/* the grains that are on, as keys (bit k = key k): the LEDs */
static uint32_t canvas_leds(void)
{
    cv_part_t *P = canvas_part();
    uint32_t k, idx, m = 0, on, lay = cv_lay(TSEL->p);
    if (!P)
        return 0;
    on = P->latched | P->held;
    for (k = 0; k < CV_KEYS; k++)
        if (cv_role(lay, k, &idx) == CV_R_GRAIN && ((on >> idx) & 1u))
            m |= 1u << k;
    return m;
}

/* the label under the map: the key pressed last (a glitch, a control) or the picked grain */
static void canvas_label(char *b, uint32_t n)
{
    uint32_t idx, role = cvs.last ? cv_role(cv_lay(TSEL->p), cvs.last - 1u, &idx) : CV_R_GRAIN;
    if (role == CV_R_GLITCH) {
        str_cpy(b, N_CV_GL[idx % CV_GL_N], n);
    } else if (role == CV_R_CTL) {
        str_cpy(b, N_CV_CT[idx % CV_CT_N], n);
    } else {
        str_cpy(b, "GRAIN ", n);
        fmt_int(b + 6, (int32_t)canvas_sel() + 1);
    }
}

/* graph_signature: what the page shows (and the key followed, once a frame) */
static uint32_t canvas_sig(void)
{
    cv_part_t *P = canvas_part();
    uint32_t h = 2166136261u, g;
    canvas_follow();
    canvas_fractal_step();
    h = (h ^ (uint32_t)(cvs.cr * 31 + cvs.ci * 7 + cvs.zoom + cvs.crush)) * 16777619u;   /* (it moves: a redraw a frame) */
    if (P) {
        h = (h ^ P->latched) * 16777619u;
        h = (h ^ P->held) * 16777619u;
        h = (h ^ P->glitch ^ (P->ctl << 16)) * 16777619u;
        for (g = 0; g < CV_KEYS; g++)
            h = (h ^ (uint32_t)(P->gain[g] > 0)) * 16777619u;
    }
    h = (h ^ (canvas_sel() * 64u + cvs.last)) * 16777619u;
    h = (h ^ (cv_src(TSEL->p) * 31u + cv_nzones(cv_src(TSEL->p)) + cv_lay(TSEL->p) * 977u)) * 16777619u;
    return h;
}

/* power-on (main.c, after the autosave): Felucca Canvas starts as the instrument. A restored project that has no
 * CANVAS track gets one on track 1 (its first preset, SPREAD; the project's saved slots are not touched, the autosave
 * takes it from here); the first CANVAS track is selected and the GRAINS page opens, the EDIT button returning to it */
/* the GRAINS page shown (the selected track a CANVAS one): 1 = shown */
static int canvas_show(void)
{
    uint32_t i;
    if (!canvas_page_ok())
        return 0;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_CANVAS)
            break;
    if (i == NPAGES)
        return 0;
    ui.page = (uint8_t)i;
    ui.fam_last[PAGES[i].fam] = (uint8_t)i;
    ui.home = 0;
    page_entered();
    return 1;
}

/* Felucca Canvas (CANVAS_ONLY): every part is CANVAS. A part that is not (an older project, a user preset, the editor,
 * a program change) becomes one, with its factory sound of power-on (engines.c TRK_DEF); main.c, every frame */
static void canvas_lock(void)
{
#if CANVAS_ONLY
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        if (trk[i].eng_req % NENGINES != ENGI_CANVAS) {
            set_engine_of(&trk[i], ENGI_CANVAS);
            apply_preset_to(&trk[i], TRK_DEF[i % NPART][1]);
            if (i == song.sel && !ui.home && cur_page()->fam == FAM_EDIT)
                canvas_show();
        }
#endif
}

static void canvas_boot(void)
{
    uint32_t i, k = NTRK;
    canvas_lock();
    for (i = 0; i < NTRK && k == NTRK; i++)
        if (trk[i].eng_req % NENGINES == ENGI_CANVAS)
            k = i;
    if (k == NTRK) {
        k = 0;
        set_engine_of(&trk[0], ENGI_CANVAS);
    }
    track_select(k);
    canvas_show();
    ui.force = 1;
}
#else
static int canvas_page_ok(void) { return 0; }
static int canvas_page_on(void) { return 0; }
static void canvas_boot(void) {}
static void canvas_lock(void) {}
static int canvas_show(void) { return 0; }
#endif
