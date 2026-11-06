#include "JSystem/JUtility/JUTAssertion.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "dolphin/os.h"
#include "dolphin/os/OSArena.h"
#include "dolphin/os/OSAlloc.h"
#include "dolphin/os/OSMemory.h"
#include "dolphin/os/OSUtil.h"
#include "dolphin/os.h"

JKRHeap* JKRHeap::sSystemHeap;
JKRHeap* JKRHeap::sCurrentHeap;
JKRHeap* JKRHeap::sRootHeap;
JKRHeapErrorHandler* JKRHeap::mErrorHandler;
void* JKRHeap::mCodeStart;
void* JKRHeap::mCodeEnd;
void* JKRHeap::mUserRamStart;
void* JKRHeap::mUserRamEnd;
u32 JKRHeap::mMemorySize;

bool JKRHeap::sDefaultFillFlag = true;

JKRHeap::JKRHeap(void* data, u32 size, JKRHeap* heap, bool errorFlag)
    : JKRDisposer(), mChildTree(this), mDisposerList() {
    OSInitMutex(&mMutex);
    mSize = size;
    mStart = (u8*)data;
    mEnd = ((u8*)data + size);
    if (heap == nullptr) {
        becomeSystemHeap();
        becomeCurrentHeap();
    } else {
        heap->mChildTree.appendChild(&mChildTree);
        if (sSystemHeap == sRootHeap)
            becomeSystemHeap();
        if (sCurrentHeap == sRootHeap)
            becomeCurrentHeap();
    }
    mErrorFlag = errorFlag;
    if (mErrorFlag == true && mErrorHandler == nullptr)
        mErrorHandler = JKRDefaultMemoryErrorRoutine;
    mDebugFill = sDefaultFillFlag;
    mInitFlag = false;
}

JKRHeap::~JKRHeap() {
    mChildTree.getParent()->removeChild(&mChildTree);
    JSUTree<JKRHeap>* nextRootHeap = sRootHeap->mChildTree.getFirstChild();
    if (sCurrentHeap == this)
        sCurrentHeap = !nextRootHeap ? sRootHeap : nextRootHeap->getObject();

    if (sSystemHeap == this)
        sSystemHeap = !nextRootHeap ? sRootHeap : nextRootHeap->getObject();
}

bool JKRHeap::initArena(char** outUserRamStart, u32* outUserRamSize, int numHeaps) {
    void* arenaLo = OSGetArenaLo();
    void* arenaHi = OSGetArenaHi();
    if (arenaLo == arenaHi) {
        return false;
    }
    void* arenaStart = OSInitAlloc(arenaLo, arenaHi, numHeaps);
    arenaHi = (u8*)OSRoundDown32B(arenaHi);
    arenaLo = (u8*)OSRoundUp32B(arenaStart);
    u8* start = (u8*)OSPhysicalToCached(0);
    mCodeStart = (u8*)start;
    mCodeEnd = (u8*)arenaLo;
    mUserRamStart = (u8*)arenaLo;
    mUserRamEnd = (u8*)arenaHi;
    mMemorySize = *(u32*)((start + 0x28));
    OSSetArenaLo(arenaHi);
    OSSetArenaHi(arenaHi);
    *outUserRamStart = (char*)arenaLo;
    *outUserRamSize = (uaddr)arenaHi - (uaddr)arenaLo;
    return true;
}

JKRHeap* JKRHeap::becomeSystemHeap() {
    JKRHeap* old = sSystemHeap;
    sSystemHeap = this;
    return old;
}

JKRHeap* JKRHeap::becomeCurrentHeap() {
    JKRHeap* old = sCurrentHeap;
    sCurrentHeap = this;
    return old;
}

#ifndef SONAR_PORT /* sonarlib port: unused, and it calls a function the game does not have */
void JKRHeap::destroy(JKRHeap* heap) {
    JUT_ASSERT(heap != 0);
    heap->destroy();
}
#endif

void* JKRHeap::alloc(u32 byteCount, int padding, JKRHeap* heap) {
    void* memory = nullptr;
    if (heap) {
        memory = heap->do_alloc(byteCount, padding);
    } else if (sCurrentHeap) {
        memory = sCurrentHeap->do_alloc(byteCount, padding);
    }
    return memory;
}

void* JKRHeap::alloc(u32 byteCount, int padding) {
    JUT_WARNING_F(!mInitFlag, "alloc %x byte in heap %x", byteCount, this);
    return do_alloc(byteCount, padding);
}

void JKRHeap::free(void* memory, JKRHeap* heap) {
    if ((heap) || (heap = findFromRoot(memory), heap)) {
        heap->free(memory);
    }
}

void JKRHeap::free(void* memory) {
    JUT_WARNING_F(!mInitFlag, "free %x in heap %x", memory, this);
    do_free(memory);
}

void JKRHeap::callAllDisposer() {
    JSUListIterator<JKRDisposer> iterator;
    while (iterator = mDisposerList.getFirst(), iterator != mDisposerList.getEnd()) {
        iterator->~JKRDisposer();
    }
}

void JKRHeap::freeAll() {
    JUT_WARNING_F(!mInitFlag, "freeAll in heap %x", this);
    do_freeAll();
}

void JKRHeap::freeTail() {
    JUT_WARNING_F(!mInitFlag, "freeTail in heap %x", this);
    do_freeTail();
}

s32 JKRHeap::resize(void* memoryBlock, u32 newSize) {
    JUT_WARNING_F(!mInitFlag, "resize block %x into %x in heap %x", memoryBlock, newSize, this);
    return do_resize(memoryBlock, newSize);
}

s32 JKRHeap::getSize(void* memoryBlock, JKRHeap* heap) {
    if (heap == nullptr && (heap = findFromRoot(memoryBlock), heap == nullptr)) {
        return -1;
    } else
        return heap->getSize(memoryBlock);
}

s32 JKRHeap::getSize(void* memoryBlock) {
    return do_getSize(memoryBlock);
}
s32 JKRHeap::getFreeSize() {
    return do_getFreeSize();
}
s32 JKRHeap::getTotalFreeSize() {
    return do_getTotalFreeSize();
}

s32 JKRHeap::changeGroupID(u8 newGroupID) {
    JUT_WARNING_F(!mInitFlag, "change heap ID into %x in heap %x", newGroupID, this);
    return do_changeGroupID(newGroupID);
}

u8 JKRHeap::getCurrentGroupId() {
    return do_getCurrentGroupId();
}

JKRHeap* JKRHeap::findFromRoot(void* ptr) {
    if (sRootHeap != nullptr)
        return sRootHeap->find(ptr);

    return nullptr;
}

JKRHeap* JKRHeap::find(void* memory) const {
    if ((mStart <= memory) && (memory <= mEnd)) {
        if (mChildTree.getNumChildren() != 0) {
            for (JSUTreeIterator<JKRHeap> iterator(mChildTree.getFirstChild()); iterator != mChildTree.getEndChild();
                 ++iterator) {
                JKRHeap* result = iterator->find(memory);
                if (result) {
                    return result;
                }
            }
        }
        return const_cast<JKRHeap*>(this);
    }
    return nullptr;
}

JKRHeap* JKRHeap::findAllHeap(void* memory) const {
    if (mChildTree.getNumChildren() != 0) {
        for (JSUTreeIterator<JKRHeap> iterator(mChildTree.getFirstChild()); iterator != mChildTree.getEndChild();
             ++iterator) {
            JKRHeap* result = iterator->findAllHeap(memory);
            if (result) {
                return result;
            }
        }
    }

    if (mStart <= memory && memory < mEnd) {
        return const_cast<JKRHeap*>(this);
    }

    return nullptr;
}

// generates __as__25JSUTreeIterator<7JKRHeap>FP17JSUTree<7JKRHeap> and __ct__25JSUTreeIterator<7JKRHeap>Fv, remove this
void JKRHeap::dispose_subroutine(uaddr begin, uaddr end) {
    JSUListIterator<JKRDisposer> last_iterator;
    JSUListIterator<JKRDisposer> next_iterator;
    JSUListIterator<JKRDisposer> iterator;
    for (iterator = mDisposerList.getFirst(); iterator != mDisposerList.getEnd(); iterator = next_iterator) {
        JKRDisposer* disposer = iterator.getObject();

        if ((void*)begin <= disposer && disposer < (void*)end) {
            disposer->~JKRDisposer();
            if (last_iterator == nullptr) {
                next_iterator = mDisposerList.getFirst();
            } else {
                next_iterator = last_iterator;
                next_iterator++;
            }
        } else {
            last_iterator = iterator;
            next_iterator = iterator;
            next_iterator++;
        }
    }
}

bool JKRHeap::dispose(void* memory, u32 size) {
    uaddr begin = (uaddr)memory;
    uaddr end = (uaddr)memory + size;
    dispose_subroutine(begin, end);
    return false;
}

void JKRHeap::dispose(void* begin, void* end) {
    dispose_subroutine((uaddr)begin, (uaddr)end);
}

void JKRHeap::dispose() {
    JSUListIterator<JKRDisposer> iterator;
    while (iterator = mDisposerList.getFirst(), iterator != mDisposerList.getEnd()) {
        iterator->~JKRDisposer();
    }
}

void JKRHeap::copyMemory(void* dst, void* src, u32 size) {
    u32 count = (size + 3) / 4;

    u32* dst_32 = (u32*)dst;
    u32* src_32 = (u32*)src;
    while (count > 0) {
        *dst_32 = *src_32;
        dst_32++;
        src_32++;
        count--;
    }
}

void JKRDefaultMemoryErrorRoutine(void* heap, u32 size, int alignment) {
    // OSReport("Error: Cannot allocate memory %d(0x%x)byte in %d byte alignment from %08x\n", size, size, alignment,
    // heap);
    OSErrorLine(710, "abort\n");
}

JKRHeapErrorHandler* JKRHeap::setErrorHandler(JKRHeapErrorHandler* newHandler) {
    JKRHeapErrorHandler* oldHandler = mErrorHandler;
    if (!newHandler) {
        newHandler = JKRDefaultMemoryErrorRoutine;
    }
    mErrorHandler = newHandler;
    return oldHandler;
}

bool JKRHeap::isSubHeap(JKRHeap* heap) const {
    if (!heap)
        return false;

    if (mChildTree.getNumChildren() != 0) {
        JSUTreeIterator<JKRHeap> iterator;
        for (iterator = mChildTree.getFirstChild(); iterator != mChildTree.getEndChild(); ++iterator) {
            if (iterator.getObject() == heap) {
                return true;
            }

            if (iterator.getObject()->isSubHeap(heap)) {
                return true;
            }
        }
    }

    return false;
}

#ifdef SONAR_PORT /* sonarlib port: the operator new rule (sonarlib docs/porting-model.md, Statically linked modules) */

#include <sonar/base/alloc.h>
#include <sonar/dolphin_os/os.h>
#include <sonar/os.h>

#include <cstddef>
#include <stdint.h>
#include <stdlib.h>

/* The host's allocator, under its own names: names.h gives malloc and free to libc64. */
#pragma push_macro("malloc")
#pragma push_macro("free")
#undef malloc
#undef free
static void* host_alloc(size_t size, size_t alignment) {
    void* memory = nullptr;
    if (alignment < alignof(std::max_align_t)) {
        alignment = alignof(std::max_align_t);
    }
    if (size == 0) {
        size = 1;
    }
#ifdef _WIN32
    memory = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&memory, alignment, size) != 0) {
        memory = nullptr;
    }
#endif
    return memory;
}

static void host_free(void* memory) {
#ifdef _WIN32
    _aligned_free(memory);
#else
    free(memory);
#endif
}
#pragma pop_macro("free")
#pragma pop_macro("malloc")

/* A thread inside sonarlib's host scope (a platform library's allocation, such as the GPU driver's), a thread the
   game does not own, or a game thread before the game's heaps exist, allocates from the host. */
static void* game_new(size_t byteCount, int alignment) {
    if (sl_mem_host_scope() || sl_os_thread_current().id == 0 || JKRHeap::sCurrentHeap == nullptr) {
        return host_alloc(byteCount, (size_t)(alignment < 0 ? -alignment : alignment));
    }
    return JKRHeap::alloc(byteCount, alignment, nullptr);
}

/* The address picks the allocator: JKR's heaps lie in main memory, the host's allocations never do. */
static void game_delete(void* memory) {
    const u8* base = (const u8*)sl_dolphin_os_memory_base();
    if (base != nullptr && (const u8*)memory >= base && (const u8*)memory < base + sl_dolphin_os_memory_size()) {
        JKRHeap::free(memory, nullptr);
    } else {
        host_free(memory);
    }
}

void* operator new(size_t byteCount) {
    return game_new(byteCount, 4);
}
void* operator new(size_t byteCount, int alignment) {
    return game_new(byteCount, alignment);
}
void* operator new(size_t byteCount, std::align_val_t alignment) {
    return game_new(byteCount, (int)alignment);
}
void* operator new(size_t byteCount, JKRHeap* heap, int alignment) {
    return JKRHeap::alloc(byteCount, alignment, heap);
}

void* operator new[](size_t byteCount) {
    return game_new(byteCount, 4);
}
void* operator new[](size_t byteCount, int alignment) {
    return game_new(byteCount, alignment);
}
void* operator new[](size_t byteCount, std::align_val_t alignment) {
    return game_new(byteCount, (int)alignment);
}
void* operator new[](size_t byteCount, JKRHeap* heap, int alignment) {
    return JKRHeap::alloc(byteCount, alignment, heap);
}

void operator delete(void* memory) {
    game_delete(memory);
}
void operator delete(void* memory, size_t) noexcept {
    game_delete(memory);
}
void operator delete(void* memory, std::align_val_t) noexcept {
    game_delete(memory);
}
void operator delete(void* memory, size_t, std::align_val_t) noexcept {
    game_delete(memory);
}
void operator delete[](void* memory) {
    game_delete(memory);
}
void operator delete[](void* memory, size_t) noexcept {
    game_delete(memory);
}
void operator delete[](void* memory, std::align_val_t) noexcept {
    game_delete(memory);
}
void operator delete[](void* memory, size_t, std::align_val_t) noexcept {
    game_delete(memory);
}
#else
void* operator new(size_t byteCount) {
    return JKRHeap::alloc(byteCount, 4, nullptr);
}
void* operator new(size_t byteCount, int alignment) {
    return JKRHeap::alloc(byteCount, alignment, nullptr);
}
void* operator new(size_t byteCount, JKRHeap* heap, int alignment) {
    return JKRHeap::alloc(byteCount, alignment, heap);
}

void* operator new[](size_t byteCount) {
    return JKRHeap::alloc(byteCount, 4, nullptr);
}
void* operator new[](size_t byteCount, int alignment) {
    return JKRHeap::alloc(byteCount, alignment, nullptr);
}
void* operator new[](size_t byteCount, JKRHeap* heap, int alignment) {
    return JKRHeap::alloc(byteCount, alignment, heap);
}

// this is not needed without the other pragma and asm bs
void operator delete(void* memory) {
    JKRHeap::free(memory, nullptr);
}
void operator delete[](void* memory) {
    JKRHeap::free(memory, nullptr);
}
#endif

/*JKRHeap::TState::TState(const JKRHeap::TState::TArgument &arg, const JKRHeap::TState::TLocation &location)
{
    // UNUSED FUNCTION
}

JKRHeap::TState::TState(const JKRHeap::TState &other, bool p2)
{
    // UNUSED FUNCTION
}

JKRHeap::TState::TState(const JKRHeap::TState &other, const JKRHeap::TState::TLocation &location, bool p3)
{
    // UNUSED FUNCTION
}*/

JKRHeap::TState::~TState() {
    // Unused, however might need it
}

void JKRHeap::state_register(JKRHeap::TState* p, u32) const {
    JUT_ASSERT(p != 0);
    JUT_ASSERT(p->getHeap() == this);
}

bool JKRHeap::state_compare(const JKRHeap::TState& r1, const JKRHeap::TState& r2) const {
    JUT_ASSERT(r1.getHeap() == r2.getHeap());
    return (r1.getCheckCode() == r2.getCheckCode());
}

// fabricated, but probably matches(except for line numbers)
void JKRHeap::state_dumpDifference(const JKRHeap::TState& r1, const JKRHeap::TState& r2) {
    JUT_LOG_F("heap       : %p / %p", r1.getHeap(), r2.getHeap());
    JUT_LOG_F("check-code : 0x%08x / 0x%08x", r1.getCheckCode(), r2.getCheckCode());
    JUT_LOG_F("id         : 0x%08x / 0x%08x", r1.getId(), r2.getId());
    JUT_LOG_F("used size  : %10u / %10u", r1.getUsedSize(), r2.getUsedSize());
}

void JKRHeap::state_dump(const TState& state) const {
    JUT_LOG_F("check-code : 0x%08x", state.getCheckCode());
    JUT_LOG_F("id         : 0x%08x", state.getId());
    JUT_LOG_F("used size  : %u", state.getUsedSize());
}
