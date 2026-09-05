#include "GifSourceScaler.hpp"

#include "GifParallel.hpp"

#include "../../../utils/GLSLLoader.hpp"

#include <Geode/Geode.hpp>
#include <Geode/cocos/platform/CCGL.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace geode::prelude;

namespace paimon::gifimport {

namespace {

// La rejilla mas fina que el trazado llega a mirar es el doble de la pedida —el
// modo render compara el detalle a 2x— y el muestreo por area quiere un par de
// pixeles por celda. Con cuatro veces la rejilla no se pierde nada de lo que se
// va a acabar viendo.
constexpr int kWorkingFactor = 4;
constexpr int kMinWorking = 128;

// Cada frame deja media docena de render targets por el camino. Sin una piscina
// propia no se sueltan hasta el final del fotograma, y un video entero se come
// la memoria de golpe antes de que le toque el turno al recolector.
struct FramePool {
    FramePool() { CCPoolManager::sharedPoolManager()->push(); }
    ~FramePool() { CCPoolManager::sharedPoolManager()->pop(); }
};

void reduceFrame(
    std::uint8_t const* source,
    int sourceWidth,
    int sourceHeight,
    std::uint8_t* target,
    int width,
    int height
) {
    for (int y = 0; y < height; ++y) {
        int const fromY = y * sourceHeight / height;
        int const toY = std::max(fromY + 1, (y + 1) * sourceHeight / height);
        for (int x = 0; x < width; ++x) {
            int const fromX = x * sourceWidth / width;
            int const toX = std::max(fromX + 1, (x + 1) * sourceWidth / width);
            std::uint32_t color[3]{};
            std::uint32_t alpha = 0;
            std::uint32_t weight = 0;
            for (int sampleY = fromY; sampleY < toY; ++sampleY) {
                auto const* row = source +
                    (static_cast<std::size_t>(sampleY) * sourceWidth + fromX) * 4;
                for (int sampleX = fromX; sampleX < toX; ++sampleX, row += 4) {
                    color[0] += static_cast<std::uint32_t>(row[0]) * row[3];
                    color[1] += static_cast<std::uint32_t>(row[1]) * row[3];
                    color[2] += static_cast<std::uint32_t>(row[2]) * row[3];
                    alpha += row[3];
                    ++weight;
                }
            }
            auto* pixel = target + (static_cast<std::size_t>(y) * width + x) * 4;
            pixel[3] = static_cast<std::uint8_t>(alpha / std::max(weight, 1u));
            // Sin ponderar por alfa, el negro transparente del borde de un
            // sprite se cuela en el color y el dibujo sale con orla oscura.
            for (int channel = 0; channel < 3; ++channel) {
                pixel[channel] = alpha > 0
                    ? static_cast<std::uint8_t>(color[channel] / alpha)
                    : 0;
            }
        }
    }
}

CCRenderTexture* renderPass(
    CCTexture2D* texture,
    CCSize const& sourceSize,
    int width,
    int height,
    bool flipped,
    CCGLProgram* shader
) {
    auto* canvas = CCRenderTexture::create(
        width, height, kCCTexture2DPixelFormat_RGBA8888);
    if (!canvas) return nullptr;
    auto* sprite = CCSprite::createWithTexture(texture);
    if (!sprite) return nullptr;

    sprite->setAnchorPoint({0.f, 0.f});
    sprite->setPosition({0.f, 0.f});
    sprite->setScaleX(static_cast<float>(width) / sourceSize.width);
    sprite->setScaleY(static_cast<float>(height) / sourceSize.height);
    // La textura de un render target viene del reves; volverla a dar la vuelta
    // deja la cadena de pasadas siempre en la misma orientacion.
    sprite->setFlipY(flipped);
    // El alfa del destino tiene que ser el que escribe el shader, no el que
    // saldria de mezclarlo contra el hueco: el trazado lee ese canal.
    sprite->setBlendFunc({GL_ONE, GL_ZERO});
    sprite->setShaderProgram(shader);

    shader->use();
    shader->setUniformsForBuiltins();
    GLint const texel = shader->getUniformLocationForName("u_texel");
    if (texel != -1) {
        shader->setUniformLocationWith2f(
            texel, 1.f / sourceSize.width, 1.f / sourceSize.height);
    }

    ccTexParams params{GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
    texture->setTexParameters(&params);

    canvas->beginWithClear(0.f, 0.f, 0.f, 0.f);
    sprite->visit();
    canvas->end();
    return canvas;
}

bool reduceOnGpu(
    SourceAnimation const& source,
    SourceAnimation& target,
    int width,
    int height
) {
    auto* shader = paimon::shaders::getGifDownscaleShader();
    if (!shader) return false;
    auto* director = CCDirector::get();
    if (!director || !director->getOpenGLView()) return false;

    CCSize const full{
        static_cast<float>(source.width), static_cast<float>(source.height)};
    for (std::size_t index = 0; index < source.frames.size(); ++index) {
        FramePool const pool;
        auto* texture = new CCTexture2D();
        bool const uploaded = texture->initWithData(
            source.frames[index].rgba.data(), kCCTexture2DPixelFormat_RGBA8888,
            source.width, source.height, full);
        if (!uploaded) {
            texture->release();
            return false;
        }

        CCRenderTexture* canvas = nullptr;
        CCTexture2D* input = texture;
        CCSize step = full;
        bool flipped = false;
        // Mitad a mitad mientras quepa: una sola reduccion grande con filtro
        // bilineal solo mira cuatro texeles y se deja fuera casi toda la imagen.
        while (true) {
            int const nextWidth = std::max(width, static_cast<int>(step.width) / 2);
            int const nextHeight = std::max(height, static_cast<int>(step.height) / 2);
            canvas = renderPass(input, step, nextWidth, nextHeight, flipped, shader);
            if (!canvas) {
                texture->release();
                return false;
            }
            step = CCSize(
                static_cast<float>(nextWidth), static_cast<float>(nextHeight));
            input = canvas->getSprite()->getTexture();
            flipped = true;
            if (nextWidth <= width && nextHeight <= height) break;
        }

        auto* image = canvas->newCCImage(true);
        texture->release();
        if (!image) return false;
        bool const usable = image->getData() &&
            image->getWidth() >= width && image->getHeight() >= height;
        if (usable) {
            auto& frame = target.frames[index];
            auto const* pixels = image->getData();
            int const stride = image->getWidth();
            for (int y = 0; y < height; ++y) {
                std::memcpy(
                    frame.rgba.data() + static_cast<std::size_t>(y) * width * 4,
                    pixels + static_cast<std::size_t>(y) * stride * 4,
                    static_cast<std::size_t>(width) * 4);
            }
        }
        image->release();
        if (!usable) return false;
    }
    return true;
}

} // namespace

std::shared_ptr<SourceAnimation> prescaleSource(
    std::shared_ptr<SourceAnimation> source,
    int maxDimension
) {
    if (!source || source->frames.empty() || source->width <= 0 || source->height <= 0) {
        return source;
    }
    int const working = std::max(kMinWorking, maxDimension * kWorkingFactor);
    int const longest = std::max(source->width, source->height);
    if (longest <= working) return source;

    auto reduced = std::make_shared<SourceAnimation>();
    reduced->width = source->width >= source->height
        ? working
        : std::max(1, working * source->width / source->height);
    reduced->height = source->width >= source->height
        ? std::max(1, working * source->height / source->width)
        : working;
    reduced->frames.resize(source->frames.size());
    for (std::size_t index = 0; index < source->frames.size(); ++index) {
        reduced->frames[index].delayMs = source->frames[index].delayMs;
        reduced->frames[index].rgba.assign(
            static_cast<std::size_t>(reduced->width) * reduced->height * 4, 0);
    }

    if (!reduceOnGpu(*source, *reduced, reduced->width, reduced->height)) {
        parallelFor(source->frames.size(), [&](std::size_t index) {
            reduceFrame(
                source->frames[index].rgba.data(), source->width, source->height,
                reduced->frames[index].rgba.data(), reduced->width, reduced->height);
        });
    }
    log::info(
        "[GifImport] Fuente reducida de {}x{} a {}x{} ({} frames)",
        source->width, source->height, reduced->width, reduced->height,
        reduced->frames.size());
    return reduced;
}

} // namespace paimon::gifimport
