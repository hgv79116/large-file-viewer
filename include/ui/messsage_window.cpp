#include "message_window.hpp"

#include <ftxui/dom/elements.hpp>

ftxui::Element  MessageWindow::getFormattedMessage() {
    using namespace ftxui;
    if (!_displayed_message) {
        return text("");
    }

    if (_displayed_message->type == MessageType::ERROR) {
        return paragraph("Error: " + _displayed_message->content + ". Press Escape to continue.")
                | color(Color::RedLight) | bold;
    }

    if (_displayed_message->type == MessageType::WARNING) {
        return paragraph("Warning: " + _displayed_message->content + ". Press Escape to continue.")
                | color(Color::Yellow) | bold;
    }

    // Info
    return paragraph(_displayed_message->content) | color(Color::LightCoral) | bold;
}

bool MessageWindow::OnEvent(ftxui::Event event) {
    // Release lock if locking the screen and the event match
    if (_required_response && _required_response == event) {
        clear();
        return true;
    }

    if (ComponentBase::OnEvent(event)) {
        return true;
    }

    return false;
};