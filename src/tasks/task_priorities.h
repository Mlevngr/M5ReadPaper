#pragma once
// Centralized task priority definitions for ReaderPaper
// Higher number = higher priority. Adjust centrally to tune scheduling.

// Lowest: background indexing
#define PRIO_INDEX 0

// Main monitoring task
#define PRIO_MAIN 1

// Device interrupts and state machine (medium)
#define PRIO_DEVICE 3
#define PRIO_STATE 3

// Display push should be high priority to keep UI responsive
#define PRIO_DISPLAY 2
