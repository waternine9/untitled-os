#ifndef SLEEP_H
#define SLEEP_H

// STUB
static inline void Sleep(size_t ms) {
  for (int j = 0; j < 100 * ms; j++) {
    // TODO: Make a sleep function
    IO_Wait();
  }
}

#endif