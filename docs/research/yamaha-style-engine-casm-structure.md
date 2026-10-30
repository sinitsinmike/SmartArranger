# Yamaha Style Engine — структура CASM внутри `.STY`

**Исследовательская заметка №4**  
**Тема:** контейнер Yamaha Style, CASM, CSEG, Sdec, Ctab/Ctb2 и Cntt.  
**Статус:** предварительная спецификация по независимым reverse-engineering проектам и опубликованным дампам реальных файлов. Это не официальная спецификация Yamaha.

## 1. Главный вывод

`.STY` нельзя считать просто MIDI-файлом. Во многих распространённых вариантах это Standard MIDI File (обычно SMF Format 0), к которому добавлены Yamaha-специфичные chunks. Один из ключевых chunks — `CASM`: он связывает секции стиля с правилами обработки каналов при смене аккорда.

Типичная наблюдаемая структура:

```text
MThd
MTrk
CASM
  CSEG
    Sdec
    Ctab / Ctb2
    [Cntt]
[OTSc]
[другие chunks]
```

Но это не универсальный шаблон для каждого `.STY`: состав и порядок дополнительных chunks могут различаться. Для SFF2 и Live Audio styles встречаются дополнительные chunks `OTSc`, `FNRc`, `AASM`, `AFil` и другие.

## 2. Уровни структуры

### 2.1 Верхний уровень: MIDI и Yamaha chunks

Исследованные независимыми проектами файлы показывают, что MIDI-часть может начинаться с `MThd` и `MTrk`, после чего идут Yamaha chunks.

Примеры опубликованных дампов в Yamaha-Style-Studio:

| Файл | Наблюдаемая структура |
|---|---|
| Audio Phraser `.aus` | `MThd → MTrk → CASM → AASM → AFil` |
| Live Audio `.sty` | `MThd → MTrk → CASM → OTSc → FNRc → AASM → AFil` |

В этих двух примерах CASM имеет разные варианты таблиц каналов. Это важный сигнал: нельзя написать один парсер, который безусловно ожидает только один тип записи.

Источник: Yamaha-Style-Studio, `sff2Writer.ts` и README. Это независимый проект, а не документация Yamaha.

### 2.2 Chunk header

Для наблюдаемых CASM/CSEG-подблоков применяется заголовок вида:

```text
4 bytes  FourCC tag
4 bytes  payload size (big-endian uint32)
N bytes  payload
```

Например:

```text
43 41 53 4D 00 00 08 4A ...
C  A  S  M  size
```

Размер относится к payload после восьмибайтового заголовка. При чтении нужно проверять, что `offset + 8 + size` не выходит за границы файла или родительского chunk.

Это правило описано в старых независимых заметках о структуре Style Files и используется в современных парсерах. Перед тем как закрепить его как форматное требование для всех поколений, следует проверить на реальных SFF1 и SFF2 fixtures.

## 3. CASM и CSEG

`CASM` — контейнер параметров, связанных с обработкой Style. Внутри располагаются один или несколько `CSEG`.

`CSEG` группирует описание секций и следующие за ним таблицы параметров каналов.

Важное наблюдение из Yamaha-Style-Studio: в одном реальном Live Audio `.sty` встречаются три CSEG, которые описывают разные группы секций:

- CSEG 1: Main A/B/C, Fill In AA/BB/CC, Intro A, Ending A, Fill In BA;
- CSEG 2: Main D, Fill In DD;
- CSEG 3: Intro B/C, Ending B/C.

Следовательно, нельзя предполагать, что один CASM содержит ровно один CSEG или что все секции стиля всегда находятся в одной группе.

## 4. Sdec — список секций

`Sdec` содержит текстовое описание секций, к которым относятся следующие channel tables. В исследованных примерах это ASCII-строка с именами, разделёнными запятыми, например:

```text
Main A,Main B,Main C,Fill In AA,Fill In BB
```

Смысловая связь:

```text
CSEG
  Sdec = список секций
  Ctab/Ctb2/Cntt = последующие описания каналов этой группы
```

Парсер должен сохранять исходную строку и разбирать её без потери неизвестных имён секций. Нельзя жёстко ограничиваться только Main A–D, Intro A–D, Ending A–D и Fill AA–DD: новые/нестандартные имена должны переживать импорт.

Старое описание структуры указывает, что Sdec предшествует последовательности Ctab. Современные дампы показывают также Ctb2 и Cntt.

## 5. Ctab и Ctb2 — две наблюдаемые формы

Нельзя считать, что существует только один формат channel table.

### Ctab

Старые и современные независимые реализации наблюдали `Ctab` длиной 27 байт в loadable Live Audio `.sty` из Yamaha-Style-Studio. Такие таблицы идут после Sdec и задают параметры канала/партии.

### Ctb2

В Audio Phraser export наблюдался `Ctb2` длиной 47 байт. В конкретном опубликованном примере был один Ctb2 для Rhythm channel.

Также проект Cadenza сообщает о реальных `.STY`, в которых встречаются многочисленные Ctb2 записи (например, 92 и 108 записей в двух исследованных файлах). Это подтверждает, что Ctb2 не является единичным исключением.

**Вывод:** Ctab и Ctb2 нельзя считать взаимозаменяемыми или предполагать, что один из них является просто более длинной версией другого. Нужны отдельные декодеры и реальные golden fixtures.

## 6. Cntt — дополнительный блок параметров NTT

Yamaha-Style-Studio описывает структуру реального Live Audio style так:

```text
CSEG
  Sdec
  N × Ctab (27 bytes)
  N × Cntt (2 bytes)
```

В README/implementation notes проекта Cadenza Cntt также рассматривается отдельно от Ctab/Ctb2.

Особенно важное предупреждение из checklist Yamaha-Style-Studio: в проверенном файле байт 21 Ctab не был флагом NTT Bass; он содержал обычное значение NTT (`0x03`), а bass flag находился в Cntt (`0x81`).

Это пример того, почему нельзя назначать значения полям по догадке или по одному старому описанию. Перед декодированием необходимо сверять байты с несколькими файлами и известными параметрами Style Creator.

## 7. Что можно утверждать о CASM с разной степенью уверенности

| Утверждение | Основание | Статус |
|---|---|---|
| CASM — отдельный chunk в ряде `.STY` после MIDI-части | реальные файлы, независимые парсеры | хорошо подтверждено для изученных вариантов |
| CASM содержит CSEG | старые описания и современные fixtures | хорошо подтверждено |
| CSEG связывает Sdec с channel tables | опубликованные дампы и реализации | хорошо подтверждено |
| Sdec содержит перечень секций | старые описания и реальные строки | хорошо подтверждено |
| Встречаются Ctab длиной 27 байт | реальные SFF2 Live Audio fixture | подтверждено для конкретного файла |
| Встречается Ctb2 длиной 47 байт | Audio Phraser fixture и другие проекты | подтверждено для конкретных файлов |
| Cntt хранит дополнительные NTT-настройки | современные независимые реализации | предварительно подтверждено; нужна собственная сверка |
| Любой SFF2 использует только Ctab | противоречит опубликованным реальным файлам | неверно |
| Один CSEG на весь стиль | противоречит опубликованному Live Audio fixture | неверно |

## 8. Известные ограничения независимых реализаций

### bures/sff2-tools

Проект умеет преобразовывать SFF1/SFF2 в YAML и обратно, но его README прямо указывает, что `ymlplay` игнорирует CASM, включая channel mapping и note/chord transposition. Автор отмечает нехватку информации о точной реализации преобразования нот при разных CASM settings.

Это полезный инструмент для изучения структуры/сохранения данных, но не эталон Yamaha playback engine.

### Cadenza

Проект описывает безопасное обнаружение CASM и чтение структуры `CASM → CSEG → Sdec + Ctab`, а также Ctb2 в реальных стилях. В его заметках перечислены незакрытые вопросы: split-range Ctb2, полное phrase fitting, SFF2 low/main/high range playback, retrigger при смене аккорда и др.

Это полезная независимая работа, но не официальная спецификация.

### Yamaha-Style-Studio

Современный проект содержит writer/parser и byte-level fixtures. Его авторы сообщают о двух реальных файлах с различающимися CASM dialects: Ctb2 для Audio Phraser и Ctab + Cntt для loadable Live Audio style.

Это наиболее полезный из найденных материалов для текущего этапа, однако его выводы относятся к конкретным fixture-файлам, а не ко всем стилям Yamaha.

## 9. Рекомендации для парсера Smart Arranger

### 9.1 Сначала lossless parser

На первом этапе не реализовывать playback. Сделать парсер, который:
- находит MIDI body и Yamaha chunks;
- читает chunk boundaries с проверкой размера;
- сохраняет неизвестные chunks byte-for-byte;
- разбирает CASM и CSEG;
- извлекает Sdec как исходную строку;
- распознаёт Ctab, Ctb2 и Cntt как разные типы;
- сохраняет неизвестные/неразобранные записи в raw bytes;
- выдаёт diagnostics, а не аварийно прекращает импорт при неизвестном chunk.

### 9.2 Не делать предположений о количестве

Не предполагать:
- ровно один CSEG;
- ровно один Ctab на секцию;
- фиксированное число каналов;
- фиксированный порядок всех top-level chunks;
- что все стили содержат OTSc или audio chunks;
- что SFF2 всегда означает один и тот же вариант CASM.

### 9.3 Структура внутренней модели

```cpp
struct RawChunk {
    FourCC id;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> originalBytes;
};

struct CasmGroup {
    std::string rawSectionList;  // original Sdec bytes decoded conservatively
    std::vector<RawChunk> channelRecords;
};

struct CasmData {
    std::vector<CasmGroup> groups;
    std::vector<RawChunk> unknownRecords;
};
```

Это только пример модели хранения, а не утверждение о точной структуре бинарного формата.

### 9.4 Отдельные этапы

1. Container/chunk parser.
2. CASM structural parser.
3. Ctab decoder.
4. Ctb2 decoder.
5. Cntt decoder.
6. Semantic mapping to NTR/NTT/RTR/limits.
7. Playback conversion engine.

Не смешивать эти слои в одном классе. Сначала добиться точного round-trip сохранения неизвестных байтов, затем семантического декодирования.

## 10. Следующий эксперимент: реальные `.STY` пользователя

Для перехода от гипотез к проверенной таблице байтов нужны сами файлы стилей.

Минимальный набор:
1. Один заводской SFF1 `.STY`.
2. Один заводской SFF2 `.STY`.
3. Один пользовательский Style Creator `.STY`, если доступен.
4. По возможности стиль с Guitar part и стиль с NTT Bass / slash chord.
5. Один стиль, у которого параметры NTR/NTT/RTR можно посмотреть на Yamaha-инструменте.

Для каждого файла нужно получить:
- полный размер и SHA-256;
- список top-level chunks и offsets;
- CASM raw dump;
- список CSEG;
- Sdec каждой группы;
- hex dump каждой Ctab/Ctb2/Cntt записи;
- модель клавиатуры/источник файла, если известны;
- значения параметров, видимые в Style Creator, если доступны.

Только после сопоставления raw bytes с известными параметрами можно достоверно закреплять offsets и bit fields.

## 11. Источники

1. Yamaha-Style-Studio, README и описание реальных CASM fixtures:  
   https://github.com/jozphe/Yamaha-Style-Studio
2. Yamaha-Style-Studio, SFF2 writer/parser source:  
   https://github.com/jozphe/Yamaha-Style-Studio/blob/main/src/lib/binary/sff2Writer.ts
3. Cadenza, CASM parsing notes and inspected real styles:  
   https://github.com/Piskocis/Cadenza/blob/master/docs/CASM_NOTES.md
4. bures/sff2-tools, README; SFF1/SFF2 to YAML tools and explicit CASM playback limitations:  
   https://github.com/bures/sff2-tools
5. Heiko Plate, *About the Structure of Style-Files* — older independent structural description:  
   https://www.heikoplate.de/structure-of-style-files.html
6. Yamaha, PSR-SX920/SX720 Reference Manual, for the meaning of SFF parameters (not binary offsets):  
   https://usa.yamaha.com/files/download/other_assets/5/2328055/PSR-SX920_reference_manual_En_B0.pdf

## 12. Решение для Smart Arranger

**Не начинать сейчас реализацию NTR/NTT/RTR из бинарных offsets, найденных в одном проекте.**

Следующий корректный шаг — собрать реальные `.STY` fixtures и выполнить byte-level сопоставление. До этого можно реализовать только безопасное обнаружение/сохранение chunks и структуру CASM без семантической интерпретации неизвестных байтов.

**Ключевой принцип:** Smart Arranger должен сначала точно прочитать и сохранить исходные данные Style, а затем воспроизводить их правила. Если парсер не уверен в значении поля, он обязан сохранить его как неизвестное, а не подменять догадкой.
