# Offline dependencies

MathQuill: Desmos fork https://github.com/desmosinc/mathquill,
commit bb9974ab637e1dd57673fe8c96a00c85cfc3da37 (main).
Built unmodified using `npm ci --ignore-scripts` and `make dev`.
MathQuill JS/CSS are Mozilla Public License 2.0; see MPL-2.0.txt.
Source: https://github.com/desmosinc/mathquill/tree/bb9974ab637e1dd57673fe8c96a00c85cfc3da37
Symbola font from the same tree, src/fonts/Symbola.woff2.

Nerdamer 1.1.13: https://github.com/jiggzson/nerdamer, npm distribution
`all.min.js`, unmodified. MIT licence: NERDAMER-LICENSE.txt.

No network resources are needed at runtime. MathScrap's engine accepts only a
restricted mathematical grammar before passing expressions to Nerdamer.
