#include "InfoPanel.h"
#include "SharedState.h"

#include <string>

namespace Test
{
    using namespace ftxui;

    namespace
    {
        Element label(const std::string &s) { return text(s) | color(Color::RGB(80, 190, 190)); }
        Element value(const std::string &s) { return text(s) | color(Color::RGB(220, 190, 100)); }

        // One fixture's detail block, or a "gone" note if it's no longer cached.
        Element fixtureBlock(const Client &client, const std::string &ip, int fixtureId)
        {
            if (fixtureId < 0 || fixtureId >= static_cast<int>(client.fixtures.size()))
                return text(" fixture " + std::to_string(fixtureId) + " on " + ip + " is gone") |
                       color(Color::RGB(210, 100, 100));

            const auto &f = client.fixtures[fixtureId];

            Elements rows;
            rows.push_back(hbox({
                text(f.name) | bold | color(Color::RGB(190, 160, 230)),
                text("  "),
                text(f.type) | color(Color::RGB(130, 130, 130)),
            }));
            rows.push_back(hbox({label("  ip        "), text(ip) | color(Color::RGB(140, 170, 210))}));
            rows.push_back(hbox({label("  universe  "), value(std::to_string(f.universe))}));
            rows.push_back(hbox({label("  address   "), value(std::to_string(f.address))}));
            rows.push_back(hbox({label("  footprint "), value(std::to_string(f.footprint))}));
            // Presets are rendered separately by PresetDropdown (interactive when
            // a single fixture is selected), so they're intentionally omitted here.

            return vbox(std::move(rows));
        }
    }

    Element infoPanel(SharedState &state,
                      const std::vector<Commands::CommandExecutor::Selection> &selection)
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

            const Client *client = nullptr;
            for (const auto &[ip, c] : clients)
                if (ip == sel.ip)
                {
                    client = &c;
                    break;
                }

            if (!client)
                blocks.push_back(text(" " + sel.ip + " offline") | color(Color::RGB(210, 100, 100)));
            else
                blocks.push_back(fixtureBlock(*client, sel.ip, sel.fixtureId));
        }

        return vbox(std::move(blocks));
    }

}
