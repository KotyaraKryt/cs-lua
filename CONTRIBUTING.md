# Contributing

## Commits

[Conventional Commits](https://www.conventionalcommits.org/), in English.

```
<type>(<scope>): <subject>

<body>
```

- **type** — `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`.
- **scope** — the area touched: a namespace (`hook`, `players`, `mysql`), a
  subsystem (`ci`, `docs`, `access`), or omit it if the commit is genuinely
  project-wide.
- **subject** — imperative mood ("add", not "added"/"adds"), lowercase, no
  trailing period, fits on one line.
- **body** — optional. Explain *why*, not what the diff already shows. Wrap
  at ~72 columns.

Breaking changes to the Lua API bump `api_version` — say so in the body and
in `CHANGELOG.md`, not just in the subject line.

```
feat(mysql): add connection pooling

fix(access): stop group inheritance loop on self-reference

docs(players): document p:trace() hitgroup values

refactor(hook): extract event table construction into one helper
```

One logical change per commit. A doc rewrite and an unrelated code fix are
two commits, not one.

## Docs

`docs/api/**` is generated — edit `scripts/docs/api/*.py`, then run
`python scripts/docs/gen.py`. Hand-written pages (`docs/intro.md`,
`docs/install.md`, `docs/plugins.md`, `docs/building.md`, `docs/api/index.md`,
`docs/api/console.md`) are edited directly.

Before committing doc changes: `cd website && npm run build` — it fails on
any broken link or anchor across all 46 pages.
