#include "ui/components/homescreen/UniverseGrid.h"
#include "core/domain/SharedState.h"

#include <array>

namespace ui
{
    using namespace ftxui;

    namespace
    {
        constexpr int kCols = 32;
        constexpr int kRows = 16;
        constexpr int kChannels = kCols * kRows; // 512

        // Distinct, readable fill colours cycled per selected fixture. Built at
        // call time (Color::RGB touches FTXUI globals, so no namespace-scope Color).
        Color slotColor(int i)
        {
            static constexpr int rgb[][3] = {
                {90, 200, 110},  // green
                {100, 150, 220}, // blue
                {220, 190, 100}, // amber
                {200, 120, 200}, // magenta
                {100, 200, 200}, // cyan
                {230, 140, 90},  // orange
            };
            const auto &c = rgb[i % static_cast<int>(std::size(rgb))];
            return Color::RGB(c[0], c[1], c[2]);
        }

        const core::Client *findClient(const std::vector<std::pair<std::string, core::Client>> &clients,
                                 const std::string &ip)
        {
            for (const auto &[cip, c] : clients)
                if (cip == ip)
                    return &c;
            return nullptr;
        }
    }

    Element universeGrid(core::SharedState &state,
                         const std::vector<core::commands::CommandExecutor::Selection> &selection)
    {
        const auto clients = state.snapshot();

        // -1 = empty; otherwise the colour slot of the owning fixture.
        std::array<int, kChannels> owner;
        owner.fill(-1);
        std::array<bool, kChannels> conflict{};

        int universe = -1; // the universe we're drawing (first selection's)

        for (std::size_t s = 0; s < selection.size(); ++s)
        {
            const auto &sel = selection[s];
            const core::Client *c = findClient(clients, sel.ip);
            if (!c || sel.fixtureId < 0 || sel.fixtureId >= static_cast<int>(c->fixtures.size()))
                continue;

            const auto &fx = c->fixtures[sel.fixtureId];
            if (universe < 0)
                universe = fx.universe;
            if (fx.universe != universe)
                continue; // only fixtures on the shown universe are mapped

            for (int ch = fx.address; ch < fx.address + fx.footprint; ++ch)
            {
                if (ch < 0 || ch >= kChannels)
                    continue;
                if (owner[ch] != -1)
                    conflict[ch] = true;
                owner[ch] = static_cast<int>(s);
            }
        }

        // The grid itself: one cell per channel, filled blocks for occupied.
        Elements rows;
        for (int r = 0; r < kRows; ++r)
        {
            Elements cells;
            for (int col = 0; col < kCols; ++col)
            {
                const int ch = r * kCols + col;
                if (conflict[ch])
                    cells.push_back(text("█") | color(Color::RGB(220, 80, 80)));
                else if (owner[ch] >= 0)
                    cells.push_back(text("█") | color(slotColor(owner[ch])));
                else
                    cells.push_back(text("·") | color(Color::RGB(60, 60, 70)));
            }
            rows.push_back(hbox(std::move(cells)));
        }

        auto title = text(universe >= 0 ? " Universe " + std::to_string(universe) : " Universe —") |
                     color(Color::RGB(80, 190, 190));

        return vbox({
            title,
            vbox(std::move(rows)) | border,
        });
    }

}
