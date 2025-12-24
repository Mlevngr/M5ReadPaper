#pragma once

#include <stdbool.h>

// Check if a force reindex is pending
bool isForceReindexPending();

// Request a force reindex (will be handled in next work cycle)
void requestForceReindex();

// Block (up to timeout_ms) until force-reindex has started
// Returns true if started, false on timeout
bool waitForForceReindexStart(unsigned long timeout_ms);

// Run a single background-index "work cycle" synchronously in the caller's
// context. This processes a pending force-reindex request (if any) and, if
// possible, performs one incremental indexing segment for the current book.
// Returns true if any useful work was done.
// This function is called from the main loop, not from a separate task.
bool runBackgroundIndexWorkCycle();
