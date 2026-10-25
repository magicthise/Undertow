# Undertow

A Tidal-style pattern engine in C++17, for a VST3 plugin (MIDI out) and a VCV Rack module (CV and MIDI out).

## Rules

- Write everything fresh. Never copy, translate or closely paraphrase code from Tidal (GPL-3.0) or Strudel (AGPL-3.0), not even small pieces. The license is undecided and this keeps it open. Match Tidal's behaviour by testing against real Tidal, not by working from its source.
- `engine/` stays free of framework and platform dependencies, so it builds inside both JUCE and the VCV Rack SDK.
- C++17 only, because of the Rack SDK.
- New pattern behaviour gets a case in `tests/conformance/cases.txt`; then rerun `python3 tools/tidal-oracle/generate.py`, which needs GHC and cabal-install 3.10 or later.

## Commands

- Build and test: `cmake -S . -B build && cmake --build build && build/tests/undertow_tests`
- Run only some tests: `build/tests/undertow_tests <part of a test name>`
