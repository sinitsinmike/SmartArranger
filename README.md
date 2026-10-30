# Smart Arranger

Smart Arranger is an experimental AU MIDI effect for Logic Pro, being developed as a style-based live arranger.

## Implemented

- AU MIDI effect built with JUCE 9.0.4.
- Passes incoming MIDI through and shows a preliminary live chord estimate.
- Loads user-selected Yamaha-style files and reads title, tempo, meter, sections, and MIDI events.
- Saves the selected style path and section in Logic project state.
- Plugin START/STOP controls accompaniment independently of Logic transport. The arranger clock uses host tempo/meter when available, otherwise style tempo/meter.
- Plays and loops the selected section; when no style is loaded, generates a basic bass-and-chord pattern.
- Imported style notes follow the detected chord root and basic triad quality, changing harmony at arranger bar boundaries.
- Vintage arranger-inspired interface; see [mockup/](mockup/).

## Try it in Logic

Load a style, choose a section, then press START in the plugin. This starts or stops the arranger only; it does not start or stop Logic transport. The selected section repeats until STOP or another section is selected. Logic still hosts MIDI/audio processing and supplies tempo/meter when available. Intro currently repeats; automatic Intro-to-Main flow is not implemented.

## Known limitations

- The chord transform is a global prototype that assumes source style notes are centered on C major. It does not implement Yamaha per-part CASM/NTR/NTT/RTR rules; passing chords, melody notes, and some styles may not harmonize correctly.
- Logic must process the track and route the MIDI FX output to an instrument. Whether Logic continuously calls this AU while transport is stopped still needs direct verification.
- Section buttons directly select a repeating section; queued transitions, automatic fills, breaks, and endings are not sequenced.
- Style parts are not mapped to instrument patches or separate Logic tracks.
- Chord recognition and style playback need more end-to-end musical verification in Logic. Five user-supplied SFF1/SFF2 files were parser smoke fixtures only.
- No Yamaha style files are bundled.

## Build and compatibility

- Current CI target: Intel x86_64 AU, macOS 10.13 deployment target.
- The AU MIDI effect belongs in Logic MIDI FX before a software instrument.
- See [docs/implementation-status.md](docs/implementation-status.md) for implementation detail and [Smart_Arranger.md](Smart_Arranger.md) for the original brief.

## Next work

1. Build and verify the v19 AU in Logic with transport stopped and running; confirm live chord changes, tempo, and note cleanup.
2. Replace the global C-major-reference transform with Yamaha per-part CASM/NTR/NTT/RTR behavior and improve voicing.
3. Add queued section flow: Intro once, then selected Main; queue fills/breaks and end cleanly. Keep arranger START/STOP separate from Logic transport.
4. Map style channels to parts and design a practical multi-instrument Logic routing workflow.
5. Polish the small section-grid and accompaniment-toggle spacing issues recorded in [docs/implementation-status.md](docs/implementation-status.md).
6. Improve chord recognition, state restoration, and supported host/macOS compatibility checks.
7. Add an optional on-screen keyboard.

## Development rules

- Treat user-provided styles as local test fixtures; only commit them when redistribution is confirmed.
- Do not claim host or style-format support until tested in Logic with real files.
