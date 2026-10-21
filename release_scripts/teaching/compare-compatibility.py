"""Compare semantic scene manifests and rendering; no lesson files are rewritten."""
import json,sys,hashlib,zipfile,math
from pathlib import Path
from PIL import Image,ImageChops
root=Path(sys.argv[1]);checks=[]
def read(name):return json.loads((root/name).read_text(encoding='utf-8-sig'))
def compare(a,b,label):
    assert a['pageCount']==b['pageCount']==3,(label,'page count')
    for index,(x,y) in enumerate(zip(a['pages'],b['pages'])):
        for key in ['pdfPage','pdfSha256','strokeGroups','polygons']:
            assert x.get(key)==y.get(key),(label,index,key,x.get(key),y.get(key))
        def normalized_ink(items):
            return sorted(items,key=lambda v:(v['color'],round(min(p[0] for p in v['points']),2),round(min(p[1] for p in v['points']),2),len(v['points'])))
        xi=normalized_ink(x['ink']);yi=normalized_ink(y['ink'])
        assert len(xi)==len(yi),(label,index,'ink polygon count')
        for left,right in zip(xi,yi):
            assert left['color']==right['color'],(label,index,'ink colour/opacity')
            assert len(left['points'])==len(right['points']),(label,index,'ink polygon vertices')
            for lp,rp in zip(left['points'],right['points']):
                assert all(math.isclose(a,b,abs_tol=.02) for a,b in zip(lp,rp)),(label,index,'ink geometry',lp,rp)
        assert sorted(x['texts'])==sorted(y['texts']),(label,index,'text')
        xs={v['uuid']:v for v in x['svg']};ys={v['uuid']:v for v in y['svg']}
        assert xs.keys()==ys.keys(),(label,index,'SVG object identifiers')
        for uuid in xs:
            assert xs[uuid]['sha256']==ys[uuid]['sha256'],(label,index,'SVG bytes',uuid)
            for key in ['dx','dy','m11','m12','m21','m22']:
                assert math.isclose(xs[uuid][key],ys[uuid][key],abs_tol=.02),(label,index,key,xs[uuid][key],ys[uuid][key])
    checks.append(label+': page count, PDF bytes/page indexes, text, handwriting/dashed-line geometry and colours, SVG bytes and transformations preserved')
compare(read('original-create-saved.json'),read('lessonboard-edit-import.json'),'OpenBoard to LessonBoard')
compare(read('lessonboard-edit-saved.json'),read('original-edit-import.json'),'LessonBoard to OpenBoard')
compare(read('original-edit-saved.json'),read('lessonboard-reopen-import.json'),'OpenBoard resave to LessonBoard')
compare(read('original-edit-saved.json'),read('lessonboard-reopen-saved.json'),'LessonBoard reopen and save')
for name in ['original.ubz','lessonboard.ubz','original-resaved.ubz','lessonboard-reopened.ubz']:
    with zipfile.ZipFile(root/name) as z:
        assert z.testzip() is None,(name,'ZIP CRC')
        svgs=[z.read(n) for n in z.namelist() if n.endswith('.svg')]
        if name!='original.ubz':
            assert sum(b'id="teaching-shape"' in v for v in svgs)==4,(name,'shape metadata')
            assert sum(b'id="graphdesk-project"' in v for v in svgs)==1,(name,'graph metadata')
        assert any(n.endswith('.pdf') for n in z.namelist()),(name,'original PDF')
checks.append('All four UBZ archives pass CRC validation and retain their PDF assets; new SVG metadata survives')
visual=[]
for left,right,label in [('original-create-saved','lessonboard-edit-import','Original lesson'),('lessonboard-edit-saved','original-edit-import','LessonBoard lesson'),('original-edit-saved','lessonboard-reopen-import','Resaved lesson')]:
    for page in range(1,4):
        a=Image.open(root/f'{left}-page-{page}.png').convert('RGB');b=Image.open(root/f'{right}-page-{page}.png').convert('RGB')
        diff=ImageChops.difference(a,b); pixels=list(diff.getdata()); changed=sum(max(p)>12 for p in pixels)/len(pixels)
        visual.append({'comparison':label,'page':page,'changed_pixel_fraction':changed})
        # Different highlighter compositing is reported rather than mistaken for lost data.
        if changed:diff.save(root/f'difference-{left}-{page}.png')
        assert changed<.02,(label,page,'Rendering differs by over 2%',changed)
checks.append('All nine cross-application page renders differ in fewer than 2% of pixels; exact fractions recorded')
report={'passed':True,'original_release':'v1.7.7','original_test_instrumentation':'Test entry point, executable/profile identity, and test source only. Original file readers/writers and graphics code are unchanged.','checks':checks,'visual':visual}
(root/'compatibility-summary.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
