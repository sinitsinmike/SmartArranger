# Smart Arranger — CASM batch analysis: six Yamaha `.STY` files

## 1. Files inspected

All six files were read as binary data. Each has a Standard MIDI header/track followed by Yamaha extension chunks. All six contain `CASM` and `OTSc` chunks.

| File | File size | CASM payload | CSEG groups | Ctb2 records |
|---|---:|---:|---:|---:|
| 01 LOUNGE 1.sty | 25,853 B | 1,374 B | 2 | 22 |
| 02 SOUL BEAT.sty | 29,750 B | 1,099 B | 2 | 17 |
| 03 DANCE TWIST.sty | 39,941 B | 1,664 B | 3 | 27 |
| 04 CLUB TABLA.sty | 34,360 B | 989 B | 2 | 15 |
| 05 DOWN DANCE.sty | 29,401 B | 1,319 B | 2 | 21 |
| 06 UK PUNJABI.sty | 34,649 B | 1,154 B | 2 | 18 |
| **Total** | **188,954 B** | **7,599 B** | **13** | **120** |

## 2. Structural result

All six files use the same visible high-level CASM organization:

```text
CASM
  CSEG
    Sdec
    Ctb2
    Ctb2
    ...
  CSEG
    Sdec
    Ctb2
    ...
```

`DANCE TWIST` has three CSEG groups; the other five have two.

**Important:** in this six-file batch, I found `Ctb2` records, not legacy `Ctab` records. I did not find `Cntt` chunks. This is consistent with the distinction we have been making between the older CASM Format 1 described in the 2013 course and the later Ctb2 dialect. It does not, by itself, identify the exact Yamaha model or firmware generation.

Every Ctb2 chunk in this batch has a declared payload length of **47 bytes**. There are 120 such records in total. No 27-byte or 31-byte Ctab records occur in these six files.

## 3. Sdec grouping

The first CSEG in five files groups the usual Main/Fill/Intro A/Ending A material; the second groups Intro B/C and Ending B/C. `DANCE TWIST` separates `Fill In DD` into its own middle CSEG:

| File | CSEG 1 style-part names | CSEG 2 | CSEG 3 |
|---|---:|---:|---:|
| LOUNGE 1 | 11 | 4 | — |
| SOUL BEAT | 11 | 4 | — |
| DANCE TWIST | 10 | 1 | 4 |
| CLUB TABLA | 11 | 4 | — |
| DOWN DANCE | 11 | 4 | — |
| UK PUNJABI | 11 | 4 | — |

The actual Sdec strings are preserved in the raw files. In DANCE TWIST, the isolated second group is exactly `Fill In DD`.

## 4. A first useful comparison

The first Ctb2 records in several styles are highly similar for the Rhythm channels. For example, the first Rhythm 1 record in LOUNGE 1, SOUL BEAT, CLUB TABLA, DOWN DANCE and UK PUNJABI begins with the same 47-byte payload pattern apart from the voice-name spelling/capitalization in some files and a small number of trailing bytes in particular styles.

This is useful because it gives us a control group: common records can help identify stable field positions, while deliberately different records (Bass, Chord, Phrase, and DANCE TWIST's special Fill In DD group) can expose fields that change with CASM behavior.

A few non-Rhythm records visibly differ in bytes in the region that appears to encode channel settings. Examples include:
- Bass records with `81` appearing in a repeated setting sequence;
- Chord records with `02` in a corresponding sequence;
- some records with `5F` where other files have `7F`;
- records with nonzero bytes near the end that are absent in some otherwise similar records.

These are **byte-level observations only**. I am not assigning definitive field names to those offsets yet. The 2013 Format 1 table is not a safe offset map for Ctb2.

## 5. What this batch establishes

1. These six `.STY` files are a coherent Ctb2 sample set: all have 47-byte Ctb2 payloads.
2. CSEG grouping is not fixed at two groups; one file uses three.
3. Sdec controls which style parts share a CSEG's settings.
4. The files provide many repeated Rhythm records for differential analysis.
5. The legacy `Ctab`/`Cntt` layout from the Style Creation Course should not be applied directly to these Ctb2 payloads.

## 6. Next reverse-engineering step

The next step should be a controlled byte-diff matrix, not speculative decoding:

- align records by channel role (Rhythm 1/2, Bass, Chord 1/2, Pad, Phrase 1/2);
- compare the same role across all six styles;
- compare the same role across different CSEG groups within one file;
- mark constant and variable byte offsets;
- correlate each changing byte with the source MIDI channel, voice label, and actual notes in the corresponding MIDI part;
- only then propose field meanings and test them against additional styles.

For each proposed field, record: offset, observed values, sample files, supporting correlation, confidence, and a regression fixture.

## 7. Caution

This report does **not** claim that the meaning of every Ctb2 byte has been decoded. It records the verified container/chunk structure and the observed byte patterns. No Ctb2 offsets have been borrowed from the legacy Ctab table.

