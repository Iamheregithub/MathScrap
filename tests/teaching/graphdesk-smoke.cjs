// Run with Node and Playwright available. No server or external service needed.
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const path = require('node:path');
(async () => {
 const browser = await chromium.launch({headless:true,args:['--no-sandbox']});
 const page = await browser.newPage({viewport:{width:1600,height:900}});
 const errors=[]; page.on('pageerror',e=>errors.push(e.message));
 await page.goto('file://'+path.resolve(__dirname,'../../resources/graphdesk/GraphDesk.html'));
 const result=await page.evaluate(()=>{
   validForExport();
   const original=project();
   const svg=exportScene(scene(),'key').svg;
   const edited={...original,plots:[
    {expr:'x^2',color:'#ff0000',show:true,dash:'solid',width:'2.5'},
    {expr:'2x+1',color:'#0000ff',show:true,dash:'dashed',width:'2.5'},
    {expr:'sin(x)',color:'#008000',show:true,dash:'solid',width:'2.5'},
    {expr:'-x+3',color:'#800080',show:true,dash:'solid',width:'2.5'}]};
   importProject(edited); validForExport();
   const multiple=exportScene(scene(),'key').svg;
   const roundTrip=project(); importProject(JSON.parse(JSON.stringify(roundTrip)));
   const restored=exportScene(scene(),'key').svg;
   return {svg,multiple,restored,plotCount:project().plots.length};
 });
 assert.equal(errors.length,0, errors.join('\n'));
 assert.equal(result.plotCount,4);
 assert.ok(result.svg.startsWith('<svg'));
 assert.ok(result.multiple.includes('stroke-dasharray'));
 assert.equal(result.multiple,result.restored,'project should restore its SVG exactly');
 console.log('PASS: GraphDesk startup, four functions, dashed export, and project round trip');
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
