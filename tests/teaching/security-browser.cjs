// Chromium enforces sandbox boundaries that jsdom cannot model.
const {chromium}=require('playwright');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
async function main(){
 const browser=await chromium.launch({headless:true});
 try{
  for(const kind of ['embed','html','video']){
   const page=await browser.newPage();
   await page.route('**/*',route=>route.fulfill({status:200,contentType:'text/html',body:'<div id="ubwidget"></div><textarea id="editor"></textarea><div id="data"></div>'}));
   await page.goto('https://mathscrap.test/');
   await page.addScriptTag({content:read('resources/library/interactivities/Order Sentence.wgt/scripts/jquery-1.6.2.min.js')});
   const source={embed:'resources/library/applications/AnyEmbed.wgt/js/ubw-main.js',html:'resources/library/applications/Html.wgt/markitup/jquery.markitup.js',video:'resources/library/applications/VideoPicker.wgt/scripts/jquery.oembed.js'}[kind];
   await page.addScriptTag({content:read(source)});
   await page.evaluate(async kind=>{
    window.results=[];window.sankore={async:{preference:async()=>''},setPreference(){}};
    window.addEventListener('message',e=>{if(e.data?.sandboxProbe)window.results.push({data:e.data,origin:e.origin})});
    const markup='<p id="rendered">Formatted <strong>content</strong></p><script>'+"let parentAccess=false,nativeAccess=false;try{parent.document.body.dataset.hacked='yes';parentAccess=true}catch(e){}try{nativeAccess=!!parent.sankore}catch(e){}parent.postMessage({sandboxProbe:true,parentAccess,nativeAccess,rendered:document.getElementById('rendered').textContent},'*');"+'</script>';
    if(kind==='embed'){
     window.sankoreLang={en:{embed:'Embed'}};window.onresize={connect(){}};await init();
     $('textarea').first().val(markup).next().trigger('click');
    }else if(kind==='html'){
     $.ajax=opts=>opts.success('<!doctype html><html><body><!-- content --></body></html>');
     $('#editor').val(markup).markItUp({previewAutoRefresh:false,markupSet:[{name:'Preview',call:'preview'}]});$('.markItUpButton').trigger('mouseup');
    }else $('#data').append($.fn.oembed.getVideoCode('https://example.com/',{html:markup}));
   },kind);
   await page.waitForFunction(()=>window.results.length===1);
   const result=await page.evaluate(()=>({probe:window.results[0],hacked:document.body.dataset.hacked}));
   assert.equal(result.probe.origin,'null',kind);assert.equal(result.probe.data.parentAccess,false,kind);assert.equal(result.probe.data.nativeAccess,false,kind);
   assert.equal(result.probe.data.rendered,'Formatted content',kind);assert.equal(result.hacked,undefined,kind);
   await page.close();
  }
  console.log('PASS: AnyEmbed, HTML preview and oEmbed render content but block parent DOM and native bridge access');
 }finally{await browser.close()}
}
main().catch(e=>{console.error(e);process.exit(1)});
