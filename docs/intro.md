---
description: "Что такое cs-lua, как устроен модуль и с чего начать."
---

# Введение

**cs-lua** — Metamod-модуль для Counter-Strike 1.6, который позволяет писать серверные плагины на Lua вместо SourcePawn/Pawn.

Под капотом используется **LuaJIT 2.1**. Состояние игроков, события геймплея и доступ к движку предоставляют **ReHLDS** и **ReGameDLL**. Для работы нужны [metamod-r](https://github.com/rehlds/Metamod-R) и [ReGameDLL_CS](https://github.com/s1lentq/ReGameDLL_CS) — подробности в разделе [Установка](/install).

<Callout icon="zap" color="yellow">
  Цикл разработки: изменил код → `lua_reload` → сразу проверил результат.
  Перекомпилировать плагин и перезапускать карту не требуется.
</Callout>

Каждый плагин работает в собственном изолированном окружении со своим `require`. При этом он имеет полный доступ к движку — как модуль AMXX.

## Дальше

<CardGroup cols={2}>
  <Card title="Установка" icon="download" href="/install">
    Как установить модуль на сервер и убедиться, что он работает.
  </Card>
  <Card title="Первый плагин" icon="sparkles" href="/guides/first-plugin">
    Создайте свой первый Lua-плагин и добавьте команду.
  </Card>
  <Card title="Структура плагина" icon="folder-tree" href="/guides/plugin-structure">
    Как устроен плагин, `require`, окружение и порядок загрузки.
  </Card>
  <Card title="Справочник API" icon="book" href="/api">
    Все пространства имён: игроки, сущности, события, таймеры, команды и т.д.
  </Card>
</CardGroup>

<Card title="Готовые плагины" icon="puzzle-piece" href="https://github.com/KotyaraKryt/cs-lua-plugins">
  Примеры и готовые решения, которые можно установить сразу.
</Card>

## Сообщество

Если что-то непонятно — создай [Issue](https://github.com/KotyaraKryt/cs-lua/issues) в репозитории.  
Туда же лучше писать баги и предложения по новым возможностям.