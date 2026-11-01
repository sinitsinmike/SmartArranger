# Implementation status — source archive v21

## Present in the prototype

- JUCE AU MIDI effect passes input MIDI through and displays a preliminary live chord estimate.
- Loads a user-selected Yamaha-style file, reads title/tempo/meter/sections/MIDI, and preserves the selected path and section in Logic project state.
- Plugin START/STOP is independent of Logic transport. Clock uses host tempo/meter when available, otherwise style metadata.
- Main sections loop. Intro, Fill, and Break play once; Ending plays once then stops. Requests made during a one-shot section wait for that section to finish; requests during a loop queue for the next bar.
- Without a style, a simple fallback bass/chord pattern is available.

## Added to source archive v21; not built yet

- Preserve raw CASM/CSEG records and decode selected Sdec, Ctab, Ctb2, and Cntt fields for inspection. Unknown payload bytes remain preserved and uninterpreted. The observed 27/31-byte Ctab and 47-byte Ctb2 layouts are scoped profiles, not universal format guarantees.
- Keep the lowest held note separate from the detected chord root; this does not yet implement Yamaha slash-chord or independent input-channel behavior.
- The GitHub Actions workflow now unpacks and builds source archive v21. No CI run or local build has yet verified these edits.

## Remaining for the PSR-SX920/SX720 target

- Apply per-part CASM NTR/NTT/RTR transformations; current style harmonization is a global C-major-reference prototype.
- Map CASM sections and parts to their MIDI sources, masks, layers, and output channels.
- Implement selectable chord-detection modes/channel masks, bass input, and reliable handling of melody notes and slash chords.
- Provide a tested multi-instrument Logic routing workflow. A MIDI effect cannot instantiate patches or create tracks by itself.
- Validate supported SFF/SFF GE styles, chord changes, section transitions, MIDI note cleanup, host tempo/meter, and state restoration in Logic. No official SX920/SX720 style corpus has been used for end-to-end validation yet.
- Future recording/synchronization modes remain design goals: record generated arrangement or control MIDI, and optionally sync arranger transport/tempo with Logic.

## Visual backlog

- The section-button grid protrudes slightly beyond its panel, and the accompaniment toggle sits too close to the first row. Rework horizontal margins and vertical spacing together.
