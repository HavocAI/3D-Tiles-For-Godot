#ifndef CESIUM_PROPERTY_INFO_H
#define CESIUM_PROPERTY_INFO_H

#include "CesiumGltf/PropertyTableView.h"
#include "CesiumGltf/PropertyTypeTraits.h"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/vector2.hpp"
#include "godot_cpp/variant/vector2i.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#if defined (CESIUM_GD_EXT)
using namespace godot;
#endif

enum class EPropertyType
{
    Invalid = 0,
    Scalar,
    Vec2,
    Vec3,
    Vec4,
    Mat2,
    Mat3,
    Mat4,
    String,
    Boolean,
    Enum
};

/// @brief type of the underlying components of a type, i.e. if the type is a Vec3f, the components are 32-bit floats 
enum class EComponentType : int64_t
{
    None = 0,
    Int8,
    Uint8,
    Int16,
    Uint16,
    Int32,
    Uint32,
    Int64,
    Uint64,
    Float32,
    Float64
};

class CesiumPropertyInfo : public RefCounted {
	GDCLASS(CesiumPropertyInfo, RefCounted)

protected:
	static void _bind_methods();

public:
    CesiumPropertyInfo() {};
    ~CesiumPropertyInfo() {};

	EPropertyType propertyType = EPropertyType::Invalid;
	EComponentType componentType = EComponentType::None;
	bool isArray = false;
	
    /// One value per feature (row) of the property table.
    godot::Variant data;

    /// @brief Every feature's value, indexed by feature ID.
    godot::Variant get_value();

    /// @brief One feature's value, or null when out of range.
    godot::Variant get_value_at(int64_t featureId);

    int64_t get_count();

    godot::String get_string_value();
};

#endif // CESIUM_PROPERTY_INFO_H