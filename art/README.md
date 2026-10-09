# Original DOWNSTREAM foundation props

`Downstream_Blockouts.blend` contains nine original flat-shaded mesh assets, their seven shared palette materials, and an isolated overview presentation collection. Every prop is editable mesh geometry. The shelter has a hollow interior and an open hinged door; the static salvage robot is a concept blockout, not a rigged or animated character.

Regenerate from the repository root using the installed Blender executable:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --factory-startup --python-exit-code 1 --python .\art\create_blockouts.py
```

The script recreates the source scene, `exports/SM_*.fbx`, `asset_manifest.json`, and `Downstream_Blockouts_Overview.png`. No imported models, external textures, fonts, or third-party geometry are required. The overview camera, labels, lights, and floor exist only in the `.blend` and are excluded from the FBX files.

The FBX files each contain one mesh at a ground-level origin. Authoring units are metres, front is local negative Y, and export uses forward `-Y`, up `Z`, unit conversion enabled, `FBX_SCALE_UNITS`, and global scale `1`. Unreal import should use uniform scale `1`, scene and scene-unit conversion enabled, and combine meshes enabled. The manifest records exact metre and expected centimetre bounds, triangles, material slot order and RGBA values. Recreate the Unreal materials by their exact `M_*` names. Generate simple collision on import; these files contain no custom collision meshes or LODs.

Run the accompanying `validate_blockouts.py` with the same Blender command arguments to check editable source geometry, ground pivots, triangle budgets and FBX round-trip dimensions, material order, palette colors and triangle counts. It writes `fbx_validation.json` and fails with a nonzero exit code if any check fails.

The generated image is an art direction and asset inventory preview; it is not an Unreal performance measurement. The materials have no textures, opacity, or rendering effects. Teal appears as a solid technology accent. Simple convex collision on the shelter would block its hollow entrance; use appropriate box primitives for the foundation and wall sections if interior movement is needed.
