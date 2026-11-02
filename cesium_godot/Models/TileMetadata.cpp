#include "TileMetadata.h"
#include "CesiumGltf/ExtensionModelExtStructuralMetadata.h"
#include "CesiumGltf/Model.h"
#include "godot_cpp/core/memory.hpp"

using namespace TileMetadataConversion;

void TileMetadata::set_model(const CesiumGltf::Model* model) {
	this->m_model = model;
	const CesiumGltf::ExtensionModelExtStructuralMetadata* extension = this->get_extension();
	const size_t tableCount = extension == nullptr ? 0 : extension->propertyTables.size();
	if (model != nullptr || this->m_tables.empty()) {
		// Keep already-built tables readable after the model is released.
		this->m_tables.assign(tableCount, Dictionary());
		this->m_tableBuilt.assign(tableCount, false);
	}
}

const CesiumGltf::Model* TileMetadata::get_model() const {
	return this->m_model;
}

const CesiumGltf::ExtensionModelExtStructuralMetadata* TileMetadata::get_extension() const {
	if (this->m_model == nullptr) return nullptr;
	return this->m_model->getExtension<CesiumGltf::ExtensionModelExtStructuralMetadata>();
}

int32_t TileMetadata::get_table_count() const {
	return static_cast<int32_t>(this->m_tables.size());
}

int64_t TileMetadata::get_feature_count(int32_t tableIndex) const {
	const CesiumGltf::ExtensionModelExtStructuralMetadata* extension = this->get_extension();
	if (extension == nullptr || tableIndex < 0 || tableIndex >= static_cast<int32_t>(extension->propertyTables.size())) {
		return 0;
	}
	return extension->propertyTables[tableIndex].count;
}

Dictionary TileMetadata::get_feature_properties(int64_t featureId, int32_t tableIndex) const {
	Dictionary result;
	const CesiumGltf::ExtensionModelExtStructuralMetadata* extension = this->get_extension();
	if (extension == nullptr || tableIndex < 0 || tableIndex >= static_cast<int32_t>(extension->propertyTables.size())) {
		return result;
	}
	CesiumGltf::PropertyTableView view(*this->m_model, extension->propertyTables[tableIndex]);
	if (view.status() != CesiumGltf::PropertyTableViewStatus::Valid || featureId < 0 || featureId >= view.size()) {
		return result;
	}
	view.forEachProperty([&result, featureId](const std::string& propertyId, auto propertyView) {
		Variant value;
		if (propertyView.status() == CesiumGltf::PropertyTablePropertyViewStatus::Valid ||
			propertyView.status() == CesiumGltf::PropertyTablePropertyViewStatus::EmptyPropertyWithDefault) {
			value = to_variant(propertyView.get(featureId));
		}
		result[String::utf8(propertyId.c_str())] = value;
	});
	return result;
}

const Dictionary& TileMetadata::get_table(int32_t index) const {
	if (index < 0 || index >= static_cast<int32_t>(this->m_tables.size())) return this->m_empty;
	if (this->m_tableBuilt[index]) return this->m_tables[index];

	const CesiumGltf::ExtensionModelExtStructuralMetadata* extension = this->get_extension();
	if (extension == nullptr || index >= static_cast<int32_t>(extension->propertyTables.size())) {
		return this->m_tables[index];
	}
	CesiumGltf::PropertyTableView view(*this->m_model, extension->propertyTables[index]);
	if (view.status() != CesiumGltf::PropertyTableViewStatus::Valid) {
		this->m_tableBuilt[index] = true;
		return this->m_tables[index];
	}

	Dictionary& table = this->m_tables[index];
	view.forEachProperty([&table](const std::string& propertyId, auto propertyView) {
		Array values;
		const bool readable = propertyView.status() == CesiumGltf::PropertyTablePropertyViewStatus::Valid ||
			propertyView.status() == CesiumGltf::PropertyTablePropertyViewStatus::EmptyPropertyWithDefault;
		const int64_t count = readable ? propertyView.size() : 0;
		values.resize(count);
		for (int64_t row = 0; row < count; row++) {
			values[row] = to_variant(propertyView.get(row));
		}
		Ref<CesiumPropertyInfo> info = memnew(CesiumPropertyInfo);
		info->data = values;
		table[String::utf8(propertyId.c_str())] = info;
	});
	this->m_tableBuilt[index] = true;
	return table;
}
