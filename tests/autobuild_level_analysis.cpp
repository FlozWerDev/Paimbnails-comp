#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "../src/features/autobuild/services/LevelAnalysis.hpp"
#include "../src/features/autobuild/services/LevelParse.hpp"
#include "../src/features/autobuild/services/ObjectTaxonomy.hpp"
#include "../src/features/autobuild/services/TemplateEdit.hpp"

using namespace paimon::autobuild;

namespace {

std::string number(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

std::string object(int id, float x, float y, std::string const& extra = {}) {
    std::string out = "1," + std::to_string(id) + ",2," + number(x) + ",3," + number(y);
    if (!extra.empty()) out += "," + extra;
    return out + ";";
}

// A level made of parts whose right answer is known: a ground the player rides,
// a backdrop behind it, an overlay in front, a repeated pillar and some logic.
struct Fixture {
    std::string text;
    LevelData data;
    LevelReport report;
};

std::string groundStrip() {
    std::string out;
    for (int column = 0; column < 62; ++column) {
        float const x = 15.f + column * 30.f;
        out += object(1, x, 15.f, "21,1004,25,0");
        out += object(1, x, 45.f, "21,1004,25,0");
    }
    // Spikes riding the floor, the way a real level places them.
    for (float x : {255.f, 285.f, 705.f}) out += object(8, x, 75.f, "21,1004");
    out += object(13, 615.f, 105.f);   // ship portal
    out += object(35, 915.f, 75.f);    // yellow pad
    return out;
}

// Five big rotated shapes on B3, painted by the background channel.
std::string backdropCluster() {
    std::string out;
    float const xs[] = {203.f, 243.f, 283.f, 323.f, 363.f};
    float const ys[] = {512.f, 517.f, 524.f, 520.f, 513.f};
    for (int i = 0; i < 5; ++i) {
        out += object(1338, xs[i], ys[i], "21,1000,24,-1,32,4,6,23");
    }
    return out;
}

// The hard case: a backdrop built out of ordinary blocks, on the grid, with a
// user colour. Only the Z layer says it is scenery.
std::string blockBackdrop() {
    std::string out;
    for (int column = 0; column < 8; ++column) {
        for (int row = 0; row < 3; ++row) {
            out += object(1, 1515.f + column * 30.f, 345.f + row * 30.f, "21,7,24,1");
        }
    }
    return out;
}

std::string foregroundOverlay() {
    std::string out;
    for (int column = 0; column < 6; ++column) {
        out += object(1338, 2115.f + column * 30.f, 255.f, "21,9,24,7,32,2");
    }
    return out;
}

// Four copies of the same pillar, standing on the ground so they merge with it.
std::string pillars() {
    std::string out;
    for (float base : {1215.f, 1395.f, 1575.f, 1755.f}) {
        out += object(1, base, 75.f, "21,1004");
        out += object(1, base, 105.f, "21,1004");
        out += object(1, base, 135.f, "21,1004");
        out += object(8, base, 165.f, "21,1004");
    }
    return out;
}

std::string triggers() {
    std::string out;
    out += object(899, 105.f, 645.f, "51,3,10,0.5");
    out += object(901, 135.f, 645.f, "51,4,10,1");
    out += object(1268, 165.f, 645.f, "51,5,62,1");
    return out;
}

std::string const kColors =
    "1_20_2_20_3_60_6_1000_7_1_5_0|1_255_2_255_3_255_6_1004_7_1_5_0|"
    "1_180_2_90_3_40_6_7_7_1_5_0|1_90_2_200_3_255_6_9_7_1_5_1";

Fixture buildFixture() {
    Fixture fixture;
    fixture.text = "kA13,0,kA15,0,kA16,0,kS38," + kColors + ",kA11,0;";
    fixture.text += groundStrip();
    fixture.text += backdropCluster();
    fixture.text += blockBackdrop();
    fixture.text += foregroundOverlay();
    fixture.text += pillars();
    fixture.text += triggers();
    fixture.data = parseLevelString(fixture.text);
    fixture.report = analyzeLevel(fixture.data);
    return fixture;
}

// Two ASCII pictures next to each other, so a wrong reconstruction is obvious
// instead of being a boolean that flipped.
void sideBySide(char const* leftTitle, std::string const& left,
                char const* rightTitle, std::string const& right, int width) {
    auto split = [](std::string const& text) {
        std::vector<std::string> lines;
        size_t start = 0;
        while (start <= text.size()) {
            auto end = text.find('\n', start);
            if (end == std::string::npos) break;
            lines.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        return lines;
    };
    auto const a = split(left);
    auto const b = split(right);

    std::string header(leftTitle);
    header.resize(width + 3, ' ');
    header += rightTitle;
    std::cout << header << '\n';
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        std::string line = i < a.size() ? a[i] : std::string();
        line.resize(width + 3, ' ');
        line += i < b.size() ? b[i] : std::string();
        std::cout << '|' << line << "|\n";
    }
}

Region const* regionAt(LevelReport const& report, float x, float y, bool motif = false) {
    for (auto const& region : report.regions) {
        if (region.motif != motif) continue;
        if (x < region.metrics.minX - 1.f || x > region.metrics.maxX + 1.f) continue;
        if (y < region.metrics.minY - 1.f || y > region.metrics.maxY + 1.f) continue;
        return &region;
    }
    return nullptr;
}

void showRegion(char const* label, LevelData const& data, Region const* region) {
    if (!region) {
        std::cout << label << ": (no encontrada)\n";
        return;
    }
    std::cout << label << ": " << regionKindName(region->kind) << " - "
              << region->metrics.objects << " objetos - estructura "
              << region->metrics.structureScore << " / fondo "
              << region->metrics.backgroundScore << '\n'
              << sketchRegion(data, *region, 40, 8);
}

bool parsesTheLevelString() {
    auto fixture = buildFixture();
    auto const& data = fixture.data;

    bool pass = data.objects.size() == 124u + 3 + 2 + 5 + 24 + 6 + 16 + 3;
    pass = pass && data.colors == kColors;
    pass = pass && !data.truncated;

    auto channels = parseColorChannels(data.colors);
    pass = pass && channels.size() == 4;
    bool sawBackground = false;
    for (auto const& channel : channels) {
        if (channel.id != kChannelBG) continue;
        sawBackground = channel.r == 20 && channel.g == 20 && channel.b == 60;
    }
    pass = pass && sawBackground;

    // The backdrop shapes carry scale 4 and 23 degrees of rotation.
    bool sawScaled = false;
    for (auto const& object : data.objects) {
        if (object.id != 1338 || object.zLayer != kZLayerB3) continue;
        sawScaled = std::abs(object.scaleX - 4.f) < 0.001f &&
                    std::abs(object.rotation - 23.f) < 0.001f;
        break;
    }
    pass = pass && sawScaled;

    std::cout << "parse: objetos=" << data.objects.size()
              << " canales=" << channels.size() << '\n';
    return pass;
}

bool separatesGroundFromBackdrop() {
    auto fixture = buildFixture();
    auto const& report = fixture.report;

    auto const* ground = regionAt(report, 315.f, 15.f);
    auto const* backdrop = regionAt(report, 283.f, 524.f);
    auto const* blocks = regionAt(report, 1590.f, 375.f);
    auto const* overlay = regionAt(report, 2175.f, 255.f);
    auto const* logic = regionAt(report, 135.f, 645.f);

    showRegion("suelo", fixture.data, ground);
    showRegion("fondo", fixture.data, backdrop);
    showRegion("fondo de bloques", fixture.data, blocks);

    bool pass = ground && ground->kind == RegionKind::Structure;
    pass = pass && backdrop && backdrop->kind == RegionKind::Background;
    // Same object id and the same grid as the ground; only the Z layer differs.
    pass = pass && blocks && blocks->kind == RegionKind::Background;
    pass = pass && overlay && overlay->kind == RegionKind::Foreground;
    pass = pass && logic && logic->kind == RegionKind::Logic;
    // The backdrop must not have been swallowed by the ground it hangs over.
    pass = pass && ground != backdrop && ground != blocks;

    std::cout << "separacion: estructuras=" << report.counts[0]
              << " fondos=" << report.counts[3]
              << " primer plano=" << report.counts[4]
              << " triggers=" << report.counts[5] << '\n';
    return pass;
}

bool findsTheRepeatedPillar() {
    auto fixture = buildFixture();
    auto const& report = fixture.report;

    Region const* pillar = nullptr;
    for (auto const& region : report.regions) {
        if (!region.motif) continue;
        if (region.metrics.hazards == 0) continue;
        if (region.repeats < 4) continue;
        pillar = &region;
        break;
    }
    if (!pillar) {
        std::cout << "motivo: no se encontro el pilar\n";
        return false;
    }

    std::cout << "motivo: " << regionKindName(pillar->kind) << " x" << pillar->repeats
              << " - " << pillar->metrics.objects << " objetos\n"
              << sketchRegion(fixture.data, *pillar, 12, 8);
    return pillar->repeats == 4;
}

bool templateLooksLikeTheOriginal() {
    auto fixture = buildFixture();
    auto suggestions = suggestTemplates(fixture.data, fixture.report);
    if (suggestions.empty()) {
        std::cout << "plantilla: sin sugerencias\n";
        return false;
    }

    bool pass = true;
    int checked = 0;
    for (auto const& suggestion : suggestions) {
        auto const& region = fixture.report.regions[suggestion.regions.front()];
        auto tpl = templateFrom(fixture.data, fixture.report, suggestion, 30.f);
        if (!tpl.valid()) continue;

        auto const original = sketchRegion(fixture.data, region, 24, 10);
        auto const rebuilt = sketchTemplate(tpl, 24, 10);
        if (original != rebuilt) {
            std::cout << "plantilla '" << suggestion.name << "' no coincide\n";
            sideBySide("nivel", original, "plantilla", rebuilt, 24);
            pass = false;
        }
        ++checked;
        if (checked >= 8) break;
    }

    // Show one of each so the classification is readable, not just asserted.
    for (auto const wanted : {RegionKind::Structure, RegionKind::Background}) {
        for (auto const& suggestion : suggestions) {
            if (suggestion.kind != wanted) continue;
            auto const& region = fixture.report.regions[suggestion.regions.front()];
            auto tpl = templateFrom(fixture.data, fixture.report, suggestion, 30.f);
            std::cout << "plantilla '" << suggestion.name << "' modo "
                      << (tpl.mode == Mode::Wave ? "onda" : "sellos") << " - "
                      << tpl.pieces.size() << " piezas de vocabulario\n";
            sideBySide("nivel", sketchRegion(fixture.data, region, 24, 8),
                       "plantilla", sketchTemplate(tpl, 24, 8), 24);
            break;
        }
    }
    return pass && checked > 0;
}

// What the level calls scenery must not come back as a gameplay template, even
// when it is built from the very same blocks as the floor.
bool suggestionsKeepTheirKind() {
    auto fixture = buildFixture();
    auto suggestions = suggestTemplates(fixture.data, fixture.report);

    bool sawStructure = false;
    bool sawBackground = false;
    bool pass = true;
    for (auto const& suggestion : suggestions) {
        auto const& region = fixture.report.regions[suggestion.regions.front()];
        sawStructure = sawStructure || suggestion.kind == RegionKind::Structure;
        sawBackground = sawBackground || suggestion.kind == RegionKind::Background;
        pass = pass && suggestion.kind == region.kind;
        // The block backdrop lives at x 1515..1725; nothing there is gameplay.
        if (region.metrics.minX >= 1500.f && region.metrics.maxX <= 1740.f &&
            region.metrics.minY >= 330.f) {
            pass = pass && suggestion.kind == RegionKind::Background;
        }
    }

    std::cout << "sugerencias: " << suggestions.size() << " - estructura="
              << (sawStructure ? "si" : "no") << " fondo="
              << (sawBackground ? "si" : "no") << '\n';
    return pass && sawStructure && sawBackground;
}

bool triggersNeverReachATemplate() {
    auto fixture = buildFixture();

    TemplateSuggestion suggestion;
    suggestion.name = "logica";
    suggestion.mode = Mode::Stamp;
    for (size_t i = 0; i < fixture.report.regions.size(); ++i) {
        if (fixture.report.regions[i].kind != RegionKind::Logic) continue;
        suggestion.regions.push_back(static_cast<int>(i));
        break;
    }
    if (suggestion.regions.empty()) {
        std::cout << "triggers: no hay region de logica\n";
        return false;
    }

    auto tpl = templateFrom(fixture.data, fixture.report, suggestion, 30.f);
    bool const pass = tpl.objectCount() == 0;

    // And they are never offered either.
    auto suggestions = suggestTemplates(fixture.data, fixture.report);
    bool offered = false;
    for (auto const& entry : suggestions) offered = offered || entry.kind == RegionKind::Logic;

    std::cout << "triggers: objetos en plantilla=" << tpl.objectCount()
              << " sugeridos=" << (offered ? "si" : "no") << '\n';
    return pass && !offered;
}

bool paletteKnowsWhatEachChannelPaints() {
    auto fixture = buildFixture();
    auto const& palette = fixture.report.palette;

    PaletteEntry const* background = nullptr;
    PaletteEntry const* object = nullptr;
    PaletteEntry const* scenery = nullptr;
    for (auto const& entry : palette) {
        if (entry.channel.id == kChannelBG) background = &entry;
        if (entry.channel.id == kChannelObj) object = &entry;
        if (entry.channel.id == 7) scenery = &entry;
    }

    bool pass = background && background->role == RegionKind::Background;
    pass = pass && object && object->role == RegionKind::Structure;
    pass = pass && scenery && scenery->role == RegionKind::Background;
    pass = pass && !palette.empty() && palette.front().channel.id == kChannelObj;

    std::cout << "paleta: " << palette.size() << " canales";
    for (auto const& entry : palette) {
        std::cout << " [" << entry.channel.id << ' ' << regionKindName(entry.role)
                  << " x" << entry.uses() << ']';
    }
    std::cout << '\n';
    return pass;
}

bool readsTheGameplayBeats() {
    auto fixture = buildFixture();
    auto const& beats = fixture.report.beats;

    bool pass = beats.size() == 2;
    pass = pass && beats[0].kind == ObjectKind::Portal && beats[0].objectId == 13;
    pass = pass && beats[1].kind == ObjectKind::Pad && beats[1].objectId == 35;
    pass = pass && fixture.report.lengthX >= 2265.f;
    pass = pass && std::abs(fixture.report.groundY - 15.f) < 0.01f;

    std::cout << "gameplay: " << beats.size() << " hitos - largo "
              << fixture.report.lengthX << " - suelo " << fixture.report.groundY << '\n';
    return pass;
}

bool overridesRetagObjects() {
    clearTaxonomyOverrides();
    bool pass = kindOf(1338) == ObjectKind::Unknown;

    int const loaded = loadTaxonomyOverrides("# comentario\n1338 hazard\n9999 nope\n8 solid\n");
    pass = pass && loaded == 2;
    pass = pass && kindOf(1338) == ObjectKind::Hazard;
    pass = pass && kindOf(8) == ObjectKind::Solid;

    clearTaxonomyOverrides();
    pass = pass && kindOf(8) == ObjectKind::Hazard;

    std::cout << "taxonomia: " << loaded << " reglas cargadas\n";
    return pass;
}

bool triggerKeysBeatTheIdTable() {
    LevelObject unknown;
    unknown.id = 4242;
    unknown.save = "1,4242,2,15,3,15,51,7,10,0.5";
    LevelObject orb;
    orb.id = 36;
    orb.save = "1,36,2,15,3,15,51,7,87,1";
    LevelObject deco;
    deco.id = 1338;
    deco.save = "1,1338,2,15,3,15,32,2";

    bool const pass = looksLikeTrigger(unknown) && !looksLikeTrigger(orb) &&
                      !looksLikeTrigger(deco);
    std::cout << "triggers-heuristica: " << (pass ? "ok" : "falla") << '\n';
    return pass;
}

bool analysisStaysCheapOnABigLevel() {
    std::string text = "kA13,0,kS38," + kColors + ";";
    for (int column = 0; column < 400; ++column) {
        for (int row = 0; row < 25; ++row) {
            text += object(1, 15.f + column * 30.f, 15.f + row * 30.f, "21,1004");
        }
    }
    auto data = parseLevelString(text);
    auto report = analyzeLevel(data);

    bool const pass = data.objects.size() == 10000 && !report.regions.empty() &&
                      report.counts[static_cast<int>(RegionKind::Structure)] >= 1;
    std::cout << "nivel-grande: " << data.objects.size() << " objetos -> "
              << report.regions.size() << " regiones\n";
    return pass;
}


Template editableTemplate() {
    Template tpl;
    tpl.mode = Mode::Wave;
    tpl.cell = 30.f;
    tpl.pieces.resize(3);
    for (int piece = 0; piece < 3; ++piece) {
        CapturedObject block;
        block.objectId = piece == 1 ? 8 : 1;
        block.save = "1," + std::to_string(block.objectId) + ",2,15,3,15,21,4,22,5";
        tpl.pieces[piece].objects.push_back(block);
        if (piece == 2) {
            CapturedObject trigger;
            trigger.objectId = 899;
            trigger.dx = 30.f;
            trigger.save = "1,899,2,45,3,15,51,7,10,0.5";
            tpl.pieces[piece].objects.push_back(trigger);
        }
    }
    SampleGrid grid;
    grid.width = 3;
    grid.height = 1;
    grid.cells = {{0, 0, 0}, {1, 0, 1}, {2, 0, 2}};
    tpl.grids.push_back(std::move(grid));
    edit::rebuildLinks(tpl);
    return tpl;
}

bool removingAPieceRenumbersEverything() {
    auto tpl = editableTemplate();
    bool pass = tpl.links.size() == 3;
    // Piece 1 sits between 0 and 2, so both rules name it.
    pass = pass && tpl.links[0].side[kRightDirection] == std::vector<int>{1};
    pass = pass && tpl.links[2].side[kLeftDirection] == std::vector<int>{1};

    pass = pass && edit::removePiece(tpl, 1);
    pass = pass && tpl.pieces.size() == 2 && tpl.links.size() == 2;
    // What was piece 2 is piece 1 now, and nothing still points at the hole.
    for (auto const& link : tpl.links) {
        for (auto const& side : link.side) {
            for (int neighbour : side) pass = pass && neighbour < 2;
        }
    }
    pass = pass && tpl.grids.size() == 1 && tpl.grids[0].cells.size() == 2;
    for (auto const& cell : tpl.grids[0].cells) pass = pass && cell.piece < 2;

    std::cout << "editor-borrar: piezas=" << tpl.pieces.size()
              << " celdas=" << (tpl.grids.empty() ? 0 : tpl.grids[0].cells.size()) << '\n';
    return pass;
}

bool filtersDropObjectsAndEmptyPieces() {
    auto tpl = editableTemplate();
    auto const kinds = edit::countKinds(tpl);
    bool pass = !kinds.empty();

    int hazards = 0;
    int triggers = 0;
    for (auto const& entry : kinds) {
        if (entry.kind == ObjectKind::Hazard) hazards = entry.objects;
        if (entry.kind == ObjectKind::Trigger) triggers = entry.objects;
    }
    pass = pass && hazards == 1 && triggers == 1;

    pass = pass && edit::removeTriggers(tpl) == 1;
    pass = pass && tpl.pieces.size() == 3;  // the piece still has its block

    pass = pass && edit::removeKind(tpl, ObjectKind::Hazard) == 1;
    // That piece had nothing else, so it goes and the rest renumber.
    pass = pass && tpl.pieces.size() == 2;
    pass = pass && tpl.grids[0].cells.size() == 2;
    for (auto const& cell : tpl.grids[0].cells) pass = pass && cell.piece < 2;

    std::cout << "editor-filtros: piezas=" << tpl.pieces.size()
              << " tipos=" << kinds.size() << '\n';
    return pass;
}

bool colourEditsRewriteOnlyChannels() {
    auto tpl = editableTemplate();
    bool pass = edit::remapChannel(tpl, 4, 40) == 3;
    pass = pass && tpl.pieces[0].objects[0].save.find("21,40") != std::string::npos;
    pass = pass && tpl.pieces[0].objects[0].save.find("22,5") != std::string::npos;

    pass = pass && edit::shiftChannels(tpl, 100) == 3;
    pass = pass && tpl.pieces[0].objects[0].save.find("21,140") != std::string::npos;
    pass = pass && tpl.pieces[0].objects[0].save.find("22,105") != std::string::npos;
    // The object id and position survive the rewrite untouched.
    pass = pass && tpl.pieces[0].objects[0].save.rfind("1,1,2,15,3,15", 0) == 0;

    std::cout << "editor-colores: " << tpl.pieces[0].objects[0].save << '\n';
    return pass;
}

bool duplicatingAPieceKeepsItPlaceable() {
    auto tpl = editableTemplate();
    bool pass = edit::duplicatePiece(tpl, 1);
    pass = pass && tpl.pieces.size() == 4 && tpl.links.size() == 4;
    // The copy inherits piece 1's rules and every rule that accepted piece 1.
    pass = pass && tpl.links[3].side[kRightDirection] == tpl.links[1].side[kRightDirection];
    auto const& right = tpl.links[0].side[kRightDirection];
    pass = pass && std::find(right.begin(), right.end(), 3) != right.end();

    edit::setWeight(tpl, 3, 7);
    pass = pass && tpl.pieces[3].weight == 7;
    edit::setWeight(tpl, 3, -5);
    pass = pass && tpl.pieces[3].weight == 1;

    std::cout << "editor-duplicar: piezas=" << tpl.pieces.size() << '\n';
    return pass;
}

} // namespace

int main() {
    bool pass = parsesTheLevelString();
    pass = separatesGroundFromBackdrop() && pass;
    pass = findsTheRepeatedPillar() && pass;
    pass = templateLooksLikeTheOriginal() && pass;
    pass = suggestionsKeepTheirKind() && pass;
    pass = triggersNeverReachATemplate() && pass;
    pass = paletteKnowsWhatEachChannelPaints() && pass;
    pass = readsTheGameplayBeats() && pass;
    pass = overridesRetagObjects() && pass;
    pass = triggerKeysBeatTheIdTable() && pass;
    pass = analysisStaysCheapOnABigLevel() && pass;
    pass = removingAPieceRenumbersEverything() && pass;
    pass = filtersDropObjectsAndEmptyPieces() && pass;
    pass = colourEditsRewriteOnlyChannels() && pass;
    pass = duplicatingAPieceKeepsItPlaceable() && pass;
    return pass ? 0 : 1;
}
