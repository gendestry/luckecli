#include "ui/FixtureGrid.h"
#include "ui/components/FixtureCard.h"
#include "core/domain/SharedState.h"
#include "core/commands/CommandExecutor.h"

#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <vector>

namespace ui
{
    using namespace ftxui;

    // A device counts as "online" if its last heartbeat arrived within this
    // window; otherwise its card's ping square goes red.
    static constexpr auto kOnlineWindow = std::chrono::seconds(5);

    FixtureGrid::FixtureGrid(core::SharedState &state, core::commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec), m_container(Container::Vertical({})) {}

    void FixtureGrid::rebuild()
    {
        // Snapshot the current selection so we can mark the matching cards.
        std::vector<core::commands::CommandExecutor::Selection> selected;
        if (m_exec)
            selected = m_exec->selection();

        const auto now = std::chrono::system_clock::now();

        // Gather the fixtures in snapshot order first. The flat index computed
        // here is the canonical one shared with the executor (flatten()/
        // selectIndex()/the `select` command), so it must be assigned in
        // snapshot order and carried on the card even if we reorder for display.
        m_onlineCount = 0;
        int index = 0;
        std::vector<FixtureCardData> entries;
        for (const auto &[ip, client] : m_state.snapshot())
        {
            const bool online = (now - client.last_ping) < kOnlineWindow;
            for (int fid = 0; fid < static_cast<int>(client.fixtures.size()); ++fid)
            {
                const auto &fx = client.fixtures[fid];
                if (online)
                    ++m_onlineCount;

                bool isSel = false;
                for (const auto &s : selected)
                    if (s.ip == ip && s.fixtureId == fid)
                    {
                        isSel = true;
                        break;
                    }

                const int idx = index++;
                entries.push_back(FixtureCardData{idx, fx.name, fx.type, ip, fx.universe, online, isSel});
            }
        }

        // Optional display-only sort by name (case-insensitive, ties broken by
        // the canonical index for stability). The idx field stays untouched.
        if (m_sortByName)
            std::sort(entries.begin(), entries.end(),
                      [](const FixtureCardData &a, const FixtureCardData &b)
                      {
                          auto lower = [](std::string s)
                          {
                              std::transform(s.begin(), s.end(), s.begin(),
                                             [](unsigned char c) { return std::tolower(c); });
                              return s;
                          };
                          const auto na = lower(a.name), nb = lower(b.name);
                          if (na != nb)
                              return na < nb;
                          return a.index < b.index;
                      });

        Components cards;
        for (const auto &data : entries)
        {
            const int idx = data.index;

            // Plain click replaces the selection; an additive click (modifier
            // held or multi-mode on) toggles it. The host's onSelect hook
            // schedules the rebuild/redraw afterwards (off the event handler,
            // so the cards aren't swapped out from under the click).
            auto onClick = [this, idx]()
            {
                if (m_canSelect && !m_canSelect())
                    return; // selection frozen (a command is in flight)
                if (m_exec)
                    m_exec->selectIndex(idx, m_additive && m_additive());
                if (m_onSelect)
                    m_onSelect();
            };

            cards.push_back(FixtureCard(data, std::move(onClick)));
        }

        m_container->DetachAllChildren();
        if (cards.empty())
        {
            m_container->Add(Renderer([]
                                      { return text("  waiting for devices...") | dim | center; }));
            return;
        }

        for (int i = 0; i < static_cast<int>(cards.size()); i += m_cols)
        {
            Components row;
            for (int j = i; j < std::min<int>(cards.size(), i + m_cols); ++j)
                row.push_back(cards[j]);
            m_container->Add(Container::Horizontal(std::move(row)));
        }
    }

}
