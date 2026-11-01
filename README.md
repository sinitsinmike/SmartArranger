# Smart Arranger

Smart Arranger is an experimental AU MIDI effect for Logic Pro, developed as a Yamaha-style live arranger. The first target profile is the PSR-SX920/SX720 family and supported SFF/SFF GE style files; compatibility still needs file-by-file verification.

## Implemented prototype

- JUCE AU MIDI effect passes incoming MIDI through and shows a preliminary live chord estimate.
- Loads a user-selected Yamaha-style file and reads its title, tempo, meter, sections, and MIDI events. Selected CASM records are decoded for inspection while unknown bytes are retained.
- Saves the selected style path and section with the Logic project.
- Plugin START/STOP is independent of Logic transport. Its clock follows host tempo/meter when available, otherwise style metadata.
- Plays style sections: Intro plays once then moves to Main; Fill and Break play once then return to Main; Ending plays once and stops. Section requests are queued to the next bar during looping playback or until a one-shot section ends.
- Without a loaded style, a simple demonstration bass-and-chord pattern is available.
- The editor groups section buttons in arranger order, places Intro A/B together, uses plain START/STOP text, and lists MIDI channels, CASM part labels, and embedded GM Program Changes when present.

## Not implemented or limited

- CASM is not yet applied as per-part Yamaha NTR/NTT/RTR harmony rules. Imported notes still use a global C-major-reference prototype.
- Chord recognition is a common pitch-class heuristic across input channels. Yamaha fingering modes, configurable chord/bass channel masks, keyboard zones, and complete slash-chord handling are not implemented.
- The channel summary shows a GM patch only when a standard MIDI Program Change is present. Yamaha bank/voice SysEx and model-specific voice names are not decoded; missing patch data is shown as unspecified.
- Style parts are not routed to separate Logic instruments. A MIDI effect cannot create tracks or instantiate Logic instruments by itself.
- Section transitions, chord response, note cleanup, style loading, and host timing need broader verification with real styles in Logic. No Yamaha style files are bundled.

## Build and compatibility

- CI target: Intel x86_64 AU with a macOS 10.13 deployment target.
- The previous Intel AU build ran in Logic. Source archive v24 contains the latest section layout and channel-summary updates and still needs its own build and Logic check.
- Place the AU MIDI effect in Logic's MIDI FX slot before a software instrument.
- See [implementation status](docs/implementation-status.md), [compatibility research](docs/compatibility-research.md), and [the original brief](Smart_Arranger.md).

## Next work

1. Verify the v24 UI/build update in Logic.
2. Apply per-part CASM NTR/NTT/RTR rules and map style parts to MIDI sources.
3. Improve Yamaha chord recognition and predictable section transitions.
4. Design and verify multitimbral routing for Logic.
5. Add optional Logic sync and MIDI recording modes.

## Development note

User-provided style files are local test fixtures. They are not committed or bundled unless redistribution is expressly permitted.
