# Powder coating: false bumps from HDRI

The saved `Powder coating.dom3d` scene reproduced broad irregular patches on
all five powder-coated spheres. Setting normalStrength to zero preserved the
patches. Disabling the studio environment removed them while retaining the
original coating normal map. Geometry and coating assets were therefore left
unchanged.

`image_environment_texture_ids` previously used resized environment images for
both rough reflections and diffuse illumination. Downsampling to 48 pixels
does not integrate illumination over a hemisphere: room features remain
visible as false surface relief.

`materials/EnvironmentPrefilter.h` now integrates directions with latitude
solid-angle weights. Diffuse uses a cosine hemisphere; the intermediate
reflection uses a narrower cosine-power lobe. Sharp reflections retain the
original image path. This is a preview approximation, not a full GGX/HDR
prefilter pipeline; it retains the existing QImage color pipeline.

Validation:

- Release builds of CatalogImportTests and Dom3D_Pro succeeded.
- `CatalogImportTests --test-procedural-material --environment-prefilter -platform offscreen`
  verifies constant illumination preservation, suppression of fine environment
  stripes, and empty input.
- Original scene rendered at four camera angles with the original HDRI settings.
  Inspected angles 0 and 2: broad patches removed, coating grain retained.
- Diagnostic images: `output/powder-coating/before-0.50-0.png`,
  `zero-0.50-0.png`, `no-env-0.50-0.png`, and `after-0.50-0.png`.
- Temporary environment setting used for diagnosis was restored immediately.
