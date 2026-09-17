# Curve direction markers

Polyline and B-Spline direction markers share DrawCurveStartArrow. The marker
projects the starting tangent into the viewport and draws a screen-facing
triangle and stem: 22 px total length, 10 px head length, 8 px head width.
Zoom and curve length no longer scale the marker, and orbit cannot turn its
head edge-on. Degenerate projected tangents are skipped. GL matrices and
modified drawing state are restored after each marker.

The surface construction and Reverse icons use light surfaces and simple
filled triangular direction arrows. Generated originals remain in the Codex
generated-images directory; 64 px RGBA copies are stored in src/resources/icons.
