// #include "Commands/CommandExecutor.h"
#include "Test/SharedState.h"
#include "Test/Heartbeat.h"
#include "Test/ESPClient.h"
#include "Test/Network/ResponseListener.h"
#include "JSONtemp.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include "Diary/Log.h"
// #include "Display/CLIDisplay.h"
// #include "Display/FTXUIDisplay.h"

// #include <memory>

int main()
{
    Test::SharedState state;
    Log log("main");

    // auto display = std::make_unique<CLIDisplay>();
    // auto display = std::make_unique<FTXUIDisplay>(state);

    // Log::setGlobalCallback([&display](const Loggable &l)
    //                        { display->onLog(l); });

    // state.onChangeCallback = [&display]()
    // {
    //     display->onStateChanged();
    // };

    Test::Heartbeat heartbeat(state);
    Test::Network::ResponseListener listener(state);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    Test::ESPClient client(state.getClient("192.168.0.143"), state);
    auto response = client.sendRequestOpt(JSONtemp::stringify("describe"), 4000ms);

    if (response.has_value())
    {
        auto resp = response.value();
        std::cout << resp << std::endl;
        if (resp.empty())
            return false;

        using nlohmann::json;
        json data;
        try
        {
            data = json::parse(resp);
        }
        catch (const json::parse_error &e)
        {
            log.error("JSON parse error: {}", e.what());
            return false;
        }
        if (data.contains("err"))
        {
            log.error("{}", data["err"].get<std::string>());
            return false;
        }

        std::cout << data.flatten() << std::endl;

        Test::Client client;
        client.engine.version = data.value("version", "?");
        client.engine.serial_report = data.value("serial_report_task", false);
        client.engine.wireless_report = data.value("wireless_report_task", false);
        // client.engine.data.value("wifi_animation", false);

        // log.println("{}Engine{}", Theme::heading(), Theme::r());
        // log.println("  {}version:{}       {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("version", "?"), Theme::r());
        // log.println("  {}serial print:{}  {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("serial_report_task", false), Theme::r());
        // log.println("  {}wifi print:{}    {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("wireless_report_task", false), Theme::r());
        // log.println("  {}wifi anim:{}     {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("wifi_animation", false), Theme::r());

        if (data.contains("wifi"))
        {
            auto &wifi = data["wifi"];
            client.wifi.connected = wifi.value("connected", false);
            client.wifi.ip = wifi.value("local_ip", "?");
            client.wifi.rssi = wifi.value("rssi", 0);

            client.wifi.ssid = data.value("ssid", "?");
            client.wifi.password = data.value("password", "?");
        }

        int a = 0;
    }

    int counter = 0;
    while (true)
    {
        counter++;
        if (counter == 100)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    listener.stop();
    // client.stop();

    // CommandExecutor exec(state, *display);
    // display->bindCommandSource(&exec);

    // exec.run();

    return 0;
}
