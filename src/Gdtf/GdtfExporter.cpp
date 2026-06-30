#include "GdtfExporter.h"

#include "VectorworksMVR.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace Test
{
    using namespace VectorworksMVR;
    using namespace VectorworksMVR::GdtfDefines;

    namespace
    {
        bool ok(VCOMError e) { return e == kVCOMError_NoError; }

        // FNV-1a 32-bit; used to derive a stable per-type fixture GUID so the same
        // fixture `type` always exports with the same FixtureTypeID.
        std::uint32_t fnv1a(const std::string &s, std::uint32_t seed)
        {
            std::uint32_t h = 2166136261u ^ seed;
            for (unsigned char ch : s)
            {
                h ^= ch;
                h *= 16777619u;
            }
            return h;
        }

        // Grouping factor from a preset name like "group by 2" -> 2 (the last run
        // of digits). 0 if the name carries no number.
        int groupFactor(const std::string &name)
        {
            int end = static_cast<int>(name.size()) - 1;
            while (end >= 0 && !std::isdigit(static_cast<unsigned char>(name[end])))
                --end;
            if (end < 0)
                return 0;
            int begin = end;
            while (begin >= 0 && std::isdigit(static_cast<unsigned char>(name[begin])))
                --begin;
            return std::stoi(name.substr(begin + 1, end - begin));
        }

        struct ModeSpec
        {
            std::string name;
            int cells;
        };
    }

    GdtfExportResult exportFixtureGdtf(const Client::Fixture &fix, const std::string &path)
    {
        const int channels = fix.footprint;
        if (channels <= 0 || channels % 3 != 0)
            return {false, "footprint (" + std::to_string(channels) +
                               ") is not a positive multiple of 3 — RGB-only export"};
        const int curCells = channels / 3;

        // One DMX mode per preset. We treat preset names as "group by N" factors
        // and derive each mode's cell count from the current footprint: the current
        // preset's factor tells us the base (group-by-1) pixel count, and every
        // other preset divides that down. (The device's reported footprint doesn't
        // vary per preset, so this is the best available derivation; a preset name
        // without a number falls back to the current cell count.)
        std::vector<ModeSpec> modes;
        if (fix.presets.empty())
        {
            modes.push_back({"RGB " + std::to_string(curCells) + "px", curCells});
        }
        else
        {
            int curN = 0;
            if (fix.presetIndex >= 0 && fix.presetIndex < static_cast<int>(fix.presets.size()))
                curN = groupFactor(fix.presets[fix.presetIndex]);
            if (curN <= 0)
                curN = 1;
            const int basePixels = curCells * curN; // cells at "group by 1"

            for (const auto &pname : fix.presets)
            {
                const int n = groupFactor(pname);
                int cells = curCells;
                if (n > 0)
                    cells = std::max(1, (basePixels + n / 2) / n); // rounded base/N
                modes.push_back({pname, cells});
            }
        }

        IGdtfFixturePtr gdtf(IID_IGdtfFixture);

        // Stable GUID derived from the fixture type.
        MvrUUID uuid(fnv1a(fix.type, 0), fnv1a(fix.type, 1),
                     fnv1a(fix.type, 2), fnv1a(fix.type, 3));

        const std::string manufacturer = "luckecli";
        std::string name = !fix.name.empty() ? fix.name : (!fix.type.empty() ? fix.type : "Fixture");

        if (!ok(gdtf->OpenForWrite(path.c_str(), name.c_str(), manufacturer.c_str(), uuid)))
            return {false, "OpenForWrite failed (cannot create " + path + ")"};

        gdtf->SetShortName(name.substr(0, 8).c_str());
        gdtf->SetFixtureTypeDescription(("Exported by luckecli (type=" + fix.type + ")").c_str());

        // Color feature group + a single RGB feature shared by the three attributes.
        IGdtfFeatureGroupPtr colorFG;
        if (!ok(gdtf->CreateFeatureGroup("Color", "Color", &colorFG)))
            return {false, "CreateFeatureGroup failed"};
        IGdtfFeaturePtr rgbFeature;
        if (!ok(colorFG->CreateFeature("RGB", &rgbFeature)))
            return {false, "CreateFeature failed"};

        // The three additive-color attributes (GDTF standard names).
        const std::array<const char *, 3> attrName{"ColorAdd_R", "ColorAdd_G", "ColorAdd_B"};
        const std::array<const char *, 3> attrPretty{"R", "G", "B"};
        std::array<IGdtfAttributePtr, 3> attr;
        for (int c = 0; c < 3; ++c)
        {
            if (!ok(gdtf->CreateAttribute(attrName[c], attrPretty[c], &attr[c])))
                return {false, std::string("CreateAttribute failed for ") + attrName[c]};
            attr[c]->SetFeature(rgbFeature);
        }

        // One geometry the whole mode hangs off of.
        IGdtfGeometryPtr geo;
        if (!ok(gdtf->CreateGeometry(EGdtfObjectType::eGdtfGeometry, "Beam", nullptr,
                                     STransformMatrix(), &geo)))
            return {false, "CreateGeometry failed"};

        // One DMX mode per preset: `cells` RGB cells laid out R,G,B per cell.
        for (const auto &spec : modes)
        {
            IGdtfDmxModePtr mode;
            if (!ok(gdtf->CreateDmxMode(spec.name.c_str(), &mode)))
                return {false, "CreateDmxMode failed for " + spec.name};
            mode->SetGeometry(geo);

            for (int i = 0; i < spec.cells; ++i)
            {
                for (int c = 0; c < 3; ++c)
                {
                    IGdtfDmxChannelPtr ch;
                    if (!ok(mode->CreateDmxChannel(geo, &ch)))
                        return {false, "CreateDmxChannel failed"};
                    ch->SetGeometry(geo);
                    ch->SetCoarse(i * 3 + c + 1); // 1-based DMX offset

                    IGdtfDmxLogicalChannelPtr lch;
                    if (!ok(ch->CreateLogicalChannel(attr[c], &lch)))
                        return {false, "CreateLogicalChannel failed"};

                    IGdtfDmxChannelFunctionPtr fn;
                    const std::string fnName = std::string(attrName[c]) + " " + std::to_string(i + 1);
                    if (!ok(lch->CreateDmxFunction(fnName.c_str(), &fn)))
                        return {false, "CreateDmxFunction failed"};
                    fn->SetAttribute(attr[c]);
                }
            }
        }

        if (!ok(gdtf->Close()))
            return {false, "Close/write failed"};
        return {true, ""};
    }
}
