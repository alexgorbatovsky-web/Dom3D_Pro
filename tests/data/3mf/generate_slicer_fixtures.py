"""Regenerate tiny synthetic slicer packages; no user geometry is included."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

ROOT = Path(__file__).parent
CORE = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
PRODUCTION = "http://schemas.microsoft.com/3dmanufacturing/production/2015/06"
REL = "http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"
PART = "3D/Objects/triangle (1).model"

model = f'''<model xmlns="{CORE}" xmlns:p="{PRODUCTION}" unit="millimeter" requiredextensions="p">
  <metadata name="Application">OrcaSlicer</metadata>
  <metadata name="OrcaSlicer">2.4.2</metadata>
  <metadata name="OrcaSlicer">2.4.2</metadata>
  <metadata name="DesignerUserId"></metadata>
  <resources><object id="2" type="model" p:UUID="00000002-61cb-4c03-9d28-80fed5dfa1dc">
    <components><component objectid="1" p:path="/{PART}"
      p:UUID="00020000-b206-40ff-9872-83e8017abed1" transform="1 0 0 0 1 0 0 0 1 1 2 3"/>
    </components></object></resources>
  <build p:UUID="2c7c17d8-22b5-4d84-8835-1976022ea369"><item objectid="2"
    p:UUID="00000002-b1ec-4553-aec9-835e5b724bb4"
    transform="2 0 0 0 3 0 0 0 4 10 20 30" printable="1" auto_drop="1"/></build>
</model>'''
mesh = f'''<model xmlns="{CORE}" xmlns:p="{PRODUCTION}" unit="millimeter" requiredextensions="p">
  <metadata name="BambuStudio:3mfVersion">1</metadata>
  <resources>
    <object id="1" type="model" p:UUID="00020000-81cb-4c03-9d28-80fed5dfa1dc">
      <mesh><vertices><vertex x="0" y="0" z="0"/><vertex x="1" y="0" z="0"/>
        <vertex x="0" y="1" z="0"/></vertices>
        <triangles><triangle v1="0" v2="1" v3="2"/></triangles></mesh>
    </object>
  </resources><build/>
</model>'''


def relationships(target):
    return f'''<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
      <Relationship Target="{target}" Id="rel-1" Type="{REL}"/></Relationships>'''


for name, root_xml, mesh_xml in (
    ("SlicerMetadata", model, mesh),
    ("SlicerInvalidIndex", model, mesh.replace('v3="2"', 'v3="99"')),
    ("SlicerMissingCoordinate", model, mesh.replace('x="1" y="0" z="0"', 'x="1" y="0"')),
    ("SlicerRequiredExtension", model.replace('requiredextensions="p"',
        'xmlns:custom="urn:dom3d:test:unsupported" requiredextensions="p custom"'), mesh),
):
    entries = {
        "[Content_Types].xml": '''<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
          <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
          <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>
        </Types>''',
        "_rels/.rels": relationships("/3D/3dmodel.model"),
        "3D/3dmodel.model": root_xml,
        "3D/_rels/3dmodel.model.rels": relationships("/" + PART),
        PART: mesh_xml,
    }
    with ZipFile(ROOT / (name + ".3mf"), "w") as archive:
        for path, xml in entries.items():
            entry = ZipInfo(path, date_time=(2026, 1, 1, 0, 0, 0))
            entry.compress_type = ZIP_DEFLATED
            archive.writestr(entry, '<?xml version="1.0" encoding="UTF-8"?>\n' + xml)
