#include "ui/components/log/LogView.h"
#include "core/commands/CommandExecutor.h"
#include "core/domain/SharedState.h"
#include "core/net/ESPClient.h"
#include "support/util/JSONtemp.h"
#include "ui/Theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace ui
{
    using namespace ftxui;

    // Lines kept per device; older ones are dropped.
    static constexpr std::size_t kMaxLines = 2000;

    LogView::LogView(core::SharedState &state, core::commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec) {}

    std::string LogView::selectedIp() const
    {
        if (!m_exec)
            return {};
        const auto ips = m_exec->selection().ips();
        return ips.empty() ? std::string{} : ips.front();
    }

    void LogView::poll()
    {
        const std::string ip = selectedIp();
        uint32_t since = 0;
        {
            std::lock_guard lock(m_mutex);
            if (ip != m_ip)
            {
                // Another device: start its log from the beginning.
                m_ip = ip;
                m_next = 0;
                m_lines.clear();
                m_status.clear();
                m_scroll = 0;
            }
            since = m_next;
        }
        if (ip.empty() || m_inFlight.exchange(true))
            return;

        m_state.getESPClient(ip)->sendRequestAsync(
            support::JSONtemp::stringify("logs", "since", since), 4000ms,
            [this, ip, alive = std::weak_ptr<int>(m_alive)](std::optional<std::string> resp)
            {
                if (!alive.lock())
                    return;
                {
                    std::lock_guard lock(m_mutex);
                    if (ip == m_ip)
                    {
                        nlohmann::json data = resp ? nlohmann::json::parse(*resp, nullptr, false)
                                                   : nlohmann::json();
                        if (!resp)
                            m_status = "no response";
                        else if (data.is_discarded() || !data.contains("lines"))
                            m_status = "no device log (firmware too old?)";
                        else
                        {
                            m_status.clear();
                            if (const uint32_t lost = data.value("lost", 0u); lost && m_next != 0)
                                m_lines.push_back("-- " + std::to_string(lost) + " lines lost --");
                            for (const auto &line : data["lines"])
                                m_lines.push_back(line.get<std::string>());
                            if (m_lines.size() > kMaxLines)
                                m_lines.erase(m_lines.begin(), m_lines.end() - kMaxLines);
                            m_next = data.value("next", m_next);
                        }
                    }
                }
                m_inFlight = false;
                if (m_onUpdate)
                    m_onUpdate();
            });
    }

    Component LogView::component()
    {
        auto renderer = Renderer([this]
        {
            std::lock_guard lock(m_mutex);

            std::string header = m_ip.empty() ? "no fixture selected" : m_ip;
            for (const auto &[ip, client] : m_state.snapshot())
                if (ip == m_ip && !client.fixtures.empty())
                    header += "  " + client.fixtures.front().name;

            Elements rows;
            if (m_ip.empty())
                rows.push_back(text("Select a fixture on Home to see its device log.") | color(Theme::muted()));
            else if (!m_status.empty())
                rows.push_back(text(m_status) | color(Theme::errColor()));
            else if (m_lines.empty())
                rows.push_back(text("waiting for lines...") | color(Theme::muted()));

            // Newest at the bottom; m_scroll moves the window up.
            const int total = static_cast<int>(m_lines.size());
            const int end = std::max(0, total - m_scroll);
            for (int i = 0; i < end; ++i)
            {
                const std::string &line = m_lines[i];
                Color c = Theme::text();
                if (line.starts_with("E ")) c = Theme::errColor();
                else if (line.starts_with("W ")) c = Theme::warnColor();
                else if (line.starts_with("D ")) c = Theme::debugColor();
                else if (line.starts_with("--")) c = Theme::muted();
                rows.push_back(text(line) | color(c));
            }

            auto title = hbox({
                text(" Device log ") | bold | color(Theme::windowTitle()),
                text(header) | color(Theme::ipColor()),
                filler(),
                text(m_scroll ? " scrolled — end to follow " : " following ") | color(Theme::muted()),
            });
            return vbox({
                title,
                separator(),
                vbox(std::move(rows)) | focusPositionRelative(0.f, 1.f) | yframe | flex,
            }) | flex;
        });

        return CatchEvent(renderer, [this](Event e)
        {
            std::lock_guard lock(m_mutex);
            const int maxScroll = std::max(0, static_cast<int>(m_lines.size()) - 1);
            if (e == Event::ArrowUp || (e.is_mouse() && e.mouse().button == Mouse::WheelUp))
            {
                m_scroll = std::min(m_scroll + (e.is_mouse() ? 3 : 1), maxScroll);
                return true;
            }
            if (e == Event::ArrowDown || (e.is_mouse() && e.mouse().button == Mouse::WheelDown))
            {
                m_scroll = std::max(m_scroll - (e.is_mouse() ? 3 : 1), 0);
                return true;
            }
            if (e == Event::PageUp) { m_scroll = std::min(m_scroll + 20, maxScroll); return true; }
            if (e == Event::PageDown) { m_scroll = std::max(m_scroll - 20, 0); return true; }
            if (e == Event::End) { m_scroll = 0; return true; }
            if (e == Event::Character('c'))
            {
                // Clear the view; keep fetching from where we are.
                m_lines.clear();
                m_scroll = 0;
                return true;
            }
            return false;
        });
    }

}
