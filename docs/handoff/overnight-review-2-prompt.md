# Overnight review 2: sound, character, controls, robustness, CPU and flash

Paste everything below the line into a **new** Claude Code session started in `/Users/jesse/Documents/Sites/immensio-versio`. It runs unattended overnight and leaves a report, listening pages and (if useful) one profile build for the morning.

- **Session:** Fable 5.1 at high effort. Pick the model and effort in the app before pasting.
- **The "Use a workflow" line** below is what lets the session orchestrate agents (a workflow only runs when you ask for one in your own words). Delete it to have the session use individual agents instead.
- **Before pasting:** close Ableton (nothing should need it, but it rules out accidents); leave the Mac plugged in and awake.

---

Use a workflow for this review, about 15 agents in all.

You're doing an independent, critical review of **Sublustris Versio**, a reverb firmware for the Noise Engineering Versio (Daisy Seed, STM32H750, Cortex-M7 at 480 MHz, 48 kHz, 48-sample blocks), with a JUCE plugin twin built from the same Core. It's had a week of tuning by ear and two rounds of CPU work; now the owner wants it held up against best practice and against other reverbs. Nobody will answer questions tonight: work on your own judgment, and leave anything that changes the sound as a clearly argued option for the owner to hear, never as a change.

**The owner** is a senior designer, new to DSP and embedded C++, with good ears. Write for them: each idea gets one plain sentence before its detail; musical words before technical ones; numbers with what they mean.

## What the owner wants to know

1. **Sound and character against best practice and other reverbs.** For each part of the sound, how it compares with established designs and with the reference units below, where ours is weaker, and what would make it better *within the current panel and character* (refine, don't add modes or features):
   - the three SPACEs (PLATE, HALL, MASSIVE): density, colouration, metallic ringing, modal density, stereo image and mono compatibility, the early part of the tail;
   - **WEAR** (ERA in the code: tape saturation, the BBD and its compander, grit, the AM radio): saturation curves, **aliasing** from the nonlinear stages at the tank's 24 kHz rate, gain staging into and out of them, how the character grows across the knob;
   - **MOTION** (DRIFT in the code; left: tape transport with wow, flutter, crinkle and dips; right: Wander): modulation depth and rates against tape machines and the literature, chorusing or pitch smear, how it interacts with long tails;
   - **SHIMMER** (including its feedback near endless DECAY, stacked with FREEZE / PAINT, and the pitch shifter's aliasing), **BLOOM** (the swell), **FREEZE / PAINT** and the twin room, the **ducking** and the **floor governor** at endless DECAY;
   - **parameter ranges and tapers** for every knob (not only DECAY): useful range, dead zones, cliffs, what ranges established units use. Build on `docs/sweetspot.md` (`tools/sweetspot.py`) and `docs/research/knob-feel-audit.md`.
2. **An audit of CPU load, flash and RAM.** Where the module's time, the 128 KB internal flash and the RAMs (DTCM, AXI SRAM, D2 `.d2_bss`, stack depth) go now, what's left, what's wasteful, and which established techniques could recover more without changing the sound. Include the worst combinations no lap has timed (for example a SPACE crossfade with PAINT, SHIMMER and a SIZE move together) and what a late block sounds like.
3. **Controls and CV.** The Versio sums each pot with its CV input in hardware (`firmware/main.cpp` ~263), so CV arrives through the knob path: what fast or audio-rate CV on SIZE, TIME, DECAY, TONE, WEAR and MOTION does (zipper noise, clicks, the crossfades restarting or never settling), and what the CV range covers. The knob path (10 ms one-pole, 0.003 deadband, `firmware/PotEndStops.h`) lives in `firmware/main.cpp` (~757-873) and **the renderer doesn't run it**: port it to a desktop harness or model it from the profile build, and say which. Also the gate input and the button (FREEZE / PAINT): fast or repeated gates, a gate held through a SPACE change. Say what only the module can answer.
4. **Robustness.** Hot Eurorack input levels (up to ±10 V: the input stage, the codec's headroom, clipping), DC offset in and out, pops or bursts at power-up, freeze and SPACE changes, NaN / Inf and denormal safety under any knob combination, hours-long stability at endless DECAY and in FREEZE / PAINT (energy creeping up or down, slow drift into rumble), pot ADC noise against the deadband. Two things nobody has checked:
   - **The release firmware ships tightened SDRAM timing** (`firmware/Makefile` line 30, `RELEASE_SDRAM_TIGHT ?= 1`: 3/3 clocks against libDaisy's 16/10), and its integrity check only ran on USB power: never on rack power, warm, or on another unit. Weigh the risk; `firmware/README.md` contradicts this in one place (find it).
   - **The desktop and the module do the maths differently** (the module has `-ffp-contract=off` and flush-to-zero; the desktop build doesn't match, per the last review), so desktop bit-identity doesn't prove the firmware unchanged, and denormal or soak results may not carry over. Say what that means for every claim.
   Build on `docs/research/soak-audit.md` and `dsp-audit.md`.
5. **Code health and docs, briefly.** Test gaps (what a regression could break without a test noticing), `SPEC.md` and the ADRs drifting from the code, dead code and stale comments. The housekeeping list in `docs/TASKS.md` ("The review's housekeeping list") already exists: add to it, don't redo it.
6. **Three open questions the owner asked you to weigh in on**, each with measurements and options. **These come first**: start them in phase 1 and write their sections before anything else.
   - **MASSIVE's loudness bump:** while playing, MASSIVE is about 4 dB louder just before endless (DECAY ~0.75-0.80, 2:30-3 o'clock) than at endless, because the ducking and the floor governor fade out across 0.80-0.85. A slow turn down from endless passes through it. Is it a problem, and what would smooth it without changing held endless or short settings?
   - **The rumble at endless:** PLATE (and HALL, less) at DECAY 1 with TONE fully clockwise slowly turns into sub-20 Hz rumble over ~10 minutes while the audible tail fades ~40 dB. Find the cause and the fix. Candidates to rule in or out: DC from asymmetric saturation (WEAR, the BBD) inside the loop, whether the loop has a DC blocker, TONE's tilt and bell at full, the governor. Measure the DC inside the loop, not just at the output.
   - **MASSIVE's CPU margin:** with SHIMMER + WEAR 0.5 + MOTION's tape side while SIZE moves, the profile lap peaks at 93-95 % (target ≤ 90 %; release runs a point or two heavier). Find a safe further saving. The option `IV_ABSORB_EXP` (loss filters on `exp` instead of `pow`, not bit-identical, ~1e-6) measured −1.5 to −4 points; it's off in release. `make 'TRIAL_DEFS= -DIV_ABSORB_EXP'` gave a byte-identical binary, probably because `BUILD_DIR` is named from tags that don't include `TRIAL_DEFS` (stale objects reused), and overriding `TRIAL_DEFS` also drops `SIZESPIKE`. Verify, then add a proper `ABSORB_EXP=1` switch that also changes the build tag, and measure its size.
7. **Blind spots.** Before you plan, spend real effort on what nobody has looked at yet. Read the research folder and the ADRs and list the areas with no study, test or measurement. Rank them by risk to the sound or to the module, and cover the top ones. Say in the report which you found and which you chose not to cover.

## The reference units

- **The owner's own recordings** (measure these): Valhalla Supermassive, VintageVerb, Room and Plate takes in `test_audio/reference/` (20 take folders plus `incoming/`), read by `tools/ingest_reference.py`. Earlier comparisons are in `docs/research/valhalla-comparison.md` and `valhalla-notes.md`. Measure ours against them side by side on the same material, with numbers: T60 per band, echo density over time (normalised echo density), spectral colouration and modal density, stereo correlation and mono fold-down, modulation depth and rate (pitch tracking on a held tone), the onset shape. Match settings by tail length, not by knob position. **These recordings are never committed, published or put on a page;** numbers, plots and your own renders are fine.
- **Classic hardware, from the literature** (no recordings): the Lexicon 224 and 480L, the EMT 140 and 250, the Eventide 2016 and SP2016, and the papers behind them. Fetch sources and cite them (URL and section); mark anything from memory as unverified. If time is short, this is the first thing to cut.

## Read first, in this order

- `CLAUDE.md`: the rules. They bind you, including the Hard rules.
- `docs/handoff/HANDOFF.md`: the current state. **All the code is on `main`** at v1.0.55 (`4658d91` or later).
- `SPEC.md` (changelog v1.0.41-v1.0.55, §3, §4, §5); `CONTEXT.md` (the vocabulary: use its terms; the panel's labels and the code's names differ: WEAR is ERA, MOTION is DRIFT, DEPTH is TIME, BLEND is MIX).
- `docs/adr/` (the index is `docs/adr/README.md`), especially 0008, 0014, 0020, 0025, 0026, 0033 and 0034.
- `docs/research/`: what has already been studied. **Don't redo it; build on it or challenge it.** The last review is `review-2026-10-26.md`: say which of its findings still hold, which were done, and which were wrong.
- The CPU data: `firmware/README.md` ("CPU save", "CPU save 2", the lap `SIZESPIKE=4`, memory placement), the laps `docs/research/m3-cpusave2-7ee389e-full.log` and `m3-cpuhunt-*.log`, and `firmware/main.cpp` (the profile build).
- `docs/TASKS.md`: the owner's open items, including "Waiting on Claude".
- The code: `core/dsp/` and `core/params/`; `firmware/main.cpp` for the knob path and the module's memory placement; `host/render/main.cpp` (~167-251) for the renderer's `IV_*` environment-variable hooks; `host/tests/`.

## Hard limits

- **Work only in a new worktree** made by hand off `main`: `git worktree add .claude/worktrees/review-2 -b review/overnight-2 main`. Commit there with explicit paths (never `git commit -a` / `-am`; `git apply --3way` also stages, so check `git status --short` before each commit). Don't touch `main`, don't merge, don't push.
- **Agents' worktrees are made by hand too**, off your branch, never with automatic isolation (it may cut from `main`): `git worktree add .claude/worktrees/r2-<name> -b review2/<name> review/overnight-2`.
- **Never delete or recreate `build/`** (the main checkout's; other tools share `build/iv_render`). Every build uses its own directory inside its worktree; two builds in one directory corrupt each other.
- **Builds in a worktree:**
  - desktop: `python3 tools/make_stimulus.py` first, then `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` (add `-DIV_BUILD_PLUGIN=OFF` where the plugin isn't needed), `cmake --build build`, `ctest --test-dir build > build/ctest.log 2>&1`, then check the log's "100% tests passed out of N" line (22 with the plugin, 21 without); a pipe's exit code hides failures;
  - firmware: `export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"; make -C firmware all-variants LIBDAISY_DIR=/Users/jesse/Documents/Sites/immensio-versio/libs/libDaisy` (a fresh worktree's own `libs/libDaisy` is empty). Never write into the main checkout's `firmware/build`.
- **The reference renderer for bit-identity:** build one pinned copy, once: `git worktree add --detach .claude/worktrees/ref-main main`, built with `-DIV_BUILD_PLUGIN=OFF`, target `iv_render`. Check `tools/same_render.py` gives SAME n/n against itself before you trust it.
- **No sound changes outside prototypes.** A prototype goes behind an `IV_*` renderer hook (an environment variable read in `host/render/main.cpp`), off by default, set **before** `prepare()` if it changes the layout. With the hook off: the renderer bit-identical (SAME n/n against the reference) and the firmware byte-identical (`cmp`).
- **A "sound-neutral" claim must be proven**, not argued: SAME n/n through SIZE moves, SPACE flips, freezes and DECAY moves. Remember it proves the desktop, not the module (see 4).
- **No hardware:** no flashing, no serial, no Audio Unit or `coreaudiod` commands (no `auval`, never `auval -a`), nothing in Ableton, don't reinstall the plugin.
- **Licences (ADR 0006):** no GPL code (Plateau and thement/midiverb_emulator are reference only), no unlicensed code (including the Daisy Griesinger reverb), no Midiverb ROM microcode. MIT and BSD code may be cited and learned from.
- **Flash is the tightest limit:** release is **112,564 B, 76 B under the 112,640 B budget** (v1.0.55; older numbers in HANDOFF or TASKS are from v1.0.54). Any proposal states its flash cost and where the bytes would come from.
- **The desktop is not the module:** the desktop is compute-bound, the M7 is memory- and flash-wait-bound. Desktop timings only rank ideas; savings are claimed from disassembly counts (`arm-none-eabi-objdump`), the module's laps, or the profile build for the owner to run.
- **Disk: under ~3 GB in all.** A 30-minute stereo float render is ~700 MB: analyse soaks while rendering, or write them as 16-bit or as level envelopes, and delete each one as soon as it's measured. List `rm` targets literally.

## What to produce

**1. A report:** `docs/research/review-2026-10-29.md`, committed on your branch as you go (each section when it's done, so an interrupted night still leaves something).
- **Summary** (one page, readable on a phone): the ten most valuable findings, ranked by what they're worth to the owner, each tagged *sound / CPU / flash / RAM / robustness / controls / code* and *measured / estimated / literature*.
- **The three open questions** (6), then **one section per area** (1-5 and 7). In each: what we do, how established designs and the reference units do it (with sources), the measurements, what's weak, and the options. For every option: what it would change in the sound (*none, provable / tiny / audible*), CPU (% of the module's budget, with assumptions), flash (bytes), risk, and whether it needs the owner's ears or the module to confirm.
- **The last review's findings revisited** (still true / done / wrong).
- **What I'd do next:** an ordered plan for the next working session, split into *sound-neutral, ready to merge after the owner's OK*, *needs the owner's ears* (with the page), *needs the module* (with the build).
- **At the top:** what you changed on the branch (prototypes, hooks, tests, tools), the branch name and HEAD, and what's waiting in `dist/` and `renders/`.

**2. Evidence for the claims that matter.** Prototype on your branch where a claim can be tested on the desktop, and measure. Put the numbers in the report.
- Loop changes and anything near the tank: fed holds at several input levels and infinite holds (energy bounded, peaks below the limiter), a 1 ms-frame click check, and energy above 3 kHz against an unswitched render (the click check misses broadband bursts).
- Sound-neutral savings: SAME n/n and the firmware's size.
- CPU claims for the module: static counts from the disassembly, plus, where only the module can answer, lines added to the profile build's lap. **One profile firmware** for the owner to run in the morning (two only if both are essential), copied to `dist/` with a unique name (`sublustris_versio-review2-<hash>-profile-<what>.bin`), with the exact steps in the report: USB power only (never with rack power), `screen /dev/tty.usbmodem* 115200` from the main checkout, Ctrl-A Shift-H to log, wait for "LAP 1 DONE".

**3. Listening pages** for the most promising sound options (**at most three**), into the main checkout's `renders/review2_<name>/`. Use `tools/review/make_review.py` without `--showcase` (that gives pick buttons), `tools/render_review_pages.sh`, and the scripts in `tools/pages/` as models:
- 100 % wet renders with `--wet-only --dry`, so the page's MIX fader blends the dry in live; MIX starting at 50 %;
- level-matched unless the level is the point (say which in the README);
- a plain README naming each version in musical words;
- a `_verify.txt` confirming each version really rendered (nulls between versions, and the renderer's stderr hook line per file) plus the click, burst and energy checks.

List the pages in the report. Don't send them anywhere.

**Done means:** the report's summary, the open questions, sections 1-7 and "What I'd do next" written and committed; the pages and the profile build (if any) listed with their checks; a final commit; your agents' worktrees removed (branches kept), the reference worktree too.

## Using agents

Split the work so that the reading, fetching and measuring happen in agents and the judgement stays with you. Load the `workflow-authoring` skill before writing the workflow. If the workflow tool fails or isn't available, carry on with the Agent tool (`run_in_background`) to the same plan.

- **Phase 1, survey and measurement (parallel, ~9 agents):**
  - the three open questions, one agent each (the rumble needs 10-minute renders: start it first);
  - literature, one agent per topic, each returning sourced findings (URL, section):
    - FDN and plate design and colouration (Jot & Chaigne 1991; Schlecht & Habets; Välimäki et al. 2012; Dattorro 1997; Costello's Valhalla DSP blog), plus the classic hardware if time allows;
    - saturation, tape, BBD and AM-radio emulation, aliasing in nonlinear stages (antiderivative antialiasing, oversampling, their cost), and modulation in reverbs and tape transports;
  - measurement, each in its own worktree off your branch:
    - ours against the Valhalla recordings (the numbers listed above);
    - every knob's range and taper, plus CV-speed and noisy-knob moves through the ported knob path;
    - robustness: hot inputs, DC, NaN / denormals, start-up, a long soak at endless DECAY and in FREEZE / PAINT;
  - the CPU, flash and RAM audit from the code, the disassembly, the laps and the map file (`nm -S --size-sort`, what libgcc and libm pull in, `.d2_bss` / AXI / DTCM / ITCM placement, the SDRAM timing).
- **Phase 2, prototypes (~4 agents):** the options you choose from phase 1, each in its own worktree off your branch, each with its gates and, for sound options, its page. A short code-health read can share an agent with one of these.
- **Phase 3, verification (~2 agents, plus yourself):** re-run the claims you intend to report. A claim goes into the report only after you've checked it (re-run its measurement, opened its source); label anything you couldn't check as the agent's and unverified. Agents' confident tone is no evidence: last week a prototype passed its first checks and still had three clicks.
- **Models:** literature and fetching on Sonnet 5.5; measurements, cost studies and prototypes on Opus 5.5; set each agent's model explicitly. Ranking, reconciling and the report are yours.
- **Brief every agent fully** (it knows only what you tell it): the goal in the owner's musical words, the files it owns, its worktree and branch, what's already known, what to return (findings with evidence, estimates with assumptions, no raw logs), and the rules: never delete or recreate `build/`, its own build dir, commits only in its worktree with explicit paths, no hardware or Audio Unit commands, no GPL or unlicensed code, the build commands above, Python at `/Users/jesse/Documents/Sites/immensio-versio/.venv/bin/python`.
- Review each agent's diff before taking anything into your branch, and re-run the cheapest decisive gate yourself (SAME n/n, or the firmware size and `cmp`).

## How to work

- **Order the night so it degrades gracefully:** the open questions and the summary's skeleton first, then the survey, then the prototypes in order of value, then the pages. Stop starting new prototypes once the report would otherwise be left unfinished; an unfinished option belongs in "What I'd do next".
- **The owner reads in the morning on their phone.** Make the summary self-contained, and list each page by its folder name. The main session starts the LAN page server for them, so don't start one yourself.
- **Check the existing measurements before trusting a hunch,** and measure an owner-facing problem against a control before proposing a fix.
- **Be honest about uncertainty:** give ranges; say what's measured, estimated or from the literature; say plainly when an idea isn't worth it. A short list of strong findings beats a long list of weak ones.
