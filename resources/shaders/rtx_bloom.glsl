// Paimon RTX - cadena de bloom, rayos volumetricos y adaptacion de luz.
//
// Un solo programa con cinco modos para no compilar cinco: 0 prepara la fuente
// (lleva la escena a rango alto, le suma la luz trazada y recorta por umbral),
// 1 baja de resolucion, 2 sube y mezcla con el nivel fino, 3 marcha desde el
// foco para los god rays y 4 mide el brillo medio de la pantalla.
//
// Los dos filtros de la piramide son los de Jimenez (Call of Duty: Advanced
// Warfare, SIGGRAPH 2014): 13 muestras al bajar y carpa de 9 al subir. Un box
// de 4 como el que habia aqui deja el bloom latiendo, porque cada nivel se come
// tres cuartas partes de los pixeles y basta que un brillo cruce medio texel
// para que aparezca y desaparezca entre fotogramas.
//
// El primer nivel promedia ademas por Karis (peso 1/(1+brillo) por grupo de
// cuatro): sin eso, un solo pixel muy brillante domina el promedio y sale
// parpadeando como una luciernaga por toda la cadena.
//
// Al subir no se acumula, se interpola. Sumar hacia arriba multiplica la energia
// por el numero de niveles, asi que la misma "fuerza" daba un halo cinco veces
// mas fuerte con cinco pases que con uno; con la mezcla el peso total siempre
// vale 1 y el numero de pases solo cambia la anchura.

varying vec2 v_texCoord;

uniform sampler2D u_src;
uniform sampler2D u_add;
uniform vec2  u_texel;
uniform float u_mode;
uniform float u_threshold;
uniform float u_softKnee;
uniform float u_radius;
uniform float u_blend;
uniform float u_anamorphic;
uniform vec2  u_lightPos;
uniform float u_decay;
uniform float u_density;
uniform float u_tonemap;
uniform float u_hdrRange;
uniform float u_giMix;
uniform float u_adaptRate;

const int kRaySamples = 24;

// prefilter: la fuente es el back buffer en sRGB y hay que expandirla; en el
// resto de niveles ya viene en luz lineal y se lee tal cual.
vec3 tap(vec2 uv, float prefilter) {
    vec3 c = texture2D(u_src, uv).rgb;
    if (prefilter > 0.5) c = min(tonemapInverse(toLinear(c), u_tonemap), vec3(u_hdrRange));
    return c;
}

vec3 down13(vec2 uv, vec2 t, float prefilter) {
    vec3 a = tap(uv + vec2(-2.0,  2.0) * t, prefilter);
    vec3 b = tap(uv + vec2( 0.0,  2.0) * t, prefilter);
    vec3 c = tap(uv + vec2( 2.0,  2.0) * t, prefilter);
    vec3 d = tap(uv + vec2(-2.0,  0.0) * t, prefilter);
    vec3 e = tap(uv, prefilter);
    vec3 f = tap(uv + vec2( 2.0,  0.0) * t, prefilter);
    vec3 g = tap(uv + vec2(-2.0, -2.0) * t, prefilter);
    vec3 h = tap(uv + vec2( 0.0, -2.0) * t, prefilter);
    vec3 i = tap(uv + vec2( 2.0, -2.0) * t, prefilter);
    vec3 j = tap(uv + vec2(-1.0,  1.0) * t, prefilter);
    vec3 k = tap(uv + vec2( 1.0,  1.0) * t, prefilter);
    vec3 l = tap(uv + vec2(-1.0, -1.0) * t, prefilter);
    vec3 m = tap(uv + vec2( 1.0, -1.0) * t, prefilter);

    vec3 q0 = (a + b + d + e) * 0.25;
    vec3 q1 = (b + c + e + f) * 0.25;
    vec3 q2 = (d + e + g + h) * 0.25;
    vec3 q3 = (e + f + h + i) * 0.25;
    vec3 q4 = (j + k + l + m) * 0.25;

    float w0 = mix(1.0, 1.0 / (1.0 + luma(q0)), prefilter) * 0.125;
    float w1 = mix(1.0, 1.0 / (1.0 + luma(q1)), prefilter) * 0.125;
    float w2 = mix(1.0, 1.0 / (1.0 + luma(q2)), prefilter) * 0.125;
    float w3 = mix(1.0, 1.0 / (1.0 + luma(q3)), prefilter) * 0.125;
    float w4 = mix(1.0, 1.0 / (1.0 + luma(q4)), prefilter) * 0.500;

    return (q0 * w0 + q1 * w1 + q2 * w2 + q3 * w3 + q4 * w4) / (w0 + w1 + w2 + w3 + w4);
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

// Rodilla suave de Unity: por debajo del umbral no entra nada, por encima entra
// entero y en medio hay una parabola. Con el corte duro que habia, el borde
// entre lo que brilla y lo que no salia como una linea dura, y cualquier cosa
// que oscilase alrededor del umbral aparecia y desaparecia de golpe.
//
// El umbral llega en brillo de pantalla, asi que pasa por la misma expansion
// que la imagen antes de comparar con ella.
vec3 knee(vec3 c) {
    float thr = tonemapInverse(vec3(u_threshold * u_threshold), u_tonemap).r;
    float br = max(max(c.r, c.g), c.b);
    float k = thr * u_softKnee + 0.0001;
    float soft = clamp(br - thr + k, 0.0, 2.0 * k);
    soft = soft * soft / (4.0 * k);
    return c * (max(soft, br - thr) / max(br, 0.0001));
}

void main() {
    vec2 uv = v_texCoord;
    vec3 outColor;

    if (u_mode > 3.5) {
        // Media de toda la pantalla leyendo el mip mas alto de la escena.
        float lum = max(luma(toLinear(texture2D(u_src, vec2(0.5), 14.0).rgb)), 0.0005);
        outColor = vec3(mix(texture2D(u_add, vec2(0.5)).r, lum, u_adaptRate));
    } else if (u_mode > 2.5) {
        // El jitter rompe las bandas concentricas que deja marchar todos los
        // pixeles desde el mismo punto de partida.
        vec2 delta = (uv - u_lightPos) * u_density / float(kRaySamples);
        vec2 p = uv - delta * hash12(gl_FragCoord.xy);
        float illum = 1.0;
        vec3 acc = vec3(0.0);
        for (int i = 0; i < kRaySamples; i++) {
            p -= delta;
            acc += texture2D(u_src, clamp(p, 0.0, 1.0)).rgb * illum;
            illum *= u_decay;
        }
        outColor = acc / float(kRaySamples);
    } else if (u_mode > 1.5) {
        vec2 spread = u_texel * u_radius * vec2(1.0 + u_anamorphic * 3.0, 1.0);
        outColor = mix(texture2D(u_add, uv).rgb, tent9(uv, spread), u_blend);
    } else if (u_mode > 0.5) {
        outColor = down13(uv, u_texel, 0.0);
    } else {
        outColor = knee(down13(uv, u_texel, 1.0) + texture2D(u_add, uv).rgb * u_giMix);
    }

    gl_FragColor = vec4(outColor, 1.0);
}
