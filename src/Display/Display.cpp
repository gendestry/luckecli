#include "Display.h"
#include "CLIDisplay.h"
#include "FTXUIDisplay.h"

namespace Test
{

    std::unique_ptr<Display> makeDisplay(DisplayType type, SharedState &state)
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
