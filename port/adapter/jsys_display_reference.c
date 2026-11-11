/* The reference's GX of the frame's seams, in ac_port_reference only (port/display_reference.h, port/reference.h;
   sonarlib docs/steps/m6-10e.md). The console's emu64, the reference, draws through GX under the state the game's
   JFWDisplay.cpp left: the copy's set-up, preGX's pixel format and dither, clearEfb's quad. Under --diff or --reference
   each hook makes the GX calls the game's file made, into the reference's front ends (port/adapter/gx_reference.c), and
   without either it does nothing, so ac_port_reference's GX outside display lists is ac_port's. The frame's clear is
   not here: the seam records it on the jsys kit in both programs, before the quad. */
#include "dolphin/gx.h"
#include "dolphin/mtx.h"

#include <port/display_reference.h>
#include <port/reference.h>

static BOOL active(void) {
    return ac_diff_enabled() || ac_reference_enabled();
}

void ac_reference_sync(ac_reference_sync_call call) {
    if (!active())
        return;
    switch (call) {
        case AC_REFERENCE_FLUSH:
            GXFlush();
            break;
        case AC_REFERENCE_DRAW_DONE:
            GXDrawDone();
            break;
        case AC_REFERENCE_SET_DRAW_DONE:
            GXSetDrawDone();
            break;
        case AC_REFERENCE_ABORT_FRAME:
            GXAbortFrame();
            break;
        case AC_REFERENCE_PIX_MODE_SYNC:
            GXPixModeSync();
            break;
    }
}

void ac_reference_copy_display(void* xfb) {
    if (active())
        GXCopyDisp(xfb, GX_TRUE);
}

/* JFWDisplay::prepareCopyDisp's, but VIFlush, which the seam calls. */
void ac_reference_prepare_copy(const ac_reference_copy* copy) {
    const GXRenderModeObj* rmode = copy->mode;
    if (!active())
        return;
    GXSetCopyClear(copy->clear, copy->z);
    GXSetDispCopySrc(0, 0, copy->width, copy->efb_height);
    GXSetDispCopyDst(copy->width, copy->xfb_height);
    GXSetDispCopyYScale(copy->xfb_height / (f32)copy->efb_height);
    GXSetCopyFilter((GXBool)rmode->aa, rmode->sample_pattern, GX_ENABLE, rmode->vfilter);
    GXSetCopyClamp((GXFBClamp)copy->clamp);
    GXSetDispCopyGamma((GXGamma)copy->gamma);
    GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);
    if (copy->enable_alpha) {
        GXSetAlphaUpdate(GX_ENABLE);
    }
}

/* JFWDisplay::preGX's. */
void ac_reference_pre_gx(const GXRenderModeObj* mode, BOOL enable_alpha) {
    if (!active())
        return;
    GXInvalidateTexAll();
    GXInvalidateVtxCache();

    if (mode->aa) {
        GXSetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
        GXSetDither(GX_ENABLE);
    } else {
        if (enable_alpha) {
            GXSetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
            GXSetDither(GX_ENABLE);
        } else {
            GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
            GXSetDither(GX_DISABLE);
        }
    }
}

/* clearEfb's z texture, JFWDisplay.cpp's clear_z_tobj and clear_z_TX: 4 x 4 Z24X8 texels of 0xFFFFFF, made at the
   first clear. */
static GXTexObj clear_z_tobj;
static BOOL clear_z_made;
static u8 clear_z_TX[] __attribute__((aligned(32))) = {
    0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
    0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
/* The quad's position matrix, JFWDisplay.cpp's e_mtx: the identity. */
static GC_Mtx clear_mtx = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };

/* JFWDisplay::clearEfb(GXColor)'s quad, and clearEfb_init's texture object at the first. */
void ac_reference_clear_efb(const GXRenderModeObj* mode, GXColor color, BOOL enable_alpha) {
    Mtx44 mtx;
    u16 height;
    u16 width;
    if (!active())
        return;
    if (!clear_z_made) {
        GXInitTexObj(&clear_z_tobj, &clear_z_TX, 4, 4, GX_TF_Z24X8, GX_REPEAT, GX_REPEAT, GX_FALSE);
        GXInitTexObjLOD(&clear_z_tobj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        clear_z_made = TRUE;
    }
    height = mode->efbHeight;
    width = mode->fbWidth;

    C_MTXOrtho(mtx, 0.0f, height, 0.0f, width, 0.0f, 1.0f);
    GXSetProjection(mtx, GX_ORTHOGRAPHIC);
    GXSetViewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
    GXSetScissor(0, 0, width, height);

    GXLoadPosMtxImm(clear_mtx, GX_PNMTX0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGB, GX_RGBX8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_CLR_RGBA, GX_RGB565, 0);
    GXSetNumChans(0);
    GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 60, GX_DISABLE, 125);
    GXLoadTexObj(&clear_z_tobj, GX_TEXMAP0);
    GXSetNumTevStages(1);
    GXSetTevColor(GX_TEVREG0, color);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetZTexture(GX_ZT_REPLACE, GX_TF_Z24X8, 0);
    GXSetZCompLoc(GX_DISABLE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_NOOP);

    if (enable_alpha) {
        GXSetAlphaUpdate(GX_ENABLE);
        GXSetDstAlpha(GX_ENABLE, 0);
    }
    GXSetZMode(GX_ENABLE, GX_ALWAYS, GX_ENABLE);
    GXSetCullMode(GX_CULL_BACK);

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition2u16(0, 0);
    GXTexCoord2u8(0, 0);

    GXPosition2u16(0 + width, 0);
    GXTexCoord2u8(1, 0);

    GXPosition2u16(0 + width, 0 + height);
    GXTexCoord2u8(1, 1);

    GXPosition2u16(0, 0 + height);
    GXTexCoord2u8(0, 1);
    GXEnd();

    GXSetZTexture(GX_ZT_DISABLE, GX_TF_Z24X8, 0);
    GXSetZCompLoc(GX_ENABLE);
    if (enable_alpha) {
        GXSetDstAlpha(GX_DISABLE, 0);
    }
}
