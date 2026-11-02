#include "Cesium3DTile.h"
#include "CesiumGltf/Model.h"
#include "CesiumGltf/PropertyTable.h"
#include "Models/TileMetadata.h"
#include "Utils/CesiumMathUtils.h"
#include "glm/ext/vector_double3.hpp"
#include "godot_cpp/classes/collision_shape3d.hpp"
#include "godot_cpp/classes/concave_polygon_shape3d.hpp"
#include "godot_cpp/classes/mesh.hpp"
#include "godot_cpp/classes/static_body3d.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include "godot_cpp/classes/mesh.hpp"
#include "CesiumGltf/PropertyTableView.h"
#include "CesiumGltf/Model.h"
#include "CesiumGltf/AccessorUtility.h"
#include "CesiumGltf/ExtensionExtMeshFeatures.h"
#include <algorithm>
#include <variant>
#include <cstdint>
#include <vector>


void Cesium3DTile::_ready() {
	// Override to delete behavior
}

void Cesium3DTile::generate_tile_collision() {
	// Get our static body and add it as a child of the mesh
	StaticBody3D* staticBody = Object::cast_to<StaticBody3D>(this->create_collision_node_custom_trimesh());
	ERR_FAIL_NULL_MSG(staticBody, "Unable to generate tile collision, failed to create the tile's shape");
	staticBody->set_name(String(this->get_name()) + "_col");

	this->add_child(staticBody, true);
	// Set all the owners
	Node* owner = this->get_owner();
	if (owner == nullptr) return;
	
	CollisionShape3D* collisionShape = Object::cast_to<CollisionShape3D>(staticBody->get_child(0));
	staticBody->set_owner(owner);
	collisionShape->set_owner(owner);
}

void Cesium3DTile::set_model(const CesiumGltf::Model* model) {
	this->m_metadata.set_model(model);
}

int64_t Cesium3DTile::get_feature_count(int32_t tableIndex) const {
	return this->m_metadata.get_feature_count(tableIndex);
}

Dictionary Cesium3DTile::get_feature_properties(int64_t featureId, int32_t tableIndex) const {
	return this->m_metadata.get_feature_properties(featureId, tableIndex);
}

static const CesiumGltf::ExtensionExtMeshFeatures* get_mesh_features(const CesiumGltf::Model* model, const CesiumGDSurfaceInfo& info, const CesiumGltf::MeshPrimitive** outPrimitive) {
	if (model == nullptr || info.meshIndex < 0 || info.meshIndex >= static_cast<int32_t>(model->meshes.size())) return nullptr;
	const CesiumGltf::Mesh& mesh = model->meshes[info.meshIndex];
	if (info.primitiveIndex < 0 || info.primitiveIndex >= static_cast<int32_t>(mesh.primitives.size())) return nullptr;
	*outPrimitive = &mesh.primitives[info.primitiveIndex];
	return (*outPrimitive)->getExtension<CesiumGltf::ExtensionExtMeshFeatures>();
}

int32_t Cesium3DTile::get_feature_id_set_count() const {
	int32_t count = 0;
	for (const CesiumGDSurfaceInfo& info : this->m_surfaces) {
		const CesiumGltf::MeshPrimitive* primitive = nullptr;
		const CesiumGltf::ExtensionExtMeshFeatures* features = get_mesh_features(this->m_metadata.get_model(), info, &primitive);
		if (features != nullptr) {
			count = std::max(count, static_cast<int32_t>(features->featureIds.size()));
		}
	}
	return count;
}

int64_t Cesium3DTile::get_feature_id(int32_t surfaceIndex, int64_t vertex, int32_t setIndex, int32_t* outPropertyTable) const {
	if (outPropertyTable != nullptr) *outPropertyTable = -1;
	if (surfaceIndex < 0 || surfaceIndex >= static_cast<int32_t>(this->m_surfaces.size())) return -1;
	const CesiumGltf::Model* model = this->m_metadata.get_model();
	const CesiumGltf::MeshPrimitive* primitive = nullptr;
	const CesiumGltf::ExtensionExtMeshFeatures* features = get_mesh_features(model, this->m_surfaces[surfaceIndex], &primitive);
	if (features == nullptr || setIndex < 0 || setIndex >= static_cast<int32_t>(features->featureIds.size())) return -1;

	const CesiumGltf::FeatureId& featureIdSet = features->featureIds[setIndex];
	if (outPropertyTable != nullptr) *outPropertyTable = featureIdSet.propertyTable;
	int64_t featureId = -1;
	if (featureIdSet.attribute.has_value()) {
		// _FEATURE_ID_<attribute>
		const CesiumGltf::FeatureIdAccessorType accessor = CesiumGltf::getFeatureIdAccessorView(*model, *primitive, static_cast<int32_t>(*featureIdSet.attribute));
		featureId = std::visit(CesiumGltf::FeatureIdFromAccessor{ vertex }, accessor);
	}
	else if (featureIdSet.texture.has_value()) {
		WARN_PRINT_ONCE("Feature ID textures are not supported; feature IDs from textures read as -1");
		return -1;
	}
	else {
		// Implicit feature IDs: the vertex index.
		featureId = vertex;
	}
	if (featureIdSet.nullFeatureId.has_value() && featureId == *featureIdSet.nullFeatureId) return -1;
	return featureId;
}

PackedInt32Array Cesium3DTile::get_vertex_feature_ids(int32_t surfaceIndex, int32_t setIndex) const {
	PackedInt32Array result;
	if (surfaceIndex < 0 || surfaceIndex >= static_cast<int32_t>(this->m_surfaces.size())) return result;
	const CesiumGDSurfaceInfo& info = this->m_surfaces[surfaceIndex];
	const CesiumGltf::MeshPrimitive* primitive = nullptr;
	const CesiumGltf::ExtensionExtMeshFeatures* features = get_mesh_features(this->m_metadata.get_model(), info, &primitive);
	if (features == nullptr || setIndex < 0 || setIndex >= static_cast<int32_t>(features->featureIds.size())) return result;
	// Vertices the loader appended as padding repeat the last glTF vertex.
	int64_t gltfVertexCount = info.vertexCount;
	const auto position = primitive->attributes.find("POSITION");
	const CesiumGltf::Model* model = this->m_metadata.get_model();
	if (position != primitive->attributes.end() && position->second >= 0 && position->second < static_cast<int32_t>(model->accessors.size())) {
		gltfVertexCount = std::min<int64_t>(gltfVertexCount, model->accessors[position->second].count);
	}
	result.resize(info.vertexCount);
	for (int64_t vertex = 0; vertex < info.vertexCount; vertex++) {
		const int64_t source = std::min<int64_t>(vertex, gltfVertexCount - 1);
		result.set(vertex, static_cast<int32_t>(this->get_feature_id(surfaceIndex, source, setIndex, nullptr)));
	}
	return result;
}

bool Cesium3DTile::resolve_face(int64_t faceIndex, int32_t* outSurface, int64_t* outVertex) const {
	if (faceIndex < 0) return false;
	const CesiumGltf::Model* model = this->m_metadata.get_model();
	if (model == nullptr) return false;
	// Mesh::get_faces() lists the triangles of each triangle surface in surface order.
	for (int32_t surface = 0; surface < static_cast<int32_t>(this->m_surfaces.size()); surface++) {
		const CesiumGDSurfaceInfo& info = this->m_surfaces[surface];
		if (faceIndex >= info.faceCount) {
			faceIndex -= info.faceCount;
			continue;
		}
		const CesiumGltf::MeshPrimitive& primitive = model->meshes[info.meshIndex].primitives[info.primitiveIndex];
		// The loader rewinds triangles by swapping their 2nd and 3rd corners, so the first is unchanged.
		const int64_t corner = faceIndex * 3;
		const CesiumGltf::IndexAccessorType indices = CesiumGltf::getIndexAccessorView(*model, primitive);
		int64_t vertex = corner;
		if (!std::holds_alternative<std::monostate>(indices)) {
			const int64_t count = std::visit([](const auto& view) -> int64_t {
				if constexpr (std::is_same_v<std::decay_t<decltype(view)>, std::monostate>) return 0;
				else return view.size();
			}, indices);
			// Indices the loader padded to a whole triangle repeat the last one.
			vertex = std::visit(CesiumGltf::IndexFromAccessor{ std::min<int64_t>(corner, count - 1) }, indices);
		}
		else {
			vertex = std::min<int64_t>(corner, info.vertexCount - 1);
		}
		*outSurface = surface;
		*outVertex = vertex;
		return vertex >= 0;
	}
	return false;
}

int64_t Cesium3DTile::get_face_feature_id(int64_t faceIndex, int32_t setIndex) const {
	int32_t surface = -1;
	int64_t vertex = -1;
	if (!this->resolve_face(faceIndex, &surface, &vertex)) return -1;
	return this->get_feature_id(surface, vertex, setIndex, nullptr);
}

Dictionary Cesium3DTile::get_face_feature_properties(int64_t faceIndex, int32_t setIndex) const {
	int32_t surface = -1;
	int64_t vertex = -1;
	if (!this->resolve_face(faceIndex, &surface, &vertex)) return Dictionary();
	int32_t propertyTable = -1;
	const int64_t featureId = this->get_feature_id(surface, vertex, setIndex, &propertyTable);
	if (featureId < 0 || propertyTable < 0) return Dictionary();
	return this->m_metadata.get_feature_properties(featureId, propertyTable);
}

Ref<ConcavePolygonShape3D> Cesium3DTile::create_trimesh_shape()  {
	// The mesh is already wound with Godot's clockwise front faces (CesiumGDModelLoader::reverse_winding),
	// which is also what one-sided concave collision expects.
	const PackedVector3Array faces = this->get_mesh()->get_faces();
	if (faces.size() == 0) {
		return Ref<ConcavePolygonShape3D>();
	}

	Ref<ConcavePolygonShape3D> shape = memnew(ConcavePolygonShape3D);
	shape->set_faces(faces);
	return shape;
}

Node* Cesium3DTile::create_collision_node_custom_trimesh() {
	Ref<ConcavePolygonShape3D> shape = this->create_trimesh_shape();
	if (shape.is_null()) {
		return nullptr;
	}
	
	StaticBody3D* staticBody = memnew(StaticBody3D);
	CollisionShape3D* collisionShape = memnew(CollisionShape3D);

	collisionShape->set_shape(shape);
	staticBody->add_child(collisionShape, true);
	return staticBody;
}

void Cesium3DTile::set_surface_info(std::vector<CesiumGDSurfaceInfo>&& surfaces) {
	this->m_surfaces = std::move(surfaces);
	this->m_baseMaterials.clear();
	Ref<Mesh> mesh = this->get_mesh();
	const int32_t count = mesh.is_valid() ? mesh->get_surface_count() : 0;
	for (int32_t i = 0; i < count; i++) {
		this->m_baseMaterials.push_back(mesh->surface_get_material(i));
	}
}

const std::vector<CesiumGDSurfaceInfo>& Cesium3DTile::get_surface_info() const {
	return this->m_surfaces;
}

Ref<Material> Cesium3DTile::get_base_material(int32_t surfaceIndex) const {
	if (surfaceIndex < 0 || surfaceIndex >= static_cast<int32_t>(this->m_baseMaterials.size())) {
		return Ref<Material>();
	}
	return this->m_baseMaterials[surfaceIndex];
}

const Dictionary& Cesium3DTile::get_metadata_table(int32_t index) const {
	return this->m_metadata.get_table(index);
}

int32_t Cesium3DTile::get_table_count() const {
	return this->m_metadata.get_table_count();
}

const Dictionary& Cesium3DTile::get_structural_metadata() const {
	return this->m_metadata.get_table(0);
}

void Cesium3DTile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_feature_count", "table_index"), &Cesium3DTile::get_feature_count, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_feature_properties", "feature_id", "table_index"), &Cesium3DTile::get_feature_properties, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_feature_id_set_count"), &Cesium3DTile::get_feature_id_set_count);
    ClassDB::bind_method(D_METHOD("get_vertex_feature_ids", "surface_index", "set_index"), &Cesium3DTile::get_vertex_feature_ids, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_face_feature_id", "face_index", "set_index"), &Cesium3DTile::get_face_feature_id, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_face_feature_properties", "face_index", "set_index"), &Cesium3DTile::get_face_feature_properties, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_metadata_table", "index"), &Cesium3DTile::get_metadata_table);
    ClassDB::bind_method(D_METHOD("get_table_count"), &Cesium3DTile::get_table_count);
    ClassDB::bind_method(D_METHOD("generate_tile_collision"), &Cesium3DTile::generate_tile_collision);
	ClassDB::bind_method(D_METHOD("get_structural_metadata"), &Cesium3DTile::get_structural_metadata);
}
