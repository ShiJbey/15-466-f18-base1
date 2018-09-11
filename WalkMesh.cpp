#include "WalkMesh.hpp"
#include "read_chunk.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <limits>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <cstddef>
#include <stdexcept>
#include <string>

WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), triangles(triangles_) {
	//TODO: construct next_vertex map
	for (uint32_t i = 0; i < triangles.size(); i++) {
		next_vertex[glm::vec2(triangles[i].x, triangles[i].y)] = triangles[i].z;
		next_vertex[glm::vec2(triangles[i].y, triangles[i].z)] = triangles[i].x;
		next_vertex[glm::vec2(triangles[i].z, triangles[i].x)] = triangles[i].y;
	}
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	WalkPoint closest;

	float closest_distance = std::numeric_limits<float>::max();

	//TODO: iterate through triangles'
	for (uint32_t i = 0; i < triangles.size(); i++) {
		//TODO: for each triangle, find closest point on triangle to world_point

		WalkPoint current;
		current.triangle = triangles[i];
		current.weights = to_barycentric(world_point, current.triangle);

		//TODO: for each triangle, find closest point on triangle to world_point
		glm::vec3 closest_world_point = this->world_point(current);

		glm::vec3 difference = world_point - closest_world_point;
		float distance = glm::length(difference);

		if (distance < closest_distance) {
			//TODO: if point is closest, closest.triangle gets the current triangle, closest.weights gets the barycentric coordinates
			closest = current;
		}
	}
	
	
	return closest;
}

// This code is corrowed from:
// https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
glm::vec3 WalkMesh::to_barycentric(glm::vec3 const &world_point, const glm::uvec3 &triangle) const {
	glm::vec3 n = get_normal(triangle);

	glm::vec3 u = vertices[triangle.y] - vertices[triangle.x];
	glm::vec3 v = vertices[triangle.z] - vertices[triangle.x];
	glm::vec3 w = world_point - vertices[triangle.x];
	
	float gamma = (glm::dot(n, glm::cross(u, w))) / glm::dot(n, n);
	float beta = (glm::dot(n, glm::cross(w, v))) / glm::dot(n, n);
	float alpha = 1 - gamma - beta;
	
	return glm::vec3(alpha, beta, gamma);
}

// This code is adapted from:
// https://math.stackexchange.com/questions/305642/how-to-find-surface-normal-of-a-triangle
// It returns the non normalized normal
glm::vec3 WalkMesh::get_normal(const glm::uvec3 &triangle) const {
	glm::vec3 u = vertices[triangle.y] - vertices[triangle.x];
	glm::vec3 v = vertices[triangle.z] - vertices[triangle.x];
	return glm::cross(u, v);
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
	//TODO: project step to barycentric coordinates to get weights_step
	glm::vec3 world_pos = this->world_point(wp);
	glm::vec3 next_world_pos = world_pos + step;

	glm::vec3 weights_step = to_barycentric(next_world_pos, wp.triangle) - wp.weights;


	//TODO: when does wp.weights + t * weights_step cross a triangle edge?
	float t = 1.0f;

	glm::vec3 new_weights = wp.weights + t * weights_step;

	if (new_weights.x >= 0 && new_weights.y >= 0 && new_weights.z >= 0) {
		// We have not crossed the triangle and the character may move
		wp.weights = wp.weights + t * weights_step;
	} else {
		glm::vec3 w = next_world_pos - vertices[wp.triangle.x];
		glm::vec3 u = vertices[wp.triangle.y] - vertices[wp.triangle.x];
		glm::vec3 v = vertices[wp.triangle.z] - vertices[wp.triangle.x];
		glm::vec3 BC = vertices[wp.triangle.z] - vertices[wp.triangle.y];
		glm::vec3 truncated_weights = wp.weights;

		// https://math.stackexchange.com/questions/1092912/find-closest-point-in-triangle-given-barycentric-coordinates-outside/2340013#2340013
		// Case 1:
		if (new_weights.x >= 0 && new_weights.y < 0) {
			if (new_weights.z < 0 && glm::dot(w, u) > 0.0f) {
				truncated_weights.y = std::min(1.0f, glm::dot(w, u) / glm::dot(u, u));
				truncated_weights.z = 0;
			} else {
				truncated_weights.y = 0;
				truncated_weights.z = std::min(1.0f, std::max(0.0f, glm::dot(w, v) / glm::dot(v, v)));
			}
			truncated_weights.x = 1 -truncated_weights.y - truncated_weights.z;
		}

		// Case 2:
		else if (new_weights.y >= 0 && new_weights.z < 0) {
			if (new_weights.x < 0 && glm::dot(w, u) > 0.0f) {
				truncated_weights.z = std::min(1.0f, glm::dot(w, u) / glm::dot(u, u));
				truncated_weights.x = 0;
			} else {
				truncated_weights.z = 0;
				truncated_weights.x = std::min(1.0f, std::max(0.0f, glm::dot(w, BC) / glm::dot(BC, BC)));
			}
			truncated_weights.y = 1 - truncated_weights.x - truncated_weights.z;
		}

		// Case 3:
		else if (new_weights.z >= 0 && new_weights.x < 0) {
			if (new_weights.y < 0 && glm::dot(w, v) > 0.0f) {
				truncated_weights.x = std::min(1.0f, glm::dot(w, v) / glm::dot(v, v));
				truncated_weights.y = 0;
			} else {
				truncated_weights.x = 0;
				truncated_weights.y = std::min(1.0f, std::max(0.0f, glm::dot(w, BC) / glm::dot(BC, BC)));
			}
			truncated_weights.z = 1 - truncated_weights.y - truncated_weights.x;
		}

		//glm::vec3 adjusted_world_pos = world_point(wp);

		// Check if there is a triangle on the other side of this edge
		if (truncated_weights.x == 0) {
			// We have gone over side BC, check for another triangle
			std::unordered_map< glm::uvec2, uint32_t >::const_iterator it = next_vertex.find(glm::vec2(wp.triangle.y, wp.triangle.z));
			if (it != next_vertex.end()) {
				// Set this triangle as the current triangle
				wp.triangle = glm::uvec3(wp.triangle.y, wp.triangle.z, it->second);
				wp.weights = to_barycentric(next_world_pos, wp.triangle);
			}
			else {
				wp.weights = truncated_weights;
			}
		} else if (truncated_weights.y == 0) {
			// We have gone over side BC, check for another triangle
			std::unordered_map< glm::uvec2, uint32_t >::const_iterator it = next_vertex.find(glm::vec2(wp.triangle.z, wp.triangle.x));
			if (it != next_vertex.end()) {
				// Set this triangle
				wp.triangle = glm::uvec3(wp.triangle.z, wp.triangle.x, it->second);
				wp.weights = to_barycentric(next_world_pos, wp.triangle);
			} else {
				wp.weights = truncated_weights;
			}
		} else if (truncated_weights.z == 0) {
			// We have gone over side BC, check for another triangle
			std::unordered_map< glm::uvec2, uint32_t >::const_iterator it = next_vertex.find(glm::vec2(wp.triangle.x, wp.triangle.y));
			if (it != next_vertex.end()) {
				// Set this triangle
				wp.triangle = glm::uvec3(wp.triangle.x, wp.triangle.y, it->second);
				wp.weights = to_barycentric(next_world_pos, wp.triangle);
			} else {
				wp.weights = truncated_weights;
			}
		}

	}

	/*
	if (t >= 1.0f) { //if a triangle edge is not crossed
		//TODO: wp.weights gets moved by weights_step, nothing else needs to be done.

	} else { //if a triangle edge is crossed
		//TODO: wp.weights gets moved to triangle edge, and step gets reduced
		//if there is another triangle over the edge:
			//TODO: wp.triangle gets updated to adjacent triangle
			//TODO: step gets rotated over the edge
		//else if there is no other triangle over the edge:
			//TODO: wp.triangle stays the same.
			//TODO: step gets updated to slide along the edge
	}
	*/
}
