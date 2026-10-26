// SPDX-License-Identifier: GPL-3.0-only
const assert=require('node:assert/strict'),path=require('node:path'),fs=require('node:fs');
const {chromium}=require('playwright');
const dir=path.resolve(__dirname,'../../resources/library/applications/FunctionCalculator.wgt');
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.CALCULATOR_CHROMIUM||undefined,headless:true,args:['--no-sandbox','--disable-dev-shm-usage','--disable-gpu']});
 const page=await browser.newPage({viewport:{width:680,height:660}}),errors=[];
 page.setDefaultTimeout(6000);page.on('pageerror',e=>errors.push(e.message));let checks=0;
 await page.addInitScript(()=>{window.savedCalculatorPreferences={};window.sankore={async:{preference:async key=>window.savedCalculatorPreferences[key]||''},setPreference:(key,value)=>{window.savedCalculatorPreferences[key]=value;}};});
 await page.goto('file://'+path.join(dir,'index.html'));await page.waitForFunction(()=>window.mathscrapCalculator);await page.evaluate(()=>mathscrapCalculator.ready);
 const api=async(code)=>page.evaluate(code);
 const set=async(latex,x='',angle='degrees')=>page.evaluate(s=>mathscrapCalculator.restore(s),{latex,x,angle});
 const answer=async expected=>{console.log('Check answer',expected);await page.waitForFunction(v=>document.querySelector('#decimal').textContent===v,'≈ '+expected);checks++;};
 const exact=async expected=>{console.log('Check exact',expected);await page.waitForFunction(v=>MathQuill.getInterface(3)(document.querySelector('#exact')).latex().replace(/\s/g,'')===v.replace(/\s/g,''),expected);checks++;};
 await api(()=>mathscrapCalculator.expression.typedText('sqrt16'));await answer('4');await exact('4');
 assert.equal(await api(()=>mathscrapCalculator.expression.latex()),'\\sqrt{16}');checks++;
 await set('\\sqrt{12}');await answer('3.46410161514');await exact('2\\cdot\\sqrt{3}');
 await set('\\frac{1}{3}+\\frac{1}{6}');await answer('.5'.replace(/^\./,'0.'));await exact('\\frac{1}{2}');
 await set('f(x)=x^{2}+3x-1','2');await answer('9');assert.equal(await page.locator('#x-row').isVisible(),true);checks++;
 await set('x^2','\\sqrt{3}');await answer('3');
 await set('\\sin(30)');await answer('0.5');await page.getByRole('button',{name:'Radians',exact:true}).click();await answer('-0.988031624093');
 await set('\\sin(\\frac{\\pi}{2})','','radians');await answer('1');
 await set('\\log_{2}(8)');await answer('3');
 await set('\\sqrt[3]{-8}');await answer('-2');
 await set('\\left|x-2\\right|','-3');await answer('5');
 // Every keypad insertion is tested using an actual single mouse click.
 for(const label of ['Fraction','Square root','Indexed root','Power','Pi','Absolute value','Logarithm with base']){
   await set(label==='Power'?'2':'');await api(()=>mathscrapCalculator.expression.focus());
   await page.getByRole('button',{name:label,exact:true}).click();
   const l=await api(()=>mathscrapCalculator.expression.latex());assert.ok(l.length>0,label);checks++;
 }
 // Build a logarithm solely by visible buttons: base slot first, then argument.
 await set('');await page.getByRole('button',{name:'Logarithm with base',exact:true}).click();
 await page.getByRole('button',{name:'2',exact:true}).click();await page.getByRole('button',{name:'Move right',exact:true}).click();
 await page.getByRole('button',{name:'Move right',exact:true}).click();await page.getByRole('button',{name:'8',exact:true}).click();await answer('3');
 // A fraction key followed by digits and Next slot, without losing cursor focus.
 await set('');await page.getByRole('button',{name:'Fraction',exact:true}).click();await page.getByRole('button',{name:'1',exact:true}).click();
 await page.getByRole('button',{name:'Next slot',exact:true}).click();await page.getByRole('button',{name:'2',exact:true}).click();await answer('0.5');
 // Chromium pen packets: pointerdown/mousedown/click must insert only once.
 await set('');const rect=await page.getByRole('button',{name:'7',exact:true}).boundingBox(),cdp=await page.context().newCDPSession(page);
 await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',x:rect.x+rect.width/2,y:rect.y+rect.height/2,button:'left',buttons:1,clickCount:1,pointerType:'pen'});
 await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',x:rect.x+rect.width/2,y:rect.y+rect.height/2,button:'left',buttons:0,clickCount:1,pointerType:'pen'});
 await answer('7');assert.equal(await api(()=>mathscrapCalculator.expression.latex()),'7');checks++;
 // A pointer release outside a key must not activate it.
 await set('');await page.mouse.move(rect.x+10,rect.y+10);await page.mouse.down();await page.mouse.move(675,615);await page.mouse.up();
 assert.equal(await api(()=>mathscrapCalculator.expression.latex()),'');checks++;
 // Clear immediately removes stale results; invalid expressions never retain answers.
 await set('13');await answer('13');await set('\\frac{1}{0}');await page.waitForFunction(()=>document.querySelector('#status').classList.contains('error'));
 assert.equal(await page.locator('#decimal').textContent(),'');assert.equal(await api(()=>MathQuill.getInterface(3)(document.querySelector('#exact')).latex()),'');checks++;
 await set('x^2','2','radians');await answer('4');const saved=await api(()=>window.savedCalculatorPreferences.functionCalculatorState);assert.deepEqual(JSON.parse(saved),{version:1,latex:'x^{2}',x:'2',angle:'radians'});checks++;
 // Reopen with the actual host preference API; independent widgets stay separate.
 await page.addInitScript(value=>{window.savedCalculatorPreferences={functionCalculatorState:value};window.sankore={async:{preference:async key=>window.savedCalculatorPreferences[key]||''},setPreference:(key,value)=>window.savedCalculatorPreferences[key]=value};},saved);
 await page.reload();await page.waitForFunction(()=>window.mathscrapCalculator);await page.evaluate(()=>mathscrapCalculator.ready);await answer('4');assert.equal(await page.getByRole('button',{name:'Radians',exact:true}).getAttribute('aria-pressed'),'true');checks++;
 const second=await browser.newPage({viewport:{width:680,height:660}});await second.goto('file://'+path.join(dir,'index.html'));await second.waitForFunction(()=>window.mathscrapCalculator);assert.equal(await second.evaluate(()=>mathscrapCalculator.expression.latex()),'');checks++;
 // Rapid edits must not allow a stale exact-worker response to replace the latest result.
 await set('\\sqrt{12}');await set('2+2');await answer('4');await exact('4');
 await set('\\sqrt{12}+\\frac{1}{3}');await page.waitForTimeout(500);
 const out=process.env.CALCULATOR_REPORT_DIR||path.join(__dirname,'calculator-report');fs.mkdirSync(out,{recursive:true});
 await page.screenshot({path:path.join(out,'calculator-expression.png')});
 await set('f(x)=x^{2}+3x-1','2');await answer('9');await page.waitForTimeout(400);await page.screenshot({path:path.join(out,'calculator-function.png')});
 assert.equal(await page.evaluate(()=>document.documentElement.scrollHeight<=innerHeight),true);checks++;
 await page.setViewportSize({width:510,height:495});await page.screenshot({path:path.join(out,'calculator-scaled.png')});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);checks++;
 assert.deepEqual(errors,[]);checks++;
 fs.writeFileSync(path.join(out,'browser-results.json'),JSON.stringify({checks,errors},null,2));console.log(`Function Calculator browser: ${checks} checks passed.`);
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
