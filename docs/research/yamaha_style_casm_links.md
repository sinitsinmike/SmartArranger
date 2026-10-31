# Yamaha Style / CASM — ссылки и материалы

Подборка официальной документации Yamaha и сторонних технических ресурсов по MIDI Song to Style, форматам SFF1/SFF2, CASM и реализации Style playback.

## 1. Официальная документация Yamaha

### MIDI Song to Style — Owner's Manual
Описание программы MIDI Song to Style и процесса преобразования MIDI-файла в Style.

https://data.yamaha.com/files/download/other_assets/4/2179884/MIDI_Song_to_Style_owners_manual_En_B0.pdf

### MIDI Song to Style v1.2.0 — Supplementary Manual
Дополнение к руководству MIDI Song to Style версии 1.2.0.

https://data.yamaha.com/files/download/other_assets/3/2928633/MIDI_song_to_style_v120_SM_En_A0.pdf

### PSR-SX600 — Reference Manual
Справочное руководство PSR-SX600. Содержит описание параметров Style и правил транспонирования.

https://usa.yamaha.com/files/download/other_assets/1/1346911/PSR-SX600_reference_manual_En_C0_web.pdf

### Genos2 — Reference Manual
Справочное руководство Genos2. Содержит описание работы со Style и параметров SFF.

https://usa.yamaha.com/files/download/other_assets/1/2318561/Genos2_reference_manual_En_C0.pdf

## 2. Технические статьи и справочные материалы

### Keyboard and Style — JosoSoft
Технический материал о клавиатурах Yamaha и файлах Style.

https://www.jososoft.dk/yamaha/articles/keyboard_and_style.pdf

## 3. Открытые проекты и исходный код

### SFF2 Tools — bures
Набор командных инструментов для чтения, редактирования и преобразования Yamaha SFF2 Style-файлов.

https://github.com/bures/sff2-tools

### JJazzLab — YamJJazz
Открытое приложение для генерации автоаккомпанемента. В исходном коде есть чтение CASM, обработка Ctab/Ctb2/Cntt и реализация части логики транспонирования партий.

https://github.com/jjazzboss/JJazzLab

---

## Примечания

- Ссылки очищены от параметров отслеживания `utm_source`.
- Руководства Yamaha — официальные источники, но описывают главным образом пользовательские функции и параметры; они не обязательно раскрывают полную бинарную структуру CASM.
- JosoSoft, SFF2 Tools и JJazzLab — сторонние технические источники и реализации. Их трактовки следует проверять по файлам Style и независимым тестам.
