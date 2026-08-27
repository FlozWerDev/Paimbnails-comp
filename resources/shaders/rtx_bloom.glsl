// Paimon RTX - cadena de bloom y rayos volumetricos.
//
// Un solo programa con cuatro modos para no compilar cuatro: 0 recorta por
// umbral y baja de resolucion, 1 solo baja, 2 sube con filtro de carpa y suma
// el nivel fino, 3 marcha radialmente desde el foco para los god rays.
#ifdef GL_ES
precision mediump float;
#endif

varying vec2 v_texCoord;

uniform sampler2D u_src;
uniform sampler2D u_add;
uniform vec2  u_texel;
uniform float u_mode;
uniform float u_threshold;
uniform float u_radius;
uniform vec2  u_lightPos;
uniform float u_decay;
uniform float u_density;

const int kRaySamples = 24;

float luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 box4(vec2 uv, vec2 t) {
    return (texture2D(u_src, uv + vec2(-t.x, -t.y)).rgb +
            texture2D(u_src, uv + vec2( t.x, -t.y)).rgb +
            texture2D(u_src, uv + vec2(-t.x,  t.y)).rgb +
            texture2D(u_src, uv + vec2( t.x,  t.y)).rgb) * 0.25;
}

vec3 tent9(vec2 uv, vec2 t) {
    vec3 c = texture2D(u_src, uv).rgb * 4.0;
    c += texture2D(u_src, uv + vec2(-t.x, 0.0)).rgb * 2.0;
    c += texture2D(u_src, uv + vec2( t.x, 0.0)).rgb * 2.0;
    c += texture2D(u_src, uv + vec2(0.0, -t.y)).rgb * 2.0;
    c += texture2D(u_src, uv + vec2(0.0,  t.y)).rgb * 2.0;
    c += texture2D(u_src, uv + vec2(-t.x, -t.y)).rgb;
    c += texture2D(u_src, uv + vec2( t.x, -t.y)).rgb;
    c += texture2D(u_src, uv + vec2(-t.x,  t.y)).rgb;
    c += texture2D(u_src, uv + vec2( t.x,  t.y)).rgb;
    return c / 16.0;
}

void main() {
    vec2 uv = v_texCoord;
    vec3 outColor;

    if (u_mode > 2.5) {
        vec2 delta = (uv - u_lightPos) * u_density / float(kRaySamples);
        vec2 p = uv;
        float illum = 1.0;
        vec3 acc = vec3(0.0);
        for (int i = 0; i < kRaySamples; i++) {
            p -= delta;
            acc += texture2D(u_src, clamp(p, 0.0, 1.0)).rgb * illum;
            illum *= u_decay;
        }
        outColor = acc / float(kRaySamples);
    } else if (u_mode > 1.5) {
        outColor = tent9(uv, u_texel * u_radius) + texture2D(u_add, uv).rgb;
    } else if (u_mode > 0.5) {
        outColor = box4(uv, u_texel);
    } else {
        vec3 c = box4(uv, u_texel);
        float l = luma(c);
        outColor = c * (max(l - u_threshold, 0.0) / max(l, 0.0001));
    }

    gl_FragColor = vec4(outColor, 1.0);
}
