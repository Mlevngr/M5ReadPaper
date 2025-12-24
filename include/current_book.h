// Compatibility wrapper to migrate g_current_book to atomic<shared_ptr<BookHandle)>
#pragma once

#include <memory>
#include <atomic>
#include "text/book_handle.h"

// Internal shared pointer variable holding current book. Define in one TU (main.cpp).
// We store a plain std::shared_ptr and use the atomic_load/atomic_store free
// functions (from <atomic>) to access it atomically. Some toolchains do not
// allow std::atomic<std::shared_ptr<T>> as a specialization.
extern std::shared_ptr<BookHandle> __g_current_book_shared;

// Return a shared_ptr to the current book (atomic load). Use this when you need
// to keep the BookHandle alive across a code region.
static inline std::shared_ptr<BookHandle> current_book_shared()
{
    return std::atomic_load(&__g_current_book_shared);
}

// Return a raw pointer to the current book (may be null). This keeps existing
// call-sites that expect a raw pointer working via the g_current_book macro.
static inline BookHandle *current_book_raw()
{
    // Keep a thread-local shared_ptr to extend lifetime of the pointed BookHandle
    // for the duration of the thread between subsequent calls. This avoids a
    // transient dangling pointer that would occur if we returned sp.get() from
    // a temporary shared_ptr that is destroyed on return. Long-term the code
    // should prefer current_book_shared() to obtain a shared_ptr explicitly.
    thread_local std::shared_ptr<BookHandle> __current_book_thread_sp;
    __current_book_thread_sp = std::atomic_load(&__g_current_book_shared);
    return __current_book_thread_sp ? __current_book_thread_sp.get() : nullptr;
}

// Backwards-compatibility: allow old code to keep using `g_current_book` as a
// pointer. This macro expands to the raw pointer returned by current_book_raw().
#define g_current_book (current_book_raw())
