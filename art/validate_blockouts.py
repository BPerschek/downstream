"""Validate generated geometry and round-trip every FBX through Blender.

Run with --background --factory-startup --python-exit-code 1 --python.
Writes fbx_validation.json. Failures raise and produce a nonzero process exit.
"""
import bpy
import json
from pathlib import Path
from io_scene_fbx import parse_fbx

ROOT = Path(__file__).resolve().parent
manifest = json.loads((ROOT / "asset_manifest.json").read_text(encoding="utf-8"))
report = {"blender_version": bpy.app.version_string, "assets": [], "passed": False}

bpy.ops.wm.open_mainfile(filepath=str(ROOT / "Downstream_Blockouts.blend"))
for asset in manifest["assets"]:
    obj = bpy.data.objects.get(asset["name"])
    assert obj and obj.type == "MESH", f"Missing editable source mesh {asset['name']}"
    obj.data.calc_loop_triangles()
    assert len(obj.data.loop_triangles) == asset["triangles"], asset["name"] + " triangle mismatch"
    assert len(obj.modifiers) == 0, asset["name"] + " has unapplied modifiers"
    assert all(not polygon.use_smooth for polygon in obj.data.polygons), asset["name"] + " is not flat shaded"
    assert abs(min(vertex.co.z for vertex in obj.data.vertices)) < 1e-5, asset["name"] + " ground pivot mismatch"
    assert asset["triangles"] < 2500, asset["name"] + " exceeds blockout triangle limit"
print("DOWNSTREAM_SOURCE_VALIDATED", len(manifest["assets"]), "editable ground-pivot meshes")

for asset in manifest["assets"]:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    # Remove old material datablocks so Blender does not append .001 suffixes.
    for material in list(bpy.data.materials):
        bpy.data.materials.remove(material)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    path = ROOT / asset["fbx"]
    assert path.exists() and path.stat().st_size > 1024, "Missing/empty FBX " + asset["name"]
    fbx_root, fbx_version = parse_fbx.parse(str(path))
    settings = next(child for child in fbx_root.elems if child.id == b"GlobalSettings")
    properties = next(child for child in settings.elems if child.id == b"Properties70")
    unit_scale = next(prop.props[4] for prop in properties.elems
                      if prop.id == b"P" and prop.props[0] == b"UnitScaleFactor")
    assert abs(unit_scale - 100.0) < 1e-5, f"{asset['name']} FBX does not declare metre units: {unit_scale}"
    bpy.ops.import_scene.fbx(filepath=str(path), global_scale=1.0, use_custom_normals=True)
    imported = [obj for obj in scene.objects if obj.type == "MESH"]
    assert len(imported) == 1, asset["name"] + " must import as one mesh"
    obj = imported[0]
    bpy.context.view_layer.update()
    dimensions = list(obj.dimensions)
    max_error = max(abs(actual - expected) for actual, expected in zip(dimensions, asset["dimensions_m"]))
    assert max_error < 0.0001, f"{asset['name']} scale mismatch: {dimensions} vs {asset['dimensions_m']}"
    slots = [slot.material.name for slot in obj.material_slots]
    assert slots == asset["material_slots"], f"{asset['name']} material mismatch {slots}"
    obj.data.calc_loop_triangles()
    triangles = len(obj.data.loop_triangles)
    assert triangles == asset["triangles"], f"{asset['name']} triangle mismatch {triangles}"
    for slot, expected_rgba in zip(obj.material_slots, asset["material_slot_colors_rgba"]):
        assert max(abs(a - b) for a, b in zip(slot.material.diffuse_color, expected_rgba)) < .0001, f"{asset['name']} palette mismatch"
    result = {"name": asset["name"], "dimensions_m": [round(value, 6) for value in dimensions],
              "scale_error_m": round(max_error, 9), "material_slots": slots,
              "triangles": triangles, "single_mesh": True,
              "fbx_version": fbx_version, "fbx_unit_scale_factor_cm": unit_scale, "passed": True}
    report["assets"].append(result)
    print("DOWNSTREAM_FBX_VALIDATED", asset["name"], "dimensions_m=", result["dimensions_m"],
          "triangles=", triangles, "slots=", slots)

report["passed"] = True
report["total_triangles"] = sum(asset["triangles"] for asset in report["assets"])
(ROOT / "fbx_validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print("DOWNSTREAM_VALIDATION_PASSED", len(report["assets"]), "FBX round trips; total triangles", report["total_triangles"])
