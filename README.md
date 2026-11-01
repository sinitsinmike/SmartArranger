# Smart Arranger

Smart Arranger is an experimental AU MIDI effect for Logic Pro. It is being developed as a style-based live arranger: play chords on a MIDI keyboard and have accompaniment follow them.

## Implemented

- AU MIDI effect target for Logic Pro, built with JUCE 9.0.4.
- Passes incoming MIDI through and shows a preliminary live chord estimate.
- Loads a user-selected Yamaha-style file from disk and reads its title, tempo, meter, section markers, and MIDI events.
- Saves the style path and selected section in the Logic project state.
- Plays style sections from the plugin's independent START/STOP control. Main variations loop; Intro plays once and moves to the selected Main; Fill and Break play once and return to Main; Ending plays once and stops.
- Queues section selections during playback: after a one-shot section finishes, or at the next bar boundary for a looping section. The section grid follows the loaded style.
- Builds a per-section CASM view associating source MIDI channels with matching Yamaha destination parts and retaining applicable rules, including shared CSEG records and channels without events in that section.
- Applies a limited SFF2 Low/Main/High zone choice by source-note range when a mapped zone uses NTT Bypass with Root Trans or Root Fixed.
- Routes style channel events to a unique CASM destination channel when available; ambiguous mappings retain the source channel. This includes channel voice events and keeps Note Off on the channel used by its Note On.
- Uses a Vintage arranger-inspired panel. The style-name card expands for long names, the channel summary lists readable CASM part labels and embedded GM instrument names when present, and the editor grows for larger section sets; see [mockup/](mockup/).

## Try it in Logic

Load a style and press START in the plugin. With an Intro selected, it plays once and moves to the current Main variation (or the first Main if none has been selected); with a Main selected, that variation loops. During playback, section selections are queued according to the transition behavior above. START/STOP controls only the arranger; it does not start or stop Logic's transport. Logic supplies tempo and meter when available. Logic still has to process the track and route MIDI FX output to an instrument.

## Known limitations

- Imported style notes follow the detected chord root and basic triad quality. A new harmony is applied at an arranger bar boundary; active notes are released using their actual transformed pitches.
- Chord following is still a prototype based on global C-major-reference transformations, not Yamaha's complete per-part CASM/NTR/NTT/RTR behavior.
- The CASM pass is deliberately limited: it handles selected SFF1/SFF2 bypass cases and chord-mask bits for chord qualities currently recognized by the plugin. Note masks, SFF2 NTT harmonization, RTR, Auto Start, Yamaha chord-mode recognition, and complete slash-chord handling remain unimplemented. Unsupported or conflicting mappings are handled conservatively. These rules still need validation with known styles.
- Section role recognition uses marker names; unusual names may not classify correctly. Queued transitions and exact bar alignment need broader Yamaha-style and Logic validation.
- CASM destination-channel routing does not create Logic tracks or configure instruments. Multitimbral playback requires a downstream instrument that responds to the routed MIDI channels and program changes.
- The channel summary reports CASM part labels and standard MIDI Program Change instruments when present. Yamaha bank/voice SysEx and model-specific voice names are not decoded; missing program data is not guessed.
- The parser has been smoke-checked against five local user-supplied SFF1/SFF2 files. Style playback, chord timing, note cleanup, and host behavior still need end-to-end verification in Logic.
- Logic must continue processing the track and route MIDI output to an instrument. Whether the AU MIDI effect is called continuously while Logic's transport is stopped needs direct verification.
- No Yamaha style files are bundled in the repository or build artifacts.

## Build and compatibility

- Current continuous-integration target: Intel x86_64 AU with a macOS 10.13 deployment target.
- v34 source and a locally built Intel AU are in the repository. The v34 source is queued for a GitHub Actions build; the current artifact still needs validation in Logic.
- The AU is a MIDI effect; place it in Logic's MIDI FX slot before a software instrument.
- See [docs/implementation-status.md](docs/implementation-status.md) and [Smart_Arranger.md](Smart_Arranger.md).

## Next work

1. Build v34 in GitHub Actions and verify it in Logic, including CASM destination-channel routing, Note Off handling, and the supported bypass/mask cases.
2. Establish Yamaha note-mask playback behavior with controlled fixtures before implementing it.
3. Validate queued section flow in Logic with real styles: Intro once, Main variations, Fill/Break return, and Ending stop.
4. Add optional Sync Start and a separate Logic transport sync mode. Keep arranger START/STOP independent when sync is off.
5. Design a practical Logic instrument-routing workflow for multitimbral playback, including per-channel patch setup.
6. Improve chord recognition, voicing, note cleanup, state restoration, and compatibility checks.
7. Add an optional on-screen keyboard with chord-zone and melody-zone highlighting.

## Development rules

- Treat user-provided style files as local test fixtures; do not commit or bundle them unless redistribution is expressly permitted.
- Do not claim support for a format or host behavior until it is tested with real files in Logic.
