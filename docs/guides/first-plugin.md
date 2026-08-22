---
description: "Как создать свой первый плагин на Lua."
---

# Первый плагин

Создадим простой плагин, который регистрирует чат-команду `!hello` и `/hello`.

## Создание плагина

Плагин представляет собой отдельную папку в `addons/lua/plugins/` с двумя обязательными файлами: `manifest.lua` и `init.lua`.

<Tree>
  <Tree.Folder name="first_plugin" defaultOpen>
    <Tree.File name="manifest.lua" />
    <Tree.File name="init.lua" />
  </Tree.Folder>
</Tree>

<Steps>
  <Step title="Создайте папку плагина">
    Создайте новую папку внутри `cstrike/addons/lua/plugins/`.
  </Step>

  <Step title="Создайте manifest.lua">
    Внутри папки `first_plugin` создайте файл `manifest.lua`.
  </Step>

  <Step title="Создайте init.lua">
    Внутри папки `first_plugin` создайте файл `init.lua`.
  </Step>

  <Step title="Настройте manifest.lua">
    Откройте `manifest.lua` и добавьте:

    ```lua
    plugin {
      name = "First Plugin",
      version = "1.0",
      api_version = 1
    }
    ```
    `manifest.lua` загружается первым и содержит основную информацию о плагине.
  </Step>

  <Step title="Настройте init.lua">
    Откройте `init.lua` и добавьте:

    ```lua
    print("Hello from First Plugin!")
    ```
    `init.lua` содержит основной код плагина и загружается после `manifest.lua`.
  </Step>

  <Step title="Загрузите плагин">
    Сохраните изменения и выполните в консоли сервера:

    ```text
    lua_reload
    ```

    После перезагрузки в консоли должно появиться:

    ```
    Hello from First Plugin!
    ```
  </Step>

  <Step title="Добавьте команду">
    Откройте `init.lua` и замените его содержимое на:

    ```lua
    cmd.add("hello", function(ctx)
        ctx.reply("Hello!")
    end, { source = "chat" })
    ```
    Этот код добавляет чат-команду `hello`, доступную через `!hello` или `/hello`.
  </Step>
</Steps>

<Tip>
  `lua_reload` перезагружает Lua-плагины без необходимости перезапускать сервер.
</Tip>

## Как это работает

Теперь разберём, что происходит внутри нашего плагина.

### `manifest.lua`

`manifest.lua` загружается первым и регистрирует плагин через `plugin()`:

```lua
plugin {
  name = "First Plugin",
  version = "1.0",
  api_version = 1
}
```

Здесь мы указываем название и версию плагина, а также версию API, с которой он работает.

### `init.lua`

`init.lua` содержит основной код плагина и загружается после `manifest.lua`.

В нашем примере он добавляет команду `hello`:

```lua
cmd.add("hello", function(ctx)
    ctx.reply("Hello!")
end, { source = "chat" })
```

`cmd.add()` регистрирует новую команду, а `ctx.reply()` отправляет ответ тому, кто её выполнил.

### Использование команды

В нашем примере команда доступна только из чата благодаря `source = "chat"`.

Выполните в чате:

```text
!hello
```
или
```text
/hello
```

Плагин ответит:
```
Hello!
```

### Контекст команды

Каждый обработчик команды получает `ctx` — контекст её выполнения.

```lua
cmd.add("hello", function(ctx)
    ctx.reply("Hello!")
end, { source = "chat" })
```

В `ctx` находится информация о текущем вызове команды. Например, `ctx.reply()` позволяет отправить ответ туда, откуда была вызвана команда.

## Дальше

<CardGroup cols={2}>
  <Card title="Структура плагина" icon="folder-tree" href="/guides/plugin-structure">
    Как устроен плагин, `require`, окружение и порядок загрузки.
  </Card>
  <Card title="Справочник API" icon="book" href="/api">
    Все пространства имён: игроки, сущности, события, таймеры, команды и т.д.
  </Card>
  <Card title="Готовые плагины" icon="puzzle-piece" href="https://github.com/KotyaraKryt/cs-lua-plugins">
    Примеры и готовые решения, которые можно поставить сразу.
  </Card>
</CardGroup>