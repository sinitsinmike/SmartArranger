# Smart Arranger — Ctb2 byte-level comparison, batch of six Yamaha Styles

**Files:** `01 LOUNGE 1.sty`, `02 SOUL BEAT.sty`, `03 DANCE TWIST.sty`, `04 CLUB TABLA.sty`, `05 DOWN DANCE.sty`, `06 UK PUNJABI.sty`  
**Scope:** the 120 `Ctb2` records found in the six CASM chunks.  
**Offset convention:** all byte offsets below are zero-based from the start of the 47-byte Ctb2 payload.

## 1. Verified record inventory

| File | CSEG | Ctb2 |
|---|---:|---:|
| 01 LOUNGE 1.sty | 2 | 22 |
| 02 SOUL BEAT.sty | 2 | 17 |
| 03 DANCE TWIST.sty | 3 | 27 |
| 04 CLUB TABLA.sty | 2 | 15 |
| 05 DOWN DANCE.sty | 2 | 21 |
| 06 UK PUNJABI.sty | 2 | 18 |
| **Total** | **13** | **120** |

All 120 records have a declared Ctb2 payload length of 47 bytes. The CSEG payloads parse cleanly as nested chunks in these files.

## 2. Strong structural pattern in the first 11 bytes

The Ctb2 records share a highly consistent prefix:

| Zero-based offset | Observed content | Evidence / confidence |
|---:|---|---|
| 0 | Small integer, often 0x08–0x0F; varies for Intro/Ending groups | Strong candidate for source channel. The value follows the MIDI source-channel numbering pattern. |
| 1–8 | Eight-byte ASCII voice/part label, space padded | Directly observed. Examples: `Rhythm1`, `Bass`, `Chord1`, `Phrase1`. |
| 9 | Small integer, commonly 0x08–0x0F | Strong candidate for destination channel. It can differ from byte 0 in Intro/Ending groups. |
| 10 | `0x01` in all 120 records inspected | Constant in this sample; semantic meaning not inferred here. |

The source/destination interpretation is supported by cross-record patterns. For example, in Main/Fill groups `Bass` commonly has byte 0 = `0x0A` and byte 9 = `0x0A`; in Intro B/C groups a Bass record may have byte 0 = `0x02` while byte 9 remains `0x0A`. Rhythm records commonly use source/destination 8 or 9. This is consistent with source-channel redirection.

**Caution:** the first 11-byte layout is an observed Ctb2 pattern. Do not assume that every Ctb2 dialect in every Yamaha generation has exactly the same layout until tested on more fixtures.

## 3. Repeated channel-role signatures

The 8-byte label field provides a practical way to align records across files. The sample contains these labels (minor spelling/padding variants included):

| Role label | Number of records |
|---|---:|
| Bass | 18 |
| Chord1 | 14 |
| Chord2 | 16 |
| Pad | 17 |
| Phrase1 | 14 |
| Phrase2 | 13 |
| Rhythm1 / Rhyth1 | 9 |
| Rhythm2 / Rhyth2 | 14 |
| Other labels (`Gt`, `Heaven`, `Shaku`, `GrandPF`, `PAD`) | 5 |

The nonstandard voice labels are important: a parser must not assume the label itself is always one of the canonical eight part names. Preserve the raw eight bytes.

## 4. Byte variability by role

For each role, records were aligned by the visible eight-byte label and compared byte-by-byte. The following lists show offsets that vary within that role. Offsets 1–8 (the label) are excluded from these lists. Offset 0 may vary because the same part role can originate on different source MIDI channels.

| Role | Records | Variable offsets (zero-based) |
|---|---:|---|
| Bass | 18 | 0, 13, 14, 15, 16, 17, 19, 21, 29, 33, 34, 35, 37, 39, 42, 43, 44 |
| Chord1 | 14 | 0, 13, 14, 15, 16, 17, 19, 28, 29, 30, 31, 32, 33, 42, 43, 44 |
| Chord2 | 16 | 0, 13, 14, 15, 16, 17, 19, 28, 29, 30, 31, 33, 42, 43, 44 |
| Pad | 17 | 0, 9, 13, 14, 15, 16, 17, 19, 20, 23, 27, 28, 29, 30, 42, 43, 44 |
| Phrase1 | 14 | 0, 9, 13, 14, 15, 16, 17, 19, 28, 29, 30, 31, 33, 44 |
| Phrase2 | 13 | 0, 9, 13, 14, 15, 16, 17, 19, 22, 29, 30, 34, 42, 43, 44 |
| Rhythm1 / Rhyth1 | 9 | 44 |
| Rhythm2 / Rhyth2 | 14 | 41, 43, 44, 45 |

### What this suggests (not yet a decoded specification)

- Rhythm1 records are almost byte-identical in this sample; only offset 44 varies across the normalized Rhythm1 label group.
- Rhythm2 is also highly stable, with variation concentrated at offsets 41, 43, 44 and 45.
- Harmonic parts (Bass, Chord1/2, Pad, Phrase1/2) vary at several repeated positions, especially offsets 13–17 and 28–35.
- Offsets 42–44 vary in multiple harmonic roles and may be part of a shared trailing settings/flags region, but the current sample does not prove their meaning.
- Offset 19 varies in all harmonic roles listed above and is a candidate for a common channel-setting field. Its meaning remains unknown.

Do **not** label these offsets NTR, NTT, RTR, NTT Bass, Note Limit or High Key yet. That requires controlled styles with one known setting changed at a time, or an independent decoder with verified semantics.

## 5. Important concrete observations

### 5.1 Rhythm records form useful controls

The repeated Rhythm1 payloads across several styles are identical apart from a trailing byte at offset 44 in this batch. This gives us a useful baseline for detecting fields that are specific to harmonic conversion rather than generic channel metadata.

Rhythm2 records are also strongly conserved. Some files have different trailing values around offsets 41 and 43–45, so those bytes should not be discarded as padding.

### 5.2 Same part role can have different source channels

The same logical role can appear with different source channel numbers in different CSEG groups. Intro B/C and Ending B/C records often use source channels 1–7 while routing to destination channels 10–15. Main/Fill records more commonly use source channels 8–15.

This is direct evidence that the parser must preserve both source and destination channel values and must not infer one from the other.

### 5.3 Sdec grouping is not the same as a single channel layout

A CSEG can contain many style-part names, but its Ctb2 entries describe channel records. Some records repeat across groups, while other groups contain only a subset of parts. Do not assume each CSEG has exactly eight records.

## 6. What this analysis does and does not establish

### Established for these six files
1. 120 Ctb2 records, each with a 47-byte payload.
2. A stable eight-byte text label at offsets 1–8.
3. Byte 0 and byte 9 behave like source and destination channel fields.
4. Byte 10 is `0x01` in every record in this sample.
5. Repeated role-based patterns exist, especially for Rhythm channels.
6. Many harmonic-part differences are concentrated in repeated byte positions.

### Not established
- Exact offsets for NTR, NTT, NTT Bass, RTR, High Key or Note Limits.
- Meaning of the varying bytes at offsets 13–17, 19, 28–35 and 42–45.
- Whether the same 47-byte Ctb2 layout is used by all SFF2 generations.
- Whether trailing bytes are flags, reserved values, identifiers or another field group.

## 7. Recommended next experiment

The highest-value next input is not another random style. It is a controlled pair of styles exported from the same Yamaha instrument, where only one Style Creator setting changes.

Suggested procedure:
1. Make a copy of a test style.
2. Change only one channel's NTR setting (e.g. Root Trans → Root Fixed).
3. Save both versions.
4. Compare the corresponding Ctb2 record byte-for-byte.
5. Repeat for NTT, RTR, High Key, Note Limit and NTT Bass.
6. Record the exact model and firmware used to save the styles.

If the user does not have access to a Yamaha keyboard/editor that exposes these settings, the next best route is to compare additional files with reliable provenance and correlate Ctb2 values with the MIDI source-channel arrangement. That can narrow candidates but cannot prove the semantic field names by itself.

## 8. Parser implementation guidance

For Smart Arranger:
- parse Ctb2 using the chunk's declared payload size;
- preserve all 47 bytes verbatim;
- preserve source channel, raw label bytes and destination channel separately as provisional structural fields;
- retain original CSEG order and Sdec text;
- keep unknown bytes in the raw record;
- do not yet populate semantic NTR/NTT/RTR properties from guessed offsets;
- add fixtures for all six files and assert 120 Ctb2 records total.

**Conclusion:** the first useful Ctb2 byte-level map is emerging. The prefix has a consistent structure and the channel-role comparison reveals stable versus variable regions. We have not yet decoded the harmonic-conversion fields. The next breakthrough requires controlled parameter changes or another source that maps exact Ctb2 bytes to Style Creator settings.
