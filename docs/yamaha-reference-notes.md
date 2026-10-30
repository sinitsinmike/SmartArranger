# Yamaha style behavior research notes

Status: preliminary source review. This is a research note, not a complete behavioral specification. No Yamaha-specific playback behavior should be considered implemented based on this note alone.

## Official references

- [PSR-SX900/SX700 Owner's Manual](https://data.yamaha.com/files/download/other_assets/1/1279261/psrsx900_en_om_b0.pdf)
- [PSR-SX900/SX700 Reference Manual](https://data.yamaha.com/files/download/other_assets/7/1279207/psrsx900_en_rm_b0.pdf)
- [PSR-S950/S750 Reference Manual](https://data.yamaha.com/files/download/other_assets/7/328237/psrs950_en_rm_a0.pdf)
- [Yamaha MIDI Song to Style](https://usa.yamaha.com/products/musical_instruments/keyboards/apps/midisong_to_style/index.html)

## Confirmed from the public material reviewed so far

- Yamaha's reference manuals include Style Creator / SFF Edit material. This documents settings for Yamaha's Style File Format (SFF); it is more useful than treating a style as a collection of unqualified MIDI loops.
- The SX900 reference material describes a style as sections (such as Intro, Main, Ending) made from source patterns, and includes SFF-related playback settings.
- Yamaha documents NTR (Note Transposition Rule) and NTT (Note Transposition Table) settings for adapting source-pattern notes to the recognized chord. The current Smart Arranger's global C-major-based transform does not implement these per-part rules.
- The SX900 Owner's Manual says that, when AUTO FILL IN is enabled, pressing a MAIN VARIATION button during play invokes the Auto Fill In behavior.
- The SX900 Reference Manual describes recording Style playback with chord-change and section-change events. This is relevant to the future performance-recording design, but does not by itself specify Smart Arranger's Logic recording implementation.

## Still to verify directly in the manuals

- Exact timing and priority for pressing a different Main variation while a section is playing.
- Whether selecting Main during an Intro queues, interrupts, or otherwise changes the current Intro on SX900 and S950.
- Exact Fill In, Break, and Ending transitions, including the section button/lamp states and when each request is applied.
- The chord-fingering modes and detection boundaries that apply to SX900 and S950, including bass/inversion handling.
- How Yamaha handles rapid chord changes: for example, whether a sequence of recognized chords within one second can each affect accompaniment, or whether chord changes are quantized to a beat, measure, or other engine boundary.
- Whether Intro and Ending follow each live chord change, hold the starting chord, or use other section-specific harmony rules.
- How Yamaha emits note-offs and handles notes crossing pattern/section boundaries.
- Which behaviors are model-specific and which belong to the shared SFF playback model.

Until those items are verified from the relevant manual sections or tested on the target instruments, section scheduling and chord handling remain provisional. Do not describe the current prototype as Yamaha-compatible.
