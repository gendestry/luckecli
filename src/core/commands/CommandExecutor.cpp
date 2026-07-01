#include "core/commands/CommandExecutor.h"
#include "core/commands/legacy/Commands.h"
#include "ui/Display.h"
#include "core/net/ESPClient.h"
#include "core/export/GdtfExporter.h"
#include "ui/Theme.h"
#include "core/commands/Tokenizer.h"
#include "support/util/JSONtemp.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>

namespace core::commands {

namespace {
constexpr std::size_t kMaxHistory = 1000;

std::string historyPath() {
  const char *home = std::getenv("HOME");
  return (home ? std::string(home) : std::string(".")) + "/.luckecli_history";
}

std::vector<std::string> loadHistory(const std::string &path) {
  std::vector<std::string> hist;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line))
    if (!line.empty())
      hist.push_back(line);
  if (hist.size() > kMaxHistory)
    hist.erase(hist.begin(), hist.end() - kMaxHistory);
  return hist;
}

void saveHistory(const std::string &path, const std::vector<std::string> &hist) {
  std::ofstream out(path, std::ios::trunc);
  for (const auto &h : hist)
    out << h << '\n';
}
} // namespace

CommandExecutor::CommandExecutor(SharedState &state, ui::Display &display)
    : log("Command"), m_sharedState(state), m_display(display) {
  m_history = loadHistory(historyPath());
  bindCommands();
  m_display.bindCommandSource(this);
}

// Append a command to the shared history and persist it. Skips blanks and
// consecutive duplicates; keeps at most kMaxHistory entries.
void CommandExecutor::recordHistory(const std::string &line) {
  if (line.empty() || (!m_history.empty() && m_history.back() == line))
    return;
  m_history.push_back(line);
  if (m_history.size() > kMaxHistory)
    m_history.erase(m_history.begin());
  saveHistory(historyPath(), m_history);
}

// Flatten the device registry into an ordered (ip, fixtureId) list so a
// single index can address any fixture across all devices.
static std::vector<CommandExecutor::Selection> flatten(SharedState &state);

void CommandExecutor::run() {
  m_display.setPrompt(selectionPrompt());
  m_display.run([this](const std::string &line) {
    resolveCommand(line);
    m_display.setPrompt(selectionPrompt());
    return m_exit;
  });
}

bool CommandExecutor::resolveCommand(const std::string &line) {
  recordHistory(line);

  Args args = tokenize(line);

  auto cmd = m_registry.resolve(args, mode());
  if (!cmd) {
    // Empty line defaults to `list` when unselected — only complain on
    // a non-empty, unrecognised command.
    if (!args.empty())
      log.error("Invalid command: {}", line);
    else if (m_selection.empty())
      return list(args);
    return false;
  }

  return cmd->func(args);
}

void CommandExecutor::bindCommands() {
  m_registry.group("Generic");

  m_registry.add("exit", "Exit the program", "exit", Command::Usable::ANYTIME,
                 [this](const Args &a) { return exit(a); });

  m_registry.add("help", "Print help", "help <command>?",
                 Command::Usable::ANYTIME,
                 [this](const Args &a) { return help(a); });

  m_registry.add("list", "List devices and fixtures", "list",
                 Command::Usable::UNSELECTED,
                 [this](const Args &a) { return list(a); });

  m_registry.add("select", "Select fixture(s) by index", "select <id> <id>...",
                 Command::Usable::UNSELECTED,
                 [this](const Args &a) { return select(a); });

  // Bare "<id> ..." (no `select` keyword) also selects, as long as the
  // first token is an integer.
  m_registry.addPredicate(
      "<id>", "Select fixture(s) by index", "<id> <id>...",
      Command::Usable::UNSELECTED, [](const Args &a) { return a.isInt(0); },
      [this](const Args &a) { return select(a); });

  m_registry.group("Selection");

  m_registry.add(std::vector<std::string>{"back", "b"}, "Clear the selection",
                 "back", Command::Usable::SELECTED,
                 [this](const Args &a) { return deselect(a); });

  m_registry.add("setuniverse", "Set universe on selected fixture(s)",
                 "setuniverse <universe>", Command::Usable::SELECTED,
                 [this](const Args &a) { return setuniverse(a); });

  m_registry.add("setaddress", "Set DMX address on the selected fixture",
                 "setaddress <address>", Command::Usable::SELECTED,
                 [this](const Args &a) { return setaddress(a); });

  m_registry.add("setpreset", "Set preset index on the selected fixture",
                 "setpreset <index>", Command::Usable::SELECTED,
                 [this](const Args &a) { return setpreset(a); });

  m_registry.add("setname", "Set the name of the selected fixture",
                 "setname <name>", Command::Usable::SELECTED,
                 [this](const Args &a) { return setname(a); });

  m_registry.add("highlight", "Flash the selected fixture",
                 "highlight <on/off>?", Command::Usable::SELECTED,
                 [this](const Args &a) { return highlight(a); });

  m_registry.add("describe",
                 "Pretty-print describe JSON for selected fixture(s)",
                 "describe", Command::Usable::SELECTED,
                 [this](const Args &a) { return describe(a); });

  m_registry.add("presets", "List presets for the selected fixture(s)",
                 "presets", Command::Usable::SELECTED,
                 [this](const Args &a) { return presets(a); });

  m_registry.add("outputs",
                 "Print the outputs JSON for the selected fixture(s)",
                 "outputs", Command::Usable::SELECTED,
                 [this](const Args &a) { return outputs(a); });

  m_registry.add("exportgdtf", "Export the selected fixture as a .gdtf file",
                 "exportgdtf <path>?", Command::Usable::SELECTED,
                 [this](const Args &a) { return exportgdtf(a); });

  m_registry.add("exportmvr",
                 "Export all fixtures (with addresses) to a .mvr file",
                 "exportmvr <path>?", Command::Usable::ANYTIME,
                 [this](const Args &a) { return exportmvr(a); });

  m_registry.group("Device");

  m_registry.add("reboot", "Reboot the selected device(s)", "reboot",
                 Command::Usable::SELECTED,
                 [this](const Args &a) { return reboot(a); });

  m_registry.add("factoryreset",
                 "Factory-reset the selected device(s) — wipes all config",
                 "factoryreset confirm", Command::Usable::SELECTED,
                 [this](const Args &a) { return factoryreset(a); });

  m_registry.add("setwifi", "Set WiFi credentials on the selected device",
                 "setwifi ssid <value> pass <value>", Command::Usable::SELECTED,
                 [this](const Args &a) { return setwifi(a); });

  m_registry.add("wifianimation", "Get/set the WiFi animation task",
                 "wifianimation <on/off>?", Command::Usable::SELECTED,
                 [this](const Args &a) {
                   return taskConfig(a, "wifianimation", "wifi_animation",
                                     &Client::Engine::wifi_animation);
                 });

  m_registry.add("serialprint", "Get/set the serial report task",
                 "serialprint <on/off>?", Command::Usable::SELECTED,
                 [this](const Args &a) {
                   return taskConfig(a, "serialprint", "serial_report_task",
                                     &Client::Engine::serial_report);
                 });

  m_registry.add("wirelessprint", "Get/set the wireless report task",
                 "wirelessprint <on/off>?", Command::Usable::SELECTED,
                 [this](const Args &a) {
                   return taskConfig(a, "wirelessprint", "wireless_report_task",
                                     &Client::Engine::wireless_report);
                 });

  m_registry.finish();
}

// ── handlers ────────────────────────────────────────────────

bool CommandExecutor::exit(const Args &) {
  m_exit = true;
  m_display.quit(); // let the frontend tear down its loop (CLI or TUI)
  return true;
}

bool CommandExecutor::help(const Args &args) {
  if (args.has(1)) {
    auto cmd = m_registry.get(args[1]);
    if (!cmd) {
      log.error("Unknown command: {}", args[1]);
      return false;
    }
    log.println("{}{}{} {}-{} {}", Theme::name(), cmd->name(), Theme::r(),
                Theme::dim(), Theme::r(), cmd->desc);
    log.println("  {}usage:{} {}{}{}", Theme::lbl(), Theme::r(), Theme::val(),
                cmd->usage, Theme::r());
    return true;
  }

  for (const auto &group : m_registry.getGroups()) {
    bool printedHeader = false;
    for (const auto &cmd : group.commands) {
      if (!cmd->isUsable(mode()) && !cmd->isUsable(Command::Usable::ANYTIME))
        continue;
      if (!printedHeader) {
        log.println("{}{}{}", Theme::heading(), group.name, Theme::r());
        printedHeader = true;
      }
      log.println("  {}{}{}  {}{}{}", Theme::name(), cmd->name(), Theme::r(),
                  Theme::dim(), cmd->desc, Theme::r());
    }
  }
  return true;
}

bool CommandExecutor::list(const Args &) {
  auto devices = m_sharedState.snapshot();
  log.println("{}Devices online{} ({})", Theme::heading(), Theme::r(),
              devices.size());

  int index = 0;
  for (const auto &[ip, client] : devices) {
    log.println("  {}{}{}  {}v{}{}  {}[{}]{}", Theme::ip(), ip, Theme::r(),
                Theme::ver(), client.engine.version, Theme::r(), Theme::dim(),
                client.last_ping_str, Theme::r());
    for (const auto &fx : client.fixtures) {
      log.println("    {}[{}]{} {}{}{}  {}{}{}", Theme::val(), index++,
                  Theme::r(), Theme::name(), fx.name, Theme::r(), Theme::dim(),
                  fx.type, Theme::r());
    }
  }
  return true;
}

bool CommandExecutor::select(const Args &args) {
  auto all = flatten(m_sharedState);

  // Collect every integer token as a flat fixture index. The leading
  // "select" keyword (if present) is not an int, so it's skipped naturally.
  std::vector<Selection> picked;
  for (size_t i = 0; i < args.size(); ++i) {
    auto idx = args.getInt(i);
    if (!idx)
      continue;
    if (*idx < 0 || static_cast<size_t>(*idx) >= all.size()) {
      log.error("Index out of range: {} (have {})", *idx, all.size());
      continue;
    }
    picked.push_back(all[*idx]);
  }

  if (picked.empty()) {
    log.error("Nothing selected");
    return false;
  }

  m_selection = std::move(picked);
  log.println("{}✓{} selected {}{}{} fixture(s)", Theme::ok(), Theme::r(),
              Theme::val(), m_selection.size(), Theme::r());
  return true;
}

bool CommandExecutor::deselect(const Args &) {
  m_selection.clear();
  return true;
}

void CommandExecutor::selectIndex(int flatIndex, bool additive) {
  auto all = flatten(m_sharedState);
  if (flatIndex < 0 || flatIndex >= static_cast<int>(all.size()))
    return;
  const auto pick = all[flatIndex];

  if (!additive) {
    m_selection = {pick};
    return;
  }

  // Toggle: drop it if already selected, otherwise add it.
  auto it = std::find_if(
      m_selection.begin(), m_selection.end(), [&](const Selection &s) {
        return s.ip == pick.ip && s.fixtureId == pick.fixtureId;
      });
  if (it != m_selection.end())
    m_selection.erase(it);
  else
    m_selection.push_back(pick);
}

void CommandExecutor::selectAll() { m_selection = flatten(m_sharedState); }

void CommandExecutor::clearSelection() { m_selection.clear(); }

bool CommandExecutor::setuniverse(const Args &args) {
  auto universe = args.getInt(1);
  if (!universe) {
    log.error("Usage: setuniverse <universe>");
    return false;
  }

  // Scalar shape: apply the same universe to every selected fixture.
  // Range/ascending and per-fixture vector inputs are planned but not yet
  // implemented (see multi-fixture-selection-plan).
  for (const auto &sel : m_selection) {
    auto client = m_sharedState.getESPClient(sel.ip);
    auto resp = client->sendRequestOpt(
        support::JSONtemp::stringify("setfixture", "id", sel.fixtureId,
                                   "universe", *universe),
        4000ms);
    if (!resp) {
      log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
      continue;
    }

    // Success: patch the cached fixture so a later `describe` reflects
    // the new universe without a re-fetch.
    m_sharedState.withClient(sel.ip, [&](Client &c) {
      if (sel.fixtureId >= 0 &&
          sel.fixtureId < static_cast<int>(c.fixtures.size()))
        c.fixtures[sel.fixtureId].universe = *universe;
    });
    okFixture(sel, "universe", std::to_string(*universe));
  }
  return true;
}

bool CommandExecutor::setaddress(const Args &args) {
  return setFixtureField(args, "setaddress", "address",
                         &Client::Fixture::address);
}

bool CommandExecutor::setpreset(const Args &args) {
  // Firmware field is "presetIndex"; the command is the friendlier "setpreset".
  if (!setFixtureField(args, "setpreset", "presetIndex",
                       &Client::Fixture::presetIndex))
    return false;

  // The new preset changes the fixture's channel footprint (computed by the
  // device), so re-describe to pull the updated footprint into the cache.
  // setFixtureField guarantees a single selection on success.
  refreshDevice(m_selection.front().ip);
  return true;
}

bool CommandExecutor::setname(const Args &args) {
  if (!args.has(1)) {
    log.error("Usage: setname <name>");
    return false;
  }
  const std::string &name = args[1];

  // Per-fixture only: a multi-selection has no single target.
  if (m_selection.size() != 1) {
    log.error("setname targets a single fixture (selected {})",
              m_selection.size());
    return false;
  }

  const auto &sel = m_selection.front();
  auto client = m_sharedState.getESPClient(sel.ip);
  auto resp = client->sendRequestOpt(
      support::JSONtemp::stringify("setfixture", "id", sel.fixtureId, "name",
                                 name),
      4000ms);
  if (!resp) {
    log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
    return false;
  }

  // Success: patch the cached fixture so a later `describe`/`list` reflects it.
  m_sharedState.withClient(sel.ip, [&](Client &c) {
    if (sel.fixtureId >= 0 &&
        sel.fixtureId < static_cast<int>(c.fixtures.size()))
      c.fixtures[sel.fixtureId].name = name;
  });
  okFixture(sel, "name", name);
  return true;
}

bool CommandExecutor::setFixtureField(const Args &args,
                                      const std::string &cmdName,
                                      const std::string &jsonKey,
                                      int Client::Fixture::*member) {
  auto value = args.getInt(1);
  if (!value) {
    log.error("Usage: {} <value>", cmdName);
    return false;
  }

  // Per-fixture only: a multi-selection has no single target.
  if (m_selection.size() != 1) {
    log.error("{} targets a single fixture (selected {})", cmdName,
              m_selection.size());
    return false;
  }

  const auto &sel = m_selection.front();
  auto client = m_sharedState.getESPClient(sel.ip);
  auto resp = client->sendRequestOpt(
      support::JSONtemp::stringify("setfixture", "id", sel.fixtureId, jsonKey,
                                 *value),
      4000ms);
  if (!resp) {
    log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
    return false;
  }

  // Success: patch the cached fixture so a later `describe` reflects it.
  m_sharedState.withClient(sel.ip, [&](Client &c) {
    if (sel.fixtureId >= 0 &&
        sel.fixtureId < static_cast<int>(c.fixtures.size()))
      c.fixtures[sel.fixtureId].*member = *value;
  });
  // Friendly label: "setaddress" -> "address", "setpreset" -> "preset".
  okFixture(sel, cmdName.starts_with("set") ? cmdName.substr(3) : cmdName,
            std::to_string(*value));
  return true;
}

bool CommandExecutor::highlight(const Args &args) {
  if (m_selection.empty()) {
    log.error("Nothing selected");
    return false;
  }

  // Default on; `highlight off`/`false`/`0` turns it back off.
  const bool on = !(args[1] == "off" || args[1] == "false" || args[1] == "0");

  // Apply to every selected fixture; one unresponsive device shouldn't stop
  // the rest, so report per-fixture and only fail if none responded.
  bool any = false;
  for (const auto &sel : m_selection) {
    auto client = m_sharedState.getESPClient(sel.ip);
    auto resp = client->sendRequestOpt(
        support::JSONtemp::stringify("setfixture", "id", sel.fixtureId,
                                     "highlight", on),
        4000ms);
    if (!resp) {
      log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
      continue;
    }
    okFixture(sel, "highlight", on ? "on" : "off");
    any = true;
  }
  return any;
}

bool CommandExecutor::reboot(const Args &) {
  auto ips = selectedIps();
  if (ips.empty()) {
    log.error("Nothing selected");
    return false;
  }

  // The device resets on reboot and won't reply, so don't treat a missing
  // response as failure — fire and report.
  for (const auto &ip : ips) {
    m_sharedState.getESPClient(ip)->sendRequestOpt(
        support::JSONtemp::stringify("reboot"), 1000ms);
    okDevice(ip, "rebooting");
  }

  // The selection points at devices that are now going away.
  m_selection.clear();
  return true;
}

bool CommandExecutor::factoryreset(const Args &args) {
  auto ips = selectedIps();
  if (ips.empty()) {
    log.error("Nothing selected");
    return false;
  }

  // Destructive and irreversible — wipes wifi creds, fixtures and engine
  // settings. Require an explicit confirmation token so it can't fire from a
  // stray keystroke or a recalled history line.
  if (args[1] != "confirm") {
    log.error("factoryreset wipes ALL config on {} device(s). Re-run as "
              "'factoryreset confirm' to proceed.",
              ips.size());
    return false;
  }

  // Like reboot, the device resets and won't reply, so a missing response is
  // expected — fire and report.
  for (const auto &ip : ips) {
    m_sharedState.getESPClient(ip)->sendRequestOpt(
        support::JSONtemp::stringify("factoryreset"), 1000ms);
    okDevice(ip, "factory reset");
  }

  // The selection points at devices that are now resetting / going away.
  m_selection.clear();
  return true;
}

bool CommandExecutor::setwifi(const Args &args) {
  auto ssid = args.value({"ssid", "s"});
  auto pass = args.value({"pass", "password", "p"});
  if (!ssid && !pass) {
    log.error("Usage: setwifi ssid <value> pass <value>");
    return false;
  }

  // Device-level: a single target only (a multi-device selection is ambiguous).
  auto ips = selectedIps();
  if (ips.size() != 1) {
    log.error("setwifi targets a single device (selected {})", ips.size());
    return false;
  }
  const std::string &ip = ips.front();

  // Build with nlohmann so credentials are properly escaped (JSONtemp isn't).
  nlohmann::json r;
  r["request"] = "wifi";
  if (ssid)
    r["ssid"] = *ssid;
  if (pass)
    r["password"] = *pass;

  log.println("{}Sending wifi config to {}{}{}...", Theme::dim(), Theme::ip(),
              ip, Theme::r());
  auto resp = m_sharedState.getESPClient(ip)->sendRequestOpt(r.dump(), 4000ms);
  if (!resp) {
    // The device may simply hop to the new network without replying.
    log.error("No response from {} (it may already be reconnecting)", ip);
  }

  // Optimistically reflect the new credentials in the cache.
  m_sharedState.withClient(ip, [&](Client &c) {
    if (ssid)
      c.wifi.ssid = *ssid;
    if (pass)
      c.wifi.password = *pass;
  });

  std::string msg;
  if (ssid && pass)
    msg = std::format("ssid {}{}{}, password set", Theme::val(), *ssid,
                      Theme::r());
  else if (ssid)
    msg = std::format("ssid {}{}{}", Theme::val(), *ssid, Theme::r());
  else
    msg = "password set";
  okDevice(ip, msg);
  return true;
}

std::vector<std::string> CommandExecutor::selectedIps() const {
  std::vector<std::string> ips;
  for (const auto &sel : m_selection)
    if (std::find(ips.begin(), ips.end(), sel.ip) == ips.end())
      ips.push_back(sel.ip);
  return ips;
}

std::string CommandExecutor::selectionPrompt() {
  if (m_selection.empty())
    return "> ";

  std::string out;
  for (size_t i = 0; i < m_selection.size(); ++i) {
    const auto &sel = m_selection[i];

    // Pull the cached fixture name; fall back to "#<id>" if unknown.
    std::string name;
    m_sharedState.withClient(sel.ip, [&](Client &c) {
      if (sel.fixtureId >= 0 &&
          sel.fixtureId < static_cast<int>(c.fixtures.size()))
        name = c.fixtures[sel.fixtureId].name;
    });
    if (name.empty())
      name = std::format("#{}", sel.fixtureId);

    if (i)
      out += ", ";
    out += std::format("{}{}{}", Theme::name(), name, Theme::r());
  }
  out += std::format(" {}>{} ", Theme::dim(), Theme::r());
  return out;
}

bool CommandExecutor::describe(const Args &args) {
  // By default `describe` prints the cached client. `describe -r` /
  // `describe refresh` re-fetches from the device to update the cache.
  const bool force = args[1] == "-r" || args[1] == "refresh";

  // Device-level: one summary per device, even if several of its fixtures
  // are selected.
  for (const auto &ip : selectedIps()) {
    // `describe -r` re-fetches first; plain `describe` prints the cache.
    if (force && !refreshDevice(ip))
      continue;

    m_sharedState.withClient(ip, [&](Client &c) {
      log.println("{}{}{}\n{}", Theme::ip(), ip, Theme::r(), c.toString());
    });
  }
  return true;
}

bool CommandExecutor::refreshDevice(const std::string &ip) {
  auto client = m_sharedState.getESPClient(ip);

  // A device is briefly unresponsive right after a state change (e.g. a
  // preset switch recomputes the channel footprint), so the first describe
  // often times out. Retry a couple times with a short backoff so the
  // refresh — and the footprint it carries — actually lands.
  std::optional<std::string> resp;
  for (int attempt = 0; attempt < 3 && !resp; ++attempt) {
    if (attempt)
      std::this_thread::sleep_for(250ms);
    resp =
        client->sendRequestOpt(support::JSONtemp::stringify("describe"), 4000ms);
  }
  if (!resp) {
    log.error("No response from {}", ip);
    return false;
  }

  // Parse, then fetch presets off-lock (fetchPresets does network I/O),
  // seeding from the cached client, then commit the result under the lock.
  // Dispatches on engine version (v0.96 vs v0.98 differ). describe() rebuilds
  // the fixture list (dropping cached preset names), so fetchPresets restores
  // them — and the fresh describe carries the updated footprint.
  Client parsed;
  m_sharedState.withClient(ip, [&](Client &c) { parsed = c; });
  // Keep the cached preset names around: describe() rebuilds the fixture
  // list from scratch (dropping them), and if a per-fixture re-fetch below
  // fails (the device is still busy) we don't want to wipe the list.
  const auto prevFixtures = parsed.fixtures;
  if (!core::legacy::Execute::describe(resp, parsed)) {
    log.error("Could not parse describe from {}", ip);
    return false;
  }
  core::legacy::Execute::fetchPresets(parsed, *client);

  // Restore preset names for any fixture the re-fetch left empty.
  for (auto &f : parsed.fixtures) {
    if (!f.presets.empty())
      continue;
    for (const auto &p : prevFixtures)
      if (p.id == f.id && !p.presets.empty()) {
        f.presets = p.presets;
        f.numPresets = static_cast<int>(p.presets.size());
        break;
      }
  }

  m_sharedState.withClient(ip, [&](Client &c) { c = std::move(parsed); });
  return true;
}

bool CommandExecutor::presets(const Args &) {
  // Per selected fixture: fetch its presets, merge into the cache and print.
  for (const auto &sel : m_selection) {
    auto client = m_sharedState.getESPClient(sel.ip);
    auto resp = client->sendRequestOpt(
        support::JSONtemp::stringify("getfixture", "id", sel.fixtureId, "value",
                                   "presets"),
        4000ms);
    if (!resp) {
      log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
      continue;
    }

    // Parse (no network) under the lock and copy the fixture out to print.
    Client::Fixture fix;
    bool ok = false;
    m_sharedState.withClient(sel.ip, [&](Client &c) {
      if (sel.fixtureId < 0 ||
          sel.fixtureId >= static_cast<int>(c.fixtures.size()))
        return;
      core::legacy::Execute::presets(resp, c, c.fixtures[sel.fixtureId]);
      fix = c.fixtures[sel.fixtureId];
      ok = true;
    });
    if (!ok) {
      log.error("Unknown fixture {} on {}", sel.fixtureId, sel.ip);
      continue;
    }

    log.println("{}{}{} {}{}{}", Theme::ip(), sel.ip, Theme::r(), Theme::name(),
                fix.name, Theme::r());
    if (fix.presets.empty()) {
      log.println("  {}(no presets){}", Theme::dim(), Theme::r());
      continue;
    }
    for (std::size_t i = 0; i < fix.presets.size(); ++i) {
      const bool current = (static_cast<int>(i) == fix.presetIndex);
      log.println("  {}{}[{}] {}{}", current ? Theme::ok() : Theme::dim(),
                  current ? "* " : "  ", i, fix.presets[i], Theme::r());
    }
  }
  return true;
}

bool CommandExecutor::outputs(const Args &) {
  // Per selected fixture: fetch its outputs and print the raw response.
  for (const auto &sel : m_selection) {
    auto client = m_sharedState.getESPClient(sel.ip);
    auto resp = client->sendRequestOpt(
        support::JSONtemp::stringify("getfixture", "id", sel.fixtureId, "value",
                                   "outputs"),
        4000ms);
    if (!resp || resp->empty()) {
      log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
      continue;
    }

    log.println("{}{}{} fixture {}{}{}", Theme::ip(), sel.ip, Theme::r(),
                Theme::name(), sel.fixtureId, Theme::r());
    log.println("{}", *resp);
  }
  return true;
}

bool CommandExecutor::exportgdtf(const Args &args) {
  // Per-fixture: a GDTF file describes one fixture type.
  if (m_selection.size() != 1) {
    log.error("exportgdtf targets a single fixture (selected {})",
              m_selection.size());
    return false;
  }
  const auto &sel = m_selection.front();

  // Copy the cached fixture out under the lock.
  Client::Fixture fix;
  bool found = false;
  m_sharedState.withClient(sel.ip, [&](Client &c) {
    if (sel.fixtureId >= 0 &&
        sel.fixtureId < static_cast<int>(c.fixtures.size())) {
      fix = c.fixtures[sel.fixtureId];
      found = true;
    }
  });
  if (!found) {
    log.error("Unknown fixture {} on {}", sel.fixtureId, sel.ip);
    return false;
  }

  // Path: explicit arg, else "<name>.gdtf" in the working directory.
  std::string path = args.has(1)
                         ? args[1]
                         : (!fix.name.empty() ? fix.name : "fixture") + ".gdtf";
  if (!path.ends_with(".gdtf"))
    path += ".gdtf";
  // libMVRgdtf's CFileIdentifier mis-parses a bare filename (no '/') as a
  // folder and silently writes nothing, so always hand it an absolute path.
  path = std::filesystem::absolute(path).string();

  auto result = exportFixtureGdtf(fix, path);
  if (!result.ok) {
    log.error("GDTF export failed: {}", result.error);
    return false;
  }
  log.println("{}exported{} {} {}({} channels = {} RGB cells){}", Theme::ok(),
              Theme::r(), path, Theme::dim(), fix.footprint, fix.footprint / 3,
              Theme::r());
  return true;
}

bool CommandExecutor::exportmvr(const Args &args) {
  // Gather every fixture across all devices from a state snapshot.
  std::vector<Client::Fixture> fixtures;
  for (const auto &[ip, c] : m_sharedState.snapshot())
    for (const auto &f : c.fixtures)
      fixtures.push_back(f);

  if (fixtures.empty()) {
    log.error("No fixtures to export");
    return false;
  }

  // Path: explicit arg, else "scene.mvr" in the working directory. Hand
  // libMVRgdtf an absolute path (see exportgdtf for why).
  std::string path = args.has(1) ? args[1] : std::string("scene.mvr");
  if (!path.ends_with(".mvr"))
    path += ".mvr";
  path = std::filesystem::absolute(path).string();

  auto result = exportFixturesMvr(fixtures, path);
  if (!result.ok) {
    log.error("MVR export failed: {}", result.error);
    return false;
  }
  log.println("{}exported{} {} {}({} fixtures){}", Theme::ok(), Theme::r(),
              path, Theme::dim(), fixtures.size(), Theme::r());
  return true;
}

bool CommandExecutor::taskConfig(const Args &args, const std::string &cmdName,
                                 const std::string &field,
                                 bool Client::Engine::*cache) {
  // Device-level: needs exactly one target device.
  auto ips = selectedIps();
  if (ips.size() != 1) {
    log.error("{} targets a single device (selected {})", cmdName, ips.size());
    return false;
  }
  const std::string &ip = ips.front();

  // With an on/off arg → SET; without → GET.
  const bool isSet = args.has(1);
  const bool value = args[1] == "on" || args[1] == "true" || args[1] == "1";

  auto request = isSet ? support::JSONtemp::stringify(field, "value", value)
                       : support::JSONtemp::stringify(field);
  auto resp = m_sharedState.getESPClient(ip)->sendRequestOpt(request, 4000ms);
  if (!resp || resp->empty()) {
    log.error("No response from {}", ip);
    return false;
  }

  // Unwrap the v0.96 {"status","response":"<json>"} envelope if present,
  // then read the resulting "val" boolean.
  nlohmann::json data;
  try {
    data = nlohmann::json::parse(*resp);
    if (data.contains("response") && data["response"].is_string())
      data = nlohmann::json::parse(data["response"].get<std::string>());
  } catch (const nlohmann::json::parse_error &) {
    log.error("Could not parse response from {}", ip);
    return false;
  }
  if (data.contains("err")) {
    log.error("{}", data["err"].dump());
    return false;
  }

  const bool current = data.value("val", value);

  // Reflect the (read-back) state in the cache when we track this flag.
  if (cache)
    m_sharedState.withClient(ip, [&](Client &c) { c.engine.*cache = current; });

  const auto col = current ? Theme::ok() : Theme::err();
  log.println("{}{}{}  {}{}{}", Theme::lbl(), cmdName, Theme::r(), col,
              current ? "on" : "off", Theme::r());
  return true;
}

void CommandExecutor::okFixture(const Selection &sel, const std::string &label,
                                const std::string &value) {
  log.println("{}✓{} {}{}{} {}→{} {}{}{}  {}{}{} fixture {}{}{}", Theme::ok(),
              Theme::r(), Theme::lbl(), label, Theme::r(), Theme::dim(),
              Theme::r(), Theme::val(), value, Theme::r(), Theme::ip(), sel.ip,
              Theme::r(), Theme::val(), sel.fixtureId, Theme::r());
}

void CommandExecutor::okDevice(const std::string &ip, const std::string &msg) {
  log.println("{}✓{} {}{}{}  {}", Theme::ok(), Theme::r(), Theme::ip(), ip,
              Theme::r(), msg);
}

static std::vector<CommandExecutor::Selection> flatten(SharedState &state) {
  std::vector<CommandExecutor::Selection> out;
  for (const auto &[ip, client] : state.snapshot())
    for (int id = 0; id < static_cast<int>(client.fixtures.size()); ++id)
      out.push_back({ip, id});
  return out;
}

} // namespace core::commands
