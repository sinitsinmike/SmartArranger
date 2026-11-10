#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>

struct SmartArrangerStyleSection
{
    juce::String name;
    uint64_t startTick = 0;
    uint64_t lengthTicks = 0;
};

struct SmartArrangerStyleEvent
{
    uint64_t tick = 0;
    int sectionIndex = -1;
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;
    uint8_t dataBytes = 0;
};

struct SmartArrangerCasmZone
{
    uint8_t ntr = 0;
    uint8_t nttAndBassOn = 0;
    uint8_t highKey = 0;
    uint8_t noteLowLimit = 0;
    uint8_t noteHighLimit = 127;
    uint8_t rtr = 0;
};

// A length-checked CASM record keeps its original bytes and exposes only
// fields whose offsets are documented or corroborated for that record form.
struct SmartArrangerCasmRecord
{
    juce::String tag;
    std::vector<uint8_t> payload;

    bool hasCommonChannelFields = false;
    uint8_t sourceChannel = 0;
    juce::String partLabel;
    uint8_t destinationChannel = 0;
    uint8_t editable = 0;
    std::array<uint8_t, 2> noteMask {};
    std::array<uint8_t, 5> chordMask {};
    uint8_t sourceRoot = 0;
    uint8_t sourceChord = 0;

    // SFF1 Ctab fields. Any bytes beyond RTR remain only in payload.
    bool hasSff1TranspositionFields = false;
    uint8_t sff1Ntr = 0;
    uint8_t sff1Ntt = 0;
    uint8_t sff1HighKey = 0;
    uint8_t sff1NoteLowLimit = 0;
    uint8_t sff1NoteHighLimit = 127;
    uint8_t sff1Rtr = 0;

    // SFF2 Ctb2 pitch zones are decoded only for the documented 47-byte form.
    bool hasSff2Zones = false;
    uint8_t ctb2MiddleLowNote = 0;
    uint8_t ctb2MiddleHighNote = 127;
    std::array<SmartArrangerCasmZone, 3> zones {};

    // Sdec is preserved as raw payload and also exposed as trimmed names.
    juce::String sectionDeclaration;
    std::vector<juce::String> sectionNames;

    // Cntt is intentionally kept opaque beyond its two observed bytes.
    bool hasCnttPair = false;
    uint8_t cnttSourceChannel = 0;
    uint8_t cnttValue = 0;
};

struct SmartArrangerCasmSegment
{
    juce::String sectionDeclaration;
    std::vector<juce::String> sectionNames;
    std::vector<SmartArrangerCasmRecord> records;
};

struct SmartArrangerStyleInfo
{
    juce::String name;
    juce::String format;
    int tempo = 120;
    int numerator = 4;
    int denominator = 4;
    int ticksPerQuarter = 0;
    uint64_t totalTicks = 0;
    uint16_t usedChannels = 0;
    bool hasCasm = false;
    // Unknown top-level CASM records; CSEG payloads are represented once in
    // casmSegments, as their parsed nested records.
    std::vector<SmartArrangerCasmRecord> casmRecords;
    std::vector<SmartArrangerCasmSegment> casmSegments;
    std::vector<SmartArrangerStyleSection> sections;
    std::vector<SmartArrangerStyleEvent> events;
};

class SmartArrangerStyleFile
{
public:
    static bool inspect (const juce::File& file,
                         SmartArrangerStyleInfo& result,
                         juce::String& error);
};
