// Paimon RTX - composicion final sobre el back buffer.
//
// Junta la escena original con la luz trazada, la oclusion, el bloom y los rayos,
// y encima aplica exposicion, mapeo de tonos, balance de color y los efectos de
// lente. u_mix mezcla contra la imagen intacta, asi que la intensidad global es
// un solo desvanecido en vez de tocar cada termino.
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

varying vec2 v_texCoord;

uniform sampler2D u_scene;
uniform sampler2D u_gi;
uniform sampler2D u_bloom;
uniform sampler2D u_rays;
uniform vec2  u_texel;
uniform float u_time;

uniform float u_mix;
uniform float u_giStrength;
uniform float u_aoStrength;
uniform float u_bloomStrength;
uniform float u_rayStrength;

uniform float u_tonemap;
uniform float u_exposure;
uniform float u_contrast;
uniform float u_saturation;
uniform float u_temperature;
uniform float u_tint;
uniform float u_gammaV;

uniform float u_ca;
uniform float u_vignette;
uniform float u_grain;
uniform float u_sharpen;

float luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 tonemapReinhard(vec3 c) {
    return c / (1.0 + c);
}

vec3 tonemapACES(vec3 c) {
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

vec3 tonemapFilmic(vec3 c) {
    vec3 x = max(vec3(0.0), c - 0.004);
    return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}

vec3 tonemapUncharted(vec3 c) {
    vec3 x = c * 2.0;
    vec3 m = ((x * (0.15 * x + 0.05) + 0.004) / (x * (0.15 * x + 0.50) + 0.06)) - 0.0666;
    float w = 11.2;
    float wm = ((w * (0.15 * w + 0.05) + 0.004) / (w * (0.15 * w + 0.50) + 0.06)) - 0.0666;
    return m / wm;
}

void main() {
    vec2 uv = v_texCoord;
    vec3 original = texture2D(u_scene, uv).rgb;

    vec3 base = original;
    if (u_ca > 0.0) {
        vec2 caOff = (uv - 0.5) * u_ca * 0.02;
        base.r = texture2D(u_scene, uv + caOff).r;
        base.b = texture2D(u_scene, uv - caOff).b;
    }

    if (u_sharpen > 0.0) {
        vec3 soft = (texture2D(u_scene, uv + vec2(u_texel.x, 0.0)).rgb +
                     texture2D(u_scene, uv - vec2(u_texel.x, 0.0)).rgb +
                     texture2D(u_scene, uv + vec2(0.0, u_texel.y)).rgb +
                     texture2D(u_scene, uv - vec2(0.0, u_texel.y)).rgb) * 0.25;
        base += (base - soft) * u_sharpen;
    }

    vec4 traced = texture2D(u_gi, uv);

    vec3 col = base * mix(1.0, clamp(traced.a, 0.0, 1.0), u_aoStrength);
    col += traced.rgb * u_giStrength;
    col += texture2D(u_bloom, uv).rgb * u_bloomStrength;
    col += texture2D(u_rays, uv).rgb * u_rayStrength;

    col *= exp2(u_exposure);

    if (u_tonemap > 3.5)      col = tonemapUncharted(col);
    else if (u_tonemap > 2.5) col = tonemapFilmic(col);
    else if (u_tonemap > 1.5) col = tonemapACES(col);
    else if (u_tonemap > 0.5) col = tonemapReinhard(col);
    col = clamp(col, 0.0, 1.0);

    col.r *= 1.0 + u_temperature * 0.25;
    col.b *= 1.0 - u_temperature * 0.25;
    col.g *= 1.0 + u_tint * 0.15;

    col = (col - 0.5) * u_contrast + 0.5;
    col = mix(vec3(luma(col)), col, u_saturation);
    col = pow(max(col, 0.0), vec3(1.0 / max(u_gammaV, 0.05)));

    vec2 vd = (uv - 0.5) * 2.0;
    col *= clamp(1.0 - dot(vd, vd) * 0.35 * u_vignette, 0.0, 1.0);

    if (u_grain > 0.0) {
        col += (hash12(gl_FragCoord.xy + u_time * 91.7) - 0.5) * u_grain * 0.12;
    }

    gl_FragColor = vec4(clamp(mix(original, col, u_mix), 0.0, 1.0), 1.0);
}
