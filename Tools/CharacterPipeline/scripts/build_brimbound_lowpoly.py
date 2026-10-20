"""BRIMBOUND / deterministic Blender source. Run with Blender 5.2 --background --python.
Coordinates: metres, Z up, faces -Y. Source geometry is intentionally editable.
"""
import bpy, bmesh, math, os, sys, json, random, time, shutil
import numpy as np
from mathutils import Vector, Quaternion, Matrix
from math import sin, cos, pi
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output_lowpoly'
for d in ['textures','renders','exports','qa']: (OUT/d).mkdir(parents=True, exist_ok=True)
if not (OUT/'textures'/'Anime_Expressions.png').exists() and (ROOT/'textures'/'Anime_Expressions.png').exists():
    shutil.copy2(ROOT/'textures'/'Anime_Expressions.png',OUT/'textures'/'Anime_Expressions.png')
if (ROOT/'textures'/'face_generation_prompts.json').exists():
    shutil.copy2(ROOT/'textures'/'face_generation_prompts.json',OUT/'qa'/'face_generation_prompts.json')
random.seed(22)
bpy.ops.wm.read_factory_settings(use_empty=True)
S = bpy.context.scene
S.name = '01 • MODULAR WORKBENCH'
S.unit_settings.system='METRIC'; S.unit_settings.scale_length=1.0

def collection(name, parent=None, scene=S):
    c=bpy.data.collections.new(name)
    (parent.children if parent else scene.collection.children).link(c)
    return c
CHAR=collection('CHARACTER'); BASE=collection('BaseBody',CHAR)
RIGC=collection('Rig',CHAR); HAIR=collection('Hair',CHAR)
OUTFITS=collection('OUTFITS'); HATS=collection('HEADGEAR'); WEAPONS=collection('WEAPONS')
TEST=collection('TEST')
CLASSES=['Peasant','Worker','Knight','Knight_Upgrade','Priest','DarkPriest','FireMage','IceMage','Archer','Gunman','Ninja']
DISPLAY=['PEASANT','WORKER','KNIGHT / I','KNIGHT / II','PRIEST','DARK PRIEST','FIRE MAGE','ICE MAGE','ARCHER','GUNMAN','NINJA / SPY']
oc={k:collection(k,OUTFITS) for k in CLASSES}
hc={k:collection(n,HATS) for k,n in zip(CLASSES[1:],['Worker_DoRag','Knight_Helmet','Knight2_Helmet','Priest_Mitre','DarkPriest_Mitre','FireMage_Hat','IceMage_Hat','Archer_Hat','Gunman_Hat','Ninja_Hood'])}
wc={n:collection(n,WEAPONS) for n in ['Worker_Axe','Knight_Sword','Knight_Pike','Priest_Staff','DarkPriest_Staff','Fire_Staff','Ice_Staff','Bow','Quiver','Blunderbuss','Dagger']}

PALETTE={
 'M_SKIN':(.898039,.713725,.509804), 'M_HAIR':(.16,.065,.029),
 'M_CLOTH_PRIMARY':(.68,.55,.35), 'M_CLOTH_SECONDARY':(.085,.12,.105),
 'M_TEAM':(.018,.40,.56), 'M_METAL':(.40,.52,.62),
 'M_LEATHER':(.12,.055,.029), 'M_WOOD':(.27,.105,.034),
 'M_MAGIC':(1.0,.21,.012), 'M_EYES':(.008,.013,.02),
 'M_GOLD':(.68,.36,.075), 'M_IVORY':(.86,.79,.59),
 'M_DARK_METAL':(.075,.105,.145), 'M_SHADOW':(.009,.018,.030),
 'M_FIRE_CLOTH':(.50,.037,.017), 'M_ICE_CLOTH':(.023,.17,.43),
 'M_GREEN':(.11,.26,.068), 'M_CORRUPTION':(.45,.026,.042),
 'M_ICE':(.13,.66,.93), 'M_WHITE':(.98,.95,.81),
 'M_HAIR_LIGHT':(.28,.125,.040), 'M_STITCH':(.37,.24,.12),
 'M_CLOTH_DARK':(.025,.041,.069), 'M_CHEEK':(.80,.26,.13),
}
MAT={}; MI={k:i for i,k in enumerate(PALETTE)}

def make_atlas():
    size=2048; tile=256
    yy,xx=np.mgrid[0:tile,0:tile].astype(np.float32)
    # Broad pigment wash and very restrained grain, exported as ordinary image maps.
    wash=(.98+.026*np.sin(xx*.019)*np.sin(yy*.022)+.018*(yy/tile-.5))
    noise=np.random.default_rng(22).normal(0,.0025,(tile,tile))
    pix=np.ones((size,size,4),dtype=np.float32)
    rough=np.ones_like(pix); metal=np.zeros_like(pix); metal[:,:,3]=1
    for k,i in MI.items():
        u,v=i%8,i//8
        rgb=np.asarray(PALETTE[k])
        if k=='M_TEAM': rgb=np.ones(3)
        pigment=np.ones_like(wash) if k=='M_SKIN' else wash+noise
        pix[v*tile:(v+1)*tile,u*tile:(u+1)*tile,:3]=np.clip(rgb[None,None,:]*pigment[:,:,None],0,1)
        rv=.36 if k in ['M_METAL','M_DARK_METAL','M_GOLD'] else (.26 if k in ['M_MAGIC','M_ICE','M_EYES'] else .78)
        rough[v*tile:(v+1)*tile,u*tile:(u+1)*tile,:3]=rv
        if k in ['M_METAL','M_DARK_METAL','M_GOLD']: metal[v*tile:(v+1)*tile,u*tile:(u+1)*tile,:3]=.72
    normal=np.ones_like(pix); normal[:,:,:3]=(.5,.5,1)
    images={}
    for label,arr in [('BaseColor',pix),('Roughness',rough),('Metallic',metal),('Normal',normal)]:
        im=bpy.data.images.new('BRIMBOUND_2048_'+label,width=size,height=size,alpha=True)
        if label!='BaseColor': im.colorspace_settings.name='Non-Color'
        im.pixels.foreach_set(arr.ravel()); im.filepath_raw=str(OUT/'textures'/f'BRIMBOUND_2048_{label}.png')
        im.file_format='PNG'; im.save(); im.pack(); images[label]=im
    return images
IMAGES=make_atlas()
for name,col in PALETTE.items():
    m=bpy.data.materials.new(name); m.diffuse_color=(*col,1); m.use_nodes=True
    nt=m.node_tree; bs=nt.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value=(*col,1)
    bs.inputs['Roughness'].default_value=.7
    if name!='M_TEAM':
        for key,socket in [('BaseColor','Base Color'),('Roughness','Roughness'),('Metallic','Metallic')]:
            tex=nt.nodes.new('ShaderNodeTexImage'); tex.image=IMAGES[key]; tex.label=key+' • shared 2048 atlas'
            tex.location=(-520,180-170*['BaseColor','Roughness','Metallic'].index(key))
            nt.links.new(tex.outputs['Color'],bs.inputs[socket])
        tex=nt.nodes.new('ShaderNodeTexImage'); tex.image=IMAGES['Normal']; tex.location=(-520,-340)
        normal=nt.nodes.new('ShaderNodeNormalMap'); normal.location=(-220,-240)
        nt.links.new(tex.outputs['Color'],normal.inputs['Color']); nt.links.new(normal.outputs['Normal'],bs.inputs['Normal'])
    if name in ['M_MAGIC','M_ICE']:
        bs.inputs['Emission Color'].default_value=(*col,1); bs.inputs['Emission Strength'].default_value=.22
    MAT[name]=m

def mesh(name, verts, faces, mat, c=BASE, smooth=True):
    me=bpy.data.meshes.new(name+' • mesh'); me.from_pydata(verts,[],faces); me.update()
    ob=bpy.data.objects.new(name,me); c.objects.link(ob); me.materials.append(MAT[mat])
    for f in me.polygons:f.use_smooth=smooth
    return ob

def ell(name,center,scale,mat,c=BASE,n=24,rings=14):
    n=min(n,8);rings=min(rings,6)
    verts=[(center[0],center[1],center[2]+scale[2])]
    for j in range(1,rings):
        a=pi*j/rings
        for i in range(n):
            t=2*pi*i/n
            verts.append((center[0]+scale[0]*sin(a)*cos(t),center[1]+scale[1]*sin(a)*sin(t),center[2]+scale[2]*cos(a)))
    verts.append((center[0],center[1],center[2]-scale[2])); bot=len(verts)-1
    faces=[]
    for i in range(n):faces.append((0,1+i,1+(i+1)%n))
    for j in range(rings-2):
        a=1+j*n; b=a+n
        for i in range(n):faces.append((a+i,b+i,b+(i+1)%n,a+(i+1)%n))
    for i in range(n):faces.append((bot,bot-n+(i+1)%n,bot-n+i))
    return mesh(name,verts,faces,mat,c)

def loft(name,rings,mat,c=BASE,n=24,cap=True,wobble=0):
    n=n if mat.startswith('STAGE') else min(n,10)
    # ring = cx,cy,z,rx,ry. Ordered from bottom to top.
    v=[]
    for j,(x,y,z,rx,ry) in enumerate(rings):
        for i in range(n):
            a=2*pi*i/n; f=1+wobble*sin(3*a+j*.8)
            v.append((x+rx*cos(a)*f,y+ry*sin(a)*f,z))
    faces=[]
    for j in range(len(rings)-1):
        for i in range(n):a=j*n+i;b=j*n+(i+1)%n;faces.append((a,b,b+n,a+n))
    if cap:faces.extend([tuple(reversed(range(n))),tuple(range((len(rings)-1)*n,len(rings)*n))])
    return mesh(name,v,faces,mat,c)

def tube(name,points,radii,mat,c=BASE,n=12):
    n=n if mat.startswith('STAGE') else min(n,5)
    if len(points)>17 and not mat.startswith('STAGE'):
        step=max(1,len(points)//12)
        indices=list(range(0,len(points)-1,step))+[len(points)-1]
        points=[points[i] for i in indices]
        if isinstance(radii,list):radii=[radii[i] for i in indices]
    closed=(Vector(points[0])-Vector(points[-1])).length<1e-7
    if closed:
        points=points[:-1]
        if isinstance(radii,list):radii=radii[:-1]
    v=[]
    for j,p in enumerate(points):
        p=Vector(p)
        tangent=(Vector(points[(j+1)%len(points)])-Vector(points[(j-1)%len(points)])) if closed else (Vector(points[min(j+1,len(points)-1)])-Vector(points[max(j-1,0)]))
        tangent.normalize(); axis=Vector((0,1,0))
        if abs(tangent.dot(axis))>.94:axis=Vector((1,0,0))
        u=tangent.cross(axis).normalized(); w=tangent.cross(u).normalized()
        r=radii[j] if isinstance(radii,(list,tuple)) else radii
        rx,ry=r if isinstance(r,(list,tuple)) else (r,r)
        for i in range(n):
            a=2*pi*i/n; v.append(p+u*(cos(a)*rx)+w*(sin(a)*ry))
    f=[]
    for j in range(len(points) if closed else len(points)-1):
        nxt=((j+1)%len(points))*n
        for i in range(n):a=j*n+i;b=j*n+(i+1)%n;f.append((a,b,nxt+(i+1)%n,nxt+i))
    if not closed:f += [tuple(reversed(range(n))),tuple(range((len(points)-1)*n,len(points)*n))]
    return mesh(name,v,f,mat,c)

def bevel(ob,width=.009,segments=2):
    segments=1
    bpy.context.view_layer.objects.active=ob; ob.select_set(True)
    mod=ob.modifiers.new('Soft manufactured edges','BEVEL'); mod.width=width;mod.segments=segments
    bpy.ops.object.modifier_apply(modifier=mod.name); ob.select_set(False)
    return ob

def box(name,center,scale,mat,c=BASE,bev=.008):
    x,y,z=center; a,b,d=(q/2 for q in scale)
    v=[(x+sx*a,y+sy*b,z+sz*d) for sz in [-1,1] for sy in [-1,1] for sx in [-1,1]]
    f=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
    ob=mesh(name,v,f,mat,c,False)
    return bevel(ob,bev,3) if bev else ob

def extrude(name,outline,y,depth,mat,c=BASE,bev=.005):
    # outline list x,z in counterclockwise or clockwise; normals fixed at asset completion.
    v=[(x,y-depth/2,z) for x,z in outline]+[(x,y+depth/2,z) for x,z in outline]; n=len(outline)
    f=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    for i in range(n):j=(i+1)%n;f.append((i,j,j+n,i+n))
    ob=mesh(name,v,f,mat,c,False)
    return bevel(ob,bev,2) if bev else ob

def ring_xz(name,center,rx,rz,r,mat,c,n=32):
    pts=[(center[0]+rx*cos(2*pi*i/n),center[1],center[2]+rz*sin(2*pi*i/n)) for i in range(n+1)]
    return tube(name,pts,r,mat,c,n=8)

def arc_xy(name,rx,ry,z,mat,c=BASE,r=.008,n=40,start=0,end=2*pi,cx=0,cy=0):
    pts=[(cx+rx*cos(start+(end-start)*i/n),cy+ry*sin(start+(end-start)*i/n),z) for i in range(n+1)]
    return tube(name,pts,r,mat,c,n=8)

def join(parts,name,c):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in parts:ob.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    if len(parts)>1:bpy.ops.object.join()
    ob=parts[0];ob.name=name; ob.data.name=name+' • UV mesh'
    # Remove redundant material slots introduced by joining pieces with the same palette.
    old=list(ob.data.materials); unique=[]; mapping={}
    for i,m in enumerate(old):
        if m not in unique:unique.append(m)
        mapping[i]=unique.index(m)
    inds=[mapping[p.material_index] for p in ob.data.polygons]
    ob.data.materials.clear()
    for m in unique:ob.data.materials.append(m)
    for p,i in zip(ob.data.polygons,inds):p.material_index=i
    bm=bmesh.new();bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=0.000001)
    bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=0.000001)
    bm.to_mesh(ob.data);bm.free();ob.data.update()
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT');ob.select_set(False)
    return ob

# The skeleton is shared by the workbench and all preview instances.
ad=bpy.data.armatures.new('BRIMBOUND_Humanoid • shared skeleton')
RIG=bpy.data.objects.new('RIG • BRIMBOUND',ad);RIGC.objects.link(RIG)
bpy.context.view_layer.objects.active=RIG; RIG.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
BONES={}
def bone(name,head,tail,parent=None,deform=True):
    b=ad.edit_bones.new(name);b.head=head;b.tail=tail;b.use_deform=deform
    if parent:b.parent=ad.edit_bones[parent]
    BONES[name]=(Vector(head),Vector(tail));return b
bone('root',(0,0,0),(0,0,.12))
bone('pelvis',(0,0,.295),(0,0,.395),'root')
bone('spine_01',(0,0,.395),(0,0,.50),'pelvis')
bone('spine_02',(0,0,.50),(0,0,.605),'spine_01')
bone('neck',(0,0,.605),(0,0,.715),'spine_02')
bone('head',(0,0,.715),(0,0,1.20),'neck')
for side,sgn in [('l',1),('r',-1)]:
    bone('clavicle_'+side,(0,0,.585),(sgn*.14,0,.585),'spine_02')
    bone('upperarm_'+side,(sgn*.14,0,.585),(sgn*.237,-.015,.455),'clavicle_'+side)
    bone('lowerarm_'+side,(sgn*.237,-.015,.455),(sgn*.295,-.045,.355),'upperarm_'+side)
    bone('hand_'+side,(sgn*.295,-.045,.355),(sgn*.319,-.068,.318),'lowerarm_'+side)
    bone('thigh_'+side,(sgn*.071,0,.325),(sgn*.078,-.005,.204),'pelvis')
    bone('calf_'+side,(sgn*.078,-.005,.204),(sgn*.081,0,.080),'thigh_'+side)
    bone('foot_'+side,(sgn*.081,0,.080),(sgn*.081,-.088,.051),'calf_'+side)
    bone('weapon_'+side,(sgn*.312,-.083,.344),(sgn*.312,-.083,.414),'hand_'+side)
    bone('hip_'+side,(sgn*.125,0,.345),(sgn*.125,0,.405),'pelvis')
bone('headgear',(0,0,.94),(0,0,1.04),'head')
bone('back',(0,.108,.535),(0,.108,.635),'spine_02')
bpy.ops.object.mode_set(mode='OBJECT');RIG.select_set(False)
RIG.show_in_front=True;ad.display_type='OCTAHEDRAL'
RIG['loadout']=0;RIG.id_properties_ui('loadout').update(min=0,max=10,description='0 Peasant · 1 Worker · 2 Knight · 3 Knight II · 4 Priest · 5 Dark Priest · 6 Fire · 7 Ice · 8 Archer · 9 Gunman · 10 Ninja')
RIG['system']='One skeleton; Z up, -Y forward; metres. Change loadout 0–10 to swap equipment.'
RIG['grip_diameter_m']=.040
ALL=[]; BYCLASS={k:[] for k in CLASSES}; COMMON=[]

def rigid(ob,b):
    g=ob.vertex_groups.new(name=b);g.add(list(range(len(ob.data.vertices))),1,'REPLACE')

def chain_weights(ob,chain):
    # Smooth per-ring skin weights interpolate across joint centres, preserving two-bone blending.
    joints=[BONES[b][0] for b in chain]+[BONES[chain[-1]][1]]
    lengths=[(joints[i+1]-joints[i]).length for i in range(len(chain))]
    cumulative=[0]
    for l in lengths:cumulative.append(cumulative[-1]+l)
    groups={b:ob.vertex_groups.new(name=b) for b in chain}
    for vert in ob.data.vertices:
        p=vert.co;best=(1e9,0)
        for i in range(len(chain)):
            d=joints[i+1]-joints[i];t=max(0,min(1,(p-joints[i]).dot(d)/d.length_squared))
            dist=(p-(joints[i]+t*d)).length
            if dist<best[0]:best=(dist,cumulative[i]+t*lengths[i])
        s=best[1];weights=[0.]*len(chain)
        idx=min(next((i for i in range(len(chain)) if s<=cumulative[i+1]),len(chain)-1),len(chain)-1)
        weights[idx]=1
        for j in range(1,len(chain)):
            width=min(lengths[j-1],lengths[j])*.38
            if abs(s-cumulative[j])<width:
                t=(s-cumulative[j]+width)/(2*width);t=t*t*(3-2*t)
                weights=[0.]*len(chain);weights[j-1]=1-t;weights[j]=t;break
        for b,w in zip(chain,weights):
            if w>0:groups[b].add([vert.index],w,'REPLACE')

def asset(parts,name,c,bone_name=None,classes=None,chain=None,kind='equipment'):
    ob=join(parts,name,c)
    # Budget source meshes, not just export LODs; all visible previews share this geometry.
    budget=100000
    if name.startswith('BODY • torso'):budget=28
    elif name.startswith('BODY • arm'):budget=132
    elif name.startswith('SHIRT • commoner'):budget=155
    elif name.startswith('SHIRT • sleeve'):budget=28
    elif name.startswith('PANTS'):budget=48
    elif name.startswith('BOOTS'):budget=100
    elif name.startswith('BELT'):budget=56
    elif name.startswith('GLOVES'):budget=20
    elif kind=='hair':budget=400 if 'tousled' in name else (60 if 'Worker' in name else 180)
    elif kind=='hat':budget={'Worker':330,'Knight':630,'Knight_Upgrade':680,'Priest':300,'DarkPriest':380,'FireMage':620,'IceMage':620,'Archer':420,'Gunman':520,'Ninja':260}[classes[0]]
    elif kind=='weapon':budget=280 if any(s in name for s in ['Staff','staff','blunderbuss']) else 210
    elif kind=='outfit':budget=180 if 'short tabard' in name else (75 if 'apron' in name else (60 if 'pauldron' in name or 'pouch' in name else 36))
    elif kind=='accessory':budget=170
    triangles=sum(len(p.vertices)-2 for p in ob.data.polygons)
    if triangles>budget:
        bpy.context.view_layer.objects.active=ob;ob.select_set(True)
        mod=ob.modifiers.new('Applied source geometry budget','DECIMATE');mod.ratio=budget/triangles;mod.use_collapse_triangulate=True
        bpy.ops.object.modifier_apply(modifier=mod.name);ob.select_set(False)
        bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-6);bm.to_mesh(ob.data);bm.free();ob.data.update()
    ob['triangle_budget']=budget
    if bone_name:rigid(ob,bone_name)
    elif chain:chain_weights(ob,chain)
    mod=ob.modifiers.new('Shared humanoid deformation','ARMATURE');mod.object=RIG;mod.use_deform_preserve_volume=False
    ob.parent=RIG;ob['asset_kind']=kind;ob['socket']=bone_name or 'skinned'
    ob['forward']='-Y';ob['units']='metres';ALL.append(ob)
    if classes is None:COMMON.append(ob)
    else:
        for k in classes:BYCLASS[k].append(ob)
        for prop in ['hide_render','hide_viewport']:
            fc=ob.driver_add(prop);d=fc.driver;d.type='SCRIPTED'
            v=d.variables.new();v.name='loadout';v.targets[0].id=RIG;v.targets[0].data_path='["loadout"]'
            d.expression=' and '.join(f'loadout != {CLASSES.index(k)}' for k in classes)
    return ob

print('PHASE: base geometry',flush=True)
# Base anatomy and clothing: purpose-built ring topology, compact limbs, broad head.
body=loft('Visible neck',[(0,0,.603,.046,.045),(0,0,.66,.047,.045),(0,0,.815,.052,.047)],'M_SKIN',n=8)
asset([body],'BODY • torso and neck',BASE,chain=['pelvis','spine_01','spine_02','neck'],kind='base')
# A deliberately modeled jaw, chin and near-vertical cheek/temple section, rather than an ellipsoid.
head_rings=[(-.130,.711,.026,.021),(-.103,.732,.074,.056),(-.054,.777,.145,.112),(-.012,.837,.214,.168),
            (0,.891,.243,.201),(0,.960,.246,.204),(0,1.038,.246,.204),(.005,1.101,.230,.190),
            (.011,1.171,.184,.157),(.012,1.224,.108,.094),(.012,1.246,.011,.009)]
hn=20;hv=[]
for cy,z,rx,ry in head_rings:
    for i in range(hn):
        a=2*pi*i/hn;hv.append((rx*cos(a),cy+ry*sin(a),z))
hf=[]
for j in range(len(head_rings)-1):
    for i in range(hn):hf.append((j*hn+i,j*hn+(i+1)%hn,(j+1)*hn+(i+1)%hn,(j+1)*hn+i))
hf += [tuple(reversed(range(hn))),tuple(range((len(head_rings)-1)*hn,len(head_rings)*hn))]
headparts=[mesh('Head • cylindrical temples and tapered chin',hv,hf,'M_SKIN')]
for sg in [-1,1]:headparts += [ell('Ear',(sg*.246,-.003,.955),(.038,.026,.052),'M_SKIN',n=8,rings=5)]
HEAD=asset(headparts,'HEAD • anime texture / shaped chin',BASE,'head',kind='base')

shirt=[loft('Tunic torso',[(0,0,.305,.123,.085),(0,0,.324,.127,.088),(0,0,.365,.117,.082),(0,0,.435,.115,.080),(0,0,.52,.13,.080),(0,0,.578,.145,.078),(0,0,.618,.065,.053)],'M_CLOTH_PRIMARY',n=32)]
shirt += [extrude('Open collar',[(-.044,.607),(0,.555),(.044,.607)],-.084,.009,'M_IVORY',bev=.003)]
asset(shirt,'SHIRT • commoner tunic',BASE,chain=['pelvis','spine_01','spine_02'],kind='base')
for side,sgn in [('l',1),('r',-1)]:
    pts=[(sgn*.137,0,.582),(sgn*.17,-.004,.55),(sgn*.206,-.009,.501),(sgn*.237,-.015,.455),(sgn*.261,-.028,.414),(sgn*.284,-.039,.375),(sgn*.295,-.045,.355)]
    arm=tube('Arm',pts[2:],[.046,.042,.038,.032,.030],'M_SKIN',n=5)
    hand=ell('Hand',(sgn*.31,-.057,.330),(.045,.038,.048),'M_SKIN',n=6,rings=4)
    thumb=ell('Thumb',(sgn*.282,-.082,.340),(.019,.022,.027),'M_SKIN',n=6,rings=4)
    asset([arm,hand,thumb],'BODY • arm and hand '+side,BASE,chain=['upperarm_'+side,'lowerarm_'+side,'hand_'+side],kind='base')
    sleeve=tube('Short sleeve',pts[:3],[.060,.061,.053],'M_CLOTH_PRIMARY',n=24)
    asset([sleeve],'SHIRT • sleeve '+side,BASE,'upperarm_'+side,kind='base')
    legs=[(sgn*.071,0,.326),(sgn*.074,0,.293),(sgn*.076,-.004,.249),(sgn*.078,-.005,.204),(sgn*.08,-.002,.16),(sgn*.081,0,.112),(sgn*.081,0,.078)]
    asset([tube('Pants',legs[:5],[.064,.063,.057,.050,.045],'M_CLOTH_SECONDARY',n=24)],'PANTS • '+side,BASE,chain=['thigh_'+side,'calf_'+side],kind='base')
    boots=[loft('Boot shaft',[(sgn*.081,0,.070,.049,.049),(sgn*.081,0,.115,.046,.045),(sgn*.081,0,.167,.047,.045),(sgn*.081,0,.178,.053,.051)],'M_LEATHER',n=24),ell('Boot toe',(sgn*.081,-.040,.058),(.061,.099,.052),'M_LEATHER',n=24,rings=12),loft('Boot sole',[(sgn*.081,-.037,.012,.060,.098),(sgn*.081,-.037,.030,.064,.103),(sgn*.081,-.037,.038,.061,.100)],'M_SHADOW',n=24)]
    asset(boots,'BOOTS • '+side,BASE,'foot_'+side,kind='base')
    cuff=tube('Glove cuff',[pts[-3],pts[-2],pts[-1]],[.040,.035,.034],'M_LEATHER',n=20)
    asset([cuff],'GLOVES • fingerless cuff '+side,BASE,classes=CLASSES[1:],chain=['lowerarm_'+side,'hand_'+side],kind='base')
belt=loft('Belt',[(0,0,.367,.12,.086),(0,0,.407,.12,.086)],'M_LEATHER',n=40)
asset([belt,box('Buckle',(0,-.090,.386),(.046,.015,.035),'M_GOLD'),box('Buckle inset',(0,-.101,.387),(.023,.006,.014),'M_LEATHER',bev=.003)],'BELT • common waist',BASE,'pelvis',kind='base')

def hair_lock(name,pts,radii,mat='M_HAIR',c=HAIR):return tube(name,pts,radii,mat,c,n=10)
fringe=[]
for i,(x,z) in enumerate([(-.185,1.045),(-.103,1.059),(-.015,1.066),(.082,1.062),(.164,1.047)]):
    fringe.append(hair_lock('Swept fringe',[(x+.018,-.12,z+.105),(x,-.193,z+.035),(x-.027,-.225,z-.04)],[(.060,.045),(.058,.032),(.002,.002)]))
for sg in [-1,1]:fringe.append(hair_lock('Sideburn',[(sg*.231,-.07,1.05),(sg*.241,-.07,.97),(sg*.223,-.08,.923)],[.039,.032,.003]))
asset(fringe,'HAIR • cropped fringe',HAIR,'head',classes=['Archer','Gunman'],kind='hair')
worker_fringe=[]
for x in [-0.17,-0.09,-0.01,0.075,0.15]:
    fy=-0.236*math.sqrt(1-(x/0.29)**2)
    outline=[(x-0.037,1.026),(x+0.032,1.026),(x+0.021,1.008),(x-0.007,0.996),(x-0.032,1.012)]
    worker_fringe.append(extrude('Short hair below cloth',outline,fy,0.013,'M_HAIR',HAIR,bev=0.002))
asset(worker_fringe,'HAIR • Worker short fringe',HAIR,'head',classes=['Worker'],kind='hair')
hair=[loft('Sculpted hair cap',[(0,.023,.962,.258,.217),(0,.017,1.025,.279,.239),(0,.015,1.11,.257,.227),(0,.02,1.195,.192,.177),(0,.016,1.235,.128,.118),(0,.016,1.266,.045,.040),(0,.016,1.27,.004,.004)],'M_HAIR',HAIR,n=32,wobble=.015)]
for i in range(11):
    a=2*pi*i/11;x=.24*cos(a);y=.20*sin(a)
    hair.append(hair_lock('Pointed hair clump',[(x*.78,y*.78,1.16),(x,y,1.074),(x*1.10,y*1.1,1.015 if y<0 else .945)],[(.072,.042),(.063,.03),(.002,.002)],'M_HAIR_LIGHT' if i%4==0 else 'M_HAIR'))
for x in [-.11,-.018,.078,.166]:
    hair.append(hair_lock('Broad front fringe',[(x+.03,-.15,1.18),(x,-.22,1.10),(x-.033,-.227,1.02)],[(.071,.044),(.062,.025),(.002,.002)]))
hair += [hair_lock('Cowlick',[(.03,.02,1.20),(.025,.028,1.29),(-.055,.018,1.315)],[.066,.034,.002]),hair_lock('Cowlick small',[(-.05,.04,1.21),(-.10,.05,1.275),(-.17,.07,1.273)],[.053,.028,.002])]
asset(hair,'HAIR • tousled peasant',HAIR,'head',classes=['Peasant'],kind='hair')

print('PHASE: checkpoint base',flush=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BRIMBOUND_checkpoint.blend'))

print('PHASE: headgear',flush=True)
def hat(parts,name,k):return asset(parts,name,hc[k],'headgear',classes=[k],kind='hat')
def ribbon(name,pts,width,mat,c):
    # Thick cloth strip with a broad, visibly fabric-like tapered end.
    radii=[(width*(1-.38*i/(len(pts)-1)),.006) for i in range(len(pts))]
    return tube(name,pts,radii,mat,c,n=8)

# WORKER: soft cloth crown, wrapped seam, actual rear knot and two trailing fabric ties.
c=hc['Worker']
parts=[loft('Soft wrapped crown',[(0,.012,1.025,.269,.23),(0,.012,1.060,.273,.235),(0,.016,1.145,.246,.218),(0,.020,1.215,.174,.153),(0,.025,1.253,.050,.047),(0,.025,1.258,.004,.004)],'M_TEAM',c,n=32,wobble=.022)]
parts += [arc_xy('Folded cloth hem',.269,.231,1.035,'M_TEAM',c,r=.009),ell('Rear tie knot',(.02,.25,1.045),(.048,.028,.030),'M_TEAM',c,n=20,rings=12)]
parts += [ribbon('Left rear cloth tail',[(.005,.259,1.046),(-.050,.277,.996),(-.10,.286,.909),(-.119,.275,.896)],.033,'M_TEAM',c),ribbon('Right rear cloth tail',[(.035,.258,1.046),(.089,.276,1.001),(.139,.278,.939),(.161,.285,.951)],.033,'M_TEAM',c)]
# Raised folds remain the same material, so every cloth element recolors together.
for sg in [-1,1]:
    parts += [tube('Gathered cloth fold',[(sg*.241,.102,1.053),(sg*.195,.171,1.056),(sg*.105,.222,1.060),(.02,.25,1.045)],[.004,.006,.006,.003],'M_TEAM',c,n=6)]
hat(parts,'HAT • Worker soft do-rag + rear ties','Worker')

def helmet(k,up=False):
    c=hc[k];sx=1.07 if up else 1
    p=[loft('Forged helmet dome',[(0,.012,1.038,.28*sx,.241*sx),(0,.012,1.092,.285*sx,.245*sx),(0,.02,1.205,.243*sx,.209*sx),(0,.022,1.278,.148*sx,.129*sx),(0,.024,1.306,.016,.018)],'M_METAL',c,n=32)]
    # Open lower face and generous stylized cheeks. Dark vertical vents are raised insets.
    p += [arc_xy('Helmet rolled brow',.286*sx,.248*sx,1.06,'M_DARK_METAL',c,r=.011)]
    for sg in [-1,1]:
        p += [extrude('Cheek plate',[(sg*.273*sx,1.10),(sg*.20*sx,1.086),(sg*.204*sx,.916),(sg*.258*sx,.886),(sg*.294*sx,.954)],-.102,.26,'M_METAL',c,bev=.014)]
        p += [ell('Hinge rivet',(sg*.265*sx,-.24,1.035),(.021,.010,.022),'M_GOLD' if up else 'M_DARK_METAL',c,n=16,rings=10)]
    p += [extrude('Visor shield',[(-.209,1.105),(0,1.148),(.209,1.105),(.20,.984),(0,.960),(-.20,.984)],-.246,.024,'M_METAL',c,bev=.009)]
    for x in [-.135,-.066,0,.066,.135]:
        top=1.108-abs(x)*.13
        p += [box('Visor ventilation',(x,-.263,(top+.995)/2),(.020,.008,top-.995),'M_SHADOW',c,bev=.007)]
    p += [tube('Helmet centre ridge',[(0,-.242,1.11),(0,-.17,1.24),(0,.015,1.32),(0,.20,1.23)],[.010,.010,.010,.006],'M_GOLD' if up else 'M_IVORY',c,n=8)]
    if up:
        for i in range(5):
            x=(i-2)*.045
            p += [tube('Fan crest',[(x,0,1.29),(x*1.20,.012,1.43),(x*1.3,.02,1.60-abs(i-2)*.033)],[(.036,.027),(.038,.035),(.008,.007)],'M_TEAM',c,n=10)]
        p += [extrude('Crest golden foot',[(-.13,1.29),(-.125,1.35),(.125,1.35),(.13,1.29)],0,.10,'M_GOLD',c,bev=.008)]
    else:
        for i in range(3):
            x=(i-1)*.048
            p += [tube('Swept cloth plume',[(x,0,1.29),(x,.018,1.42),(x,.12,1.48),(x,.247,1.424),(x,.31,1.335)],[(.031,.041),(.046,.047),(.044,.042),(.030,.028),(.002,.003)],'M_TEAM',c,n=10)]
    return hat(p,'HAT • Knight '+('II fan-crest helm' if up else 'I swept-plume helm'),k)
helmet('Knight');helmet('Knight_Upgrade',True)

def mitre(k,dark=False):
    c=hc[k];mat='M_CLOTH_DARK' if dark else 'M_IVORY';trim='M_CORRUPTION' if dark else 'M_GOLD'
    outline=[(-.249,1.085),(-.274,1.276),(-.172,1.470),(0,1.704),(.172,1.470),(.274,1.276),(.249,1.085)]
    p=[extrude('Mitre body',outline,.012,.306,mat,c,bev=.017)]
    path=[(x,-.157,z) for x,z in outline+[outline[0]]]
    p += [tube('Mitre bound edge',path,.012,trim,c,n=8)]
    p += [tube('Mitre centre spine',[(0,-.165,1.11),(0,-.165,1.30),(0,-.165,1.51),(0,-.163,1.65)],[.010,.012,.009,.002],trim,c,n=8)]
    if dark:
        for sg in [-1,1]:
            p += [extrude('Corrupted side prong',[(sg*.18,1.28),(sg*.27,1.33),(sg*.29,1.535),(sg*.215,1.445)],-.018,.22,'M_DARK_METAL',c,bev=.009)]
        p += [extrude('Broken diamond seal',[(0,1.385),(.075,1.29),(0,1.20),(-.075,1.29)],-.184,.023,trim,c,bev=.003),extrude('Seal inset',[(0,1.335),(.031,1.29),(0,1.244),(-.031,1.29)],-.2,.008,'M_SHADOW',c,bev=.001)]
    else:
        p += [extrude('Sun diamond seal',[(0,1.422),(.060,1.32),(0,1.223),(-.060,1.32)],-.184,.024,'M_GOLD',c,bev=.008)]
        for x in [-.09,.09]:p += [tube('Healing rays',[(x,-.187,1.322),(x*1.37,-.187,1.353)],[.011,.006],'M_GOLD',c,n=8)]
    for sg in [-1,1]:p += [ribbon('Mitre rear lappet',[(sg*.11,.182,1.13),(sg*.115,.22,1.02),(sg*.13,.23,.923)],.040,'M_TEAM',c)]
    hat(p,'HAT • '+('Dark angular mitre' if dark else 'Priest sun mitre'),k)
mitre('Priest');mitre('DarkPriest',True)

def flame_shape(name,x,y,z,s,mat,c):
    coords=[(-.40,-.45),(-.54,-.14),(-.32,.24),(-.20,.09),(.015,.67),(.20,.29),(.37,.39),(.44,-.04),(.28,-.39),(0,-.53)]
    return extrude(name,[(x+a*s,z+b*s) for a,b in coords],y,s*.18,mat,c,bev=s*.045)
def wizard(k,ice=False):
    c=hc[k];mat='M_ICE_CLOTH' if ice else 'M_FIRE_CLOTH'
    p=[loft('Soft wide brim',[(0,0,1.103,.330,.273),(0,0,1.122,.355,.290),(0,0,1.148,.331,.272),(0,0,1.160,.235,.210)],mat,c,n=40,wobble=.025)]
    rings=[(0,0,1.134,.245,.211),(0,.008,1.225,.215,.185),(-.013,.019,1.342,.175,.148),(.002,.026,1.467,.125,.104),(.061,.028,1.591,.081,.067),(.149,.028,1.650,.054,.045),(.228,.028,1.609,.032,.028),(.279,.03,1.65 if ice else 1.555,.003,.003)]
    p += [loft('Bent wizard cone',rings,mat,c,n=32,wobble=.015)]
    p += [loft('Team wizard band',[(0,.008,1.171,.238,.205),(0,.014,1.219,.22,.190)],'M_TEAM',c,n=32),arc_xy('Brim piping',.344,.282,1.127,'M_ICE' if ice else 'M_GOLD',c,r=.007)]
    if ice:
        for a in [0,pi/3,2*pi/3]:
            pts=[(.065*cos(a)*sg,-.183,1.333+.065*sin(a)*sg) for sg in [-1,1]]
            p += [tube('Snow crystal spoke',pts,.009,'M_WHITE',c,n=6)]
            for sg in [-1,1]:
                tip=Vector((.065*cos(a)*sg,-.183,1.333+.065*sin(a)*sg))
                for off in [-.65,.65]:
                    p += [tube('Snow crystal barb',[tip,tip+Vector((-.031*cos(a+off)*sg,0,-.031*sin(a+off)*sg))],.006,'M_WHITE',c,n=6)]
    else:
        p += [flame_shape('Fire hat insignia',0,-.183,1.334,.122,'M_GOLD',c),flame_shape('Fire insignia core',0,-.20,1.324,.055,'M_MAGIC',c)]
    hat(p,'HAT • '+('Ice crystal wizard' if ice else 'Fire floppy wizard'),k)
wizard('FireMage');wizard('IceMage',True)

def feather(name,base,tip,width,mat,c):
    b=Vector(base);t=Vector(tip);d=t-b;side=Vector((d.z,0,-d.x)).normalized()
    v=[b-side*.004,b+d*.20-side*width*.65,b+d*.55-side*width,b+d*.78-side*width*.6,t,b+d*.72+side*width*.65,b+d*.48+side*width*.85,b+d*.15+side*width*.40]
    p=[extrude(name,[(a.x,a.z) for a in v],b.y,.014,mat,c,bev=.003),tube(name+' shaft',[b,t],.005,'M_GOLD',c,n=6)]
    # Three broad engraved ribs, not individual barbs.
    for f in [.28,.46,.64]:
        start=b+d*f;p += [tube('Feather rib',[start,start+d*.14+side*width*.58],.003,'M_IVORY',c,n=5)]
    return p
c=hc['Archer']
p=[loft('Archer cloth cap',[(0,0,1.073,.273,.233),(0,.014,1.146,.273,.231),(.018,.025,1.239,.225,.192),(.084,.03,1.326,.145,.122),(.194,.032,1.36,.067,.048),(.277,.03,1.325,.002,.002)],'M_GREEN',c,n=32)]
p += [loft('Turned cloth brim',[(0,0,1.076,.286,.246),(0,0,1.119,.305,.255),(0,0,1.164,.286,.241)],'M_GREEN',c,n=32),arc_xy('Archer brim seam',.298,.252,1.132,'M_STITCH',c,r=.005)]
p += feather('Archer team feather',(-.22,.03,1.13),(-.31,.03,1.591),.077,'M_TEAM',c)
hat(p,'HAT • Archer folded cap and broad feather','Archer')
c=hc['Gunman']
p=[loft('Renaissance leather brim',[(0,0,1.085,.321,.264),(0,0,1.104,.352,.286),(0,0,1.13,.330,.273)],'M_LEATHER',c,n=40,wobble=.018),loft('Low tapered crown',[(0,0,1.11,.239,.209),(0,0,1.17,.231,.200),(-.010,.01,1.30,.210,.18),(-.02,.014,1.357,.221,.188)],'M_WOOD',c,n=32)]
p += [loft('Gunman team hatband',[(0,0,1.142,.239,.206),(0,0,1.196,.231,.201)],'M_TEAM',c,n=32),arc_xy('Brim gold binding',.344,.280,1.109,'M_GOLD',c,r=.007)]
for x in [-.128,0,.128]:p += [ell('Hatband fitting',(x,-.207+abs(x)*.19,1.169),(.019,.010,.021),'M_GOLD',c,n=12,rings=8)]
p += feather('Gunman cream feather',(.211,.034,1.17),(.348,.034,1.555),.079,'M_IVORY',c)
hat(p,'HAT • Gunman low leather bonnet','Gunman')

c=hc['Ninja']
outer=[(0,1.435),(.175,1.28),(.292,1.087),(.311,.942),(.244,.841),(0,.80),(-.244,.841),(-.311,.942),(-.292,1.087),(-.175,1.28)]
inner=[(0,1.165),(.105,1.111),(.179,1.038),(.208,.947),(.164,.896),(0,.877),(-.164,.896),(-.208,.947),(-.179,1.038),(-.105,1.111)]
v=[(x,-.186,z) for x,z in outer]+[(x,-.253,z) for x,z in inner]+[(x*.97,.205,.998+(z-.998)*.84) for x,z in outer]
v += [(0,.29,1.06)]; f=[];n=len(outer)
for i in range(n):j=(i+1)%n;f += [(i,j,j+n,i+n),(i,2*n+i,2*n+j,j),(2*n+i,3*n,2*n+j)]
# The visible frame and back shell are one watertight open-face hood after inner rim closure.
v += [(x,-.235,z) for x,z in inner]; backinner=3*n+1
for i in range(n):j=(i+1)%n;f += [(n+i,n+j,backinner+j,backinner+i)]
v += [(0,.275,1.06)]; innerpole=len(v)-1
for i in range(n):f += [(backinner+i,backinner+(i+1)%n,innerpole)]
p=[bevel(mesh('Faceted cloth hood',v,f,'M_CLOTH_DARK',c),.008,2)]
p += [tube('Hood opening seam',[(x,-.262,z) for x,z in inner+[inner[0]]],.008,'M_DARK_METAL',c,n=8)]
p += [extrude('Lower face mask',[(-.203,.940),(-.153,.849),(0,.824),(.153,.849),(.203,.940),(.12,.928),(0,.92),(-.12,.928)],-.243,.029,'M_SHADOW',c,bev=.012)]
p += [tube('Team temple stripe',[(-.239,-.21,1.03),(-.21,-.23,1.086),(-.173,-.238,1.12)],[.010,.010,.007],'M_TEAM',c,n=8)]
hat(p,'HAT • Ninja pointed cloth hood and mask','Ninja')

print('PHASE: class clothing',flush=True)
def tabard(k,mat,trim,plate=False):
    c=oc[k]
    p=[loft('Class fitted torso',[(0,0,.315,.131,.095),(0,0,.348,.127,.092),(0,0,.435,.122,.087),(0,0,.522,.138,.087),(0,0,.585,.151,.085),(0,0,.615,.071,.061)],mat,c,n=24)]
    # Short separated tabard ends stop above the knees and use pelvis weights.
    p += [extrude('Front tabard',[(-.064,.39),(-.060,.281),(0,.256),(.060,.281),(.064,.39)],-.100,.012,trim,c,bev=.007)]
    p += [extrude('Team chest accent',[(-.037,.594),(-.030,.446),(.030,.446),(.037,.594)],-.094,.010,'M_TEAM',c,bev=.004)]
    asset(p,'OUTFIT • '+k+' short tabard',c,chain=['pelvis','spine_01','spine_02'],classes=[k],kind='outfit')
    for side,sg in [('l',1),('r',-1)]:
        sleeve=tube('Class short sleeve',[(sg*.137,0,.582),(sg*.17,-.004,.55),(sg*.206,-.009,.501)],[.064,.065,.057],mat,c,n=20)
        asset([sleeve],'OUTFIT • '+k+' sleeve '+side,c,'upperarm_'+side,classes=[k],kind='outfit')
    if plate:
        for side,sg in [('l',1),('r',-1)]:
            p=[ell('Rounded pauldron',(sg*.163,.0,.569),(.077,.093,.066),'M_METAL',c,n=20,rings=12),tube('Bracer',[(sg*.261,-.028,.417),(sg*.281,-.039,.38)],[.045,.040],'M_METAL',c,n=16)]
            # Upper arm and forearm armor must not share one rigid weight.
            asset(p[:1],'ARMOR • '+k+' pauldron '+side,c,'upperarm_'+side,classes=[k],kind='outfit')
            asset(p[1:],'ARMOR • '+k+' bracer '+side,c,'lowerarm_'+side,classes=[k],kind='outfit')
            if k=='Knight_Upgrade':
                asset([ell('Knee guard',(sg*.078,-.049,.207),(.043,.024,.045),'M_GOLD',c,n=16,rings=10)],'ARMOR • Knight II knee '+side,c,'calf_'+side,classes=[k],kind='outfit')
tabard('Knight','M_DARK_METAL','M_METAL',True)
tabard('Knight_Upgrade','M_DARK_METAL','M_GOLD',True)
tabard('Priest','M_IVORY','M_GOLD');tabard('DarkPriest','M_CLOTH_DARK','M_CORRUPTION')
tabard('FireMage','M_FIRE_CLOTH','M_GOLD');tabard('IceMage','M_ICE_CLOTH','M_IVORY')
tabard('Archer','M_GREEN','M_LEATHER');tabard('Gunman','M_WOOD','M_LEATHER')
tabard('Ninja','M_CLOTH_DARK','M_SHADOW')
asset([extrude('Worker apron',[(-.073,.51),(-.095,.30),(.095,.30),(.073,.51)],-.101,.018,'M_LEATHER',oc['Worker'],bev=.008),box('Apron pocket',(0,-.116,.381),(.105,.01,.055),'M_WOOD',oc['Worker'],bev=.010)],'OUTFIT • Worker short leather apron',oc['Worker'],chain=['pelvis','spine_01'],classes=['Worker'],kind='outfit')
for k in ['Worker','Archer','Gunman','Ninja']:
    c=oc[k]
    p=[box('Utility pouch',(.142,.006,.360),(.074,.075,.085),'M_LEATHER',c,bev=.015),box('Pouch flap',(.144,-.036,.38),(.079,.018,.047),'M_WOOD',c,bev=.008)]
    if k=='Ninja':p += [loft('Team sash',[(0,0,.377,.131,.098),(0,0,.41,.129,.096)],'M_TEAM',c,n=32)]
    asset(p,'ACCESSORY • '+k+' belt pouch',c,'hip_l',classes=[k],kind='outfit')

bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BRIMBOUND_checkpoint.blend'))

print('PHASE: weapons',flush=True)
def grip(c,length=.15,r=.022):
    p=[tube('Common 40 mm hand grip',[(0,0,-length/2),(0,0,length/2)],r,'M_LEATHER',c,n=16)]
    return p
def weapon(parts,name,key,k,side='r',tilt=-.12):
    ob=join(parts,name,wc[key]);q=Quaternion((0,1,0),tilt);pos=BONES['weapon_'+side][0]
    for v in ob.data.vertices:v.co=q@v.co+pos
    return asset([ob],name,wc[key],'weapon_'+side,classes=[k],kind='weapon')

c=wc['Worker_Axe'];p=grip(c)
p += [tube('Curved ash handle',[(0,0,-.17),(.014,0,-.10),(0,0,0),(-.008,0,.24),(-.01,0,.395)],[.021,.022,.022,.019,.023],'M_WOOD',c,n=16)]
p += [extrude('Practical axe head',[(-.045,.403),(-.117,.452),(-.217,.472),(-.247,.297),(-.18,.30),(-.096,.349),(.039,.346),(.039,.417)],0,.053,'M_DARK_METAL',c,bev=.009)]
p += [extrude('Broad sharpened axe edge',[(-.217,.472),(-.247,.297),(-.208,.302),(-.184,.460)],-.004,.059,'M_METAL',c,bev=.003)]
weapon(p,'WEAPON • Worker utility axe','Worker_Axe','Worker')

def blade(name,basez,length,width,c):
    # A real diamond cross section gives broad, readable highlights.
    v=[(-width,0,basez),(0,-.016,basez),(width,0,basez),(0,.016,basez),(-width*.76,0,basez+length*.79),(0,-.009,basez+length*.79),(width*.76,0,basez+length*.79),(0,.009,basez+length*.79),(0,0,basez+length)]
    f=[(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,8),(5,6,8),(6,7,8),(7,4,8),(3,2,1,0)]
    return bevel(mesh(name,v,f,'M_METAL',c,False),.002,2)
c=wc['Knight_Sword'];p=grip(c)
p += [blade('Broad short sword',.113,.56,.050,c),tube('Swept sword guard',[(-.115,0,.089),(-.079,0,.108),(0,0,.117),(.079,0,.108),(.115,0,.089)],.015,'M_GOLD',c,n=10),ell('Pommel',(0,0,-.098),(.031,.022,.032),'M_GOLD',c,n=16,rings=10)]
weapon(p,'WEAPON • Knight broad sword','Knight_Sword','Knight',tilt=-0.40)
c=wc['Knight_Pike'];p=grip(c,.17)
p += [tube('Long pike shaft',[(0,0,-.321),(0,0,1.58)],[.020,.014],'M_WOOD',c,n=16),blade('Pike leaf spearhead',1.575,.307,.050,c),loft('Spear socket',[(0,0,1.48,.025,.025),(0,0,1.64,.020,.020)],'M_DARK_METAL',c,n=16),ell('Pike butt',(0,0,-.329),(.025,.025,.030),'M_DARK_METAL',c,n=16,rings=10)]
weapon(p,'WEAPON • Knight II 2.24 m long pike','Knight_Pike','Knight_Upgrade',tilt=-.075)

# All staffs share the same grasp and lower haft family.
for k,key,theme in [('Priest','Priest_Staff','heal'),('DarkPriest','DarkPriest_Staff','dark'),('FireMage','Fire_Staff','fire'),('IceMage','Ice_Staff','ice')]:
    c=wc[key];p=grip(c,.17);trim='M_DARK_METAL' if theme=='dark' else 'M_GOLD'
    p += [tube('Staff family hardwood haft',[(0,0,-.255),(0,0,.48),(0,0,.64)],[.023,.019,.023],'M_WOOD',c,n=16)]
    for z in [-.23,.48,.57]:p += [loft('Staff ferrule',[(0,0,z-.018,.026,.026),(0,0,z+.018,.026,.026)],trim,c,n=16)]
    if theme=='heal':
        p += [ring_xz('Open healing halo',(0,0,.751),.114,.119,.020,'M_GOLD',c),extrude('Healing sun diamond',[(0,.863),(.043,.753),(0,.651),(-.043,.753)],-.011,.040,'M_IVORY',c,bev=.010)]
        for sg in [-1,1]:p += [tube('Halo sun ray',[(sg*.098,0,.80),(sg*.151,0,.827)],.016,'M_GOLD',c,n=10)]
    elif theme=='dark':
        p += [ring_xz('Corrupted staff halo',(0,0,.751),.109,.115,.025,'M_DARK_METAL',c,n=24),ell('Corrupted orb',(0,-.009,.751),(.067,.045,.077),'M_CORRUPTION',c,n=16,rings=12)]
        for a in [0,pi/2,pi,3*pi/2]:
            b=(.10*cos(a),0,.751+.10*sin(a));e=(.165*cos(a),0,.751+.165*sin(a))
            p += [tube('Corrupted halo point',[b,e],[.034,.002],'M_DARK_METAL',c,n=6)]
    elif theme=='fire':
        p += [flame_shape('Large flame crystal',0,-.003,.748,.239,'M_MAGIC',c),flame_shape('Flame pale core',0,-.030,.72,.132,'M_WHITE',c)]
        for sg in [-1,1]:p += [tube('Flame cradle',[(0,0,.56),(sg*.067,0,.61),(sg*.10,0,.73)],[.026,.025,.01],'M_GOLD',c,n=10)]
    else:
        p += [loft('Large ice prism',[(0,0,.612,.012,.012),(0,0,.702,.087,.069),(0,0,.80,.059,.048),(0,0,.931,.002,.002)],'M_ICE',c,n=6)]
        for sg in [-1,1]:p += [tube('Ice cradle',[(0,0,.56),(sg*.074,0,.61),(sg*.104,0,.733)],[.025,.023,.008],'M_METAL',c,n=10)]
    weapon(p,'WEAPON • '+key.replace('_',' '),key,k,tilt=-.20)

c=wc['Bow'];p=[]
points=[(0,0,-.343),(.090,0,-.27),(.134,0,-.15),(.112,0,0),(.134,0,.15),(.09,0,.27),(0,0,.343)]
# Position grip at local origin and recurved limbs extending away from the wearer.
points=[(x-.112,y,z) for x,y,z in points]
p += [tube('Recurve ash bow',points,[.011,.017,.021,.026,.021,.017,.011],'M_WOOD',c,n=12)]
p += [tube('Visible bow string',[points[0],(-.18,0,0),points[-1]],.0035,'M_IVORY',c,n=6)]
for z in [-.045,-.022,0,.022,.045]:p += [arc_xy('Bow grip wrap',.030,.026,z,'M_LEATHER',c,r=.004,n=16)]
weapon(p,'WEAPON • Archer recurve bow','Bow','Archer',side='l',tilt=.05)
c=wc['Quiver'];p=[]
p += [loft('Leather quiver',[(.135,.155,.331,.047,.043),(.135,.155,.40,.055,.052),(.135,.155,.69,.060,.055)],'M_LEATHER',c,n=20),arc_xy('Quiver lip',.063,.058,.68,'M_GOLD',c,r=.007,cx=.135,cy=.155)]
for i in range(4):
    x=.108+(i%2)*.050;y=.14+(i//2)*.034;z=.865+.025*(i%2)
    p += [tube('Arrow shaft',[(x,y,.47),(x,y,z)],.006,'M_WOOD',c,n=6)]
    p += [extrude('Broad arrow fletching',[(x,z-.07),(x-.024,z-.045),(x-.022,z+.017),(x,z)],y,.009,'M_IVORY',c,bev=.001)]
asset(p,'BACK • Archer quiver with four arrows',c,'back',classes=['Archer'],kind='accessory')

c=wc['Blunderbuss'];p=[]
p += [extrude('Walnut shoulder stock',[(-.055,-.083),(-.065,.096),(.014,.12),(.080,.037),(.070,-.043),(.02,-.056)],.068,.24,'M_WOOD',c,bev=.015)]
p += [box('Wooden fore-end',(0,-.144,.103),(.091,.268,.079),'M_WOOD',c,bev=.014)]
# A hollow flared barrel, with a real muzzle opening and recessed dark bore.
v=[];n=24
for y,z,r in [(.007,.148,.044),(-.23,.154,.047),(-.35,.16,.083),(-.396,.164,.097),(-.405,.164,.078),(-.353,.16,.068),(-.245,.154,.033)]:
    for i in range(n):a=2*pi*i/n;v.append((r*cos(a),y,z+r*sin(a)))
f=[]
for j in range(6):
    for i in range(n):a=j*n+i;b=j*n+(i+1)%n;f.append((a,b,b+n,a+n))
f += [tuple(reversed(range(n))),tuple(range(6*n,7*n))]
p += [mesh('Flared blunderbuss barrel with bore',v,f,'M_DARK_METAL',c),tube('Hammer',[(.051,.003,.137),(.071,.036,.203),(.073,.068,.205)],.013,'M_METAL',c,n=10),ring_xz('Trigger guard',(0,-.01,.022),.042,.043,.010,'M_GOLD',c,n=20)]
for y in [-.13,-.25]:
    pts=[(.05*cos(2*pi*i/24),y,.154+.05*sin(2*pi*i/24)) for i in range(25)]
    p += [tube('Barrel band',pts,.009,'M_GOLD',c,n=8)]
weapon(p,'WEAPON • Gunman hollow-mouth blunderbuss','Blunderbuss','Gunman',tilt=0)
c=wc['Dagger'];p=grip(c,.117,.020)
p += [blade('Leaf dagger',.077,.255,.041,c),tube('Dagger guard',[(-.074,0,.064),(0,0,.080),(.074,0,.064)],.012,'M_DARK_METAL',c,n=10),ring_xz('Dagger ring pommel',(0,0,-.081),.028,.029,.009,'M_DARK_METAL',c,n=20)]
weapon(p,'WEAPON • Ninja ring dagger','Dagger','Ninja',tilt=-.30)

bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BRIMBOUND_checkpoint.blend'))

print('PHASE: UV unwrap',flush=True)
if os.environ.get('BRIMBOUND_COUNTS_ONLY'):
    print(json.dumps({k:sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in COMMON+BYCLASS[k]) for k in CLASSES}),flush=True)
    raise SystemExit(0)
for ob in ALL:
    if ob.animation_data:
        for fc in ob.animation_data.drivers:fc.mute=True
    ob.hide_viewport=False;ob.hide_render=False;ob.hide_set(False)
bpy.context.view_layer.update()
for num,ob in enumerate(ALL):
    bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.012,area_weight=.2,scale_to_bounds=True)
    bpy.ops.object.mode_set(mode='OBJECT')
    uv=ob.data.uv_layers.active;uv.name='UV_Atlas'
    unique=ob.data.uv_layers.new(name='UV_Unique')
    for a,b in zip(uv.data,unique.data):b.uv=a.uv
    for poly in ob.data.polygons:
        idx=MI[ob.data.materials[poly.material_index].name];u=idx%8;v=idx//8
        for li in poly.loop_indices:
            old=uv.data[li].uv.copy();uv.data[li].uv=((u+.045+old.x*.91)/8,(v+.045+old.y*.91)/8)
    ob.data.uv_layers.active_index=0;uv.active_render=True
    ob['UV_note']='UV_Atlas: shared palette atlas, intentional reuse across assets. UV_Unique: unique unwrap within this object for repainting.'
    if num%25==0:print('  UV',num,'/',len(ALL),flush=True)
    ob.select_set(False)

print('PHASE: bake anime expression textures onto the head UVs',flush=True)
# These are ordinary opaque head textures; no face cards, eye meshes, blush meshes or alpha geometry.
FACE_NAMES=['Smile','Joy','Determined','Surprise'];FACE_MATS={}
face_source=bpy.data.images.load(str(OUT/'textures'/'Anime_Expressions.png'),check_existing=True)
face_source.name='SOURCE • generated anime expression sheet'
uv=HEAD.data.uv_layers['UV_Atlas'];unique=HEAD.data.uv_layers['UV_Unique']
for a,b in zip(uv.data,unique.data):a.uv=b.uv
projected=HEAD.data.uv_layers.new(name='UV_FacialProjection')
original_count=hn*len(head_rings)
front_flags=[all(vi<original_count for vi in poly.vertices) and poly.normal.y<-.26 for poly in HEAD.data.polygons]
front=bpy.data.materials.new('TEMP • face projection');front.use_nodes=True;nt=front.node_tree;nt.nodes.clear()
em=nt.nodes.new('ShaderNodeEmission');out=nt.nodes.new('ShaderNodeOutputMaterial');nt.links.new(em.outputs[0],out.inputs['Surface'])
read=nt.nodes.new('ShaderNodeTexImage');read.image=face_source;read.extension='EXTEND'
coords=nt.nodes.new('ShaderNodeUVMap');coords.uv_map='UV_FacialProjection';nt.links.new(coords.outputs['UV'],read.inputs['Vector']);nt.links.new(read.outputs['Color'],em.inputs['Color'])
front_target=nt.nodes.new('ShaderNodeTexImage')
back=bpy.data.materials.new('TEMP • head skin bake');back.use_nodes=True;bn=back.node_tree;bn.nodes.clear()
be=bn.nodes.new('ShaderNodeEmission');be.inputs['Color'].default_value=(.7835,.4678,.2232,1)
bo=bn.nodes.new('ShaderNodeOutputMaterial');bn.links.new(be.outputs[0],bo.inputs['Surface']);back_target=bn.nodes.new('ShaderNodeTexImage')
HEAD.data.materials.clear();HEAD.data.materials.append(front);HEAD.data.materials.append(back)
for poly,is_front in zip(HEAD.data.polygons,front_flags):poly.material_index=0 if is_front else 1
S.render.engine='CYCLES';S.cycles.samples=1;S.render.bake.margin=16
bpy.ops.object.select_all(action='DESELECT');HEAD.select_set(True);bpy.context.view_layer.objects.active=HEAD
HEAD.data.uv_layers.active_index=0;uv.active_render=True
for idx,name in enumerate(FACE_NAMES):
    col=idx%2;row=idx//2
    for loop in HEAD.data.loops:
        co=HEAD.data.vertices[loop.vertex_index].co
        u=max(.012,min(.988,co.x/.50+.50));v=max(.012,min(.988,(co.z-.758)/.350))
        projected.data[loop.index].uv=((col+u)*.5,(1-row+v)*.5)
    im=bpy.data.images.new('BRIMBOUND_Face_'+name+'_2048',2048,2048,alpha=False)
    front_target.image=im;nt.nodes.active=front_target
    back_target.image=im;bn.nodes.active=back_target
    bpy.ops.object.bake(type='EMIT',use_clear=True,margin=16)
    im.filepath_raw=str(OUT/'textures'/('BRIMBOUND_Face_'+name+'_2048.png'));im.file_format='PNG';im.save();im.pack()
    mat=bpy.data.materials.new('M_FACE_'+name);mat.use_nodes=True;mat.use_fake_user=True
    principled=mat.node_tree.nodes.get('Principled BSDF');principled.inputs['Roughness'].default_value=.82
    image_node=mat.node_tree.nodes.new('ShaderNodeTexImage');image_node.image=im
    mat.node_tree.links.new(image_node.outputs['Color'],principled.inputs['Base Color'])
    FACE_MATS[name]=mat;MAT[mat.name]=mat;IMAGES['Face_'+name]=im
    print('BAKED',name,flush=True)
HEAD.data.materials.clear();HEAD.data.materials.append(FACE_MATS['Smile'])
for poly in HEAD.data.polygons:poly.material_index=0
HEAD.data.uv_layers.remove(projected);HEAD.data.uv_layers.active_index=0;HEAD.data.uv_layers[0].active_render=True
HEAD['UV_note']='Both channels are a unique head unwrap. M_FACE_* uses its baked 2048 head expression texture. No extra facial geometry.'
HEAD['expressions']='M_FACE_Smile / M_FACE_Joy / M_FACE_Determined / M_FACE_Surprise'
HEAD.select_set(False)
bpy.data.materials.remove(front);bpy.data.materials.remove(back)
if face_source.users==0:bpy.data.images.remove(face_source)

print('PHASE: reusable animation tests',flush=True)
ACTIONS={}
def reset_pose(rig=RIG):
    for pb in rig.pose.bones:pb.rotation_mode='QUATERNION';pb.rotation_quaternion=(1,0,0,0);pb.location=(0,0,0);pb.scale=(1,1,1)
def world_rot(rig,b,xyz):
    q=Quaternion((1,0,0),math.radians(xyz[0])) @ Quaternion((0,1,0),math.radians(xyz[1])) @ Quaternion((0,0,1),math.radians(xyz[2]))
    rest=rig.data.bones[b].matrix_local.to_quaternion()
    rig.pose.bones[b].rotation_quaternion=rest.inverted()@q@rest
def translate_bone(rig,b,xyz):rig.pose.bones[b].location=rig.data.bones[b].matrix_local.to_quaternion().inverted()@Vector(xyz)

def absolute_bone_rotation(rig,name,q):
    bpy.context.view_layer.update()
    pb=rig.pose.bones[name];pos=pb.matrix.translation.copy()
    rest=rig.data.bones[name].matrix_local.to_quaternion()
    pb.matrix=Matrix.Translation(pos)@(q@rest).to_matrix().to_4x4()
    bpy.context.view_layer.update()

def aim_arm(rig,side,target):
    # Analytic two-bone reach, baked to FK keys; no Blender-only constraints are needed at runtime.
    upper='upperarm_'+side;lower='lowerarm_'+side;hand='hand_'+side
    shoulder=BONES[upper][0];target=Vector(target)
    l1=(BONES[upper][1]-shoulder).length;l2=(BONES[lower][1]-BONES[lower][0]).length
    d=target-shoulder;dist=min(d.length,(l1+l2)*.985);d.normalize();target=shoulder+d*dist
    a=(l1*l1-l2*l2+dist*dist)/(2*dist);h=math.sqrt(max(0,l1*l1-a*a))
    pole=Vector((1 if side=='l' else -1,.08,-.20));pole=(pole-d*pole.dot(d)).normalized()
    elbow=shoulder+d*a+pole*h
    q1=(BONES[upper][1]-shoulder).rotation_difference(elbow-shoulder)
    absolute_bone_rotation(rig,upper,q1)
    q2=(BONES[lower][1]-BONES[lower][0]).rotation_difference(target-elbow)
    absolute_bone_rotation(rig,lower,q2)
    absolute_bone_rotation(rig,hand,Quaternion())

def apply_test(rig,mode,t):
    reset_pose(rig);s=sin(t*2*pi);pulse=sin(pi*t)**2
    if mode=='Idle':
        translate_bone(rig,'pelvis',(0,0,.004*s));world_rot(rig,'spine_02',(1*s,0,0));world_rot(rig,'head',(0,1.2*s,0))
        for side,sg in [('l',1),('r',-1)]:world_rot(rig,'upperarm_'+side,(1.5*s,sg*1.5*s,0))
    elif mode=='Run':
        translate_bone(rig,'pelvis',(0,0,.012*(1-cos(t*4*pi))));world_rot(rig,'spine_01',(8,0,0))
        for side,sg in [('l',1),('r',-1)]:
            world_rot(rig,'thigh_'+side,(sg*36*s,0,0));world_rot(rig,'calf_'+side,(max(0,-sg*s)*67+9,0,0))
            world_rot(rig,'upperarm_'+side,(-sg*27*s,-sg*8,0));world_rot(rig,'lowerarm_'+side,(-28,0,0))
    elif mode in ['Sword_Attack','Axe_Swing','Dagger_Attack']:
        reach={'Sword_Attack':78,'Axe_Swing':87,'Dagger_Attack':40}[mode]
        world_rot(rig,'upperarm_r',(-18*pulse,reach*pulse,0));world_rot(rig,'lowerarm_r',(-12*pulse,0,0))
        world_rot(rig,'spine_02',(0,0,-13*s));world_rot(rig,'upperarm_l',(-15*pulse,-18*pulse,0))
        absolute_bone_rotation(rig,'weapon_r',Quaternion((1,0,0),math.radians(35*sin(2*pi*t))))
    elif mode=='Pike_Thrust':
        aim_arm(rig,'r',(-.14,-.08-.045*pulse,.49))
        aim_arm(rig,'l',(0.048,-0.22,0.525))
        q=Vector((-.075,0,.997)).rotation_difference(Vector((.79,-.60,.10)))
        absolute_bone_rotation(rig,'weapon_r',q)
    elif mode=='Staff_Cast':
        world_rot(rig,'upperarm_r',(-28*pulse,26*pulse,0));world_rot(rig,'lowerarm_r',(-14*pulse,0,0))
        world_rot(rig,'upperarm_l',(-65*pulse,-35*pulse,0));world_rot(rig,'head',(-6*pulse,0,0))
        absolute_bone_rotation(rig,'weapon_r',Quaternion())
    elif mode=='Bow_Aim':
        aim_arm(rig,'l',(.10,-.245,.55));aim_arm(rig,'r',(.075,-.10,.54))
        absolute_bone_rotation(rig,'weapon_l',Quaternion((1,0,0),0.25)@Quaternion((0,0,1),-pi/2));world_rot(rig,'head',(0,0,8))
    elif mode=='Blunderbuss_Fire':
        aim_arm(rig,'r',(0,-.04+.015*pulse,.43));aim_arm(rig,'l',(-.017,-.21+.015*pulse,.52))
        absolute_bone_rotation(rig,'weapon_r',Quaternion((1,0,0),math.radians(-6*pulse)))
    elif mode=='Arms_Raised':
        for side,sg in [('l',1),('r',-1)]:world_rot(rig,'upperarm_'+side,(0,-sg*110,0))
    elif mode=='Arms_Forward':
        for side,sg in [('l',1),('r',-1)]:world_rot(rig,'upperarm_'+side,(-83,-sg*12,0))
    elif mode=='Deep_Knee_Bend':
        translate_bone(rig,'pelvis',(0,0,-.096));world_rot(rig,'spine_01',(12,0,0))
        for side,sg in [('l',1),('r',-1)]:
            world_rot(rig,'thigh_'+side,(-57,-sg*7,0));world_rot(rig,'calf_'+side,(108,0,0));world_rot(rig,'foot_'+side,(-51,0,0))
            world_rot(rig,'upperarm_'+side,(-65,-sg*15,0))

MODES=['Idle','Run','Sword_Attack','Axe_Swing','Pike_Thrust','Staff_Cast','Bow_Aim','Blunderbuss_Fire','Dagger_Attack','Arms_Raised','Arms_Forward','Deep_Knee_Bend']
RIG.animation_data_create()
for mode in MODES:
    action=bpy.data.actions.new('TEST • '+mode);action.use_fake_user=True;RIG.animation_data.action=action
    end=49 if mode=='Idle' else 25
    for frame in range(1,end+1,4):
        t=(frame-1)/(end-1);apply_test(RIG,mode,t)
        for pb in RIG.pose.bones:
            pb.keyframe_insert('rotation_quaternion',frame=frame,group=pb.name)
            if pb.name=='pelvis':pb.keyframe_insert('location',frame=frame,group=pb.name)
    action['purpose']='Rig and weapon clearance test; not a production combat animation.'
    ACTIONS[mode]=action
RIG.animation_data.action=None;reset_pose();S.frame_start=1;S.frame_end=49;S.render.fps=24

print('PHASE: QA data',flush=True)
import bmesh
AUDIT={'system':'BRIMBOUND LOW POLY','units':'metres','forward':'-Y','base_head_height_m':.535,'bare_head_character_height_m':1.246,'triangle_cap_per_equipped_character':3000,'separate_eye_or_blush_geometry':False,'face_expressions':FACE_NAMES,'skeleton_bones':len(ad.bones),'skeleton_names':list(ad.bones.keys()),'animations':list(ACTIONS),'assets':[],'loadouts':{}}
def tris(ob):return sum(len(p.vertices)-2 for p in ob.data.polygons)
for ob in ALL:
    bm=bmesh.new();bm.from_mesh(ob.data)
    badedges=sum(not e.is_manifold for e in bm.edges);degenerate=sum(f.calc_area()<1e-10 for f in bm.faces);bm.free()
    unweighted=sum(not v.groups for v in ob.data.vertices)
    badweights=sum(abs(sum(g.weight for g in v.groups)-1)>1e-4 for v in ob.data.vertices)
    maxinfluences=max(len(v.groups) for v in ob.data.vertices)
    row={'name':ob.name,'kind':ob['asset_kind'],'vertices':len(ob.data.vertices),'triangles':tris(ob),'uv_layers':list(ob.data.uv_layers.keys()),'nonmanifold_edges':badedges,'degenerate_faces':degenerate,'unweighted_vertices':unweighted,'bad_weight_sums':badweights,'max_influences':maxinfluences,'scale':list(ob.scale),'socket':ob['socket']}
    AUDIT['assets'].append(row)
for k in CLASSES:
    objs=COMMON+BYCLASS[k]
    AUDIT['loadouts'][k]={'triangles':sum(tris(o) for o in objs),'mesh_count':len(objs),'assets':[o.name for o in objs]}
assert max(x['triangles'] for x in AUDIT['loadouts'].values())<=3000,'An equipped class exceeds the requested triangle cap'
AUDIT['structural_errors']=[a['name'] for a in AUDIT['assets'] if a['unweighted_vertices'] or a['bad_weight_sums'] or a['nonmanifold_edges'] or a['degenerate_faces']]
AUDIT['limitations']=['Cloth is skinned; no cloth simulation.','Body/equipment UV_Atlas intentionally shares color tiles. Head uses a unique UV unwrap with four interchangeable painted expressions.','Test animations are FK validation clips, not gameplay combat motion.','Unreal import and retargeting must be verified in the target project.']
(OUT/'qa'/'asset_audit.json').write_text(json.dumps(AUDIT,indent=2),encoding='utf-8')

# Restore workbench's live loadout selector once operators have finished editing assets.
for ob in ALL:
    if ob.animation_data:
        for fc in ob.animation_data.drivers:fc.mute=False
RIG['loadout']=0;bpy.context.view_layer.update()

def solid_mat(name,col,metallic=0,rough=.8,emission=False):
    m=bpy.data.materials.new(name);m.diffuse_color=(*col,1);m.use_nodes=True
    b=m.node_tree.nodes.get('Principled BSDF');b.inputs['Base Color'].default_value=(*col,1);b.inputs['Roughness'].default_value=rough;b.inputs['Metallic'].default_value=metallic
    if emission:b.inputs['Emission Color'].default_value=(*col,1);b.inputs['Emission Strength'].default_value=.6
    MAT[name]=m;return m
solid_mat('STAGE • ink',(.018,.032,.048));solid_mat('STAGE • plinth',(.040,.065,.078));solid_mat('STAGE • ivory lettering',(.64,.74,.72),emission=True);solid_mat('STAGE • muted lettering',(.20,.35,.39),emission=True);solid_mat('STAGE • brass line',(.29,.20,.082),.3)

def setup_scene(scene,w=2200,h=1400):
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
    scene.render.resolution_x=w;scene.render.resolution_y=h;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    scene.view_settings.view_transform='AgX'
    world=bpy.data.worlds.new(scene.name+' world');world.use_nodes=True
    world.node_tree.nodes['Background'].inputs['Color'].default_value=(.12,.18,.24,1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value=.35;scene.world=world
    scene.render.fps=24;scene.frame_start=1;scene.frame_end=49

def camera(scene,c,name,loc,target,ortho):
    data=bpy.data.cameras.new(name);ob=bpy.data.objects.new(name,data);c.objects.link(ob);ob.location=loc
    ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=ortho;data.lens=50
    scene.camera=ob;return ob
def light(c,name,loc,power,color,size,target=(0,0,.7)):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.color=color;data.shape='DISK';data.size=size
    ob=bpy.data.objects.new(name,data);c.objects.link(ob);ob.location=loc;ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler();return ob
def studio(scene,c,extent=200):
    mesh('STAGE • seamless floor',[(-extent,-extent,-.081),(extent,-extent,-.081),(extent,extent,-.081),(-extent,extent,-.081)],[(0,1,2,3)],'STAGE • ink',c,False)
    light(c,'LIGHT • warm softbox',(-3,-5,7),1300,(1,.83,.64),5)
    light(c,'LIGHT • cool fill',(5,-2,5),1050,(.61,.79,1),4)
    light(c,'LIGHT • soft rim',(0,5,7),1700,(.70,.86,1),4)
def label(c,name,body,location,size=.12,mat='STAGE • ivory lettering',rotation=None,align='CENTER'):
    cu=bpy.data.curves.new(name,'FONT');cu.body=body;cu.size=size;cu.align_x=align;cu.space_character=1.15;cu.extrude=0
    ob=bpy.data.objects.new(name,cu);c.objects.link(ob);ob.location=location
    if rotation is not None:ob.rotation_euler=rotation
    cu.materials.append(MAT[mat]);return ob
def overlay(c,cam,text,x,y,size,mat='STAGE • ivory lettering'):
    ob=label(c,'TITLE • '+text,text,(0,0,0),size,mat,align='LEFT');ob.parent=cam;ob.location=(x,y,-8);ob['overlay_camera']=cam.name;return ob

INSTANCES=[]
def instance(scene,k,pos=(0,0,0),angle=0,pose=None,phase=.25,suffix=''):
    c=collection('PREVIEW • '+k+suffix,scene=scene)
    rr=RIG.copy();rr.data=ad;rr.name='PREVIEW RIG • '+k+suffix;c.objects.link(rr);rr.animation_data_clear();reset_pose(rr);rr.location=pos;rr.rotation_euler.z=angle
    meshes=[]
    for src in COMMON+BYCLASS[k]:
        ob=src.copy();ob.data=src.data;ob.name=k+' / '+src.name+suffix;ob.animation_data_clear();ob.hide_viewport=False;ob.hide_render=False;c.objects.link(ob)
        ob.parent=rr;ob.matrix_parent_inverse.identity();ob.location=(0,0,0);ob.rotation_euler=(0,0,0);ob.scale=(1,1,1)
        for m in ob.modifiers:
            if m.type=='ARMATURE':m.object=rr
        if src==HEAD:
            expression={'Worker':'Joy','Knight':'Determined','Knight_Upgrade':'Determined','DarkPriest':'Determined','FireMage':'Joy','IceMage':'Determined','Ninja':'Determined'}.get(k,'Smile')
            ob.material_slots[0].link='OBJECT';ob.material_slots[0].material=FACE_MATS[expression]
        meshes.append(ob)
    if pose:apply_test(rr,pose,phase)
    INSTANCES.append((rr,meshes));return rr,meshes

print('PHASE: scene assembly',flush=True)
setup_scene(S,1300,1300);studio(S,TEST)
workcam=camera(S,TEST,'CAMERA • workbench',(2.6,-5,2.7),(0,0,.85),2.35)
G=bpy.data.scenes.new('02 • CLASS GALLERY');setup_scene(G,2400,1500)
bpy.context.window.scene=G
stage=collection('TEST • gallery stage',scene=G);studio(G,stage)
cam=camera(G,stage,'CAMERA • class gallery',(1.4,-16,10.5),(0,0,.60),10.3)
positions=[]
for i,k in enumerate(CLASSES):
    row=0 if i<6 else 1;j=i if i<6 else i-6;count=6 if row==0 else 5
    x=(j-(count-1)/2)*1.43;y=1.7 if row==0 else -1.55
    positions.append((x,y,0));rr,meshes=instance(G,k,(x,y,0),angle=.10)
    if k=='Gunman':world_rot(rr,'upperarm_r',(0,0,-35))
    plinth=loft('STAGE • '+k+' base',[(x,y,-.073,.60,.46),(x,y,-.015,.60,.46),(x,y,-.001,.574,.445)],'STAGE • plinth',stage,n=48)
    arc_xy('STAGE • brass inset',.576,.444,-.006,'STAGE • brass line',stage,r=.006,n=48,cx=x,cy=y)
    label(stage,'LABEL • '+k,DISPLAY[i],(x,y-.55,.045),.12,rotation=cam.rotation_euler)
    label(stage,'LABEL • index '+k,f'{i+1:02d}',(x-.44,y-.42,.044),.073,'STAGE • muted lettering',cam.rotation_euler)
overlay(stage,cam,'B R I M B O U N D',-4.65,2.65,.36)
overlay(stage,cam,'SMALL FOLK.   UNMISTAKABLE HATS.',-4.62,2.39,.115,'STAGE • muted lettering')
overlay(stage,cam,'11 CLASSES   /   UNDER 3,000 TRIANGLES EACH',1.0,2.65,.095)
overlay(stage,cam,'LOW POLY EDITION   /   PAINTED ANIME FACES   /   SHAPED CHIN',-4.62,-2.89,.10,'STAGE • muted lettering')

P=bpy.data.scenes.new('03 • GAMEPLAY COURTYARD');setup_scene(P,1800,1200)
bpy.context.window.scene=P
pc=collection('TEST • gameplay stage',scene=P);studio(P,pc)
solid_mat('STAGE • warm stone',(.14,.19,.18));solid_mat('STAGE • moss',(.08,.12,.053))
for ix in range(-8,9):
    for iy in range(-6,7):
        x=ix*.48+(iy%2)*.24;y=iy*.48
        # Broad staggered stone slabs are intentionally understated around the character silhouettes.
        box('STAGE • courtyard slab',(x,y,-.067),(.46,.46,.065),'STAGE • warm stone' if random.random()>.10 else 'STAGE • moss',pc,bev=.025)
game_positions=[(-2.3,-1.4,0),(-.8,-1.8,0),(.7,-1.8,0),(2.3,-1.3,0),(-2.7,.4,0),(-1.1,.4,0),(.5,.2,0),(2.1,.65,0),(-1.5,2.1,0),(.1,2,0),(1.8,2.0,0)]
for i,k in enumerate(CLASSES):
    pose='Run' if k in ['Worker','Ninja'] else None
    instance(P,k,game_positions[i],angle=[-.3,.35,-.2,.5][i%4],pose=pose,phase=.125,suffix=' • gameplay')
normal_cam=camera(P,pc,'CAMERA • NORMAL / 9 m framing',(7,-10,10),(0,0,.6),9.5)
close_cam=camera(P,pc,'CAMERA • CLOSE / 3 m framing',(4,-8,6),(-1.4,-1.4,.8),3.3)
far_cam=camera(P,pc,'CAMERA • FAR / 15 m framing',(9,-13,13),(0,0,.6),15)
P.camera=normal_cam
overlay(pc,normal_cam,'BRIMBOUND   /   GAMEPLAY READABILITY',-4.25,2.70,.18)
overlay(pc,normal_cam,'ORTHOGRAPHIC  •  NORMAL DISTANCE  •  SHARED TEAM MATERIAL',-4.25,2.44,.09,'STAGE • muted lettering')
overlay(pc,far_cam,'BRIMBOUND   /   FAR DISTANCE',-6.8,4.2,.26)
overlay(pc,far_cam,'15 M FRAMING  •  SILHOUETTE AND TEAM COLOR CHECK',-6.8,3.83,.12,'STAGE • muted lettering')
overlay(pc,close_cam,'BRIMBOUND   /   CLOSE DISTANCE',-1.47,.91,.067)

# A separate testing scene lets the clips be inspected without disturbing the gallery.
A=bpy.data.scenes.new('04 • ANIMATION TESTS');setup_scene(A,1800,1400)
bpy.context.window.scene=A
ac=collection('TEST • pose stage',scene=A);studio(A,ac)
acam=camera(A,ac,'CAMERA • deformation contact sheet',(1,-16,14),(0,0,.8),10.4)
test_pairs=[('Peasant','Arms_Raised'),('Peasant','Arms_Forward'),('Peasant','Deep_Knee_Bend'),('Worker','Axe_Swing'),('Knight','Sword_Attack'),('Knight_Upgrade','Pike_Thrust'),('FireMage','Staff_Cast'),('Archer','Bow_Aim'),('Gunman','Blunderbuss_Fire'),('Ninja','Dagger_Attack'),('Peasant','Run'),('Peasant','Idle')]
for i,(k,mode) in enumerate(test_pairs):
    x=(i%4-1.5)*2.05;y=(1-i//4)*3.05
    rr,meshes=instance(A,k,(x,y,0),angle=.08,pose=mode,phase=.5 if mode!='Run' else .125,suffix=' • '+mode)
    rr.animation_data_create();rr.animation_data.action=ACTIONS[mode]
    # Preserve a useful contact-sheet pose at frame 13; each rig runs its corresponding action.
    label(ac,'TEST LABEL • '+mode,mode.replace('_',' ').upper(),(x,y-.55,.03),.095,rotation=acam.rotation_euler)
A.frame_set(13)
overlay(ac,acam,'RIG & EQUIPMENT   /   TEST POSES',-4.0,3.12,.20)
overlay(ac,acam,'FK VALIDATION CLIPS  •  24 FPS  •  26 BONES',-4,2.87,.09,'STAGE • muted lettering')

W=bpy.data.scenes.new('05 • WORKER & TEAM COLORS');setup_scene(W,1800,1000)
bpy.context.window.scene=W
tc=collection('TEST • cloth and team proof',scene=W);studio(W,tc)
wcam=camera(W,tc,'CAMERA • do-rag front and rear',(1,-9,4.3),(0,0,.80),5.3)
red=solid_mat('M_TEAM_RED • preview variant',(0.65,0.032,0.040),rough=.7)
for j,(x,ang,is_red) in enumerate([(-1.8,0,False),(-.6,pi,False),(.6,0,True),(1.8,pi,True)]):
    rr,meshes=instance(W,'Worker',(x,0,0),angle=ang,suffix=' • cloth proof '+str(j))
    if is_red:
        for ob in meshes:
            for slot in ob.material_slots:
                if slot.material==MAT['M_TEAM']:slot.link='OBJECT';slot.material=red
    label(tc,'LABEL • worker view '+str(j),('RED' if is_red else 'BLUE')+' / '+('REAR' if ang else 'FRONT'),(x,-.45,.05),.105,rotation=wcam.rotation_euler)
overlay(tc,wcam,'WORKER  /  CLOTH, KNOT & TWO TAILS',-2.39,1.19,.15)
overlay(tc,wcam,'ONE MESH.  TEAM COLOR CHANGES THROUGH M_TEAM.',-2.39,.99,.075,'STAGE • muted lettering')

H=bpy.data.scenes.new('06 • SILHOUETTE TEST');setup_scene(H,2200,1200)
bpy.context.window.scene=H;sc=collection('TEST • silhouette silhouettes',scene=H)
H.view_settings.view_transform='Standard'
H.world.node_tree.nodes['Background'].inputs['Color'].default_value=(1,1,1,1);H.world.node_tree.nodes['Background'].inputs['Strength'].default_value=1
black=bpy.data.materials.new('TEST • silhouette black');black.use_nodes=True
nt=black.node_tree;nt.nodes.clear();out=nt.nodes.new('ShaderNodeOutputMaterial');em=nt.nodes.new('ShaderNodeEmission');em.inputs[0].default_value=(0.012,.020,.025,1);nt.links.new(em.outputs[0],out.inputs['Surface'])
H.view_layers[0].material_override=black
hcam=camera(H,sc,'CAMERA • monochrome silhouettes',(1,-16,10.5),(0,0,.6),10.3)
for i,k in enumerate(CLASSES):
    row=0 if i<6 else 1;j=i if i<6 else i-6;count=6 if row==0 else 5
    x=(j-(count-1)/2)*1.43;y=1.7 if row==0 else -1.55
    instance(H,k,(x,y,0),angle=.10,suffix=' • silhouette')
    label(sc,'LABEL • silhouette '+k,DISPLAY[i],(x,y-.55,.02),.09,rotation=hcam.rotation_euler)
overlay(sc,hcam,'SILHOUETTES   /   CLASS ID WITHOUT COLOR',-4.65,2.22,.17)

F=bpy.data.scenes.new('07 • ANIME FACES & HEAD FORM');setup_scene(F,2000,1400)
bpy.context.window.scene=F;fc=collection('TEST • expression and head form study',scene=F);studio(F,fc)
fcam=camera(F,fc,'CAMERA • expression and head profile',(0,-9,3.0),(0,0,1.14),3.8)
overlay(fc,fcam,'PAINTED FACES   /   ZERO EXTRA GEOMETRY',-1.71,1.10,.10)
overlay(fc,fcam,'4 EXPRESSIONS   •   ONE 564-TRIANGLE HEAD',-1.71,.96,.058,'STAGE • muted lettering')
for idx,name in enumerate(FACE_NAMES):
    x=(idx-1.5)*.86
    rr,meshes=instance(F,'Peasant',(x,0,.69),suffix=' • expression '+name)
    for ob in meshes:
        if ob.get('asset_kind')!='hair' and 'HEAD •' not in ob.name:ob.hide_render=True;ob.hide_viewport=True
        if 'HEAD •' in ob.name:
            ob.material_slots[0].link='OBJECT';ob.material_slots[0].material=FACE_MATS[name]
    label(fc,'EXPRESSION • '+name,name.upper(),(x,-.26,1.30),.063,rotation=fcam.rotation_euler)
clay=solid_mat('TEST • clay head',(.7835,.4678,.2232),rough=.82)
for x,angle,title in [(-.67,0,'FRONT / JAW & CHIN'),(.67,-pi/2,'PROFILE / CHIN PROJECTION')]:
    rr,meshes=instance(F,'Peasant',(x,0,-.15),angle=angle,suffix=' • clay '+title)
    for ob in meshes:
        if 'HEAD •' not in ob.name:ob.hide_render=True;ob.hide_viewport=True
        else:ob.material_slots[0].link='OBJECT';ob.material_slots[0].material=clay
    label(fc,'FORM • '+title,title,(x,-.25,.49),.048,rotation=fcam.rotation_euler)

README='''BRIMBOUND — LOW POLY / ANIME FACE EDITION

REVISION
Every fully equipped character, including headgear, clothing, weapon and back accessories,
contains fewer than 3,000 TRIANGLES. The source meshes and FBX exports use the same geometry.
The head has near-vertical cylindrical cheek/temple sides, a tapered jaw and a forward chin.
Eyes, eyebrows, nose marks, mouths and blush are entirely painted; no eye/blush geometry remains.
This delivery is rebuilt from the low-poly source in Tools/CharacterPipeline.

OPENING THE FILE
The file opens in 02 • CLASS GALLERY. Use the Scene selector at the top right to switch scenes.
01 • MODULAR WORKBENCH contains the editable source assets and the single source armature.
Select RIG • BRIMBOUND and change Object Properties > Custom Properties > loadout (0–10).
0 Peasant, 1 Worker, 2 Knight, 3 Knight II, 4 Priest, 5 Dark Priest,
6 Fire Mage, 7 Ice Mage, 8 Archer, 9 Gunman, 10 Ninja.
Drivers update equipment visibility; no addon or script execution is required.
All preview rigs share the SAME armature datablock; all preview meshes link source mesh data.

SCENES
01 modular source workbench; 02 gallery; 03 gameplay courtyard with close/normal/far cameras;
04 twelve animated test rigs; 05 blue/red Worker front/rear cloth proof; 06 silhouette test;
07 four anime expressions and front/profile clay head studies.
Use Numpad 0 for the camera, Z > Material Preview for the palette.

RIG AND ATTACHMENTS
26 bones. Root at ground, Z up, face direction -Y, scale in metres.
Attachment bones: weapon_r, weapon_l, headgear, back, hip_l, hip_r.
Source meshes use the armature origin and identity object transforms, including rigid items.
Rigid items are weighted 100% to their socket; deforming clothing has blended bone weights.
Weapons use a consistent nominal 40 mm hand grip. All hats fit the same head.
The bare scalp top is 1.246 m, with a 0.535 m head; hair adds approximately 7 cm.
The pike is approximately 2.24 m long. Base anatomy is deliberately slim.

MATERIALS AND UVS
M_TEAM is a simple Principled base color; edit it to recolor every team accent together.
M_TEAM_RED is used only for the team proof scene. The actual asset has one team slot.
Packed 2048 x 2048 body/equipment base-color, roughness, metallic and flat normal maps are supplied externally.
Body/equipment UV_Atlas intentionally shares palette tiles; UV_Unique is a per-object repainting unwrap.
The HEAD uses a unique full 0–1 UV unwrap in both channels. Four baked 2048 head textures are supplied:
BRIMBOUND_Face_Smile_2048.png, BRIMBOUND_Face_Joy_2048.png,
BRIMBOUND_Face_Determined_2048.png and BRIMBOUND_Face_Surprise_2048.png.
Select the HEAD object's material slot and choose M_FACE_Smile, M_FACE_Joy,
M_FACE_Determined or M_FACE_Surprise. All expressions use the same head mesh and UVs.
Gallery previews use Object-linked material slots so expression choices can differ per character.
The generated expression source sheet and prompts are included. Head maps are baked in Blender.
The low-noise finish uses actual mesh bevels; the normal map is flat, not a high-poly bake.
Assets use standard image textures and Principled BSDF, with modest emissive magic accents.

ANIMATIONS
12 actions named TEST • ... at 24 fps: Idle, Run, Sword Attack, Axe Swing, Pike Thrust,
Staff Cast, Bow Aim, Blunderbuss Fire, Dagger Attack, Arms Raised, Arms Forward, Deep Knee Bend.
These are FK validation animations/poses. They are not production combat timings.
Choose an Action in the Action Editor on the source rig to test another loadout.
Scene 04 assigns one test per rig; press Space to play.

EXPORT
The delivery includes FBX loadouts, individual modular meshes and skeleton test clips.
Each assembled loadout is one skinned mesh with 2–4 material slots: shared palette, face,
team color when present, and magic when present. Individual modules retain logical source slots.
Use the same imported skeleton for every module and loadout. Do not add leaf bones.
The FBX files include unit metadata; inspect a 1.246 m bare scalp height on import.
Map M_TEAM to a vector-parameter material instance in the target engine.
Source orientation is -Y forward / Z up. Configure the game's forward axis during import.
Unreal project import, collision/physics, LOD generation and engine retarget settings remain
integration work that must be verified against the receiving project's requirements.

QA
See qa/asset_audit.json for actual triangle counts, weights, UVs and topology checks.
Every equipment mesh is separate; multipart closed surfaces have intentional intersections
at seams and assembly contacts. They are not intended as a 3D printing solid.
See the renders for front/rear Worker checks, gameplay distances, monochrome silhouettes,
class variants, deformation tests, four expressions, and head profiles.
'''
textblock=bpy.data.texts.new('START HERE • BRIMBOUND');textblock.write(README)
(OUT/'README.md').write_text(README,encoding='utf-8')

for datablock in list(bpy.data.meshes):
    if datablock.users==0:bpy.data.meshes.remove(datablock)
for datablock in list(bpy.data.materials):
    if datablock.users==0 and not datablock.use_fake_user:bpy.data.materials.remove(datablock)
for im in IMAGES.values():im.filepath='//textures/'+Path(im.filepath).name

print('PHASE: saving assembled project',flush=True)
for scene in [S,G,P,A]:
    scene.render.image_settings.color_mode='RGBA';scene.render.film_transparent=False
bpy.context.window.scene=G
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
            area.spaces.active.overlay.show_overlays=False
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BRIMBOUND_Modular_Characters.blend'))
G.render.filepath=str(OUT/'renders'/'01_Class_Gallery.png')
bpy.ops.render.render(write_still=True,scene=G.name)
A.render.filepath=str(OUT/'renders'/'05_Animation_Tests.png')
bpy.ops.render.render(write_still=True,scene=A.name)
P.render.filepath=str(OUT/'renders'/'02_Gameplay_Normal.png')
for ob in pc.objects:
    if ob.get('overlay_camera'):ob.hide_render=ob['overlay_camera']!=normal_cam.name
bpy.ops.render.render(write_still=True,scene=P.name)
P.camera=far_cam;P.render.filepath=str(OUT/'renders'/'03_Gameplay_Far.png')
for ob in pc.objects:
    if ob.get('overlay_camera'):ob.hide_render=ob['overlay_camera']!=far_cam.name
bpy.ops.render.render(write_still=True,scene=P.name)
P.camera=close_cam;P.render.filepath=str(OUT/'renders'/'04_Gameplay_Close.png')
for ob in pc.objects:
    if ob.get('overlay_camera'):ob.hide_render=ob['overlay_camera']!=close_cam.name
bpy.ops.render.render(write_still=True,scene=P.name)
P.camera=normal_cam
for ob in pc.objects:
    if ob.get('overlay_camera'):ob.hide_render=ob['overlay_camera']!=normal_cam.name
W.render.filepath=str(OUT/'renders'/'06_Worker_Cloth_and_Teams.png')
bpy.ops.render.render(write_still=True,scene=W.name)
H.render.filepath=str(OUT/'renders'/'07_Silhouettes.png')
bpy.ops.render.render(write_still=True,scene=H.name)
F.render.filepath=str(OUT/'renders'/'08_Anime_Faces_and_Head_Form.png')
bpy.ops.render.render(write_still=True,scene=F.name)
bpy.context.window.scene=G
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BRIMBOUND_Modular_Characters.blend'))
print('BUILD COMPLETE',flush=True)
