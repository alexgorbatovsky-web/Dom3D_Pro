"""Render actual admitted quads and rejected-face CAD boundaries to SVG.
Usage: python tools/Visualize-QuadroChartFill.py REPORT.json OUTPUT.svg
No production model changes; rejected faces are explicitly marked as donors.
"""
import json
import math
import sys
from collections import Counter
from pathlib import Path
from html import escape

d=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
faces=d['faces']
out=['<svg xmlns="http://www.w3.org/2000/svg" width="1350" height="900" viewBox="0 0 1350 900">',
     '<rect width="1350" height="900" fill="#111e29"/>']
def text(x,y,s,size=18,color='#e0eaf3'):
    out.append(f'<text x="{x}" y="{y}" font-family="Arial" font-size="{size}" fill="{color}">{escape(str(s))}</text>')
text(30,42,'Hairdryer · experimental CAD chart consumer',28)
text(30,76,f"{d['donors']}/85 inputs ready · {d['accepted']}/85 quad faces admitted",21)
text(30,108,'Green: admitted quads · Red outlines: rejected CAD face boundaries',17)
polygons=[];vertices=[]
for face in faces:
    points=face['xyz'] if face['ready'] else face['sourceXYZ']
    cells=face['quads'] if face['ready'] else face['triangles']
    vertices.extend(points)
    if face['ready']:
        polygons.extend(([points[i] for i in cell],True) for cell in cells)
    else:
        edges=Counter(tuple(sorted((cell[k],cell[(k+1)%len(cell)]))) for cell in cells for k in range(len(cell)))
        polygons.extend(([points[i] for i in edge],False) for edge,count in edges.items() if count==1)
center=[(min(p[k] for p in vertices)+max(p[k] for p in vertices))/2 for k in range(3)]
a,e=math.radians(35),math.radians(20)
def project(p):
    x,y,z=[p[k]-center[k] for k in range(3)]
    u=x*math.cos(a)-y*math.sin(a);w=x*math.sin(a)+y*math.cos(a)
    return (-w*math.sin(e)+z*math.cos(e),-u,w*math.cos(e)+z*math.sin(e))
projected=[([project(p) for p in poly],ready) for poly,ready in polygons]
allp=[p for poly,_ in projected for p in poly]
x0,x1=min(p[0] for p in allp),max(p[0] for p in allp)
y0,y1=min(p[1] for p in allp),max(p[1] for p in allp)
scale=min(780/(x1-x0),665/(y1-y0))
for poly,ready in sorted(projected,key=lambda item:sum(p[2] for p in item[0])/len(item[0])):
    coords=' '.join(f'{425+(p[0]-(x0+x1)/2)*scale:.2f},{493-(p[1]-(y0+y1)/2)*scale:.2f}' for p in poly)
    out.append(f'<{"polygon" if ready else "polyline"} points="{coords}" fill="{"#66b9a6" if ready else "none"}" stroke="{"#244d49" if ready else "#ff8f82"}" stroke-width="{0.45 if ready else 1.2}"/>')
for j,faceid in enumerate([81,82]):
    face=next(f for f in faces if f['face']==faceid)
    x,y,w,h=870,178+j*350,440,250
    text(x,y-28,f"F{faceid} · {len(face['quads'])} admitted quads",20,'#7de0c5')
    out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="#1a3040"/>')
    uv=face['uv'];u0,u1=min(p[0] for p in uv),max(p[0] for p in uv);v0,v1=min(p[1] for p in uv),max(p[1] for p in uv)
    s=min((w-24)/(u1-u0),(h-24)/(v1-v0))
    for q in face['quads']:
        pts=' '.join(f'{x+w/2+(uv[i][0]-(u0+u1)/2)*s:.2f},{y+h/2-(uv[i][1]-(v0+v1)/2)*s:.2f}' for i in q)
        out.append(f'<polygon points="{pts}" fill="#66b9a6" stroke="#244d49" stroke-width="0.65"/>')
    text(x,y+h+24,'UV chart · hole remains empty' if faceid==81 else 'UV chart · separate periodic seam instances',15)
text(30,866,'85/85 faces · closed body validated · OBJ available · experimental path, UI not connected' if d.get('bodyClosed') else 'Diagnostic composite; whole-body validation not recorded.',18,'#f3cb8a')
out.append('</svg>')
Path(sys.argv[2]).write_text('\n'.join(out),encoding='utf-8')
