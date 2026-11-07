#ifndef TCOLOR_H
#define TCOLOR_H

#ifdef __cplusplus
extern "C" {

#include "types.h"
#include <dolphin/gx.h>

#define TCOLOR_WHITE JUtility::TColor(0xFF, 0xFF, 0xFF, 0xFF)
#define TCOLOR_BLACK JUtility::TColor(0, 0, 0, 0)

namespace JUtility {
struct TColor : public GXColor {
    TColor() {
        set(0xFFFFFFFF);
    }

    TColor(u8 _r, u8 _g, u8 _b, u8 _a) {
        set(_r, _g, _b, _a);
    }

    TColor(u32 u32Color) {
        set(u32Color);
    }

    TColor(GXColor color) {
        set(color);
    }

    TColor& operator=(const TColor& other) {
        ((GXColor*)this)->operator=(other);
        return *this;
    }

    /** @fabricated */
    TColor& operator=(const GXColorS10& other) {
        r = other.r;
        g = other.g;
        b = other.b;
        a = other.a;
        return *this;
    }

    operator u32() const {
        return toUInt32();
    }
    u32 toUInt32() const {
#ifdef SONAR_PORT /* sonarlib port: the console's u32 is r in its high byte; the host's would put it in the low one */
        return (u32)r << 24 | (u32)g << 16 | (u32)b << 8 | a;
#else
        return *(u32*)&r;
#endif
    }

    void set(u8 cR, u8 cG, u8 cB, u8 cA) {
        r = cR;
        g = cG;
        b = cB;
        a = cA;
    }

    void set(u32 u32Color) {
#ifdef SONAR_PORT /* sonarlib port: the console's u32 is r in its high byte; the host's would put it in the low one */
        r = (u8)(u32Color >> 24);
        g = (u8)(u32Color >> 16);
        b = (u8)(u32Color >> 8);
        a = (u8)u32Color;
#else
        *(u32*)&r = u32Color;
#endif
    }

    void set(GXColor gxColor) {
        *(GXColor*)&r = gxColor;
    }
    void set(TColor color) {
        *this = color;
    }

    void setRGB(u8 cR, u8 cG, u8 cB) {
        r = cR;
        g = cG;
        b = cB;
    }

    void setRGB(const TColor& other) {
        setRGB(other.r, other.g, other.b);
    }

    void setRGBA(const TColor& other) {
        r = other.r;
        g = other.g;
        b = other.b;
        a = other.a;
    }

    // _00 = GXColor (_00 = r, _01 = g, _02 = b, _03 = a)
};
} // namespace JUtility
}
#endif

#endif
