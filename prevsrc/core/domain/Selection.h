#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "core/domain/Client.h"

namespace core
{

    // A snapshot of the device registry: (ip, Client) pairs, as returned by
    // SharedState::snapshot(). Selections resolve against one of these.
    using ClientList = std::vector<std::pair<std::string, Client>>;

    // A selected fixture = a device IP plus a fixture id within that device.
    struct Selection
    {
        std::string ip;
        int fixtureId = -1;

        bool operator==(const Selection &) const = default;

        // Stable string key ("<ip>#<id>") for dedup / map lookups.
        std::string key() const;

        // Resolve against a snapshot. Returns nullptr if the device is gone or
        // the fixture id is out of range.
        const Client *client(const ClientList &clients) const;
        const Client::Fixture *fixture(const ClientList &clients) const;
    };

    // The current selection: a set of fixtures commands can target at once.
    // Wraps a vector with membership/toggle/query helpers, while still iterating
    // and converting to a plain vector so existing call sites keep working.
    class SelectionSet
    {
    public:
        SelectionSet() = default;
        SelectionSet(std::vector<Selection> items) : m_items(std::move(items)) {}

        // ── queries ─────────────────────────────────────────────
        bool empty() const { return m_items.empty(); }
        std::size_t size() const { return m_items.size(); }
        const Selection &front() const { return m_items.front(); }
        const std::vector<Selection> &items() const { return m_items; }

        bool contains(const Selection &s) const;

        // If exactly one fixture is selected, a pointer to it; else nullptr.
        const Selection *single() const;

        // Unique device IPs across the selection (a device may host several
        // selected fixtures). For device-level commands (reboot, setwifi).
        std::vector<std::string> ips() const;

        // Resolve every selection against a snapshot, dropping any that no
        // longer exist. Fixture pointers alias into `clients`.
        std::vector<std::pair<Selection, const Client::Fixture *>>
        resolved(const ClientList &clients) const;

        // ── mutation ────────────────────────────────────────────
        void clear() { m_items.clear(); }
        void set(std::vector<Selection> items) { m_items = std::move(items); }
        void add(const Selection &s) { m_items.push_back(s); }

        // Toggle membership; returns true if the fixture is now selected.
        bool toggle(const Selection &s);

        // ── iteration / interop ─────────────────────────────────
        auto begin() const { return m_items.begin(); }
        auto end() const { return m_items.end(); }

        // Implicit view as a plain vector so functions taking
        // `const std::vector<Selection>&` (and copies) keep working.
        operator const std::vector<Selection> &() const { return m_items; }

    private:
        std::vector<Selection> m_items;
    };

}
