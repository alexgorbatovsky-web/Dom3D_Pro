# Native raytrace dependency

`raytrace-smooth-lighting.patch` records the smooth-normal lighting correction
in the external dependency selected by `DOM3D_RAYTRACE_SOURCE_DIR`
(default: `../raytrace-0.1`). It is already applied to that local source tree.
When transferring these changes to another checkout, apply the patch there
before rebuilding (`git apply <absolute-path-to-patch>` in the dependency root).
The `NativeColors` CTest regression checks that the correction is present,
as well as color ratios across exposure, opaque shadows and thread consistency.

The correction evaluates the light-facing hemisphere using the interpolated
normal for each light sample. It preserves two-sided shading and normal shadow
ray intersection; it does not ignore adjacent triangles or entire objects.

`raytrace-edge-antialiasing.patch` is applied after the smooth-lighting patch.
For AA levels 2 through 7, pixels with sample contrast above 0.04 are retraced
on an 8x8 subpixel grid. AA=1 remains explicitly disabled; AA>=8 uses the
requested grid. This improves silhouette coverage without filtering the final
image. `NativeColors` compares a sloping edge to an 8x8 reference and checks
that progressive passes and worker count do not change the result.
