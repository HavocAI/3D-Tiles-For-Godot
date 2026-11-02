#include "GodotPrepareRenderResources.h"
#include "CesiumGltf/ImageAsset.h"
#include "CesiumGltfReader/ImageDecoder.h"
#include "Models/Cesium3DTile.h"
#include "Models/CesiumGlobe.h"
#include "Utils/CesiumVariantHash.h"
#include "error_names.hpp"
#include "glm/ext/vector_double3.hpp"
#include "glm/fwd.hpp"
#include "godot_cpp/core/error_macros.hpp"

#if defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/mesh_instance3d.hpp>
#elif defined(CESIUM_GD_MODULE)
#include "scene/3d/mesh_instance_3d.h"
using namespace godot;
#endif

#include "CesiumAsync/AsyncSystem.h"
#include "Cesium3DTilesSelection/Tile.h"
#include "../CesiumGDModelLoader.h"
#include "../Utils/CesiumGDTextureLoader.h"
#include "CesiumRasterOverlays/RasterOverlayTile.h"
#include "CesiumRasterOverlays/RasterOverlay.h"
#include "../Utils/CesiumMathUtils.h"
#include "CesiumGltf/Node.h"
#include "CesiumGltf/ExtensionModelExtStructuralMetadata.h"
#include <glm/gtc/quaternion.hpp>
#include "../Models/CesiumGDTileset.h"

using namespace CesiumAsync;
using namespace Cesium3DTilesSelection;

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> GodotPrepareRenderResources::prepareInLoadThread(const CesiumAsync::AsyncSystem& asyncSystem, Cesium3DTilesSelection::TileLoadResult&& tileLoadResult, const glm::dmat4& transform, const std::any& rendererOptions)
{
	CesiumGltf::Model* model = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);

	if (model == nullptr) {
		return asyncSystem.createResolvedFuture(TileLoadResultAndRenderResources{ std::move(tileLoadResult), nullptr });
	}

	return asyncSystem.createFuture<TileLoadResultAndRenderResources>([=, this, tileLoadResult = std::move(tileLoadResult)](Promise<TileLoadResultAndRenderResources> p_promise) mutable {
		// Re-extract model pointer since tileLoadResult was moved
		CesiumGltf::Model* movedModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
		if (movedModel == nullptr) {
			ERR_PRINT("Model was invalidated after move!");
			TileLoadResultAndRenderResources errorResult{ std::move(tileLoadResult), nullptr };
			p_promise.resolve(errorResult);
			return;
		}

		Error err;
		std::vector<CesiumGDSurfaceInfo> surfaces;
		Ref<ArrayMesh> meshData = CesiumGDModelLoader::generate_meshes_from_model(*movedModel, &err, &surfaces);

		if (err != Error::OK) {
			String errorMsg = String("Error generating meshes for tile ") + REFLECT_ERR_NAME(err);
			ERR_PRINT(errorMsg);
			// Always resolve with nullptr - reject() crashes with LIBASYNC_NO_EXCEPTIONS
			TileLoadResultAndRenderResources errorResult{ std::move(tileLoadResult), nullptr };
			p_promise.resolve(errorResult);
			return;
		}

		Cesium3DTile* instance = memnew(Cesium3DTile);
		instance->set_mesh(meshData);
		instance->set_surface_info(std::move(surfaces));

		// Check if model has nodes before accessing
		if (movedModel->nodes.empty()) {
			ERR_PRINT("Tile model has no nodes - cannot render");
			TileLoadResultAndRenderResources errorResult{ std::move(tileLoadResult), nullptr };
			p_promise.resolve(errorResult);
			return;
		}

		const CesiumGltf::Node &rootNode = movedModel->nodes[0];

		const std::vector<double> &translationArray = rootNode.translation;
		const std::vector<double> &rotationArray = rootNode.rotation;
		const std::vector<double> &scaleArray = rootNode.scale;

		Transform3D gdTransform = Transform3D(Basis(), Vector3());
		gdTransform.scale(Vector3(1, 1, 1));
		instance->set_transform(gdTransform);

		// glTF scale is optional - default to 1,1,1 if not present
		Vector3 scale(1.0f, 1.0f, 1.0f);
		if (scaleArray.size() >= 3) {
			scale.x = scaleArray[0];
			scale.y = scaleArray[1];
			scale.z = scaleArray[2];
		}

		const glm::dmat4 transformationMat = CesiumMathUtils::array_to_dmat4(rootNode.matrix);

		glm::dvec3 glmPos;
		glm::dquat glmRot;
		// Applies for tilesets that 
		constexpr int32_t worldTerrainId = 1;
		constexpr int32_t osmBuildingsId = 96188;

		constexpr int32_t translationColumnIndex = 3;
		glmPos = transformationMat[translationColumnIndex];
		glmRot = glm::quat_cast(transformationMat);

		Vector3 translation;
		Quaternion rotation = CesiumMathUtils::from_glm_quat(glmRot);
		CesiumGeoreference* geoReferenceNode = nullptr;

		if (this->m_tileset->is_georeferenced(&geoReferenceNode)) {
			real_t scaleFactor = geoReferenceNode->get_scale_factor();
			instance->set_scale({ scaleFactor, scaleFactor, scaleFactor });
			translation *= scaleFactor;
		}

		// Avoid adding as child in the worker thread
		instance->set_tileset_no_reparent(this->m_tileset);

		// Save this for use later
		instance->set_original_position(glmPos);

		Vector3 eulerAngles = rotation.get_euler();

		translation = CesiumMathUtils::from_glm_vec3(glmPos);
		instance->set_position(translation);
		instance->set_rotation(eulerAngles);
		if (this->m_tileset->get_create_physics_meshes()) {
			instance->generate_tile_collision();
		}

		// Metadata extraction
		const CesiumGltf::ExtensionModelExtStructuralMetadata* modelMetadata = movedModel->getExtension<CesiumGltf::ExtensionModelExtStructuralMetadata>();
		instance->add_metadata(movedModel, modelMetadata);

		TileLoadResultAndRenderResources result{
			std::move(tileLoadResult),
			static_cast<void*>(instance)
		};

		p_promise.resolve(result);

	});
}

void* GodotPrepareRenderResources::prepareInMainThread(Tile& tile, void* pLoadThreadResult)
{
	// If load thread failed, pLoadThreadResult will be nullptr
	if (pLoadThreadResult == nullptr) {
		return nullptr;
	}

	const Cesium3DTilesSelection::TileContent& content = tile.getContent();
	const Cesium3DTilesSelection::TileRenderContent* pRenderContent = content.getRenderContent();

	if (pRenderContent == nullptr) {
		return pLoadThreadResult;
	}

	const CesiumGltf::Model& model = pRenderContent->getModel();
	// Apply the transform if any
	Cesium3DTile* instance = reinterpret_cast<Cesium3DTile*>(pLoadThreadResult);

	glm::dmat4 tileTransform = tile.getTransform();
	tileTransform = CesiumGDModelLoader::apply_rtc_center(model, tileTransform);
	tileTransform = CesiumGDModelLoader::apply_gltf_up_axis_transform(model, tileTransform);
	// Then apply the z up
	glm::dvec3 position = glm::dvec3(CesiumMathUtils::ecef_to_engine(tileTransform[3]));
	instance->set_original_position(instance->get_original_position() + position);
	return pLoadThreadResult;
}

void GodotPrepareRenderResources::free(Tile& tile, void* pLoadThreadResult, void* pMainThreadResult) noexcept
{
	const auto& tileId = tile.getTileID();
	uint64_t hash = static_cast<uint64_t>(std::visit(CesiumVariantHash{}, tileId));
	auto* instance = static_cast<Cesium3DTile*>(pMainThreadResult);
	this->m_tileset->call_deferred("free_tile", instance, hash);
}

static Cesium3DTile* get_tile_instance(const Tile& tile)
{
	const Cesium3DTilesSelection::TileRenderContent* renderContent = tile.getContent().getRenderContent();
	if (renderContent == nullptr) return nullptr;
	return static_cast<Cesium3DTile*>(renderContent->getRenderResources());
}

void GodotPrepareRenderResources::attachRasterInMainThread(const Tile& tile, int32_t overlayTextureCoordinateID, const CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pMainThreadRendererResources, const glm::dvec2& translation, const glm::dvec2& scale)
{
	Cesium3DTile* meshInstance = get_tile_instance(tile);
	if (meshInstance == nullptr || pMainThreadRendererResources == nullptr) return;
	if (overlayTextureCoordinateID < 0 || overlayTextureCoordinateID > 1) return;

	const Ref<ImageTexture>& godotTexture = *static_cast<Ref<ImageTexture>*>(pMainThreadRendererResources);
	const CesiumGltf::Model& model = tile.getContent().getRenderContent()->getModel();
	Ref<ArrayMesh> arrayMesh = meshInstance->get_mesh();
	if (arrayMesh.is_null()) return;

	// Raster coordinates are overlayUV * scale + translation, with V flipped for the image.
	const Vector3 uvScale(scale.x, -scale.y, 1.0);
	const Vector3 uvOffset(translation.x, 1.0 - translation.y, 0.0);

	// Only swap materials: the surfaces already carry the overlay coordinates (see
	// CesiumGDModelLoader::generate_meshes_from_model), so no surface is added or rebuilt.
	const std::vector<CesiumGDSurfaceInfo>& surfaces = meshInstance->get_surface_info();
	const int32_t surfaceCount = std::min<int32_t>(surfaces.size(), arrayMesh->get_surface_count());
	for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; surfaceIndex++) {
		const CesiumGDSurfaceInfo& info = surfaces[surfaceIndex];
		const int8_t uvChannel = info.overlayUvChannel[overlayTextureCoordinateID];
		if (uvChannel == 0) continue;

		Ref<StandardMaterial3D> overlayMaterial;
		Ref<StandardMaterial3D> baseMaterial = meshInstance->get_base_material(surfaceIndex);
		if (baseMaterial.is_valid()) {
			overlayMaterial = baseMaterial->duplicate();
		}
		else {
			// The base is a shader material (KHR_texture_transform); start from the glTF material.
			const CesiumGltf::MeshPrimitive& primitive = model.meshes[info.meshIndex].primitives[info.primitiveIndex];
			overlayMaterial.instantiate();
			CesiumGDModelLoader::copy_material_properties(CesiumGDModelLoader::get_primitive_material(model, primitive), overlayMaterial, model);
		}

		if (uvChannel == 1) {
			overlayMaterial->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, godotTexture);
			overlayMaterial->set_uv1_scale(uvScale);
			overlayMaterial->set_uv1_offset(uvOffset);
		}
		else {
			// UV holds the model's own texture coordinates, so draw the overlay as a detail layer on UV2.
			overlayMaterial->set_feature(BaseMaterial3D::FEATURE_DETAIL, true);
			overlayMaterial->set_detail_uv(BaseMaterial3D::DETAIL_UV_2);
			overlayMaterial->set_detail_blend_mode(BaseMaterial3D::BLEND_MODE_MIX);
			overlayMaterial->set_texture(BaseMaterial3D::TEXTURE_DETAIL_ALBEDO, godotTexture);
			overlayMaterial->set_uv2_scale(uvScale);
			overlayMaterial->set_uv2_offset(uvOffset);
		}
		arrayMesh->surface_set_material(surfaceIndex, overlayMaterial);
	}
}

void GodotPrepareRenderResources::detachRasterInMainThread(const Tile& tile, int32_t overlayTextureCoordinateID, const CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pMainThreadRendererResources) noexcept
{
	Cesium3DTile* meshInstance = get_tile_instance(tile);
	if (meshInstance == nullptr) return;
	if (overlayTextureCoordinateID < 0 || overlayTextureCoordinateID > 1) return;
	Ref<ArrayMesh> arrayMesh = meshInstance->get_mesh();
	if (arrayMesh.is_null()) return;

	// Put back the surfaces' own materials; a replacement raster tile is attached afterwards.
	const std::vector<CesiumGDSurfaceInfo>& surfaces = meshInstance->get_surface_info();
	const int32_t surfaceCount = std::min<int32_t>(surfaces.size(), arrayMesh->get_surface_count());
	for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; surfaceIndex++) {
		if (surfaces[surfaceIndex].overlayUvChannel[overlayTextureCoordinateID] == 0) continue;
		arrayMesh->surface_set_material(surfaceIndex, meshInstance->get_base_material(surfaceIndex));
	}
}

void* GodotPrepareRenderResources::prepareRasterInLoadThread(CesiumGltf::ImageAsset& image, const std::any& rendererOptions)
{
	//I guess we just generate mip maps here (?
	CesiumGltfReader::ImageDecoder::generateMipMaps(image);
	return nullptr;
}

void* GodotPrepareRenderResources::prepareRasterInMainThread(CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pLoadThreadResult)
{
	const CesiumGltf::ImageAsset* imageCesium = rasterTile.getImage().get();
	if (imageCesium == nullptr || imageCesium->width <= 0 || imageCesium->height <= 0) {
		return nullptr;
	}
	const bool hasMipMaps = !imageCesium->mipPositions.empty();
	Ref<ImageTexture> godotTexture = CesiumGDTextureLoader::load_image_texture(*imageCesium, false, hasMipMaps);
	if (godotTexture.is_null()) {
		return nullptr;
	}
	// Owned by cesium-native until freeRaster.
	return new Ref<ImageTexture>(godotTexture);
}

void GodotPrepareRenderResources::freeRaster(const CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pLoadThreadResult, void* pMainThreadResult) noexcept
{
	delete static_cast<Ref<ImageTexture>*>(pMainThreadResult);
}
