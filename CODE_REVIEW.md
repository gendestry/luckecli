# luckecli code review

**Date:** 2026-10-29
**Scope:** everything under `src/` on branch `firmware_upgrade`, including uncommitted changes, plus the parts of `lib/Utils` the app calls (sACN, `String::isInt`).
**Note:** `analysis/*.md` (May 2026) describes the layout from before the restructure (`ESPClient` statics, `CommandList`, the `setwifi` parity bug, …). Most of those bugs are fixed or the code no longer exists. This review replaces it.

Severity: 🔴 crash, data loss, or security · 🟠 wrong behaviour users will hit · 🟡 quality or robustness

---

## 🔴 High

### 1. Use-after-free on shutdown: Heartbeat callbacks run after Heartbeat is destroyed
- `main.cpp:35-49`: objects are destroyed in this order: `exec` → `listener` → `heartbeat` → `display` → `state`. The `ESPClient`s live in `SharedState`, so they are destroyed last.
- `ESPClient.cpp:56-67`: during shutdown the worker calls `onResponse(std::nullopt)` on **every** pending job. The comment says "pending async jobs are simply dropped", but the code does not drop them.
- `Heartbeat.cpp:166-192, 211-222`: the describe and presets callbacks capture `this` (the Heartbeat) and call `logger.error(...)`. When they run during `~SharedState`, the Heartbeat is already gone.
- **When it happens:** you quit while a describe or presets request is queued or retrying, for example a few seconds after startup or with one unresponsive device (3 retries × 4 s per request).
- **Fix:** at shutdown, fulfil only the synchronous promises and drop async callbacks. Or give Heartbeat a liveness token (a `weak_ptr`, the same pattern `LogView` uses). Or stop all ESPClients explicitly before `heartbeat` is destroyed.

### 2. `CommandExecutor` runs on several threads with no synchronisation
- `TerminalView.cpp:170-172` starts a **new detached thread for each command** you type. Two quick Enters run in parallel and can finish in either order.
- `GridView.cpp:63-77` runs commands on its own worker thread.
- `ControlView.cpp:52` and the menubar and keyboard handlers (`selectAll`, `clearSelection`, `selectIndex`) call the executor on the FTXUI thread.
- `MainView.cpp:184-194`: the ticker thread calls `LogView::poll()`, which reads `m_exec->selection()` (`LogView.cpp:26-31`).
- Render code (`StatusBar`, `InfoPanel::sync/render`, `FixtureGrid::rebuild`, `PresetDropdown::sync`) copies `selection()` on the UI thread.
- **What is shared:** `m_selection` (a `std::vector`), `m_history`, `m_logNext`, `m_exit`, and the history file write. Reading a vector while another thread reallocates it is UB and can crash. `m_busy` in GridView only freezes clicks against GridView's own queue.
- Detached threads also capture `this` and `onCommand`. After `exit` they keep running (for up to the 4 s timeout) while `exec` and `display` are being destroyed.
- **Fix:** run every executor call through one command thread or queue (GridView already has one; make it shared). Have the executor publish an immutable selection snapshot behind a mutex for the renderers. Join, don't detach.

### 3. Malformed device JSON throws exceptions nothing catches → `std::terminate`
nlohmann's `.value()` throws `type_error` when the JSON isn't an object or the field has the wrong type. Every parsing site catches only `parse_error`, or nothing. There is no top-level `try` in `workerLoop`, in the heartbeat thread, or in `resolveCommandQuiet`.
- `Heartbeat.cpp:130-143`: if the heartbeat is an array, or has `"version": 1.0` or `"protocol": "2"`, the heartbeat thread dies and takes the process with it. Any host on the LAN can send that packet to UDP 12343.
- `Protocol.cpp:202` `err.value("msg", err.value("code", "error"))` throws if `code` is numeric. Line 207 throws if `request` is not a string. Lines 222 and 225 call `.value()` on `data` without checking that it's an object.
- `legacy/v96.h`, `legacy/v98.h`: any wrong field type in describe or presets throws on the ESPClient worker.
- `LogView.cpp:76` and `CommandExecutor.cpp:838`: `line.get<std::string>()`. `CommandExecutor.cpp:1219` `data.value("val", bool)`. `CommandExecutor.cpp:450` `reply.value("err", string)` when `err` is an object.
- `GdtfExporter.cpp:48`: `std::stoi` on a preset name with a long run of digits throws `out_of_range`.
- **Fix:** wrap each callback invocation in `workerLoop`, the heartbeat loop body, and `resolveCommandQuiet` in `try/catch(const std::exception&)` and log the error. Add a small helper that checks `is_object()` before `.value()`.

### 4. Device errors are reported as success, and the cache is patched anyway
`setuniverse` (`CommandExecutor.cpp:390`), `setname` (485), `setFixtureField`, which covers `setaddress` and `setpreset` (524), and `highlight` (570) treat any reply as success. If the device answers `{"err":"..."}`, they still print ✓ and write the rejected value into the cache. Protocol 2 makes this worse, because `Protocol::fromV2` turns **every** failure into `{"err": msg}`. Only `setleds` and `taskConfig` check for errors.
**Fix:** add one helper that sends, parses, and checks for `err`, returning `std::expected<json, std::string>`, and use it for every request.

### 5. `connect()` has no timeout
`Connection.cpp:110` uses a blocking `connect`. For a device that is powered off, it blocks for the kernel's SYN-retry time (about 2 minutes on Linux) no matter what `timeout` the request asked for. That stalls the device's whole job queue, any synchronous command on the UI or REPL thread, and shutdown, because `~ESPClient` joins the worker.
**Fix:** use a non-blocking connect plus `poll` with the request deadline.

### 6. SIGPIPE can kill the process
`Connection.cpp:136` calls `::send(sock_, …, 0)`, and nothing in the app ignores SIGPIPE. If a device accepts and then resets the connection (the firmware is single-connection and "not ready right after the previous connection closed"), the process dies silently.
**Fix:** pass `MSG_NOSIGNAL`, or call `signal(SIGPIPE, SIG_IGN)` in `main`.

### 7. The Wi-Fi password is stored and shown in plaintext
- `InfoPanel.cpp:343` builds `"setwifi ssid X pass SECRET"` → `GridView` dispatch → `resolveCommand` → `recordHistory`. That writes the line to `~/.luckecli_history`, created through `std::ofstream` with default permissions (usually 0644, readable by every user on the machine). Typed `setwifi` commands end up there too, and the TUI terminal echoes them.
- `Client.cpp:49` prints `password:` in `describe`.
- **Fix:** don't record `setwifi` in history (or redact it), create the history file with mode 0600, and mask the password when displaying it.

### 8. Fixture indices change when a device is discovered
`SharedState::snapshot()` (`SharedState.h:82-86`) copies an `unordered_map` in iteration order. In libstdc++ a newly inserted node usually goes to the **front** of that order, and a rehash reorders everything. `flatten()` (`CommandExecutor.cpp:1245`), `list`, the grid's card `idx`, and the `<id>`/`select` commands all depend on that order.
**Failure:** `list` shows `[2] Kitchen`, a new device sends its first heartbeat, then `select 2` selects a different fixture. In the TUI a click can land in the window before the 1 s rebuild with the same result.
**Fix:** use `std::map`, or sort the snapshot by IP, then by fixture id.

---

## 🟠 Medium

### 9. Only the first heartbeat is parsed (this matters for the firmware-upgrade work)
`Heartbeat.cpp:115-154`: once a device is known, later heartbeats only update the ping time. If its version, protocol, or fixture list changes after an OTA update, reboot, or `setleds`, nothing notices, so `ESPClient` keeps translating with the old `protocol`. Devices that disappear are never removed, and their ESPClient worker threads stay alive. A describe that fails after its retries is never retried.
**Fix:** compare `version`, `protocol`, and fixture count on every heartbeat and re-describe when they change. Expire devices after N seconds without a heartbeat. Retry a failed describe on a later heartbeat.

### 10. `JSONtemp` doesn't escape strings, so commands can inject JSON
The comment at `JSONtemp.h:18` says "STRING HANDLING (SAFE)", but `escapeString` only wraps the string in quotes. Example: `setname a","leds":5,"x":"` sends `{"request":"setfixture","id":0,"name":"a","leds":5,"x":""}`, which is valid JSON. It changes the LED count and reboots the device. `setwifi` already switched to nlohmann for this reason.
**Fix:** build every request with `nlohmann::json` and delete `JSONtemp`.

### 11. The tokenizer splits on every space, which breaks names, SSIDs and passwords
- `setname` only uses `args[1]`, so "Kitchen Strip" becomes "Kitchen". This affects `InfoPanel.cpp:316` and `FixtureEditor.cpp:80`.
- If the SSID field in the TUI is empty, the command becomes `setwifi ssid  pass X`, the spaces get normalised to `setwifi ssid pass X`, and the SSID is set to **"pass"**.
- SSIDs or passwords that contain spaces, or that equal the alias tokens `s`, `p`, `pass` or `ssid`, get parsed wrong.
**Fix:** add quoting to `Tokenizer.h` (it was planned), and have the UI call typed executor methods instead of building command strings.

### 12. Concurrent describes overwrite each other's changes
`refreshDevice` (`CommandExecutor.cpp:1013-1037`) and the Heartbeat describe callback (`Heartbeat.cpp:177-184`) both copy the `Client`, parse a reply into the copy, then write back the **whole** object. Anything that changed in between is lost: `last_ping`, `highlighted`, preset names the heartbeat fetched, or a `setname` patch.
**Fix:** merge only the fields that came from the describe reply, inside `withClient`.

### 13. Some `Client` fields are never initialised
`Client.h:15-16` (`serial_report`, `wireless_report`) and `:24` (`rssi`) have no default value. `InfoPanel::sync` and `describe` read them before the first describe reply arrives, which is UB and in practice shows random toggles.

### 14. `ResponseListener` shutdown doesn't work
- `stop()` (`ResponseListener.cpp:55-62`) detaches the thread while it is blocked in `accept()`. The destructor then sends a **UDP** datagram to the **TCP** port, which does nothing.
- The detached accept thread and its per-connection threads capture `this` and `SharedState`. A response that arrives during teardown therefore touches freed memory.
- If `accept` fails repeatedly (e.g. `EMFILE`), lines 144-151 loop at 100% CPU and log on every pass.
- If `bind` fails (for example, a second instance already holds port 12345), the constructor waits 2 s and continues silently, and the 0.96 response path just doesn't work.
**Fix:** use `shutdown(serverSock)` or a self-pipe with `poll` to wake `accept`, join instead of detach, and surface bind failure to the user.

### 15. "Fixture id" means two different things
Selections, `requestPresets`, and every `setfixture` send the **vector index** as `"id"`. `fetchPresets` (`Commands.h:87`) sends `fix.id` from describe. Fixtures created from the heartbeat all have `id = 0`. This only works while the firmware's ids happen to be 0..n-1. Pick one meaning and use it everywhere.

### 16. CLI terminal handling
- `CLIDisplay.cpp:109-127`: raw mode is restored only on a normal exit. Ctrl-C (ISIG is still on), any exception, or `std::terminate` leaves the shell with echo off. Restore the terminal with an RAII guard plus a SIGINT handler or `atexit`.
- `onLog` writes to `std::cout` from worker threads while the user is typing a line, so the output gets mixed up. It should clear the line, print the log, and redraw the prompt and buffer.
- Backspace removes one byte, which corrupts UTF-8 input.

### 17. A large heartbeat can be truncated
`Heartbeat.cpp:94` uses `char buffer[512]`. A device with many fixtures, or long names, sends a truncated datagram → parse error → it is never discovered. Use a 1500 or 64 KiB buffer and check `MSG_TRUNC`.

### 18. Latency stacks up in the per-device queue
Synchronous user commands wait behind heartbeat-driven describe and presets jobs (up to 4 retries × 4 s each, per fixture). There is no priority and no way to cancel. Combined with #5, one flaky device can freeze the REPL for a long time. Options: a priority lane for interactive requests, and capping or dropping background retries while a user command is waiting.

### 19. Reply handling edge cases
- `Connection::readLine` (`Connection.cpp:63-77`): when the peer closes, any final reply without a trailing newline left in `rx_` is thrown away.
- `doRequest` (`ESPClient.cpp:126-135`): for protocol ≥ 1, once the peer closes there is nothing left to wait for, but the loop keeps polling the 0.96 queue until the full timeout expires.
- Requests carry no correlation id. A late reply from a request that already timed out can arrive after the next request's `clear()` and be matched to the wrong request. Protocol 2 could add an `id` field.

---

## 🟡 Low / code quality

**Uncommitted state**
- `main.cpp:38` hardcodes `DisplayType::CLI`, and `pickDisplay` is unused (it's commented out in the diff), so the TUI can't be reached. Revert this before committing.

**SharedState**
- `isNewClient()` returns `true` for **known** clients (`SharedState.cpp:40-44`). The name is the opposite of what it does.
- `getClient()` returns a reference after releasing the lock. It's unused, so delete it.
- The lock in `getQueue()` does nothing.
- `onChangeCallback` is never called, so the UI depends on the 1 s polling ticker.
- Member initialisation order doesn't match declaration order (`-Wreorder`). `addClient` has an unused `auto &c`.

**Logging (`Log.h`)**
- `s_log`, plus the per-type, per-scope and per-instance lists, grow forever.
- Format strings are checked only at runtime (`std::vformat`). Switching to `std::format_string<Args...>` gets compile-time checking.
- The display callback runs while the global log mutex is held, so a callback that logs would deadlock.
- `getLog()` returns a reference without synchronisation.

**ESPClient and the net code**
- `using namespace std::chrono_literals;` appears at global scope in the headers `ESPClient.h` and `ResponseListener.h`.
- `enable_shared_from_this` is unused.
- Logger scopes are inconsistent: `ESPClient` and `ResponseListener` are both "CONFIG", and `Connection` sets its scope twice.
- `inet_ntoa` should be `inet_ntop`.

**Dead code** (roughly 400 lines that are commented out): `ESPClient.h:93-189`, `ESPClient.cpp:160-218`, `SharedState.cpp:61-220` (with the matching block in the header), `JSONtemp.h:118-176`, and `ResponseListener.cpp:20-97`, which also holds the unused `listen_ingest_udp` declaration.

**sACN / colour**
- Every send creates a new socket and calls `Interfaces::primaryIP()`, for every universe, at up to 50 Hz from the colour sliders. Cache one `SacnSender` per universe.
- The universe frame is sent whole, so `setcolor` on a subset blacks out the other fixtures on that universe. Keeping a persistent per-universe DMX buffer would fix this and would also let `blackout` stop clearing fixtures luckecli doesn't manage.
- Nothing is resent periodically. E1.31 receivers treat a source as lost after 2.5 s of silence, so whether the colour holds depends on the firmware's timeout behaviour.
- In `lib/Utils`, `SacnSender` can be copied even though it owns a raw fd, so a copy would close the socket twice.

**GDTF/MVR**
- The fixed temp directory `/tmp/luckecli-mvr` (`GdtfExporter.cpp:280`) can collide with a concurrent export and is a symlink risk on a shared machine. Use `mkdtemp`.
- If `CreateFixture` fails, the fixture is skipped without telling the user.
- `exportgdtf` derives the GUID from `type` alone, but the modes depend on preset and footprint. Two fixtures of the same type with different footprints get the same FixtureTypeID with different modes.

**UI**
- `MainView` reads `m_view` from the ticker thread, and `FTXUIDisplay::m_quit` is written from other threads. Neither is atomic.
- `StatusBar` calls `snapshot()` (a deep copy of every client, including preset vectors) several times per frame.
- `InfoPanel::applyEdits` keeps the typed values as the new baseline even when the command fails, so the panel shows values the device rejected.
- In `LogView::poll`, the `alive.lock()` result is discarded right away, so it doesn't keep `this` alive for the rest of the callback.

**Validation**
- `setaddress` and `setuniverse` accept any int (negative, above 511 or 63999). Validate before sending.
- `taskConfig` treats any argument that isn't on/true/1 (including typos such as "of") as **off**.

**Time**
- `Client::last_ping` uses `system_clock`, so a wall-clock jump makes devices flap between online and offline. Use `steady_clock`.
- `last_ping_str` shows the interval between the last two pings, not how long ago the last one was.

**History**
- `recordHistory` rewrites the whole file on every command, and not atomically. Append to the file, or write a temp file and rename it.

**Architecture**
- Core code (`Client::toString`, every handler) emits ANSI colours from `support/term/Theme.h`, and the TUI strips them out again (`stripAnsi`). There are also two `Theme.h` files. Have commands return structured results and let each frontend render them.

**Build**
- `cmake_minimum_required(VERSION 3.10)`, but `CMAKE_CXX_STANDARD 23` needs CMake 3.20 or later.
- There are no warning flags. `-Wall -Wextra` would have caught the reorder, uninitialised-field and unused-variable issues above.
- `include/` is empty but is exported as a PUBLIC include directory.
- There are no tests. `Protocol::toV2/fromV2`, the v96/v98 parsers, `Args`, `Tokenizer`, `Selection` and `computeModes` are pure functions and would be cheap to unit test (doctest or Catch2 plus `ctest`). Protocol translation in particular deserves tests while protocol 2 is being added.

---

## Suggested order of work
1. **Stop the crashes:** catch exceptions in the worker, heartbeat and dispatch paths (#3), fix shutdown ordering and the async callbacks (#1, #14), and handle SIGPIPE (#6).
2. **One command thread** for the executor (#2), which also removes the detached-thread use-after-free at exit.
3. **A request helper** that checks for `err` and enforces a connect timeout (#4, #5).
4. **Stable fixture ordering** (#8) and one consistent meaning for fixture id (#15).
5. **Use nlohmann for all requests and add quoting to the tokenizer** (#10, #11).
6. **Re-check heartbeats** for version/protocol changes and expire stale devices (#9). This is needed for the firmware upgrade flow.
7. **Wi-Fi password handling** (#7).
8. **Warnings and a small test target**, then work through the 🟡 list.
