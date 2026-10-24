// Exercise real widget functions with HTML-shaped text and imported settings.
// jsdom checks DOM structure; browser sandbox isolation is checked separately.
const {JSDOM, VirtualConsole}=require('jsdom');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
const payload='</textarea><img data-injected="yes" src=x onerror="parent.hacked=1"> & "quotes"';
const jquery='resources/library/interactivities/Order Sentence.wgt/scripts/jquery-1.6.2.min.js';
function environment(source,html='<div id="mp_word"></div><div id="data"></div>',jq=jquery){
 const dom=new JSDOM(html,{runScripts:'dangerously',url:'https://widget.test/',virtualConsole:new VirtualConsole(),pretendToBeVisual:true});
 const w=dom.window;w.eval(read(jq));
 for(const plugin of ['sortable','draggable','droppable','disableTextSelect'])w.$.fn[plugin]=function(){return this};
 w.eval(read(source));return dom;
}
function noInjection(w){assert.equal(w.document.querySelectorAll('[data-injected]').length,0);assert.equal(w.hacked,undefined)}
function files(dir,name){return fs.readdirSync(path.join(root,dir),{recursive:true}).filter(n=>n.endsWith(name)).map(n=>dir+'/'+n)}
async function main(){
 let checks=0;
 for(const [widget,variable] of [['Order Words','word'],['Order Sentence','sentence']]){
  for(const source of files('resources/library/interactivities/'+widget+'.wgt','template2.js')){
   const dom=environment(source),w=dom.window;
   w[variable]=payload;w.modeEdit();
   assert.equal(w.$('#mp_word textarea').val(),payload,source);noInjection(w);
   w.modeView();noInjection(w);w.modeEdit();assert.equal(w.$('#mp_word textarea').val(),payload,source);
   dom.window.close();checks++;
  }
 }
 for(const source of files('resources/library/interactivities/Constrast.wgt','blackYellow.js')){
  const dom=environment(source),w=dom.window;w.addTask(payload,'x < 3 & y > 2');
  assert.equal(w.$('.taskContainer').eq(0).text(),payload);assert.equal(w.$('.taskContainer').eq(1).text(),'x < 3 & y > 2');noInjection(w);dom.window.close();checks++;
 }
 for(const source of files('resources/library/interactivities/Cat text.wgt','script.js')){
  const dom=environment(source,'<div id="data"><div id="category"><input name="mask"><button id="add"></button></div></div>'),w=dom.window;
  w.$('#category input').val(payload);w.addText(w.$('#category'),w.$('#add'));
  assert.equal(w.$('.img_block input[type=hidden]').val(),payload,source);noInjection(w);dom.window.close();checks++;
 }
 for(const source of files('resources/library/interactivities/Cat pict.wgt','script.js')){
  const dom=environment(source,'<div id="data"><div id="category"><input name="mask"></div></div>'),w=dom.window;
  w.setInterval=()=>0;w.$('#category input').val(payload);
  w.onDropTarget(w.document.getElementById('category'),{dataTransfer:{getData:()=>'<drop><ready>true</ready><path>picture.png</path></drop>'}});
  assert.equal(w.$('.img_block input[type=hidden]').val(),payload,source);noInjection(w);dom.window.close();checks++;
 }
 for(const source of files('resources/library/interactivities/Order Letters.wgt','script.js')){
  const dom=environment(source,'<button id="wgt_display"></button><button id="wgt_edit"></button><div id="data"></div>'),w=dom.window;
  await w.start();w.$('#data').empty().append('<div class="cont"><div class="sub_cont"></div><ul></ul><input type="hidden"></div>');
  w.$('.cont input').val(payload);w.$('#wgt_edit').removeClass('selected').trigger('click');
  assert.equal(w.$('.audio_answer').text(),payload,source);noInjection(w);
  w.$('#wgt_display').removeClass('selected').trigger('click');noInjection(w);
  assert.equal(w.$('.cont input').val(),payload,source);dom.window.close();checks++;
 }
 for(const source of files('resources/library/interactivities/Order Pictures.wgt','script.js')){
  const dom=environment(source,'<button id="wgt_display"></button><button id="wgt_edit"></button><div id="data"></div>'),w=dom.window;
  await w.start();w.$('#data').empty().append('<div class="cont"><div class="imgs_cont"><div class="img_block"><input></div></div></div>');
  w.$('.img_block input').val(payload);w.$('#wgt_edit').removeClass('selected').trigger('click');
  assert.equal(w.$('.numb_img').text(),payload,source);noInjection(w);dom.window.close();checks++;
 }
 {
  const dom=environment('resources/library/applications/QR-Code.wgt/js/main.js','<button id="ok"></button><input id="qrtext"><div id="codediv"></div><div id="display"></div>'),w=dom.window;
  w.$.fn.qrcode=function(){return this};await new Promise(resolve=>w.$(resolve));
  w.$('#qrtext').val(payload);w.$('#ok').trigger('click');assert.equal(w.$('#display').text(),payload);noInjection(w);dom.window.close();checks++;
 }
 {
  const dom=environment('resources/library/applications/Notes.wgt/js/jquery.ubwidget.js','<div id="button"><div class="ubw-button-body"><div class="ubw-button-content"></div></div></div>'),w=dom.window;
  let selected;w.$('#button').ubwdropdown({},[payload],value=>selected=value);w.$('.ubw-dropdown li').trigger('click');
  assert.equal(selected,payload);assert.equal(w.$('.ubw-button-content').text(),payload);noInjection(w);dom.window.close();checks++;
 }
 {
  const source='resources/library/applications/ColorPicker.wgt/js/jquery.js';
  const dom=environment(source),w=dom.window;
  w.eval('jQuery.extend(true, {}, JSON.parse(\'{"__proto__":{"polluted":true},"constructor":{"prototype":{"polluted":true}}}\'))');
  assert.equal(w.eval('({}).polluted'),undefined);
  const result=w.$.extend(true,{}, {nested:{value:'kept'},values:[1,2]});
  assert.equal(result.nested.value,'kept');assert.equal(result.values.length,2);
  // The selector form of .load must strip mixed-case scripts using a parser.
  w.$.ajax=opts=>opts.complete({responseText:'<section><SCRIPT>window.hacked=1</SCRIPT><p>kept</p></section>'},'success');
  w.$('#data').load('/example section');assert.equal(w.$('#data script').length,0);assert.equal(w.$('#data p').text(),'kept');noInjection(w);
  dom.window.close();checks++;
 }
 {
  const dom=environment('resources/library/search/Pixabay.wgs/select/jquery.selectBox.js','<select id="choice"></select>'),w=dom.window;
  w.$('#choice').selectBox();const data={};data[payload]=payload;data.group={};data.group[payload]=payload;
  w.$('#choice').selectBox('options',data);
  assert.equal(w.$('#choice option').length,2);assert.equal(w.$('#choice option').eq(0).val(),payload);assert.equal(w.$('#choice option').eq(0).text(),payload);noInjection(w);dom.window.close();checks++;
 }
 {
  const dom=environment('resources/library/applications/Papier.wgt/spectrum-1.8.1/spectrum.js','<input id="color">','resources/library/applications/Papier.wgt/jquery/jquery-2.2.4.min.js'),w=dom.window;
  w.$('#color').spectrum({flat:true,showPalette:true,showInitial:true,allowEmpty:true,palette:[['#ff0000',null]],noColorSelectedText:payload});
  assert.ok(w.document.querySelector('.sp-palette-row'));assert.equal(w.$('.sp-clear-display').attr('title'),payload);
  w.$('#color').spectrum('set','#008000');assert.equal(w.$('#color').spectrum('get').toHexString(),'#008000');noInjection(w);dom.window.close();checks++;
 }
 {
  const dom=environment('resources/library/applications/AnyEmbed.wgt/js/ubw-main.js','<div id="ubwidget"></div>'),w=dom.window;
  w.sankore={async:{preference:async()=>''},setPreference(){}};w.sankoreLang={en:{embed:'Embed'}};w.onresize={connect(){}};
  await w.init();w.$('textarea').val(payload);w.$('textarea').next().trigger('click');
  const frame=w.document.querySelector('iframe');assert.ok(frame);assert.equal(frame.srcdoc,payload);assert.equal(frame.getAttribute('sandbox'),'allow-scripts allow-forms');noInjection(w);dom.window.close();checks++;
 }
 {
  const dom=environment('resources/library/applications/Html.wgt/markitup/jquery.markitup.js','<textarea id="editor"></textarea>'),w=dom.window;
  w.$.ajax=opts=>opts.success('<!doctype html><html><body><!-- content --></body></html>');
  w.$('#editor').val(payload).markItUp({id:payload,nameSpace:payload,previewAutoRefresh:false,markupSet:[{name:'Preview',call:'preview'}]});
  w.$('.markItUpButton').trigger('mouseup');
  const frame=w.document.querySelector('iframe');assert.ok(frame);assert.ok(frame.srcdoc.includes(payload));assert.equal(frame.getAttribute('sandbox'),'allow-scripts allow-forms');noInjection(w);dom.window.close();checks++;
 }
 {
  const dom=environment('resources/library/applications/VideoPicker.wgt/scripts/jquery.oembed.js'),w=dom.window;
  const photo=w.$.fn.oembed.getPhotoCode('javascript:alert(1)',{url:'javascript:alert(1)',html:payload});w.$('#data').append(photo);
  assert.equal(w.$('#data a').attr('href'),'');assert.equal(w.$('#data img').attr('src'),'');
  const frame=w.document.querySelector('iframe');assert.ok(frame);assert.equal(frame.getAttribute('sandbox'),'allow-scripts allow-forms');assert.equal(frame.srcdoc,payload);noInjection(w);
  const generic=w.$.fn.oembed.getGenericCode('https://example.com/',{title:payload});w.$('#data').empty().append(generic);assert.equal(w.$('#data a').text(),payload);noInjection(w);dom.window.close();checks++;
 }
 console.log('PASS: '+checks+' widget, library, injection and persistence checks');
}
main().catch(e=>{console.error(e);process.exit(1)});
