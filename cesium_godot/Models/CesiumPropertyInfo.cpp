#include "CesiumPropertyInfo.h"

godot::Variant CesiumPropertyInfo::get_value() {
	return this->data;
}

godot::Variant CesiumPropertyInfo::get_value_at(int64_t featureId) {
	if (this->data.get_type() != Variant::ARRAY) return godot::Variant();
	const godot::Array values = this->data;
	if (featureId < 0 || featureId >= values.size()) return godot::Variant();
	return values[featureId];
}

int64_t CesiumPropertyInfo::get_count() {
	if (this->data.get_type() != Variant::ARRAY) return 0;
	return godot::Array(this->data).size();
}

godot::String CesiumPropertyInfo::get_string_value() {
	return godot::String(this->data);
}

void CesiumPropertyInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value"), &CesiumPropertyInfo::get_value);
	ClassDB::bind_method(D_METHOD("get_value_at", "feature_id"), &CesiumPropertyInfo::get_value_at);
	ClassDB::bind_method(D_METHOD("get_count"), &CesiumPropertyInfo::get_count);
	ClassDB::bind_method(D_METHOD("get_string_value"), &CesiumPropertyInfo::get_string_value);
}
