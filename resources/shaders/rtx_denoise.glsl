// Paimon RTX - filtrado del trazado.
//
// Bilateral 3x3 guiado por la luminancia de la escena (para no sangrar luz a
// traves de los bordes) mas realimentacion temporal. El historial se recorta al
// rango de los vecinos de este fotograma: sin eso, el desplazamiento lateral del
// nivel deja estelas.
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

varying vec2 v_texCoord;

uniform sampler2D u_current;
uniform sampler2D u_history;
uniform sampler2D u_scene;
uniform vec2  u_texel;
uniform float u_denoise;
uniform float u_temporal;
uniform float u_clamp;

float luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec2 uv = v_texCoord;
    float centerL = luma(texture2D(u_scene, uv).rgb);

    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    vec4 lo = vec4(1e9);
    vec4 hi = vec4(-1e9);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 o = vec2(float(x), float(y)) * u_texel * u_denoise;
            vec4 s = texture2D(u_current, uv + o);
            float l = luma(texture2D(u_scene, uv + o).rgb);
            float w = exp(-abs(l - centerL) * 12.0);
            sum += s * w;
            wsum += w;
            lo = min(lo, s);
            hi = max(hi, s);
        }
    }

    vec4 spatial = sum / max(wsum, 0.0001);

    vec4 hist = texture2D(u_history, uv);
    if (u_clamp > 0.5) hist = clamp(hist, lo, hi);

    gl_FragColor = mix(spatial, hist, u_temporal);
}
