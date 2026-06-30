#include "core/domain/Client.h"
#include "ui/Theme.h"
#include <format>
#include <sstream>

namespace core
{
    Client::Client()
    {
        last_ping = std::chrono::system_clock::now();
    }

    // void Client::connect()
    // {
    //     connection = std::make_shared<core::net::Connection>(wifi.ip, )
    // }

    void Client::updateLastPing()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now - last_ping;

        // Convert to seconds or milliseconds (optional)
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        last_ping_str = std::format("{} ms", milliseconds);
        last_ping = now;
    }

    std::string Client::toString() const
    {
        std::ostringstream out;

        out << std::format("{}Engine{}\n", Theme::heading(), Theme::r());
        out << std::format("  {}version:{}       {}{}{}\n", Theme::lbl(), Theme::r(), Theme::val(), engine.version, Theme::r());
        out << std::format("  {}serial print:{}  {}{}{}\n", Theme::lbl(), Theme::r(), Theme::val(), engine.serial_report, Theme::r());
        out << std::format("  {}wifi print:{}    {}{}{}\n", Theme::lbl(), Theme::r(), Theme::val(), engine.wireless_report, Theme::r());
        out << std::format("  {}wifi anim:{}     {}{}{}\n", Theme::lbl(), Theme::r(), Theme::val(), engine.wifi_animation, Theme::r());

        const auto connCol = wifi.connected ? Theme::ok() : Theme::err();
        out << std::format("{}Wifi{}\n", Theme::heading(), Theme::r());
        out << std::format("  {}connected:{}  {}{}{}\n", Theme::lbl(), Theme::r(), connCol, wifi.connected, Theme::r());
        out << std::format("  {}ip:{}         {}{}{}\n", Theme::lbl(), Theme::r(), Theme::ip(), wifi.ip, Theme::r());
        out << std::format("  {}rssi:{}       {}{}{}\n", Theme::lbl(), Theme::r(), Theme::val(), wifi.rssi, Theme::r());
        out << std::format("  {}ssid:{}       {}{}{}\n", Theme::lbl(), Theme::r(), Theme::val(), wifi.ssid, Theme::r());
        out << std::format("  {}password:{}   {}{}{}\n", Theme::lbl(), Theme::r(), Theme::val(), wifi.password, Theme::r());

        out << std::format("{}Fixtures{} ({})\n", Theme::heading(), Theme::r(), fixtures.size());
        for (const auto &f : fixtures)
        {
            out << std::format("  [{}] {}{}{}  {}{}{}\n",
                               f.id,
                               Theme::name(), f.name, Theme::r(),
                               Theme::dim(), f.type, Theme::r());
            out << std::format("      {}uni:{} {}{}{}  {}addr:{} {}{}{}  {}preset:{} {}{}/{}{}  {}footprint:{} {}{}{}\n",
                               Theme::lbl(), Theme::r(), Theme::val(), f.universe, Theme::r(),
                               Theme::lbl(), Theme::r(), Theme::val(), f.address, Theme::r(),
                               Theme::lbl(), Theme::r(), Theme::val(), f.presetIndex, f.numPresets, Theme::r(),
                               Theme::lbl(), Theme::r(), Theme::val(), f.footprint, Theme::r());
            for (std::size_t p = 0; p < f.presets.size(); ++p)
            {
                const bool current = (static_cast<int>(p) == f.presetIndex);
                out << std::format("      {}{}[{}] {}{}\n",
                                   current ? Theme::ok() : Theme::dim(),
                                   current ? "* " : "  ", p, f.presets[p], Theme::r());
            }
        }

        return out.str();
    }
}