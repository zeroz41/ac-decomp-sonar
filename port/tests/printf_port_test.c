/* Checks the port's libultra formatter (src/static/libultra/xprintf.c and xldtob.c) through libc64's sprintf: %f, %e
   and %g of negative and positive doubles print what the host's printf prints, which reads a double as the console's
   formatter does. Infinities print as the formatter spells them. No value is a halfway case for its precision: the
   formatter rounds those up, where the host's rounds to even. */
#include <stdio.h>
#include <string.h>

/* libc64's sprintf, ac_sprintf by port/adapter/names.h. */
int ac_sprintf(char* dst, const char* fmt, ...);

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

int main(void) {
    static const char* const formats[] = { "%f", "%e", "%g", "%E", "%G", "%.3f", "%10.2e", "%-12.4g", "%+f", "% e" };
    static const double values[] = { 1.5,      -1.5,     0.0,  -0.0,     123.456, -123.456, 1e-5,
                                     -1e-5,    3.25e10,  -3.25e10, 0.1,    -0.1,    255.0,  -65536.3 };
    size_t i, j;

    for (i = 0; i < sizeof formats / sizeof formats[0]; i++) {
        for (j = 0; j < sizeof values / sizeof values[0]; j++) {
            char game[64];
            char host[64];

            ac_sprintf(game, formats[i], values[j]);
            snprintf(host, sizeof host, formats[i], values[j]);
            if (strcmp(game, host) != 0) {
                printf("%s of %g: \"%s\", host \"%s\"\n", formats[i], values[j], game, host);
                failures++;
            }
        }
    }

    {
        char game[64];

        ac_sprintf(game, "%f|%e", 1.0 / 0.0, -1.0 / 0.0);
        CHECK(strcmp(game, "Inf|-Inf") == 0);
        ac_sprintf(game, "%d %x %s", -42, 0xBEEF, "ok");
        CHECK(strcmp(game, "-42 beef ok") == 0);
    }

    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}
