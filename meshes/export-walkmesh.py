#!/usr/bin/env python

#based on export-meshes.py

# This script is only for exporting the walk meshes from a blender scene
# unlike the original exporter, this exports the same data in all cases
# and does not have an option to only export the edges

import sys,re

args = []
for i in range(0,len(sys.argv)):
	if sys.argv[i] == '--':
		args = sys.argv[i+1:]

if len(args) != 2:
	print("\n\nUsage:\nblender --background --python export-meshes.py -- <infile.blend>[:layer] <outfile.p[n][c][t][l]>\nExports the meshes referenced by all objects in layer (default 1) to a binary blob, indexed by the names of the objects that reference them. If 'l' is specified in the file extension, only mesh edges will be exported.\n")
	exit(1)

infile = args[0]
layer = 1
m = re.match(r'^(.*):(\d+)$', infile)
if m:
	infile = m.group(1)
	layer = int(m.group(2))
outfile = args[1]

assert layer >= 1 and layer <= 20

print("Will export walk mesh referenced from layer " + str(layer) + " of '" + infile + "' to '" + outfile + "'.")

class FileType:
	def __init__(self, magic):
		self.magic = magic
		self.vertex_bytes = 0
		self.vertex_bytes += 3 * 4
		self.vertex_bytes += 3 * 4
		self.tri_bytes = 3 * 4


# We only have one supported file type for this exporter
filetype = FileType(b"pnt.")


import bpy
import struct
import argparse

bpy.ops.wm.open_mainfile(filepath=infile)

# Find only the meshes with names containing "WalkMesh"
to_write = set()
for obj in bpy.data.objects:
	if obj.layers[layer-1] and ("WalkMesh" in obj.name):
		to_write.add(obj.data)

# Byte string to hold the data (pos and norms) for all vertices
vert_data = b''
# Byte string to hold the data (a, b, c) for all tris
tri_data = b''
# Mesh name
strings = b''
# Offsets to the data, names, and tris for each mesh
index = b''

vertex_count = 0
tri_count = 0
for obj in bpy.data.objects:
	if obj.data in to_write:
		to_write.remove(obj.data)
	else:
		continue

	mesh = obj.data
	name = mesh.name

	print("Writing '" + name + "'...")
	if bpy.context.mode == 'EDIT':
		bpy.ops.object.mode_set(mode='OBJECT') #get out of edit mode (just in case)

	#make sure object is on a visible layer:
	bpy.context.scene.layers = obj.layers
	#select the object and make it the active object:
	bpy.ops.object.select_all(action='DESELECT')
	obj.select = True
	bpy.context.scene.objects.active = obj

	bpy.ops.object.convert(target='MESH')

	#subdivide object's mesh into triangles:
	bpy.ops.object.mode_set(mode='EDIT')
	bpy.ops.mesh.select_all(action='SELECT')
	bpy.ops.mesh.quads_convert_to_tris(quad_method='BEAUTY', ngon_method='BEAUTY')
	bpy.ops.object.mode_set(mode='OBJECT')

	#compute normals (respecting face smoothing):
	mesh.calc_normals_split()

	#record mesh name, start position and vertex count in the index:
	name_begin = len(strings) # Start position for this mesh's name
	strings += bytes(name, "utf8") # Stores the mesh name in the strings buffer
	name_end = len(strings) # Get the end position of the string
	index += struct.pack('I', name_begin) # Add index entries for the start and end of the mesh name
	index += struct.pack('I', name_end)

	index += struct.pack('I', vertex_count) # Add index value for the index of the first vertex in the mesh in the index_data
	index += struct.pack('I', tri_count) # Add the index of the start position for the triangles for this mesh
	#...count will be written below

	#write the mesh triangles:
	for poly in mesh.polygons:
		# Check that polys only have three verts
		assert(len(poly.loop_indices) == 3)
		for i in range(0,3):
			# Make sure that the loop refers to the proper vert
			assert(mesh.loops[poly.loop_indices[i]].vertex_index == poly.vertices[i])
			loop = mesh.loops[poly.loop_indices[i]]
			vertex = mesh.vertices[loop.vertex_index]
			# Add the vertex position data
			for x in vertex.co:
				vert_data += struct.pack('f', x)
			for x in loop.normal:
				vert_data += struct.pack('f', x)
			# For each index in the polygon add its vertex index to the tri data
			tri_data += struct.pack('I', loop.vertex_index)
	# Increment the counts
	vertex_count += len(mesh.polygons) * 3
	tri_count += len(mesh.polygons)


	index += struct.pack('I', vertex_count) #vertex_end
	index += struct.pack('I', tri_count) # save the index of the last tri to be added

#check that we wrote as much data as anticipated:
assert(vertex_count * filetype.vertex_bytes == len(vert_data))
assert(tri_count * filetype.tri_bytes == len(tri_data))



#write the data chunk and index chunk to an output blob:
blob = open(outfile, 'wb')
#first chunk: the vertex data
blob.write(struct.pack('4s', b'vert')) #type
blob.write(struct.pack('I', len(vert_data))) #length
blob.write(vert_data)
#second chunk: the tri data
blob.write(struct.pack('4s', b'tris')) #type
blob.write(struct.pack('I', len(tri_data))) #length
blob.write(tri_data)
#third chunk: the strings
blob.write(struct.pack('4s',b'str0')) #type
blob.write(struct.pack('I', len(strings))) #length
blob.write(strings)
#fourth chunk: the index
blob.write(struct.pack('4s',b'idx0')) #type
blob.write(struct.pack('I', len(index))) #length
blob.write(index)
wrote = blob.tell()
blob.close()

print("Wrote " + str(wrote) + " bytes [== " + str(len(vert_data)+8) + " bytes of vert data + " + " bytes [== " + str(len(tri_data)+8) + " bytes of tri data + " + str(len(strings)+8) + " bytes of strings + " + str(len(index)+8) + " bytes of index] to '" + outfile + "'")