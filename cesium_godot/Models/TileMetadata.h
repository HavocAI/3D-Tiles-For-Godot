#ifndef TILE_METADATA_H
#define TILE_METADATA_H

#include "CesiumPropertyInfo.h"
#include "CesiumGltf/PropertyArrayView.h"
#include "CesiumGltf/PropertyTableView.h"
#include "CesiumGltf/PropertyTypeTraits.h"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/vector2.hpp"
#include "godot_cpp/variant/vector2i.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include "godot_cpp/variant/vector3i.hpp"
#include "godot_cpp/variant/vector4.hpp"
#include "godot_cpp/variant/vector4i.hpp"
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>
#if defined (CESIUM_GD_EXT)
using namespace godot;
#endif

namespace CesiumGltf {
	struct Model;
	struct ExtensionModelExtStructuralMetadata;
}

/// @brief Reads a tile's EXT_structural_metadata property tables on demand.
///
/// The tile's glTF model is owned by cesium-native and stays alive until the tile's renderer
/// resources are freed, at which point the model is cleared and lookups return nothing.
class TileMetadata {

public:
	void set_model(const CesiumGltf::Model* model);

	const CesiumGltf::Model* get_model() const;

	int32_t get_table_count() const;

	/// @brief Number of features (rows) in a property table, or 0.
	int64_t get_feature_count(int32_t tableIndex) const;

	/// @brief Every property of one feature as {name: value}; missing values are null.
	Dictionary get_feature_properties(int64_t featureId, int32_t tableIndex) const;

	/// @brief {name: CesiumPropertyInfo} where each value holds one entry per feature.
	/// Built on first use and kept, so it stays readable after the model is released.
	const Dictionary& get_table(int32_t index) const;

private:
	const CesiumGltf::ExtensionModelExtStructuralMetadata* get_extension() const;

	const CesiumGltf::Model* m_model = nullptr;

	mutable std::vector<Dictionary> m_tables;

	mutable std::vector<bool> m_tableBuilt;

	Dictionary m_empty;
};

namespace TileMetadataConversion {

template <typename T> struct IsArrayValue : std::false_type {};
template <typename T> struct IsArrayValue<CesiumGltf::PropertyArrayView<T>> : std::true_type {};
template <typename T> struct IsArrayValue<CesiumGltf::PropertyArrayCopy<T>> : std::true_type {};

/// @brief Converts one metadata value (as returned by PropertyTablePropertyView::get) to a Variant.
template <typename T>
Variant to_variant(const T& value) {
	if constexpr (std::is_same_v<T, bool>) {
		return Variant(value);
	}
	else if constexpr (std::is_integral_v<T>) {
		return Variant(static_cast<int64_t>(value));
	}
	else if constexpr (std::is_floating_point_v<T>) {
		return Variant(static_cast<double>(value));
	}
	else if constexpr (std::is_same_v<T, std::string_view>) {
		return Variant(String::utf8(value.data(), static_cast<int64_t>(value.size())));
	}
	else if constexpr (CesiumGltf::IsMetadataVecN<T>::value) {
		using Component = typename T::value_type;
		constexpr glm::length_t N = T::length();
		if constexpr (std::is_integral_v<Component>) {
			if constexpr (N == 2) return Variant(Vector2i(value[0], value[1]));
			else if constexpr (N == 3) return Variant(Vector3i(value[0], value[1], value[2]));
			else return Variant(Vector4i(value[0], value[1], value[2], value[3]));
		}
		else {
			if constexpr (N == 2) return Variant(Vector2(value[0], value[1]));
			else if constexpr (N == 3) return Variant(Vector3(value[0], value[1], value[2]));
			else return Variant(Vector4(value[0], value[1], value[2], value[3]));
		}
	}
	else if constexpr (CesiumGltf::IsMetadataMatN<T>::value) {
		// Column-major, as stored in glTF.
		PackedFloat64Array result;
		for (glm::length_t c = 0; c < T::length(); c++) {
			for (glm::length_t r = 0; r < T::col_type::length(); r++) {
				result.push_back(static_cast<double>(value[c][r]));
			}
		}
		return Variant(result);
	}
	else if constexpr (IsArrayValue<T>::value) {
		Array result;
		for (int64_t i = 0; i < value.size(); i++) {
			result.push_back(to_variant(value[i]));
		}
		return Variant(result);
	}
	else {
		return Variant();
	}
}

template <typename T>
Variant to_variant(const std::optional<T>& value) {
	return value.has_value() ? to_variant(*value) : Variant();
}

}

#endif
