"""Build the local Australian Airfield from a Fab installation."""
# SPDX-License-Identifier: MIT
import math

import unreal

source = '/Game/Maps/Practice_Editable'
target = '/Game/Maps/Practice_AustralianAirfield'
demo = '/Game/RuralAustralia/Maps/RuralAustralia_Example_03'
if not unreal.EditorAssetLibrary.does_asset_exist(demo):
    raise RuntimeError('Install Rural Australia from Fab into Content/RuralAustralia first.')

if not unreal.EditorAssetLibrary.does_asset_exist(target):
    assert unreal.EditorAssetLibrary.duplicate_asset(source, target)
    world = unreal.EditorLoadingAndSavingUtils.load_map(target)
    assert world
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    bearing = math.radians(20)
    retained = 0
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        pos = actor.get_actor_location()
        along = (pos.x * math.cos(bearing) + pos.y * math.sin(bearing)) / 100
        across = (-pos.x * math.sin(bearing) + pos.y * math.cos(bearing)) / 100
        if label.startswith('Runway 02'):
            actor.tags = ['OneClickPilot.BakedScenery']
        elif label != 'Airfield ground' and not (
            label == 'Scenery detail' and 0 <= along <= 1036 and -160 <= across <= 30
        ):
            assert actors.destroy_actor(actor), label
            continue
        actor.set_folder_path('Practice Airfield')
        retained += 1

    offset = unreal.Transform(location=unreal.Vector(65000, 92500, 110))
    sublevel = unreal.EditorLevelUtils.add_level_to_world_with_transform(
        world, demo, unreal.LevelStreamingAlwaysLoaded, offset)
    assert sublevel
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, target)
    unreal.log(f'AIRFIELD saved {target} with {retained} airport actors and the Rural Australia demo')

if not unreal.EditorAssetLibrary.does_asset_exist('/Game/RuralAustralia/Maps/RuralAustralia_Tile'):
    source = '/Game/RuralAustralia/Maps/RuralAustralia_Example_03'
    target = '/Game/RuralAustralia/Maps/RuralAustralia_Tile'
    assert unreal.EditorAssetLibrary.duplicate_asset(source, target)
    world = unreal.EditorLoadingAndSavingUtils.load_map(target)
    assert world
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    keep = {'Landscape', 'LandscapeSplineActor', 'InstancedFoliageActor', 'StaticMeshActor', 'Spline_Fence_C'}
    for actor in actors.get_all_level_actors():
        if actor.get_class().get_name() not in keep:
            assert actors.destroy_actor(actor), actor.get_actor_label()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, target)
    unreal.log(f'TILE saved {target}')

folder = '/Game/RuralAustralia/Local'
name = 'M_AirfieldDistanceGround'
assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
material = assets.load_asset(f'{folder}/{name}') if assets.does_asset_exist(f'{folder}/{name}') else None
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
else:
    edit.delete_all_material_expressions(material)

def node(kind, **properties):
    expression = edit.create_material_expression(material, kind)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression

def wire(source, target, socket, output=''):
    assert edit.connect_material_expressions(source, output, target, socket)

def constant(value):
    return node(unreal.MaterialExpressionConstant, r=value)

position = node(unreal.MaterialExpressionWorldPosition)
xy = node(unreal.MaterialExpressionComponentMask, r=True, g=True)
wire(position, xy, '')

def sample(texture, metres):
    scale = node(unreal.MaterialExpressionMultiply, const_b=1.0 / (metres * 100.0))
    wire(xy, scale, 'A')
    texture = assets.load_asset('/Game/RuralAustralia/Landscape/Textures/' + texture)
    result = node(unreal.MaterialExpressionTextureSample, texture=texture)
    wire(scale, result, 'UVs')
    return result

near = sample('T_Ground_Grass_01_CA', 9.0)
far = sample('T_Distant_Ground_01_CA', 1320.0)
macro = sample('T_Distant_Ground_02_CA', 3970.0)
variation = node(unreal.MaterialExpressionLinearInterpolate, const_alpha=0.38)
wire(far, variation, 'A', 'RGB')
wire(macro, variation, 'B', 'RGB')
depth = node(unreal.MaterialExpressionPixelDepth)
fade = node(unreal.MaterialExpressionSmoothStep, const_min=14000.0, const_max=78000.0)
wire(depth, fade, 'Value')
colour = node(unreal.MaterialExpressionLinearInterpolate)
wire(near, colour, 'A', 'RGB')
tint = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.72, 0.40, 0.20, 1.0))
warm_ground = node(unreal.MaterialExpressionMultiply)
wire(variation, warm_ground, 'A')
wire(tint, warm_ground, 'B')
wire(warm_ground, colour, 'B')
wire(fade, colour, 'Alpha')
assert edit.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
assert edit.connect_material_property(constant(0.95), '', unreal.MaterialProperty.MP_ROUGHNESS)
edit.layout_material_expressions(material)
edit.recompile_material(material)
assert assets.save_loaded_asset(material)

path = '/Game/Maps/Practice_AustralianAirfield'
world = unreal.EditorLoadingAndSavingUtils.load_map(path)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ground, = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == 'Airfield ground']
ground.static_mesh_component.set_material(0, material)
assert unreal.EditorLoadingAndSavingUtils.save_map(world, path)
unreal.log('DISTANT GROUND saved')

landscape = assets.load_asset('/Game/RuralAustralia/Landscape/MI_Landscape_Example_03')
switches = edit.get_static_switch_parameter_names(landscape)
unreal.log(f'LANDSCAPE SWITCHES {list(switches)}')
for switch in switches:
    label = str(switch).lower()
    if label == 'overlay enabled':
        before = edit.get_material_instance_static_switch_parameter_value(landscape, switch)
        if before:
            edit.set_material_instance_static_switch_parameter_value(landscape, switch, False)
        after = edit.get_material_instance_static_switch_parameter_value(landscape, switch)
        unreal.log(f'DISTANCE SWITCH {switch}: {before} -> {after}')
assert assets.save_loaded_asset(landscape)
