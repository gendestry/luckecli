#pragma once
#include <ftxui/component/component.hpp>

namespace ui
{

    // A tab-hostable view. Unlike View, a Panel does not own a screen or event
    // loop: it just contributes an interactive component tree that the host
    // (HomeScreen) slots into a Container::Tab. The host runs the single
    // screen.Loop and posts sync() on the panels when the model changes.
    class Panel
    {
    public:
        virtual ~Panel() = default;

        // The panel's interactive tree, including its own Renderer. Built once and
        // kept alive by the host, so the panel's focus/scroll state survives tab
        // switches.
        virtual ftxui::Component component() = 0;

        // Refresh the panel's widgets from the shared model. Called on the UI
        // thread (via screen.Post) after a command completes or on the ping tick.
        virtual void sync() {}

        // Short label shown in the tab bar.
        virtual const char *title() const = 0;
    };

}
