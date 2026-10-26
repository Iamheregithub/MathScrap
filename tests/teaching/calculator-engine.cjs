// SPDX-License-Identifier: GPL-3.0-only
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const dir=path.resolve(__dirname,'../../resources/library/applications/FunctionCalculator.wgt');
const E=require(path.join(dir,'engine.js'));const sandbox={};vm.createContext(sandbox);vm.runInContext(fs.readFileSync(path.join(dir,'vendor/nerdamer.js'),'utf8'),sandbox);const N=sandbox.nerdamer;
let count=0;
function check(latex,expected,x='',mode='degrees'){
 const r=E.calculate(latex,x,mode,N);assert.ok(Math.abs(r.value-expected)<=1e-11*Math.max(1,Math.abs(expected)),`${latex}: ${r.value} != ${expected}`);count++;return r;
}
function rejects(latex,x='',mode='degrees'){assert.throws(()=>E.calculate(latex,x,mode,N),E.InputError,latex);count++;}
check('\\sqrt{16}+3^{2}',13);check('\\frac{1}{3}+\\frac{1}{6}',.5);
assert.equal(check('\\sqrt{12}',Math.sqrt(12)).exact,'2 \\cdot \\sqrt{3}');
assert.equal(check('\\frac{1}{3}',1/3).exact,'\\frac{1}{3}');
check('f(x)=x^{2}+3x-1',9,'2');check('y=2x+1',2,'\\frac{1}{2}');check('x^{2}',3,'\\sqrt{3}');
check('\\frac{1+\\frac{1}{x}}{1-\\frac{1}{x}}',3,'2');
check('\\frac{-6+\\sqrt{6^{2}-4\\cdot1\\cdot7}}{2\\cdot1}',-3+Math.sqrt(2));
check('\\sqrt[3]{-8}',-2);check('\\sqrt[4]{16}',2);check('2^{\\frac{3}{2}}',Math.sqrt(8));
check('-2^{2}',-4);check('(-2)^{2}',4);check('2^{-3}',.125);check('2^{3^{2}}',512);
check('\\sin(30)',.5);check('\\cos(60)',.5);check('\\tan(45)',1);check('\\sin^{2}(30)',.25);
check('\\arcsin(\\frac{1}{2})',30);check('\\arccos(\\frac{1}{2})',60);check('\\arctan(1)',45);
check('\\sin(\\frac{\\pi}{2})',1,'','radians');check('\\arcsin(1)',Math.PI/2,'','radians');
check('\\log_{2}(8)',3);check('\\log(100)',2);check('\\ln(e)',1);check('e^{0}',1);
check('1e2',2*Math.E);check('2\\pi',2*Math.PI);check('3(x+1)',9,'2');check('.5+1.25',1.75);
check('\\left|x-2\\right|',5,'-3');check('\\operatorname{abs}(x)',2,'-2');
check('5!',120);check('0!',1);check('\\sec(60)',2);check('\\csc(30)',2);check('\\cot(45)',1);
for(const l of ['', '1+', '\\frac{1}{}','\\sqrt{}','x^{ }','(1+2','1+2)', '\\sqrt{-1}', '\\sqrt[0]{8}', '\\sqrt[4]{-8}', '\\frac{1}{0}', '0^{-1}', '0^{0}', '\\log(0)', '\\ln(-1)', '\\log_{1}(3)', '\\log_{-2}(8)', '\\arcsin(2)', '\\tan(90)', '\\sec(90)', '\\csc(0)', '\\cot(180)', '(-1)!', '1.5!', '171!', '2^{10001}', 'a+1', 'alert(1)', 'x=2', '\\int_0^1x', '\\sum_{n=1}^{5}n', '1;2', 'constructor(1)', '1'.repeat(701)])rejects(l);
rejects('x+1');rejects('x+1','x');rejects('x+1','\\sqrt{-1}');
const large=check('1.00001^{9999}',Math.pow(1.00001,9999));assert.equal(large.exact,'');
// The generated worker must match its auditable sources.
const expected=['vendor/nerdamer.js','engine.js','worker.js'].map(p=>fs.readFileSync(path.join(dir,p),'utf8')).join('\n');
const workerContext={window:{}};vm.runInNewContext(fs.readFileSync(path.join(dir,'exact-worker-source.js'),'utf8'),workerContext);assert.equal(workerContext.window.mathscrapExactWorkerSource,expected);count++;
console.log(`Function Calculator engine: ${count} checks passed.`);
