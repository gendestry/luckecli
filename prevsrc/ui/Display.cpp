#include "ui/Display.h"
#include "ui/CLIDisplay.h"
#include "ui/FTXUIDisplay.h"

namespace ui
{

    std::unique_ptr<Display> makeDisplay(DisplayType type, core::SharedState &state)
    {
        switch (type)
        {
        case DisplayType::FTXUI:
            return std::make_unique<FTXUIDisplay>(state);
        case DisplayType::CLI:
        default:
            return std::make_unique<CLIDisplay>();
        }
    }

}
