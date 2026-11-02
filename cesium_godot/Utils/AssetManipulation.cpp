#include "AssetManipulation.h"

#include "../Models/CesiumGlobe.h"
#include "Models/CesiumGDConfig.h"
#include "Models/CesiumGDCreditSystem.h"
#include "Models/CesiumGDRasterOverlay.h"
#include "Models/CesiumGDTileset.h"
#include "godot_cpp/classes/camera3d.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/resource_loader.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/script.hpp"
#include "godot_cpp/classes/window.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/array.hpp"
#include <cstdint>

const char* CESIUM_GLOBE_NAME = "CesiumGeoreference";
const char* CESIUM_TILESET_NAME = "Cesium3DTileset";
const char* DYNAMIC_CAM_SCRIPT = "res://addons/cesium_godot/scripts/camera_controllers/CesiumDynamicCamera.gd";
const char* ORBIT_CAM_SCRIPT = "res://addons/cesium_godot/scripts/camera_controllers/CesiumOrbitCamera.gd";

const char* NO_ROOT_MSG = "No root node found in scene, add a Node3D to your scene in order to add Cesium Assets";

CesiumGeoreference* Godot3DTiles::AssetManipulation::find_or_create_globe(Node* baseNode) {
	Node* root = get_root_of_edit_scene(baseNode);
	ERR_FAIL_COND_V_MSG(root == nullptr, nullptr, NO_ROOT_MSG);
	CesiumGeoreference* globe = nullptr;
	int32_t count = root->get_child_count();
	for (int32_t i = 0; i < count; i++) {
		Node* child = root->get_child(i);
		CesiumGeoreference* foundChild = Object::cast_to<CesiumGeoreference>(child);
		if (foundChild != nullptr) {
			return foundChild;
		}
	}
	
	//Create a globe
	globe = memnew(CesiumGeoreference);
	globe->set_name(CESIUM_GLOBE_NAME);
	globe->set_rotation_degrees(Vector3(-90.0, 0.0, 0.0));
	root->add_child(globe, true);
	globe->set_owner(root);
	return globe;
}

CesiumGDConfig* Godot3DTiles::AssetManipulation::find_or_create_config_node(Node* baseNode) {
	Node* root = baseNode->get_tree()->get_root();
	ERR_FAIL_COND_V_MSG(root == nullptr, nullptr, NO_ROOT_MSG);
	CesiumGDConfig* result = find_node_in_scene<CesiumGDConfig>(root);
	if (result != nullptr) {
		return result;
	}
	result = memnew(CesiumGDConfig);
	root->add_child(result);
	result->set_owner(root);
	return result;
}

CesiumGDCreditSystem* Godot3DTiles::AssetManipulation::find_or_create_credit_system(Node* baseNode, bool deferred) {
	// HACK: assume we will always have this root as a node 3d on the child of the window
	Node* root = baseNode->get_tree()->get_root();
	ERR_FAIL_COND_V_MSG(root == nullptr, nullptr, NO_ROOT_MSG);
	CesiumGDCreditSystem* result = find_node_in_scene<CesiumGDCreditSystem>(root);
	if (result != nullptr) {
		return result;
	}
	
	CesiumGeoreference* globe = find_node_in_scene<CesiumGeoreference>(root);
	ERR_FAIL_COND_V_MSG(globe == nullptr, nullptr, "No CesiumGeoreference found in scene, please add one manually or with the Cesium Panel!");
	// The georeference's parent need not be a Node3D (it can be a plain Node or the root window).
	Node* parent = globe->get_parent();
	ERR_FAIL_COND_V_MSG(parent == nullptr, nullptr, "The CesiumGeoreference must be inside the scene tree to show data attribution");
	result = memnew(CesiumGDCreditSystem);
	constexpr bool readableName = true;
	if (deferred) {
		parent->call_deferred("add_child", result, readableName, godot::Node::INTERNAL_MODE_FRONT);
		result->call_deferred("set_owner", parent);
	}
	else {
		parent->add_child(result, readableName, godot::Node::INTERNAL_MODE_FRONT);
		result->set_owner(parent);
	}
	return result;
}

Node* Godot3DTiles::AssetManipulation::get_root_of_edit_scene(Node* baseNode) {
  return Object::cast_to<Node>(baseNode->get_tree()->get_edited_scene_root());
}


void Godot3DTiles::AssetManipulation::instantiate_tileset(Node* baseNode, int32_t assetId, const String& assetType, const String& assetName) {
	Node* root = get_root_of_edit_scene(baseNode);
	ERR_FAIL_COND_MSG(root == nullptr, NO_ROOT_MSG);
	Cesium3DTileset* tileset = memnew(Cesium3DTileset);
	root->add_child(tileset, true);
	
	CesiumIonRasterOverlay* rasterOverlay = nullptr;
		
	if (assetId == 0) {
		// We just create the current tileset and that's it
		return;
	}
	
	// If the asset type is terrain or 3D tiles, just set the asset id
	if (assetType == "3DTILES" || assetType == "TERRAIN") {
		tileset->set_name(assetName);
		tileset->set_ion_asset_id(assetId);
		return;
	}
	if (assetType != "IMAGERY") {
		// We currently do not support any other asset types, so these will have to be added manually
		ERR_PRINT("3D Tiles For Godot currently does not support the asset you're trying to add, try adding it manually through the Cesium3DTileset Node!");
		return;
	}
	
	// For imagery we can create a tileset with Cesium's world terrain and then add the imagery on top of it
	// In the future we might wanna check if the tileset already exists, but I do not want to assume too much rn
	constexpr int64_t worldTerrainId = 1; 
	tileset->set_ion_asset_id(worldTerrainId);
	tileset->set_name("Cesium World Terrain");

	rasterOverlay = memnew(CesiumIonRasterOverlay);
	rasterOverlay->set_asset_id(assetId);
	tileset->add_child(rasterOverlay, true);
	rasterOverlay->set_owner(root);
	rasterOverlay->set_name(assetName);
}


void Godot3DTiles::AssetManipulation::instantiate_dynamic_cam(Node* baseNode) {
	Node* root = get_root_of_edit_scene(baseNode);
	ERR_FAIL_COND_MSG(root == nullptr, NO_ROOT_MSG);
	CesiumGeoreference* globe = find_or_create_globe(baseNode);
	Camera3D* camera = memnew(Camera3D);
	root->add_child(camera, true);
	camera->set_owner(root);
	Ref<Resource> script = ResourceLoader::get_singleton()->load(DYNAMIC_CAM_SCRIPT, "Script");
	camera->set_script(script);
	camera->set("tilesets", find_all_tilesets(baseNode));
	camera->set("globe_node", globe);
	camera->set_name("CesiumDynamicCam");
}

void Godot3DTiles::AssetManipulation::instantiate_orbit_cam(Node* baseNode) {
	Node* root = get_root_of_edit_scene(baseNode);
	ERR_FAIL_COND_MSG(root == nullptr, NO_ROOT_MSG);
	CesiumGeoreference* globe = find_or_create_globe(baseNode);
	Camera3D* camera = memnew(Camera3D);
	root->add_child(camera, true);
	camera->set_owner(root);
	Ref<Resource> script = ResourceLoader::get_singleton()->load(ORBIT_CAM_SCRIPT, "Script");
	camera->set_script(script);
	camera->set("tilesets", find_all_tilesets(baseNode));
	camera->set("globe_node", globe);
	camera->set("target", globe);
	camera->set_name("CesiumOrbitCam");
}



Cesium3DTileset* find_first_tileset(Node* baseNode) {
	//Get a globe
	return nullptr;
}


Camera3D* Godot3DTiles::AssetManipulation::find_georef_cam(Node* rootNode) {
	Variant georef = rootNode->get("globe_node");
	if (georef.get_type() != Variant::NIL) {
		return Object::cast_to<Camera3D>(rootNode);
	}

	int32_t childCount = rootNode->get_child_count();
	for (int32_t i = 0; i < childCount; i++) {
		Node* currChild = rootNode->get_child(i);
		Camera3D* foundCam = find_georef_cam(currChild);
		if (foundCam != nullptr) {
			return foundCam;
		}
	}
		
	return nullptr;
}

void Godot3DTiles::AssetManipulation::update_camera_tilesets(Camera3D* camera) {
	Array allTilesets = find_all_tilesets(camera->get_tree()->get_root());
	camera->set("tilesets", allTilesets);
}

Array Godot3DTiles::AssetManipulation::find_all_tilesets(Node* baseNode) {
	// All tilesets will be inside the globe
	CesiumGeoreference* globeNode = find_or_create_globe(baseNode);
	int32_t count = globeNode->get_child_count();
	Array results;
	for (int32_t i = 0; i < count; i++) {
		Node* child = globeNode->get_child(i);
		auto* foundTileset = Object::cast_to<Cesium3DTileset>(child);
		if (foundTileset == nullptr || foundTileset->is_queued_for_deletion()) continue;
		results.append(foundTileset);
	}
	return results;
}
