# Undertow

A live-coding pattern language for music in the style of [TidalCycles](https://tidalcycles.org), built for two products:

- **A VST3 plugin** that plays patterns as MIDI, in Ableton Live, Cubase and other DAWs.
- **A VCV Rack module** that plays them as CV, gates and MIDI.

Both will share one pattern engine, written in C++17 with no framework dependencies. That engine is what exists so far; there's no plugin or module yet.

## What works

- **Pattern engine:** exact fractional time, and `fast`, `slow`, `early` (`<~`), `late` (`~>`), `rev`, `every`, `stack`, `cat`, `fastcat`, `timecat`, `compress` and `euclid`, with patterned arguments.
- **Mini-notation:** sequences, `~` and `-` rests, `[ ]` groups, `< >` alternation, `{ }` polymeter with `%steps`, `,` layers, `*` and `/`, `!` repeats, `@` weights, `_` elongation, `.` grouping and `(pulses,steps,rotation)` Euclidean rhythms.
- **Notes:** semitone numbers and note names (`c`, `fs`, `ef4`).
- **MIDI files**, through the `undertow-render` command-line tool.
- **Checked against real Tidal:** 65 cases produce the same events as Tidal 1.10.

Not yet: randomness (`?`, `|`, `sometimes`), `..` ranges, chords, an interpreter for full Tidal code, the real-time scheduler, the VST3 and the Rack module.

## Building

Needs CMake 3.20 or later and a C++17 compiler.

```sh
cmake -S . -B build
cmake --build build
build/tests/undertow_tests          # or: ctest --test-dir build
build/tests/undertow_tests mini     # only tests whose names contain "mini"
```

## Trying it

```sh
build/cli/undertow-render -c 4 -o out.mid "<0 3> [7 10](3,8) c6"
build/cli/undertow-render --print --velocity "1 0.5" "c e g"
```

Notes are semitones from middle C, or note names where octave 5 is the middle one. Run `undertow-render --help` for tempo, length, channel and velocity options.

## Checking against Tidal

`tests/conformance/cases.txt` lists patterns that run through both Undertow and real Tidal. `tools/tidal-oracle/generate.py` runs them through Tidal and saves the results to `tests/conformance/tidal-expected.txt`, which is committed, so the normal tests don't need Haskell.

To regenerate after adding cases, install GHC and cabal-install 3.10 or later, then run:

```sh
cabal update
python3 tools/tidal-oracle/generate.py
```

The first run downloads and builds Tidal, which takes several minutes. Older cabal-install versions reject Hackage's current signing keys.

Events must match exactly, with one exception: fragments that don't include their event's start are compared by time and value only. Tidal sometimes reports a misplaced whole event for those fragments, and since they never trigger anything, it can't change what plays.

## Layout

| Path | Contents |
|---|---|
| `engine/` | The pattern engine library |
| `cli/` | `undertow-render` |
| `tests/` | Unit tests and the Tidal comparison |
| `tools/tidal-oracle/` | The Haskell program that runs cases through real Tidal |

## Where the code comes from

Undertow is written from scratch. It doesn't copy or translate code from Tidal or Strudel. Real Tidal is only run as a reference to compare results against, and isn't part of anything Undertow ships. That keeps the choice of license open.

## License

Not chosen yet; all rights reserved until then.
