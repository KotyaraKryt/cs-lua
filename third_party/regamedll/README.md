# cssdk

Заголовки CS-классов (`CBasePlayer` и компания) и ReGameDLL API. Заменяют собой
HLSDK из metamod-r: две копии SDK в одной единице трансляции конфликтуют, поэтому
сборка целиком идёт через этот набор.

## Откуда взято

Скопировано из **ReAPI**, `reapi/include/cssdk`:

```
git clone --depth 1 --filter=blob:none --sparse https://github.com/s1lentq/reapi.git
cd reapi && git sparse-checkout set reapi/include
cp -r reapi/include/cssdk <сюда>
```

## Почему именно эта копия, а не из релиза ReGameDLL

В бинарном релизе ReGameDLL (`regamedll-bin-*.zip`) тоже есть папка `cssdk`, но
она **не компилируется**: сгенерирована автоматически, и тела inline-функций в
ней разорваны — например, в `dlls/cbase.h` получается

```cpp
void EXPORT SUB_CallUseToggle();
{
	Use(this, this, USE_TOGGLE, 0);
```

Исходники самого ReGameDLL тоже не подходят напрямую: они рассчитаны на
внутреннюю сборку и требуют `precompiled.h` с `g_vecZero`, `VectorRef`,
`EXT_FUNC` и прочим внутренним обвесом.

Набор из ReAPI — единственный из трёх, предназначенный для компиляции **снаружи**
игрового модуля, что нам и нужно: `#include <regamedll_api.h>` работает сразу
после `<extdll.h>`, без плясок с порядком заголовков.

## Обновление

Просто скопировать свежий `reapi/include/cssdk` поверх. Локальных правок нет —
следить не за чем.

Версия API в заголовках должна быть не новее, чем у `mp.dll` на сервере: плагин
при старте сверяет major (строго) и minor (не ниже) и пишет результат в консоль.
