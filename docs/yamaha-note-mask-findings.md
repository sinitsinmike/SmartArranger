# Yamaha note-mask research — stage result

## Question

How should the 12-key Note Mute / Active Keys mask affect a style part, and is that rule verified for PSR-SX920/SX720 SFF2 styles?

## Evidence

1. The repository copy of *Style Files — Introduction and Details*, v2.1, describes old CASM Format 1 Note Mute in section 5.2.4: bit 1 plays notes in the corresponding pitch class; bit 0 suppresses them. This is a reverse-engineered third-party guide, not an official Yamaha specification. Source: `Info and sources/StyleFileDescription_v21.pdf`.
2. The 2013 *Yamaha Keyboards – Style Creation Course* identifies Active Keys as a CASM function and Note Mute as a mask of allowed notes. It covers an older CASM Format 1 and warns that the CASM format changed substantially from Tyros 3 onward. Source: `docs/research/yamaha-style-engine-casm-course-analysis.md` in the GitHub repository.
3. The project's CASM reference maps Ctb2 payload offsets 11–12 to a 12-pitch-class mask, but says detailed playback has not been controlled-tested. This is structural evidence, not proof of the behavior on the target generation.
4. The official PSR-SX920/SX720 Reference Manual explains SFF note conversion, but does not specify Note Mute bit polarity or Ctb2 offsets. [Official manual](https://data.yamaha.com/files/download/other_assets/5/2328055/PSR-SX920_reference_manual_En_B0.pdf).

## Confidence

| Claim | Status |
|---|---|
| Old CASM Format 1: bit 1 enables a pitch class, bit 0 suppresses it | Supported by a reverse-engineered guide |
| Ctb2 offsets 11–12 are structurally the analogous mask | Structurally corroborated in project research |
| PSR-SX920/SX720 Ctb2 uses the same polarity and filtering stage | Not verified |
| Whether filtering is against source pitch or converted pitch | Not established |

## Implementation decision

Do not yet apply Note Mute to the SFF2 Ctb2 playback path as a confirmed Yamaha rule. This would turn a plausible SFF1-to-SFF2 continuity assumption into a compatibility guarantee.

Decisive evidence would be a controlled pair of SX920/SX720-compatible styles that differ only in one Active Keys bit, with playback or MIDI output captured, or a technical source that explicitly documents Ctb2/SFF2 Note Mute for this generation. Comparing behavior before and after chord transposition would distinguish source-pitch gating from converted-pitch gating.
