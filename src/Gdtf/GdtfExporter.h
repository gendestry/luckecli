#pragma once
#include <string>
#include <vector>

#include "Client.h"

namespace Test
{
    struct GdtfExportResult
    {
        bool ok = false;
        std::string error; // populated when !ok
    };

    // The DMX mode name a fixture is currently on (its current preset's name, or
    // the single "RGB Npx" mode when no presets are cached). Matches the mode
    // names emitted by exportFixtureGdtf, so an MVR can reference them.
    std::string gdtfCurrentModeName(const Client::Fixture &fix);

    // Export a single fixture to a .gdtf file at `path` using libMVRgdtf.
    //
    // RGB-only model (for now): the fixture is treated as footprint/3 RGB cells,
    // each contributing ColorAdd_R / ColorAdd_G / ColorAdd_B channels in order.
    // Fails if the footprint isn't a positive multiple of 3. universe/address are
    // patch state and are intentionally not written (that's MVR, not GDTF).
    GdtfExportResult exportFixtureGdtf(const Client::Fixture &fix, const std::string &path);

    // Export every given fixture to a .mvr file at `path`. Embeds one GDTF per
    // unique fixture type and lists each fixture instance with its DMX address
    // (absolute = universe*512 + address) and its current-preset DMX mode. Uses
    // the same RGB-only model as exportFixtureGdtf.
    GdtfExportResult exportFixturesMvr(const std::vector<Client::Fixture> &fixtures,
                                       const std::string &path);
}
