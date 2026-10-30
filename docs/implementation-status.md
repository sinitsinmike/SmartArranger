# Implementation status

## Increment 1: live chord display

- Observe Note On/Off events in the incoming MIDI stream without modifying the stream.
- Track held notes by MIDI channel and note number.
- Recognize common major, minor, diminished, augmented, suspended, and seventh-chord pitch-class patterns.
- Display the current estimate in the AU editor.
- Keep the audio callback free of UI locks; the editor polls atomic chord state.

### Current limits

- The detector examines all held notes and can misidentify extensions, melody notes, inversions, or ambiguous pitch collections.
- The chord detector provides the harmony estimate used by the basic generator, but is not yet used to adapt imported style material.
- No `.STY` parser, style playback, section transitions, or multi-part routing is implemented.
- Compilation proves the AU target builds, not that this detector has been musically validated in Logic.

## Increment 2: style metadata inspection

- Add a file picker for `.STY` files.
- Parse the leading MIDI header and one format-0 MIDI track using bounded readers.
- Read tempo, meter, section marker ranges, MIDI channels containing notes, and whether a CASM block exists.
- Preserve channel MIDI events with absolute ticks, raw status/data bytes, and section index for the playback engine.
- Skip length-prefixed non-MIDI sections without attempting to interpret their rules.

### Current limits

- The parser source has been compiled with a lightweight JUCE API shim and run against five local user-supplied style files: four SFF2 and one SFF1, all parsed with 16 sections and nonempty MIDI event lists. These fixtures are not verified as official Yamaha styles and are excluded from the repository and build archive.
- The parser has not yet been built against JUCE itself or exercised through the AU in Logic, so the end-to-end loader/playback path remains unverified.
- It does not interpret CASM/NTR/NTT/RTR or map MIDI channels to Yamaha part names.
- The event playback stage below schedules one selected section unchanged; it does not adapt notes to performed chords.

## Increment 3: host-synchronized basic accompaniment

- Add an opt-in accompaniment switch in the editor.
- Keep the most recently recognized chord after the performed chord is released.
- Generate a simple bass-and-chord fallback pattern, clocked to host PPQ position and tempo.
- Schedule generated MIDI from Logic's PPQ position and tempo; release generated notes when stopped or disabled.

### Current limits

- This is a fixed demonstration pattern, not a Yamaha style playback engine.
- Chord changes inside a MIDI block are applied at block granularity.
- Voicing, voice-leading, fills, sections, part assignment, and CASM rules are not implemented.
- Host PPQ/tempo delivery and generated MIDI behavior still need direct Logic verification.

## Increment 4: host parameter and project state

- Expose accompaniment enable as a JUCE parameter, allowing Logic automation.
- Bind the editor toggle to the parameter attachment.
- Save and restore the parameter through the processor state so projects reopen consistently.

## Increment 5: host-synchronized playback of one style section

- Add a section selector populated from the loaded style markers.
- Copy the selected section's MIDI channel events into an immutable playback buffer off the audio thread, then publish it with a two-slot reader-count handoff.
- Schedule the section's MIDI events against the Logic PPQ position and host tempo, and loop the selected section continuously while accompaniment is enabled.
- Preserve event channels and one- or two-data-byte MIDI messages; stop generated notes when playback stops, the host seeks, the style changes, or accompaniment is disabled.
- If no valid style is loaded, retain the basic generated accompaniment as the fallback.

## Increment 6: persist style selection in the Logic project

- Save the loaded style's local file path and selected section index in the processor state.
- Restore the file and section from the editor after the host restores the project state.
- Keep style files out of the repository and AU artifact; the user selects a local file.

## Increment 7: host-meter-aware fallback accompaniment

- Read the host time signature from JUCE `AudioPlayHead::PositionInfo` and default to 4/4 if it is unavailable or malformed.
- Place the demonstration accompaniment on a subdivision grid derived from the denominator and bar length from the numerator.
- Keep event timing tied to the host PPQ position and tempo.

## Increment 8: separate Intel and Apple Silicon CI artifacts

- Configure a macOS 10.13+ x86_64 AU build and a macOS 11.0+ arm64 AU build.
- Publish each architecture as a separately named GitHub Actions artifact for later Logic verification and release packaging.

### Current limits

- If the style file has moved or is unavailable, playback cannot resume until the file is available at the saved path or selected again.
- Style state restoration reads the file during host state restoration; this path has not yet been verified in Logic.
- The fallback rhythm remains a simple demonstration pattern; it does not yet use meter-specific musical phrasing or style data.
- The arm64 build and native Apple Silicon Logic loading still require CI and host verification.

### Current limits

- Playback is unmodified source MIDI: chord input does not yet transpose or otherwise adapt style notes.
- Only the selected section is looped; intro, fill, break, ending, and automatic transitions are not sequenced.
- Style selection restoration still needs direct Logic verification.
- MIDI event scheduling and note cleanup still need direct verification in Logic Pro on Intel and Apple Silicon hosts.

## Next increments

1. Validate the style reader and section scheduler with real SFF1/SFF2 `.STY` files and harden them against format variants.
2. Map preserved MIDI channels to style parts and interpret Yamaha CASM/NTR/NTT/RTR rules so notes follow the performed chord.
3. Add tempo and meter aware section transitions, fills, breaks, intros, and endings after the single-section scheduler is verified.

## Visual polish backlog

Logic review of the Vintage editor found small layout defects to address in a later UI-only pass:

- The section button grid extends a few pixels beyond the left and right edges of its panel.
- The accompaniment toggle is too close to the first row of section buttons and appears slightly high.
- Rework the editor's horizontal margins and vertical rhythm together; these are visual issues and do not block style loading or playback.
