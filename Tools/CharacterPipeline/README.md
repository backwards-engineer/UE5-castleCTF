# BRIMBOUND character pipeline

Rebuildable Blender source for eleven modular fantasy character loadouts, with a shared 26-bone rig, four painted anime expressions, and a maximum of 3,000 triangles per fully equipped character. The shaped head has cylindrical cheeks, a tapered jaw, and a pronounced chin. Eyes, eyebrows, mouths, and blush are textures on the head, with no separate face-detail geometry.

## Rebuild

Use **Blender 5.2 LTS**, including its bundled Python, NumPy, and FBX importer/exporter. No pip packages, add-ons, network access, or credentials are required. The expression sheet is included, so rebuilding does not call an image-generation service.

Run these commands from this directory in PowerShell, changing the Blender executable path if needed:

```powershell
$blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
$stages = @(
    'build_brimbound_lowpoly.py',
    'export_lowpoly.py',
    'roundtrip_lowpoly.py',
    'finalize_lowpoly.py'
)
foreach ($stage in $stages) {
    & $blender --background --factory-startup --python-exit-code 1 --python (Join-Path $PWD "scripts/$stage")
    if ($LASTEXITCODE -ne 0) { throw "Character pipeline failed: $stage" }
}
```

Each stage resolves paths relative to its source file. The first stage creates the model, UVs, materials, textures, rig, test animations, and eight preview renders. The next two stages export and re-import FBX files to validate the result. The last stage checks those reports and packages the delivery. Rendering uses Cycles on the CPU; build time depends on the machine. Rebuilding replaces files in `output_lowpoly/` and the delivery archive.

Open `output_lowpoly/BRIMBOUND_Modular_Characters.blend` in Blender. It opens in the class gallery; the scene selector also exposes the modular workbench, gameplay-distance previews, animation tests, Worker/team variants, silhouettes, and expression/head-profile study. In the workbench, select `RIG • BRIMBOUND` and set its `loadout` custom property to 0–10. To change the face, assign `M_FACE_Smile`, `M_FACE_Joy`, `M_FACE_Determined`, or `M_FACE_Surprise` to the head's material slot.

## Files

| Source | Purpose |
| --- | --- |
| `scripts/build_brimbound_lowpoly.py` | Procedural meshes, low-poly budgets, rigging, UV baking, materials, scenes, and renders |
| `scripts/export_lowpoly.py` | Loadout/module/skeleton/animation FBX export and structural/pose validation |
| `scripts/roundtrip_lowpoly.py` | Clean-scene FBX re-import checks |
| `scripts/finalize_lowpoly.py` | Final head checks, audit summary, checksum manifest, and ZIP packaging |
| `textures/Anime_Expressions.png` | Opaque 2×2 anime expression source sheet; baked into four 2K head maps |
| `textures/face_generation_prompts.json` | Provenance and prompts for the generated expression sheet |

Generated files remain local and are ignored by Git:

- `output_lowpoly/BRIMBOUND_Modular_Characters.blend`: editable source scene with packed textures.
- `output_lowpoly/textures/`: body/equipment atlas and four full-head expression textures.
- `output_lowpoly/exports/`: eleven assembled loadouts, eighty modules, a skeleton, and twelve animation clips.
- `output_lowpoly/renders/`: eight rendered previews.
- `output_lowpoly/qa/`: topology, weights, visibility, sampled poses, face components, and FBX round-trip reports.
- `output_lowpoly/FINAL_QA.md` and `delivery_manifest.json`: verification summary and SHA-256 file manifest.
- `BRIMBOUND_LowPoly_Delivery.zip`: validated delivery plus the source needed to rebuild it.

## Triangle counts

These counts include the body, head, hair, clothing, hat, weapon, and back accessories. They use triangulated faces, including after FBX re-import.

| Loadout | Triangles |
| --- | ---: |
| Peasant | 1,780 |
| Worker | 2,132 |
| Knight | 2,620 |
| Knight upgrade | 2,748 |
| Priest | 2,232 |
| Dark Priest | 2,312 |
| Fire Mage | 2,552 |
| Ice Mage | 2,518 |
| Archer | 2,692 |
| Gunman | 2,692 |
| Ninja | 2,182 |

The head and ears total 564 triangles, with three connected components: the scalp/jaw and two ears. Assembled FBX exports use one skinned mesh and two to four material slots. The checks verify matching triangle counts, bounds, bone names, weights, both UV channels, texture paths, and painted-face materials. Twelve animation clips are checked for skeleton and action ranges; 77 sampled poses are checked for finite geometry and weapon-to-head intersections.

## Unreal integration

The Blender sources use metres, Z up, and -Y forward. All equipment shares the same armature origin; rigid pieces are weighted to attachment bones. Import the FBX files with one shared skeleton and verify scale, forward axis, material assignments, and socket behavior against the receiving project. `M_TEAM` can map to a material-instance color parameter. The normal map is flat; the visible shape comes from mesh geometry.

The supplied FK clips are rig and weapon tests. Production combat timing, physics, collision, retargeting, LODs, and integration with Crownfall's existing Unreal assets still need engine-side work. The preview courtyard is a Blender presentation scene. Building this toolkit does not populate the Unreal `Content/` directory or make a source-only clone playable.
