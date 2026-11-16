/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; CANVAS: the Felucca Canvas fork */
/* EDIT > GRAINS (a CANVAS track only, after EDIT 2): the keys of eng_canvas.c as a map, and the picked grain's settings.
 *   KNOB 1..4    the picked grain's POS SIZE DENS PTCH
 *   PRESETS      picks the next / previous grain (without playing it)
 *   KEYS         play as everywhere (grains, glitches, controls: the layout, KEYS); pressing a grain key picks it, so
 *                hold a grain key and turn the knobs to shape it while it sounds
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
} cvs;

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
#else
static int canvas_page_ok(void) { return 0; }
static int canvas_page_on(void) { return 0; }
#endif
