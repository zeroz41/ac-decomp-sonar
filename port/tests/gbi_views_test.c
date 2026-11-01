/* Checks the port's views of a display-list command (port/tools/gbi_views.py): each field lands where the console's
   compiler put it, in a Gfx whose words the game's macros write. */
#include "libforest/gbi_extensions.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The decompilation's math headers define functions that read MSL's constants, which the port does not link. */
unsigned long __float_nan[1] = {0x7fc00000};
unsigned long __float_huge[1] = {0x7f800000};

static int failures;

/* Each word of the console's struct: word k is half k % 2 of Gfx k / 2. */
static uintptr_t word_of(const Gfx* g, int k) { return k % 2 == 0 ? g[k / 2].words.w0 : g[k / 2].words.w1; }

static void check_words(const char* view, const char* field, const Gfx* g, int count, int word, uintptr_t mask) {
    for (int k = 0; k < count * 2; k++) {
        uintptr_t expected = k == word ? mask : 0;
        if (word_of(g, k) != expected) {
            printf("%s.%s: word %d is 0x%llx, expected 0x%llx\n", view, field, k,
                   (unsigned long long)word_of(g, k), (unsigned long long)expected);
            failures++;
        }
    }
}

#define CHECK_SIZE(view, gfx_count)                                                                                   \
    if (sizeof(view) != (gfx_count) * sizeof(Gfx)) {                                                                  \
        printf(#view ": %zu bytes, expected %zu\n", sizeof(view), (size_t)(gfx_count) * sizeof(Gfx));                 \
        failures++;                                                                                                   \
    }
#define CHECK_FIELD(view, field, word, mask)                                                                          \
    {                                                                                                                 \
        Gfx g[3];                                                                                                     \
        memset(g, 0, sizeof g);                                                                                       \
        ((view*)g)->field = -1;                                                                                       \
        check_words(#view, #field, g, 3, word, mask);                                                                 \
    }

static void check_views(void) {
#include "gbi_views_cases.inc"
}

/* The game's own macros against the views, independent of the generator's reading of the console's layout. */
static void check_macros(void) {
    Gfx g[3];
    Gfx* pkt;

    memset(g, 0, sizeof g);
    pkt = g;
    gDPLoadTLUT_Dolphin(pkt++, 5, 256, 2, (void*)g);
    {
        const Gloadtlut_dolphin* v = (const Gloadtlut_dolphin*)g;
        if (v->cmd != (signed char)G_LOADTLUT || v->type != G_TLUT_DOLPHIN || v->tlut_name != 5 || v->pad1 != 2 ||
            v->count != 256 || v->tlut_addr != (uintptr_t)g) {
            printf("gDPLoadTLUT_Dolphin: read back %d %u %u %u %u\n", v->cmd, v->type, v->tlut_name, v->pad1,
                   v->count);
            failures++;
        }
    }

    memset(g, 0, sizeof g);
    pkt = g;
    gSPVertex(pkt++, (void*)g, 12, 3);
    {
        const Gvtx* v = (const Gvtx*)g;
        if (v->cmd != G_VTX || v->n != 12 || v->vn != (3 + 12) * 2 || v->addr != (uintptr_t)g) {
            printf("gSPVertex: read back %u %u %u\n", v->cmd, v->n, v->vn);
            failures++;
        }
    }

    memset(g, 0, sizeof g);
    pkt = g;
    gSP2Triangles(pkt++, 1, 2, 3, 0, 4, 5, 6, 0);
    {
        const Gtri2* v = (const Gtri2*)g;
        if (v->cmd != G_TRI2 || v->t0v0 != 2 || v->t0v1 != 4 || v->t0v2 != 6 || v->t1v0 != 8 || v->t1v1 != 10 ||
            v->t1v2 != 12) {
            printf("gSP2Triangles: read back %u %u %u %u %u %u\n", v->t0v0, v->t0v1, v->t0v2, v->t1v0, v->t1v1,
                   v->t1v2);
            failures++;
        }
    }
}

int main(void) {
    check_views();
    check_macros();
    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}
