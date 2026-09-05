#include "GifStampLibrary.hpp"

#include "GifStampCatalog.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/ObjectToolbox.hpp>

#include <algorithm>
#include <vector>

using namespace geode::prelude;

namespace paimon::gifimport {

namespace {

// Cada objeto se dibuja en su celda de un atlas y se lee la lamina entera de una
// vez: mil objetos son cuatro lecturas de la GPU en vez de mil, que es la
// diferencia entre un parpadeo y medio minuto.
constexpr int kCellSide = kStampMaskSide;
constexpr int kAtlasCells = 16;
constexpr int kAtlasSide = kCellSide * kAtlasCells;
constexpr int kBatch = kAtlasCells * kAtlasCells;
constexpr unsigned char kAlphaFloor = 96;
// Por debajo de esto el objeto es un contorno o una chispa: como molde solo sabe
// dejar huecos, y el trazado de pintura ya cubre ese tamano mejor.
constexpr float kMinCoverage = 0.12f;

bool g_ready = false;

// Una tanda crea cientos de objetos y sus laminas: sin piscina propia no se
// sueltan hasta el final del fotograma y el pico de memoria es el de la
// biblioteca entera a la vez.
struct BatchPool {
    BatchPool() { CCPoolManager::sharedPoolManager()->push(); }
    ~BatchPool() { CCPoolManager::sharedPoolManager()->pop(); }
};

struct Pending {
    int objectId = 0;
    CCSize content{30.f, 30.f};
};

bool usableObject(GameObject* object) {
    if (!object) return false;
    if (object->m_objectType != GameObjectType::Decoration) return false;
    if (!object->m_isSolidColorBlock && !object->canChangeMainColor()) return false;
    auto const size = object->getContentSize();
    return size.width > 4.f && size.height > 4.f &&
        size.width < 512.f && size.height < 512.f;
}

// Recorta la celda a lo que pinta y la devuelve como molde, con el corrimiento
// que hace falta para que ese recorte caiga donde el plan lo pida.
bool cutStamp(
    unsigned char const* pixels,
    int stride,
    int cellX,
    int cellY,
    Pending const& pending,
    CatalogEntry& entry
) {
    int minX = kCellSide;
    int minY = kCellSide;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < kCellSide; ++y) {
        for (int x = 0; x < kCellSide; ++x) {
            std::size_t const index =
                (static_cast<std::size_t>(cellY + y) * stride + cellX + x) * 4;
            if (pixels[index + 3] < kAlphaFloor) continue;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX < minX || maxY < minY) return false;

    int const width = maxX - minX + 1;
    int const height = maxY - minY + 1;
    int covered = 0;
    entry.mask.width = width;
    entry.mask.height = height;
    entry.mask.coverage.assign(static_cast<std::size_t>(width) * height, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::size_t const index =
                (static_cast<std::size_t>(cellY + minY + y) * stride + cellX + minX + x) * 4;
            auto const alpha = pixels[index + 3];
            entry.mask.coverage[static_cast<std::size_t>(y) * width + x] = alpha;
            covered += alpha >= kAlphaFloor;
        }
    }
    if (covered < static_cast<int>(width * height * kMinCoverage)) return false;

    entry.objectId = pending.objectId;
    entry.baseWidth = pending.content.width * width / kCellSide;
    entry.baseHeight = pending.content.height * height / kCellSide;
    float const centerX = (minX + maxX + 1) * 0.5f;
    float const centerY = (minY + maxY + 1) * 0.5f;
    entry.offsetX = (kCellSide * 0.5f - centerX) / width;
    entry.offsetY = (kCellSide * 0.5f - centerY) / height;
    return true;
}

void drawBatch(
    std::vector<Pending> const& batch,
    std::vector<CCSpriteFrame*> const& frames,
    std::vector<CatalogEntry>& entries
) {
    auto* canvas = CCRenderTexture::create(
        kAtlasSide, kAtlasSide, kCCTexture2DPixelFormat_RGBA8888);
    if (!canvas) return;
    canvas->beginWithClear(0.f, 0.f, 0.f, 0.f);
    for (std::size_t slot = 0; slot < batch.size(); ++slot) {
        auto* sprite = CCSprite::createWithSpriteFrame(frames[slot]);
        if (!sprite) continue;
        auto const content = sprite->getContentSize();
        if (content.width < 1.f || content.height < 1.f) continue;
        int const column = static_cast<int>(slot) % kAtlasCells;
        int const row = static_cast<int>(slot) / kAtlasCells;
        sprite->setAnchorPoint({0.5f, 0.5f});
        sprite->setScaleX(kCellSide / content.width);
        sprite->setScaleY(kCellSide / content.height);
        // La lamina se lee con las filas de arriba abajo, asi que la fila 0 del
        // atlas se dibuja arriba del todo para que los indices cuadren.
        sprite->setPosition({
            (column + 0.5f) * kCellSide,
            kAtlasSide - (row + 0.5f) * kCellSide
        });
        sprite->visit();
    }
    canvas->end();

    auto* image = canvas->newCCImage(true);
    if (!image) return;
    auto const* pixels = image->getData();
    int const stride = image->getWidth();
    if (pixels && stride >= kAtlasSide && image->getHeight() >= kAtlasSide) {
        for (std::size_t slot = 0; slot < batch.size(); ++slot) {
            CatalogEntry entry;
            int const column = static_cast<int>(slot) % kAtlasCells;
            int const row = static_cast<int>(slot) / kAtlasCells;
            if (cutStamp(
                    pixels, stride, column * kCellSide, row * kCellSide,
                    batch[slot], entry)) {
                entries.push_back(std::move(entry));
            }
        }
    }
    image->release();
}

} // namespace

bool stampLibraryReady() {
    return g_ready;
}

std::size_t buildStampLibrary() {
    if (g_ready) return stampVariants().size();
    auto* toolbox = ObjectToolbox::sharedState();
    if (!toolbox) return 0;

    std::vector<int> ids;
    ids.reserve(toolbox->m_allKeys.size());
    for (auto const& key : toolbox->m_allKeys) ids.push_back(key.first);

    std::vector<CatalogEntry> entries;
    for (std::size_t start = 0; start < ids.size(); start += kBatch) {
        // displayFrame() devuelve una lamina nueva cada vez y se va con la
        // piscina, asi que la tanda se dibuja dentro del mismo ambito en el que
        // se pidieron los objetos.
        BatchPool const pool;
        std::vector<Pending> batch;
        std::vector<CCSpriteFrame*> frames;
        std::size_t const end = std::min(start + kBatch, ids.size());
        for (std::size_t index = start; index < end; ++index) {
            auto* object = GameObject::createWithKey(ids[index]);
            if (!usableObject(object)) continue;
            auto* frame = object->displayFrame();
            if (!frame) continue;
            batch.push_back({ids[index], object->getContentSize()});
            frames.push_back(frame);
        }
        if (!batch.empty()) drawBatch(batch, frames, entries);
    }

    g_ready = true;
    setStampCatalog(std::move(entries));
    auto const variants = stampVariants().size();
    log::info("[GifImport] Biblioteca de moldes: {} orientaciones", variants);
    return variants;
}

} // namespace paimon::gifimport
