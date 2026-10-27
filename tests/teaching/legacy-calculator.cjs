// Exercise the shipped Calculator bundle after rebuilding it with patched tools.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {JSDOM} = require('jsdom');
const bundle = fs.readFileSync(path.resolve(__dirname,
  '../../resources/library/applications/Calculator.wgt/dist/calculator.js'), 'utf8');
function open(state = {}) {
  const dom = new JSDOM('<div id="ubwidget"></div>', {runScripts: 'dangerously'});
  // jsdom has no layout or scrolling implementation.
  dom.window.HTMLElement.prototype.scrollIntoView = function () {};
  dom.window.eval(bundle);
  const calculator = dom.window.Sankore.Calculator.create('ubwidget', {
    locale: 'en', ready() { this.init(state); }
  });
  return {dom, calculator};
}
const {dom, calculator} = open();
try {
  for (const [expression, expected] of [
    ['2+3*4', 14], ['(2+3)*4', 20], ['7-12', -5],
    ['3/4', 0.75], ['0.5+0.25', 0.75]
  ]) {
    assert.equal(calculator.calculusEngine.evaluate(expression).getValue(), expected, expression);
  }
  function press(text) {
    const button = [...dom.window.document.querySelectorAll('td button')]
      .find(element => element.innerText === text);
    assert.ok(button, `Missing calculator button: ${text}`);
    button.dispatchEvent(new dom.window.MouseEvent('mousedown', {bubbles: true, button: 0}));
  }
  for (const key of ['2', '+', '3', '×', '4', 'M+']) press(key);
  assert.equal(calculator.output.getValue(), 14);
  assert.equal(dom.window.document.querySelector('.result-row').textContent, '14');
  const state = JSON.parse(JSON.stringify(calculator.getState()));
  assert.equal(Number(state.memory), 14);
  const restored = open(state);
  try {
    assert.equal(restored.calculator.output.getValue(), 14);
    assert.equal(restored.calculator.memory, 14);
    assert.equal(restored.calculator.history.length, 1);
  } finally { restored.dom.window.close(); }
  press('C');
  assert.equal(calculator.output, null);
  for (const key of ['8', '÷', '2', '=']) press(key);
  assert.equal(calculator.output.getValue(), 4);
  console.log('Bundled Calculator: arithmetic, buttons, memory and state restore passed.');
} finally { dom.window.close(); }
