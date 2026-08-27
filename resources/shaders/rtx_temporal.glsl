// Paimon RTX - acumulacion temporal del trazado.
//
// Reproyecta el fotograma anterior con el movimiento real de la camara: la capa
// de objetos del juego solo se traslada y escala, asi que su transformada da la
// correspondencia exacta pixel a pixel y no hace falta un buffer de velocidad.
// Sin esto habria que dejar la realimentacion baja para que no arrastrase, y es
// justo la realimentacion alta lo que quita el ruido del trazado.
//
// El historial se recorta contra media +- k*desviacion del vecindario (variance
// clipping) en vez de contra su minimo y maximo: con entrada ruidosa el rango
// min/max es tan ancho que no recorta nada, y las estelas pasan igual.
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
uniform vec2  u_texel;
uniform float u_temporal;
uniform float u_clampSigma;
uniform vec2  u_reprojNow;
uniform vec2  u_reprojPrev;
uniform float u_reprojScale;

void main() {
    vec2 uv = v_texCoord;
    vec4 current = texture2D(u_current, uv);

    vec4 m1 = vec4(0.0);
    vec4 m2 = vec4(0.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec4 s = texture2D(u_current, uv + vec2(float(x), float(y)) * u_texel);
            m1 += s;
            m2 += s * s;
        }
    }
    m1 /= 9.0;
    m2 /= 9.0;
    vec4 sigma = sqrt(max(m2 - m1 * m1, vec4(0.0)));

    vec2 histUV = (uv - u_reprojNow) * u_reprojScale + u_reprojPrev;
    float valid = (histUV.x < 0.0 || histUV.x > 1.0 || histUV.y < 0.0 || histUV.y > 1.0)
        ? 0.0 : 1.0;

    vec4 hist = texture2D(u_history, clamp(histUV, 0.0, 1.0));
    if (u_clampSigma > 0.0) {
        hist = clamp(hist, m1 - sigma * u_clampSigma, m1 + sigma * u_clampSigma);
    }

    gl_FragColor = mix(current, hist, u_temporal * valid);
}
