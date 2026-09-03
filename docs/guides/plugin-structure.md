---
description: "Как устроены Lua-плагины, их файлы, окружение и порядок загрузки."
---

## Структура плагина

Все плагины находятся в `addons/lua/plugins/`. Каждый плагин имеет собственную папку:

<Tree>
  <Tree.Folder name="addons/lua" defaultOpen>
    <Tree.Folder name="plugins" defaultOpen>
      <Tree.Folder name="my_plugin" defaultOpen>
        <Tree.File name="manifest.lua" />
        <Tree.File name="init.lua" />
      </Tree.Folder>
    </Tree.Folder>
    <Tree.Folder name="core" />
    <Tree.Folder name="lib" />
  </Tree.Folder>
</Tree>

Каждый плагин должен содержать два обязательных файла:

- `manifest.lua` — описывает плагин и его настройки
- `init.lua` — содержит основной код плагина

## Файлы плагина

### `manifest.lua`

`manifest.lua` загружается перед `init.lua` и определяет метаданные и настройки плагина.

```lua
plugin {
  name = "My Plugin",
  version = "1.0",
  api_version = 1
}
```

Плагин должен вызвать `plugin()` в `manifest.lua`. Если этого не произошло, `init.lua` не будет загружен.

### `init.lua`

`init.lua` загружается после `manifest.lua` и содержит основной код плагина.

```lua
cmd.add("hello", function(ctx)
  ctx.reply("Hello!")
end, { source = "chat" })
```

Именно здесь обычно размещается логика плагина: команды, обработчики событий, таймеры и работа с API `cs-lua`.

## Системные директории

Помимо `plugins/`, в `addons/lua/` находятся директории, которые используются самим `cs-lua`.

<AccordionGroup>
  <Accordion title="core/">
    Содержит базовый Lua-код `cs-lua`, который загружается до плагинов.

    Файлы из `core/` определяют общие функции и API, доступные плагинам.
  </Accordion>

  <Accordion title="lib/">
    Содержит общие Lua-модули, которые можно подключать через `require()`.

    В отличие от `core/`, файлы из `lib/` не загружаются автоматически.
  </Accordion>

  <Accordion title="plugins/">
    Содержит установленные Lua-плагины. Каждый плагин располагается в отдельной папке.
  </Accordion>
</AccordionGroup>

## Подключение модулей

Для подключения Lua-модулей используется стандартная функция `require()`.

Модули плагина ищутся сначала в его собственной папке, а затем в общей директории `addons/lua/lib/`.

Например, если структура плагина выглядит так:

```text
plugins/
└── my_plugin/
    ├── manifest.lua
    ├── init.lua
    └── lib/
        └── sessions.lua
```

В `init.lua` модуль можно подключить:

```lua
local Session = require("lib.sessions")
```

Общие модули из `lib/` подключаются таким же образом:

```lua
local class = require("class")
```

Каждый плагин имеет собственное окружение и кеш `require`, поэтому одинаковые локальные модули разных плагинов не конфликтуют между собой.

## Окружение плагина

Каждый плагин запускается в собственном Lua-окружении.

Это означает, что глобальные переменные и локальные модули одного плагина не влияют на другие плагины:

```lua
-- plugins/first_plugin/init.lua
shared_value = "First Plugin"
```

```lua
-- plugins/second_plugin/init.lua
print(shared_value) -- nil
```

При этом API `cs-lua` и стандартные возможности Lua доступны каждому плагину.

Такое разделение позволяет плагинам использовать одинаковые имена переменных и собственные модули без конфликтов.

## Порядок загрузки

По умолчанию плагины загружаются в алфавитном порядке. Если порядок имеет значение, его можно задать в `load_order.txt`.

Файл находится в:

```text
cstrike/addons/lua/load_order.txt
```

Укажите имена плагинов, которые должны загрузиться первыми:

```text
# Плагины, которые должны загрузиться первыми

godmode
damager
stats

# Остальные плагины загружаются автоматически
```

Плагины из `load_order.txt` загружаются строго в указанном порядке. Все остальные загружаются после них в алфавитном порядке.

Пустые строки и комментарии, начинающиеся с `#`, игнорируются.

<Note>
  `load_order.txt` не обязан содержать все плагины. Указывайте только те, для которых порядок загрузки действительно важен.
</Note>

## Взаимодействие между плагинами

Плагины изолированы друг от друга и не могут напрямую обращаться к глобальным переменным другого плагина.

Для взаимодействия между плагинами используются `export()`, `import()` и `optional()`.

<CodeGroup>
```lua shop
-- plugins/shop/init.lua

export("give_vip", function(p)
  -- ...
end)
```

```lua stats
-- plugins/stats/init.lua

local shop = import("shop")

shop.give_vip(p)
```
</CodeGroup>

`import()` создаёт жёсткую зависимость от указанного плагина. Если плагин не найден, загрузка зависимого плагина завершится ошибкой.

Если зависимость необязательна, используйте `optional()`:

```lua
-- plugins/admin_manager/init.lua

local shop = optional("shop")

if shop then
  shop.give_vip(p)
end
```

<Note>
  Если взаимодействие между плагинами не является прямым вызовом функции, используйте систему событий через `hook`.
</Note>

## Дальше

<CardGroup cols={2}>
  <Card title="Первый плагин" icon="sparkles" href="/guides/first-plugin">
    Создайте свой первый Lua-плагин и добавьте команду.
  </Card>
  <Card title="Справочник API" icon="book" href="/api">
    Все пространства имён: игроки, сущности, события, таймеры, команды и т.д.
  </Card>
  <Card title="Готовые плагины" icon="puzzle-piece" href="https://github.com/KotyaraKryt/cs-lua-plugins">
    Примеры и готовые решения, которые можно установить сразу.
  </Card>
</CardGroup>