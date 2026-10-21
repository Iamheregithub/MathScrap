// DOM-level export/persistence test. Requires jsdom; does not verify Qt or pixels.
const {JSDOM,VirtualConsole}=require('jsdom');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const errors=[];
const consoleBridge=new VirtualConsole();consoleBridge.on('jsdomError',e=>errors.push(e.message));
const dom=new JSDOM(fs.readFileSync(path.resolve(__dirname,'../../resources/graphdesk/GraphDesk.html'),'utf8'),{
 runScripts:'dangerously',url:'https://graphdesk.test/',pretendToBeVisual:true,virtualConsole:consoleBridge,
 beforeParse(w){w.structuredClone=structuredClone;w.ResizeObserver=class{observe(){} unobserve(){} disconnect(){}};w.matchMedia=()=>({matches:false,addListener(){},removeListener(){}});}
});
const w=dom.window;
const result=w.eval(`(()=>{
 validForExport();
 const p=project();
 p.plots=[
  {expr:'x^2',color:'#ff0000',show:true,dash:'solid',width:'2.5'},
  {expr:'2x+1',color:'#0000ff',show:true,dash:'dashed',width:'2.5'},
  {expr:'sin(x)',color:'#008000',show:true,dash:'solid',width:'2.5'},
  {expr:'-x+3',color:'#800080',show:true,dash:'solid',width:'2.5'}];
 importProject(p);validForExport();
 const svg=exportScene(scene(),'key').svg;
 const serialized=JSON.stringify(project());
 importProject(JSON.parse(serialized));
 return {svg,restored:exportScene(scene(),'key').svg,count:project().plots.length,serialized};
})()`);
assert.equal(errors.length,0,errors.join('\n'));
assert.equal(result.count,4);
assert.ok(result.svg.startsWith('<svg'));
assert.ok(result.svg.includes('stroke-dasharray'));
assert.equal(result.svg,result.restored);
// Exercise the exact metadata format passed between C++ and GraphDesk.
const metadata=Buffer.from(result.serialized).toString('base64');
const embedded=result.svg.replace('>','><metadata id="graphdesk-project">'+metadata+'</metadata>');
const svgdoc=new JSDOM(embedded,{contentType:'image/svg+xml'});
const recovered=Buffer.from(svgdoc.window.document.getElementById('graphdesk-project').textContent,'base64').toString();
assert.deepEqual(JSON.parse(recovered),JSON.parse(result.serialized));
console.log('PASS: DOM startup, four functions, dashed SVG, project restore, embedded metadata round trip');
svgdoc.window.close();dom.window.close();
