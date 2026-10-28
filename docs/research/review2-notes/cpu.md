# Review 2: MASSIVE's CPU margin (review2/cpu, 28 Oct 2026)

Work in progress; the final report is at the end of this file once the night is over.

## 1. Why `make 'TRIAL_DEFS= -DIV_ABSORB_EXP'` gave the same .bin (verified)

- `BUILD_DIR` (firmware/Makefile, `override BUILD_DIR = build/obj-$(MODE)-$(PROFILE_BLOCK)...`) is named from
  `RELEASE_TAG` / `PROFILE_TAG`, which hold `TRIAL_TAG`, not `TRIAL_DEFS`. A command-line `TRIAL_DEFS` changes
  the compile flags but not the directory, and make tracks files, not flags: every object in `obj-release-48`
  was newer than its source, so nothing compiled. `make -n` with the override prints **0 compile commands**,
  only the three `cp build/obj-release-48/... build/...` lines: the old release, copied up again.
- A command-line variable replaces the whole computed `TRIAL_DEFS`, so with `SIZESPIKE=4` the
  `-DIV_PROFILE_SIZESPIKE=4` define vanished (`make -n MODE=profile SIZESPIKE=4 'TRIAL_DEFS= -DIV_ABSORB_EXP'`: 0
  occurrences) while the directory and name still said `-cpusave` (`PROFILE_TAG` reads `SIZESPIKE` itself). A
  fresh tree would have built the corner lap under the cpusave name.
- The hazard the other way: after a clean, the override builds `exp` objects into `obj-release-48` and names the
  result `sublustris_versio.bin`; every later default build reuses them. Never override `TRIAL_DEFS`.

## 2. The `ABSORB_EXP=1` switch

`make -C firmware ABSORB_EXP=1` (release) and `make -C firmware MODE=profile ABSORB_EXP=1 [SIZESPIKE=4]` add
`-DIV_ABSORB_EXP` to `C_DEFS` through `TRIAL_DEFS` and `-absexp` to `TRIAL_TAG`, so each gets its own object
directory and .bin (`obj-release-48-absexp`, `sublustris_versio-absexp.bin`, `sublustris_versio_profile-absexp-cpusave.bin`).
`all-variants`' size table now names the tagged files (the same names when no switch is set).

Default builds byte-identical: a clean `make all-variants` before and after (and again after the Core edits below),
`cmp` against the baseline copies: release 112,564 B, m0test 83,556 B, profile 127,808 B, all three identical.
