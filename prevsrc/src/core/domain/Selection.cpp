#include "core/domain/Selection.h"

#include <algorithm>
#include <format>

namespace core
{

    std::string Selection::key() const
    {
        return std::format("{}#{}", ip, fixtureId);
    }

    const Client *Selection::client(const ClientList &clients) const
    {
        for (const auto &[cip, client] : clients)
            if (cip == ip)
                return &client;
        return nullptr;
    }

    const Client::Fixture *Selection::fixture(const ClientList &clients) const
    {
        const Client *c = client(clients);
        if (!c || fixtureId < 0 ||
            fixtureId >= static_cast<int>(c->fixtures.size()))
            return nullptr;
        return &c->fixtures[fixtureId];
    }

    bool SelectionSet::contains(const Selection &s) const
    {
        return std::find(m_items.begin(), m_items.end(), s) != m_items.end();
    }

    const Selection *SelectionSet::single() const
    {
        return m_items.size() == 1 ? &m_items.front() : nullptr;
    }

    std::vector<std::string> SelectionSet::ips() const
    {
        std::vector<std::string> out;
        for (const auto &sel : m_items)
            if (std::find(out.begin(), out.end(), sel.ip) == out.end())
                out.push_back(sel.ip);
        return out;
    }

    std::vector<std::pair<Selection, const Client::Fixture *>>
    SelectionSet::resolved(const ClientList &clients) const
    {
        std::vector<std::pair<Selection, const Client::Fixture *>> out;
        for (const auto &sel : m_items)
            if (const Client::Fixture *fx = sel.fixture(clients))
                out.emplace_back(sel, fx);
        return out;
    }

    bool SelectionSet::toggle(const Selection &s)
    {
        auto it = std::find(m_items.begin(), m_items.end(), s);
        if (it != m_items.end())
        {
            m_items.erase(it);
            return false;
        }
        m_items.push_back(s);
        return true;
    }

}
