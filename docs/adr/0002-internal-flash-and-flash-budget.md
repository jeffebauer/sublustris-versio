# 0002 — Internal flash, Firmware Swap, and a flash budget

**Status:** Accepted, 21 Oct 2026 (owner approved spec v1.0). Phase 1 finding; follows Resilio ADR 0011.

**Context:** The owner and friends install firmware with Noise Engineering's Firmware Swap web app, which only takes a standard libDaisy app in the 128 KB internal flash (no bootloader). Resilio fitted, but ended at **97.7 %** (about 3 KB free) with one spring tank. Immensio carries more code: a Plate, an FDN with two voicings, a companion, five Era stages and Reverse.

**Decision:**
- Firmware is a standard libDaisy app in internal flash, installed with Firmware Swap.
- Flash is a tracked budget from M0: **release ≤ 110 KB at v1.0** (≥ 18 KB free). Every firmware build reports its size and fails over 128 KB.
- Resilio's techniques from the start: no QSPI chip setup, no USB device stack in release, `-Os` on control-rate code (`IV_SIZE_OPT`), only shipped voicings compiled in.
- Two engines, not three (ADR 0006).
- If the budget can't be met, the fallback is BOOT_SRAM with a custom linker script. It loses the Firmware Swap install, so it is **the owner's call, through a new ADR**.

**Rejected:** BOOT_QSPI (hot code is bigger than the I-cache); planning on the bootloader from the start (friends can't install it with Firmware Swap).
