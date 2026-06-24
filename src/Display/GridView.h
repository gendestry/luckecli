#pragma once
#include "View.h"
#include <ftxui/component/component.hpp>
#include <functional>

class SharedState;
class CommandExecutor;

// Grid of online-fixture cards. The screen and component tree live for the
// whole grid session; only the card children are rebuilt in place when the
// device set changes (driven by onStateChanged on the UI thread).
class GridView : public View {
public:
    GridView(SharedState& state, CommandExecutor*& exec);

    void run(std::function<bool(const std::string&)>& onCommand) override;
    void onStateChanged() override;   // rebuild the cards in place, then redraw

private:
    SharedState& m_state;
    CommandExecutor*& m_exec;
    ftxui::Components m_buttons;
    std::function<void()> m_rebuild;  // valid only while run() is looping
};
