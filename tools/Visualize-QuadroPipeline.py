# -*- coding: utf-8 -*-
"""Build an offline diagnostic viewer from DOM3D_PIPELINE_DUMP JSON snapshots.

Usage: python tools/Visualize-QuadroPipeline.py DUMP_DIRECTORY OUTPUT.html
No dependencies, network resources, or production mesh edits.
"""
import argparse
import json
import math
from pathlib import Path


def enrich(surface):
    endpoints = [(i, k, p) for i, edge in enumerate(surface.get('prepared', []))
                 if edge and math.dist(edge[0], edge[-1]) > surface['joinTolerance']
                 for k, p in enumerate((edge[0], edge[-1]))]
    gaps = []
    seen = set()
    for i, k, p in endpoints:
        candidates = [(math.dist(p, q), j, l, q) for j, l, q in endpoints if j != i]
        if not candidates:
            continue
        distance, j, l, q = min(candidates)
        pair = tuple(sorted(((i, k), (j, l))))
        if pair not in seen:
            seen.add(pair)
            gaps.append(dict(edge=i, end=k, other=j, otherEnd=l, a=p, b=q,
                             distance=distance,
                             ratio=distance / surface['joinTolerance']))
    surface['endpointGaps'] = gaps
    vertices = surface.get('vertices', [])
    bad = []
    for i, cell in enumerate(surface.get('cells', [])):
        ps = [vertices[j] for j in cell]
        lengths = [math.dist(a, b) for a, b in zip(ps, ps[1:] + ps[:1])]
        ratio = max(lengths) / max(min(lengths), 1e-15)
        if min(lengths) < 1e-8 or ratio > 10:
            bad.append(dict(cell=i, ratio=ratio, minEdge=min(lengths)))
    surface['suspectCells'] = bad
    return surface


HTML = r'''<!doctype html><html lang="ru"><meta charset="utf-8">
<title>Диагностика Quadro pipeline</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#111922;color:#e5edf5;font:14px system-ui}header{padding:18px 24px;border-bottom:1px solid #344556}h1{font-size:22px;margin:0 0 8px}select,button{background:#253647;color:inherit;border:1px solid #617385;padding:7px;border-radius:5px}main{display:grid;grid-template-columns: minmax(600px,1fr) 390px}aside{padding:16px;max-height:85vh;overflow:auto}canvas{display:block;width:100%;height:67vh;background:#0c131c}#uv{height:240px}#controls{padding:12px;display:flex;gap:12px;flex-wrap:wrap}.note{color:#a9bdcc;font-size:12px}.bad{color:#ff7979}table{border-collapse:collapse;width:100%;font-size:12px}td,th{padding:6px;border-bottom:1px solid #33414e;text-align:left}pre{white-space:pre-wrap}label{white-space:nowrap}h3{margin-bottom:8px}#status{padding:12px 24px;color:#ffc16c}code{color:#ffc16c}a{color:#86cfff}
</style><header><h1>Quadro: границы → UV → patches → клетки</h1>
<select id="body"></select> <select id="face"></select>
<button id="fit">Вписать</button> <button id="focus">Увеличить разрыв</button>
<span class="note">Мышь: вращение · колесо: масштаб. Номера граней с нуля.</span></header>
<div id="status"></div><main><section><canvas id="world"></canvas><div id="controls">
<label><input id="original" type="checkbox">CAD: опорная триангуляция</label>
<label><input id="mesh" type="checkbox" checked>Квадро-клетки</label>
<label><input id="cad" type="checkbox">CAD wires</label>
<label><input id="initial" type="checkbox">Рёбра до синхронизации*</label>
<label><input id="prepared" type="checkbox" checked>Подготовленные рёбра</label>
<label><input id="special" type="checkbox" checked>Особые узлы / разрывы</label>
<label><input id="directions" type="checkbox">Направления ∂S/∂u, ∂S/∂v</label>
<label><input id="outside" type="checkbox" checked>Клетки вне CAD</label>
<label><input id="pcurves" type="checkbox">CAD pcurves в UV</label>
<label><input id="badcells" type="checkbox" checked>Клетки с отношением рёбер &gt;10</label>
</div><p class="note" style="padding:0 16px">*Независимый повтор PrepareEdges на отдельной CSurfaceFace. Опорная триангуляция строится на копии CAD, если исходный кэш пуст. Данные сетки — после ReBuldMesh. Оранжевые стрелки: порядок узлов границы. Голубой/зелёный: параметрические направления, не поле фронта.</p></section><aside><div id="details"></div><h3>UV-проекция соединённых цепочек</h3><canvas id="uv"></canvas><p class="note">Красный отрезок — незамкнутость. Он увеличивается кнопкой «Увеличить разрыв». Оси U/V и направления цепочки показаны независимо от допуска контура.</p><h3>Ошибка XYZ → UV → XYZ по узлам</h3><canvas id="residual" style="height:130px"></canvas><div id="metrics"></div></aside></main>
<script>const DATA=__DATA__;
const el=id=>document.getElementById(id), checked=id=>el(id).checked;
const palette=['#6fc6fa','#ca9cff','#62dec4','#e9c66b','#f89595','#9ace70'];
let current=0, selected=-1, yaw=.7, pitch=.75, scale=1, center=[0,0,0], base=1, focusGap=false;
const add=(a,b)=>a.map((x,i)=>x+b[i]),sub=(a,b)=>a.map((x,i)=>x-b[i]),mul=(a,s)=>a.map(x=>x*s),norm=a=>Math.hypot(...a);
const faces=()=>DATA[current].after.surfaces;
const shown=()=>faces().filter(s=>selected<0||s.id===selected);
const esc=s=>String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;');
function options(){el('face').innerHTML='<option value="-1">Все грани</option>'+faces().map(s=>`<option value="${s.id}">${s.error||(s.outsideCells||[]).length?'⚠ ':''}Face ${s.id} · ${s.geometry===0?'plane':(['plane','cylinder','cone','sphere','torus','bezier','bspline'][s.geometry]||'geom '+s.geometry)}</option>`).join('');el('face').value=selected;}
function resize(canvas){const r=canvas.getBoundingClientRect();canvas.width=Math.round(r.width*devicePixelRatio);canvas.height=Math.round(r.height*devicePixelRatio);const ctx=canvas.getContext('2d');ctx.setTransform(devicePixelRatio,0,0,devicePixelRatio,0,0);return [ctx,r.width,r.height];}
function fit(){focusGap=false;let ps=shown().flatMap(s=>(s.prepared||[]).flat());if(!ps.length)ps=shown().flatMap(s=>s.vertices||[]);if(!ps.length)return;const lo=[0,1,2].map(i=>Math.min(...ps.map(p=>p[i]))),hi=[0,1,2].map(i=>Math.max(...ps.map(p=>p[i])));center=mul(add(lo,hi),.5);base=Math.max(...sub(hi,lo),1);scale=1;draw();}
function project(p,w,h){const [x,y,z]=sub(p,center),u=x*Math.cos(yaw)-y*Math.sin(yaw),v=x*Math.sin(yaw)+y*Math.cos(yaw);const k=Math.min(w,h)*.78/base*scale;return [w/2+u*k,h/2-(z*Math.cos(pitch)-v*Math.sin(pitch))*k];}
function path(ctx,ps,color,width=1,close=false){if(ps.length<2)return;ctx.beginPath();ps.forEach((p,i)=>i?ctx.lineTo(...p):ctx.moveTo(...p));if(close)ctx.closePath();ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();}
function dot(ctx,p,color,r=4){ctx.fillStyle=color;ctx.beginPath();ctx.arc(...p,r,0,Math.PI*2);ctx.fill();}
function arrow(ctx,a,b,color){path(ctx,[a,b],color,1.5);const t=Math.atan2(b[1]-a[1],b[0]-a[0]);path(ctx,[[b[0]-6*Math.cos(t-.5),b[1]-6*Math.sin(t-.5)],b,[b[0]-6*Math.cos(t+.5),b[1]-6*Math.sin(t+.5)]],color,1.5);}
function draw(){const [ctx,w,h]=resize(el('world'));ctx.clearRect(0,0,w,h);const p=x=>project(x,w,h);
for(const s of shown()){
 const color=palette[s.id%palette.length];
 if(checked('outside'))for(const index of s.outsideCells||[])path(ctx,s.cells[index].map(i=>p(s.vertices[i])),'#ff5d35',2,true);
 if(checked('original')){const before=DATA[current].before.surfaces.find(f=>f.id===s.id);if(before)for(const c of before.cells||[])path(ctx,c.map(i=>p(before.vertices[i])), '#526577',.45,true);}
 if(checked('mesh'))for(const c of s.cells||[])path(ctx,c.map(i=>p(s.vertices[i])),color,.6,true);
 if(checked('badcells'))for(const b of s.suspectCells)path(ctx,s.cells[b.cell].map(i=>p(s.vertices[i])),'#ff5b86',2,true);
 if(checked('cad'))for(const wire of s.cadWires)for(const e of wire)path(ctx,e.map(p),'#fafafa',1.5);
 if(checked('initial'))for(const e of s.isolatedPreparedReplay||[])path(ctx,e.map(p),'#6fff99',2);
 if(checked('prepared'))for(const [i,e]of(s.prepared||[]).entries()){path(ctx,e.map(p),palette[i%palette.length],2);if(e.length>2){let m=Math.floor(e.length/2);arrow(ctx,p(e[m-1]),p(e[m]),'#ffa84c');}}
 if(checked('directions'))for(const l of s.joinedReplay||[])for(const d of l.directions||[])for(const [key,c] of [['du','#51caff'],['dv','#6fff99']]){const v=d[key],n=norm(v);if(n>1e-14)arrow(ctx,p(d.origin),p(add(d.origin,mul(v,base*.045/scale/n))),c);else dot(ctx,p(d.origin),'#ff3333',7);}
 if(checked('special'))for(const g of s.endpointGaps){if(g.ratio>1){path(ctx,[p(g.a),p(g.b)],'#ff5252',4);dot(ctx,p(g.a),'#ff5252',6);dot(ctx,p(g.b),'#ffe76e',4);ctx.fillStyle='#fff';ctx.fillText(`F${s.id} gap=${g.distance.toPrecision(4)}`,p(g.a)[0]+10,p(g.a)[1]-10);}else if(selected>=0)dot(ctx,p(g.a),color,2);}
}drawUV();drawResidual();}
function drawUV(){const[ctx,w,h]=resize(el('uv'));const s=shown()[0];let lines=(s?.joinedReplay||[]).map(l=>l.uv.filter(Boolean));if(checked('pcurves'))lines.push(...(s?.topology||[]).map(w=>w.flatMap(e=>e.pcurve||[])));if(checked('initial'))lines.push(...(s?.isolatedPreparedUV||[]).map(l=>l.filter(Boolean)));let ps=lines.flat();if(!ps.length){ctx.fillStyle='#a9bdcc';ctx.fillText('Нет UV-данных',10,30);return;}let lo=[0,1].map(i=>Math.min(...ps.map(p=>p[i]))),hi=[0,1].map(i=>Math.max(...ps.map(p=>p[i])));if(focusGap){const l=(s.joinedReplay||[]).find(l=>!l.closed);if(l){const a=l.uv[0],b=l.uv.at(-1),d=Math.max(Math.hypot(a[0]-b[0],a[1]-b[1]),1e-6);lo=a.map((v,i)=>(v+b[i])/2-d*2);hi=lo.map(v=>v+d*4);}}
const k=Math.min((w-30)/Math.max(hi[0]-lo[0],1e-10),(h-30)/Math.max(hi[1]-lo[1],1e-10));const p=a=>[15+(a[0]-lo[0])*k,h-15-(a[1]-lo[1])*k];lines.forEach((l,i)=>{path(ctx,l.map(p),palette[i%palette.length],1.5);if(i<(s.joinedReplay||[]).length && !s.joinedReplay[i].closed){path(ctx,[p(l[0]),p(l.at(-1))],'#ff5252',3);dot(ctx,p(l[0]),'#ff5252');dot(ctx,p(l.at(-1)),'#ffe76e');}});ctx.fillStyle='#cbd7e2';ctx.fillText('U →   V ↑',10,14);}
function drawResidual(){const[ctx,w,h]=resize(el('residual'));const s=shown()[0];const values=(s?.joinedReplay||[]).flatMap(l=>l.projectionResidual||[]);ctx.fillStyle='#a9bdcc';ctx.fillText('log10(residual), XYZ units',8,14);const ps=values.map((v,i)=>v===null?null:[25+i*(w-40)/Math.max(1,values.length-1),h-15-(Math.max(-12,Math.min(0,Math.log10(Math.max(v,1e-12))))+12)*(h-40)/12]);path(ctx,ps.filter(Boolean),'#73d7e5',1.5);for(const y of [-12,-6,0]){ctx.fillText(String(y),0,h-15-(y+12)*(h-40)/12);}if(values.length)ctx.fillText('max='+Math.max(...values.filter(v=>v!==null)).toPrecision(4),w-140,14);}
function details(){const ss=shown(),bad=ss.filter(s=>s.error);el('status').textContent=bad.length?`${bad.length} грань(и) с ошибкой. Красные точки: разрывы границ. Оранжевые клетки: центр вне CAD-face.`:'У выбранных граней нет сохранённой ошибки заполнения.';
el('details').innerHTML=ss.map(s=>`<h3>Face ${s.id} · ${s.cadValid?'CAD valid':'CAD invalid'}</h3><div class="${s.error?'bad':'note'}">${esc(s.error||'Построена сетка')}</div><p>${s.isolatedFillSucceeded===true?'Контроль: исходные CAD-рёбра заполняются ('+s.isolatedFillCellCount+' клеток).':''}</p><p>${(s.outsideCells||[]).length} клеток с центром вне CAD-face · ${(s.topology||[]).length} CAD wires</p><p>${(s.cells||[]).length} клеток · ${s.suspectCells.length} с коротким ребром / отношением &gt;10</p><p class="note">${esc(s.iterationStatus)}. История фронта успешных граней не записана; число итераций не подменяется нулём.</p>`).join('');
el('metrics').innerHTML=ss.map(s=>`<h3>Стыки F${s.id}</h3><div class="note">δ=${s.joinTolerance.toPrecision(6)}. Красный: gap/δ &gt; 1.</div><table><tr><th>Рёбра</th><th>gap</th><th>gap/δ</th></tr>${s.endpointGaps.map(g=>`<tr class="${g.ratio>1?'bad':''}"><td>${g.edge} ↔ ${g.other}</td><td>${g.distance.toPrecision(5)}</td><td>${g.ratio.toFixed(2)}</td></tr>`).join('')}</table><p class="note">Join replay: ${(s.joinedReplay||[]).length} цепочек, ${(s.joinedReplay||[]).filter(l=>l.closed).length} замкнутых. Ошибки проекции: ${(s.joinedReplay||[]).reduce((a,l)=>a+l.projectionFailedNodes.length,0)}. Входных patches: ${(s.quadrangulatorInput||[]).length} (кэш не охватывает все ветки).</p>`).join('');}
el('body').innerHTML=DATA.map((d,i)=>`<option value="${i}">${esc(d.after.solid)}</option>`).join('');
el('body').onchange=()=>{current=+el('body').value;selected=-1;options();details();fit();};el('face').onchange=()=>{selected=+el('face').value;details();fit();};
el('fit').onclick=fit;el('focus').onclick=()=>{const s=shown().find(s=>s.endpointGaps.some(g=>g.ratio>1));if(!s)return;selected=s.id;el('face').value=selected;const g=s.endpointGaps.reduce((a,b)=>a.ratio>b.ratio?a:b);center=mul(add(g.a,g.b),.5);base=Math.max(g.distance*7,1e-4);scale=1;focusGap=true;details();draw();};
el('controls').onchange=draw;let drag=null;el('world').onpointerdown=e=>{drag=[e.clientX,e.clientY];el('world').setPointerCapture(e.pointerId);};el('world').onpointermove=e=>{if(!drag)return;yaw+=(e.clientX-drag[0])*.008;pitch+=(e.clientY-drag[1])*.008;drag=[e.clientX,e.clientY];draw();};el('world').onpointerup=()=>drag=null;el('world').onwheel=e=>{e.preventDefault();scale*=Math.exp(-e.deltaY*.001);draw();};window.onresize=draw;const requested=new URLSearchParams(location.search).get('face');if(requested!==null&&faces().some(s=>s.id===+requested))selected=+requested;options();details();fit();
</script></html>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data = []
    for path in sorted(args.directory.glob('*-after.json')):
        after = json.loads(path.read_text(encoding='utf-8'))
        before = json.loads(path.with_name(path.name.replace('-after.json', '-before.json')).read_text(encoding='utf-8'))
        after['surfaces'] = [enrich(s) for s in after['surfaces']]
        data.append(dict(before=before, after=after))
    if not data:
        parser.error('No before/after snapshot pairs found')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(HTML.replace('__DATA__', json.dumps(data, ensure_ascii=False).replace('</', '<\\/')), encoding='utf-8')
    print(args.output.resolve())


if __name__ == '__main__':
    main()
