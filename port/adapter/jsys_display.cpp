// JFWDisplay's seam (port/jsys_display.h; sonarlib docs/steps/m6-10e.md): every function the game's JFWDisplay.cpp
// defines, e_mtx among them, against the game's header, unchanged. The members that use no GX are the game's text:
// the buffers' indices, the retrace waits, the fader, the process bar. What the file handed the console's graphics
// goes to sonarlib's kits/jsys instead: the EFB's clear, recorded into the frame the GX glue holds open
// (port/gx_sink.h, ac_frame_task) at the EFB's pixel format, as preGX set it. A display copy clears the EFB to the
// copy's clear, recorded at the end of beginRender, and clearEfb clears it to its color. The GX that only the
// reference's program needs, the copy's set-up and the copy, preGX's state, clearEfb's quad and the waits, goes to its
// hooks (port/reference.h, port/display_reference.h). diagnoseGpHang and the clear's z texture, which only GX used,
// are dropped. GX's draw done called JUTVideo's callback at once, which the seam calls after each
// (ac_jsys_video_draw_done). It calls nothing of GX.
#include <libc/string.h>
#include <dolphin/vi.h>
#include "_mem.h"
#include "JSystem/J2D/J2DGrafContext.h"
#include "JSystem/JUtility/JUTAssertion.h"
#include "JSystem/JUtility/JUTConsole.h"
#include "JSystem/JUtility/JUTDbPrint.h"
#include "JSystem/JUtility/JUTVideo.h"
#include "JSystem/JUtility/JUTProcBar.h"
#include "JSystem/JFramework/JFWDisplay.h"

#include <port/display_reference.h>
#include <port/gx_sink.h>
#include <port/jsys_display.h>
#include <port/reference.h>

#include <sonar/jsys/display.h>
#include <sonar/render.h>

// Sources: https://github.com/zeldaret/tp/blob/master/libs/JSystem/JFramework/JFWDisplay.cpp
// https://github.com/kiwi515/ogws/blob/master/src/egg/core/eggAsyncDisplay.cpp
// gpHang: https://github.com/valentinaslover/paper-mar/blob/master/source/sdk/DEMOInit.c#L280

GC_Mtx e_mtx = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };

JFWDisplay* JFWDisplay::sManager;

extern void JFWThreadAlarmHandler(OSAlarm*, OSContext*);
extern void JFWGXAbortAlarmHandler(OSAlarm*, OSContext*);
void waitForTick(u32, u16);

namespace {

// The display copy's clear, as prepareCopyDisp gave it last; GXInit's copy clear before the first (GXInit.c).
GXColor copy_color = { 64, 64, 64, 255 };
u32 copy_z = 0xFFFFFF;

// The EFB's pixel format, as preGX set it last; GXInit's before the first.
sl_gx_pixel_format efb_format = SL_GX_PF_RGB8_Z24;

// A display copy cleared the EFB since the last beginRender, to these.
bool clear_pending = false;
GXColor pending_color = { 0, 0, 0, 0 };
u32 pending_z = 0;

// The frame's clear, the last recorded, for ac_jsys_display_reclear.
bool cleared = false;
sl_gx_pixel_format clear_format = SL_GX_PF_RGB8_Z24;
u8 clear_color[4] = { 0, 0, 0, 0 };
u32 clear_z = 0;

// Keeps the clear as the frame's, at the EFB's format, and records it into the open frame after what the reference
// gave it before, whether or not there is a frame to record into.
void record_clear(u8 red, u8 green, u8 blue, u8 alpha, u32 z) {
    sl_frame* frame;
    cleared = true;
    clear_format = efb_format;
    clear_color[0] = red;
    clear_color[1] = green;
    clear_color[2] = blue;
    clear_color[3] = alpha;
    clear_z = z;
    if (ac_frame_task(&frame))
        (void)sl_jsys_display_clear(frame, clear_format, clear_color, clear_z);
}

// The end of beginRender's: the clear of the copy since the last, when there was one.
void record_pending_clear() {
    if (!clear_pending)
        return;
    clear_pending = false;
    record_clear(pending_color.r, pending_color.g, pending_color.b, pending_color.a, pending_z);
}

} // namespace

inline void JFWDrawDoneAlarm() {
    OSAlarm alarm;
    OSCreateAlarm(&alarm);
    OSSetAlarm(&alarm, (OS_TIMER_CLOCK), JFWGXAbortAlarmHandler);
    ac_reference_sync(AC_REFERENCE_DRAW_DONE);
    ac_jsys_video_draw_done();
    OSCancelAlarm(&alarm);
}

void JFWDisplay::ctor_subroutine(const GXRenderModeObj* rmode, bool enableAlpha) {
    mEnableAlpha = enableAlpha;
    mClamp = GX_CLAMP_TOP | GX_CLAMP_BOTTOM;
    mClearColor.set(0, 0, 0, 0);

    mZClear = 0xFFFFFF;
    mRMode = (rmode) ? rmode : JUTVideo::sManager->getRenderMode();

    mGamma = 0;
    mFader = nullptr;
    mFrameRate = 1;
    mTickRate = 0;
    mCombinationRatio = 0.0f;

    mFrameTime = 0;
    mStartTick = OSGetTick();
    mVideoFrameTime = 0;
    mDrawingXfbNo = 0;
    mIsSingleXfb = false;
    mDrawDoneMethod = UNK_METHOD_0;
    clearEfb_init();
    JUTProcBar::create();
    JUTProcBar::clear();
}

JFWDisplay::JFWDisplay(const _GXRenderModeObj* rmode, JKRHeap* heap, JUTXfb::EXfbNumber bufferCount, bool p3) {
    ctor_subroutine(rmode, p3);
    mXfb = JUTXfb::createManager(rmode, heap, bufferCount);
}

JFWDisplay::~JFWDisplay() {
    waitBlanking(2);
    JUTProcBar::destroy();
    JUTXfb::destroyManager();
}

JFWDisplay* JFWDisplay::createManager(const GXRenderModeObj* rmode, JKRHeap* heap, JUTXfb::EXfbNumber bufferCount,
                                      bool p4) {
    JUT_CONFIRM_MESSAGE(sManager == 0);

    if (sManager == nullptr)
        sManager = new JFWDisplay(rmode, heap, bufferCount, p4);

    return sManager;
}

void JFWDisplay::destroyManager() {
    JUT_CONFIRM_MESSAGE(sManager);
    delete sManager;
    sManager = nullptr;
}

void callDirectDraw() {
    JUTChangeFrameBuffer(JUTXfb::getManager()->getDrawingXfb(), JUTVideo::getManager()->getEfbHeight(),
                         JUTVideo::getManager()->getFbWidth());
    JUTAssertion::flushMessage();
}

void JFWDisplay::prepareCopyDisp() {
    GXRenderModeObj* rmode = JUTVideo::getManager()->getRenderMode();
    u16 width = (u16)JUTVideo::getManager()->getFbWidth();
    u16 height = (u16)JUTVideo::getManager()->getEfbHeight();
    u16 xfbHeight = (u16)JUTVideo::getManager()->getXfbHeight();
    ac_reference_copy copy;

    copy.clear = mClearColor;
    copy.z = mZClear;
    copy.mode = rmode;
    copy.width = width;
    copy.efb_height = height;
    copy.xfb_height = xfbHeight;
    copy.clamp = mClamp;
    copy.gamma = mGamma;
    copy.enable_alpha = mEnableAlpha;
    copy_color = mClearColor;
    copy_z = mZClear;
    ac_reference_prepare_copy(&copy);
    VIFlush();
}

void JFWDisplay::drawendXfb_single() {
    JUTXfb* manager = JUTXfb::getManager();
    if (manager->getDrawingXfbIndex() >= 0) {
        prepareCopyDisp();
        JFWDrawDoneAlarm();
        ac_reference_sync(AC_REFERENCE_FLUSH);
        manager->setDrawnXfbIndex(manager->getDrawingXfbIndex());
    }
}

void JFWDisplay::exchangeXfb_double() {
    JUTXfb* xfbMng = JUTXfb::getManager();

    if (xfbMng->getDrawnXfbIndex() == xfbMng->getDisplayingXfbIndex()) {
        if (xfbMng->getDrawingXfbIndex() >= 0) {
            prepareCopyDisp();
            ac_reference_copy_display(xfbMng->getDrawingXfb());
            ac_jsys_display_copied();
            if (mDrawDoneMethod == UNK_METHOD_0) {
                ac_reference_sync(AC_REFERENCE_DRAW_DONE);
                ac_jsys_video_draw_done();
                JUTVideo::dummyNoDrawWait();
            } else {
                JUTVideo::drawDoneStart();
            }

            if (mDrawDoneMethod == UNK_METHOD_0) {
                callDirectDraw();
            }
        }
        int cur_xfb_index = xfbMng->getDrawingXfbIndex();
        xfbMng->setDrawnXfbIndex(cur_xfb_index);
        xfbMng->setDrawingXfbIndex(cur_xfb_index >= 0 ? cur_xfb_index ^ 1 : 0);
    } else {
        clearEfb(JUtility::TColor(0, 0, 0, 0xff));
        if (xfbMng->getDrawingXfbIndex() < 0) {
            xfbMng->setDrawingXfbIndex(0);
        }
    }
}

void JFWDisplay::exchangeXfb_triple() {
    JUTXfb* xfbMng = JUTXfb::getManager();

    if (xfbMng->getDrawingXfbIndex() >= 0) {
        callDirectDraw();
    }

    xfbMng->setDrawnXfbIndex(xfbMng->getDrawingXfbIndex());

    s16 drawing_idx = xfbMng->getDrawingXfbIndex() + 1;
    do {
        if (drawing_idx >= 3 || drawing_idx < 0) {
            drawing_idx = 0;
        }
    } while (drawing_idx == xfbMng->getDisplayingXfbIndex());
    xfbMng->setDrawingXfbIndex(drawing_idx);
}

void JFWDisplay::copyXfb_triple() {
    JUTXfb* xfbMng = JUTXfb::getManager();

    if (xfbMng->getDrawingXfbIndex() >= 0) {
        prepareCopyDisp();
        ac_reference_copy_display(xfbMng->getDrawingXfb());
        ac_jsys_display_copied();
        ac_reference_sync(AC_REFERENCE_PIX_MODE_SYNC);
    }
}

void JFWDisplay::preGX() {
    ac_reference_pre_gx(mRMode, mEnableAlpha);

    if (mRMode->aa) {
        efb_format = SL_GX_PF_RGB565_Z16;
    } else {
        if (mEnableAlpha) {
            efb_format = SL_GX_PF_RGBA6_Z24;
        } else {
            efb_format = SL_GX_PF_RGB8_Z24;
        }
    }
}

void JFWDisplay::endGX() {
    u32 width = JUTVideo::getManager()->getFbWidth();
    u32 height = JUTVideo::getManager()->getEfbHeight();

    J2DOrthoGraph ortho(0.0f, 0.0f, width, height, -1.0f, 1.0f);

    if (mFader != nullptr) {
        ortho.setPort();
        mFader->control();
    }

    JUTDbPrint::getManager()->flush();

    if (JUTConsoleManager::getManager() != nullptr) {
        JUTConsoleManager::getManager()->draw();
    }

    ortho.setPort();
    JUTProcBar::getManager()->draw();

    if (mDrawDoneMethod != UNK_METHOD_0 || JUTXfb::getManager()->getBufferNum() == 1) {
        JUTAssertion::flushMessage_dbPrint();
    }
    ac_reference_sync(AC_REFERENCE_FLUSH);
}

void JFWDisplay::beginRender() {
    JUTProcBar::getManager()->wholeLoopEnd();
    JUTProcBar::getManager()->wholeLoopStart();
    JUTProcBar::getManager()->idleStart();

    waitForTick(mTickRate, mFrameRate);
    JUTVideo::getManager()->waitRetraceIfNeed();

    u32 tick = OSGetTick();
    mFrameTime = tick - mStartTick;
    mStartTick = tick;
    mVideoFrameTime = mStartTick - JUTVideo::getVideoLastTick();

    JUTProcBar::getManager()->idleEnd();
    JUTProcBar::getManager()->gpStart();

    JUTXfb* xfbMgr = JUTXfb::getManager();
    switch (xfbMgr->getBufferNum()) {
        case 1:
            if (xfbMgr->getSDrawingFlag() != 2) {
                xfbMgr->setSDrawingFlag(1);
                clearEfb(JUtility::TColor(0, 0, 0, 0xff));
            } else {
                xfbMgr->setSDrawingFlag(1);
            }
            xfbMgr->setDrawingXfbIndex(mDrawingXfbNo);
            break;
        case 2:
            exchangeXfb_double();
            break;
        case 3:
            exchangeXfb_triple();
            break;
        default:
            break;
    }

    preGX();
    record_pending_clear();
}

void JFWDisplay::endRender() {
    endGX();

    switch (JUTXfb::getManager()->getBufferNum()) {
        case 1:
            drawendXfb_single();
        case 2:
            break;
        case 3:
            copyXfb_triple();
        default:
            break;
    }

    JUTProcBar::getManager()->cpuStart();
    calcCombinationRatio();
}

void JFWDisplay::endFrame() {
    JUTProcBar::getManager()->cpuEnd();
    JUTProcBar::getManager()->gpWaitStart();

    switch (JUTXfb::getManager()->getBufferNum()) {
        case 1:
            break;
        case 2:
            JFWDrawDoneAlarm();
            ac_reference_sync(AC_REFERENCE_FLUSH);
            break;
        case 3:
            JFWDrawDoneAlarm();
            ac_reference_sync(AC_REFERENCE_FLUSH);
            break;
        default:
            break;
    }

    JUTProcBar::getManager()->gpWaitEnd();
    JUTProcBar::getManager()->gpEnd();

    static u32 prevFrame = VIGetRetraceCount();
    u32 retrace_cnt = VIGetRetraceCount();
    JUTProcBar::getManager()->setCostFrame(retrace_cnt - prevFrame);
    prevFrame = retrace_cnt;
}

void JFWDisplay::waitBlanking(int p1) {
    while (p1-- > 0) {
        waitForTick(mTickRate, mFrameRate);
    }
}

void waitForTick(u32 p1, u16 p2) {
    if (p1 != 0) {
        static s64 nextTick = OSGetTime();
        s64 time = OSGetTime();
        while (time < nextTick) {
            JFWDisplay::getManager()->threadSleep((nextTick - time));
            time = OSGetTime();
        }
        nextTick = time + p1;
    } else {
        static u32 nextCount = VIGetRetraceCount();
        u32 uVar1 = (p2 == 0) ? 1 : p2;
        OSMessage msg;
        do {
            if (!OSReceiveMessage(JUTVideo::getManager()->getMessageQueue(), &msg, OS_MESSAGE_BLOCK)) {
                msg = 0;
            }
        } while (((saddr)msg - (int)nextCount) < 0);
        nextCount = (saddr)msg + uVar1;
    }
}

void JFWThreadAlarmHandler(OSAlarm* p_alarm, OSContext* p_ctx) {
    JFWAlarm* alarm = static_cast<JFWAlarm*>(p_alarm);
    OSResumeThread(alarm->getThread());
}

void JFWDisplay::threadSleep(s64 time) {
    JFWAlarm alarm;
    alarm.createAlarm();
    alarm.setThread(OSGetCurrentThread());
    s32 status = OSDisableInterrupts();

    OSSetAlarm(&alarm, time, JFWThreadAlarmHandler);
    OSSuspendThread(alarm.getThread());
    OSRestoreInterrupts(status);
}

void* JFWDisplay::changeToSingleXfb(int index) {
    JUTXfb* xfb = JUTXfb::getManager();
    s16 xfbNo = !index ? 1 : 0;

    // Check if xfb is single buffered
    if (xfb->getBufferNum() != 2) {
        return nullptr;
    }

    if (mIsSingleXfb) {
        return nullptr;
    }

    VIWaitForRetrace();

    if (xfbNo != xfb->getDisplayingXfbIndex()) {
        u32 xfbSize = xfb->accumeXfbSize();
        DCInvalidateRange(xfb->getDrawingXfb(), xfbSize);
        memcpy(xfb->getDrawingXfb(), xfb->getDisplayingXfb(), xfbSize);
        DCStoreRange(xfb->getDrawingXfb(), xfbSize);
        xfb->setDrawnXfbIndex(xfb->getDrawingXfbIndex());
        VISetNextFrameBuffer(xfb->getDrawingXfb());
        VIFlush();
        VIWaitForRetrace();
    }

    xfb->setSDrawingFlag(99);
    xfb->setBufferNum(1);
    mDrawingXfbNo = xfbNo;
    mIsSingleXfb = true;
    return xfb->getXfb(index);
}

void* JFWDisplay::changeToDoubleXfb() {
    JUTXfb* xfb = JUTXfb::getManager();

    // Check if xfb is not single buffered already
    if (!mIsSingleXfb) {
        return nullptr;
    }

    VIWaitForRetrace();
    if (xfb->getSDrawingFlag() != 0) {
        VIWaitForRetrace();
    }

    s16 xfbNo = !mDrawingXfbNo ? 1 : 0;
    xfb->setDrawnXfbIndex(mDrawingXfbNo);
    xfb->setDisplayingXfbIndex(xfbNo);
    xfb->setDrawingXfbIndex(xfbNo);
    xfb->setBufferNum(2);
    mIsSingleXfb = false;
    return xfb->getXfb(xfbNo);
}

void JFWDisplay::clearEfb_init() {
}

void JFWDisplay::clearEfb(GXColor color) {
    record_clear(color.r, color.g, color.b, mEnableAlpha ? 0 : color.a, 0xFFFFFF);
    ac_reference_clear_efb(mRMode, color, mEnableAlpha);
}

void JFWDisplay::calcCombinationRatio() {
    u32 vidInterval = JUTVideo::getVideoInterval();
    s32 unk30 = mFrameTime * 2;

    s32 i = vidInterval;
    for (; i < unk30; i += vidInterval) {}

    s32 tmp = (i - unk30) - mVideoFrameTime;
    if (tmp < 0) {
        tmp += vidInterval;
    }
    mCombinationRatio = (f32)tmp / (f32)mFrameTime;
    if (mCombinationRatio > 1.0f) {
        mCombinationRatio = 1.0f;
    }
}

void JFWGXAbortAlarmHandler(OSAlarm* param_0, OSContext* param_1) {
    ac_reference_sync(AC_REFERENCE_ABORT_FRAME);
    ac_reference_sync(AC_REFERENCE_SET_DRAW_DONE);
    ac_jsys_video_draw_done();
}

extern "C" void ac_jsys_display_copied(void) {
    pending_color = copy_color;
    pending_z = copy_z;
    clear_pending = true;
}

extern "C" void ac_jsys_display_reclear(sl_frame* frame) {
    if (frame != NULL && cleared)
        (void)sl_jsys_display_clear(frame, clear_format, clear_color, clear_z);
}
