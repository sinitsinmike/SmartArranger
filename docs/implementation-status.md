# Implementation status

## Increment 1: live chord display

- Observe Note On/Off events in the incoming MIDI stream without modifying the stream.
- Track held notes by MIDI channel and note number.
- Recognize common major, minor, diminished, augmented, suspended, and seventh-chord pitch-class patterns.
- Display the current estimate in the AU editor.
- Keep the audio callback free of UI locks; the editor polls atomic chord state.

### Current limits

- The detector examines all held notes and can misidentify extensions, melody notes, inversions, or ambiguous pitch collections.
- The chord detector provides the harmony estimate used by the basic generator and the initial style-note transform below.
- Compilation proves the AU target builds, not that chord recognition has been musically validated in Logic.

## Increment 2: style metadata inspection

- Add a file picker for `.STY` files.
- Parse the leading MIDI header and one format-0 MIDI track using bounded readers.
- Read tempo, meter, section marker ranges, MIDI channels containing notes, and whether a CASM block exists.
- Preserve channel MIDI events with absolute ticks, raw status/data bytes, and section index for the playback engine.
- Skip length-prefixed non-MIDI sections without attempting to interpret their rules.

### Current limits

- The parser has been smoke-checked against five local user-supplied style files: four SFF2 and one SFF1. They are not verified as official Yamaha styles and are excluded from the repository and build archive.
- The end-to-end loader/playback path still needs direct verification in Logic.
- It does not interpret CASM/NTR/NTT/RTR or map MIDI channels to Yamaha part names.
- The playback stage schedules one selected section and applies the global chord transform documented in Increment 9.

## Increment 3: basic accompaniment

- Add an opt-in accompaniment switch in the editor.
- Keep the most recently recognized chord after the performed chord is released.
- Generate a simple bass-and-chord fallback pattern from an internal audio-block clock, using Logic tempo/meter when available.
- Release generated notes when the plugin START/STOP toggle is stopped.

### Current limits

- This is a fixed demonstration pattern, not a Yamaha style playback engine.
- Chord changes inside a MIDI block are applied at block granularity.
- Voicing, voice-leading, fills, sections, part assignment, and CASM rules are not implemented.
- Host tempo/meter delivery and generated MIDI behavior still need direct Logic verification.

## Increment 4: host parameter and project state

- Expose accompaniment enable as a JUCE parameter, allowing Logic automation.
- Bind the editor toggle to the parameter attachment.
- Save and restore the parameter through the processor state so projects reopen consistently.

## Increment 5: playback of one style section

- Add a section selector populated from the loaded style markers.
- Copy the selected section MIDI events into an immutable playback buffer off the audio thread, then publish it with a two-slot reader-count handoff.
- Schedule section events against an internal clock advanced by audio blocks and loop while accompaniment is enabled; use Logic tempo/meter when available and style metadata otherwise.
- Preserve MIDI channels and messages; stop generated notes when plugin accompaniment is disabled or the selected style section changes.
- If no style is loaded, retain the basic generated accompaniment as the fallback.

## Increment 6: persist style selection

- Save the loaded style local path and selected section index in processor state.
- Restore the file and section from the editor after host state restoration.
- Keep style files out of the repository and AU artifact; the user selects a local file.

## Increment 7: host-meter-aware fallback accompaniment

- Read the host time signature from JUCE `AudioPlayHead::PositionInfo` and default to 4/4 if unavailable or malformed.
- Place the demo accompaniment on a subdivision grid derived from the denominator and bar length from the numerator.
- Use host meter and tempo when available while advancing from the plugin audio-block clock.

## Increment 8: separate Intel and Apple Silicon CI artifacts

- Configure a macOS 10.13+ x86_64 AU build and a macOS 11.0+ arm64 AU build.
- Publish each architecture as a separately named GitHub Actions artifact.

### Current limits

- Style state restoration and Apple Silicon loading still need direct verification in Logic.

## Increment 9: basic chord-following for imported styles

- Pass incoming MIDI through while retaining the latest recognized harmony.
- Transpose imported style notes from a C-major reference to the detected chord root, with basic third and fifth changes for common chord qualities.
- Apply harmony changes at arranger bar boundaries and send note-offs for active notes using the pitches that were actually started.
- Keep program changes, channels, timing, and other non-note style events unchanged.

### Current limits

- The transform is global and assumes source style notes are centered on C major. It does not decode Yamaha CASM, NTR, NTT, or RTR instructions, so some styles may not reharmonize correctly.
- Passing chords, part-specific rules, inversions, and voice-leading are not implemented.

## Increment 10: arranger transport independent of Logic transport

- The plugin START/STOP toggle controls style playback without starting or stopping Logic transport.
- Advance the style clock from processed audio blocks; follow host tempo/meter when available and use style tempo/meter otherwise.
- Apply chord changes on arranger bar boundaries rather than Logic playhead positions.

### Current limits

- Logic still has to process the instrument track and route MIDI FX output to an instrument. Whether Logic continuously calls this AU while transport is stopped must be verified on the target setup.
- START/STOP begins the selected section and loops it. Automatic Intro-to-Main and queued section transitions are not implemented.

## Next increments

1. Verify style reader, chord-following, plugin START/STOP with transport stopped, host tempo behavior, and note cleanup in Logic.
2. Map MIDI channels to style parts and interpret Yamaha CASM/NTR/NTT/RTR rules to replace the global chord transform.
3. Add queued flow: Intro once, then selected Main; add fills, breaks, and endings.

## Visual polish backlog

Logic review of the Vintage editor found small layout defects for a later UI-only pass:

- The section button grid extends a few pixels beyond the left and right edges of its panel.
- The accompaniment toggle is too close to the first row of section buttons and appears slightly high.
- Rework the editor horizontal margins and vertical rhythm together; these visual issues do not block style loading or playback.
