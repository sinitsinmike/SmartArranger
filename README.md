# Smart Arranger

Smart Arranger is an experimental AU MIDI effect for Logic Pro, being developed as a Yamaha-style live arranger. The first target profile is the PSR-SX920/SX720 family and supported SFF/SFF GE style files; compatibility still needs file-by-file verification.

## Implemented prototype

- JUCE AU MIDI effect passes incoming MIDI through and displays a preliminary live chord estimate.
- Loads a user-selected .STY file and reads its title, tempo, meter, sections, and MIDI events. Selected CASM records and fields are decoded for inspection while unknown bytes are retained.
- Saves the selected style path and section with the Logic project.
- Plugin START/STOP is independent of Logic transport. The arranger clock follows host tempo/meter when available, otherwise style metadata.
- Plays the selected style section. Intro plays once and moves to Main; Fill and Break play once and return to Main; Ending plays once and stops. Requests during a one-shot section wait until it finishes; requests during a looping section queue for the next bar.
- Without a loaded style, the plugin can generate a simple demonstration bass-and-chord pattern.
- Vintage arranger-inspired interface with style name, section buttons, and live chord display. See [mockup/](mockup/).

## Not implemented yet

- CASM parsing is not yet used to apply Yamaha per-part NTR/NTT/RTR harmony rules. Imported style notes still use a global C-major-reference prototype, so harmony may be wrong for some parts and styles.
- Chord recognition is a common pitch-class heuristic across incoming MIDI channels. Yamaha fingering modes, selectable chord/bass channel masks, keyboard zones, and full slash-chord handling are not implemented.
- Style channels are not mapped to instrument parts or separate Logic instruments; the MIDI effect cannot instantiate patches or create tracks.
- The current source archive v21 has not been rebuilt or validated in Logic. The new CASM decoder and chord-state changes may compile, but need a build and host testing before relying on them.
- No Yamaha style files are bundled.

## Build and testing

- CI target: Intel x86_64 AU with a macOS 10.13 deployment target. The workflow uses the versioned source archive and can be started manually when ready.
- Place the AU MIDI effect in Logic's MIDI FX slot before a software instrument.
- See [docs/implementation-status.md](docs/implementation-status.md) for the detailed implementation record and [Smart_Arranger.md](Smart_Arranger.md) for the original brief.
