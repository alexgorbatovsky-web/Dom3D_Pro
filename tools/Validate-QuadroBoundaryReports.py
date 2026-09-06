"""Validate the pillow/hairdryer diagnostic exports (no CAD or mesh mutation).

Usage: python tools/Validate-QuadroBoundaryReports.py output/boundary-stage2
"""
import json
import sys
from pathlib import Path


def contains(loop, point):
    x, y = point
    points = [v['uv'] for v in loop['vertices']]
    inside = False
    for (ax, ay), (bx, by) in zip(points, points[1:] + points[:1]):
        if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
            inside = not inside
    return inside


def validate(data):
    assert data['topologyValid']
    assert not data['meshingConnected']
    for wire in data['wires']:
        ids = wire['occurrences']
        for a, b in zip(ids, ids[1:] + ids[:1]):
            assert data['occurrences'][a]['lastVertex'] == data['occurrences'][b]['firstVertex']
            assert data['views'][a]['samples'][-1]['node'] == data['views'][b]['samples'][0]['node']
    for occurrence, view in zip(data['occurrences'], data['views']):
        assert view['occurrence'] == occurrence['id']
        edge = data['edges'][occurrence['edge']]
        if not edge['degenerate']:
            expected = data['masters'][edge['id']]['nodes']
            if occurrence['reversed']:
                expected = list(reversed(expected))
            assert [s['node'] for s in view['samples']] == expected
            params = [s['parameter'] for s in view['samples']]
            assert all((a > b if occurrence['reversed'] else a < b) for a, b in zip(params, params[1:]))
    for chart in data['charts']:
        if not chart['ready']:
            assert chart['issues']
            continue
        assert not chart['issues']
        assert sum(loop['outer'] for loop in chart['loops']) == 1
        for loop in chart['loops']:
            assert (loop['signedArea'] > 0) == loop['outer']
            assert loop['outer'] == data['wires'][loop['wire']]['outer']
            for v in loop['vertices']:
                assert v['node'] == data['views'][v['occurrence']]['samples'][v['sample']]['node']


root = Path(sys.argv[1])
pillow = json.loads((root / 'pillow.json').read_text(encoding='utf-8'))
hairdryer = json.loads((root / 'hairdryer.json').read_text(encoding='utf-8'))
for data in (pillow, hairdryer):
    validate(data)
assert len(pillow['faces']) == 18 and all(c['ready'] for c in pillow['charts'])
assert len(hairdryer['faces']) == 85
assert sum(o['seam'] for o in hairdryer['occurrences']) == 8
for face in (15, 25, 30, 31, 37, 50, 51, 52, 82, 83, 84):
    assert hairdryer['charts'][face]['ready'], face
chart = hairdryer['charts'][15]
assert len(chart['loops']) == 2
outer = next(l for l in chart['loops'] if l['outer'])
hole = next(l for l in chart['loops'] if not l['outer'])
assert contains(outer, (4, 15)) and not contains(hole, (4, 15))
assert contains(hole, (7.854, 15.45)), 'F15 lost its real hole'
assert not hairdryer['charts'][47]['ready'], 'F47 crossing must not be hidden'
print('Pillow/hairdryer: identity, order, seam, provenance, holes and rejection checks passed.')
