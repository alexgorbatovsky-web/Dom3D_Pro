"""Render an auditable SVG comparison of the hairdryer F47 boundary reports.

Usage: python tools/Visualize-QuadroBoundary.py STRICT.json RECONCILED.json OUT.svg
The two reports must use matching master sample counts; the regression test
exports such pairs through DOM3D_BOUNDARY_TEST_REPORT.
"""
import json
import sys
from pathlib import Path
from html import escape

before, after = [json.loads(Path(p).read_text(encoding='utf-8')) for p in sys.argv[1:3]]
svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="850" viewBox="0 0 1200 850">',
       '<rect width="1200" height="850" fill="#111c28"/>']
def text(x,y,value,size=18,color='#dde8f2'):
    svg.append(f'<text x="{x}" y="{y}" font-family="Arial" font-size="{size}" fill="{color}">{escape(str(value))}</text>')
def line(x1,y1,x2,y2,color,width=1):
    svg.append(f'<path d="M{x1},{y1} L{x2},{y2}" fill="none" stroke="{color}" stroke-width="{width}"/>')
text(35,40,'Hairdryer · CAD boundary preparation',27)
text(35,72,f"Charts: {sum(c['ready'] for c in before['charts'])}/{len(before['charts'])} → {sum(c['ready'] for c in after['charts'])}/{len(after['charts'])}")
text(35,99,'F47 · UV close-up · each white point keeps its CAD vertex identity',16)
for panel, report, title, color in [(0,before,'Before reconciliation','#ff776e'),(1,after,'After reconciliation + shared refinement','#51d4c8')]:
    ox=35+panel*600; oy=155; w=530; h=310
    svg.append(f'<defs><clipPath id="panel{panel}"><rect x="{ox}" y="{oy}" width="{w}" height="{h}"/></clipPath></defs>')
    svg.append(f'<rect x="{ox}" y="{oy}" width="{w}" height="{h}" fill="#172b3d"/>')
    text(ox,135,title,19,color)
    # UV axes are displayed with separate scales to expose the thin strip.
    project=lambda uv:(ox+(uv[0]+.034)/.011*w,oy+h-(uv[1]+.0002)/.0042*h)
    svg.append(f'<g clip-path="url(#panel{panel})">')
    for loop in report['charts'][47]['loops']:
        vertices=loop['vertices'];pts=[project(v['uv']) for v in vertices]
        path=' '.join(f'{x:.3f},{y:.3f}' for x,y in pts)
        svg.append(f'<polygon points="{path}" fill="none" stroke="{color}" stroke-width="2"/>')
        for v,(x,y) in zip(vertices,pts):
            svg.append(f'<circle cx="{x}" cy="{y}" r="{4 if v["sample"]==0 else 2}" fill="{"#ffffff" if v["sample"]==0 else color}"/>')
    svg.append('</g>')
    text(ox,490,'U: −0.034 … −0.023    V: −0.0002 … 0.0040',14)
ratios=[]
for old,new in zip(before['views'],after['views']):
    assert len(old['samples'])==len(new['samples']), 'Use reports with equal master counts'
    for a,b in zip(old['samples'],new['samples']):
        assert a['node']==b['node']
        if a['residual']>a['budget']:ratios.append((a['residual']/a['budget'],b['residual']/b['budget']))
text(35,540,f'{len(ratios)} initially rejected samples · residual / unchanged CAD budget',20)
for ratio in [0.9,1,1.1,1.2]:
    y=735-(ratio-.9)/.35*150
    line(50,y,1150,y,'#f0c96c' if ratio==1 else '#2f4253')
    text(12,y+5,f'{ratio:.1f}',12)
for i,(a,b) in enumerate(ratios):
    x=60+i*1080/max(len(ratios)-1,1)
    for ratio,color in [(a,'#ff776e'),(b,'#51d4c8')]:
        y=735-(ratio-.9)/.35*150
        svg.append(f'<circle cx="{x}" cy="{y}" r="3" fill="{color}"/>')
text(35,785,f"Maximum master movement: {max(n['displacement'] for n in after['nodes']):.9f} model units · CAD vertices fixed",16)
text(35,817,'Boundary validation only — this is not yet the hairdryer quad mesh.',16,'#f0c96c')
svg.append('</svg>')
Path(sys.argv[3]).write_text('\n'.join(svg),encoding='utf-8')
