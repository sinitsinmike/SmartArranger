#include "StyleFile.h"

#include <array>
#include <limits>
#include <utility>

namespace
{
class Reader
{
public:
    Reader (const uint8_t* bytesIn, size_t sizeIn) : bytes (bytesIn), size (sizeIn), limit (sizeIn) {}

    bool has (size_t count) const noexcept { return position <= limit && count <= limit - position; }
    size_t tell() const noexcept { return position; }
    void seek (size_t offset) noexcept { position = offset; }
    void setLimit (size_t newLimit) noexcept { limit = juce::jmin (newLimit, size); }
    void restoreLimit() noexcept { limit = size; }

    bool readByte (uint8_t& value) noexcept
    {
        if (! has (1)) return false;
        value = bytes[position++];
        return true;
    }

    bool readBE16 (uint16_t& value) noexcept
    {
        if (! has (2)) return false;
        value = static_cast<uint16_t> ((static_cast<uint16_t> (bytes[position]) << 8)
                                      | bytes[position + 1]);
        position += 2;
        return true;
    }

    bool readBE32 (uint32_t& value) noexcept
    {
        if (! has (4)) return false;
        value = (static_cast<uint32_t> (bytes[position]) << 24)
              | (static_cast<uint32_t> (bytes[position + 1]) << 16)
              | (static_cast<uint32_t> (bytes[position + 2]) << 8)
              | static_cast<uint32_t> (bytes[position + 3]);
        position += 4;
        return true;
    }

    bool readVariableLength (uint32_t& value) noexcept
    {
        value = 0;
        for (int i = 0; i < 4; ++i)
        {
            uint8_t byte = 0;
            if (! readByte (byte)) return false;
            value = (value << 7) | (byte & 0x7fu);
            if ((byte & 0x80u) == 0)
                return true;
        }
        return false;
    }

    bool readTag (char (&tag)[5]) noexcept
    {
        if (! has (4)) return false;
        for (int i = 0; i < 4; ++i)
            tag[i] = static_cast<char> (bytes[position + static_cast<size_t> (i)]);
        tag[4] = '\0';
        position += 4;
        return true;
    }

    const uint8_t* at (size_t offset) const noexcept
    {
        return offset <= size ? bytes + offset : nullptr;
    }

    bool skip (size_t count) noexcept
    {
        if (! has (count)) return false;
        position += count;
        return true;
    }

private:
    const uint8_t* bytes;
    size_t size;
    size_t position = 0;
    size_t limit;
};

bool isSectionMarker (const juce::String& marker)
{
    static const std::array<const char*, 16> names = {
        "SInt", "Main A", "Main B", "Main C", "Main D", "Fill In AA", "Fill In BB",
        "Fill In CC", "Fill In DD", "Intro A", "Intro B", "Intro C", "Ending A",
        "Ending B", "Ending C", "Fill In BA"
    };
    for (const auto* name : names)
        if (marker == name)
            return true;
    return false;
}

bool parseTrack (Reader& reader, size_t trackEnd, SmartArrangerStyleInfo& info,
                 juce::String& error)
{
    reader.setLimit (trackEnd);
    uint8_t runningStatus = 0;
    uint64_t absoluteTicks = 0;
    int openSection = -1;

    while (reader.tell() < trackEnd)
    {
        uint32_t delta = 0;
        if (! reader.readVariableLength (delta))
        {
            error = "Повреждён MIDI delta-time в MTrk.";
            return false;
        }
        if (absoluteTicks > std::numeric_limits<uint64_t>::max() - delta)
        {
            error = "Слишком большая длительность MIDI-событий.";
            return false;
        }
        absoluteTicks += delta;

        uint8_t first = 0;
        if (! reader.readByte (first))
        {
            error = "Неожиданный конец MIDI-трека.";
            return false;
        }

        uint8_t status = first;
        bool firstIsData = (first & 0x80u) == 0;
        if (firstIsData)
        {
            if (runningStatus < 0x80u || runningStatus >= 0xf0u)
            {
                error = "В MIDI-треке встретилось running status без предыдущей команды.";
                return false;
            }
            status = runningStatus;
        }
        else if (status < 0xf0u)
        {
            runningStatus = status;
        }
        else
        {
            runningStatus = 0;
        }

        if (status == 0xffu)
        {
            uint8_t metaType = 0;
            uint32_t length = 0;
            if (firstIsData || ! reader.readByte (metaType) || ! reader.readVariableLength (length)
                || ! reader.has (length))
            {
                error = "Повреждено MIDI meta-событие.";
                return false;
            }

            const auto* payload = reader.at (reader.tell());
            if (metaType == 0x06u)
            {
                juce::String marker = juce::String::fromUTF8 (reinterpret_cast<const char*> (payload),
                                                              static_cast<int> (length)).trim();
                if (marker == "SFF1" || marker == "SFF2")
                    info.format = marker;

                if (isSectionMarker (marker))
                {
                    if (openSection >= 0)
                        info.sections[static_cast<size_t> (openSection)].lengthTicks =
                            absoluteTicks - info.sections[static_cast<size_t> (openSection)].startTick;
                    info.sections.push_back ({ marker, absoluteTicks });
                    openSection = static_cast<int> (info.sections.size() - 1);
                }
            }
            else if (metaType == 0x51u && length == 3)
            {
                const uint32_t microsPerQuarter = (static_cast<uint32_t> (payload[0]) << 16)
                                                | (static_cast<uint32_t> (payload[1]) << 8)
                                                | payload[2];
                if (microsPerQuarter != 0)
                    info.tempo = static_cast<int> ((60000000u + microsPerQuarter / 2u)
                                                   / microsPerQuarter);
            }
            else if (metaType == 0x58u && length >= 2)
            {
                info.numerator = payload[0];
                const auto exponent = juce::jmin (payload[1], static_cast<uint8_t> (7));
                info.denominator = 1 << exponent;
            }
            else if (metaType == 0x2fu)
            {
                if (! reader.skip (length))
                    return false;
                break;
            }

            if (! reader.skip (length))
            {
                error = "Meta-событие выходит за границы MIDI-трека.";
                return false;
            }
            continue;
        }

        if (status == 0xf0u || status == 0xf7u)
        {
            uint32_t length = 0;
            if (firstIsData || ! reader.readVariableLength (length) || ! reader.skip (length))
            {
                error = "Повреждено MIDI SysEx-событие.";
                return false;
            }
            continue;
        }

        if (status < 0x80u || status >= 0xf0u)
        {
            error = "Неподдерживаемая MIDI-команда в стиле.";
            return false;
        }

        const auto command = static_cast<uint8_t> (status & 0xf0u);
        const int dataBytes = (command == 0xc0u || command == 0xd0u) ? 1 : 2;
        uint8_t data1 = firstIsData ? first : 0;
        uint8_t data2 = 0;
        if ((! firstIsData && ! reader.readByte (data1))
            || (dataBytes == 2 && ! reader.readByte (data2)))
        {
            error = "Неожиданный конец MIDI-команды.";
            return false;
        }

        if ((command == 0x80u || command == 0x90u) && data1 < 128)
            info.usedChannels |= static_cast<uint16_t> (1u << (status & 0x0fu));

        if (info.events.size() >= 1000000)
        {
            error = "В стиле больше миллиона MIDI-событий; загрузка остановлена.";
            return false;
        }
        info.events.push_back ({ absoluteTicks, openSection, status, data1, data2,
                                 static_cast<uint8_t> (dataBytes) });
    }

    if (openSection >= 0)
    {
        auto& section = info.sections[static_cast<size_t> (openSection)];
        section.lengthTicks = absoluteTicks - section.startTick;
    }
    info.totalTicks = juce::jmax (info.totalTicks, absoluteTicks);
    reader.restoreLimit();
    return true;
}

bool readCasmRecord (Reader& reader, SmartArrangerCasmRecord& record,
                     juce::String& error, const juce::String& context)
{
    char tag[5] {};
    uint32_t payloadLength = 0;
    if (! reader.readTag (tag) || ! reader.readBE32 (payloadLength)
        || ! reader.has (payloadLength))
    {
        error = "Повреждена длина записи CASM в " + context + ".";
        return false;
    }

    record.tag = juce::String::fromUTF8 (tag);
    const auto* payload = reader.at (reader.tell());
    record.payload.assign (payload, payload + payloadLength);
    return reader.skip (payloadLength);
}

void decodeCasmRecordFields (SmartArrangerCasmRecord& record)
{
    const auto& bytes = record.payload;
    if (record.tag == "Sdec")
    {
        if (! bytes.empty())
        {
            record.sectionDeclaration = juce::String::fromUTF8 (
                reinterpret_cast<const char*> (bytes.data()), static_cast<int> (bytes.size()));
            juce::StringArray names;
            names.addTokens (record.sectionDeclaration, ",", "");
            for (auto name : names)
            {
                name = name.trim();
                if (name.isNotEmpty())
                    record.sectionNames.push_back (name);
            }
        }
        return;
    }

    if (record.tag == "Cntt")
    {
        if (bytes.size() == 2)
        {
            record.hasCnttPair = true;
            record.cnttSourceChannel = bytes[0];
            record.cnttValue = bytes[1];
        }
        return;
    }

    if ((record.tag != "Ctab" && record.tag != "Ctb2") || bytes.size() < 20)
        return;

    record.hasCommonChannelFields = true;
    record.sourceChannel = bytes[0];
    record.partLabel = juce::String::fromUTF8 (
        reinterpret_cast<const char*> (bytes.data() + 1), 8).trim();
    record.destinationChannel = bytes[9];
    record.editable = bytes[10];
    record.noteMask = { bytes[11], bytes[12] };
    for (size_t i = 0; i < record.chordMask.size(); ++i)
        record.chordMask[i] = bytes[13 + i];
    record.sourceRoot = bytes[18];
    record.sourceChord = bytes[19];

    if (record.tag == "Ctab")
    {
        // The legacy 27- and 31-byte records are observed in the corpus.
        // Keep the extra four bytes of the latter opaque in the raw payload.
        if (bytes.size() == 27 || bytes.size() == 31)
        {
            record.hasSff1TranspositionFields = true;
            record.sff1Ntr = bytes[20];
            record.sff1Ntt = bytes[21];
            record.sff1HighKey = bytes[22];
            record.sff1NoteLowLimit = bytes[23];
            record.sff1NoteHighLimit = bytes[24];
            record.sff1Rtr = bytes[25];
        }
        return;
    }

    // Do not apply the 47-byte map to other Ctb2 lengths. Those payloads stay
    // available verbatim for a later profile-specific decoder.
    if (bytes.size() != 47)
        return;

    record.hasSff2Zones = true;
    record.ctb2MiddleLowNote = bytes[20];
    record.ctb2MiddleHighNote = bytes[21];
    for (size_t zoneIndex = 0; zoneIndex < record.zones.size(); ++zoneIndex)
    {
        const size_t offset = 22 + zoneIndex * 6;
        auto& zone = record.zones[zoneIndex];
        zone.ntr = bytes[offset];
        zone.nttAndBassOn = bytes[offset + 1];
        zone.highKey = bytes[offset + 2];
        zone.noteLowLimit = bytes[offset + 3];
        zone.noteHighLimit = bytes[offset + 4];
        zone.rtr = bytes[offset + 5];
    }
}

bool parseCasmChunk (const uint8_t* bytes, size_t length,
                     SmartArrangerStyleInfo& info, juce::String& error)
{
    Reader reader (bytes, length);
    while (reader.tell() < length)
    {
        SmartArrangerCasmRecord record;
        if (! readCasmRecord (reader, record, error, "CASM"))
            return false;

        if (record.tag == "CSEG")
        {
            SmartArrangerCasmSegment segment;
            Reader segmentReader (record.payload.data(), record.payload.size());
            while (segmentReader.tell() < record.payload.size())
            {
                SmartArrangerCasmRecord segmentRecord;
                if (! readCasmRecord (segmentReader, segmentRecord, error, "CSEG"))
                    return false;
                decodeCasmRecordFields (segmentRecord);
                if (segmentRecord.tag == "Sdec")
                {
                    segment.sectionDeclaration = segmentRecord.sectionDeclaration;
                    segment.sectionNames = segmentRecord.sectionNames;
                }
                segment.records.push_back (std::move (segmentRecord));
            }
            info.casmSegments.push_back (std::move (segment));
        }
        else
        {
            // CSEG's nested records are already stored above. Retain only
            // non-CSEG top-level records here to avoid copying the full
            // CSEG payload a second time.
            info.casmRecords.push_back (std::move (record));
        }
    }

    return true;
}
}

bool SmartArrangerStyleFile::inspect (const juce::File& file,
                                      SmartArrangerStyleInfo& result,
                                      juce::String& error)
{
    result = {};
    error.clear();
    result.name = file.getFileNameWithoutExtension();

    juce::MemoryBlock data;
    constexpr int64_t maximumStyleBytes = 64 * 1024 * 1024;
    if (file.getSize() > maximumStyleBytes || ! file.loadFileAsData (data) || data.getSize() < 14)
    {
        error = "Не удалось прочитать файл стиля или он слишком короткий.";
        return false;
    }

    Reader reader (static_cast<const uint8_t*> (data.getData()), data.getSize());
    char tag[5] {};
    uint32_t headerLength = 0;
    uint16_t format = 0, trackCount = 0, division = 0;
    if (! reader.readTag (tag) || juce::String (tag) != "MThd"
        || ! reader.readBE32 (headerLength) || headerLength < 6
        || ! reader.readBE16 (format) || ! reader.readBE16 (trackCount) || ! reader.readBE16 (division)
        || ! reader.skip (headerLength - 6))
    {
        error = "Файл не начинается с ожидаемого MIDI-заголовка Yamaha Style (MThd).";
        return false;
    }

    if (format != 0 || trackCount == 0 || (division & 0x8000u) != 0 || division == 0)
    {
        error = "Пока поддерживается стиль с MIDI format 0 и PPQ-временной сеткой.";
        return false;
    }
    result.ticksPerQuarter = division;

    bool foundTrack = false;
    while (reader.tell() < data.getSize())
    {
        const size_t chunkStart = reader.tell();
        if (! reader.readTag (tag))
            break;

        if (juce::String (tag) == "MTrk")
        {
            uint32_t trackLength = 0;
            if (! reader.readBE32 (trackLength) || ! reader.has (trackLength))
            {
                error = "MIDI-трек MTrk обрезан или имеет неверную длину.";
                return false;
            }
            const size_t trackEnd = reader.tell() + trackLength;
            if (! parseTrack (reader, trackEnd, result, error))
                return false;
            reader.seek (trackEnd);
            foundTrack = true;
            continue;
        }

        const bool isCasm = juce::String (tag) == "CASM";

        // Yamaha's non-MIDI sections are length-prefixed chunks. Preserve
        // bounded raw CASM records; other chunk semantics and note
        // transposition remain later implementation stages.
        uint32_t chunkLength = 0;
        if (! reader.readBE32 (chunkLength) || ! reader.has (chunkLength))
        {
            error = "Неизвестная секция файла не имеет безопасной длины.";
            return false;
        }
        if (isCasm)
        {
            result.hasCasm = true;
            const auto* chunkData = reader.at (reader.tell());
            if (! parseCasmChunk (chunkData, chunkLength, result, error))
                return false;
        }

        reader.skip (chunkLength);

        if (reader.tell() <= chunkStart)
        {
            error = "Не удалось продвинуться по секциям файла стиля.";
            return false;
        }
    }

    if (! foundTrack)
    {
        error = "В файле не найден MIDI-трек MTrk.";
        return false;
    }

    if (result.sections.empty())
        result.sections.push_back ({ "Track", 0, result.totalTicks });

    if (result.format.isEmpty())
        result.format = result.hasCasm ? "SFF style (CASM present)" : "SFF style";

    return true;
}
