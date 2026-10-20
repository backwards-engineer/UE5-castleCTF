"""Re-import delivered FBX files in clean scenes; compare actual data, not exporter success."""
import bpy, json, math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'output_lowpoly'
audit=json.loads((OUT/'qa'/'asset_audit.json').read_text(encoding='utf-8'))
exp=json.loads((OUT/'qa'/'export_and_pose_validation.json').read_text(encoding='utf-8'))
report={'loadouts':{},'animations':{},'errors':[]}
for k,expected in audit['loadouts'].items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(OUT/'exports'/'loadouts'/('BRIMBOUND_'+k+'.fbx')))
    rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE'];meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    tris=sum(len(p.vertices)-2 for o in meshes for p in o.data.polygons)
    vv=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
    bounds={'min':[min(v[i] for v in vv) for i in range(3)],'max':[max(v[i] for v in vv) for i in range(3)]}
    source=exp['source_bounds'][k]
    max_error=max(abs(bounds[m][i]-source[m][i]) for m in ['min','max'] for i in range(3))
    unweighted=sum(not v.groups for o in meshes for v in o.data.vertices)
    bones=sorted(rigs[0].data.bones.keys()) if len(rigs)==1 else []
    uv_ok=all(len(o.data.uv_layers)==2 for o in meshes)
    images=[i for i in bpy.data.images if i.source=='FILE'];missing=[]
    for im in images:
        path=Path(bpy.path.abspath(im.filepath))
        if not path.exists():missing.append(str(path))
    checks={'one_rig':len(rigs)==1,'one_skinned_mesh':len(meshes)==1,'same_bones':bones==sorted(audit['skeleton_names']),
            'same_triangle_count':tris==expected['triangles'],'scale_and_bounds':max_error<1e-5,'two_uv_channels':uv_ok,
            'weighted':unweighted==0,'texture_paths':not missing,'under_3000_triangles':tris<=3000,
            'painted_face_material':any(m.name.startswith('M_FACE_') for o in meshes for m in o.data.materials)}
    row={'pass':all(checks.values()),'checks':checks,'triangles':tris,'material_slots':sum(len(o.data.materials) for o in meshes),
         'max_bounds_error_m':max_error,'bounds_m':bounds,'missing_texture_files':missing}
    report['loadouts'][k]=row
    if not row['pass']:report['errors'].append(k)
    print('ROUNDTRIP',k,row['pass'],flush=True)
for mode in audit['animations']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(OUT/'exports'/'animations'/('TEST_'+mode+'.fbx')))
    rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    actions=list(bpy.data.actions)
    valid=len(rigs)==1 and len(rigs[0].data.bones)==26 and len(actions)==1
    fr=list(actions[0].frame_range) if actions else []
    valid=valid and len(fr)==2 and fr[1]>fr[0]
    report['animations'][mode]={'pass':bool(valid),'actions':len(actions),'frame_range':fr}
    if not valid:report['errors'].append('Animation '+mode)
(OUT/'qa'/'fbx_roundtrip.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('ROUNDTRIP COMPLETE',len(report['errors']),'errors',flush=True)
