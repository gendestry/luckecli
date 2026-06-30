#include "GdtfExporter.h"

#include "VectorworksMVR.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <map>
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

        // An identity transform. libMVRgdtf's STransformMatrix is a plain aggregate,
        // so a default `STransformMatrix()` zero-initialises EVERY field — including
        // the ux/vy/wz diagonal — yielding a singular (all-zero) matrix. Written to
        // GDTF that collapses every geometry/cell onto one degenerate point, and
        // consoles (MagicQ) then can't build a cell layout, so the fixture imports
        // with NO selectable sub-fixtures (can't fan colour). A real identity is
        // required; `oy` offsets each cell along one axis so the pixels lay out in
        // order (matching tested fixtures like Astera).
        STransformMatrix identityMatrix(double oy = 0.0)
        {
            STransformMatrix m{}; // zero
            m.ux = 1.0;
            m.vy = 1.0;
            m.wz = 1.0;
            m.oy = oy;
            return m;
        }

        // Replace characters not safe in a file name with '_'.
        std::string sanitizeFileName(const std::string &s)
        {
            std::string out;
            for (char c : s)
                out += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') ? c : '_';
            return out.empty() ? "Fixture" : out;
        }
    }

    std::string gdtfCurrentModeName(const Client::Fixture &fix)
    {
        if (fix.presets.empty())
            return "RGB " + std::to_string(fix.footprint / 3) + "px";
        int idx = std::clamp(fix.presetIndex, 0, static_cast<int>(fix.presets.size()) - 1);
        return fix.presets[idx];
    }

    namespace
    {
        // The DMX modes for a fixture: one per preset (named by the preset), with a
        // cell count derived from the "group by N" factor relative to the current
        // footprint (the current preset's factor gives the base group-by-1 pixel
        // count; others divide it down). No presets cached -> a single "RGB Npx".
        std::vector<ModeSpec> computeModes(const Client::Fixture &fix)
        {
            const int curCells = fix.footprint / 3;
            std::vector<ModeSpec> modes;
            if (fix.presets.empty())
            {
                modes.push_back({"RGB " + std::to_string(curCells) + "px", curCells});
                return modes;
            }
            int curN = 0;
            if (fix.presetIndex >= 0 && fix.presetIndex < static_cast<int>(fix.presets.size()))
                curN = groupFactor(fix.presets[fix.presetIndex]);
            if (curN <= 0)
                curN = 1;
            const int basePixels = curCells * curN;
            for (const auto &pname : fix.presets)
            {
                const int n = groupFactor(pname);
                int cells = curCells;
                if (n > 0)
                    cells = std::max(1, (basePixels + n / 2) / n); // rounded base/N
                modes.push_back({pname, cells});
            }
            return modes;
        }

        // Write a GDTF file containing the given RGB modes. `gdtfName` is the
        // FixtureType Name shown by consoles; `type` seeds the description;
        // `guidSeed` seeds the FixtureTypeID (stable per whatever identity the
        // caller wants — the type for a standalone export, the instance for MVR).
        GdtfExportResult buildGdtfFile(const std::string &gdtfName, const std::string &type,
                                       const std::string &guidSeed,
                                       const std::vector<ModeSpec> &modes, const std::string &path)
        {
            IGdtfFixturePtr gdtf(IID_IGdtfFixture);

            MvrUUID uuid(fnv1a(guidSeed, 0), fnv1a(guidSeed, 1), fnv1a(guidSeed, 2), fnv1a(guidSeed, 3));
            const std::string manufacturer = "luckecli";
            const std::string name = !gdtfName.empty() ? gdtfName : (!type.empty() ? type : "Fixture");

            if (!ok(gdtf->OpenForWrite(path.c_str(), name.c_str(), manufacturer.c_str(), uuid)))
                return {false, "OpenForWrite failed (cannot create " + path + ")"};

            gdtf->SetShortName(name.substr(0, 8).c_str());
            gdtf->SetFixtureTypeDescription(("Exported by luckecli (type=" + type + ")").c_str());

            // Color feature group + a single RGB feature shared by the attributes.
            IGdtfFeatureGroupPtr colorFG;
            if (!ok(gdtf->CreateFeatureGroup("Color", "Color", &colorFG)))
                return {false, "CreateFeatureGroup failed"};
            IGdtfFeaturePtr rgbFeature;
            if (!ok(colorFG->CreateFeature("RGB", &rgbFeature)))
                return {false, "CreateFeature failed"};

            // Attribute Name stays the GDTF standard (so consoles recognise the
            // colour); Pretty is the friendly label shown in the patch.
            const std::array<const char *, 3> attrName{"ColorAdd_R", "ColorAdd_G", "ColorAdd_B"};
            const std::array<const char *, 3> attrPretty{"Red", "Green", "Blue"};
            std::array<IGdtfAttributePtr, 3> attr;
            for (int c = 0; c < 3; ++c)
            {
                if (!ok(gdtf->CreateAttribute(attrName[c], attrPretty[c], &attr[c])))
                    return {false, std::string("CreateAttribute failed for ") + attrName[c]};
                attr[c]->SetFeature(rgbFeature);
            }

            // Physical emitters (CIE 1931 xyY, sRGB primaries) so consoles know the
            // real colours, render them, and can build colour palettes. Linking the
            // colour functions to these makes MagicQ treat the fixture as additive
            // colour (Colour Mix) — which is what drives its intensity/colour.
            const std::array<CieColor, 3> emitColor{{{0.6400, 0.3300, 21.26},
                                                     {0.3000, 0.6000, 71.52},
                                                     {0.1500, 0.0600, 7.22}}};
            const std::array<const char *, 3> emitName{"Red", "Green", "Blue"};
            std::array<IGdtfPhysicalEmitterPtr, 3> emitter;
            for (int c = 0; c < 3; ++c)
                gdtf->CreateEmitter(emitName[c], emitColor[c], &emitter[c]);

            // No dimmer/intensity channel: the device is RGB-only, and MagicQ
            // only auto-patches its working per-RGB-element Virtual Dimmer to a
            // head that has NO intensity channel (Head Editor > Virtual Dim = yes).
            // A GDTF (virtual) dimmer can't be driven by MagicQ and suppresses that
            // auto-VDim, so we deliberately leave it out.

            // Pixel-matrix layout (the structure tested fixtures use — e.g. Astera):
            // per mode, a "Base" geometry holds one GeometryReference per cell, each
            // with a DMX Break giving that cell's start address, all referencing a
            // shared "Cell" Beam. The mode's channels are defined ONCE on that Cell
            // Beam (relative offsets) and the references replicate them per pixel —
            // this is what makes consoles expose real per-pixel control.
            for (const auto &spec : modes)
            {
                IGdtfGeometryPtr cell;
                const std::string cellName = "Cell " + spec.name;
                if (!ok(gdtf->CreateGeometry(EGdtfObjectType::eGdtfGeometryLamp, cellName.c_str(),
                                             nullptr, identityMatrix(), &cell)))
                    return {false, "CreateGeometry failed for " + cellName};

                IGdtfGeometryPtr base;
                const std::string baseName = "Base " + spec.name;
                if (!ok(gdtf->CreateGeometry(EGdtfObjectType::eGdtfGeometry, baseName.c_str(),
                                             nullptr, identityMatrix(), &base)))
                    return {false, "CreateGeometry failed for " + baseName};

                for (int i = 0; i < spec.cells; ++i)
                {
                    IGdtfGeometryPtr ref;
                    const std::string refName = spec.name + " Pixel " + std::to_string(i + 1);
                    // Step each cell along one axis (0.05 m apart) so the pixels lay
                    // out in order — consoles use these positions to build the cell
                    // matrix you fan colour across.
                    if (!ok(base->CreateGeometry(EGdtfObjectType::eGdtfGeometryReference, refName.c_str(),
                                                 nullptr, identityMatrix(i * 0.05), &ref)))
                        return {false, "CreateGeometry(reference) failed for " + refName};
                    ref->SetGeometryReference(cell);
                    IGdtfBreakPtr brk;
                    ref->CreateBreak(1, i * 3 + 1, &brk); // break 1, 1-based start address
                }

                IGdtfDmxModePtr mode;
                if (!ok(gdtf->CreateDmxMode(spec.name.c_str(), &mode)))
                    return {false, "CreateDmxMode failed for " + spec.name};
                mode->SetGeometry(base);

                // R/G/B at offsets 1,2,3 RELATIVE to the cell (the references shift
                // them per pixel via their breaks). Defined once, not per pixel.
                for (int c = 0; c < 3; ++c)
                {
                    IGdtfDmxChannelPtr ch;
                    if (!ok(mode->CreateDmxChannel(cell, &ch)))
                        return {false, "CreateDmxChannel failed"};
                    ch->SetGeometry(cell);
                    ch->SetCoarse(c + 1); // relative offset within the cell

                    IGdtfDmxLogicalChannelPtr lch;
                    if (!ok(ch->CreateLogicalChannel(attr[c], &lch)))
                        return {false, "CreateLogicalChannel failed"};

                    IGdtfDmxChannelFunctionPtr fn;
                    if (!ok(lch->CreateDmxFunction(attrPretty[c], &fn)))
                        return {false, "CreateDmxFunction failed"};
                    fn->SetAttribute(attr[c]);
                    fn->SetDefaultValue(0); // start dark (not full-on)
                    if (emitter[c])
                        fn->SetEmitter(emitter[c]); // real colour
                }
            }

            if (!ok(gdtf->Close()))
                return {false, "Close/write failed"};
            return {true, ""};
        }
    }

    GdtfExportResult exportFixtureGdtf(const Client::Fixture &fix, const std::string &path)
    {
        if (fix.footprint <= 0 || fix.footprint % 3 != 0)
            return {false, "footprint (" + std::to_string(fix.footprint) +
                               ") is not a positive multiple of 3 — RGB-only export"};
        const std::string name = !fix.name.empty() ? fix.name : (!fix.type.empty() ? fix.type : "Fixture");
        return buildGdtfFile(name, fix.type, fix.type, computeModes(fix), path);
    }

    GdtfExportResult exportFixturesMvr(const std::vector<Client::Fixture> &fixtures,
                                       const std::string &path)
    {
        namespace fs = std::filesystem;
        if (fixtures.empty())
            return {false, "no fixtures to export"};

        // Drop fixtures we can't represent as RGB (footprint unknown/0 because they
        // haven't been described yet, or not a multiple of 3) — keeping them would
        // emit empty/odd modes that importers reject.
        std::vector<Client::Fixture> valid;
        for (const auto &f : fixtures)
            if (f.footprint > 0 && f.footprint % 3 == 0)
                valid.push_back(f);
        if (valid.empty())
            return {false, "no fixtures with a usable RGB footprint (describe them first?)"};

        std::error_code ec;
        const fs::path tmp = fs::temp_directory_path() / "luckecli-mvr";
        fs::create_directories(tmp, ec);

        // Stage one GDTF per fixture, named after the instance — consoles (MagicQ)
        // show the GDTF's FixtureType Name as the patch name, so this is what makes
        // each head carry its own fixture name. Each per-fixture GDTF holds exactly
        // that fixture's modes, so its referenced GDTFMode always resolves.
        std::vector<std::string> gdtfFileFor(valid.size()); // parallel to `valid`
        std::vector<fs::path> staged;
        std::map<std::string, int> usedNames; // base name -> count, for de-duping files
        for (std::size_t i = 0; i < valid.size(); ++i)
        {
            const auto &f = valid[i];
            const std::string gname = !f.name.empty() ? f.name : (!f.type.empty() ? f.type : "Fixture");
            std::string base = sanitizeFileName(gname);
            if (int &n = usedNames[base]; ++n > 1)
                base += "_" + std::to_string(n); // disambiguate duplicate names
            const std::string gfile = base + ".gdtf";
            const fs::path gpath = tmp / gfile;

            // GUID seed unique per fixture so distinct heads don't share a type id.
            const std::string seed = gname + "|" + f.type + "|" + std::to_string(i);
            auto r = buildGdtfFile(gname, f.type, seed, computeModes(f), gpath.string());
            if (!r.ok)
            {
                for (const auto &p : staged)
                    fs::remove(p, ec);
                return {false, "GDTF for '" + gname + "': " + r.error};
            }
            gdtfFileFor[i] = gfile;
            staged.push_back(gpath);
        }

        auto cleanup = [&]
        {
            for (const auto &p : staged)
                fs::remove(p, ec);
            fs::remove(tmp, ec);
        };

        IMediaRessourceVectorInterfacePtr mvr(IID_MediaRessourceVectorInterface);
        if (!ok(mvr->OpenForWrite(path.c_str())))
        {
            cleanup();
            return {false, "OpenForWrite failed (cannot create " + path + ")"};
        }
        mvr->AddProviderAndProviderVersion("luckecli", "1.0");

        // Point the lib at the staged GDTFs so CreateFixture can resolve each
        // fixture's type internally, then embed them under their base names.
        mvr->AddGdtfFolderLocation(tmp.string().c_str());
        for (const auto &p : staged)
            mvr->AddFileToMvrFile(p.string().c_str());

        ISceneObjPtr layer;
        if (!ok(mvr->CreateLayerObject(MvrUUID(fnv1a("layer", 0), fnv1a("layer", 1),
                                               fnv1a("layer", 2), fnv1a("layer", 3)),
                                       "luckecli", &layer)))
        {
            cleanup();
            return {false, "CreateLayerObject failed"};
        }

        for (std::size_t i = 0; i < valid.size(); ++i)
        {
            const auto &f = valid[i];
            const std::string fname = !f.name.empty() ? f.name : (!f.type.empty() ? f.type : "Fixture");
            // Per-instance UUID: stable from type/name/address plus the index so
            // two identically-configured fixtures still get distinct GUIDs.
            const std::string key = f.type + "|" + f.name + "|" + std::to_string(f.universe) +
                                    "|" + std::to_string(f.address) + "|" + std::to_string(i);
            MvrUUID fuuid(fnv1a(key, 0), fnv1a(key, 1), fnv1a(key, 2), fnv1a(key, 3));

            ISceneObjPtr fx;
            if (!ok(mvr->CreateFixture(fuuid, identityMatrix(), fname.c_str(), layer, &fx)))
                continue;

            fx->SetGdtfName(gdtfFileFor[i].c_str());
            fx->SetGdtfMode(gdtfCurrentModeName(f).c_str());
            // Absolute DMX address. MVR/consoles use 1-based addressing: universe N
            // occupies [(N-1)*512+1 .. N*512]. The device reports a 0-based channel
            // (addr 0 == first channel), so add 1.
            const int uni = f.universe > 0 ? f.universe : 1;
            const std::size_t absAddr = static_cast<std::size_t>(uni - 1) * 512 + f.address + 1;
            fx->AddAdress(absAddr, 0); // single DMX break (id 0)
            fx->SetFixtureIdNumeric(i + 1);
        }

        const bool wrote = ok(mvr->Close());
        cleanup();
        if (!wrote)
            return {false, "Close/write failed"};
        return {true, ""};
    }
}
