#include "RTXRenderer.hpp"

#include "RTXManager.hpp"
#include "../../../core/RuntimeLifecycle.hpp"
#include "../../../utils/GLSLLoader.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/cocos/shaders/ccGLStateCache.h>

#include <algorithm>
#include <cmath>
#include <string>

using namespace cocos2d;
using namespace geode::prelude;

namespace paimon::rtx {

namespace {

// Sin actividad durante 5 segundos soltamos los FBOs: entre menus con RTX fuera
// de ambito no tiene sentido retener ~20 MB de VRAM.
constexpr unsigned kIdleReleaseFrames = 300;

// Tope duro del lado largo del trazado. El coste va con el numero de pixeles
// trazados, asi que a 1440p o 4K la escala al 100% se dispara sin que la imagen
// mejore: el resultado se filtra y se reescala igualmente.
constexpr int kMaxTraceLongEdge = 1280;

constexpr GLfloat kQuad[] = {
    -1.f,  1.f,  0.f, 1.f,
    -1.f, -1.f,  0.f, 0.f,
     1.f, -1.f,  1.f, 0.f,

    -1.f,  1.f,  0.f, 1.f,
     1.f, -1.f,  1.f, 0.f,
     1.f,  1.f,  1.f, 1.f,
};

GLuint compilePart(GLenum type, std::string const& src) {
    GLuint id = glCreateShader(type);
    if (!id) return 0;

    char const* ptr = src.c_str();
    glShaderSource(id, 1, &ptr, nullptr);
    glCompileShader(id);

    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char infoLog[1024] = {};
        glGetShaderInfoLog(id, sizeof(infoLog) - 1, nullptr, infoLog);
        log::warn("[PaimonRTX] fallo al compilar {}: {}",
                  type == GL_VERTEX_SHADER ? "vertex" : "fragment", infoLog);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

GLuint linkProgram(char const* tag, std::string const& vert, std::string const& frag) {
    GLuint vs = compilePart(GL_VERTEX_SHADER, vert);
    if (!vs) return 0;
    GLuint fs = compilePart(GL_FRAGMENT_SHADER, frag);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    // Mismos slots que cocos para que su cache de atributos siga siendo valida.
    glBindAttribLocation(prog, kCCVertexAttrib_Position, "aPosition");
    glBindAttribLocation(prog, kCCVertexAttrib_TexCoords, "aTexCoord");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char infoLog[1024] = {};
        glGetProgramInfoLog(prog, sizeof(infoLog) - 1, nullptr, infoLog);
        log::warn("[PaimonRTX] fallo al enlazar {}: {}", tag, infoLog);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

void bindSampler(GLuint prog, char const* name, int unit) {
    GLint loc = glGetUniformLocation(prog, name);
    if (loc != -1) glUniform1i(loc, unit);
}

} // namespace

RTXRenderer& RTXRenderer::get() {
    static RTXRenderer instance;
    return instance;
}

bool RTXRenderer::ensurePrograms() {
    if (m_trace.id && glIsProgram(m_trace.id) == GL_TRUE) return true;

    auto vert = paimon::shaders::readShaderFile("rtx_fullscreen.vert");
    auto traceSrc = paimon::shaders::readShaderFile("rtx_trace.glsl");
    auto temporalSrc = paimon::shaders::readShaderFile("rtx_temporal.glsl");
    auto atrousSrc = paimon::shaders::readShaderFile("rtx_atrous.glsl");
    auto bloomSrc = paimon::shaders::readShaderFile("rtx_bloom.glsl");
    auto compositeSrc = paimon::shaders::readShaderFile("rtx_composite.glsl");

    if (vert.empty() || traceSrc.empty() || temporalSrc.empty() || atrousSrc.empty()
        || bloomSrc.empty() || compositeSrc.empty()) {
        log::warn("[PaimonRTX] faltan shaders en resources/shaders - RTX desactivado");
        return false;
    }

    GLuint trace = linkProgram("trace", vert, traceSrc);
    GLuint temporal = linkProgram("temporal", vert, temporalSrc);
    GLuint atrous = linkProgram("atrous", vert, atrousSrc);
    GLuint bloom = linkProgram("bloom", vert, bloomSrc);
    GLuint composite = linkProgram("composite", vert, compositeSrc);

    if (!trace || !temporal || !atrous || !bloom || !composite) {
        if (trace) glDeleteProgram(trace);
        if (temporal) glDeleteProgram(temporal);
        if (atrous) glDeleteProgram(atrous);
        if (bloom) glDeleteProgram(bloom);
        if (composite) glDeleteProgram(composite);
        return false;
    }

    m_trace = TraceProgram{};
    m_trace.id               = trace;
    m_trace.texel            = glGetUniformLocation(trace, "u_texel");
    m_trace.frame            = glGetUniformLocation(trace, "u_frame");
    m_trace.rayCount         = glGetUniformLocation(trace, "u_rayCount");
    m_trace.raySteps         = glGetUniformLocation(trace, "u_raySteps");
    m_trace.rayDistance      = glGetUniformLocation(trace, "u_rayDistance");
    m_trace.stepGrowth       = glGetUniformLocation(trace, "u_stepGrowth");
    m_trace.lightThreshold   = glGetUniformLocation(trace, "u_lightThreshold");
    m_trace.lightRange       = glGetUniformLocation(trace, "u_lightRange");
    m_trace.bounceFalloff    = glGetUniformLocation(trace, "u_bounceFalloff");
    m_trace.giSaturation     = glGetUniformLocation(trace, "u_giSaturation");
    m_trace.normalStrength   = glGetUniformLocation(trace, "u_normalStrength");
    m_trace.thickness        = glGetUniformLocation(trace, "u_thickness");
    m_trace.aoRadius         = glGetUniformLocation(trace, "u_aoRadius");
    m_trace.aoPower          = glGetUniformLocation(trace, "u_aoPower");
    m_trace.reflectStrength  = glGetUniformLocation(trace, "u_reflectStrength");
    m_trace.reflectRoughness = glGetUniformLocation(trace, "u_reflectRoughness");
    m_trace.reflectFresnel   = glGetUniformLocation(trace, "u_reflectFresnel");
    m_trace.reflectFade      = glGetUniformLocation(trace, "u_reflectFade");
    ccGLUseProgram(trace);
    bindSampler(trace, "u_scene", 0);

    m_temporalProg = TemporalProgram{};
    m_temporalProg.id          = temporal;
    m_temporalProg.texel       = glGetUniformLocation(temporal, "u_texel");
    m_temporalProg.temporal    = glGetUniformLocation(temporal, "u_temporal");
    m_temporalProg.clampSigma  = glGetUniformLocation(temporal, "u_clampSigma");
    m_temporalProg.reprojNow   = glGetUniformLocation(temporal, "u_reprojNow");
    m_temporalProg.reprojPrev  = glGetUniformLocation(temporal, "u_reprojPrev");
    m_temporalProg.reprojScale = glGetUniformLocation(temporal, "u_reprojScale");
    ccGLUseProgram(temporal);
    bindSampler(temporal, "u_current", 0);
    bindSampler(temporal, "u_history", 1);

    m_atrousProg = AtrousProgram{};
    m_atrousProg.id     = atrous;
    m_atrousProg.texel  = glGetUniformLocation(atrous, "u_texel");
    m_atrousProg.stride = glGetUniformLocation(atrous, "u_stride");
    m_atrousProg.phi    = glGetUniformLocation(atrous, "u_phi");
    ccGLUseProgram(atrous);
    bindSampler(atrous, "u_src", 0);
    bindSampler(atrous, "u_guide", 1);

    m_bloom = BloomProgram{};
    m_bloom.id        = bloom;
    m_bloom.texel     = glGetUniformLocation(bloom, "u_texel");
    m_bloom.mode      = glGetUniformLocation(bloom, "u_mode");
    m_bloom.threshold = glGetUniformLocation(bloom, "u_threshold");
    m_bloom.radius    = glGetUniformLocation(bloom, "u_radius");
    m_bloom.lightPos  = glGetUniformLocation(bloom, "u_lightPos");
    m_bloom.decay     = glGetUniformLocation(bloom, "u_decay");
    m_bloom.density   = glGetUniformLocation(bloom, "u_density");
    ccGLUseProgram(bloom);
    bindSampler(bloom, "u_src", 0);
    bindSampler(bloom, "u_add", 1);

    m_composite = CompositeProgram{};
    m_composite.id            = composite;
    m_composite.texel         = glGetUniformLocation(composite, "u_texel");
    m_composite.time          = glGetUniformLocation(composite, "u_time");
    m_composite.mixAmount     = glGetUniformLocation(composite, "u_mix");
    m_composite.giStrength    = glGetUniformLocation(composite, "u_giStrength");
    m_composite.aoStrength    = glGetUniformLocation(composite, "u_aoStrength");
    m_composite.bloomStrength = glGetUniformLocation(composite, "u_bloomStrength");
    m_composite.rayStrength   = glGetUniformLocation(composite, "u_rayStrength");
    m_composite.tonemap       = glGetUniformLocation(composite, "u_tonemap");
    m_composite.exposure      = glGetUniformLocation(composite, "u_exposure");
    m_composite.contrast      = glGetUniformLocation(composite, "u_contrast");
    m_composite.saturation    = glGetUniformLocation(composite, "u_saturation");
    m_composite.temperature   = glGetUniformLocation(composite, "u_temperature");
    m_composite.tint          = glGetUniformLocation(composite, "u_tint");
    m_composite.gammaV        = glGetUniformLocation(composite, "u_gammaV");
    m_composite.ca            = glGetUniformLocation(composite, "u_ca");
    m_composite.vignette      = glGetUniformLocation(composite, "u_vignette");
    m_composite.grain         = glGetUniformLocation(composite, "u_grain");
    m_composite.sharpen       = glGetUniformLocation(composite, "u_sharpen");
    ccGLUseProgram(composite);
    bindSampler(composite, "u_scene", 0);
    bindSampler(composite, "u_gi", 1);
    bindSampler(composite, "u_bloom", 2);
    bindSampler(composite, "u_rays", 3);

    if (!m_vbo) {
        glGenBuffers(1, &m_vbo);
        if (!m_vbo) return false;
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (!m_blackTex) {
        unsigned char const black[4] = {0, 0, 0, 0};
        glGenTextures(1, &m_blackTex);
        ccGLBindTexture2DN(0, m_blackTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    log::info("[PaimonRTX] programas compilados");
    return true;
}

bool RTXRenderer::makeTarget(Target& t, int w, int h) {
    w = std::max(1, w);
    h = std::max(1, h);
    if (t.fbo && t.w == w && t.h == h) return true;

    dropTarget(t);

    glGenTextures(1, &t.tex);
    ccGLBindTexture2DN(0, t.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        log::warn("[PaimonRTX] FBO incompleto ({}x{})", w, h);
        dropTarget(t);
        return false;
    }

    // El alfa lleva la oclusion, asi que el historial tiene que arrancar en 1 o
    // el primer fotograma sale completamente a oscuras.
    GLfloat prevClear[4] = {0.f, 0.f, 0.f, 1.f};
    glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClear);
    glViewport(0, 0, w, h);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(prevClear[0], prevClear[1], prevClear[2], prevClear[3]);

    t.w = w;
    t.h = h;
    return true;
}

void RTXRenderer::dropTarget(Target& t) {
    if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    t = Target{};
}

bool RTXRenderer::ensureFullTargets(int srcW, int srcH) {
    if (m_sceneTex && srcW == m_sceneW && srcH == m_sceneH) return true;

    if (m_sceneTex) {
        glDeleteTextures(1, &m_sceneTex);
        m_sceneTex = 0;
    }

    glGenTextures(1, &m_sceneTex);
    if (!m_sceneTex) return false;
    ccGLBindTexture2DN(0, m_sceneTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, srcW, srcH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    for (int i = 0; i < kBloomLevels; ++i) {
        int const w = std::max(1, srcW >> (i + 1));
        int const h = std::max(1, srcH >> (i + 1));
        if (!makeTarget(m_bloomDown[i], w, h)) return false;
        if (!makeTarget(m_bloomUp[i], w, h)) return false;
    }

    int const rayLevel = std::min(kBloomLevels - 1, 2);
    if (!makeTarget(m_rays, m_bloomDown[rayLevel].w, m_bloomDown[rayLevel].h)) return false;

    m_sceneW = srcW;
    m_sceneH = srcH;
    log::debug("[PaimonRTX] objetivos de pantalla completa {}x{}", srcW, srcH);
    return true;
}

bool RTXRenderer::ensureTraceTargets(int srcW, int srcH, float scale) {
    int w = std::max(64, static_cast<int>(std::lround(srcW * scale)));
    int h = std::max(64, static_cast<int>(std::lround(srcH * scale)));

    int const longEdge = std::max(w, h);
    if (longEdge > kMaxTraceLongEdge) {
        float const k = static_cast<float>(kMaxTraceLongEdge) / static_cast<float>(longEdge);
        w = std::max(64, static_cast<int>(std::lround(w * k)));
        h = std::max(64, static_cast<int>(std::lround(h * k)));
    }

    if (m_traceRT.fbo && w == m_traceW && h == m_traceH) return true;

    if (!makeTarget(m_traceSrc, w, h)) return false;
    if (!makeTarget(m_traceRT, w, h)) return false;
    if (!makeTarget(m_history[0], w, h)) return false;
    if (!makeTarget(m_history[1], w, h)) return false;
    if (!makeTarget(m_atrous[0], w, h)) return false;
    if (!makeTarget(m_atrous[1], w, h)) return false;

    m_traceW = w;
    m_traceH = h;
    m_giResultTex = m_history[m_historyIndex].tex;
    // El historial recien creado no corresponde a la camara anterior.
    m_hasPrevCamera = false;
    log::debug("[PaimonRTX] objetivos de trazado {}x{} (escala {:.2f})", w, h, scale);
    return true;
}

void RTXRenderer::releaseAll() {
    dropTarget(m_traceSrc);
    dropTarget(m_traceRT);
    dropTarget(m_history[0]);
    dropTarget(m_history[1]);
    dropTarget(m_atrous[0]);
    dropTarget(m_atrous[1]);
    for (int i = 0; i < kBloomLevels; ++i) {
        dropTarget(m_bloomDown[i]);
        dropTarget(m_bloomUp[i]);
    }
    dropTarget(m_rays);

    if (m_sceneTex) {
        glDeleteTextures(1, &m_sceneTex);
        m_sceneTex = 0;
    }
    m_sceneW = 0;
    m_sceneH = 0;
    m_traceW = 0;
    m_traceH = 0;
    m_bloomResultTex = 0;
    m_giResultTex = 0;
    m_hasPrevCamera = false;
}

void RTXRenderer::onGLContextReload() {
    // El contexto viejo sigue vivo aqui, asi que los delete van sobre names
    // propios; todo se reconstruye perezosamente en el siguiente renderFrame.
    releaseAll();

    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_blackTex) {
        glDeleteTextures(1, &m_blackTex);
        m_blackTex = 0;
    }
    if (m_trace.id) glDeleteProgram(m_trace.id);
    if (m_temporalProg.id) glDeleteProgram(m_temporalProg.id);
    if (m_atrousProg.id) glDeleteProgram(m_atrousProg.id);
    if (m_bloom.id) glDeleteProgram(m_bloom.id);
    if (m_composite.id) glDeleteProgram(m_composite.id);

    m_trace = TraceProgram{};
    m_temporalProg = TemporalProgram{};
    m_atrousProg = AtrousProgram{};
    m_bloom = BloomProgram{};
    m_composite = CompositeProgram{};

    m_wasActive = false;
    m_broken = false;
}

void RTXRenderer::drawInto(Target const& t) {
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glViewport(0, 0, t.w, t.h);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void RTXRenderer::updateAdaptiveScale(RTXConfig const& cfg) {
    if (!cfg.adaptive) {
        m_activeScale = cfg.renderScale;
        return;
    }

    if (++m_adaptTicks < 30) return;
    m_adaptTicks = 0;

    float const budget = 1000.f / static_cast<float>(cfg.targetFps);
    if (m_frameMs > budget * 1.15f && m_activeScale > 0.20f) {
        m_activeScale = std::max(0.20f, m_activeScale - 0.05f);
        m_upTicks = 0;
        return;
    }

    // El umbral de subida esta justo por encima del presupuesto, no muy por
    // debajo: con vsync el fotograma dura siempre lo mismo, y contra un margen
    // holgado la resolucion bajaria una vez y no volveria a subir nunca. Subir
    // sigue siendo cuatro veces mas lento que bajar para que no oscile.
    if (m_frameMs < budget * 1.02f && m_activeScale < cfg.renderScale) {
        if (++m_upTicks >= 4) {
            m_upTicks = 0;
            m_activeScale = std::min(cfg.renderScale, m_activeScale + 0.05f);
        }
    }
}

void RTXRenderer::runTrace(RTXConfig const& cfg) {
    float const texelX = 1.f / static_cast<float>(m_traceW);
    float const texelY = 1.f / static_cast<float>(m_traceH);

    ccGLUseProgram(m_bloom.id);
    glUniform1f(m_bloom.mode, 1.f);
    glUniform2f(m_bloom.texel, 1.f / static_cast<float>(m_sceneW),
                               1.f / static_cast<float>(m_sceneH));
    ccGLBindTexture2DN(0, m_sceneTex);
    ccGLBindTexture2DN(1, m_blackTex);
    drawInto(m_traceSrc);

    ccGLUseProgram(m_trace.id);
    glUniform2f(m_trace.texel, texelX, texelY);
    glUniform1f(m_trace.frame, static_cast<float>(m_frameCounter % 4096u));
    glUniform1f(m_trace.rayCount, static_cast<float>(cfg.rayCount));
    glUniform1f(m_trace.raySteps, static_cast<float>(cfg.raySteps));
    glUniform1f(m_trace.rayDistance, cfg.rayDistance);
    glUniform1f(m_trace.stepGrowth, cfg.stepGrowth);
    glUniform1f(m_trace.lightThreshold, cfg.lightThreshold);
    glUniform1f(m_trace.lightRange, cfg.lightRange);
    glUniform1f(m_trace.bounceFalloff, cfg.bounceFalloff);
    glUniform1f(m_trace.giSaturation, cfg.giSaturation);
    glUniform1f(m_trace.normalStrength, cfg.normalStrength);
    glUniform1f(m_trace.thickness, cfg.thickness);
    glUniform1f(m_trace.aoRadius, cfg.aoRadius);
    glUniform1f(m_trace.aoPower, cfg.aoPower);
    glUniform1f(m_trace.reflectStrength, cfg.reflectStrength);
    glUniform1f(m_trace.reflectRoughness, cfg.reflectRoughness);
    glUniform1f(m_trace.reflectFresnel, cfg.reflectFresnel);
    glUniform1f(m_trace.reflectFade, cfg.reflectFade);
    ccGLBindTexture2DN(0, m_traceSrc.tex);
    drawInto(m_traceRT);

    runFilter(cfg);
}

void RTXRenderer::runFilter(RTXConfig const& cfg) {
    float const texelX = 1.f / static_cast<float>(m_traceW);
    float const texelY = 1.f / static_cast<float>(m_traceH);

    // La capa de objetos del juego solo traslada y escala, asi que su
    // transformada al mundo describe entera la correspondencia entre el
    // fotograma anterior y este. En los menus no hay capa y la reproyeccion
    // queda en identidad.
    float nowX = 0.f, nowY = 0.f, prevX = 0.f, prevY = 0.f, ratio = 1.f;
    auto* game = GJBaseGameLayer::get();
    auto* layer = game ? game->m_objectLayer : nullptr;
    if (layer) {
        auto const win = CCDirector::get()->getWinSize();
        auto const xf = layer->nodeToWorldTransform();
        float const scale = std::abs(xf.a) > 0.0001f ? xf.a : 1.f;

        nowX = xf.tx / win.width;
        nowY = xf.ty / win.height;
        if (m_hasPrevCamera) {
            prevX = m_prevCamX / win.width;
            prevY = m_prevCamY / win.height;
            ratio = m_prevCamScale / scale;
        } else {
            prevX = nowX;
            prevY = nowY;
        }

        m_prevCamX = xf.tx;
        m_prevCamY = xf.ty;
        m_prevCamScale = scale;
        m_hasPrevCamera = true;
    } else {
        m_hasPrevCamera = false;
    }

    int const dst = 1 - m_historyIndex;
    ccGLUseProgram(m_temporalProg.id);
    glUniform2f(m_temporalProg.texel, texelX, texelY);
    glUniform1f(m_temporalProg.temporal, cfg.temporal);
    glUniform1f(m_temporalProg.clampSigma, cfg.ghostClamp ? cfg.clampSigma : 0.f);
    glUniform2f(m_temporalProg.reprojNow, nowX, nowY);
    glUniform2f(m_temporalProg.reprojPrev, prevX, prevY);
    glUniform1f(m_temporalProg.reprojScale, ratio);
    ccGLBindTexture2DN(0, m_traceRT.tex);
    ccGLBindTexture2DN(1, m_history[m_historyIndex].tex);
    drawInto(m_history[dst]);
    m_historyIndex = dst;

    int const passes = std::clamp(cfg.atrousPasses, 0, 5);
    if (passes == 0) {
        m_giResultTex = m_history[m_historyIndex].tex;
        return;
    }

    // El corte va sobre la diferencia de luminancia con el pixel central, asi
    // que phi alto deja de mezclar en cuanto hay borde (nitido y ruidoso) y phi
    // bajo mezcla a traves de todo (limpio y plano).
    float const phi = 48.f - std::clamp(cfg.denoise, 0.f, 4.f) * 11.f;

    ccGLUseProgram(m_atrousProg.id);
    glUniform2f(m_atrousProg.texel, texelX, texelY);
    glUniform1f(m_atrousProg.phi, phi);
    ccGLBindTexture2DN(1, m_traceSrc.tex);

    GLuint src = m_history[m_historyIndex].tex;
    int out = 0;
    for (int i = 0; i < passes; ++i) {
        glUniform1f(m_atrousProg.stride, static_cast<float>(1 << i));
        ccGLBindTexture2DN(0, src);
        drawInto(m_atrous[out]);
        src = m_atrous[out].tex;
        out = 1 - out;
    }
    m_giResultTex = src;
}

void RTXRenderer::runBloom(RTXConfig const& cfg) {
    int const levels = std::clamp(cfg.bloomPasses, 1, kBloomLevels);

    ccGLUseProgram(m_bloom.id);
    ccGLBindTexture2DN(1, m_blackTex);

    glUniform1f(m_bloom.mode, 0.f);
    glUniform1f(m_bloom.threshold, cfg.bloomThreshold);
    glUniform2f(m_bloom.texel, 1.f / static_cast<float>(m_sceneW),
                               1.f / static_cast<float>(m_sceneH));
    ccGLBindTexture2DN(0, m_sceneTex);
    drawInto(m_bloomDown[0]);

    glUniform1f(m_bloom.mode, 1.f);
    for (int i = 1; i < levels; ++i) {
        glUniform2f(m_bloom.texel, 1.f / static_cast<float>(m_bloomDown[i - 1].w),
                                   1.f / static_cast<float>(m_bloomDown[i - 1].h));
        ccGLBindTexture2DN(0, m_bloomDown[i - 1].tex);
        drawInto(m_bloomDown[i]);
    }

    glUniform1f(m_bloom.mode, 2.f);
    glUniform1f(m_bloom.radius, cfg.bloomRadius);
    for (int i = levels - 2; i >= 0; --i) {
        Target const& src = (i == levels - 2) ? m_bloomDown[levels - 1] : m_bloomUp[i + 1];
        glUniform2f(m_bloom.texel, 1.f / static_cast<float>(src.w),
                                   1.f / static_cast<float>(src.h));
        ccGLBindTexture2DN(0, src.tex);
        ccGLBindTexture2DN(1, m_bloomDown[i].tex);
        drawInto(m_bloomUp[i]);
    }

    m_bloomResultTex = (levels == 1) ? m_bloomDown[0].tex : m_bloomUp[0].tex;

    if (cfg.godRayStrength > 0.001f) {
        Target const& src = m_bloomDown[std::min(levels - 1, 2)];
        glUniform1f(m_bloom.mode, 3.f);
        glUniform2f(m_bloom.lightPos, cfg.godRayX, cfg.godRayY);
        glUniform1f(m_bloom.decay, cfg.godRayDecay);
        glUniform1f(m_bloom.density, cfg.godRayDensity);
        glUniform2f(m_bloom.texel, 1.f / static_cast<float>(src.w),
                                   1.f / static_cast<float>(src.h));
        ccGLBindTexture2DN(0, src.tex);
        ccGLBindTexture2DN(1, m_blackTex);
        drawInto(m_rays);
    }
}

void RTXRenderer::runComposite(RTXConfig const& cfg, GLint const* viewport, GLuint prevFbo) {
    ccGLUseProgram(m_composite.id);
    glUniform2f(m_composite.texel, 1.f / static_cast<float>(m_sceneW),
                                   1.f / static_cast<float>(m_sceneH));
    glUniform1f(m_composite.time, m_shaderTime);
    glUniform1f(m_composite.mixAmount, cfg.intensity);
    glUniform1f(m_composite.giStrength, cfg.giStrength);
    glUniform1f(m_composite.aoStrength, cfg.aoStrength);
    glUniform1f(m_composite.bloomStrength, cfg.bloomStrength);
    glUniform1f(m_composite.rayStrength, cfg.godRayStrength);
    glUniform1f(m_composite.tonemap, static_cast<float>(cfg.tonemap));
    glUniform1f(m_composite.exposure, cfg.exposure);
    glUniform1f(m_composite.contrast, cfg.contrast);
    glUniform1f(m_composite.saturation, cfg.saturation);
    glUniform1f(m_composite.temperature, cfg.temperature);
    glUniform1f(m_composite.tint, cfg.tint);
    glUniform1f(m_composite.gammaV, cfg.gamma);
    glUniform1f(m_composite.ca, cfg.chromatic);
    glUniform1f(m_composite.vignette, cfg.vignette);
    glUniform1f(m_composite.grain, cfg.grain);
    glUniform1f(m_composite.sharpen, cfg.sharpen);

    ccGLBindTexture2DN(0, m_sceneTex);
    ccGLBindTexture2DN(1, m_giResultTex ? m_giResultTex : m_blackTex);
    ccGLBindTexture2DN(2, m_bloomResultTex ? m_bloomResultTex : m_blackTex);
    ccGLBindTexture2DN(3, (cfg.godRayStrength > 0.001f && m_rays.tex) ? m_rays.tex : m_blackTex);

    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void RTXRenderer::renderFrame() {
    if (paimon::isRuntimeShuttingDown() || m_broken) return;

    auto const& cfg = RTXManager::get().config();
    if (!RTXManager::get().shouldRender()) {
        m_wasActive = false;
        if (m_sceneTex && ++m_idleFrames > kIdleReleaseFrames) {
            releaseAll();
            m_idleFrames = 0;
        }
        return;
    }
    m_idleFrames = 0;

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) return;

    if (!ensurePrograms()) {
        m_broken = true;
        log::warn("[PaimonRTX] no se pudieron preparar los shaders - RTX apagado esta sesion");
        return;
    }

    auto const now = std::chrono::steady_clock::now();
    if (m_wasActive) {
        float const ms = std::chrono::duration<float, std::milli>(now - m_lastFrame).count();
        m_frameMs = m_frameMs > 0.f ? m_frameMs * 0.9f + ms * 0.1f : ms;
        m_shaderTime += ms * 0.001f;
        updateAdaptiveScale(cfg);
    } else {
        m_activeScale = cfg.renderScale;
        m_frameMs = 0.f;
    }
    m_lastFrame = now;
    m_wasActive = true;

    // Se captura antes de crear render targets: makeTarget deja su propio FBO y
    // su propio viewport puestos, y el compuesto tiene que volver al back buffer.
    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLboolean const scissor = glIsEnabled(GL_SCISSOR_TEST);
    if (scissor) glDisable(GL_SCISSOR_TEST);

    while (glGetError() != GL_NO_ERROR) {}

    if (ensureFullTargets(viewport[2], viewport[3])
        && ensureTraceTargets(viewport[2], viewport[3], m_activeScale)) {

        ccGLBindTexture2DN(0, m_sceneTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], m_sceneW, m_sceneH);

        ccGLBlendFunc(GL_ONE, GL_ZERO);
#if CC_TEXTURE_ATLAS_USE_VAO
        ccGLBindVAO(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ccGLEnableVertexAttribs(kCCVertexAttribFlag_Position | kCCVertexAttribFlag_TexCoords);
        glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), reinterpret_cast<void*>(0));
        glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), reinterpret_cast<void*>(2 * sizeof(GLfloat)));

        bool const wantsTrace = cfg.giStrength > 0.001f || cfg.aoStrength > 0.001f
                             || cfg.reflectStrength > 0.001f;
        bool const wantsBloom = cfg.bloomStrength > 0.001f || cfg.godRayStrength > 0.001f;

        unsigned const cadence = static_cast<unsigned>(std::max(1, cfg.frameSkip + 1));
        if (wantsTrace && m_frameCounter % cadence == 0) runTrace(cfg);

        if (wantsBloom) {
            runBloom(cfg);
        } else {
            m_bloomResultTex = 0;
        }

        runComposite(cfg, viewport, static_cast<GLuint>(prevFbo));

        GLenum const err = glGetError();
        if (err != GL_NO_ERROR) {
            m_broken = true;
            log::warn("[PaimonRTX] error GL 0x{:X} en el postproceso - RTX apagado esta sesion",
                      static_cast<unsigned>(err));
        }
    } else {
        m_broken = true;
        log::warn("[PaimonRTX] no se pudieron crear los render targets - RTX apagado esta sesion");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    ccGLBindTexture2DN(3, 0);
    ccGLBindTexture2DN(2, 0);
    ccGLBindTexture2DN(1, 0);
    ccGLBindTexture2DN(0, 0);
    glActiveTexture(GL_TEXTURE0);
    if (scissor) glEnable(GL_SCISSOR_TEST);

    m_frameCounter++;
}

} // namespace paimon::rtx
