# amxmodx-sdk

Публичный Module SDK AmxModX — то, из чего собирается сторонний AMXX-модуль
(`AMXX_Query`/`AMXX_Attach`/`AMXX_Detach`, `MF_*` таблица функций,
`MF_RegisterSPForwardByName`/`MF_ExecuteForward` и т.д.). Нужен только
[amxx_bridge.cpp](../../src/amxx_bridge.cpp) — экспериментальному мосту
Lua → public-функции AMXX-плагинов на ветке `experiment/amxx-native-bridge`.

## Откуда взято

```
curl -sL -o amxxmodule.h   https://raw.githubusercontent.com/alliedmodders/amxmodx/<sha>/public/sdk/amxxmodule.h
curl -sL -o IGameConfigs.h https://raw.githubusercontent.com/alliedmodders/amxmodx/<sha>/public/IGameConfigs.h
curl -sL -o ITextParsers.h https://raw.githubusercontent.com/alliedmodders/amxmodx/<sha>/public/ITextParsers.h
```

sha на момент копирования: `eaab23f0068b34e295987486ed5ca89408326271`
(HEAD ветки `master`, взят через `git ls-remote`).

Целевая версия сервера — AmxModX 1.9.0.5262 (номер билда, не git-тег: это
build counter их CI, тегов под конкретные билды нет). Module ABI
(`amxxmodule.h`/`.cpp`) не менялся между 1.8.x/1.9.x/1.10.x, так что SDK из
текущего `master` совместим.

## Почему именно эти три файла

`amxxmodule.h` тянет `<IGameConfigs.h>`, та — `<ITextParsers.h>`, та — только
`<string.h>`. На этом цепочка обрывается: `amtl` (у amxmodx это отдельный
git-submodule `public/amtl`) в неё не входит, весь остальной `public/` —
тоже.

`amxxmodule.cpp` (готовая обвязка `AMXX_Query`/`Attach`/`Detach` из того же
`public/sdk/`) намеренно **не** взята. Её `AMXX_Attach` безусловно
запрашивает у ядра несколько десятков функций через `REQFUNC`, и на сервере
с AMXX 1.9.0.5249 это кончалось зависанием вместо внятной ошибки.
[amxx_bridge.cpp](../../src/amxx_bridge.cpp) реализует эти три экспорта сам и
просит ровно две функции, которые ему нужны.

`moduleconfig.h` — не отсюда: `public/sdk/moduleconfig.in.h` в апстриме это
шаблон под ручное редактирование каждым модулем (не генерируется сборкой),
поэтому у нас это [src/moduleconfig.h](../../src/moduleconfig.h) — свой
файл, а не копия чужого.

## Обновление

Перекачать все четыре по свежему sha, обновить sha в этом README. Локальных
правок в вендоренных файлах нет.

## Лицензия

GPLv3 (AmxModX Development Team) — см. заголовок каждого файла. Актуально
только для этой экспериментальной ветки; в `main` не попадает.
