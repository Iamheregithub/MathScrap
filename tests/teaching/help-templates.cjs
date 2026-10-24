const {JSDOM,VirtualConsole}=require('jsdom');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../../resources/library/interactivities');
let checks=0;
for(const dir of fs.readdirSync(root)){
 const base=path.join(root,dir),help=path.join(base,'help.html');
 if(!fs.existsSync(path.join(base,'js/help-template.html')))continue;
 const html=fs.readFileSync(help,'utf8');
 for(const locale of ['','fr','sk','fr_CA','xx']){
  const dom=new JSDOM(html,{runScripts:'outside-only',url:'https://help.test/',virtualConsole:new VirtualConsole()});
  const w=dom.window;w.eval(fs.readFileSync(path.join(base,'js/lib/jquery-1.7.1.min.js'),'utf8'));
  w.eval(fs.readFileSync(path.join(base,'js/lib/mustache.js'),'utf8'));
  w.$.fn.ready=function(){return this};
  let loaded='';w.$.ajax=opts=>{
   const file=path.join(base,opts.url),exists=fs.existsSync(file);
   if(exists){loaded=opts.url;opts.success(fs.readFileSync(file,'utf8'),'success')}
   return {fail(fn){if(!exists)fn();return this}};
  };
  for(const script of w.document.querySelectorAll('script:not([src])'))w.eval(script.textContent);
  w.loadFile('js/help',locale);
  assert.ok(loaded.endsWith('.html'),dir+': '+locale);
  assert.ok(w.document.querySelector('#core>.wrapper>div').textContent.trim(),dir+': '+locale);
  dom.window.close();checks++;
 }
}
assert.equal(checks,50);
console.log('PASS: 50 help-template language and fallback loads');
