# CASM SFF1/SFF2 reference model

## Purpose and scope

This document is the research reference for Yamaha Style `CASM` semantics used by `yamsty`.

Its purpose is **not** to import or reproduce third-party source code. It normalizes the public descriptions, open-source implementations and the project's own binary evidence into a model that can be tested independently and later implemented safely.

The evidence hierarchy used here is:

1. **Primary project evidence** — read-only factory/user Style files and controlled `STS -> STY` differential experiments.
2. **Official Yamaha UI semantics** — current Yamaha MIDI Song to Style documentation. This documents user-facing concepts but not the complete binary representation.
3. **Technical reverse-engineering reference** — Peter Wierzba / Michael P. Bedesem, *Style Files - Introduction and Details*, V2.1 (2 Apr 2015). It is unusually detailed, but the authors explicitly state that it is not Yamaha-approved and may be incomplete.
4. **Reference implementations** — `bures/sff2-tools`, JJazzLab `CASMDataReader`, MIDI LAML. These are useful for checking how other authors interpreted the format, but they are not independent specifications. In particular, MIDI LAML explicitly cites Wierzba/Bedesem as its best CASM reference, and `sff2-tools` also credits that document.
5. **Secondary material** — forums, tutorials and proprietary editors. These may suggest experiments but do not establish byte semantics.

A statement is called **confirmed** in the project only when it is supported by project corpus evidence / a controlled experiment, or by sufficiently strong independent evidence consistent with the corpus. Unknown bytes remain raw.

## Source inventory

| Source | Scope | Use in this research | Trust / limitation |
|---|---|---|---|
| `StyleFileDescription_v21.pdf`, Peter Wierzba / Michael P. Bedesem, V2.1 | SFF1 + SFF2 container, CASM/Ctab/Ctb2/Cntt and musical semantics | Main technical field map and terminology | High-value reverse engineering, not an official Yamaha specification |
| Yamaha *MIDI Song to Style Owner's Manual* (2024) | Current SFF GE UI and conversion workflow | Official terminology for eight Style Parts, Lo/Main/Hi zones, Style Playing Parameter, Convert to Style Parameter, HiKey/limits/MegaVoice | Official, but UI-level; preset -> binary mapping is not documented |
| `bures/sff2-tools` | SFF1/SFF2 parser/serializer | Cross-check field layout and enum values | Reference implementation; derived at least partly from Wierzba/Bedesem; no complete CASM playback semantics |
| JJazzLab `CASMDataReader` | SFF1/SFF2 CASM reader | Cross-check CSEG/Ctab/Ctb2/Cntt structure and SFF1-to-SFF2 normalization | Independent codebase, but not treated as a specification |
| Kurt Nørmark, MIDI LAML | SFF1 CASM formalization and sequencing | Historical semantic model, especially source/destination mapping | Explicitly cites Wierzba/Bedesem; not an independent confirmation of those descriptions |
| Jørgen Sørensen software / articles | Historical style creation and CASM editing | Context, tool behavior, candidate experiments | CASM Editor is available as software, but no public source code was found during this audit |
| Reinhold Pöhnl, *Styles & Patterns* | Musical behavior of Yamaha accompaniment | Wierzba/Bedesem cites it for some NTR/NTT recommendations | Full text not available to this research; secondary quotations only |
| `yamsty` PSR-SX600 factory corpus | 8 Yamaha factory Styles | Primary SFF2 binary/corpus evidence | Direct evidence for this corpus only |
| `Sib_AA01` STS/STY controlled corpus | MIDI Song to Style project + controlled exports | Causal field mapping | Strongest evidence for fields changed one at a time |

Primary public locations:

- Wierzba/Bedesem page: `https://wierzba.hier-im-netz.de/stylefiles.htm`
- `sff2-tools`: `https://github.com/bures/sff2-tools`
- JJazzLab reader: `https://github.com/jjazzboss/JJazzLab/blob/master/plugins/YamJJazz/src/main/java/org/jjazz/yamjjazz/CASMDataReader.java`
- MIDI LAML CASM documentation: `https://homes.cs.aau.dk/~normark/laml-distributions/laml/styles/xml-in-laml/midi/man/midi-mirror.html`
- Jørgen Sørensen Yamaha resources: `https://www.jososoft.dk/yamaha/`

## Architectural model

The strongest shared model, now also corroborated by the factory corpus, is:

```text
Style MIDI source channels (MIDI channels 1..16)
        |
        | CASM / CSEG / Ctab or Ctb2
        | - section membership
        | - source -> destination mapping
        | - chord/note eligibility masks
        | - source chord
        | - NTR / NTT
        | - note limits / High Key / RTR
        v
eight logical accompaniment destinations
        |
        +-- MIDI 9  / destination 0x08 : Rhythm 1 (historical "Sub Rhythm")
        +-- MIDI 10 / destination 0x09 : Rhythm 2 (historical "Rhythm")
        +-- MIDI 11 / destination 0x0A : Bass
        +-- MIDI 12 / destination 0x0B : Chord 1
        +-- MIDI 13 / destination 0x0C : Chord 2
        +-- MIDI 14 / destination 0x0D : Pad
        +-- MIDI 15 / destination 0x0E : Phrase 1
        `-- MIDI 16 / destination 0x0F : Phrase 2
```

Therefore **eight Style Parts are not an eight-source-channel storage limit**. A valid SFF2 Style can contain more than eight note-active source channels in one musical Section, and several source channels can feed one logical destination.

This is an important `yamsty` invariant: a verifier must never reject a Style merely because `active source MIDI channels > 8`.

## CSEG and section sharing

`CASM` contains one or more `CSEG` structures. Each CSEG begins with `Sdec`, whose text names one or more musical Sections. The following channel-table records apply to all Sections named by that Sdec.

A CSEG can therefore be shared:

```text
CSEG
  Sdec = "Main A,Main B,Fill In AA,..."
  Ctb2 source 1 ...
  Ctb2 source 2 ...
  ...
```

A Ctb2 record present in such a shared CSEG does not imply that its source channel has note events in every member Section. MIDI activity must be checked per Section separately.

## Common Ctab/Ctb2 first part

Wierzba/Bedesem V2.1 describes the first 20 payload bytes as common to old `Ctab` and new `Ctb2`. The same layout is implemented by `sff2-tools` and JJazzLab and is consistent with the PSR-SX600 factory corpus.

Offsets below are zero-based inside the Ctab/Ctb2 payload, excluding the 4-byte id and 4-byte length.

| Offset | Size | Meaning | Encoding / note | Project evidence status |
|---:|---:|---|---|---|
| 0 | 1 | Source channel | `0x00..0x0F` = MIDI 1..16 | Corpus-corroborated; all 16 values occur |
| 1..8 | 8 | Record name | 8-byte space-padded text | Corpus-corroborated; semantic names visible |
| 9 | 1 | Destination channel | `0x08..0x0F` = logical Style Parts / MIDI 9..16 | Corpus-corroborated; only these 8 values occur |
| 10 | 1 | Editable flag | `0` editable, `1` not editable in Style Creator according to reference | All 343 factory Ctb2 records are `1`; behavior not hardware-tested |
| 11..12 | 2 | Note-play / note-mute mask | 12 pitch classes | Structural mapping corroborated; detailed playback not controlled-tested |
| 13..17 | 5 | Chord-play / chord-mute mask | recognized chord-type eligibility plus special bits in byte 13 | Strongly corroborated by factory Maj/min alternatives |
| 18 | 1 | Source chord root | `C=0 ... B=11` | Corpus values currently all C |
| 19 | 1 | Source chord type | chord-type enum (`Maj`, `Maj7`, `min`, ... ) | Factory corpus uses `Maj`, `Maj7`, `min`; semantic names correlate |

### Chord mask interpretation

For ordinary chord-selection bits, `1` means the source channel is enabled for that recognized chord type and `0` means it is muted. Byte 13 also contains special/unused bits, including the historically documented Auto Start flag for rhythm/percussion use.

This is not merely descriptive metadata: the factory `GuitarSerenade.T611.prs` provides strong direct evidence that these masks select alternative source material.

## SFF1 `Ctab` second part

Old `Ctab` stores one set of transposition parameters for the complete source channel.

| Offset | Meaning | SFF1 values |
|---:|---|---|
| 20 | NTR | `0 Root Transposition`, `1 Root Fixed` |
| 21 | NTT | `0 Bypass`, `1 Melody`, `2 Chord`, `3 Bass`, `4 Melodic Minor`, `5 Harmonic Minor` |
| 22 | High Key | pitch class `C=0 ... B=11` |
| 23 | Note Low Limit | MIDI note `0..127` |
| 24 | Note High Limit | MIDI note `0..127` |
| 25 | RTR | `0 Stop`, `1 Pitch shift`, `2 Pitch shift to root`, `3 Retrigger`, `4 Retrigger to root`, `5 Note generator` |
| 26.. | Special feature | historical optional data, e.g. extra break drum voice |

The key conceptual point is that SFF1 already contains the same core musical model later used inside each SFF2 zone.

## SFF2 `Ctb2` extension

SFF2 replaces the single Ctab transposition set with up to three pitch zones.

| Ctb2 payload offset | Meaning |
|---:|---|
| 20 | lowest MIDI note belonging to Middle/Main zone; `0` disables Low zone |
| 21 | highest MIDI note belonging to Middle/Main zone; `127` disables High zone |
| 22..27 | Low-zone 6-byte transposition substructure |
| 28..33 | Middle/Main-zone 6-byte transposition substructure |
| 34..39 | High-zone 6-byte transposition substructure |
| 40..46 | seven still-unknown bytes |

Each 6-byte zone substructure is:

| Relative offset | Meaning |
|---:|---|
| 0 | NTR |
| 1 | NTT plus Bass-On bit |
| 2 | High Key |
| 3 | Note Low Limit |
| 4 | Note High Limit |
| 5 | RTR |

This layout is particularly well anchored by the controlled STS experiment already in `docs/casm_notes.md`: Main-zone `HiKey`, `LoLim`, `HiLim` map to absolute Ctb2 payload offsets `30`, `31`, `32`, exactly as predicted by the structure above.

### NTR in Ctb2

| Raw | Meaning | Practical interpretation |
|---:|---|---|
| 0 | Root Transposition | preserve pitch relationships while moving with chord root; typical melodic/bass behavior |
| 1 | Root Fixed | keep output near a stable register, allowing inversion changes; typical chordal behavior |
| 2 | Guitar | guitar-specific mapping where source note classes represent strings/functions rather than ordinary pitch relations |

For `Guitar`, the Wierzba/Bedesem reference describes a source-note-to-string mapping and octave-dependent fretboard positions. This is useful semantic guidance, but exact playback should still be verified before implementing a generator.

### NTT in Ctb2

Bit 7 is the `Bass On` flag. Bits 0..6 select the table.

For non-Guitar NTR:

| Raw rule | Meaning |
|---:|---|
| 0 | Bypass |
| 1 | Melody |
| 2 | Chord |
| 3 | Melodic minor |
| 4 | Melodic minor 5th variation |
| 5 | Harmonic minor |
| 6 | Harmonic minor 5th variation |
| 7 | Natural minor |
| 8 | Natural minor 5th variation |
| 9 | Dorian |
| 10 | Dorian 5th variation |

For `NTR=Guitar`:

| Raw rule | Meaning |
|---:|---|
| 0 | All-Purpose |
| 1 | Stroke |
| 2 | Arpeggio |

`Bass On` changes on-bass chord handling; in later formats it replaces the older dedicated SFF1 `NTT Bass` concept with, typically, an ordinary NTT plus the high bit.

### NTT musical semantics

The reference material gives useful intended use:

- `Bypass`: no harmonic transposition; drums/SFX and composed Intro/Ending progressions.
- `Melody`: melodic lines, commonly Phrase parts.
- `Chord`: chord-oriented material.
- old `Bass`, or later `Melody + Bass On`: bass behavior.
- `Melodic Minor`: major/minor adaptation centered on the third degree.
- `Harmonic Minor`: major/minor adaptation involving the third and sixth degrees.
- the Natural Minor, Dorian and `5th Var.` tables are documented structurally, but their complete note-by-note behavior is not sufficiently specified for a `yamsty` transposition engine.
- Guitar `Stroke` and `Arpeggio` are specialized guitar mappings; `All-Purpose` is intended for mixed material.

### RTR

RTR controls notes already sounding when the recognized chord changes:

| Raw | Meaning |
|---:|---|
| 0 | Stop |
| 1 | Pitch shift |
| 2 | Pitch shift to root |
| 3 | Retrigger |
| 4 | Retrigger to root |
| 5 | Note generator |

The current factory corpus contains raw RTR values `0..4`; value `5` was not observed in the eight PSR-SX600 factory files.

## `Cntt`: historical extension, not a normal Ctb2 companion

Wierzba/Bedesem describes `Cntt` as a two-byte source-channel + NTT/Bass override used with old Ctab and states that Cntt is not used when a Style contains Ctb2.

This is consistent with the current PSR-SX600 factory audit:

- 35 CSEG structures;
- 343 `Ctb2` records;
- **0 `Ctab` records**;
- **0 `Cntt` records**.

This corpus supports, but does not universally prove, the mutual-exclusion claim. `yamsty` should still parse/preserve Cntt if encountered rather than reject it based on this observation.

JJazzLab's reader also treats Cntt as an override of Ctab NTT data. That is useful corroboration of the historical interpretation, not a reason to assume all mixed Ctb2/Cntt files are invalid without evidence.

## Factory corpus validation

The eight-file PSR-SX600 factory corpus used for this audit:

- `2019DancePop.T547.prs`
- `2019DancehallPop.T547.sty`
- `2019LatinPop.T547.prs`
- `70sCountryPop.T554.prs`
- `ChinaKidDance.T589.sst`
- `DJ Mix2019.T551.prs`
- `EtherealMovie.T611.sty`
- `GuitarSerenade.T611.prs`

Aggregate direct observations:

- 120 musical Sections;
- 45/120 Sections have note events on more than eight MIDI source channels;
- maximum note-active source channels in one Section: **14**;
- 35 CSEG structures;
- 343 Ctb2 records, all with payload length **47 bytes**;
- maximum Ctb2 records in one CSEG: **14**;
- Ctb2 source byte 0 spans the full `0..15` range in the corpus;
- destination byte 9 is restricted to `8..15`;
- all 343 factory records have `editable=1`;
- source chord type values observed: `Maj` (140), `Maj7` (133), `min` (70);
- no Ctab or Cntt records were found.

The maximum number of simultaneously note-active source channels feeding one destination, already recorded in Issue #7, is:

| Logical destination | Maximum active source channels |
|---|---:|
| Rhythm 1 | 2 |
| Rhythm 2 | 1 |
| Bass | 2 |
| Chord 1 | 4 |
| Chord 2 | 4 |
| Pad | 2 |
| Phrase 1 | 4 |
| Phrase 2 | 2 |

### Important correction to the simple "alternatives only" model

Multiple sources mapped to one logical Part are **not only mutually exclusive alternatives**.

The corpus contains both:

1. **conditional alternatives**, selected by chord mask;
2. **simultaneous layers**, where two sources mapped to the same destination are eligible for overlapping chord types and their note intervals overlap in time;
3. combinations of both mechanisms in the same destination.

Across the 120 Sections, 233 Section/destination groups have more than one note-active source channel. A conservative static check found:

- 157 groups whose active sources have pairwise-disjoint ordinary chord masks — strong evidence for conditional alternatives;
- 76 groups with at least one pair that both has overlapping chord eligibility **and** overlapping source-note time intervals — strong evidence that multi-source logical Parts can also act as simultaneous layers.

These numbers describe the current corpus and static CASM/MIDI eligibility. Hardware playback should be used before claiming that no additional undocumented condition can suppress a layer.

## `GuitarSerenade`: combined layering and Maj/min selection

The Intro/Ending CSEG in `GuitarSerenade.T611.prs` is the clearest architecture example.

For Chord 1:

```text
source 2  "Gt1A Maj" -> Chord 1
source 4  "Gt1B Maj" -> Chord 1
source 6  "Gt1A min" -> Chord 1
source 8  "Gt1B min" -> Chord 1
```

Chord 2 has the analogous four-source structure.

The major records use chord mask:

```text
03 ff f8 00 ff
```

The minor records use:

```text
00 00 07 ff 00
```

Using the documented chord-mask bit layout:

- the major mask enables major, augmented, dominant/sus and related non-minor families while excluding the minor/diminished family represented by the other mask;
- the minor mask enables the minor and diminished family;
- the two families are disjoint in ordinary chord-selection bits.

Thus the **Maj vs min source pairs are conditional alternatives**.

At the same time, `Gt1A Maj` and `Gt1B Maj` have the same major mask and overlapping note intervals in Intro B/C and Ending B/C; the corresponding `A/B` minor sources likewise overlap. Therefore this one destination demonstrates a two-dimensional scheme:

```text
recognized chord family
        |
        +-- major-like -> Gt1A Maj + Gt1B Maj
        |
        `-- minor/dim  -> Gt1A min + Gt1B min
```

So one logical Chord Part can contain **multiple simultaneous source layers inside each condition**, plus a different layer set for another chord family.

This resolves an important part of Issue #7 and shows why the eight logical Style Parts must not be modeled as eight simple MIDI tracks.

## Seven unknown Ctb2 bytes

Wierzba/Bedesem explicitly leaves payload offsets `40..46` unresolved and reports correlations with some drum behavior.

The PSR-SX600 factory corpus shows these patterns:

| `Ctb2[40..46]` | Count |
|---|---:|
| `00 00 00 00 00 00 00` | 214 |
| `00 00 00 00 80 00 00` | 106 |
| `80 00 00 00 00 00 00` | 7 |
| `00 01 00 18 31 3C 00` | 7 |
| `00 01 00 18 31 50 00` | 5 |
| `00 01 00 18 31 2E 00` | 2 |
| `00 01 00 18 31 48 00` | 1 |
| `00 01 00 18 31 7C 00` | 1 |

The drum-associated `00 01 00 18 ...` family agrees with observations in the 2015 reference, but that is still correlation. These seven bytes remain **opaque raw data** in `yamsty`.

A notable corpus difference is that the all-zero pattern is more common on these PSR-SX600 styles than the `00 00 00 00 80 00 00` pattern described as typical for many older Tyros3-era files. Historical frequency must not become a default-writing rule.

## Relationship to MIDI Song to Style

The current Yamaha MIDI Song to Style manual exposes three pitch regions:

- Lo Zone;
- Main Zone;
- Hi Zone.

For Lo/Main it exposes Style Playing Parameter, Convert to Style Parameter, High Key and note limits; Hi Zone additionally has Mega Voice handling. The structural similarity to the Ctb2 low/middle/high split is strong, and the controlled HiKey/LoLim/HiLim experiment confirms the Main-zone positional mapping.

However two UI layers must be kept distinct:

```text
source MIDI Song
        |
        | Convert to Style Parameter
        | - Root only / Root & 5th
        | - Stable for Main / Active for Main
        | - Semi-active Bass / guitar conversions / ...
        v
generated Style source MIDI pattern
        |
        | Style Playing Parameter
        | - playback note-conversion behavior
        | - candidate expansion into NTR / NTT / Bass-On / RTR / masks / zones
        v
Ctb2 + Style MIDI
        |
        v
Style playback under recognized chords
```

This distinction is important for `yamsty`: `Convert to Style Parameter` is a **conversion-stage policy**, while `Style Playing Parameter` describes the **future Style playback behavior**. A conversion choice may therefore materialize in generated MIDI, generated CASM, or both; it must not be assumed to correspond to one persistent Ctb2 enum.

### Style Playing Parameter

Examples: `Bass`, `Chord`, `Phrase chordal`, `Phrase melodic`, `Guitar Stroke`, `Guitar Arpeggio`, `Chromatic`, `Maj-min (Intro)`, drum/SFX presets.

These are described by Yamaha as playback note-conversion rules. The manual does **not** document their exact binary expansion into NTR, NTT, Bass-On, RTR, note/chord masks, zone split or other fields.

Wierzba/Bedesem predates the current MIDI Song to Style preset names, but its documented NTR/NTT semantics provide useful **candidate correspondences**. They are not treated as confirmed preset mappings:

| STS Style Playing preset | Candidate low-level relationship | Evidence strength / caveat |
|---|---|---|
| `Bass` | SFF2 `NTT=Melody` + `Bass On`; bass-oriented NTR/RTR likely | Strong historical continuity for the NTT/Bass concept; exact preset bundle untested |
| `Chord` | `NTT=Chord`; `Root Fixed` is a plausible/common chord-oriented NTR | Strong semantic candidate; exact NTR/RTR/masks untested |
| `Phrase melodic` | `NTT=Melody` | Strong semantic candidate; exact full bundle untested |
| `Phrase chordal` | chord-oriented NTT behavior is plausible | Exact distinction from `Chord` is not documented and must be measured |
| `Guitar Stroke` | `NTR=Guitar` + guitar `NTT=Stroke` | Very strong structural/name correspondence; still needs controlled export confirmation |
| `Guitar Arpeggio` | `NTR=Guitar` + guitar `NTT=Arpeggio` | Very strong structural/name correspondence; still needs controlled export confirmation |
| Drum / SFX presets | `NTT=Bypass` is the natural historical behavior | Strong semantic candidate; drum-specific trailing bytes may also matter |
| `Maj-min (Intro)` | one or more minor-adaptation NTT families (`Melodic Minor`, `Harmonic Minor`, later variants) | Historical behavior explains the preset concept, but exact chosen table/bundle is unknown |
| `Chromatic` | no safe one-to-one mapping yet | Do not infer `Bypass` or another NTT without a controlled delta |

`Bass (Intro)` and other Intro-specific presets are likewise not assigned a low-level mapping from terminology alone. Source Chord, Section Key, masks and conversion-stage rewriting may be as important as NTR/NTT.

### Convert to Style Parameter

Examples: `Root only`, `Root & 5th`, `Stable for Main`, `Active for Main`, `Semi-active Bass`, guitar conversions.

These govern conversion of the source MIDI into a Style Part. Yamaha's documentation distinguishes this conversion step from playback note-conversion rules. The most conservative technical model is therefore:

- `Root only` / `Root & 5th` describe how harmonic content is normalized or reduced while creating the Style source pattern;
- `Stable for Main` prioritizes a generally usable converted Style pattern;
- `Active for Main` retains more of the analyzed source performance and therefore depends more strongly on correct chord analysis;
- `Semi-active Bass` and guitar conversion choices are specialized conversion algorithms for their source material classes.

The exact note-by-note transformations are not documented in the available references. In particular, `Stable` and `Active` must **not** be equated directly with a particular NTR/NTT value. A controlled export may show changes only in `MTrk`, only in `CASM`, or in both.

Therefore `yamsty` must not hardcode a guessed preset mapping. Controlled STS -> STY one-parameter experiments are the correct next step.

## Reference implementation audit

### `bures/sff2-tools`

Useful findings:

- common Ctab/Ctb2 first part matches the V2.1 field map;
- Ctb2 is modeled as low/middle/high 6-byte substructures plus seven unknown bytes;
- NTR/NTT/Bass flag/HighKey/limits/RTR enums agree with the reference;
- Cntt is modeled as source channel + NTT/Bass override.

Limitation: its README explicitly says CASM-aware playback is not implemented because exact transposition behavior under all CASM settings was not known. It is therefore useful for **structure**, not proof of musical semantics.

### JJazzLab

Useful findings:

- separately recognizes `Ctab`, `Ctb2` and `Cntt`;
- creates low/main/high Ctb2 settings;
- interprets the NTT high bit as Bass On;
- adapts old SFF1 NTT values into its newer internal model;
- treats Cntt as overriding Ctab NTT data.

Again, this is implementation evidence, not specification authority.

### MIDI LAML

MIDI LAML provides a clear SFF1 object model and states the 16-source -> 8-destination CASM architecture. Its documentation explicitly names Wierzba/Bedesem as the best available CASM description and says some wording is taken from it. Therefore it must not be counted as an independent documentary confirmation of the same field meanings.

### Recent third-party implementations

A wider GitHub search finds additional recent CASM parsers. At least one inspected project (`AIArranger`) currently disagrees with the well-supported Ctb2 offsets and even labels NTR values inconsistently with Wierzba/Bedesem and the two mature implementations. Such sources are useful only as leads and demonstrate why `yamsty` should not accept a GitHub implementation as truth without corpus validation.

## SFF1 -> SFF2 continuity

The evidence supports a strong continuity model:

| Concept | SFF1 `Ctab` | SFF2 `Ctb2` | Status |
|---|---|---|---|
| source channel | yes | same common field | strong/corpus-corroborated |
| record name | yes | same | strong/corpus-corroborated |
| destination Style Part | yes | same | strong/corpus-corroborated |
| editable flag | yes | same | structurally strong |
| note mask | yes | same | structurally strong |
| chord mask | yes | same | strong; behavior corroborated by Maj/min variants |
| source root/type | yes | same | strong/corpus-corroborated |
| NTR Root Trans / Root Fixed | one set | one set per zone | strong |
| NTT core tables | one set | expanded table set per zone | strong |
| dedicated SFF1 Bass NTT | yes | replaced by NTT + Bass-On bit | strong reference model |
| High Key | one | one per zone | Main-zone controlled confirmation |
| note limits | one pair | one pair per zone | Main-zone controlled confirmation |
| RTR | one | one per zone | strong structural mapping |
| Guitar NTR/NTT | no | added | SFF2 extension |
| pitch zones | no | Low/Main/High | SFF2 extension |
| Cntt NTT override | optional extension to Ctab | normally unnecessary/not observed with Ctb2 | historical/strong but not universal |
| final 7 bytes | n/a | present | unknown |

The practical conclusion is that SFF2 is not a completely different accompaniment model. It preserves the SFF1 core and extends the per-source transposition model with zones, expanded NTT choices, Bass-On and guitar-specific behavior.

## Implications for `yamsty`

1. Keep the current raw-preservation policy for CASM until write semantics are explicitly implemented.
2. A future read-only CASM model can safely be designed around:
   - CSEG/Sdec;
   - common Ctab/Ctb2 mapping;
   - Ctb2 zones;
   - raw chord/note masks;
   - source root/type;
   - NTR/NTT/Bass flag;
   - HighKey/limits/RTR;
   - raw trailing unknown bytes.
3. Do **not** impose `<= 8` active source channels.
4. Do **not** collapse several Ctb2 sources that share a destination. They may be conditional alternatives, simultaneous layers, or both.
5. Preserve Ctb2 record identity and source-channel identity independently of destination Part.
6. Do not synthesize values for bytes 40..46.
7. Parse/preserve `Ctab` and `Cntt` for compatibility even though the current factory SFF2 corpus contains only Ctb2.
8. Treat UI presets from MIDI Song to Style as higher-level bundles until controlled experiments map them.
9. Keep conversion-stage (`Convert to Style Parameter`) semantics separate from playback-stage (`Style Playing Parameter`) semantics in the data model and documentation.
10. A future CASM writer should first target byte-exact no-edit roundtrip and one-field controlled edits before general semantic regeneration.

## Next controlled experiments

Priority order after the reference audit:

1. **HiKey delta** — confirm Ctb2 Main-zone offset 30 causally.
2. **HiLim delta** — confirm offset 32 causally.
3. **Style Playing Parameter matrix** — same musical material, vary one preset at a time; diff both MIDI and CASM and test candidate NTR/NTT/RTR mappings above.
4. **Convert to Style Parameter matrix** — determine whether each choice changes generated MIDI, CASM, or both; do not assume a one-field mapping.
5. **Section Key** — map STS Section Key into Source Root/Source Chord and/or generated MIDI behavior.
6. **Lo Zone split** — map STS split value to Ctb2 payload offset 20 and verify low-zone field use.
7. **Hi/MegaVoice behavior** — determine whether the STS Mega Voice option materializes in Ctb2[40..46], zone NTR/NTT, MIDI, or another field.
8. **Chord-mask controlled playback** — create a tiny Style with two source channels mapped to one destination and complementary masks; verify on PSR-SX600.
9. **Simultaneous-layer controlled playback** — two source channels mapped to one destination with identical permissive masks and overlapping notes; verify that both sound.
10. **Legacy compatibility fixture** — obtain a small known SFF1 Style containing `Ctab`, ideally also one with `Cntt`, for parser regression.

## Remaining source gaps

The research is no longer blocked by missing documentation, but two historically useful sources are not fully available:

- Reinhold Pöhnl, *Styles & Patterns*: only bibliographic information and quotations/summary through Wierzba/Bedesem were available. A legal personal copy would be useful for deeper NTR/NTT musical behavior, but is not required for structural work.
- Source code for Jørgen Sørensen's historical CASM Editor (and the older Evgeny Osenenko CasmEdit) was not found. The programs/documentation are useful historical evidence, but they should not be reverse-engineered or treated as open source without a verified source/license.

If either source becomes available legitimately, it should be added to the inventory and audited for provenance before using it.

## Research boundary

This document intentionally stops before implementing CASM write support or a full Yamaha transposition engine. The next engineering step, when prioritized, should be a **read-only CASM report with raw evidence**, followed by controlled field editing. The current project priority on STS unpack/repack remains unchanged.
