#include "TemplateEdit.hpp"

#include "SaveString.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>

namespace paimon::autobuild::edit {

namespace {

ObjectKind kindOfObject(CapturedObject const& object) {
    auto kind = kindOf(object.objectId);
    if (kind != ObjectKind::Unknown) return kind;
    LevelObject probe;
    probe.id = object.objectId;
    probe.save = object.save;
    return looksLikeTrigger(probe) ? ObjectKind::Trigger : ObjectKind::Unknown;
}

// Every index that can point at a piece moves at once, so links and sample
// grids never end up describing a piece that is no longer there.
void applyRemap(Template& tpl, std::vector<int> const& remap, size_t newCount) {
    std::vector<Links> links(newCount);
    for (size_t old = 0; old < tpl.links.size() && old < remap.size(); ++old) {
        if (remap[old] < 0) continue;
        auto& into = links[remap[old]];
        into = tpl.links[old];
        for (int direction = 0; direction < kNeighbourDirections; ++direction) {
            std::vector<int> mapped;
            mapped.reserve(into.side[direction].size());
            for (int neighbour : into.side[direction]) {
                if (neighbour < 0 || neighbour >= static_cast<int>(remap.size())) continue;
                if (remap[neighbour] < 0) continue;
                mapped.push_back(remap[neighbour]);
            }
            std::sort(mapped.begin(), mapped.end());
            mapped.erase(std::unique(mapped.begin(), mapped.end()), mapped.end());
            into.side[direction] = std::move(mapped);
        }
    }
    tpl.links = std::move(links);

    for (auto& grid : tpl.grids) {
        std::vector<SampleCell> cells;
        cells.reserve(grid.cells.size());
        for (auto const& cell : grid.cells) {
            if (cell.piece < 0 || cell.piece >= static_cast<int>(remap.size())) continue;
            if (remap[cell.piece] < 0) continue;
            cells.push_back({cell.x, cell.y, remap[cell.piece]});
        }
        grid.cells = std::move(cells);
    }
    tpl.grids.erase(std::remove_if(tpl.grids.begin(), tpl.grids.end(),
                                   [](SampleGrid const& grid) { return grid.cells.empty(); }),
                    tpl.grids.end());
}

int forEachObject(Template& tpl, std::function<bool(CapturedObject const&)> const& drop) {
    int removed = 0;
    for (auto& piece : tpl.pieces) {
        auto const before = piece.objects.size();
        piece.objects.erase(std::remove_if(piece.objects.begin(), piece.objects.end(), drop),
                            piece.objects.end());
        removed += static_cast<int>(before - piece.objects.size());
        measurePiece(piece);
    }
    return removed;
}

} // namespace

std::vector<KindCount> countKinds(Template const& tpl) {
    std::map<ObjectKind, int> counts;
    for (auto const& piece : tpl.pieces) {
        for (auto const& object : piece.objects) counts[kindOfObject(object)]++;
    }
    std::vector<KindCount> out;
    out.reserve(counts.size());
    for (auto const& [kind, objects] : counts) out.push_back({kind, objects});
    std::sort(out.begin(), out.end(), [](KindCount const& a, KindCount const& b) {
        return a.objects > b.objects;
    });
    return out;
}

int removeKind(Template& tpl, ObjectKind kind) {
    int const removed = forEachObject(tpl, [kind](CapturedObject const& object) {
        return kindOfObject(object) == kind;
    });
    if (removed > 0) dropEmptyPieces(tpl);
    return removed;
}

int removeTriggers(Template& tpl) {
    return removeKind(tpl, ObjectKind::Trigger);
}

int keepOnlyKinds(Template& tpl, std::vector<ObjectKind> const& kinds) {
    std::set<ObjectKind> wanted(kinds.begin(), kinds.end());
    int const removed = forEachObject(tpl, [&wanted](CapturedObject const& object) {
        return wanted.find(kindOfObject(object)) == wanted.end();
    });
    if (removed > 0) dropEmptyPieces(tpl);
    return removed;
}

int dropEmptyPieces(Template& tpl) {
    bool empty = false;
    for (auto const& piece : tpl.pieces) empty = empty || piece.objects.empty();
    if (!empty) return 0;

    std::vector<int> remap(tpl.pieces.size(), -1);
    std::vector<Piece> pieces;
    pieces.reserve(tpl.pieces.size());
    for (size_t i = 0; i < tpl.pieces.size(); ++i) {
        if (tpl.pieces[i].objects.empty()) continue;
        remap[i] = static_cast<int>(pieces.size());
        pieces.push_back(std::move(tpl.pieces[i]));
    }
    int const dropped = static_cast<int>(tpl.pieces.size() - pieces.size());

    tpl.pieces = std::move(pieces);
    tpl.links.resize(remap.size());
    applyRemap(tpl, remap, tpl.pieces.size());
    return dropped;
}

bool removePiece(Template& tpl, int index) {
    if (index < 0 || index >= static_cast<int>(tpl.pieces.size())) return false;
    if (tpl.pieces.size() <= 1) return false;

    std::vector<int> remap(tpl.pieces.size(), -1);
    int next = 0;
    for (size_t i = 0; i < tpl.pieces.size(); ++i) {
        if (static_cast<int>(i) == index) continue;
        remap[i] = next++;
    }
    tpl.pieces.erase(tpl.pieces.begin() + index);
    tpl.links.resize(remap.size());
    applyRemap(tpl, remap, tpl.pieces.size());
    return true;
}

bool duplicatePiece(Template& tpl, int index) {
    if (index < 0 || index >= static_cast<int>(tpl.pieces.size())) return false;
    tpl.pieces.push_back(tpl.pieces[index]);
    if (tpl.mode == Mode::Wave) {
        tpl.links.resize(tpl.pieces.size());
        tpl.links.back() = tpl.links[index];
        // The copy takes the original's place in every rule, so it can stand
        // wherever the original could.
        for (auto& link : tpl.links) {
            for (int direction = 0; direction < kNeighbourDirections; ++direction) {
                auto& side = link.side[direction];
                if (std::find(side.begin(), side.end(), index) == side.end()) continue;
                side.push_back(static_cast<int>(tpl.pieces.size()) - 1);
            }
        }
    }
    return true;
}

void setWeight(Template& tpl, int index, int weight) {
    if (index < 0 || index >= static_cast<int>(tpl.pieces.size())) return;
    tpl.pieces[index].weight = std::clamp(weight, 1, 99999);
}

int remapChannel(Template& tpl, int from, int to) {
    if (from <= 0 || to <= 0 || from == to) return 0;
    int changed = 0;
    for (auto& piece : tpl.pieces) {
        for (auto& object : piece.objects) {
            auto rewritten = rewriteColors(object.save, [from, to](int id) {
                return id == from ? to : id;
            });
            if (rewritten == object.save) continue;
            object.save = std::move(rewritten);
            ++changed;
        }
    }
    return changed;
}

int shiftChannels(Template& tpl, int delta) {
    if (delta == 0) return 0;
    int changed = 0;
    for (auto& piece : tpl.pieces) {
        for (auto& object : piece.objects) {
            auto rewritten = rewriteColors(object.save, [delta](int id) {
                if (id < 1 || id > 999) return id;
                int const shifted = id + delta;
                return (shifted < 1 || shifted > 999) ? id : shifted;
            });
            if (rewritten == object.save) continue;
            object.save = std::move(rewritten);
            ++changed;
        }
    }
    return changed;
}

void rebuildLinks(Template& tpl) {
    tpl.links.assign(tpl.pieces.size(), Links{});
    if (tpl.mode != Mode::Wave || tpl.pieces.empty()) return;

    auto packCell = [](int x, int y) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
               static_cast<std::uint32_t>(y);
    };
    std::vector<std::array<std::set<int>, kNeighbourDirections>> sides(tpl.pieces.size());
    for (auto const& grid : tpl.grids) {
        std::unordered_map<std::uint64_t, int> byCell;
        for (auto const& cell : grid.cells) byCell[packCell(cell.x, cell.y)] = cell.piece;
        for (auto const& cell : grid.cells) {
            if (cell.piece < 0 || cell.piece >= static_cast<int>(tpl.pieces.size())) continue;
            for (int direction = 0; direction < kNeighbourDirections; ++direction) {
                auto found = byCell.find(packCell(cell.x + kDirectionX[direction],
                                                  cell.y + kDirectionY[direction]));
                if (found == byCell.end()) {
                    tpl.links[cell.piece].open[direction] = true;
                } else {
                    sides[cell.piece][direction].insert(found->second);
                }
            }
        }
    }
    for (size_t piece = 0; piece < tpl.pieces.size(); ++piece) {
        for (int direction = 0; direction < kNeighbourDirections; ++direction) {
            tpl.links[piece].side[direction].assign(sides[piece][direction].begin(),
                                                    sides[piece][direction].end());
        }
    }
}

} // namespace paimon::autobuild::edit
