"""Package the verified low-poly revision and its rebuildable source."""
import bpy, json, zipfile, hashlib, ast
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'output_lowpoly'
audit=json.loads((OUT/'qa'/'asset_audit.json').read_text(encoding='utf-8'))
export=json.loads((OUT/'qa'/'export_and_pose_validation.json').read_text(encoding='utf-8'))
rt=json.loads((OUT/'qa'/'fbx_roundtrip.json').read_text(encoding='utf-8'))
assert not audit['structural_errors'] and not export['errors'] and not rt['errors'], 'QA has errors; do not package.'
assert all(v['triangles']<=3000 for v in rt['loadouts'].values())
lines=['# BRIMBOUND — low-poly revision audit','',
'Every complete loadout is below the requested 3,000-polygon limit using the stricter **triangulated face count**. Counts include the body, shaped head, hair, clothing, hat, weapons and back accessories. Blender source meshes and exported meshes use the same reduced topology.','',
'| Class | Triangles | FBX material slots |','|---|---:|---:|']
for k,d in rt['loadouts'].items():
    lines.append(f'| {k} | {d["triangles"]:,} | {d["material_slots"]} |')
lines+=['','## Head and face changes','',
'- The head is rebuilt from shaped horizontal rings: nearly cylindrical cheek/temple section, narrowing jaw, and a forward, pronounced chin.',
'- Head plus two ears: **564 triangles / 288 vertices**. Exactly three connected mesh components: scalp/jaw and two ears.',
'- No modeled eyes, catchlights, nose marks, eyebrows, mouths or blush. All facial detail is painted into the head textures.',
'- Four interchangeable 2048 × 2048 head maps: Smile, Joy, Determined and Surprise. Each uses the same head topology and UV layout.',
'- The original expression sheet was generated with the built-in ImageGen tool, then baked into the head UVs in Blender. Prompts are in `qa/face_generation_prompts.json`.',
'- Body skin color matches the face textures. Hat and hair clearances were adjusted for the shaped head and reduced mesh density.',
'','## Verification','',
f'- {len(audit["assets"])} source mesh modules: no non-manifold edges, degenerate faces, unweighted vertices or incorrect weight sums.',
'- All eleven loadout visibility states match their manifests.',
'- All eleven assembled FBX files re-imported into clean Blender scenes with the same triangle counts, bounds, bone names, weights, two UV channels and valid texture paths.',
'- Each re-imported loadout has one skinned mesh, a painted face material, and fewer than 3,000 triangles.',
'- Twelve test animation FBXs re-imported with the shared 26-bone skeleton and nonempty action ranges.',
'- Seventy-seven sampled test poses have finite deformed geometry and no weapon-to-head surface intersections.',
'- Rendered and inspected class, gameplay-distance, Worker front/rear, silhouette, pose and expression/head-profile views.',
'','## Use and limits','',
'In the workbench, select the HEAD object and choose `M_FACE_Smile`, `M_FACE_Joy`, `M_FACE_Determined` or `M_FACE_Surprise` in its material slot. Preview instances use Object-linked material slots so each instance can have its own expression.',
'',
'Body and equipment intentionally reuse the shared palette atlas. The head uses its own unique UV unwrap and painted texture. The expressions add one face-material slot, not additional geometry. Assembled exports use two to four material slots depending on team and magic accents.',
'',
'The FK clips are rig/weapon tests, not final combat animations. The intersection check samples weapons against the head, not all possible mesh-pair collisions. Unreal-project import, physics, retargeting and gameplay integration remain untested.',
'',
'This revision is in `output_lowpoly/` and `BRIMBOUND_LowPoly_Delivery.zip`. Rebuild instructions and the required source texture are included under `source/` in the archive.','']
summary='\n'.join(lines);(OUT/'FINAL_QA.md').write_text(summary,encoding='utf-8')
# Read the authoritative build README without executing the rebuild.
source=(ROOT/'scripts'/'build_brimbound_lowpoly.py').read_text(encoding='utf-8')
tree=ast.parse(source);readme=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='README' for t in n.targets))
(OUT/'README.md').write_text(readme,encoding='utf-8')
bpy.ops.wm.open_mainfile(filepath=str(OUT/'BRIMBOUND_Modular_Characters.blend'))
head=bpy.data.objects['HEAD • anime texture / shaped chin']
# Independent connected-component check confirms no small eye/blush islands survived.
adj=[[] for _ in head.data.vertices]
for e in head.data.edges:a,b=e.vertices;adj[a].append(b);adj[b].append(a)
seen=set();parts=[]
for idx in range(len(adj)):
    if idx in seen:continue
    stack=[idx];seen.add(idx);count=0
    while stack:
        v=stack.pop();count+=1
        for w in adj[v]:
            if w not in seen:seen.add(w);stack.append(w)
    parts.append(count)
assert sorted(parts)==[34,34,220],parts
(OUT/'qa'/'face_geometry_audit.json').write_text(json.dumps({'head_components_vertices':parts,'head_triangles':sum(len(p.vertices)-2 for p in head.data.polygons),'separate_eye_blush_geometry':False,'expression_materials':[m.name for m in bpy.data.materials if m.name.startswith('M_FACE_')]},indent=2),encoding='utf-8')
tb=bpy.data.texts.get('START HERE • BRIMBOUND');tb.clear();tb.write(readme)
tb=bpy.data.texts.get('FINAL QA • low-poly revision') or bpy.data.texts.new('FINAL QA • low-poly revision');tb.clear();tb.write(summary)
bpy.context.window.scene=bpy.data.scenes['02 • CLASS GALLERY']
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BRIMBOUND_Modular_Characters.blend'))
paths=[OUT/'BRIMBOUND_Modular_Characters.blend',OUT/'README.md',OUT/'FINAL_QA.md']
for name in ['textures','exports','renders','qa']:paths+=sorted((OUT/name).rglob('*'))
paths=[p for p in paths if p.is_file() and p.name!='Anime_Expressions_Source.png']
manifest={str(p.relative_to(OUT)).replace('\\','/'):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths}
(OUT/'delivery_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8');paths.append(OUT/'delivery_manifest.json')
archive=ROOT/'BRIMBOUND_LowPoly_Delivery.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for p in paths:z.write(p,str(p.relative_to(OUT)))
    for name in ['build_brimbound_lowpoly.py','export_lowpoly.py','roundtrip_lowpoly.py','finalize_lowpoly.py']:
        z.write(ROOT/'scripts'/name,'source/scripts/'+name)
    z.write(ROOT/'README.md','source/README.md')
    for name in ['Anime_Expressions.png','face_generation_prompts.json']:
        z.write(ROOT/'textures'/name,'source/textures/'+name)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    print('PACKAGED',len(z.infolist()),'files',archive.stat().st_size,'bytes',flush=True)
print('LOW-POLY DELIVERY READY',flush=True)
