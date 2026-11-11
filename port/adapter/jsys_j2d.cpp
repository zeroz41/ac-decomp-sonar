// J2D's seam (port/jsys_j2d.h; sonarlib docs/steps/m6-10c.md): every member of J2DGrafContext and J2DOrthoGraph
// that the game's J2DGrafContext.cpp and J2DOrthoGraph.cpp define, scissorBounds and J2D's free functions, against
// the game's header, unchanged, so the vtables are emitted here as they were there. The game's arithmetic and members
// are kept: the bounds and the scissor's boxes, its row and hard bounds and the integers they gave the console, the
// ortho's 0.5 on its bottom, setOrtho's negated near and far, setColor's blends, the corners' colors, mPrevPos,
// mLineWidth, and mMtx44 and mPosMtx, still filled by C_MTXOrtho and PSMTXIdentity. What the two files handed the
// console's graphics goes to sonarlib's kits/jsys instead: the port's viewport and scissor, the projection, and quads
// and lines in the position matrix. setup2D's state is the kit's materials, and a line's width, kept, is not drawn.
// The position matrix is kept where GX loaded it, for JUTResFont's seam (jsys_font.cpp), which draws its glyphs
// through the same object, under the same port.
#include "JSystem/J2D/J2DGrafContext.h"

#include <port/gx_sink.h>
#include <port/jsys_j2d.h>

#include <sonar/jsys/j2d.h>
#include <sonar/render.h>

namespace {

// The kit's object, handed over by the program; NULL before, when J2D and the font draw nothing.
sl_jsys_j2d* j2d_kit = NULL;

// The position matrix last loaded, as GXLoadPosMtxImm loaded it: at setup2D and at each draw. The font's glyphs are
// drawn under it (ac_jsys_j2d_position).
sl_mat34 loaded_position = sl_mat34_identity();

// J2D's port as the console held it, one for all of J2D's contexts: the last scissor setScissor set and the last
// viewport setPort set, handed to the kit together.
u32 port_scissor[4] = { 0, 0, 0, 0 };
f32 port_viewport[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

void hand_port() {
    if (j2d_kit != NULL)
        sl_jsys_j2d_set_bounds(j2d_kit, port_viewport[0], port_viewport[1], port_viewport[2], port_viewport[3],
                               port_scissor[0], port_scissor[1], port_scissor[2], port_scissor[3]);
}

// The kit's blend for J2D's: type 0 is none, whatever its factors, and type 1 with source alpha (4) and one less
// source alpha (5) blends. false for any other, which no code sets: it draws nothing.
bool kit_blend(const J2DGrafBlend& blend, sl_jsys_j2d_blend* out) {
    if (blend.mType == 0) {
        *out = SL_JSYS_J2D_OPAQUE;
        return true;
    }
    if (blend.mType == 1 && blend.mSrcFactor == 4 && blend.mDestFactor == 5) {
        *out = SL_JSYS_J2D_BLEND;
        return true;
    }
    return false;
}

// The open frame, after the bytes written so far are recorded into it, so a draw follows them; NULL when the kit is
// not handed over, no frame is open or the frame was refused (port/gx_sink.h, ac_frame_task).
sl_frame* open_frame() {
    sl_frame* frame = NULL;
    if (j2d_kit == NULL || !ac_frame_task(&frame))
        return NULL;
    return frame;
}

// GXLoadPosMtxImm's: m is the position matrix from here on, and the one a draw hands the kit.
const sl_mat34& load_position(const GC_Mtx m) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++)
            loaded_position.m[r][c] = m[r][c];
    return loaded_position;
}

// A vertex at z 0, as J2D's were, in the color as the console took it: red, green, blue, alpha.
sl_jsys_j2d_vertex vertex(f32 x, f32 y, const JUtility::TColor& color) {
    sl_jsys_j2d_vertex v = { x, y, 0.0f, { color.r, color.g, color.b, color.a } };
    return v;
}

} // namespace

extern "C" void ac_jsys_j2d_start(sl_jsys_j2d* j2d) {
    j2d_kit = j2d;
}

extern "C" sl_jsys_j2d* ac_jsys_j2d_kit(void) {
    return j2d_kit;
}

extern "C" void ac_jsys_j2d_position(sl_mat34* out) {
    *out = loaded_position;
}

extern "C" void ac_jsys_j2d_flush(void) {
    if (j2d_kit != NULL)
        (void)sl_jsys_j2d_flush(j2d_kit);
}

extern "C" void ac_jsys_j2d_end_frame(void) {
    if (j2d_kit != NULL)
        sl_jsys_j2d_end_frame(j2d_kit);
}

J2DGrafContext::J2DGrafContext(f32 left, f32 top, f32 right, f32 bottom)
    : mBounds(left, top, left + right, top + bottom), mScissorBounds(left, top, left + right, top + bottom) {
    JUtility::TColor color(-1);
    setColor(color);
    setLineWidth(6);
}

void J2DGrafContext::setPort() {
    setScissor();
    setup2D();

    port_viewport[0] = mBounds.i.x;
    port_viewport[1] = mBounds.i.y;
    port_viewport[2] = mBounds.f.x - mBounds.i.x;
    port_viewport[3] = mBounds.f.y - mBounds.i.y;
    hand_port();
}

// The state J2D's draws need, vertex colors with no texture, no depth and no cull, is the kit's materials. The
// position matrix is loaded, as the game loaded it, and handed with each draw.
void J2DGrafContext::setup2D() {
    (void)load_position(mPosMtx);
}

void J2DGrafContext::setScissor() {
    JGeometry::TBox2f hardBounds(0, 0, 1024, 1000);
    JGeometry::TBox2f newBounds(mScissorBounds);

    mScissorBounds.intersect(hardBounds);
    newBounds.absolute();
    newBounds.addPos(0.0f, -1.0f);

    if (newBounds.intersect(hardBounds)) {
        port_scissor[0] = (u32)newBounds.i.x;
        port_scissor[1] = (u32)newBounds.i.y;
        port_scissor[2] = (u32)newBounds.getWidth();
        port_scissor[3] = (u32)newBounds.getHeight();
    } else {
        port_scissor[0] = 0;
        port_scissor[1] = 0;
        port_scissor[2] = 0;
        port_scissor[3] = 0;
    }
    hand_port();
}

void J2DGrafContext::scissor(const JGeometry::TBox2f& bounds) {
    mScissorBounds = bounds;
}

void J2DGrafContext::place(const JGeometry::TBox2f& bounds) {
    mBounds = bounds;
    mScissorBounds = bounds;
}

void J2DGrafContext::setColor(JUtility::TColor colorTL, JUtility::TColor colorTR, JUtility::TColor colorBR,
                              JUtility::TColor colorBL) {
    mColorTL = colorTL;
    mColorTR = colorTR;
    mColorBR = colorBR;
    mColorBL = colorBL;

    _B0.mType = 1;
    _B0.mSrcFactor = 4;
    _B0.mDestFactor = 5;

    mLinePart.mType = 1;
    mLinePart.mSrcFactor = 4;
    mLinePart.mDestFactor = 5;

    mBoxPart.mType = 1;
    mBoxPart.mSrcFactor = 4;
    mBoxPart.mDestFactor = 5;

    if ((u8)u32(mColorTL) != 0xFF) {
        return;
    }

    _B0.mType = 0;
    _B0.mSrcFactor = 1;
    _B0.mDestFactor = 0;

    if ((u8)u32(mColorBR) != 0xFF) {
        return;
    }

    mLinePart.mType = 0;
    mLinePart.mSrcFactor = 1;
    mLinePart.mDestFactor = 0;

    if ((u8)u32(mColorTR) != 0xFF) {
        return;
    }
    if ((u8)u32(mColorBL) != 0xFF) {
        return;
    }

    mBoxPart.mType = 0;
    mBoxPart.mSrcFactor = 1;
    mBoxPart.mDestFactor = 0;
}

// The width is kept, in sixths of a pixel; the kit's lines are one pixel of the main target wide.
void J2DGrafContext::setLineWidth(u8 width) {
    mLineWidth = width;
}

// The corners in the game's order and colors: the bottom right takes mColorBL and the bottom left mColorBR.
void J2DGrafContext::fillBox(const JGeometry::TBox2f& box) {
    const sl_mat34 position = load_position(mPosMtx);
    sl_jsys_j2d_blend blend;
    if (!kit_blend(mBoxPart, &blend))
        return;
    sl_frame* frame = open_frame();
    if (frame == NULL)
        return;
    const sl_jsys_j2d_vertex corners[4] = { vertex(box.i.x, box.i.y, mColorTL), vertex(box.f.x, box.i.y, mColorTR),
                                            vertex(box.f.x, box.f.y, mColorBL), vertex(box.i.x, box.f.y, mColorBR) };
    (void)sl_jsys_j2d_quad(j2d_kit, frame, &position, corners, blend);
}

// fillBox's corners as a strip of five lines' ends, back to the first, in the box's blend.
void J2DGrafContext::drawFrame(const JGeometry::TBox2f& box) {
    const sl_mat34 position = load_position(mPosMtx);
    sl_jsys_j2d_blend blend;
    if (!kit_blend(mBoxPart, &blend))
        return;
    sl_frame* frame = open_frame();
    if (frame == NULL)
        return;
    const sl_jsys_j2d_vertex strip[5] = { vertex(box.i.x, box.i.y, mColorTL), vertex(box.f.x, box.i.y, mColorTR),
                                          vertex(box.f.x, box.f.y, mColorBL), vertex(box.i.x, box.f.y, mColorBR),
                                          vertex(box.i.x, box.i.y, mColorTL) };
    (void)sl_jsys_j2d_lines(j2d_kit, frame, &position, strip, 5, 1, blend);
}

void J2DGrafContext::line(JGeometry::TVec2f start, JGeometry::TVec2f end) {
    const sl_mat34 position = load_position(mPosMtx);
    sl_jsys_j2d_blend blend;
    if (!kit_blend(mLinePart, &blend))
        return;
    sl_frame* frame = open_frame();
    if (frame == NULL)
        return;
    const sl_jsys_j2d_vertex ends[2] = { vertex(start.x, start.y, mColorTL), vertex(end.x, end.y, mColorBR) };
    (void)sl_jsys_j2d_lines(j2d_kit, frame, &position, ends, 2, 0, blend);
}

void J2DGrafContext::lineTo(JGeometry::TVec2f pos) {
    line(mPrevPos, pos);
    mPrevPos = pos;
}

J2DOrthoGraph::J2DOrthoGraph() : J2DGrafContext(0.0f, 0.0f, 0.0f, 0.0f) {
    setLookat();
}

J2DOrthoGraph::J2DOrthoGraph(f32 left, f32 top, f32 right, f32 bottom, f32 near, f32 far)
    : J2DGrafContext(left, top, right, bottom) {
    mOrtho = JGeometry::TBox2f(0, 0, right, bottom);
    mNear = near;
    mFar = far;
    setLookat();
}

void J2DOrthoGraph::setPort() {
    J2DGrafContext::setPort();
    C_MTXOrtho(mMtx44, mOrtho.i.y, 0.5f + mOrtho.f.y, mOrtho.i.x, mOrtho.f.x, mNear, mFar);
    if (j2d_kit != NULL)
        sl_jsys_j2d_set_ortho(j2d_kit, mOrtho.i.x, mOrtho.i.y, mOrtho.f.x, 0.5f + mOrtho.f.y, mNear, mFar);
}

void J2DOrthoGraph::setOrtho(const JGeometry::TBox2f& bounds, f32 far, f32 near) {
    mOrtho = bounds;
    mNear = -near;
    mFar = -far;
}

// The identity, handed with each draw as the position matrix.
void J2DOrthoGraph::setLookat() {
    PSMTXIdentity(mPosMtx);
}

void J2DOrthoGraph::scissorBounds(JGeometry::TBox2f* out, const JGeometry::TBox2f* src) {
    f32 widthPower = this->getWidthPower();
    f32 heightPower = this->getHeightPower();
    f32 ix = mBounds.i.x >= 0 ? mBounds.i.x : 0;
    f32 iy = mBounds.i.y >= 0 ? mBounds.i.y : 0;
    f32 f0 = ix + widthPower * (src->i.x - mOrtho.i.x);
    f32 f2 = ix + widthPower * (src->f.x - mOrtho.i.x);
    f32 f1 = iy + heightPower * (src->i.y - mOrtho.i.y);
    f32 f3 = iy + heightPower * (src->f.y - mOrtho.i.y);
    out->set(f0, f1, f2, f3);
    out->intersect(mScissorBounds);
}

void J2DDrawLine(f32 x1, f32 y1, f32 x2, f32 y2, JUtility::TColor color, int line_width) {
    J2DOrthoGraph oGrph;
    oGrph.setLineWidth(line_width);
    oGrph.setColor(color);
    oGrph.moveTo(x1, y1);
    oGrph.lineTo(x2, y2);
}

void J2DFillBox(f32 l, f32 t, f32 x, f32 y, JUtility::TColor color) {
    J2DFillBox(JGeometry::TBox2f(l, t, l + x, t + y), color);
}

void J2DFillBox(const JGeometry::TBox2f& box, JUtility::TColor color) {
    J2DOrthoGraph oGrph;
    oGrph.setColor(color);
    oGrph.fillBox(box);
}

void J2DFillBox(f32 l, f32 t, f32 x, f32 y, JUtility::TColor c1, JUtility::TColor c2, JUtility::TColor c3,
                JUtility::TColor c4) {
    J2DFillBox(JGeometry::TBox2f(l, t, l + x, t + y), c1, c2, c3, c4);
}

void J2DFillBox(const JGeometry::TBox2f& box, JUtility::TColor c1, JUtility::TColor c2, JUtility::TColor c3,
                JUtility::TColor c4) {
    J2DOrthoGraph oGrph;
    oGrph.setColor(c1, c2, c3, c4);
    oGrph.fillBox(box);
}

void J2DDrawFrame(f32 l, f32 t, f32 x, f32 y, JUtility::TColor color, u8 line_width) {
    J2DDrawFrame(JGeometry::TBox2f(l, t, l + x, t + y), color, line_width);
}

void J2DDrawFrame(const JGeometry::TBox2f& box, JUtility::TColor color, u8 line_width) {
    J2DOrthoGraph oGrph;
    oGrph.setColor(color);
    oGrph.setLineWidth(line_width);
    oGrph.drawFrame(box);
}
