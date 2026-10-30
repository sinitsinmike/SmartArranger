# Smart Arranger

Smart Arranger is an experimental AU MIDI effect for Logic Pro. It is being developed as a style-based live arranger: play chords on a MIDI keyboard and have accompaniment follow them.

## Implemented

- AU MIDI effect target for Logic Pro, built with JUCE 9.0.4.
- Passes incoming MIDI through and shows a preliminary live chord estimate.
- Loads a user-selected Yamaha-style file from disk and reads its title, tempo, meter, section markers, and MIDI events.
- Saves the style path and selected section in the Logic project state.
- Plays and loops one selected style section in sync with Logic's tempo and transport. The section button grid is populated from the loaded style.
- Provides a host-synced demonstration bass-and-chord pattern when no style is loaded.
- Uses a dark arranger-inspired panel with the loaded style name and section buttons.

## Known limitations

- Imported style notes currently play unchanged. Live chords are detected and displayed, but do not yet transpose or reharmonize style parts.
- Yamaha CASM, NTR, NTT, and RTR rules are not interpreted.
- Section buttons select a loop directly; queued transitions, automatic fills, intro-to-main flow, breaks, and endings are not sequenced yet.
- Style parts are not yet mapped to instrument patches. A MIDI effect cannot directly create Logic tracks or instantiate Logic instruments; instrument routing is a separate design task.
- The style loader/playback and chord timing still need more end-to-end musical verification in Logic. The parser has been smoke-checked against five local user-supplied SFF1/SFF2 files.
- No Yamaha style files are bundled in the repository or build artifacts.

## Build and compatibility

- Current continuous-integration target: Intel x86_64 AU, macOS 10.13 deployment target.
- The Intel AU has built successfully in GitHub Actions. A previous Apple Silicon build also succeeded, but CI is temporarily Intel-only while the current UI and behavior are stabilized.
- The AU is a MIDI effect; place it in Logic's MIDI FX slot before a software instrument.
- See [docs/implementation-status.md](docs/implementation-status.md) for implementation detail and [Smart_Arranger.md](Smart_Arranger.md) for the original product brief.

## Next work

1. Verify the latest Intel AU's text rendering, style loading, section buttons, and playback in Logic.
2. Implement chord-following for style parts, starting with root/quality-aware note transformation, then add per-part Yamaha CASM/NTR/NTT/RTR behavior.
3. Add musical section transitions: Intro, Main A-D, Fill, Break, and Ending with bar-aware switching.
4. Map style channels to parts and design a practical Logic instrument-routing workflow for multi-part playback.
5. Improve chord recognition, voicing, note cleanup, state restoration, and compatibility checks on supported Logic/macOS versions.
6. Add an optional on-screen keyboard with chord-zone and melody-zone highlighting.

## Development rules

- Treat user-provided style files as local test fixtures; do not commit them unless redistribution is expressly permitted.
- Do not claim support for a format or host behavior until it is tested with real files in Logic.
