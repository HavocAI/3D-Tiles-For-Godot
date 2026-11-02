#ifndef CESIUM_3D_TILE
#define CESIUM_3D_TILE

#include "CesiumGltf/Model.h"
#include "Models/GeoreferencedNode.h"
#include "Models/TileMetadata.h"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include <cstdint>
#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/concave_polygon_shape3d.hpp"
using namespace godot;
#endif

#include <glm/ext/vector_double3.hpp>
#include <vector>
#include "CesiumGDModelLoader.h"

namespace CesiumGltf {
	class Model;
	
	class ExtensionModelExtStructuralMetadata;
}

class Cesium3DTile : public GeoreferencedMesh {
	GDCLASS(Cesium3DTile, GeoreferencedMesh)
	
public:
	
	void _ready() override;
	
	void generate_tile_collision();
	
	/// @brief Points metadata and feature ID lookups at the tile's glTF model (owned by
	/// cesium-native), or clears them with nullptr when cesium-native frees the tile.
	void set_model(const CesiumGltf::Model* model);
	
	const Dictionary& get_metadata_table(int32_t idx) const;

	int32_t get_table_count() const;

	const Dictionary& get_structural_metadata() const;

	int64_t get_feature_count(int32_t tableIndex) const;

	Dictionary get_feature_properties(int64_t featureId, int32_t tableIndex) const;

	int32_t get_feature_id_set_count() const;

	PackedInt32Array get_vertex_feature_ids(int32_t surfaceIndex, int32_t setIndex) const;

	int64_t get_face_feature_id(int64_t faceIndex, int32_t setIndex) const;

	Dictionary get_face_feature_properties(int64_t faceIndex, int32_t setIndex) const;

	/// @brief Records which glTF primitive each surface came from and the surfaces' own materials,
	/// so raster overlays can be attached to and detached from the right surfaces.
	void set_surface_info(std::vector<CesiumGDSurfaceInfo>&& surfaces);

	const std::vector<CesiumGDSurfaceInfo>& get_surface_info() const;

	/// @brief The material the surface had before any raster overlay was attached.
	Ref<Material> get_base_material(int32_t surfaceIndex) const;
	
private:
	/// Feature ID of one glTF vertex in one feature ID set, or -1; also reports the set's property table.
	int64_t get_feature_id(int32_t surfaceIndex, int64_t vertex, int32_t setIndex, int32_t* outPropertyTable) const;

	/// Surface and glTF vertex of the first corner of a face from Mesh::get_faces(), or false.
	bool resolve_face(int64_t faceIndex, int32_t* outSurface, int64_t* outVertex) const;

	std::vector<CesiumGDSurfaceInfo> m_surfaces;

	std::vector<Ref<Material>> m_baseMaterials;


	Ref<ConcavePolygonShape3D> create_trimesh_shape();

	Node* create_collision_node_custom_trimesh();

	TileMetadata m_metadata;
	
	glm::dvec3 m_originalPosition;

protected:

	static void _bind_methods();
	
};


#endif
