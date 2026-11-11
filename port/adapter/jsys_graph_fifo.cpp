// JUTGraphFifo's seam (sonarlib docs/steps/m6-10e.md): what the game's JUTGraphFifo.cpp defines, against the game's
// header, unchanged. The first FIFO's GXInit is the glue's, in its init bracket (port/gx_sink.h, ac_gx_init), and a
// later FIFO's base and pointers are its too. The destructor frees the base and drops the FIFO's save and the GP's
// wait, as no GP runs; the game never calls it. It calls nothing of GX.
#include <dolphin/gx.h>
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTGraphFifo.h"

#include <port/gx_sink.h>

bool JUTGraphFifo::sInitiated;
JUTGraphFifo* JUTGraphFifo::sCurrentFifo;
GXBool JUTGraphFifo::mGpStatus[5];

JUTGraphFifo::JUTGraphFifo(u32 size) {
    mSize = ALIGN_NEXT(size, 32);
    if (sInitiated) {
        mFifo = (GXFifoObj*)JKRAllocFromSysHeap(mSize + sizeof(GXFifoObj), 32);
        mBase = mFifo + 1;
        ac_gx_init_fifo(mFifo, mBase, mSize);
    } else {
        /** TODO: Figure out what has sizeof 0xA0. */
        mBase = JKRAllocFromSysHeap(mSize + 0xA0, 32);
        mBase = (void*)ALIGN_NEXT((uaddr)mBase, 32);
        mFifo = (GXFifoObj*)ac_gx_init(mBase, mSize);
        sInitiated = true;
        sCurrentFifo = this;
    }
}

JUTGraphFifo::~JUTGraphFifo() {
    if (sCurrentFifo == this) {
        sCurrentFifo = nullptr;
    }

    JKRFreeToSysHeap(mBase);
}
