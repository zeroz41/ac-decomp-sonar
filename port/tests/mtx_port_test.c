/* Checks the port's C fallbacks for the SDK's paired-single matrix library (src/static/dolphin/mtx): each computes
   the matrix or vector the assembly does, writes the elements and lanes the assembly writes, and gives the same
   result when its output is one of its inputs. Bit-exact checks where the console's result is known, close checks
   against double-precision math elsewhere. Outputs start as NaNs (0xFF bytes), so an element left unwritten fails. */
#include <dolphin/mtx.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

/* The game's sinf and cosf (port/adapter/names.h), which PSMTXRotRad and PSMTXRotAxisRad call. */
float ac_sinf(float x) { return (float)sin((double)x); }
float ac_cosf(float x) { return (float)cos((double)x); }

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

static int close_to(double x, double reference, double scale) { return fabs(x - reference) <= 1e-5 * scale; }

static int mtx_close(GC_Mtx m, const double reference[3][4]) {
    double scale = 1.0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            scale = fmax(scale, fabs(reference[i][j]));
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            if (!close_to(m[i][j], reference[i][j], scale)) {
                printf("  [%d][%d]: %.9g, expected %.9g\n", i, j, m[i][j], reference[i][j]);
                return 0;
            }
        }
    }
    return 1;
}

static int mtx_equal(GC_Mtx a, GC_Mtx b) { return memcmp(a, b, sizeof(GC_Mtx)) == 0; }

/* A x B for 3x4 matrices with an implied bottom row of 0 0 0 1. */
static void concat_reference(GC_Mtx a, GC_Mtx b, double ab[3][4]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            double sum = j == 3 ? a[i][3] : 0.0;
            for (int k = 0; k < 3; k++) {
                sum += (double)a[i][k] * b[k][j];
            }
            ab[i][j] = sum;
        }
    }
}

static void fill(GC_Mtx m, unsigned seed) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            seed = seed * 1103515245u + 12345u;
            m[i][j] = (float)((int)(seed >> 16 & 0x7fff) - 0x4000) / 1024.0f;
        }
    }
}

static void check_layout(void) {
    GC_Mtx m;
    static const double identity[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    static const double trans[3][4] = {{1, 0, 0, 2}, {0, 1, 0, -3}, {0, 0, 1, 4.5}};
    static const double scale[3][4] = {{2, 0, 0, 0}, {0, -3, 0, 0}, {0, 0, 4.5, 0}};

    memset(m, 0xFF, sizeof m);
    PSMTXIdentity(m);
    CHECK(mtx_close(m, identity));
    memset(m, 0xFF, sizeof m);
    PSMTXTrans(m, 2.0f, -3.0f, 4.5f);
    CHECK(mtx_close(m, trans));
    memset(m, 0xFF, sizeof m);
    PSMTXScale(m, 2.0f, -3.0f, 4.5f);
    CHECK(mtx_close(m, scale));

    /* The rotations write every element: cos and sin on the axis' plane, 1 on the axis, 0 elsewhere. */
    const float s = 0.6f, c = 0.8f;
    const double rx[3][4] = {{1, 0, 0, 0}, {0, c, -s, 0}, {0, s, c, 0}};
    const double ry[3][4] = {{c, 0, s, 0}, {0, 1, 0, 0}, {-s, 0, c, 0}};
    const double rz[3][4] = {{c, -s, 0, 0}, {s, c, 0, 0}, {0, 0, 1, 0}};
    memset(m, 0xFF, sizeof m);
    PSMTXRotTrig(m, 'x', s, c);
    CHECK(mtx_close(m, rx));
    memset(m, 0xFF, sizeof m);
    PSMTXRotTrig(m, 'Y', s, c);
    CHECK(mtx_close(m, ry));
    memset(m, 0xFF, sizeof m);
    PSMTXRotTrig(m, 'z', s, c);
    CHECK(mtx_close(m, rz));
    memset(m, 0xFF, sizeof m);
    PSMTXRotTrig(m, 'w', s, c); /* not an axis: nothing written */
    CHECK(((unsigned char*)m)[0] == 0xFF && ((unsigned char*)m)[sizeof m - 1] == 0xFF);

    /* About the z axis, PSMTXRotAxisRad is PSMTXRotTrig's rotation. */
    const Vec axis = {0.0f, 0.0f, 2.0f};
    PSMTXRotAxisRad(m, &axis, 0.5f);
    const double rz2[3][4] = {{cos(0.5), -sin(0.5), 0, 0}, {sin(0.5), cos(0.5), 0, 0}, {0, 0, 1, 0}};
    CHECK(mtx_close(m, rz2));
    const Vec tilted = {1.0f, 2.0f, -2.0f};
    PSMTXRotAxisRad(m, &tilted, 1.25f);
    {
        /* Rodrigues' formula. */
        const double x = 1.0 / 3, y = 2.0 / 3, z = -2.0 / 3, sn = sin(1.25), cs = cos(1.25), t = 1 - cs;
        const double r[3][4] = {{t * x * x + cs, t * x * y - sn * z, t * x * z + sn * y, 0},
                                {t * x * y + sn * z, t * y * y + cs, t * y * z - sn * x, 0},
                                {t * x * z - sn * y, t * y * z + sn * x, t * z * z + cs, 0}};
        CHECK(mtx_close(m, r));
    }

    /* A unit quaternion's rotation, and a scaled one's: PSMTXQuat divides by the squared norm. */
    const PSQuaternion q = {0.5f, -0.5f, 0.5f, 0.5f};
    const PSQuaternion q2 = {1.0f, -1.0f, 1.0f, 1.0f};
    const double x = 0.5, y = -0.5, z = 0.5, w = 0.5;
    const double rq[3][4] = {{1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y), 0},
                             {2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x), 0},
                             {2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y), 0}};
    memset(m, 0xFF, sizeof m);
    PSMTXQuat(m, &q);
    CHECK(mtx_close(m, rq));
    PSMTXQuat(m, &q2);
    {
        /* fres and one Newton step: closer than 1e-5 is not promised, so compare at 1e-4. */
        int ok = 1;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 4; j++) {
                ok &= fabs(m[i][j] - rq[i][j]) < 1e-4;
            }
        }
        CHECK(ok);
    }
}

static void check_products(void) {
    GC_Mtx a, b, ab, inv, check;
    double reference[3][4];
    static const double identity[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};

    for (unsigned seed = 1; seed < 40; seed++) {
        fill(a, seed);
        fill(b, seed * 7919u);
        concat_reference(a, b, reference);
        PSMTXConcat(a, b, ab);
        CHECK(mtx_close(ab, reference));

        /* The output may be either input. */
        GC_Mtx alias;
        memcpy(alias, a, sizeof alias);
        PSMTXConcat(alias, b, alias);
        CHECK(mtx_equal(alias, ab));
        memcpy(alias, b, sizeof alias);
        PSMTXConcat(a, alias, alias);
        CHECK(mtx_equal(alias, ab));

        PSMTXCopy(a, alias);
        CHECK(mtx_equal(alias, a));

        if (PSMTXInverse(a, inv)) {
            PSMTXConcat(a, inv, check);
            double scale = 0.0;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 4; j++) {
                    scale = fmax(scale, fabs(inv[i][j]) * 16.0);
                }
            }
            int ok = 1;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 4; j++) {
                    ok &= fabs(check[i][j] - identity[i][j]) <= 2e-3 * fmax(scale, 1.0);
                }
            }
            CHECK(ok);
            memcpy(alias, a, sizeof alias);
            CHECK(PSMTXInverse(alias, alias) == 1);
            CHECK(mtx_equal(alias, inv));
        }

        PSMTXTransApply(a, alias, 1.5f, -2.0f, 3.25f);
        CHECK(alias[0][3] == a[0][3] + 1.5f && alias[1][3] == a[1][3] + -2.0f && alias[2][3] == a[2][3] + 3.25f);
        CHECK(alias[0][0] == a[0][0] && alias[1][2] == a[1][2] && alias[2][1] == a[2][1]);
        PSMTXScaleApply(a, alias, 1.5f, -2.0f, 3.25f);
        CHECK(alias[0][1] == a[0][1] * 1.5f && alias[1][3] == a[1][3] * -2.0f && alias[2][2] == a[2][2] * 3.25f);
    }

    /* A singular matrix has no inverse, and its output is left alone. */
    memset(a, 0, sizeof a);
    a[0][0] = 1.0f;
    memset(inv, 0xFF, sizeof inv);
    CHECK(PSMTXInverse(a, inv) == 0);
    CHECK(((unsigned char*)inv)[0] == 0xFF);
    PSMTXScale(a, 4.0f, 0.5f, -2.0f);
    CHECK(PSMTXInverse(a, inv) == 1);
    CHECK(inv[0][0] == 0.25f && inv[1][1] == 2.0f && inv[2][2] == -0.5f && inv[0][3] == 0.0f);
}

static void check_vectors(void) {
    GC_Mtx m;
    fill(m, 99);
    const Vec v = {1.5f, -2.25f, 0.75f};
    Vec out;

    PSMTXMultVec(m, &v, &out);
    CHECK(close_to(out.x, (double)m[0][0] * v.x + (double)m[0][1] * v.y + (double)m[0][2] * v.z + m[0][3], 64));
    CHECK(close_to(out.y, (double)m[1][0] * v.x + (double)m[1][1] * v.y + (double)m[1][2] * v.z + m[1][3], 64));
    CHECK(close_to(out.z, (double)m[2][0] * v.x + (double)m[2][1] * v.y + (double)m[2][2] * v.z + m[2][3], 64));
    Vec in_place = v;
    PSMTXMultVec(m, &in_place, &in_place);
    CHECK(memcmp(&in_place, &out, sizeof out) == 0);

    PSMTXMultVecSR(m, &v, &out);
    CHECK(close_to(out.x, (double)m[0][0] * v.x + (double)m[0][1] * v.y + (double)m[0][2] * v.z, 64));
    CHECK(close_to(out.z, (double)m[2][0] * v.x + (double)m[2][1] * v.y + (double)m[2][2] * v.z, 64));

    /* The array form, for two or more vectors, writes each and nothing past the last. */
    Vec src[5], dst[6];
    for (int i = 0; i < 5; i++) {
        src[i].x = v.x * (float)(i + 1);
        src[i].y = v.y - (float)i;
        src[i].z = v.z + 0.5f * (float)i;
    }
    memset(dst, 0xFF, sizeof dst);
    PSMTXMultVecArray(m, src, dst, 5);
    for (int i = 0; i < 5; i++) {
        PSMTXMultVec(m, &src[i], &out);
        CHECK(close_to(dst[i].x, out.x, 64) && close_to(dst[i].y, out.y, 64) && close_to(dst[i].z, out.z, 64));
    }
    CHECK(((unsigned char*)&dst[5])[0] == 0xFF);

    const Vec a = {3.0f, -4.0f, 12.0f}, b = {0.5f, 2.0f, -1.0f};
    PSVECAdd(&a, &b, &out);
    CHECK(out.x == 3.5f && out.y == -2.0f && out.z == 11.0f);
    PSVECSubtract(&a, &b, &out);
    CHECK(out.x == 2.5f && out.y == -6.0f && out.z == 13.0f);
    PSVECScale(&a, &out, 0.5f);
    CHECK(out.x == 1.5f && out.y == -2.0f && out.z == 6.0f);
    CHECK(PSVECDotProduct(&a, &b) == 1.5f - 8.0f - 12.0f);
    CHECK(PSVECSquareDistance(&a, &b) == 2.5f * 2.5f + 36.0f + 169.0f);
    CHECK(close_to(PSVECDistance(&a, &b), sqrt(2.5 * 2.5 + 36.0 + 169.0), 1));
    CHECK(close_to(PSVECMag(&a), 13.0, 1));
    const Vec zero = {0.0f, 0.0f, 0.0f};
    CHECK(PSVECMag(&zero) == 0.0f); /* the estimate is infinite, and fsel picks the zero */
    CHECK(PSVECDistance(&a, &a) == 0.0f);

    PSVECCrossProduct(&a, &b, &out);
    CHECK(out.x == -4.0f * -1.0f - 12.0f * 2.0f && out.y == 12.0f * 0.5f - 3.0f * -1.0f &&
          out.z == 3.0f * 2.0f - -4.0f * 0.5f);
    Vec cross_in_place = a;
    PSVECCrossProduct(&cross_in_place, &b, &cross_in_place);
    CHECK(memcmp(&cross_in_place, &out, sizeof out) == 0);

    PSVECNormalize(&a, &out);
    CHECK(close_to(out.x, 3.0 / 13, 1) && close_to(out.y, -4.0 / 13, 1) && close_to(out.z, 12.0 / 13, 1));

    /* C_MTXLookAt: rows right, up and back, each with its translation. */
    const Vec eye = {0.0f, 0.0f, 5.0f}, up = {0.0f, 1.0f, 0.0f}, target = {0.0f, 0.0f, 0.0f};
    C_MTXLookAt(m, &eye, &up, &target);
    static const double look[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, -5}};
    CHECK(mtx_close(m, look));
}

int main(void) {
    check_layout();
    check_products();
    check_vectors();
    if (failures != 0) {
        printf("%d failed\n", failures);
        return 1;
    }
    return 0;
}
