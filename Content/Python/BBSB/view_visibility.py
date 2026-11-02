"""Prepare material siblings and baked render triangles before PIE. Originals stay intact.
The game module invokes this for the loaded scene; new source materials need no actor list.
"""
import unreal,hashlib
E=unreal.MaterialEditingLibrary
lib=unreal.EditorAssetLibrary
folder='/Game/BBSB/Visibility'
def save(a):
    unreal.log('BBSB visibility saving '+a.get_path_name())
    assert lib.save_loaded_asset(a,False),a
    unreal.log('BBSB visibility saved '+a.get_path_name())
def connect(a,b,pin,output=''):
    if not output and isinstance(a,unreal.MaterialExpressionMaterialFunctionCall):output=str(E.get_material_expression_output_names(a)[0])
    assert E.connect_material_expressions(a,output,b,pin),(a,b,pin,output)
def function_call(owner,function,x,y):
    n=E.create_material_expression(owner,unreal.MaterialExpressionMaterialFunctionCall,x,y)
    assert n.set_material_function(function)
    return n
def prepare_scene():
    settings=lib.load_asset(folder+'/DA_ViewOcclusion')
    if not settings:return
    mf=lib.load_asset(folder+'/MF_PlayerViewVisibility')
    report={'variants':{},'skipped':[]}
    variants={key.get_path_name():value for key,value in settings.get_editor_property("Variants").items()}
    for copy in variants.values():
        if isinstance(copy,unreal.Material) and settings.prepare_masked_material(copy):save(copy)
    def variant(original):
        path=original.get_path_name()
        if path in variants:return variants[path]
        name=('MI_View_' if isinstance(original,unreal.MaterialInstanceConstant) else 'M_View_')+original.get_name()+'_'+hashlib.sha1(path.encode()).hexdigest()[:8]
        target=folder+'/Materials/'+name
        if isinstance(original,unreal.MaterialInstanceConstant):
            parent=variant(original.get_editor_property('parent'))
            if not parent:return None
            copy=lib.load_asset(target) if lib.does_asset_exist(target) else lib.duplicate_asset(path,target)
            assert copy;E.set_material_instance_parent(copy,parent);E.update_material_instance(copy);save(copy)
        elif isinstance(original,unreal.Material):
            if original.get_editor_property('material_domain')!=unreal.MaterialDomain.MD_SURFACE:return None
            blend=original.get_editor_property('blend_mode')
            if blend not in [unreal.BlendMode.BLEND_OPAQUE,unreal.BlendMode.BLEND_MASKED]:
                report['skipped'].append(path);return None
            unreal.log('BBSB visibility preparing '+path)
            copy=lib.load_asset(target) if lib.does_asset_exist(target) else lib.duplicate_asset(path,target);assert copy
            if any(str(n.get_editor_property('desc'))=='BBSB_ViewVisibility' for n in E.get_material_expressions(copy)):
                variants[path]=copy;report['variants'][path]=copy.get_path_name();return copy
            attributes=copy.get_editor_property('use_material_attributes')
            attributes_source=E.get_material_property_input_node(copy,unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES) if attributes else None
            source=E.get_material_property_input_node(copy,unreal.MaterialProperty.MP_OPACITY_MASK)
            source_output=E.get_material_property_input_node_output_name(copy,unreal.MaterialProperty.MP_OPACITY_MASK) if source else ''
            if not source:
                source=E.create_material_expression(copy,unreal.MaterialExpressionConstant,-350,400);source.set_editor_property('r',1.)
            call=function_call(copy,mf,-350,550)
            multiply=E.create_material_expression(copy,unreal.MaterialExpressionMultiply,-80,400);connect(source,multiply,'A',str(source_output));connect(call,multiply,'B')
            shadow_mask=E.create_material_expression(copy,unreal.MaterialExpressionMultiply,-80,650);connect(source,shadow_mask,'A',str(source_output));connect(call,shadow_mask,'B','ShadowVisibility')
            shadow=E.create_material_expression(copy,unreal.MaterialExpressionShadowReplace,150,400);connect(multiply,shadow,'Default');connect(shadow_mask,shadow,'Shadow')
            shadow.set_editor_property('desc','BBSB_ViewVisibility')
            if attributes:
                assert attributes_source,path
                if blend==unreal.BlendMode.BLEND_MASKED:
                    split=E.create_material_expression(copy,unreal.MaterialExpressionBreakMaterialAttributes,-600,350);connect(attributes_source,split,'Attributes')
                    mask_pin=next(s for s in E.get_material_expression_output_names(split) if 'opacity' in str(s).lower() and 'mask' in str(s).lower())
                    connect(split,multiply,'A',str(mask_pin));connect(split,shadow_mask,'A',str(mask_pin))
                setter=E.create_material_expression(copy,unreal.MaterialExpressionSetMaterialAttributes,380,400)
                setter.set_editor_property('attribute_set_types',[unreal.Guid(a=0x679FFB17,b=0x2BB5422C,c=0xAD520483,d=0x166E0C75)])
                pins=[str(s) for s in E.get_material_expression_input_names(setter)]
                mask_pin=next(s for s in pins if 'opacity' in s.lower() and 'mask' in s.lower());attributes_pin=next(s for s in pins if 'attrib' in s.lower())
                connect(attributes_source,setter,attributes_pin);connect(shadow,setter,mask_pin)
                assert E.connect_material_property(setter,'',unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
            else:assert E.connect_material_property(shadow,'',unreal.MaterialProperty.MP_OPACITY_MASK)
            copy.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
            copy.set_editor_property('used_with_instanced_static_meshes',True)
            copy.set_editor_property('used_with_skeletal_mesh',True)
            settings.prepare_masked_material(copy)
            E.recompile_material(copy);save(copy)
        else:raise RuntimeError(path)
        variants[path]=copy;report['variants'][path]=copy.get_path_name();return copy
    original_count=len(variants)
    for material in settings.scene_materials():
        while isinstance(material,unreal.MaterialInstanceDynamic):material=material.get_editor_property('parent')
        if not material or not material.get_path_name().startswith(('/Game/','/Engine/')) or material.get_path_name().startswith('/Game/BBSB/Visibility/'):continue
        variant(material)
    if len(variants)!=original_count:
        settings.set_editor_property('Variants',{lib.load_asset(path):copy for path,copy in variants.items()})
    geometry=settings.cache_loaded_geometry()
    if len(variants)!=original_count or geometry:save(settings)
    unreal.log('BBSB visibility prepared: '+str(len(variants))+' materials; '+str(geometry)+' geometry caches updated')
    return {'materials':len(variants),'new_geometry':geometry,'skipped':report['skipped']}
