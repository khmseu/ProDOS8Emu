# Phase 3 Complete: Warm Restart Vector Initialization

Added a helper to initialize the Apple II Control-Reset vector used by ProDOS system programs by writing the restart entrypoint to $03F2/$03F3 and setting the power-up byte ($03F4) to 0xA5.

**Files created/changed:**

- include/prodos8emu/system_loader.hpp
- src/system_loader.cpp
- tests/system_test.cpp

**Functions created/changed:**

- `initWarmStartVector(Apple2Memory& mem, uint16_t entryAddr)`

**Tests created/changed:**

- System test covering warm restart vector initialization (vector + power-up byte)

**Review Status:** APPROVED

**Git Commit Message:**
feat: Initialize Apple II warm restart vector

- Add initWarmStartVector() helper to set $03F2 reset vector and $03F4 marker
- Write entry address little-endian and set power-up byte to 0xA5
- Add system test to verify vector and power-up byte contents
