// Paimon RTX - filtro a-trous del trazado (SVGF sin la estimacion de varianza).
//
// Kernel B3-spline 3x3 aplicado varias veces con el paso doblandose en cada
// pasada: tres pasadas con paso 1, 2 y 4 cubren 15x15 pixeles con 27 muestras en
// vez de 225. El corte por luminancia de la escena impide que la luz cruce los
// bordes de los objetos, que es lo que convierte un desenfoque cualquiera en un
// reductor de ruido.
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

varying vec2 v_texCoord;

uniform sampler2D u_src;
uniform sampler2D u_guide;
uniform vec2  u_texel;
uniform float u_stride;
uniform float u_phi;

float luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float bspline(int i) {
    return i == 0 ? 0.5 : 0.25;
}

void main() {
    vec2 uv = v_texCoord;
    float center = luma(texture2D(u_guide, uv).rgb);

    vec4 sum = vec4(0.0);
    float wsum = 0.0;

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 o = vec2(float(x), float(y)) * u_texel * u_stride;
            float guide = luma(texture2D(u_guide, uv + o).rgb);
            float w = bspline(x) * bspline(y) * exp(-abs(guide - center) * u_phi);
            sum += texture2D(u_src, uv + o) * w;
            wsum += w;
        }
    }

    gl_FragColor = sum / max(wsum, 0.0001);
}
