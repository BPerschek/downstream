"""Rebuild DOWNSTREAM's original low-poly foundation props in Blender 5.2.

Run: blender.exe --background --factory-startup --python art/create_blockouts.py
All dimensions are metres. No external models, textures, or fonts are required.
Each exported mesh has a ground-level origin and one mesh object per FBX.
"""
import bpy
import json
import math
import random
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
EXPORTS = ROOT / "exports"
EXPORTS.mkdir(parents=True, exist_ok=True)
random.seed(1037)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
for data in list(bpy.data.materials):
    bpy.data.materials.remove(data)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 1.0
scene.unit_settings.length_unit = "METERS"
bpy.context.preferences.filepaths.save_version = 0

PALETTE = {
    "M_SandConcrete": (0.53, 0.38, 0.23, 1.0),
    "M_LightSand": (0.78, 0.64, 0.43, 1.0),
    "M_DarkSteel": (0.075, 0.105, 0.12, 1.0),
    "M_Rust": (0.55, 0.18, 0.065, 1.0),
    "M_Teal": (0.04, 0.48, 0.43, 1.0),
    "M_Rubber": (0.025, 0.03, 0.025, 1.0),
    "M_DryWood": (0.24, 0.16, 0.095, 1.0),
}
MATS = {}
for name, rgba in PALETTE.items():
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = rgba
    mat.use_nodes = True
    principled = mat.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = rgba
    principled.inputs["Roughness"].default_value = 0.86
    if name in {"M_DarkSteel", "M_Rust", "M_Teal"}:
        principled.inputs["Metallic"].default_value = 0.32
    MATS[name] = mat

parts = []
assets = []
details = {}


def activate(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def register(obj, name, mat):
    obj.name = name
    obj.data.materials.append(MATS[mat])
    parts.append(obj)
    return obj


def bevel(obj, amount=0.02):
    if amount <= 0:
        return obj
    activate(obj)
    modifier = obj.modifiers.new("Single angular chamfer", "BEVEL")
    modifier.width = amount
    modifier.segments = 1
    modifier.affect = "EDGES"
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def box(name, position, size, mat, chamfer=0.0, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=position, rotation=rotation)
    obj = bpy.context.object
    obj.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    register(obj, name, mat)
    bevel(obj, chamfer)
    return obj


def cylinder(name, position, radius, depth, mat, vertices=10, rotation=(0, 0, 0), radius2=None):
    if radius2 is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth,
                                          end_fill_type="NGON", location=position, rotation=rotation)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius, radius2=radius2,
                                      depth=depth, end_fill_type="NGON", location=position,
                                      rotation=rotation)
    return register(bpy.context.object, name, mat)


def mesh(name, verts, faces, mat):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    return register(obj, name, mat)


def beam(name, start, end, width, mat, depth=None, chamfer=0):
    a, b = Vector(start), Vector(end)
    obj = box(name, (a + b) / 2, (width, depth or width, (b - a).length), mat, chamfer)
    obj.rotation_euler = (b - a).to_track_quat("Z", "Y").to_euler()
    return obj


def tube(name, points, radius, mat):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 1
    curve.bevel_depth = radius
    curve.bevel_resolution = 0
    spline = curve.splines.new("POLY")
    spline.points.add(len(points) - 1)
    for point, xyz in zip(spline.points, points):
        point.co = (*xyz, 1)
    obj = bpy.data.objects.new(name, curve)
    scene.collection.objects.link(obj)
    obj.data.materials.append(MATS[mat])
    activate(obj)
    bpy.ops.object.convert(target="MESH")
    parts.append(obj)
    return obj


def polygon_wall(name, outline, y, depth, mat):
    count = len(outline)
    verts = [(x, y - depth / 2, z) for x, z in outline]
    verts += [(x, y + depth / 2, z) for x, z in outline]
    faces = [tuple(range(count - 1, -1, -1)), tuple(range(count, count * 2))]
    faces += [(i, (i + 1) % count, (i + 1) % count + count, i + count) for i in range(count)]
    return mesh(name, verts, faces, mat)


def roof_panel(name, x0, x1, y0, y1, thickness, offset, mat):
    def height(y):
        return 2.47 + 0.15 * y + offset
    verts = [(x, y, height(y) + dz) for dz in (0, thickness)
             for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
    return mesh(name, verts, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                              (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], mat)


def finish(name, note):
    """Join mesh islands; keep editable geometry and stable material slots."""
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    obj.data.name = name + "_Mesh"
    scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    lowest = min(vertex.co.z for vertex in obj.data.vertices)
    for vertex in obj.data.vertices:
        vertex.co.z -= lowest
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    obj["source"] = "Original procedurally authored DOWNSTREAM blockout"
    obj["units"] = "metres; ground pivot; front = local -Y"
    obj["details"] = note
    collection = bpy.data.collections.new(name)
    scene.collection.children.link(collection)
    for existing in list(obj.users_collection):
        existing.objects.unlink(obj)
    collection.objects.link(obj)
    assets.append(obj)
    details[name] = note
    parts.clear()
    return obj


# The home shelter: a real hollow shell with a small open entrance.
box("Foundation slab", (0, 0, .075), (4.15, 3.15, .15), "M_SandConcrete", .04)
box("Rear wall", (0, 1.42, 1.35), (4.0, .16, 2.55), "M_LightSand", .018)
polygon_wall("West sloped wall", [(-1.5, .15), (1.5, .15), (1.5, 2.7), (-1.5, 2.25)],
             0, .16, "M_LightSand")
west = parts[-1]
west.rotation_euler.z = math.pi / 2
west.location.x = -1.92
polygon_wall("East sloped wall", [(-1.5, .15), (1.5, .15), (1.5, 2.7), (-1.5, 2.25)],
             0, .16, "M_SandConcrete")
east = parts[-1]
east.rotation_euler.z = math.pi / 2
east.location.x = 1.92
box("Front left wall", (-.9, -1.42, 1.17), (2.2, .16, 2.04), "M_LightSand", .012)
box("Front right wall", (1.55, -1.42, 1.17), (.9, .16, 2.04), "M_LightSand", .012)
box("Door lintel", (.65, -1.42, 2.11), (1.0, .18, .2), "M_DarkSteel")
box("Door left jamb", (.18, -1.43, 1.12), (.075, .2, 1.95), "M_DarkSteel")
box("Door right jamb", (1.12, -1.43, 1.12), (.075, .2, 1.95), "M_DarkSteel")
# A hinged leaf is visibly open, leaving the doorway traversable.
door_angle = math.radians(-62)
door_center = (1.13 - .42 * math.cos(door_angle), -1.43 - .42 * math.sin(door_angle), 1.02)
box("Open salvaged door", door_center, (.84, .055, 1.74), "M_DryWood", .01,
    rotation=(0, 0, door_angle))
box("Door rust repair", (door_center[0], door_center[1] - .04, .78), (.70, .035, .40), "M_Rust",
    rotation=(0, 0, door_angle))
roof_panel("Single slope metal roof", -2.2, 2.2, -1.7, 1.7, .075, 0, "M_DarkSteel")
roof_panel("Copper roof patch", -1.88, -.7, -.9, .4, .025, .082, "M_Rust")
roof_panel("Pale repair panel", .58, 1.85, .55, 1.54, .025, .08, "M_LightSand")
roof_panel("Rear rust patch", -1.25, -.1, .78, 1.55, .025, .08, "M_Rust")
roof_panel("Front folded repair", .8, 1.7, -1.6, -.75, .025, .085, "M_Rust")
for x in (-1.65, -.55, .55, 1.65):
    beam("Raised standing roof seam", (x, -1.69, 2.27), (x, 1.69, 2.78), .025, "M_DarkSteel")
beam("Front roof edge", (-2.21, -1.7, 2.255), (2.21, -1.7, 2.255), .065, "M_Rust")
beam("Rear roof edge", (-2.21, 1.7, 2.765), (2.21, 1.7, 2.765), .065, "M_DarkSteel")
box("Technology sign casing", (-.6, -1.55, 1.85), (.76, .09, .23), "M_DarkSteel", .015)
box("Teal shelter marker", (-.6, -1.607, 1.85), (.6, .015, .115), "M_Teal")
box("Front rust lower repair", (-1.36, -1.52, .52), (1.0, .04, .6), "M_Rust")
for z in (.34, .73):
    for x in (-1.76, -.96):
        cylinder("Repair rivet", (x, -1.552, z), .026, .025, "M_DarkSteel", 6, (math.pi / 2, 0, 0))
box("Side vertical brace", (-2.015, -.45, 1.35), (.07, .16, 2.15), "M_DryWood")
finish("SM_Shelter", "4 x 3 m hollow shell; 0.86 m entrance; open hinged door; single sloped sheet-metal roof with four raised repair patches, seams, rivets and teal home marker.")

# A small salvage machine; all hard edges and limbs remain simple mesh islands.
for x in (-.21, .21):
    box("Chamfered foot", (x, -.035, .07), (.27, .40, .14), "M_DarkSteel", .025)
    cylinder("Ankle pin", (x, 0, .18), .10, .20, "M_Rust", 8, (0, math.pi / 2, 0))
    box("Lower leg", (x, 0, .32), (.16, .18, .28), "M_DarkSteel", .02)
    box("Knee guard", (x, -.12, .36), (.18, .05, .18), "M_LightSand", .01)
box("Salvage body", (0, 0, .59), (.57, .39, .38), "M_LightSand", .055)
box("Chest rust repair", (-.12, -.219, .60), (.22, .025, .23), "M_Rust", .008)
box("Battery backpack", (0, .24, .61), (.41, .18, .32), "M_DarkSteel", .025)
box("Battery teal stripe", (0, .34, .65), (.30, .012, .07), "M_Teal")
cylinder("Neck", (0, 0, .81), .085, .12, "M_DarkSteel", 8)
box("Angular sensor head", (0, -.025, .94), (.40, .30, .22), "M_DarkSteel", .035)
box("Teal sensor visor", (0, -.181, .96), (.31, .018, .066), "M_Teal", .006)
box("Small head sand plate", (0, -.012, 1.057), (.26, .21, .018), "M_LightSand")
for sign in (-1, 1):
    x = sign * .365
    cylinder("Shoulder swivel", (x, 0, .68), .10, .15, "M_Rust", 8, (0, math.pi / 2, 0))
    beam("Upper arm", (x, 0, .68), (sign * .46, -.04, .47), .09, "M_DarkSteel")
    beam("Forearm", (sign * .46, -.04, .47), (sign * .43, -.19, .36), .11, "M_LightSand")
    beam("Claw outer", (sign * .43, -.19, .37), (sign * .48, -.25, .25), .035, "M_DarkSteel")
    beam("Claw inner", (sign * .43, -.19, .37), (sign * .37, -.25, .26), .035, "M_DarkSteel")
cylinder("Stub antenna", (.14, .035, 1.11), .018, .10, "M_Rust", 6)
finish("SM_SalvageBot", "1.16 m salvage robot; sand armor, rust repairs, teal sensor visor and battery stripe; two articulated block legs and mechanical claw arms. Static concept mesh, no rig.")

box("Crate box", (0, 0, .44), (.87, .82, .86), "M_LightSand", .035)
for x in (-.36, .36):
    box("Crate steel strap", (x, 0, .44), (.065, .85, .9), "M_DarkSteel", .006)
for y in (-.34, .34):
    box("Crate lid rails", (0, y, .87), (.9, .065, .08), "M_Rust", .008)
for z in (.12, .72):
    box("Front rail", (0, -.435, z), (.89, .05, .065), "M_DarkSteel")
box("Shipping marker backing", (.02, -.442, .47), (.29, .035, .16), "M_DarkSteel", .008)
box("Teal shipping marker", (.02, -.464, .47), (.18, .012, .075), "M_Teal")
finish("SM_Crate", "0.9 m reinforced salvage crate with steel bindings, rust lid rails and teal shipping marker.")

polygon_wall("Jagged broken concrete", [(-1.5, .0), (1.5, .0), (1.5, .47), (1.23, .65),
    (.94, .61), (.86, 1.05), (.57, 1.32), (.38, 1.2), (.04, 1.57), (-.19, 1.51),
    (-.43, 1.91), (-.74, 1.8), (-.91, 2.17), (-1.5, 2.17)], 0, .28, "M_SandConcrete")
box("Concrete wall footing", (0, 0, .095), (3.08, .51, .19), "M_LightSand", .045)
polygon_wall("Pale exposed aggregate face", [(-1.48, .43), (-.83, .49), (-.78, 1.34),
    (-1.04, 1.56), (-1.49, 1.54)], -.148, .018, "M_LightSand")
for x, z in ((-.62, 1.78), (-.13, 1.5), (.57, 1.25), (1.07, .57)):
    beam("Exposed rust rebar", (x, 0, z - .18), (x + .05, 0, z + .32), .028, "M_Rust")
for x, y, size in ((1.20, -.47, .28), (.88, -.49, .21), (-.15, -.43, .17)):
    box("Loose concrete rubble", (x, y, size / 2), (size * 1.3, size, size), "M_LightSand",
        .035, (0, .12, x))
finish("SM_BrokenWall", "3.08 m industrial concrete wall; stepped chipped silhouette, exposed rust rebar, aggregate face and footing rubble. Maximum height approximately 2.17 m.")

cylinder("Tapered dry pole", (0, 0, 2.5), .14, 5, "M_DryWood", 8, radius2=.09)
cylinder("Pole steel ground shoe", (0, 0, .32), .175, .64, "M_DarkSteel", 8)
box("Pole crossarm", (0, 0, 4.47), (1.55, .17, .16), "M_Rust", .02)
beam("Left crossarm brace", (0, 0, 3.88), (-.54, 0, 4.43), .045, "M_DarkSteel")
beam("Right crossarm brace", (0, 0, 3.88), (.54, 0, 4.43), .045, "M_DarkSteel")
for x in (-.62, .62):
    cylinder("Insulator pin", (x, 0, 4.65), .035, .3, "M_DarkSteel", 6)
    for z in (4.66, 4.76, 4.86):
        cylinder("Faceted teal insulator", (x, 0, z), .09, .06, "M_Teal", 8)
box("Pole rust service box", (.11, -.14, 1.48), (.29, .18, .48), "M_Rust", .018)
box("Service box latch", (.11, -.238, 1.48), (.045, .018, .16), "M_DarkSteel")
tube("Loose hanging cable", [(.62, .015, 4.91), (.70, -.1, 4.55), (.67, -.14, 3.6),
                             (.3, -.22, 2.85), (.19, -.22, 1.73)], .023, "M_Rubber")
finish("SM_PowerPole", "5 m dry power pole; tapered octagonal shaft, rust crossarm, teal ceramic insulators, bracing, grounded steel shoe and a short disconnected hanging cable.")


def rock(name, position, scale, mat, seed):
    rng = random.Random(seed)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=position)
    obj = bpy.context.object
    for vert in obj.data.vertices:
        vert.co.x *= scale[0] * rng.uniform(.82, 1.13)
        vert.co.y *= scale[1] * rng.uniform(.85, 1.12)
        vert.co.z *= scale[2] * rng.uniform(.85, 1.12)
        # Flat ground contact is deliberate, avoiding half-buried negative pivots.
        vert.co.z = max(-position[2], vert.co.z)
    return register(obj, name, mat)


rock("Large angular sandstone", (-.36, .08, .35), (.72, .51, .60), "M_SandConcrete", 41)
rock("Pale companion stone", (.54, -.16, .22), (.44, .32, .35), "M_LightSand", 42)
rock("Small sandstone", (.27, .48, .13), (.26, .23, .23), "M_SandConcrete", 43)
finish("SM_RockCluster", "Three ground-contact faceted sandstone rocks; deterministic vertex offsets, flat shading, no subdivision modifier.")

drum = cylinder("Ten-sided dented drum", (0, 0, .46), .31, .9, "M_Rust", 12)
for vert in drum.data.vertices:
    if vert.co.x > .1 and vert.co.y < -.1:
        vert.co.x *= .83
        vert.co.y *= .87
for z in (.075, .29, .64, .90):
    cylinder("Drum steel rib", (0, 0, z), .324, .05, "M_DarkSteel", 12)
cylinder("Drum top lid", (0, 0, .94), .3, .026, "M_Rust", 12)
cylinder("Drum bung", (.12, .08, .966), .036, .032, "M_DarkSteel", 8)
box("Drum pale marking", (0, -.298, .48), (.25, .028, .17), "M_LightSand")
finish("SM_Drum", "0.98 m rust drum; low-sided slightly dented shell, four steel rings, top lid and bung, pale marking. No textures.")

for i, (x, y, z, angle, material) in enumerate((
        (-.1, -.12, .10, .12, "M_DarkSteel"), (.12, .18, .17, -.16, "M_Rust"),
        (-.16, -.08, .27, .07, "M_DarkSteel"), (.17, .11, .34, -.07, "M_Rust"))):
    box("Scrap I-beam flange", (x, y, z), (1.73 - i * .12, .28, .07), material,
        .008, (0, .02, angle))
    box("Scrap I-beam web", (x, y, z + .065), (1.73 - i * .12, .045, .1), material,
        0, (0, .02, angle))
    box("Scrap I-beam upper flange", (x, y, z + .125), (1.73 - i * .12, .28, .045), material,
        .006, (0, .02, angle))
for x in (-.66, .67):
    box("Binding strap", (x, 0, .30), (.044, .57, .62), "M_DarkSteel", .005)
cylinder("Offcut pipe", (0, .43, .20), .12, 1.05, "M_Rust", 8, (0, math.pi / 2, 0))
box("Discarded teal panel", (.21, -.31, .50), (.57, .34, .055), "M_Teal", .015,
    (.10, -.13, .24))
finish("SM_ScrapStack", "Bound stack of four angular steel I-beam offcuts, rust pipe and discarded teal technology panel; approximately 1.8 m long.")

# Optional dry silhouette prop; no foliage cards or alpha textures.
beam("Angular trunk lower", (0, 0, .0), (.12, 0, 1.1), .18, "M_DryWood", .16)
beam("Angular trunk upper", (.12, 0, 1.1), (-.03, .03, 2.38), .13, "M_DryWood", .11)
for start, mid, end in (
        ((.07, 0, 1.25), (-.48, .05, 1.82), (-.71, .02, 2.24)),
        ((.03, 0, 1.76), (.62, -.04, 2.17), (.86, -.03, 2.65)),
        ((.07, .01, 1.09), (.34, .40, 1.60), (.43, .60, 1.91)),
        ((.02, .01, 2.03), (-.25, -.24, 2.5), (-.27, -.31, 2.76))):
    beam("Dry branch", start, mid, .085, "M_DryWood", .07)
    beam("Dry twig", mid, end, .048, "M_DryWood", .04)
rock("Root stone", (-.17, .05, .10), (.29, .21, .17), "M_SandConcrete", 57)
finish("SM_AngularTree", "2.8 m bare angular desert tree with branching dry wood silhouette and sandstone at roots. No alpha cards, leaves, collision tricks or wind animation.")


EXPORT_SETTINGS = dict(
    use_selection=True, object_types={"MESH"}, global_scale=1.0,
    apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
    axis_forward="-Y", axis_up="Z", use_space_transform=True,
    bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE",
    use_tspace=False, add_leaf_bones=False, bake_anim=False, path_mode="AUTO",
)
manifest = {
    "project": "DOWNSTREAM", "generator": "art/create_blockouts.py",
    "blender_version": bpy.app.version_string,
    "source_units": "metres", "unit_scale_length": scene.unit_settings.scale_length,
    "authoring_front_axis": "-Y", "pivot": "ground level at local origin",
    "unreal_import": {"import_uniform_scale": 1.0, "convert_scene": True,
                      "convert_scene_unit": True, "combine_meshes": True,
                      "expected_unit": "centimetres", "collision": "generate simple collision on import"},
    "palette": {name: {"rgba": list(rgba), "roughness": .86,
                       "metallic": .32 if name in {"M_DarkSteel", "M_Rust", "M_Teal"} else 0}
                for name, rgba in PALETTE.items()},
    "export_settings": {key: sorted(value) if isinstance(value, set) else value
                        for key, value in EXPORT_SETTINGS.items()},
    "assets": [],
    "license": "Original project-authored geometry, generated locally; no third-party assets.",
}
for obj in assets:
    activate(obj)
    bpy.context.view_layer.update()
    obj.data.calc_loop_triangles()
    dimensions = [round(value, 6) for value in obj.dimensions]
    slots = [slot.material.name for slot in obj.material_slots]
    export_path = EXPORTS / (obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(export_path), **EXPORT_SETTINGS)
    manifest["assets"].append({
        "name": obj.name, "fbx": "exports/" + export_path.name,
        "dimensions_m": dimensions, "expected_dimensions_cm": [round(v * 100, 4) for v in dimensions],
        "vertices": len(obj.data.vertices), "triangles": len(obj.data.loop_triangles),
        "material_slots": slots,
        "material_slot_colors_rgba": [list(PALETTE[name]) for name in slots],
        "details": details[obj.name],
    })
    print("DOWNSTREAM_EXPORT", obj.name, "metres=", dimensions,
          "triangles=", len(obj.data.loop_triangles), "materials=", slots)

(ROOT / "asset_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

# Presentation is isolated from the exports; source meshes remain editable.
presentation = bpy.data.collections.new("PRESENTATION_ONLY")
scene.collection.children.link(presentation)
layout = {
    "SM_Shelter": (-4.6, 1.3, 0),
    "SM_SalvageBot": (-5.8, -3.1, 0),
    "SM_Crate": (-2.7, -2.9, 0),
    "SM_BrokenWall": (1.2, 2.0, 0),
    "SM_PowerPole": (4.8, 3.9, 0),
    "SM_RockCluster": (4.1, .05, 0),
    "SM_Drum": (.1, -2.9, 0),
    "SM_ScrapStack": (3.3, -3.0, 0),
    "SM_AngularTree": (-7.9, -1.2, 0),
}
for obj in assets:
    obj.location = layout[obj.name]


def presentation_object(obj):
    for collection in list(obj.users_collection):
        collection.objects.unlink(obj)
    presentation.objects.link(obj)
    return obj


ground = bpy.data.materials.new("Preview_Ground")
ground.diffuse_color = (.63, .52, .365, 1)
ground.use_nodes = True
ground.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (.63, .52, .365, 1)
ground.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = .95
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.027))
floor = presentation_object(bpy.context.object)
floor.name = "Overview matte desert floor"
floor.data.materials.append(ground)

ink = bpy.data.materials.new("Preview_Ink")
ink.diffuse_color = (.018, .026, .031, 1)
ink.use_nodes = True
ink.node_tree.nodes.clear()
ink_output = ink.node_tree.nodes.new("ShaderNodeOutputMaterial")
ink_emission = ink.node_tree.nodes.new("ShaderNodeEmission")
ink_emission.inputs["Color"].default_value = (.018, .026, .031, 1)
ink_emission.inputs["Strength"].default_value = 1
ink.node_tree.links.new(ink_emission.outputs["Emission"], ink_output.inputs["Surface"])


def text_label(body, position, size, name):
    curve = bpy.data.curves.new(name, "FONT")
    curve.body = body
    curve.size = size
    curve.extrude = .0
    curve.align_x = "LEFT"
    curve.space_character = 1.05
    obj = bpy.data.objects.new(name, curve)
    presentation.objects.link(obj)
    obj.location = position
    obj.data.materials.append(ink)
    return obj


title = text_label("DOWNSTREAM", (-8.0, 5.6, .015), .84, "Overview title")
subtitle = text_label("ORIGINAL LOW-POLY FOUNDATION  /  M0", (-7.96, 4.95, .015), .235, "Overview subtitle")
for name, xy, label in (
    ("shelter", (-6.25, -1.05), "01   HOME SHELTER"),
    ("bot", (-6.34, -3.98), "02   SALVAGE BOT"),
    ("crate", (-3.3, -3.98), "03   CRATE"),
    ("drum", (-.50, -3.98), "04   DRUM"),
    ("scrap", (2.43, -4.05), "05   SCRAP STACK"),
    ("wall", (-.23, .97), "06   BROKEN WALL"),
    ("stone", (3.40, -1.02), "07   SANDSTONE"),
    ("pole", (4.01, 3.2), "08   POWER POLE"),
    ("tree", (-8.72, -2.17), "09   DRY TREE"),
):
    text_label(label, (*xy, .018), .195, "Label " + name)
footer = text_label("METRES  /  FLAT SHADED  /  SEVEN SHARED MATERIALS", (-7.92, -5.25, .018), .20,
                    "Overview footer")

bpy.ops.object.camera_add(location=(12, -19, 15))
camera = presentation_object(bpy.context.object)
camera.name = "Orthographic prop overview"
target = Vector((-.8, .6, .5))
camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 20.8
camera.data.lens = 50
scene.camera = camera
bpy.context.view_layer.update()
for label, camera_xy in ((title, (-9.0, 5.63)), (subtitle, (-8.96, 5.12)),
                         (footer, (-8.96, -5.80))):
    label.location = camera.matrix_world @ Vector((*camera_xy, -18.0))
    label.rotation_euler = camera.rotation_euler
bpy.ops.object.light_add(type="AREA", location=(-4, -6, 13))
key = presentation_object(bpy.context.object)
key.name = "Large soft warm key"
key.data.energy = 2100
key.data.shape = "DISK"
key.data.size = 9
key.rotation_euler = (Vector((-1, 1, 0)) - key.location).to_track_quat("-Z", "Y").to_euler()
bpy.ops.object.light_add(type="AREA", location=(6, 6, 8))
fill = presentation_object(bpy.context.object)
fill.name = "Broad cool fill"
fill.data.energy = 1200
fill.data.size = 8
fill.data.color = (.78, .89, 1.0)
fill.rotation_euler = (Vector((0, 0, 1)) - fill.location).to_track_quat("-Z", "Y").to_euler()
scene.world.color = (.3, .3, .3)
scene.world.use_nodes = True
scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (.60, .57, .50, 1)
scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = .55
scene.render.engine = "CYCLES"
scene.cycles.samples = 48
scene.cycles.use_denoising = True
scene.render.resolution_x = 1600
scene.render.resolution_y = 1100
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.render.filepath = str(ROOT / "Downstream_Blockouts_Overview.png")
scene.view_settings.view_transform = "AgX"
try:
    scene.view_settings.look = "AgX - Medium High Contrast"
except TypeError:
    pass
scene.view_settings.exposure = .4
scene.render.image_settings.color_mode = "RGBA"

# Present a sensible solid viewport when an artist opens the .blend.
activate(assets[0])
for area in bpy.context.screen.areas:
    if area.type == "VIEW_3D":
        area.spaces.active.shading.color_type = "MATERIAL"
        area.spaces.active.shading.light = "STUDIO"
        area.spaces.active.region_3d.view_distance = 19
        area.spaces.active.region_3d.view_location = target
        area.spaces.active.region_3d.view_rotation = camera.rotation_euler.to_quaternion()
scene["DOWNSTREAM source units"] = "METRES"
scene["Rebuild command"] = "blender.exe --background --factory-startup --python art/create_blockouts.py"
scene["Original artwork"] = "All props are original local procedural geometry. PRESENTATION_ONLY is excluded from FBX exports."
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "Downstream_Blockouts.blend"))
bpy.ops.render.render(write_still=True)
print("DOWNSTREAM_COMPLETE", len(assets), "assets; manifest, editable source and overview saved")
