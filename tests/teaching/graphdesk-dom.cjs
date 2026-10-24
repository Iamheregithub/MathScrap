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
// HTML-shaped titles stay text through graph, worksheet preview and print paths.
w.eval(`(()=>{
 const p=project();p.settings.title='</text><img data-injected="yes" src=x onerror="window.hacked=1"> & "quotes"';
 p.settings.xname='x < 3 & y > 2';importProject(p);validForExport();
 printSVG(exportScene(scene(),'key').svg);
 pageItems=[scene()];showSVG($('pagePreview'),worksheetSVG().svg);printSVG(worksheetSVG().svg,true);
})()`);
assert.equal(w.document.querySelectorAll('[data-injected]').length,0);
assert.equal(w.hacked,undefined);
assert.ok(w.document.getElementById('pagePreview').textContent.includes('</text><img'));
assert.ok(w.document.querySelector('#printArea .print-sheet svg'));
// Project and worksheet-snapshot imports must validate attributes before mutation.
for(const kind of ['plot','snapshotPlot','snapshotFont','snapshotColor']){
 const before=w.eval('JSON.stringify(project())');
 const malformed=JSON.parse(before);
 if(kind==='plot')malformed.plots[0].width='2.5" data-injected="yes';
 else{
  const item=JSON.parse(before);delete item.pageItems;
  if(kind==='snapshotPlot')item.plots[0].width='2.5" data-injected="yes';
  if(kind==='snapshotFont')item.settings.fontSize='14" onload="window.hacked=1';
  if(kind==='snapshotColor')item.settings.axisColor='#000000" onload="window.hacked=1';
  malformed.pageItems=[item];
 }
 assert.throws(()=>w.importProject(malformed),undefined,kind);
 assert.equal(w.eval('JSON.stringify(project())'),before,kind+' must not alter the current lesson');
 assert.equal(w.document.querySelectorAll('[data-injected],[onload]').length,0,kind);
}
{
 const item=JSON.parse(w.eval('JSON.stringify(project())'));delete item.pageItems;
 item.settings.fontSize='14" onload="window.hacked=1';
 assert.throws(()=>w.GraphDesk.exportScene(item,'key'));
}
// Exercise the exact metadata format passed between C++ and GraphDesk.
const metadata=Buffer.from(result.serialized).toString('base64');
const svgdoc=new JSDOM(result.svg,{contentType:'image/svg+xml'});
const metadataNode=svgdoc.window.document.createElementNS('http://www.w3.org/2000/svg','metadata');
metadataNode.id='graphdesk-project';metadataNode.textContent=metadata;
svgdoc.window.document.documentElement.prepend(metadataNode);
const recovered=Buffer.from(svgdoc.window.document.getElementById('graphdesk-project').textContent,'base64').toString();
assert.deepEqual(JSON.parse(recovered),JSON.parse(result.serialized));
console.log('PASS: DOM startup, four functions, dashed SVG, project restore, embedded metadata round trip');
svgdoc.window.close();dom.window.close();
