/* SPDX-License-Identifier: GPL-3.0-only */
/* Clipper's names, for the computer's side of the user's Clipper sounds (tools/ysb.py, the page): the factory
 * sounds (engine, preset, name), each engine's eight EDIT labels, the parameters' labels (Clipper's P_*), the
 * reverb's models. Writes web/clp_names.json.   make clp-names */
#include <stdio.h>
#include "clp.c"
int plat_cpu2_on(void) { return 0; }
int plat_cpu2_run(void (*f)(uint32_t), uint32_t a) { (void)f, (void)a; return 0; }
int plat_cpu2_wait(void) { return 0; }

static void str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; s && *s; s++)
        fprintf(f, *s == '"' || *s == '\\' ? "\\%c" : "%c", *s);
    fputc('"', f);
}

int main(int argc, char **argv)
{
    unsigned i, e, k;
    const param_desc_t *g = &GP[G_RTYPE];
    FILE *f = fopen(argc > 1 ? argv[1] : "web/clp_names.json", "w");
    if (!f)
        return 1;
    clp_init();
    fprintf(f, "{\"format\": \"mvane-yard-clipper-names\", \"params\": %d, \"edit0\": %d,\n \"sounds\": [", P_COUNT, P_E0);
    for (i = 0; i < clp_sounds(); i++) {
        unsigned key = clp_sound_key(i);
        fprintf(f, "%s\n  {\"engine\": %u, \"preset\": %u, \"name\": ", i ? "," : "", key >> 8, key & 0xFFu);
        str(f, clp_sound_name(i));
        fprintf(f, ", \"engine_name\": ");
        str(f, clp_sound_engine(i));
        fprintf(f, "}");
    }
    fprintf(f, "],\n \"labels\": [");
    for (i = 0; i < P_COUNT; i++) {
        fprintf(f, "%s", i ? ", " : "");
        str(f, i >= P_E0 ? "" : TP[i].label ? TP[i].label : "");
    }
    fprintf(f, "],\n \"edit\": {");
    for (e = 0, k = 0; e < NENGINES; e++) {
        unsigned j;
        if (!ENGINES[e]->npresets)
            continue;
        fprintf(f, "%s\n  \"%u\": [", k++ ? "," : "", e);
        for (j = 0; j < 8u; j++) {
            fprintf(f, "%s", j ? ", " : "");
            str(f, ENGINES[e]->edit[j].label ? ENGINES[e]->edit[j].label : "");
        }
        fprintf(f, "]");
    }
    fprintf(f, "},\n \"reverb\": {\"min\": %d, \"names\": [", g->min);
    for (i = 0; g->names && (int)i <= g->max - g->min; i++) {
        fprintf(f, "%s", i ? ", " : "");
        str(f, g->names[i]);
    }
    fprintf(f, "]}\n}\n");
    fclose(f);
    printf("clp_names: %u sounds, %d parameters\n", clp_sounds(), P_COUNT);
    return 0;
}
