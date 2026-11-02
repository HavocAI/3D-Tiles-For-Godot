#include "CesiumGDCreditSystem.h"

#include "CesiumUtility/CreditSystem.h"
#include "Implementations/DocumentContainer.h"
#include "Utils/AssetManipulation.h"
#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/vector2.hpp"
#include "missing_functions.hpp"
#include "godot_cpp/classes/link_button.hpp"
#include "godot_cpp/classes/panel_container.hpp"
#include "godot_cpp/variant/callable.hpp"
#include "godot_cpp/variant/rect2.hpp"
#include <unordered_set>

CesiumGDCreditSystem* CesiumGDCreditSystem::get_singleton(Node3D* baseNode) {
  if (s_instance != nullptr) {
    return s_instance;
  }
  s_instance = Godot3DTiles::AssetManipulation::find_or_create_credit_system(baseNode, false);
  if (s_instance == nullptr) {
    ERR_PRINT("Could not find Credit System Node in the CesiumGeoreference, try adding it manually");
    return nullptr;
  }
  return s_instance;
}

void CesiumGDCreditSystem::_process(double p_delta) {
  this->update_credits();
}


void CesiumGDCreditSystem::update_credits() {
  if (is_editor_mode()) return;

  // Several tilesets often carry the same credit (e.g. the Cesium ion logo); list each once.
  std::vector<std::string> onScreen;
  std::vector<std::string> collapsible;
  std::unordered_set<std::string> seen;
  for (const auto& creditSystem : this->m_creditSystems) {
    const auto& creditSnapshot = creditSystem->getSnapshot();
    for (const CesiumUtility::Credit& credit : creditSnapshot.currentCredits) {
      const std::string& html = creditSystem->getHtml(credit);
      if (!seen.insert(html).second) continue;
      if (creditSystem->shouldBeShownOnScreen(credit)) {
        onScreen.push_back(html);
      } else {
        collapsible.push_back(html);
      }
    }
  }

  std::string allHtml;
  for (const std::string& html : onScreen) allHtml += html + "\n";
  for (const std::string& html : collapsible) allHtml += html + "\n";

  std::string barHtml = allHtml;
  if (this->m_collapsed) {
    barHtml.clear();
    for (const std::string& html : onScreen) barHtml += html + "\n";
  }
  // Show the link whenever collapsing hides something; the panel lists every credit.
  const bool showLink = this->m_collapsed && !collapsible.empty();

  if (barHtml != this->m_lastHtml) {
    this->m_lastHtml = barHtml;
    this->m_rect->set_html_stl(barHtml);
  }

  if (!showLink) {
    if (this->m_attributionLink != nullptr) {
      this->m_attributionLink->hide();
      this->m_attributionPanel->hide();
    }
    return;
  }

  this->ensure_attribution_controls();
  this->m_attributionLink->show();
  if (allHtml != this->m_lastPanelHtml) {
    this->m_lastPanelHtml = allHtml;
    // One credit per line in the panel.
    std::string panelHtml;
    for (const std::string& html : onScreen) panelHtml += "<div>" + html + "</div>\n";
    for (const std::string& html : collapsible) panelHtml += "<div>" + html + "</div>\n";
    this->m_panelDocument->set_html_stl(panelHtml);
  }
  if (this->m_attributionPanel->is_visible()) {
    this->position_attribution_panel();
  }
}

void CesiumGDCreditSystem::ensure_attribution_controls() {
  if (this->m_attributionLink != nullptr) return;

  this->m_attributionLink = memnew(LinkButton);
  this->m_attributionLink->set_text("Data attribution");
  this->m_attributionLink->set_v_size_flags(Control::SIZE_SHRINK_END);
  this->m_attributionLink->connect("pressed", Callable(this, "toggle_attribution_panel"));
  this->add_child(this->m_attributionLink, false, INTERNAL_MODE_BACK);

  // Top level so the box container does not lay it out and the parent's scale does not shrink it.
  this->m_attributionPanel = memnew(PanelContainer);
  this->m_attributionPanel->set_as_top_level(true);
  this->m_attributionPanel->hide();
  this->m_panelDocument = memnew(DocumentContainer);
  this->m_attributionPanel->add_child(this->m_panelDocument);
  this->add_child(this->m_attributionPanel, false, INTERNAL_MODE_BACK);
}

void CesiumGDCreditSystem::position_attribution_panel() {
  // Just above the credit bar's top-left corner, kept inside the viewport.
  const Rect2 bar = this->get_global_rect();
  const Vector2 size = this->m_attributionPanel->get_combined_minimum_size();
  Vector2 position(bar.position.x, bar.position.y - size.y);
  if (position.y < 0.0f) position.y = 0.0f;
  this->m_attributionPanel->set_size(size);
  this->m_attributionPanel->set_global_position(position);
}

void CesiumGDCreditSystem::toggle_attribution_panel() {
  if (this->m_attributionPanel == nullptr) return;
  this->m_attributionPanel->set_visible(!this->m_attributionPanel->is_visible());
  if (this->m_attributionPanel->is_visible()) {
    this->position_attribution_panel();
  }
}

void CesiumGDCreditSystem::set_collapsed(bool collapsed) {
  this->m_collapsed = collapsed;
}

bool CesiumGDCreditSystem::is_collapsed() const {
  return this->m_collapsed;
}

void CesiumGDCreditSystem::set_credits_collapsed(bool collapsed) {
  s_collapseByDefault = collapsed;
  if (s_instance != nullptr) {
    s_instance->set_collapsed(collapsed);
  }
}

  void CesiumGDCreditSystem::add_credit_system(std::shared_ptr<CesiumUtility::CreditSystem> creditSystem) {
  this->m_creditSystems.emplace_back(creditSystem);
}

void CesiumGDCreditSystem::_enter_tree() {
  if (!is_editor_mode()){
    this->set_process(true);
  }   
  // Create the HTML rect and set its html to something to test it
  s_instance = this;
  this->m_collapsed = s_collapseByDefault;
  
  this->m_creditSystems.reserve(3);
  if (this->get_child_count() > 0) {
      this->m_rect = Object::cast_to<DocumentContainer>(this->get_child(0));
      if (this->m_rect != nullptr) {
        return;
      }
  }

  
  this->set_anchors_preset(Control::LayoutPreset::PRESET_BOTTOM_LEFT);
  this->set_offset(Side::SIDE_TOP, -150.0f);
  this->set_offset(Side::SIDE_LEFT, 0.0f);
  this->set_offset(Side::SIDE_BOTTOM, 0.0f);
  this->m_rect = memnew(DocumentContainer);
  this->m_rect->set_size(Vector2(500, 100));
  this->add_child(this->m_rect, false, INTERNAL_MODE_FRONT);
  this->m_rect->set_owner(this->get_parent());
}

void CesiumGDCreditSystem::turn_off() {
  if (s_instance == nullptr) return;
  s_instance->set_visible(false);
}


void CesiumGDCreditSystem::turn_on() {
  if (s_instance == nullptr) return;
  s_instance->set_visible(true);
}

void CesiumGDCreditSystem::_bind_methods() {
  ClassDB::bind_static_method("CesiumGDCreditSystem", D_METHOD("turn_on"), &CesiumGDCreditSystem::turn_on);
  ClassDB::bind_static_method("CesiumGDCreditSystem", D_METHOD("turn_off"), &CesiumGDCreditSystem::turn_off);
  ClassDB::bind_static_method("CesiumGDCreditSystem", D_METHOD("set_credits_collapsed", "collapsed"), &CesiumGDCreditSystem::set_credits_collapsed);
  ClassDB::bind_method(D_METHOD("set_collapsed", "collapsed"), &CesiumGDCreditSystem::set_collapsed);
  ClassDB::bind_method(D_METHOD("is_collapsed"), &CesiumGDCreditSystem::is_collapsed);
  ClassDB::bind_method(D_METHOD("toggle_attribution_panel"), &CesiumGDCreditSystem::toggle_attribution_panel);
  ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collapsed"), "set_collapsed", "is_collapsed");
}
