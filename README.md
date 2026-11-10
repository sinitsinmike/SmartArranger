# Smart Arranger

Smart Arranger is an experimental AU MIDI effect for Logic Pro. It is being developed as a style-based live arranger: play chords on a MIDI keyboard and have accompaniment follow them.

An experimental macOS Standalone diagnostic app is also available. It uses the same processor and style parser, exposes a CoreMIDI virtual output named `Smart Arranger MIDI Out`, and displays style part/output-channel information plus live note-on counts for MIDI channels 1-16. In the standalone app, an optional MIDI keyboard is used only for chord control; its raw notes are not passed to the output. The arranger starts and stops from its own control.

The standalone app also follows the current macOS default audio output, including when headphones are connected or disconnected while it is running. Its macOS **Settings** menu controls optional internal SF2 playback and the keyboard split point (default C3): notes at or below the split control accompaniment, while higher notes play as melody. Choose a GM or XG `.sf2` and enable **Internal Sounds** to hear the generated MIDI through the Mac's audio output. Preset names and bank/program pairs are read from the chosen SoundFont; it is not bundled with the app. Right-click a MIDI output channel indicator to choose an SF2 preset or a General MIDI voice, or return to the style's own assignment. Manual assignment sends Bank Select and Program Change on that output channel. Press **Space** to start/stop and **1–0** to select the first ten visible sections. The selected **View** layout and section visibility are saved and restored when the standalone app restarts. SF2 feature coverage is limited by TinySoundFont; see [implementation status](docs/implementation-status.md).

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
- Standalone is built as Intel x86_64 with macOS 10.13 as its deployment target, intended for High Sierra Intel Macs. The app was compiled and launched on a newer macOS; runtime behavior on an actual High Sierra machine still needs confirmation.
- Build the diagnostic app with `cmake -S . -B build-standalone -DJUCE_DIR=/path/to/JUCE -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 -DCMAKE_OSX_ARCHITECTURES=x86_64`, then `cmake --build build-standalone --target SmartArranger_Standalone`.
- In Logic, choose `Smart Arranger MIDI Out` as the MIDI input on one or more instrument tracks. The standalone channel counters show which channels the shared engine sends; Logic recording confirms what arrives at the host.
- v34 source and a locally built Intel AU are in the repository. The v34 source is queued for a GitHub Actions build; the current artifact still needs validation in Logic.
- The AU is a MIDI effect; place it in Logic's MIDI FX slot before a software instrument.
- See [docs/implementation-status.md](docs/implementation-status.md) and [Smart_Arranger.md](Smart_Arranger.md).

## Android development build

- An Android Studio project is generated from `SmartArrangerAndroid.jucer` using the same JUCE 9.0.4 source tree as the macOS builds. Open `Builds/Android` in Android Studio, or build from a terminal with `cd Builds/Android && ./gradlew assembleDebug`.
- The current debug APK targets ARM64 Android devices, with Android API 26 as its minimum and API 35 as its target. The Android SDK, CMake 3.22.1, and NDK 28.1.13356709 are required.
- The current APK is a platform build of the shared desktop-oriented UI. Phone-first layout, an in-app Settings screen, device selection and Android hardware testing are still the next development steps; successful compilation does not yet mean the app is ready for everyday phone use.

## Next work

1. Build v34 in GitHub Actions and verify it in Logic, including CASM destination-channel routing, Note Off handling, and the supported bypass/mask cases.
2. Establish Yamaha note-mask playback behavior with controlled fixtures before implementing it.
3. Validate queued section flow in Logic with real styles: Intro once, Main variations, Fill/Break return, and Ending stop.
4. Add optional Sync Start and a separate Logic transport sync mode. Keep arranger START/STOP independent when sync is off.
5. Design a practical Logic instrument-routing workflow for multitimbral playback, including per-channel patch setup.
6. Improve chord recognition, voicing, note cleanup, state restoration, and compatibility checks.
7. Add an optional on-screen keyboard with chord-zone and melody-zone highlighting.
8. Add MIDI Learn and manual MIDI CC assignment for every arranger section and the Start/Stop control.
9. Plan an Android version with a phone-first compact screen: style-section buttons, Start/Stop, Load STY, Settings, tempo, meter, and the recognized live chord. Keep the instrument list off the phone screen to save space. Put Split Point and MIDI input/output device selection on a Settings page, listing available devices. Adapt the layout for tablets without shifted or overlapping controls.

## Development rules

- Treat user-provided style files as local test fixtures; do not commit or bundle them unless redistribution is expressly permitted.
- Do not claim support for a format or host behavior until it is tested with real files in Logic.
