#ifndef CESIUM_GD_CREDIT_SYSTEM
#define CESIUM_GD_CREDIT_SYSTEM

#include "Implementations/DocumentContainer.h"
#include "godot_cpp/classes/box_container.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include <memory>
#if defined(CESIUM_GD_EXT)

using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#endif

#include <memory.h>
#include <string>

namespace CesiumUtility {
  class CreditSystem;
}

namespace godot {
  class LinkButton;
  class PanelContainer;
}

class CesiumGDCreditSystem : public BoxContainer {
  GDCLASS(CesiumGDCreditSystem, BoxContainer)
  public:
    static CesiumGDCreditSystem* get_singleton(Node3D* baseNode);

    CesiumGDCreditSystem() = default;
    
    void add_credit_system(std::shared_ptr<CesiumUtility::CreditSystem> creditSystem);
    
    void update_credits();
    
    void _process(double p_delta) override;  

    void _enter_tree() override;

    /// @brief SHOULD NOT BE CALLED UNLESS YOU OWN THE LICENSE TO YOUR DATA!!!
    static void turn_off();
    
    static void turn_on();

    /// @brief When collapsed, only credits that must stay on screen are shown, followed by a
    /// "Data attribution" link that toggles a panel listing every credit.
    void set_collapsed(bool collapsed);

    bool is_collapsed() const;

    /// @brief Collapses or expands the credits of the current overlay and of one created later.
    static void set_credits_collapsed(bool collapsed);

    void toggle_attribution_panel();
    
  private:
    void ensure_attribution_controls();

    void position_attribution_panel();

    // HtmlRect* m_rect;
    DocumentContainer* m_rect;
    std::string m_lastHtml;
    std::string m_lastPanelHtml;
    std::vector<std::shared_ptr<CesiumUtility::CreditSystem>> m_creditSystems;
    bool m_collapsed = false;
    LinkButton* m_attributionLink = nullptr;
    PanelContainer* m_attributionPanel = nullptr;
    DocumentContainer* m_panelDocument = nullptr;
    static inline CesiumGDCreditSystem*  s_instance = nullptr;
    static inline bool s_collapseByDefault = false;
     
  protected:
    static void _bind_methods();
};

#endif
