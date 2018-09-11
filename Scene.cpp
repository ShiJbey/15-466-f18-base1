#include "Scene.hpp"
#include "read_chunk.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <cstddef>
#include <map>
#include <unordered_map>

glm::mat4 Scene::Transform::make_local_to_parent() const {
	return glm::mat4( //translate
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(position, 1.0f)
	)
	* glm::mat4_cast(rotation) //rotate
	* glm::mat4( //scale
		glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, scale.z, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
	);
}

glm::mat4 Scene::Transform::make_parent_to_local() const {
	glm::vec3 inv_scale;
	inv_scale.x = (scale.x == 0.0f ? 0.0f : 1.0f / scale.x);
	inv_scale.y = (scale.y == 0.0f ? 0.0f : 1.0f / scale.y);
	inv_scale.z = (scale.z == 0.0f ? 0.0f : 1.0f / scale.z);
	return glm::mat4( //un-scale
		glm::vec4(inv_scale.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, inv_scale.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, inv_scale.z, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
	)
	* glm::mat4_cast(glm::inverse(rotation)) //un-rotate
	* glm::mat4( //un-translate
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-position, 1.0f)
	);
}

glm::mat4 Scene::Transform::make_local_to_world() const {
	if (parent) {
		return parent->make_local_to_world() * make_local_to_parent();
	} else {
		return make_local_to_parent();
	}
}

glm::mat4 Scene::Transform::make_world_to_local() const {
	if (parent) {
		return make_parent_to_local() * parent->make_world_to_local();
	} else {
		return make_parent_to_local();
	}
}

void Scene::Transform::DEBUG_assert_valid_pointers() const {
	if (parent == nullptr) {
		//if no parent, can't have siblings:
		assert(prev_sibling == nullptr);
		assert(next_sibling == nullptr);
	} else {
		//if have parent, last child if and only if no next sibling:
		assert((next_sibling == nullptr) == (this == parent->last_child));
	}
	//check proper return pointers from neighbors:
	assert(prev_sibling == nullptr || prev_sibling->next_sibling == this);
	assert(next_sibling == nullptr || next_sibling->prev_sibling == this);
	assert(last_child == nullptr || last_child->parent == this);
}


void Scene::Transform::set_parent(Transform *new_parent, Transform *before) {
	DEBUG_assert_valid_pointers();
	assert(before == nullptr || (new_parent != nullptr && before->parent == new_parent));
	if (parent) {
		//remove from existing parent:
		if (prev_sibling) prev_sibling->next_sibling = next_sibling;
		if (next_sibling) next_sibling->prev_sibling = prev_sibling;
		else parent->last_child = prev_sibling;
		next_sibling = prev_sibling = nullptr;
	}
	parent = new_parent;
	if (parent) {
		//add to new parent:
		if (before) {
			prev_sibling = before->prev_sibling;
			next_sibling = before;
			next_sibling->prev_sibling = this;
		} else {
			prev_sibling = parent->last_child;
			parent->last_child = this;
		}
		if (prev_sibling) prev_sibling->next_sibling = this;
	}
	DEBUG_assert_valid_pointers();
}

//---------------------------

glm::mat4 Scene::Camera::make_projection() const {
	return glm::infinitePerspective( fovy, aspect, near );
}

//---------------------------

//templated helper functions to avoid having to write the same new/delete code three times:
template< typename T, typename... Args >
T *list_new(T * &first, Args&&... args) {
	T *t = new T(std::forward< Args >(args)...); //"perfect forwarding"
	if (first) {
		t->alloc_next = first;
		first->alloc_prev_next = &t->alloc_next;
	}
	t->alloc_prev_next = &first;
	first = t;
	return t;
}

template< typename T >
void list_delete(T * t) {
	assert(t && "It is invalid to delete a null scene object [yes this is different than 'delete']");
	assert(t->alloc_prev_next);
	if (t->alloc_next) {
		t->alloc_next->alloc_prev_next = t->alloc_prev_next;
	}
	*t->alloc_prev_next = t->alloc_next;
	//PARANOIA:
	t->alloc_next = nullptr;
	t->alloc_prev_next = nullptr;
}

Scene::Transform *Scene::new_transform() {
	return list_new< Scene::Transform >(first_transform);
}

void Scene::delete_transform(Scene::Transform *transform) {
	list_delete< Scene::Transform >(transform);
}

Scene::Object *Scene::new_object(Scene::Transform *transform) {
	assert(transform && "Scene::Object must be attached to a transform.");
	return list_new< Scene::Object >(first_object, transform);
}

void Scene::delete_object(Scene::Object *object) {
	list_delete< Scene::Object >(object);
}

Scene::Camera *Scene::new_camera(Scene::Transform *transform) {
	assert(transform && "Scene::Camera must be attached to a transform.");
	return list_new< Scene::Camera >(first_camera, transform);
}

void Scene::delete_camera(Scene::Camera *object) {
	list_delete< Scene::Camera >(object);
}

void Scene::draw(Scene::Camera const *camera) {
	assert(camera && "Must have a camera to draw scene from.");

	glm::mat4 world_to_camera = camera->transform->make_world_to_local();
	glm::mat4 world_to_clip = camera->make_projection() * world_to_camera;

	for (Scene::Object *object = first_object; object != nullptr; object = object->alloc_next) {
		glm::mat4 local_to_world = object->transform->make_local_to_world();

		//compute modelview+projection (object space to clip space) matrix for this object:
		glm::mat4 mvp = world_to_clip * local_to_world;

		//compute modelview (object space to camera local space) matrix for this object:
		glm::mat4 mv = local_to_world;

		//NOTE: inverse cancels out transpose unless there is scale involved
		glm::mat3 itmv = glm::inverse(glm::transpose(glm::mat3(mv)));

		//set up program uniforms:
		glUseProgram(object->program);
		if (object->program_mvp_mat4 != -1U) {
			glUniformMatrix4fv(object->program_mvp_mat4, 1, GL_FALSE, glm::value_ptr(mvp));
		}
		if (object->program_mv_mat4x3 != -1U) {
			glUniformMatrix4x3fv(object->program_mv_mat4x3, 1, GL_FALSE, glm::value_ptr(mv));
		}
		if (object->program_itmv_mat3 != -1U) {
			glUniformMatrix3fv(object->program_itmv_mat3, 1, GL_FALSE, glm::value_ptr(itmv));
		}

		if (object->set_uniforms) object->set_uniforms();

		glBindVertexArray(object->vao);

		//draw the object:
		glDrawArrays(GL_TRIANGLES, object->start, object->count);
	}
}


std::unordered_map<std::string, Scene::Transform*> Scene::load(std::string const &filename) {

	// Open the file
	std::ifstream file(filename, std::ios::binary);

	struct BlenderTransform {
		int parent;
		uint32_t name_start;
		uint32_t name_end;
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;
	};
	static_assert(sizeof(BlenderTransform) == 4+4+4+(4*3)+(4*4)+(4*3), "Simple transform should be packed");
	
	// All the transforms exported by the scene
	std::vector< BlenderTransform > transforms;

	struct BlenderMesh {
		int hierarchy_ref;
		uint32_t name_start;
		uint32_t name_end;
	};

	static_assert(sizeof(BlenderMesh) == 4+4+4, "Simple mesh should be packed");
	std::vector< BlenderMesh > meshes;

	struct BlenderCamera {
		int hierarchy_ref;
		char type[4] = {'\0', '\0', '\0', '\0'};
		float fov_scale;
		float clip_start;
		float clip_end;
	};

	static_assert(sizeof(BlenderCamera) == 4+(1*4)+4+4+4, "Simple camera should be packed");
	std::vector< BlenderCamera > cameras;

	struct BlenderLamp {
		int hierarchy_ref;
		char type;
		char r;
		char g;
		char b;
		float energy;
		float distance;
		float fov;
	};
	static_assert(sizeof(BlenderLamp) == 4+1+1+1+1+4+4+4, "Lamp should be packed");
	std::vector<BlenderLamp> lamps;

	std::vector< char > strings;
	read_chunk(file, "str0", &strings);

	auto print_blender_transform = [](BlenderTransform trans) {
		printf("POS: x: %f, y: %f, z: %f\nROT: x: %f, y: %f, z: %f, w: %f\nSCL: x: %f, y: %f, z: %f\n",
		trans.position.x, trans.position.y, trans.position.z,
		trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w, 
		trans.scale.x, trans.scale.y, trans.scale.z
		);
	};

	auto get_blendermesh_name = [&strings](BlenderMesh mesh) {
		std::string name(&strings[0] + mesh.name_start, &strings[0] + mesh.name_end);
		return name;
	};

	if (filename.size() >= 6 && filename.substr(filename.size() - 6) == ".scene") {

		read_chunk(file, "xfh0", &transforms);
		printf("%zd transforms were imported from blender\n", transforms.size());
		
		

		printf("All Blender Transforms:\n");
		for (uint32_t i = 0; i < transforms.size(); i++) {
			printf("Transform: %d\n", i);
			print_blender_transform(transforms[i]);
		}
		


		read_chunk(file, "msh0", &meshes);

		printf("All Meshes:\n");
		for (uint32_t i = 0; i < meshes.size(); i++) {
			printf("Name: %s - Ref: %d\n", get_blendermesh_name(meshes[i]).c_str(), meshes[i].hierarchy_ref);
		}


		read_chunk(file, "cam0", &cameras);

		printf("Cameras: \n");
		for (uint32_t i = 0; i < cameras.size(); i++) {
			printf("Type: %.*s - Ref: %d\n", 4, cameras[i].type, cameras[i].hierarchy_ref);
		}

		read_chunk(file, "lmp0", &lamps);
		printf("Lamps: \n");
		for (uint32_t i = 0; i < lamps.size(); i++) {
			printf("Type: %c - Ref: %d\n", lamps[i].type, lamps[i].hierarchy_ref);
		}

	} else {
		throw std::runtime_error("Unknown file type '" + filename + "'");
	}

	

	// Lambda to check if the begin and end name indices are valid
	auto valid_range = [&strings](uint32_t name_begin, uint32_t name_end) {
		return name_begin <= name_end && name_end <= strings.size();
	};

	// Returns the index of the transform thar mathces this mesh
	auto get_matching_transform_index = [&transforms](const BlenderMesh& mesh) {
		for (uint32_t i = 0; i < transforms.size(); i++) {
			if (transforms[i].name_start == mesh.name_start && 
				transforms[i].name_end == mesh.name_end) {
					return (int)i;
			}
		}
		return -1;
	};

	

	std::unordered_map<std::string, Transform*> name_to_trans;
	std::unordered_map<int, std::string> ref_to_name;
	

	{ // Create the transform heirarchy
		Transform *transform;

		// Fill the two maps created above
		for (uint32_t i = 0; i < meshes.size(); i++) {
			if (valid_range(meshes[i].name_start, meshes[i].name_end)) {
				BlenderTransform btrans = transforms[meshes[i].hierarchy_ref];
				printf("Found Transform for mesh...:\n");
				print_blender_transform(btrans);

				std::string name = get_blendermesh_name(meshes[i]);
				// Create new transform
				transform = new_transform();

				transform->name = name;
				transform->position = btrans.position;
				transform->rotation = btrans.rotation;
				transform->scale = btrans.scale;

				bool inserted = false;
				inserted = name_to_trans.insert(std::make_pair(name, transform)).second;
				if (!inserted) {
					std::cerr << "WARNING: mesh name '" + name + "' in filename '" + filename + "' collides with existing mesh." << std::endl;
				}

				inserted = ref_to_name.insert(std::make_pair(meshes[i].hierarchy_ref, name)).second;
				if (!inserted) {
					std::cerr << "WARNING: mesh hierarchy reference number \'" << (int)(meshes[i].hierarchy_ref)  << "\' in filename \'" << filename << "\' collides with existing reference." << std::endl;
				}
			}
		}

		// Do the same for the cameras
		for (uint32_t i = 0; i < cameras.size(); i++) {
			std::string name("Camera-" + i);
			BlenderTransform btrans = transforms[cameras[i].hierarchy_ref];
			// Create a new scene transform
			transform = new_transform();
			transform->name = name;
			transform->position = btrans.position;
			transform->rotation = btrans.rotation;
			transform->scale = btrans.scale;
			// Add the camera to the scene since we dont attach objects
			Camera *camera = new_camera(transform);
			//camera->fovy = cameras[i].fov_scale;


			bool inserted = false;
			inserted = name_to_trans.insert(std::make_pair(name, transform)).second;
			if (!inserted) {
				std::cerr << "WARNING: Camera name '" + name + "' collides with existing camera." << std::endl;
			}

			inserted = ref_to_name.insert(std::make_pair(cameras[i].hierarchy_ref, name)).second;
			if (!inserted) {
				std::cerr << "WARNING: Camera hierarchy reference number \'" << (int)(meshes[i].hierarchy_ref)  << "\' in filename \'" << filename << "\' collides with existing reference." << std::endl;
			}
		}

		// Do the same for lamps
		for (uint32_t i = 0; i < lamps.size(); i++) {
			std::string name("Lamp-" + i);
			BlenderTransform btrans = transforms[lamps[i].hierarchy_ref];
			// Create a new scene transform
			transform = new_transform();
			transform->name = name;
			transform->position = btrans.position;
			transform->rotation = btrans.rotation;
			transform->scale = btrans.scale;


			bool inserted = false;
			inserted = name_to_trans.insert(std::make_pair(name, transform)).second;
			if (!inserted) {
				std::cerr << "WARNING: Lamp name '" + name + "' collides with existing lamp." << std::endl;
			}

			inserted = ref_to_name.insert(std::make_pair(lamps[i].hierarchy_ref, name)).second;
			if (!inserted) {
				std::cerr << "WARNING: Lamp hierarchy reference number \'" << (int)(lamps[i].hierarchy_ref)  << "\' in filename \'" << filename << "\' collides with existing reference." << std::endl;
			}
		}


		// Loop through the transforms and fix parents for meshes
		for (uint32_t i = 0; i < transforms.size(); i++) {
			// Find the pointer to the parent's transform
			Transform *parent_trans = nullptr;
			auto it = ref_to_name.find(transforms[i].parent);
			if (it != ref_to_name.end()) {
				std::string parent_name = it->second;
				auto it = name_to_trans.find(parent_name);
				if (it != name_to_trans.end()) {
					parent_trans = it->second;
				}
			}

			// Get the real transform for this simple transform
			Transform *my_trans = nullptr;
			std::string name(&strings[0] + transforms[i].name_start, &strings[0] + transforms[i].name_end);
			auto it2 = name_to_trans.find(name);
			if (it2 !=  name_to_trans.end()) {
				my_trans = it2->second;
			}

			// Set the parent to the transform
			if (my_trans != nullptr && parent_trans != nullptr) {
				my_trans->set_parent(parent_trans);
			}
		}
	}
	
	return name_to_trans;
}

Scene::~Scene() {
	while (first_camera) {
		delete_camera(first_camera);
	}
	while (first_object) {
		delete_object(first_object);
	}
	while (first_transform) {
		delete_transform(first_transform);
	}
}

Scene::Object *Scene::get_object(std::string const &name) {
	for (Scene::Object *object = first_object; object != nullptr; object = object->alloc_next) {
		if (object->transform->name == name) {
			return object;
		}
	}
	return nullptr;
}