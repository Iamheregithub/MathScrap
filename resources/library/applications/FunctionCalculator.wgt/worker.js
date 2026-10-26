/* SPDX-License-Identifier: GPL-3.0-only
 * Bundled after Nerdamer and engine.js by build-calculator-worker.cjs. */
onmessage=function(event){try{postMessage(FunctionCalculatorEngine.calculate(event.data.latex,event.data.x,event.data.angle));}catch(_){postMessage({exact:''});}};
