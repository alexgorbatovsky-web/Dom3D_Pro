"""Show admitted logical Coons patches and their unchanged boundary instances.
Usage: python tools/Visualize-QuadroLogicalPatches.py REPORT.json OUTPUT.svg
"""
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
faces = [f for f in report["faces"] if f["strategy"] == "logical-coons" and f["ready"]]
height = 100 + 410 * ((len(faces) + 1) // 2)
out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}" viewBox="0 0 1200 {height}">',
       '<rect width="100%" height="100%" fill="#10212e"/>',
       '<g font-family="sans-serif" fill="#e8f1f5">',
       '<text x="30" y="35" font-size="24">Hairdryer: admitted logical patches (U/V axes scaled independently)</text>',
       '<text x="30" y="63" font-size="16">Yellow: logical corners at existing boundary nodes. CAD segments stay unchanged.</text>']
for k, face in enumerate(faces):
    x, y = 30 + 600 * (k % 2), 110 + 410 * (k // 2)
    uv = face["uv"]
    lo = [min(p[i] for p in uv) for i in range(2)]
    hi = [max(p[i] for p in uv) for i in range(2)]
    su, sv = 490 / (hi[0] - lo[0]), 270 / (hi[1] - lo[1])
    def point(p):
        return x + 270 + (p[0] - (hi[0] + lo[0]) / 2) * su, y + 170 - (p[1] - (hi[1] + lo[1]) / 2) * sv
    out.append(f'<text x="{x}" y="{y}" font-size="22">F{face["face"]}: {len(face["quads"])} quads</text>')
    for q in face["quads"]:
        points = " ".join(f"{point(uv[i])[0]:.3f},{point(uv[i])[1]:.3f}" for i in q)
        out.append(f'<polygon points="{points}" fill="#65b6a2" stroke="#204c4b" stroke-width="0.7"/>')
    for corner in face["logicalCorners"]:
        px, py = point(face["sourceUV"][corner])
        out.append(f'<circle cx="{px}" cy="{py}" r="5" fill="#ffd46a"/><text x="{px+8}" y="{py-8}" font-size="15">{corner}</text>')
    out.append(f'<text x="{x}" y="{y+365}" font-size="16">Boundary indices: {face["logicalCorners"]}</text>')
    out.append(f'<text x="{x}" y="{y+387}" font-size="14">Native ranges: U={hi[0]-lo[0]:.5g}, V={hi[1]-lo[1]:.5g}</text>')
out.extend(['</g>', '</svg>'])
Path(sys.argv[2]).write_text('\n'.join(out), encoding="utf-8")
