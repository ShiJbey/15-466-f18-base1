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
	for (uint32_t i = 0; i < triangles.size(); i++) {
		next_vertex.insert({glm::vec2(triangles[i].x, triangles[i].y), triangles[i].z});
		next_vertex.insert({glm::vec2(triangles[i].y, triangles[i].z), triangles[i].x});
		next_vertex.insert({glm::vec2(triangles[i].z, triangles[i].x), triangles[i].y});
	}
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	
	WalkPoint closest;
	float closest_distance = std::numeric_limits<float>::max();

	auto in_triangle = [](WalkPoint const &walkpoint) {
		return walkpoint.weights.x >= 0 && walkpoint.weights.y >= 0 && walkpoint.weights.z >= 0;
	};

	//TODO: iterate through triangles'
	for (uint32_t i = 0; i < triangles.size(); i++) {
		// Just find a triangle where that this point falls inside
		//printf("Current: Triangle <%d, %d, %d>: \n", triangles[i].x, triangles[i].y, triangles[i].z);

		WalkPoint projected_point;
		projected_point.triangle = triangles[i];
		projected_point.weights = to_barycentric(world_point, projected_point.triangle);

		
		if (in_triangle(projected_point)) {
			//TODO: if point is closest, closest.triangle gets the current triangle, closest.weights gets the barycentric coordinates
			//printf("=== MATCH ===: Triangle <%d, %d, %d>: \n", triangles[i].x, triangles[i].y, triangles[i].z);
			closest.triangle = triangles[i];
			closest.weights = to_barycentric(world_point, triangles[i]);
			
			/*
			printf("Triangle <%d, %d, %d>: \n", triangles[i].x, triangles[i].y, triangles[i].z);
			printf("Its verts have the following coords:\n");
			printf("<%f, %f, %f>\n",vertices[projected_point.triangle.x].x, vertices[projected_point.triangle.x].y, vertices[projected_point.triangle.x].z);
			printf("<%f, %f, %f>\n",vertices[projected_point.triangle.y].x, vertices[projected_point.triangle.y].y, vertices[projected_point.triangle.y].z);
			printf("<%f, %f, %f>\n",vertices[projected_point.triangle.z].x, vertices[projected_point.triangle.z].y, vertices[projected_point.triangle.z].z);
			*/
		}
	}

	if (closest.triangle.x == -1U) {
		std::cerr << "This point is not above any point on the nav mesh" << std::endl;
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
	
	float gamma = glm::dot(glm::cross(u, w), n) / glm::dot(n, n);
	float beta = glm::dot(glm::cross(w, v), n) / glm::dot(n, n);
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

	//printf("In Triangle: <%d, %d, %d>\n", wp.triangle.x, wp.triangle.y, wp.triangle.z);

	glm::vec3 world_pos = this->world_point(wp);

	//printf("At Bary centric: <%f, %f, %f>\n", wp.weights.x, wp.weights.y, wp.weights.z);

	glm::vec3 next_world_pos = world_pos + step;

	glm::vec3 next_world_pos_bary = to_barycentric(next_world_pos, wp.triangle);

	//printf("Wants to move to: <%f, %f, %f>\n", next_world_pos_bary.x, next_world_pos_bary.y, next_world_pos_bary.z);

	glm::vec3 step_barycentric = to_barycentric(next_world_pos, wp.triangle) - wp.weights;

	//printf("Step Change: <%f, %f, %f>\n", step_barycentric.x, step_barycentric.y, step_barycentric.z);

	WalkPoint predicted_walkpoint;
	predicted_walkpoint.triangle = wp.triangle;
	predicted_walkpoint.weights = next_world_pos_bary;
	glm::vec3 walkpoint_world_pos = this->world_point(predicted_walkpoint);
	//printf("Results in world position: <%f, %f, %f>\n", walkpoint_world_pos.x, walkpoint_world_pos.y, walkpoint_world_pos.z);

	if (next_world_pos_bary.x >= 0 && next_world_pos_bary.y >= 0 && next_world_pos_bary.z >= 0) {
		// We have not crossed the triangle and the character may move
		wp.weights = next_world_pos_bary;
	} else {
		
		bool adjacent_triangle_found = false;

		if (next_world_pos_bary.x < 0.0f) {
			// check if there is another triangle with points y and z
			auto it = next_vertex.find(glm::uvec2(wp.triangle.z, wp.triangle.y));
			if (it != next_vertex.end()) {
				// There is another triangle move to it
				//printf("==Found Adj Triangle: <%d, %d, %d>\n", wp.triangle.z, wp.triangle.y, it->second);
				wp.triangle = glm::uvec3(wp.triangle.z, wp.triangle.y, it->second);
				wp.weights = to_barycentric(next_world_pos, wp.triangle);
				adjacent_triangle_found = true;
			}
		}

		if (next_world_pos_bary.y < 0.0f) {
			// check if there is another triangle with points y and z
			auto it = next_vertex.find(glm::uvec2(wp.triangle.x, wp.triangle.z));
			if (it != next_vertex.end()) {
				//printf("==Found Adj Triangle: <%d, %d, %d>\n", wp.triangle.x, wp.triangle.x, it->second);
				// There is another triangle move to it
				wp.triangle = glm::uvec3(wp.triangle.x, wp.triangle.z, it->second);
				wp.weights = to_barycentric(next_world_pos, wp.triangle);
				adjacent_triangle_found = true;
			}
		}

		if (next_world_pos_bary.z < 0.0f) {
			// check if there is another triangle with points y and z
			auto it = next_vertex.find(glm::uvec2(wp.triangle.y, wp.triangle.x));
			if (it != next_vertex.end()) {
				//printf("==Found Adj Triangle: <%d, %d, %d>\n", wp.triangle.y, wp.triangle.x, it->second);
				// There is another triangle move to it
				wp.triangle = glm::uvec3(wp.triangle.y, wp.triangle.x, it->second);
				wp.weights = to_barycentric(next_world_pos, wp.triangle);
				adjacent_triangle_found = true;
			}
		}
	}
}

/*
{ // Adjust the weights if no new triangle is found
	glm::vec3 AP = next_world_pos - vertices[wp.triangle.x];
	glm::vec3 AB = vertices[wp.triangle.y] - vertices[wp.triangle.x];
	glm::vec3 AC = vertices[wp.triangle.z] - vertices[wp.triangle.x];
	glm::vec3 BC = vertices[wp.triangle.z] - vertices[wp.triangle.y];
	glm::vec3 adjusted_weights = wp.weights;

	// https://math.stackexchange.com/questions/1092912/find-closest-point-in-triangle-given-barycentric-coordinates-outside/2340013#2340013
	// Case 1:
	if (next_world_pos_bary.x >= 0.0f && next_world_pos_bary.y < 0.0f) {
		if (next_world_pos_bary.z < 0.0f && glm::dot(AP, AB) > 0.0f) {
			adjusted_weights.y = std::min(1.0f, glm::dot(AP, AB) / glm::dot(AB, AB));
			adjusted_weights.z = 0.0f;
		} else {
			adjusted_weights.y = 0.0f;
			adjusted_weights.z = std::min(1.0f, std::max(0.0f, glm::dot(AP, AC) / glm::dot(AC, AC)));
		}
		adjusted_weights.x = 1.0f - adjusted_weights.y - adjusted_weights.z;
	}
	// Case 2:
	else if (next_world_pos_bary.y >= 0.0f && next_world_pos_bary.z < 0.0f) {
		if (next_world_pos_bary.x < 0.0f && glm::dot(AP, AB) > 0.0f) {
			adjusted_weights.z = std::min(1.0f, glm::dot(AP, AB) / glm::dot(AB, AB));
			adjusted_weights.x = 0.0f;
		} else {
			adjusted_weights.z = 0.0f;
			adjusted_weights.x = std::min(1.0f, std::max(0.0f, glm::dot(AP, BC) / glm::dot(BC, BC)));
		}
		adjusted_weights.y = 1 - adjusted_weights.x - adjusted_weights.z;
	}
	// Case 3:
	else if (next_world_pos_bary.z >= 0.0f && next_world_pos_bary.x < 0.0f) {
		if (next_world_pos_bary.y < 0.0f && glm::dot(AP, AC) > 0.0f) {
			adjusted_weights.x = std::min(1.0f, glm::dot(AP, AC) / glm::dot(AC, AC));
			adjusted_weights.y = 0.0f;
		} else {
			adjusted_weights.x = 0.0f;
			adjusted_weights.y = std::min(1.0f, std::max(0.0f, glm::dot(AP, BC) / glm::dot(BC, BC)));
		}
		adjusted_weights.z = 1 - adjusted_weights.y - adjusted_weights.x;
	}

	bool adjacent_triangle_found = false;

if (adjusted_weights.x == 0.0f) {
	// check if there is another triangle with points y and z
	auto it = next_vertex.find(glm::uvec2(wp.triangle.z, wp.triangle.y));
	if (it != next_vertex.end()) {
		// There is another triangle move to it
		wp.triangle = glm::uvec3(wp.triangle.z, wp.triangle.y, it->second);
		wp.weights = to_barycentric(next_world_pos, wp.triangle);
		adjacent_triangle_found = true;
	}
}

if (adjusted_weights.y == 0.0f) {
	// check if there is another triangle with points y and z
	auto it = next_vertex.find(glm::uvec2(wp.triangle.x, wp.triangle.z));
	if (it != next_vertex.end()) {
		// There is another triangle move to it
		wp.triangle = glm::uvec3(wp.triangle.x, wp.triangle.z, it->second);
		wp.weights = to_barycentric(next_world_pos, wp.triangle);
		adjacent_triangle_found = true;
	}
}

if (adjusted_weights.z == 0.0f) {
	// check if there is another triangle with points y and z
	auto it = next_vertex.find(glm::uvec2(wp.triangle.y, wp.triangle.x));
	if (it != next_vertex.end()) {
		// There is another triangle move to it
		wp.triangle = glm::uvec3(wp.triangle.y, wp.triangle.x, it->second);
		wp.weights = to_barycentric(next_world_pos, wp.triangle);
		adjacent_triangle_found = true;
	}
}


	//if (!adjacent_triangle_found)
		//wp.weights = adjusted_weights;

} // End Weight Adjustment
*/


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
