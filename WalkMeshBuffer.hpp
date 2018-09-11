#pragma once
#include "WalkMesh.hpp"

#include <map>


// This struct is designed after the MeshBuffer struct, but it
// only coverns itself with WalkMeshes.
// The buffer will allow us to have multiple walk meshes in a
// level and define different movement regions for players/NPCs
struct WalkMeshBuffer {
    // Internals
    std::map< std::string, WalkMesh > meshes;

    WalkMeshBuffer(std::string const &filename);

    const WalkMesh *lookup(std::string const &name) const;
};