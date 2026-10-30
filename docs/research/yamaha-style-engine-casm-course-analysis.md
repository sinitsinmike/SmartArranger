# Yamaha Style Engine — анализ Yamaha Style Creation Course (2013)

**Исходный документ:** *Yamaha Keyboards – Style Creation Course*, Jørgen Sørensen, 2013-08-17, 19 страниц.  
**Тип источника:** независимое руководство по созданию Yamaha Styles и reverse engineering CASM, не официальный документ Yamaha.  
**Главная ценность:** страницы 15–19 содержат подробное описание старого CASM Section Format 1, включая поля Ctab и отдельный блок Cntt.

## 1. Самое важное предупреждение: у CASM есть поколения

Автор прямо сообщает:
- исходный формат CASM был reverse-engineered в 1999 году и опубликован в 2000-м;
- небольшие изменения появились с первым Tyros, были исследованы в 2003-м и опубликованы в 2004-м;
- с выходом Tyros 3 в ноябре 2008 года формат CASM был **радикально изменён**;
- новый формат применялся в последующих моделях, перечисленных автором, и на дату документа ещё не был reverse-engineered.

Источник: стр. 15, раздел 1.13.

**Следствие для Smart Arranger:** таблицы полей на страницах 15–19 нельзя автоматически считать спецификацией CASM у современных SFF2/PSR-SX900/SX920/Genos. Это историческая спецификация старого формата. Сначала нужно определить поколение и dialect конкретного CASM.

## 2. Общая модель Style-файла

Автор описывает Style как MIDI Format 0 с дополнительными секциями. MIDI-часть содержит Style Parts, отмеченные MIDI Text Markers. Перечислены:
- Main A–D;
- Intro A–C;
- Ending A–C;
- Fill In AA–DD;
- Fill In BA (Break).

Каждая Style Part может содержать до 16 source channels. Типовое назначение destination channels 9–16:
- 9 — Rhythm Sub;
- 10 — Rhythm Main;
- 11 — Bass;
- 12 — Chord 1;
- 13 — Chord 2;
- 14 — Pad;
- 15 — Phrase 1;
- 16 — Phrase 2.

Если source channel layout отличается от destination layout или используется больше восьми каналов, CASM должен задавать перенаправление source channels.

Источник: стр. 2–4, разделы 1.2–1.3.

## 3. CASM: какие параметры описывает руководство

В разделе 1.10.2 (стр. 11–12) перечислены:
- Redirection Schema;
- Chord Root / Chord Type;
- NTR;
- NTT;
- High Key;
- Note Low / High Limit;
- RTR;
- Bass On;
- Auto Start;
- Editable;
- Active Keys;
- Active Chords.

Важное различие: это список функциональных параметров CASM в целом, а не гарантия, что каждый параметр присутствует в каждом dialect и в одинаковом бинарном виде.

Источник: стр. 11–12.

## 4. Старый CASM Format 1 — структура CSEG

Согласно таблице на стр. 15–19:

```text
CASM
  CSEG
    Sdec
      comma-separated style-part names
    Ctab
      channel record
    Ctab
      channel record
    ...
    Cntt
      source channel
      extended NTT value
    Cntt
      ...
  CSEG
    ...
```

- `CASM` — начало контейнера.
- `CSEG` — группа Style Parts с одинаковыми настройками.
- `Sdec` — список имён Style Parts, разделённых запятыми.
- После Sdec идут channel records `Ctab`.
- После Ctab records могут идти `Cntt` records для каналов, у которых задано расширенное значение NTT.

Автор подчёркивает: CSEG группирует Style Parts с одинаковыми настройками.

Источник: стр. 15, 18–19.

## 5. Старый Ctab — документированная последовательность полей

В таблице Format 1 на стр. 15–18 приведена следующая последовательность полей одного channel record:

| Поле из руководства | Назначение |
|---|---|
| Source Channel | MIDI source channel, значения 00–0F |
| Voice Name | 8 байт |
| Destination Channel | destination channel 08–0F |
| Editable | 00/01 |
| Note Mute | маски разрешённых нот |
| Chord Mute | маски разрешённых типов аккордов |
| Source Chord Root | исходный корень паттерна |
| Source Chord Type | исходный тип аккорда |
| NTR | Root Trans или Root Fixed |
| NTT | базовый тип таблицы преобразования |
| High Key | верхний предел корня |
| Note Low Limit | нижняя граница диапазона нот |
| Note High Limit | верхняя граница диапазона нот |
| RTR | правило для удерживаемых нот |
| End Marker | завершающий байт записи |

Это сильная подсказка для анализа Ctab в старых файлах. Но offsets и byte counts нужно сверить по конкретным hex dumps: таблица в PDF использует нумерацию строк логического описания, а не абсолютные offsets от начала payload.

### NTR

Указаны:
- `00` — Root Trans;
- `01` — Root Fixed.

Примеры на стр. 17:
- Root Trans: C3–E3–G3 в C превращается в F3–A3–C4 при переходе к F;
- Root Fixed: те же исходные ноты превращаются в C3–F3–A3.

### NTT

В Ctab указаны коды:
- Bypass `00`;
- Melody `01`;
- Chord `02`;
- Bass `03`;
- Melodic Minor `04`;
- Harmonic Minor `05`.

Для некоторых вариантов автор указывает, что требуется дополнительная настройка NTT в Cntt.

### High Key и Note Limits

- High Key задаёт верхний предел корня; работает только при NTR = Root Trans.
- Note Low/High Limit задают диапазон нот; выходящие за диапазон ноты перемещаются на ближайшую октаву внутри диапазона.

### RTR

Коды, указанные в таблице:
- Stop `00`;
- Pitch Shift `01`;
- Pitch Shift to Root `02`;
- Retrigger `03`;
- Retrigger to Root `04`;
- Note Generator `05`.

**Новая для нашей исследовательской базы деталь:** `Note Generator (05)` описан как режим, который доступен только если он запрограммирован в исходном Style. Он генерирует заданную ноту с заданными высотой, длительностью и velocity в соответствии с новым аккордом.

Источник: стр. 17–18.

## 6. Cntt — отдельная двухбайтовая запись расширенного NTT

На стр. 18–19 описана запись Cntt:
- FourCC `Cntt`;
- длина `00 00 00 02`;
- Source Channel — один байт, тот же номер, что в соответствующем Ctab;
- Note Transposition Table — один байт.

Таблица NTT в Cntt:

| Тип | Bass Off | Bass On |
|---|---:|---:|
| Bypass | не определено | 80 |
| Melody + Bass | не определено | 81 |
| Chord | не определено | 82 |
| Melodic Minor | не определено | 83 |
| Melodic Minor 5th Var | 04 | 84 |
| Harmonic Minor | не определено | 85 |
| Harmonic Minor 5th Var | 06 | 86 |
| Natural Minor | 07 | 87 |
| Natural Minor 5th Var | 08 | 88 |
| Dorian | 09 | 89 |
| Dorian 5th Var | 0A | 8A |

Важное наблюдение: в этом старом формате дополнительные NTT/Bass On значения находятся не в обычном поле NTT Ctab, а в отдельной Cntt записи. Cntt содержит source channel и extended NTT value.

Источник: стр. 18–19.

## 7. Важное сопоставление с нашими файлами

В предыдущих исследовательских партиях мы встретили:
- `Ctb2` 47 и 51 байт в нескольких `.STY`;
- `Ctab` 27 и 31 байт в отдельных `.STY` и `.PRS`;
- `Cntt` записи, которые в одной партии имели двухбайтовый payload;
- совпадение количества Ctab и Cntt в трёх `.PRS`.

PDF даёт полезную гипотезу для последней группы: старый Format 1 действительно описывает Ctab channel records и отдельные двухбайтовые Cntt records, причём Cntt указывает source channel и расширенный NTT.

Однако **пока нельзя объявлять, что наши Ctab 27/31 байт — точно этот Format 1**. Нужно подтвердить:
1. совпадает ли порядок полей;
2. совпадает ли длина именно payload, а не размер с FourCC/служебными байтами;
3. совпадают ли значения source channel между Ctab и Cntt;
4. соответствует ли последний байт End Marker;
5. являются ли 31-байтовые записи расширением, другим поколением или следствием того, как был посчитан размер chunk.

Аналогично нельзя переносить эту таблицу на `Ctb2`: документ описывает Ctab старого формата, а не Ctb2.

## 8. Что меняется в нашем плане

### Приоритет 1 — проверить старый Ctab/Cntt на `.PRS`

Взять `86510_bld_alp.prs`, `86511_bld_alp.prs`, `94511_bld_alp.prs` и проверить:
- каждый Ctab по полям Source Channel, Voice Name, Destination Channel;
- значения Source Channel в Cntt;
- коды NTR/NTT/RTR;
- End Marker;
- связь Cntt с Ctab по source channel.

Если это совпадёт, мы сможем подтвердить старый Format 1 на реальных файлах.

### Приоритет 2 — не смешивать поколения

Создать разные decoder dialects:
- Legacy CASM Format 1 / Ctab + Cntt;
- более поздний CASM dialect с Ctb2;
- неизвестный/неподдерживаемый dialect, который сохраняется как raw bytes.

### Приоритет 3 — исследовать Ctb2 независимо

Для Ctb2 продолжать byte-level diff и сопоставление с SFF2 источниками. Не использовать Ctab Format 1 offsets для Ctb2.

## 9. Источник

Jørgen Sørensen, *Yamaha Keyboards – Style Creation Course*, 2013-08-17, 19 pages.  
Особенно важные страницы:
- стр. 2–4 — MIDI Format 0, Style Parts, source/destination channels;
- стр. 11–12 — назначение параметров CASM;
- стр. 15–19 — CASM Section Format 1, Ctab и Cntt.

Авторский сайт: http://www.jososoft.dk/yamaha

**Итог:** PDF даёт нам наиболее подробное описание старого Ctab/Cntt из всех материалов, которые мы пока рассмотрели. Но автор прямо предупреждает, что CASM был радикально изменён с Tyros 3. Поэтому этот документ — ключ к проверке legacy `.PRS`/`Ctab`, но не готовая спецификация CASM современных SFF2 файлов.
