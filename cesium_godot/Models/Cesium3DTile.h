#ifndef CESIUM_3D_TILE
#define CESIUM_3D_TILE

#include "CesiumGltf/Model.h"
#include "Models/GeoreferencedNode.h"
#include "Models/TileMetadata.h"
#include "godot_cpp/variant/dictionary.hpp"
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
	
	void add_metadata(const CesiumGltf::Model* model, const CesiumGltf::ExtensionModelExtStructuralMetadata* metadata);
	
	const Dictionary& get_metadata_table(int32_t idx) const;

	int32_t get_table_count() const;

	const Dictionary& get_structural_metadata() const;

	/// @brief Records which glTF primitive each surface came from and the surfaces' own materials,
	/// so raster overlays can be attached to and detached from the right surfaces.
	void set_surface_info(std::vector<CesiumGDSurfaceInfo>&& surfaces);

	const std::vector<CesiumGDSurfaceInfo>& get_surface_info() const;

	/// @brief The material the surface had before any raster overlay was attached.
	Ref<Material> get_base_material(int32_t surfaceIndex) const;
	
private:
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
