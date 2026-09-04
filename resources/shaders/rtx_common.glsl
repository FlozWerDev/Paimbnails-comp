// Paimon RTX - preambulo comun de la cadena de postproceso.
//
// El C++ antepone este archivo a cada fragment shader de RTX porque GLSL no
// tiene include y las conversiones de espacio tienen que ser identicas en los
// tres pases: si el bloom expande la imagen con una curva y el compuesto la
// comprime con otra, el resultado ya no vuelve a la imagen original y el juego
// se ve lavado aunque no se sume nada.
//
// Todo el trabajo va en luz lineal. El back buffer viene en sRGB, que es una
// senal ya preparada para el monitor: sumar, desenfocar o promediar sobre ella
// mezcla numeros que no son cantidades de luz, y de ahi salen los halos
// lechosos y los grises sucios. La ida y vuelta usa gamma 2.0 (c*c y sqrt) en
// vez de la curva sRGB de verdad: se desvia un pelo de 2.2 pero es un par
// exacto, asi que la vuelta cancela la ida hasta el ultimo bit.
//
// Encima de eso va el truco que usan los inyectores de post sobre juegos LDR
// (MagicHDR, BloomingHDR y compania): antes de anadir luz se aplica la INVERSA
// del mapeo de tonos para volver a un rango alto plausible, y al final se aplica
// el mapeo. Sin eso, pasar ACES sobre una imagen que ya venia mapeada sube los
// medios y baja los blancos a gris - que es exactamente el aspecto raro que
// tenia esto antes. Con el par inverso, RTX sin efectos deja la imagen intacta.
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

// Techo de las inversas: en el blanco puro todas divergen.
const float kHdrCeil = 0.9995;
const float kU2White = 0.72519;

float luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 toLinear(vec3 c) { return c * c; }
vec3 toDisplay(vec3 c) { return sqrt(max(c, 0.0)); }

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 tmReinhard(vec3 c) { return c / (1.0 + c); }
vec3 tmReinhardInv(vec3 c) { return c / (1.0 - min(c, kHdrCeil)); }

vec3 tmAces(vec3 c) {
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}
// Raiz positiva de la cuadratica que define la curva de Narkowicz.
vec3 tmAcesInv(vec3 c) {
    vec3 y = min(c, kHdrCeil);
    return (sqrt(max(-10127.0 * y * y + 13702.0 * y + 9.0, 0.0)) + 59.0 * y - 3.0)
         / (502.0 - 486.0 * y);
}

// Hejl-Burgess lleva la correccion sRGB dentro, asi que su salida se eleva al
// cuadrado para devolverla a lineal como las otras tres.
vec3 tmFilmic(vec3 c) {
    vec3 x = max(vec3(0.0), c - 0.004);
    vec3 s = (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
    return s * s;
}
vec3 tmFilmicInv(vec3 c) {
    vec3 s = min(toDisplay(c), kHdrCeil);
    vec3 a = 6.2 * (s - 1.0);
    vec3 b = 1.7 * s - 0.5;
    vec3 k = 0.06 * s;
    return (-b - sqrt(max(b * b - 4.0 * a * k, 0.0))) / (2.0 * a) + 0.004;
}

vec3 tmU2Curve(vec3 x) {
    return ((x * (0.15 * x + 0.05) + 0.004) / (x * (0.15 * x + 0.50) + 0.06)) - 0.066667;
}
vec3 tmUncharted(vec3 c) { return tmU2Curve(c * 2.0) / kU2White; }
vec3 tmUnchartedInv(vec3 c) {
    vec3 q = min(c, kHdrCeil) * kU2White + 0.066667;
    vec3 a = 0.15 * (q - 1.0);
    vec3 b = 0.50 * q - 0.05;
    vec3 k = 0.06 * q - 0.004;
    return ((-b - sqrt(max(b * b - 4.0 * a * k, 0.0))) / (2.0 * a)) * 0.5;
}

vec3 tonemapApply(vec3 c, float mode) {
    if (mode > 3.5) return tmUncharted(c);
    if (mode > 2.5) return tmFilmic(c);
    if (mode > 1.5) return tmAces(c);
    if (mode > 0.5) return tmReinhard(c);
    return c;
}

vec3 tonemapInverse(vec3 c, float mode) {
    if (mode > 3.5) return tmUnchartedInv(c);
    if (mode > 2.5) return tmFilmicInv(c);
    if (mode > 1.5) return tmAcesInv(c);
    if (mode > 0.5) return tmReinhardInv(c);
    return c;
}
