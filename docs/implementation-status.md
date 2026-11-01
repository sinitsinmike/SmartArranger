# Implementation status — source archive v24

## In the working prototype

- AU MIDI effect passes input MIDI through and displays a preliminary chord estimate. It tracks held notes and retains the last recognized harmony after release.
- Loads a Yamaha-style MIDI file, reading title, tempo, meter, sections, channels, and events. It preserves raw CASM/CSEG records and decodes selected Sdec, Ctab, Ctb2, and Cntt fields for inspection.
- Saves the style path and selected section in the Logic project.
- START/STOP runs independently of Logic transport. Clock follows host tempo/meter when available, otherwise the style.
- Plays loaded sections. Intro plays once then moves to Main; Fill and Break play once and return to Main; Ending plays once then stops. Requests are queued at section/bar boundaries.
- The editor groups section buttons in arranger order, places Intro A/B together, and uses plain START/STOP text. Source v24 narrows and lowers the section grid and places START/STOP centered as a matching third-row button.
- The style summary reports MIDI channels, CASM part labels, and standard MIDI Program Changes when present.

## Known limits

- Harmony is still a global C-major-reference transform. CASM NTR/NTT/RTR rules are decoded for inspection but not applied per part.
- Chord recognition is a common pitch-class heuristic across input channels, not Yamaha's selectable fingering modes. Bass/channel masks, split zones, and reliable slash chords are not implemented.
- Instrument names are shown only when supported by embedded standard MIDI Program Changes. Yamaha bank/voice SysEx and model-specific voice names are not decoded; absent program data is shown as unspecified.
- Style channels are not routed to separate Logic instruments. A MIDI effect cannot create Logic tracks or instantiate patches.
- Section flow, chord response, note cleanup, loading, and host timing still need broader checks with real styles in Logic. No style files are bundled.
- CASM layout profiles are based on scoped observations, not a guarantee for every Yamaha generation or SFF dialect. Local smoke checks used five user-provided files, not a verified official SX920/SX720 corpus.

## Build and validation

- GitHub Actions targets Intel x86_64 AU with a macOS 10.13 deployment target and JUCE 9.0.4.
- The previous Intel build was confirmed working in Logic. Source v24 has the latest layout and channel summary and needs a new build and Logic check.
- Apple Silicon launch was confirmed on an earlier build; v24 remains Intel-only in CI.

## Next development steps

1. Verify source v24 in Logic, including the section grid, START/STOP label, channel summary, and playback.
2. Apply per-part CASM NTR/NTT/RTR transformations and connect sections to their CASM records.
3. Improve chord recognition and validate queued section transitions with supported Yamaha styles.
4. Design and verify a multitimbral Logic routing workflow.
5. Add optional Logic synchronization and MIDI recording modes.

## Future recording design

Support both generated-arrangement recording and control-MIDI recording (played chords and section actions). Add a separate Sync On/Off mode for Logic tempo, meter, and transport; these are design goals, not implemented features.

## Distribution note

User-provided styles remain local test fixtures and are not committed or bundled unless redistribution is expressly permitted.
