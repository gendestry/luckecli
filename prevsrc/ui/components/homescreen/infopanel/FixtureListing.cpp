#include "ui/components/homescreen/infopanel/FixtureListing.h"
#include "ui/Theme.h"
#include "ui/components/homescreen/infopanel/InfoStyle.h"
#include "core/domain/SharedState.h"

namespace ui
{
    using namespace ftxui;

    namespace
    {
        // One fixture's read-only detail block, or a "gone" note if it's no longer
        // cached.
        Element fixtureBlock(const core::Client &client, const std::string &ip, int fixtureId)
        {
            if (fixtureId < 0 || fixtureId >= static_cast<int>(client.fixtures.size()))
                return text(" fixture " + std::to_string(fixtureId) + " on " + ip + " is gone") |
                       color(Theme::errColor());

            const auto &f = client.fixtures[fixtureId];

            Elements rows;
            rows.push_back(hbox({
                text(f.name) | bold | color(Theme::accent()),
                text("  "),
                text(f.type) | color(Theme::muted()),
            }));
            rows.push_back(hbox({infoLabel("  ip        "), text(ip) | color(Theme::ipColor())}));
            rows.push_back(hbox({infoLabel("  universe  "), infoValue(std::to_string(f.universe))}));
            rows.push_back(hbox({infoLabel("  address   "), infoValue(std::to_string(f.address))}));
            rows.push_back(hbox({infoLabel("  footprint "), infoValue(std::to_string(f.footprint))}));
            // Presets are rendered separately by PresetDropdown (interactive when
            // a single fixture is selected), so they're intentionally omitted here.

            return vbox(std::move(rows));
        }
    }

    Element fixtureListing(core::SharedState &state,
                           const std::vector<core::commands::CommandExecutor::Selection> &selection)
    {
        if (selection.empty())
            return text(" nothing selected") | dim | center;

        // Index the snapshot by IP so several selected fixtures on one device
        // each get a fresh lookup.
        const auto clients = state.snapshot();

        Elements blocks;
        for (std::size_t i = 0; i < selection.size(); ++i)
        {
            const auto &sel = selection[i];
            if (i)
                blocks.push_back(separator());

            const core::Client *client = findClient(clients, sel.ip);
            if (!client)
                blocks.push_back(text(" " + sel.ip + " offline") | color(Theme::errColor()));
            else
                blocks.push_back(fixtureBlock(*client, sel.ip, sel.fixtureId));
        }

        return vbox(std::move(blocks));
    }

}
