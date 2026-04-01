// src/ui_components.cpp
#include "astra/ui.h"

namespace astra {

UIContext UIContext::panel(const PanelDesc& desc) {
    Rect content_rect = renderer_->draw_panel(bounds_, desc);
    return UIContext(renderer_, content_rect);
}

void UIContext::progress_bar(const ProgressBarDesc& desc) {
    ProgressBarDesc abs = desc;
    abs.x += bounds_.x;
    abs.y += bounds_.y;
    renderer_->draw_progress_bar(abs.x, abs.y, abs);
}

void UIContext::text(const TextDesc& desc) {
    renderer_->draw_ui_text(bounds_.x + desc.x, bounds_.y + desc.y, desc);
}

void UIContext::styled_text(const StyledTextDesc& desc) {
    renderer_->draw_styled_text(bounds_.x + desc.x, bounds_.y + desc.y, desc);
}

void UIContext::list(const ListDesc& desc) {
    renderer_->draw_list(bounds_, desc);
}

void UIContext::tab_bar(const TabBarDesc& desc) {
    renderer_->draw_tab_bar(bounds_, desc);
}

void UIContext::separator(const SeparatorDesc& desc) {
    renderer_->draw_separator(bounds_, desc);
}

void UIContext::label_value(const LabelValueDesc& desc) {
    renderer_->draw_label_value(bounds_.x + desc.x, bounds_.y + desc.y, desc);
}

void UIContext::galaxy_map(const GalaxyMapDesc& desc) {
    renderer_->draw_galaxy_map(bounds_, desc);
}

} // namespace astra
