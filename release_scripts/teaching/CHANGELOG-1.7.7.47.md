# MathScrap 1.7.7.47

- Update the bundled Calculator widget from Grunt 0.4.2 to 1.6.3, addressing
  the three reported Dependabot alerts for code execution, path traversal
  and file-copy races.
- Update the minification and watch plugins, remove the unused concat plugin,
  and pin the watch dependency's minimatch to patched version 3.1.5.
- Add a dependency lockfile, rebuild the calculator bundle, and document its
  build commands.
- Add calculator regression tests and CI checks for reproducible builds and
  dependency vulnerabilities.
- Update About and Windows installer versions to 1.7.7.47.

Validation: the widget builds with Node.js 24; npm audit reports zero
vulnerabilities. Calculator checks cover arithmetic precedence, parentheses,
negative and decimal results, button input, memory and saved-state restoration.
The standard teaching and browser security checks also run in CI.
