# Implementation status — standalone Main D NTT 7 fix (local build succeeded; playback check pending)

## Increment 1: live chord display

- Pass incoming MIDI through while tracking held notes by channel and pitch.
- Recognize common major, minor, diminished, augmented, suspended, and seventh-chord pitch-class patterns.
- Display the current estimate in the AU editor using atomic state polled by the UI.

### Current limits

- The detector examines all held notes across all channels; melody notes, inversions, extensions, and ambiguous collections can confuse it.
- Lowest held MIDI note is now reported separately from the detected chord root, so a bass inversion does not itself force the root estimate. This is only a partial improvement; selectable chord/bass input channels and Yamaha fingering-mode behavior remain unimplemented.
- Keyboard split zones and selectable fingering modes are not implemented.
- The chord detector and its musical behavior still need direct verification in Logic.

## Increment 2: Yamaha-style file inspection

- Load a user-selected style file and read its title, tempo, meter, section markers, MIDI channels, and event ranges.
- Parse the leading MIDI data with bounded readers and skip length-prefixed non-MIDI chunks.
- Preserve length-checked raw CASM records and nested records for each CSEG in the loaded style snapshot.
- Decode Sdec section names, the common Ctab/Ctb2 channel-table prefix, observed 27/31-byte Ctab transposition fields, and the documented 47-byte Ctb2 zones while retaining every original payload byte.
- Keep Cntt's observed two-byte pair opaque and preserve unrecognized record lengths without assigning playback meaning.
- Preserve the local style path and selected section index in project state.
- Display MIDI channels, CASM part labels, and embedded standard MIDI Program Change values in the editor when present. Unsupported Yamaha bank/voice data is not guessed.
- Group visible section buttons in arranger order so Intro A and Intro B are adjacent; START/STOP labels use plain text to avoid broken symbol rendering.

### Current limits

- The parser was smoke-checked against five local user-provided files (four SFF2 and one SFF1); they are not verified official Yamaha files and are not included in the repository or artifacts.
- The newer CASM field decoder has not yet been checked against those files or an official PSR-SX920/SX720 style corpus.
- Source v28 builds an immutable per-section view that links source MIDI channels to CASM destinations and retains all matching records. This is metadata only: playback does not yet apply the linked rules, chord masks, or multi-source layering. Section names currently match case-insensitively after trimming; aliases are not guessed.
- The 27/31-byte Ctab and 47-byte Ctb2 layouts are scoped profiles, not a claim that every Yamaha generation or SFF dialect has these exact lengths.
- Style load/restore and scheduling need broader real-file verification in Logic.

## Increment 3: accompaniment and style playback

- Generate a simple fallback bass/chord pattern when no style is loaded.
- Play MIDI events from a loaded style, preserving channels, program changes, and other non-note messages.
- Use an internal audio-block clock, following host tempo/meter when available and style metadata otherwise.
- Keep the plugin START/STOP control independent from Logic's transport.

### Current limits

- The fallback pattern is a demonstration, not a full arranger pattern.
- Whether Logic continues calling this MIDI effect while transport is stopped must be checked on the target setup.
- A MIDI effect cannot create Logic tracks or instantiate multi-part instrument patches by itself.

## Increment 4: basic chord-following

- Retain the latest recognized chord after input notes are released.
- Transform imported style notes from a C-major reference to the detected root and basic chord quality.
- Apply harmony changes at arranger bar boundaries and release notes using the output pitches that were actually started.

### Current limits

- This is a global transform; it does not implement Yamaha's part-specific CASM/NTR/NTT/RTR behavior.
- The current root detector is still a common-chord pitch-class heuristic; it is not yet validated against the PSR-SX920/SX720 chord recognition modes.
- Passing chords, independent melody treatment, voice leading, and per-part transposition are not implemented.

## Increment 6: conservative SFF1 bypass transposition

- For a section/source channel with exactly one CASM destination and consistent decoded SFF1 records, apply `NTT Bypass` with `NTR Root Trans` as a semitone shift from the record's source root to the current detected chord root.
- `NTT Bypass` with `NTR Root Fixed` keeps the authored pitches, as described in Yamaha's reference manual.
- Mapped channels with unsupported, conflicting, or multiple destination rules pass through unchanged. Unmapped channels retain the earlier global prototype transform.
- Note-offs use the output pitch recorded for their corresponding note-on.

### Current limits

- This increment handles only SFF1 Bypass + Root Trans/Root Fixed. It does not implement NTT chord conversion, SFF2 zones, chord/note mask gating, or RTR behavior.
- Multi-source layering and a complete Yamaha fingering/chord recognition model remain future work.
- v31 configured successfully but compilation found a C++ visibility error in the helper's reference to a private nested type. v32 fixed it and built successfully in GitHub Actions; its AU artifact is run #67, awaiting Logic check. v33 adds bounded SFF2 Bypass zone selection and is awaiting CI.

## Increment 7: chord-mask source selection

- Decode the mask bits documented for common chord types currently recognized by the plugin: major, minor, diminished, augmented, sus2/sus4, dominant 7, major 7, minor 7, half-diminished 7, and diminished 7.
- At a recognized chord, a mapped source channel plays only when its CASM mask enables that chord type. Separate source channels remain independent, so multiple enabled sources mapped to one destination can sound as simultaneous layers.
- Mask gating is permissive for unsupported chord types, conflicting records, and sources mapped to multiple destinations. Before the first detected chord, masks are not used to discard sources.

### Current limits

- The mask bit table comes from the reverse-engineered V2.1 reference and is corroborated by factory-style mask examples; behavior still needs controlled hardware comparison.
- Note masks, Yamaha's complete chord vocabulary/fingering modes, and Auto Start before the first chord are not implemented.
- General SFF2 zone NTR/NTT settings are not applied by this increment; see Increment 8 for the bounded bypass subset.

## Increment 8: limited SFF2 zone selection

- For one unambiguous mapped source part with identical 47-byte Ctb2 records, select Low, Main, or High by the source MIDI note and the decoded inclusive Main-zone bounds.
- Apply only NTT Bypass without Bass-On combined with Root Trans (shift by current root minus source root) or Root Fixed (retain authored pitch).
- Conflicting records, Guitar NTR, Bass-On, non-Bypass NTT, and malformed/unmapped Ctb2 data remain unchanged.

### Current limits

- High Key, note limits, RTR, Guitar NTR, and non-Bypass NTT details remain incomplete. For SFF2 `Root Trans + Melody NTT + Bass-On`, the engine now applies the source-root-to-live-root semitone shift so bass follows chord-root changes; chord-quality substitutions and slash-bass behavior are not implemented.
- The 12 pitch-class note mask is decoded but its playback effect remains uncertain; implement only after a controlled differential fixture establishes its exact gate.
- This is a bounded SFF2 increment, not general SFF2/GE compatibility.

## Targeted fix: SFF2 Root Trans + Harmonic Minor 5th

- In `Bridge Over Troubled Water.sty`, Main D maps source channel 6 (`PhrsMain`) to output MIDI 15. Its Main zone is `NTR=Root Trans`, `NTT=7 (Harmonic Minor 5th)`, with Bass-On disabled.
- The renderer previously passed this NTT through unchanged, which left that part in the authored key while the other mapped parts transposed.
- The renderer now transposes this combination by the signed root interval and applies the documented major/minor third change and fifth adjustment for diminished/augmented target chords. Other NTT values remain unchanged until implemented.
- This is a bounded interpretation of the Yamaha NTT description, not a bit-for-bit verified Yamaha engine. Confirm Main D in the standalone after the build.

## Increment 9: CASM destination-channel routing

- Route channel voice events from a source MIDI channel to its unique CASM destination channel, including notes, program changes, controllers, pitch bend, and channel pressure.
- Preserve source-channel playback when there is no mapping, the mapping is malformed, or the same source channel has conflicting destinations.
- Track the actual output channel for each active style note so its Note Off and stop/transition cleanup use the same channel as its Note On.

### Current limits

- This only emits MIDI on the logical destination channels. It does not create Logic tracks, configure a multitimbral instrument, or map Yamaha voice/bank data to patches.
- Multiple sources can map to the same destination. Their same-pitch overlaps and program-change interactions need controlled MIDI/Logic verification.
- The v34 AU compiled locally for x86_64 with JUCE 9.0.4 and a macOS 10.13 deployment target. It has not been installed or checked in Logic. GitHub integration write access is currently unavailable.

## Increment 10: initial SFF2 Root Fixed + NTT Chord mapping

- For an unambiguous SFF2 source using Root Fixed + NTT Chord with Bass-On disabled, decode a bounded set of common source chord types and identify authored chord tones relative to Source Root.
- Map those chord tones to the nearest pitch in the currently detected chord, preferring the higher pitch on equal-distance choices and applying the zone's Note Low/High limits. Non-chord/passing notes and unknown source chord types remain unchanged.
- This lets common chordal patterns respond to the live chord without changing unsupported NTR/NTT combinations.

### Current limits

- Yamaha's PSR-SX920/SX720 Reference Manual describes the purpose of Root Fixed and NTT Chord but does not publish the complete note-to-note NTT algorithm. The implemented nearest-tone mapping is a conservative first approximation informed by the non-Yamaha Style Files v2.1 technical reference; it is **not verified bit-for-bit Yamaha behavior**.
- Duplicate chord tones, exact voice allocation for 4/5-note source chords, note masks, High Key, RTR, Guitar NTR, and the complete Yamaha chord vocabulary are not resolved here. Compare against a PSR-SX920/SX720 or a trusted reference renderer before claiming Yamaha-compatible voicing.
- The first target is the user-provided `Bridge Over Troubled Water.sty` Main A Ctb2 case (Source Root C, Source Chord Maj7, Root Fixed + Chord). Other SFF2 cases use the same conservative subset only when their metadata is recognized.

## Increment 11: SFF2 Root Trans bass-root following

- For a decoded `Root Trans + Melody NTT + Bass-On` zone, transpose the authored notes by the difference between the style's Source Root and current chord root. This fixes bass parts that previously stayed in the authored key because Bass-On zones were returned unchanged.
- This applies root movement only. Yamaha's full Melody NTT chord-quality conversion, slash-chord bass-note selection, and RTR behavior remain unimplemented and require reference validation.

## Increment 5: bar-aware section flow

- Load MIDI events for all named sections into an immutable playback snapshot.
- Queue section-button requests made during playback for the next bar boundary.
- Play Intro once, then move to the last selected Main variation (or the first Main if none was selected).
- Play Fill and Break once, then return to Main.
- Play Ending once, release active notes, and stop accompaniment.
- Loop Main and unclassified sections. Stop releases generated notes and clears pending transitions.

### Current limits

- Section roles are inferred from marker names; unusual naming may classify incorrectly.
- Fill/Break/Ending transitions and automatic Intro-to-Main flow still need validation with real Yamaha-style files in Logic.
- Transition timing currently uses the host meter when supplied, otherwise the style meter. Logic transport synchronization remains a future opt-in mode; current START/STOP remains independent.

## Future recording and synchronization modes

- Keep two recording options in scope: capture the complete generated MIDI arrangement, or capture the performed chord/control events and regenerate the style on playback.
- Add a separate Sync On/Off mode for Logic tempo, meter, and transport when recording. Preserve the independent arranger transport when Sync is off.
- Record section-button actions as replayable MIDI events or automation. This is a design target, not implemented behavior.

## Future controls and standalone interface

- Add manual MIDI CC assignment and MIDI Learn for every arranger section (Intro, Main, Fill, Ending, etc.) and Start/Stop. Mapping is not implemented yet.
- Android is a future target. Phone main screen should show only the essentials: style-section buttons, Start/Stop, Load STY, Settings, tempo, meter, and the recognized live chord; omit the instrument list to preserve space. A dedicated Settings page should offer Split Point plus MIDI input/output selection from available devices. Use a responsive layout for phones and tablets, keeping controls aligned and usable at both sizes.
- Windows is also feasible with JUCE, but no Windows target or artifact is configured yet. Add a Windows standalone CI job after separating desktop targets from macOS-only AU and deployment settings.
- Standalone now has a macOS `Settings` menu and an `Internal Sounds and SoundFont…` panel. It can load a user-selected SF2 file, enable/disable internal playback, and remember both choices.
- Standalone SF2 rendering uses TinySoundFont v0.9 (MIT). The selected file remains user-provided; no SoundFont is bundled. Settings reports how many of the 128 bank-0 programs exist, but that count alone does not certify a file's GM/XG mapping or sound quality.
- Standard MIDI Bank Select (CC 0/32) and Program Change messages from a style are sent to the internal SF2 renderer. Yamaha styles may store setup inside SInt; the latest preceding bank/program state is now copied into each section so direct section starts inherit the style's instrument assignments.
- Standalone keyboard shortcuts: Space toggles arranger start/stop; number keys 1–9 and 0 select the first ten visible sections in their displayed order. Hovering a MIDI output channel indicator shows the SF2 bank/program currently selected and whether bank fallback was used.
- The standalone app saves the selected View layout and visibility toggles for MIDI Controls, Style Parts, MIDI Out Activity, and Arranger, then restores them on the next launch.
- TinySoundFont does not implement all SoundFont 2 features (including modulators and chorus/reverb sends). This is a first practical playback engine, not a reference-grade Yamaha XG synthesizer. Yamaha-specific SysEx voice decoding remains unsupported; bank/preset selection works only when the loaded SF2 contains a matching bank and program.
- The shared editor's header is labelled `STYLE SELECTION`; product branding belongs in the app or plug-in window title rather than repeated inside that panel.

## Build and compatibility

- The current GitHub Actions workflow builds an Intel x86_64 AU with a macOS 10.13 deployment target.
- An earlier local source snapshot was configured and built with JUCE 9.0.4 and Apple Clang 15 as a Mach-O x86_64 AU with a macOS 10.13 deployment target. That bundle is not code-signed or notarized.
- The user confirmed that source v27 loaded in Logic and that sections switch and the arrangement responds to chord input. Source v28 added CASM metadata mapping; v29 added limited SFF1 bypass transposition; v30 added chord-mask source selection but its first workflow attempt failed before configuration because the archive omitted `CMakeLists.txt`. Source v31 corrected the package and exposed a compile error; v32 fixed it and built successfully in GitHub Actions. Source v33 adds bounded SFF2 zone selection and built successfully in GitHub Actions; Logic verification remains pending.
- Source v34 adds CASM destination-channel routing. Its Intel AU compiled locally using JUCE 9.0.4, Apple Clang 15, and the Command Line Tools SDK with a macOS 10.13 deployment target. It is not code-signed or notarized; GitHub Actions and Logic host validation remain pending.
- Source v27 layout was verified by the user in Logic: section buttons and long style-name sizing display correctly. Source v28 adds the section/channel CASM mapping; that new code still needs a build and host check.
- Previous Apple Silicon builds and the user's Logic launch demonstrate that the plugin can load in that tested setup; every new behavior still needs host verification.
- The standalone with Settings and initial SF2 playback support compiles locally for x86_64 with JUCE 9.0.4 and a macOS 10.13 deployment target. SF2 loading and audible playback still need to be checked on the user's Mac with a GM-compatible file.
- No style files are bundled with the repository or artifacts.

## Visual polish

- Source v28 keeps section buttons inside their panel for large styles; the editor grows vertically as needed. Long style names expand the editor horizontally, with the title card staying aligned to Load. START/STOP sits in the section-panel header, away from section labels and Live Chord text.
- Channel rows show readable CASM part labels and instrument names inferred from standard MIDI Program Changes. PC numbers are omitted; Yamaha bank and model-specific voice decoding remains future work.
