"""Bake the procedural practice scenery into a new, editable Unreal map."""
# SPDX-License-Identifier: MIT
import unreal

destination = '/Game/Maps/Practice_Editable'
if unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError(f'{destination} already exists. Preserve or rename it before baking again.')

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if not unreal.OneClickPilotGameMode.bake_practice_scenery(world):
    raise RuntimeError('Could not bake practice scenery into the editor world')
if not unreal.EditorLoadingAndSavingUtils.save_map(world, destination):
    raise RuntimeError(f'Could not save {destination}')
unreal.log(f'Saved editable practice scenery to {destination}')
