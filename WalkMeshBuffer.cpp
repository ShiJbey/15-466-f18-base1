#include "WalkMeshBuffer.hpp"
#include "WalkMesh.hpp"

#include "read_chunk.hpp"
#include "GL.hpp"

#include <glm/glm.hpp>

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <cstddef>
#include <map>

WalkMeshBuffer::WalkMeshBuffer(std::string const &filename) {
    // Open the file
	std::ifstream file(filename, std::ios::binary);

    // Create vertex struct for extracting vertex data
    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
    };
	static_assert(sizeof(Vertex) == 3*4+3*4, "Vertex is packed.");
    std::vector< Vertex > vert_data;

    // Create triangle struct for extracting trangle data
    struct Triangle {
        glm::uvec3 verts;
    };
    static_assert(sizeof(Triangle) == 3 * 4, "Triangle is packed.");
    std::vector< Triangle > tri_data;

	// Keep a counter of the total amount of vertex and Triangle data read in
	GLuint vert_total = 0;
	GLuint tri_total = 0;

	if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".pnt") {
		
		read_chunk(file, "vert", &vert_data);
		vert_total = GLuint(vert_data.size());

		
		read_chunk(file, "tris", &tri_data);
		tri_total = GLuint(tri_data.size());
		
	} else {
		throw std::runtime_error("Unknown file type '" + filename + "'");
	}

	std::vector< char > strings;
	read_chunk(file, "str0", &strings);

	{
		struct IndexEntry {
			uint32_t name_begin, name_end;
			uint32_t vert_begin, tri_begin;
			uint32_t vert_end, tri_end;
		};
		static_assert(sizeof(IndexEntry) == 24, "Index entry should be packed");

		std::vector< IndexEntry > index;
		read_chunk(file, "idx0", &index);

		for (auto const &entry : index) {
			if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
				throw std::runtime_error("index entry has out-of-range name begin/end");
			}
			if (!(entry.vert_begin <= entry.vert_end && entry.vert_end <= vert_total)) {
				throw std::runtime_error("index entry has out-of-range vertex start/count");
			}
			if (!(entry.tri_begin <= entry.tri_end && entry.tri_end <= tri_total)) {
				throw std::runtime_error("index entry has out-of-range triangle start/count");
			}

            // Start Constructing WalkMeshes
            std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);

            // Create vector of glm::vec3 vertex positions for this mesh
            std::vector< glm::vec3 > vertices;
            std::vector< glm::vec3 > normals;
            for (uint32_t i = entry.vert_begin; i < entry.vert_end; i++) {
                vertices.push_back(vert_data[i].Position);
                normals.push_back(vert_data[i].Normal);
            }

            // Create a vector of glm::uvec3 triangles
            std::vector< glm::uvec3 > triangles;
            for (uint32_t i = entry.tri_begin; i < entry.tri_end; i++) {
                triangles.push_back(tri_data[i].verts);
            }

            WalkMesh mesh(vertices, triangles);
            mesh.vertex_normals = normals;

            bool inserted = meshes.insert(std::make_pair(name, mesh)).second;
			if (!inserted) {
				std::cerr << "WARNING: mesh name '" + name + "' in filename '" + filename + "' collides with existing mesh." << std::endl;
			}
		}
	}

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in mesh file '" << filename << "'" << std::endl;
	}
}

const WalkMesh *WalkMeshBuffer::lookup(std::string const &name) const {
    auto f = meshes.find(name);
	if (f == meshes.end()) {
		throw std::runtime_error("Looking up mesh '" + name + "' that doesn't exist.");
	}
	return &(f->second);
}