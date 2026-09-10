# Associative drafting dimensions

Seven tools live on the Drafting tool panel: horizontal, vertical, parallel,
point-to-line perpendicular, radius, diameter and angular dimensions.

## Interaction

- Linear dimensions: pick two snapping points, then place the dimension line.
- Perpendicular: pick a point, pick a straight edge, then place the dimension.
- Radius/diameter: pick a circle or circular arc, then place the annotation.
- Angular: pick two straight edges, then move into the desired sector and place
  the arc. Placement chooses the supplementary angle as well as the arc radius.
- The blue marker shows snapping; a blue dimension previews the final click.
  The persistent prompt below the viewport describes the next step. Esc or the
  right mouse button cancels an unfinished placement.
- In Select mode, drag a dimension to reposition it. Double-click it to edit its
  prefix, symbol, displayed text, suffix, tolerances, lower note, font, height,
  precision, arrows, colour and line width. Empty displayed text restores the
  measured value. Overriding text does not change model geometry.
- Reattach in the editor replaces the supports of the existing dimension.
  Cancelling reattachment keeps the old supports.

## References and limitations

The versioned `dimension` object is stored inside the existing sheet primitive
JSON, which is already persisted in DOM3D projects. Drafting geometry uses the
primitive ID and an endpoint/corner/centre/edge parameter. CAD geometry uses the
model-view primitive ID, body ID, an indexed topological edge, curve type, edge
count and normalised curve parameter. It does not reference an HLR polyline
index. Circle centres and curve quarter points can also be selected.

Resolution reads the current source geometry. Sheet geometry uses the sheet
scale; model geometry uses its view scale. The projection's centre is saved as
`viewCenterX/Y`, keeping CAD anchors aligned with the normalised HLR cache.
Older views acquire this metadata when loaded. Refresh Views recomputes the
projection while preserving the dimension definitions and placement offsets.
Returning to Drafting after a body shape, placement or visibility change also
refreshes the projections and dimensions. The in-memory change signature uses
body IDs, shape identity, location and visibility; it is not part of the file.

Dimensions measure the geometry in the drawing projection. Radius and diameter
require a circular projection; a foreshortened ellipse is not treated as a
circle. A radius attached to an open arc keeps its arrow on that arc.

References now also retain nine 3D curve samples. After a topology change, an
unchanged edge is resolved by its complete sampled geometry within the original
body, including reversal of the parameter direction. Indices and samples are
updated after resolution. Multiple coincident matches are treated as ambiguous;
missing matches remain explicitly invalid. A changed edge count never permits
fallback to the previous numeric index. Valid legacy references acquire samples
when loaded, before the next edit. The existing indexed fallback remains for
parameter edits with unchanged edge counts/types; this is not a general
persistent topological naming system.

Deleting a drafting primitive or view removes its dependent dimensions. Body
deletion invalidates its annotations.

The Detail-1 hole regression contains five legacy dimensions created when the
body had 143 edges and a later hole increasing the count to 146. The recovery
test replays history without the last hole in an isolated document to reconstruct
the old references, then geometrically maps them to the current body. All nine
dimensions must resolve; only a separate restored project is written.

Rendering uses the same QGraphicsScene paths and text for the screen, print
preview and vector PDF. Moving a source and its dependent dimension together
translates the dimension only once.

## Validation

`CatalogImportTests --test-drafting-dimensions` checks all seven measurements,
supplementary angles, circle recognition, property JSON round-trip, source
resize, placement through scene events, dragging, editing, reattachment, CAD
parameter changes and missing bodies. It renders `seven-types.png` and
`seven-types.pdf` in `output/drafting-dimensions` relative to its working folder.

`ctest --test-dir build -C Release -R Drafting --output-on-failure` also covers
model outlines and initial display of saved sheets.
