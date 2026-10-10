---
phase: 5
title: "Verification, Tests & Documentation"
status: pending
priority: P1
effort: "2h"
dependencies: ["4"]
---

# Phase 05: Verification, Tests & Documentation

## Overview
Perform full verification: run native unit tests (`test/lunar_calendar`), build simulator (`pio run -e simulator`), build ESP32-C3 firmware (`pio run -e default`), verify flash and RAM metrics, capture simulator screenshots, update `docs/user-guide.md`, and record changes in `CHANGELOG.md`.

## Requirements
- Functional:
  - Native tests pass cleanly via CMake / CTest.
  - `pio run -e simulator` builds cleanly and simulator runs.
  - `pio run -e default` builds cleanly and binary fits within 6.5 MB app partition.
  - `docs/user-guide.md` updated with the two new sleep screens in the sleep screen modes table.
  - `CHANGELOG.md` updated under Added section with descriptive entries for both screens.
- Non-functional:
  - Memory headroom verified on C3 default environment.
  - No heap or stack regressions.

## Related Code Files
- Modify: `docs/user-guide.md`
- Modify: `CHANGELOG.md`

## Success Criteria
- [ ] `pio run -e simulator` succeeds.
- [ ] `pio run -e default` succeeds.
- [ ] Native tests pass.
- [ ] User documentation and CHANGELOG updated.
