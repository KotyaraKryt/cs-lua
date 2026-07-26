import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import CodeBlock from '@theme/CodeBlock';
import type {ReactNode} from 'react';

const snippet = `plugin { name = "Heal", api_version = 2 }

access.declare("heal.use", { desc = "Лечить себя", default = "vip" })

cmd.add("heal", function(ctx)
	local p = ctx.player
	if not p or not p:alive() then return end

	p:health(p:health() + 25)
	p:chat("{green}[Server]{default} подлечили до " .. p:health())
end, { perm = "heal.use" })`;

const features = [
  {
    title: 'Перезагрузка на лету',
    text: 'Положил файл в plugins/, набрал lua_reload — работает. Рестарт сервера и пересборка не нужны.',
  },
  {
    title: 'Изоляция плагинов',
    text: 'Плагин — папка со своим require и своим окружением. Глобалки не текут наружу, ошибка снимает плагин целиком, а не половину.',
  },
  {
    title: 'Права как именованные ноды',
    text: 'shop.vip.buy: группы, наследование, иммунитет по весу, выдача на срок, личные разрешения и запреты.',
  },
  {
    title: 'Без оффсетов',
    text: 'Состояние игрока читается через entvars и ReGameDLL API, а не по сигнатурам — не ломается от обновления игры.',
  },
  {
    title: 'Прекеш не роняет сервер',
    text: 'Модуль считает занятые слоты и отключает плагин-виновника до того, как движок упадёт с Host_Error.',
  },
  {
    title: 'Windows и Linux',
    text: 'Один код, lua_mm.dll и lua_mm_i386.so. Metamod поверх ReHLDS и ReGameDLL, внутри LuaJIT 2.1.',
  },
];

export default function Home(): ReactNode {
  const {siteConfig} = useDocusaurusContext();

  return (
    <Layout
      title="Плагины для CS 1.6 на Lua"
      description="Metamod-модуль поверх ReHLDS и ReGameDLL: пишешь плагины на Lua, перезагружаешь без рестарта сервера.">
      <header className="hero hero--home text--center">
        <div className="container">
          <h1 className="hero__title">{siteConfig.title}</h1>
          <p className="hero__subtitle">
            Плагины для CS&nbsp;1.6 на Lua вместо SourcePawn. Metamod-модуль
            поверх ReHLDS/ReGameDLL, внутри — LuaJIT&nbsp;2.1.
          </p>

          <div className="heroButtons">
            <Link className="button button--primary button--lg" to="/intro">
              Начать
            </Link>
            <Link
              className="button button--secondary button--lg"
              to="/install">
              Установка
            </Link>
          </div>

          <div className="heroSnippet">
            <CodeBlock language="lua" title="addons/lua/plugins/heal.lua">
              {snippet}
            </CodeBlock>
          </div>
        </div>
      </header>

      <main className="container">
        <div className="featureGrid">
          {features.map((f) => (
            <div className="featureCard" key={f.title}>
              <h3>{f.title}</h3>
              <p>{f.text}</p>
            </div>
          ))}
        </div>
      </main>
    </Layout>
  );
}
