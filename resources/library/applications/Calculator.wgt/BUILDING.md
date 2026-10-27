# Building the Calculator widget

The application loads `dist/calculator.js`; Node.js and Grunt are development
tools only. From this directory, with Node.js 24:

```sh
npm ci --ignore-scripts
npm run build
```

Commit the rebuilt bundle alongside source changes. `npm run watch` rebuilds
when files under `js/src` change.

Grunt and its plugins are pinned in `package.json`, with their full dependency
tree recorded in `package-lock.json`. The unused concat plugin was removed.
The `globule` override selects patched minimatch 3.1.5 within the same major
version used by the watch plugin. Do not remove the override without auditing
the resulting dependency tree.

Run `npm audit --audit-level=moderate` here and `npm test` from
`tests/teaching` at the repository root. Security CI rebuilds this bundle,
checks reproducibility, audits dependencies, and exercises calculator buttons,
arithmetic, memory and saved-state restoration.
