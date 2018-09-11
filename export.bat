blender --background --python meshes/export-meshes.py -- meshes/nyhm_reloaded.blend dist/nyhm.pnc

blender --background --python meshes/export-scene.py -- meshes/nyhm_reloaded.blend dist/nyhm.scene

blender --background --python meshes/export-walkmesh.py -- meshes/nyhm_reloaded.blend dist/nyhm.pnt