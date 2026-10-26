/* SPDX-License-Identifier: GPL-3.0-only */
(function(){
'use strict';
const $=id=>document.getElementById(id), MQ=MathQuill.getInterface(3), Engine=FunctionCalculatorEngine;
let angle='degrees', active, restoring=true, sequence=0, timer, worker, workerTimer, host=null;
const exactField=MQ.StaticMath($('exact'));
function resize(){document.querySelector('main').style.transform='scale('+Math.min(innerWidth/680,innerHeight/660)+')';}
window.addEventListener('resize',resize);resize();
function state(){return {version:1,latex:expression.latex(),x:xField.latex(),angle};}
function persist(){if(!restoring&&host)host.setPreference('functionCalculatorState',JSON.stringify(state()));}
function clearOutput(){exactField.latex('');$('decimal').textContent='';}
function stopWorker(){clearTimeout(workerTimer);if(worker){worker.terminate();worker=null;}}
function update(){
  if(restoring)return;
  persist();const id=++sequence;clearTimeout(timer);stopWorker();clearOutput();
  const latex=expression.latex();let hasX=/x/.test(latex.replace(/\\[a-zA-Z]+/g,''));
  try{hasX=Engine.containsX(Engine.parse(latex));}catch(_){}
  $('x-row').hidden=!hasX;document.body.classList.toggle('has-x',hasX);
  $('status').className='';$('status').textContent=latex?'Calculating…':'Enter an expression to calculate.';
  if(!latex)return;
  timer=setTimeout(()=>{
    try{
      const result=Engine.calculate(latex,xField.latex(),angle,null);
      $('status').textContent='Answer';$('decimal').textContent='≈ '+result.decimal;
      // Exact simplification runs outside the UI thread, with a time limit.
      try{
        const url=URL.createObjectURL(new Blob([window.mathscrapExactWorkerSource],{type:'text/javascript'}));
        worker=new Worker(url);URL.revokeObjectURL(url);
        worker.onmessage=e=>{if(id!==sequence)return;const r=e.data;if(r.exact){try{exactField.latex(r.exact);}catch(_){} }stopWorker();};
        worker.onerror=()=>stopWorker();
        worker.postMessage({latex,x:xField.latex(),angle});workerTimer=setTimeout(stopWorker,1500);
      }catch(_){stopWorker();}
    }catch(e){$('status').className='error';$('status').textContent=e instanceof Engine.InputError?e.message:'Unable to calculate this expression.';}
  },120);
}
const options={autoCommands:'sqrt nthroot pi',autoOperatorNames:'sin cos tan sec csc cot arcsin arccos arctan ln log abs exp',
  restrictMismatchedBrackets:true,supSubsRequireOperand:true,spaceBehavesLikeTab:true,leftRightIntoCmdGoes:'up',maxDepth:24,
  handlers:{edit:update,enter:update}};
const expression=MQ.MathField($('expression'),options), xField=MQ.MathField($('x-value'),options);active=expression;
for(const [id,field] of [['expression',expression],['x-value',xField]]){
  $(id).addEventListener('focusin',()=>{active=field;});
  $(id).querySelector('textarea').setAttribute('aria-label',id==='expression'?'Expression or function':'Value of x');
}
function setAngle(value){angle=value;for(const id of ['degrees','radians'])$(id).setAttribute('aria-pressed',String(id===value));update();}
$('degrees').addEventListener('click',()=>setAngle('degrees'));$('radians').addEventListener('click',()=>setAngle('radians'));
for(const button of document.querySelectorAll('button[data-type],button[data-cmd],button[data-key],button[data-write]')){
  // Preserve the field's selection and cursor on mouse/pen press. Use ONE click
  // handler for activation; pointerdown + click would insert each symbol twice.
  button.addEventListener('pointerdown',e=>e.preventDefault());
  button.addEventListener('mousedown',e=>e.preventDefault());
  button.addEventListener('click',()=>{
    active.focus();if(button.dataset.cmd)active.cmd(button.dataset.cmd);
    if(button.dataset.type)active.typedText(button.dataset.type);
    if(button.dataset.write)active.write(button.dataset.write);
    if(button.dataset.key)active.keystroke(button.dataset.key);
    update();
  });
}
$('clear').addEventListener('click',()=>{active.latex('');active.focus();update();});
$('calculate').addEventListener('click',()=>{active.focus();update();});
const controls=Array.from(document.querySelectorAll('button,textarea'));
controls.forEach(control=>control.disabled=true);
async function restore(){
  // Native WebChannel arrives asynchronously. Never overwrite saved preferences
  // with the initially empty editor while that channel is still connecting.
  if(window.qt){for(let i=0;i<200&&!window.sankore?.async;i++)await new Promise(r=>setTimeout(r,25));}
  host=window.sankore?.async?window.sankore:null;
  try{
    const raw=host?await host.async.preference('functionCalculatorState',''):'';
    if(raw){const s=JSON.parse(raw);if(s.version===1){
      if(typeof s.latex==='string'&&s.latex.length<=700)expression.latex(s.latex);
      if(typeof s.x==='string'&&s.x.length<=700)xField.latex(s.x);
      angle=s.angle==='radians'?'radians':'degrees';
    }}
  }catch(_){$('status').textContent='The saved expression could not be read. Enter a new expression.';}
  restoring=false;controls.forEach(control=>control.disabled=false);setAngle(angle);expression.focus();
}
// Local API for native integration tests; UI and tests use the same operations.
window.mathscrapCalculator={expression,xField,getState:state,restore:async s=>{
  restoring=true;expression.latex(s.latex||'');xField.latex(s.x||'');restoring=false;setAngle(s.angle==='radians'?'radians':'degrees');
},ready:restore()};
})();
