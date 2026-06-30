#pragma once
#include <string>

#include "Client.h"

namespace Test
{
    struct GdtfExportResult
    {
        bool ok = false;
        std::string error; // populated when !ok
    };

    // Export a single fixture to a .gdtf file at `path` using libMVRgdtf.
    //
    // RGB-only model (for now): the fixture is treated as footprint/3 RGB cells,
    // each contributing ColorAdd_R / ColorAdd_G / ColorAdd_B channels in order.
    // Fails if the footprint isn't a positive multiple of 3. universe/address are
    // patch state and are intentionally not written (that's MVR, not GDTF).
    GdtfExportResult exportFixtureGdtf(const Client::Fixture &fix, const std::string &path);
}
