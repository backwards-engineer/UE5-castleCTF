"""Export delivery FBX and perform independent structural/pose validation."""
import bpy, json, math, re, traceback
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'output_lowpoly'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'BRIMBOUND_Modular_Characters.blend'))
audit=json.loads((OUT/'qa'/'asset_audit.json').read_text(encoding='utf-8'))
source=bpy.data.objects['RIG • BRIMBOUND'];scene=bpy.data.scenes['01 • MODULAR WORKBENCH']
bpy.context.window.scene=scene
report={'visibility':{},'pose_checks':[],'exports':[],'errors':[],'source_bounds':{}}
classes=list(audit['loadouts'])
for k in classes:
    vv=[v.co for name in audit['loadouts'][k]['assets'] for v in bpy.data.objects[name].data.vertices]
    report['source_bounds'][k]={'min':[min(v[i] for v in vv) for i in range(3)],'max':[max(v[i] for v in vv) for i in range(3)]}
source.animation_data.action=None
for pb in source.pose.bones:pb.matrix_basis.identity()
for idx,k in enumerate(classes):
    source['loadout']=idx;source.update_tag();scene.frame_set(1);bpy.context.view_layer.update()
    actual=sorted(o.name for o in scene.objects if o.type=='MESH' and o.get('asset_kind') and not o.hide_viewport and not o.hide_render)
    expected=sorted(audit['loadouts'][k]['assets'])
    report['visibility'][k]={'pass':actual==expected,'mesh_count':len(actual)}
    if actual!=expected:report['errors'].append('Visibility mismatch '+k)

# Compare weapon surfaces with the head through each weapon test; hands intentionally contact grips.
checks=[('Peasant','Run'),('Peasant','Arms_Raised'),('Peasant','Arms_Forward'),('Peasant','Deep_Knee_Bend'),('Worker','Axe_Swing'),('Knight','Sword_Attack'),('Knight_Upgrade','Pike_Thrust'),('FireMage','Staff_Cast'),('Archer','Bow_Aim'),('Gunman','Blunderbuss_Fire'),('Ninja','Dagger_Attack')]
for k,mode in checks:
    source['loadout']=classes.index(k);source.update_tag()
    source.animation_data.action=bpy.data.actions['TEST • '+mode]
    assets=[bpy.data.objects[n] for n in audit['loadouts'][k]['assets']]
    weapons=[o for o in assets if o.get('asset_kind')=='weapon']
    for frame in [1,5,9,13,17,21,25]:
        scene.frame_set(frame);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get()
        head=bpy.data.objects['HEAD • anime texture / shaped chin'];hb=BVHTree.FromObject(head,dg)
        overlaps={w.name:len(hb.overlap(BVHTree.FromObject(w,dg))) for w in weapons}
        finite=True
        for ob in assets:
            ev=ob.evaluated_get(dg);me=ev.to_mesh()
            finite=finite and all(math.isfinite(n) for v in me.vertices for n in v.co)
            ev.to_mesh_clear()
        report['pose_checks'].append({'loadout':k,'action':mode,'frame':frame,'finite_vertices':finite,'weapon_head_surface_intersections':overlaps})
        if not finite:report['errors'].append(f'Invalid posed geometry {k}/{mode}/{frame}')
        if any(overlaps.values()):report['errors'].append(f'Weapon/head intersection {k}/{mode}/{frame}')
source.animation_data.action=None
for pb in source.pose.bones:pb.matrix_basis.identity()
source['loadout']=0;source.update_tag();scene.frame_set(1)

export_scene=bpy.data.scenes.new('TEMP_EXPORT');export_scene.unit_settings.system='METRIC';export_scene.unit_settings.scale_length=1
export_scene.render.fps=24;bpy.context.window.scene=export_scene
rig=source.copy();rig.name='Armature';rig.data=source.data;rig.animation_data_clear();export_scene.collection.objects.link(rig)
rig.location=(0,0,0);rig.rotation_euler=(0,0,0);rig.scale=(1,1,1)
for pb in rig.pose.bones:pb.matrix_basis.identity()
rig.animation_data_create()

def clean_name(name):return re.sub(r'[^A-Za-z0-9_]+','_',name).strip('_')
def copy_mesh(name):
    src=bpy.data.objects[name];ob=src.copy();ob.name='SK_'+clean_name(name);ob.data=src.data.copy()
    ob.animation_data_clear();ob.hide_viewport=False;ob.hide_render=False
    export_scene.collection.objects.link(ob);ob.parent=rig;ob.matrix_parent_inverse.identity()
    for mod in ob.modifiers:
        if mod.type=='ARMATURE':mod.object=rig
    return ob
def export(path,objects,animation=False):
    path.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True);bpy.context.view_layer.objects.active=rig
    for ob in objects:ob.select_set(True)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'ARMATURE','MESH'},
        global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
        axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',
        use_triangles=True,add_leaf_bones=False,use_armature_deform_only=False,
        bake_anim=animation,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True,bake_anim_step=1.0,bake_anim_simplify_factor=0.0,
        path_mode='RELATIVE',embed_textures=False,use_custom_props=False)
    report['exports'].append({'file':str(path.relative_to(OUT)).replace('\\','/'),'bytes':path.stat().st_size,'animation':animation})
    for ob in objects:bpy.data.objects.remove(ob,do_unlink=True)

for k in classes:
    obs=[copy_mesh(n) for n in audit['loadouts'][k]['assets']]
    expression={'Worker':'Joy','Knight':'Determined','Knight_Upgrade':'Determined','DarkPriest':'Determined','FireMage':'Joy','IceMage':'Determined','Ninja':'Determined'}.get(k,'Smile')
    for ob in obs:
        if 'HEAD' in ob.name:
            ob.data.materials.clear();ob.data.materials.append(bpy.data.materials['M_FACE_'+expression])
    # Preassembled engine loadouts: one skinned mesh, 2 material slots (3 with magic).
    # The independent module exports below retain the logical source material slots.
    bpy.ops.object.select_all(action='DESELECT')
    for ob in obs:ob.select_set(True)
    bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();combined=obs[0]
    combined.name='SK_BRIMBOUND_'+k
    atlas=bpy.data.materials.get('M_CHARACTER_ATLAS')
    if atlas is None:
        atlas=bpy.data.materials['M_SKIN'].copy();atlas.name='M_CHARACTER_ATLAS'
    old=list(combined.data.materials);mapped=[]
    for m in old:mapped.append(m if m.name in ['M_TEAM','M_MAGIC','M_ICE'] or m.name.startswith('M_FACE_') else atlas)
    unique=list(dict.fromkeys(mapped));inds=[unique.index(mapped[p.material_index]) for p in combined.data.polygons]
    combined.data.materials.clear()
    for m in unique:combined.data.materials.append(m)
    for poly,idx in zip(combined.data.polygons,inds):poly.material_index=idx
    export(OUT/'exports'/'loadouts'/('BRIMBOUND_'+k+'.fbx'),[combined])
    print('EXPORTED LOADOUT',k,flush=True)
for a in audit['assets']:
    export(OUT/'exports'/'modules'/(clean_name(a['name'])+'.fbx'),[copy_mesh(a['name'])])
export(OUT/'exports'/'BRIMBOUND_Skeleton.fbx',[])
for mode in audit['animations']:
    rig.animation_data.action=bpy.data.actions['TEST • '+mode]
    export_scene.frame_start=1;export_scene.frame_end=49 if mode=='Idle' else 25
    export_scene.frame_set(1)
    export(OUT/'exports'/'animations'/('TEST_'+mode+'.fbx'),[],True)
    rig.animation_data.action=None
    for pb in rig.pose.bones:pb.matrix_basis.identity()

(OUT/'qa'/'export_and_pose_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('EXPORT AND POSE VALIDATION COMPLETE',len(report['exports']),'files',len(report['errors']),'errors',flush=True)
