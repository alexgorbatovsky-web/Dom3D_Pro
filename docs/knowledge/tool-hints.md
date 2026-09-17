# Tool hints in language catalogs

Tool hover help uses the existing `toolKey` shared by actions and buttons.
Add matching entries to `Languages/English.xml` and the translated catalogs:

```xml
<TextItem>
  <ID>SurfaceSmartHybrid</ID>
  <Text>Smart Hybrid</Text>
</TextItem>
<TextItem>
  <ID>SurfaceSmartHybrid_HINT</ID>
  <Text>Select open curves forming a connected frame of four-sided regions.
Enable Make Solid to create a solid when the frame is closed.</Text>
</TextItem>
```

IDs are case-sensitive and match the actual tool key (`BSplineCurve`,
`DrawSpline`, `BezierCurve3D`, `NurbsCurve3D`, `fillet_edge`, etc.). Keep hint
text plain; line breaks are preserved. Escape XML characters such as `&amp;`.
No C++ changes are required to author a hint for an already registered tool.

The tooltip contains the localized title, the tool's own icon at 64×64,
the hint, and currently assigned QAction shortcuts. Missing or empty local
translations fall back to English. Tools without a `_HINT` entry retain
their previous tooltip. Language changes and shortcut changes are reflected
on the next hover. Restart the application after manually editing catalogs.

Hints cover curves, node editing, Smart Hybrid, four-curve surfaces,
fillets and all 35 Solid room buttons in English and Russian. The five
unconnected Solid placeholders explicitly describe their unavailable state;
placeholder buttons also expose `toolKey` for localized hover help.
`ToolHints` tests buttons,
toolbar actions, menu actions, language switching, fallback and shortcuts.

Surface and Curves room coverage: all 13 Surface buttons (including three
unconnected placeholders) and all 23 Curves buttons have English and Russian
hints. Descriptions state the input selection, result and confirmation or
pick sequence. SurfaceRuled uses one spline, a length and an axis; CurveFillets
picks a polyline/sketch corner; CurveSimplifyByPoint is the Split by Point tool.
