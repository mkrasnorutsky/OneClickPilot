"""Create minimal original materials and the practice map; run inside UE Editor."""
import unreal
assets=unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory('/Game/Materials')
unreal.EditorAssetLibrary.make_directory('/Game/Maps')
for name,color in {'M_Runway':(.045,.045,.05),'M_White':(.85,.85,.8),'M_Panel':(.025,.03,.035),'M_TaxiLine':(.9,.58,.04),'M_Windsock':(.85,.17,.025),'M_Field':(.19,.26,.09),'M_Brick':(.38,.19,.12),'M_Roof':(.19,.075,.044),'M_Slate':(.065,.08,.09),'M_Glass':(.035,.075,.095),'M_Sand':(.48,.39,.24),'M_Bark':(.13,.075,.038),'M_LeavesLight':(.11,.22,.043),'M_LeavesDark':(.035,.085,.022),'M_Paving':(.32,.31,.28),'M_Pasture':(.18,.25,.08),'M_Crop':(.28,.29,.10),'M_Hedge':(.052,.105,.024)}.items():
    path='/Game/Materials/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path):continue
    material=assets.create_asset(name,'/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
    expression=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
    expression.set_editor_property('constant',unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(expression,'',unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant)
    roughness.set_editor_property('r', .9)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path)
# Runtime-created instances need the instanced shader permutation in the cook.
for name in ('M_Brick', 'M_White', 'M_Roof', 'M_Slate', 'M_Glass', 'M_Runway', 'M_Field', 'M_Bark', 'M_LeavesLight', 'M_LeavesDark', 'M_Paving', 'M_Pasture', 'M_Crop', 'M_Hedge'):
    material = unreal.EditorAssetLibrary.load_asset('/Game/Materials/' + name)
    if not material.get_editor_property('used_with_instanced_static_meshes'):
        unreal.MaterialEditingLibrary.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
water_path = '/Game/Materials/M_CoastalWater'
if not unreal.EditorAssetLibrary.does_asset_exist(water_path):
    material = assets.create_asset('M_CoastalWater', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    position = edit.create_material_expression(material, unreal.MaterialExpressionWorldPosition)
    time = edit.create_material_expression(material, unreal.MaterialExpressionTime)
    ripples = edit.create_material_expression(material, unreal.MaterialExpressionCustom)
    ripples.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = [unreal.CustomInput(), unreal.CustomInput()]
    for entry, name in zip(inputs, ('Position', 'Seconds')):
        entry.set_editor_property('input_name', name)
    ripples.set_editor_property('inputs', inputs)
    ripples.set_editor_property('code', """
        float2 p = Position.xy * 0.01;
        float a = dot(p, float2(1.2, 2.7)) + Seconds * 1.1;
        float b = dot(p, float2(-2.1, 0.8)) - Seconds * 0.8;
        return normalize(float3(0.08 * cos(a) - 0.05 * cos(b),
                                0.11 * cos(a) + 0.05 * cos(b), 1.0));
    """)
    edit.connect_material_expressions(position, '', ripples, 'Position')
    edit.connect_material_expressions(time, '', ripples, 'Seconds')
    edit.connect_material_property(ripples, '', unreal.MaterialProperty.MP_NORMAL)
    color = edit.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant', unreal.LinearColor(.018, .082, .11, 1))
    edit.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = edit.create_material_expression(material, unreal.MaterialExpressionConstant)
    roughness.set_editor_property('r', .24)
    edit.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(water_path)
ridge_path = '/Game/Materials/M_RidgeTerrain'
if not unreal.EditorAssetLibrary.does_asset_exist(ridge_path):
    material = assets.create_asset('M_RidgeTerrain', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    color = edit.create_material_expression(material, unreal.MaterialExpressionVertexColor)
    roughness = edit.create_material_expression(material, unreal.MaterialExpressionConstant)
    roughness.set_editor_property('r', .95)
    edit.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    edit.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(ridge_path)
grass_path = '/Game/Materials/M_CoastalGrass'
if not unreal.EditorAssetLibrary.does_asset_exist(grass_path):
    texture = unreal.EditorAssetLibrary.load_asset('/Game/Materials/T_CoastalGrass')
    if texture is None:
        raise RuntimeError('Missing bundled T_CoastalGrass texture')
    texture.set_editor_property('power_of_two_mode', unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    material = assets.create_asset('M_CoastalGrass', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    coords = edit.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate)
    coords.set_editor_property('u_tiling', 2116.0)
    coords.set_editor_property('v_tiling', 2116.0)
    sample = edit.create_material_expression(material, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property('texture', texture)
    edit.connect_material_expressions(coords, '', sample, 'UVs')
    edit.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    edit.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(grass_path)
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/Materials/M_Sky'):
    material=assets.create_asset('M_Sky','/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('two_sided',True)
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('is_sky',True)
    expression=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
    expression.set_editor_property('constant',unreal.LinearColor(.2,.45,.8,1))
    unreal.MaterialEditingLibrary.connect_material_property(expression,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset('/Game/Materials/M_Sky')
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/Materials/M_AircraftLamp'):
    material = assets.create_asset('M_AircraftLamp', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    parameter = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    parameter.set_editor_property('parameter_name', 'LampColor')
    parameter.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    unreal.MaterialEditingLibrary.connect_material_property(parameter, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset('/Game/Materials/M_AircraftLamp')
cockpit_path = '/Game/Aircraft/BP_OneClickPilotCockpit'
if not unreal.EditorAssetLibrary.does_asset_exist(cockpit_path):
    unreal.EditorAssetLibrary.make_directory('/Game/Aircraft')
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.OneClickPilotPawn.static_class())
    cockpit = assets.create_asset('BP_OneClickPilotCockpit', '/Game/Aircraft', unreal.Blueprint, factory)
    if cockpit is None or not unreal.EditorAssetLibrary.save_loaded_asset(cockpit):
        raise RuntimeError('Could not create the cockpit Blueprint')

if not unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/Practice'):
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/Practice')
unreal.log('OneClickPilot offline practice map and materials prepared')
