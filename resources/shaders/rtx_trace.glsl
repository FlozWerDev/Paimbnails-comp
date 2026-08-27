// Paimon RTX - trazado en espacio de pantalla.
//
// El unico dato de entrada es el color ya rasterizado, asi que la superficie se
// reconstruye a partir de el: la luminancia mas la saturacion hacen de campo de
// altura, sus derivadas dan la normal, y el brillo por encima del umbral marca
// que pixeles emiten luz. Sobre esa altura se marchan rayos: cada uno avanza en
// pantalla hasta que la altura del pixel muestreado supera la del rayo, y ese
// impacto aporta rebote de color, oclusion y reflejo.
//
// Dos decisiones mandan sobre la calidad a igualdad de coste:
//
//  - Las direcciones van estratificadas (el rayo i cubre el sector i de la
//    circunferencia) y solo se rota el abanico entero con ruido. Con rotacion
//    aleatoria por rayo, dos rayos caen a menudo casi juntos y dejan el resto
//    del giro sin muestrear; de ahi salia la varianza.
//  - Esa rotacion es Interleaved Gradient Noise, no un hash. El hash es ruido
//    blanco: cada pixel elige un giro sin relacion con sus vecinos y el
//    resultado es la estatica que se ve en pantalla. IGN reparte los giros de
//    forma ordenada dentro de cada bloque 3x3, asi que el error queda en alta
//    frecuencia y tanto el filtro espacial como el temporal lo eliminan.
//
// Los pasos crecen en progresion geometrica en vez de ser uniformes: los
// primeros caen muy juntos (contacto nitido) y los ultimos se separan (alcance
// largo barato), que es lo mismo que busca un marchado jerarquico por mips.
#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

varying vec2 v_texCoord;

uniform sampler2D u_scene;
uniform vec2  u_texel;
uniform float u_frame;

uniform float u_rayCount;
uniform float u_raySteps;
uniform float u_rayDistance;
uniform float u_stepGrowth;
uniform float u_lightThreshold;
uniform float u_lightRange;
uniform float u_bounceFalloff;
uniform float u_giSaturation;
uniform float u_normalStrength;
uniform float u_thickness;
uniform float u_aoRadius;
uniform float u_aoPower;
uniform float u_reflectStrength;
uniform float u_reflectRoughness;
uniform float u_reflectFresnel;
uniform float u_reflectFade;

const int   kMaxRays  = 16;
const int   kMaxSteps = 32;
const float kTau      = 6.28318531;

float luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float heightOf(vec3 c) {
    float sat = max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
    return luma(c) * 0.78 + sat * 0.22;
}

float heightAt(vec2 uv) {
    return heightOf(texture2D(u_scene, uv).rgb);
}

float emissiveOf(vec3 c) {
    return smoothstep(u_lightThreshold, u_lightThreshold + u_lightRange, luma(c));
}

// Interleaved Gradient Noise (Jimenez), con el desplazamiento por fotograma que
// lo convierte en una secuencia de baja discrepancia en el tiempo.
float ign(vec2 p, float frame) {
    p += 5.588238 * mod(frame, 64.0);
    return fract(52.9829189 * fract(0.06711056 * p.x + 0.00583715 * p.y));
}

bool outside(vec2 p) {
    return p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0;
}

void main() {
    vec2 uv = v_texCoord;

    float h0 = heightAt(uv);
    float hL = heightAt(uv - vec2(u_texel.x, 0.0));
    float hR = heightAt(uv + vec2(u_texel.x, 0.0));
    float hD = heightAt(uv - vec2(0.0, u_texel.y));
    float hU = heightAt(uv + vec2(0.0, u_texel.y));

    vec3 n = normalize(vec3((hL - hR) * u_normalStrength,
                            (hD - hU) * u_normalStrength,
                            1.0));

    float rot = ign(gl_FragCoord.xy, u_frame);

    // t(s) = alcance * (g^s - 1) / (g^n - 1): reparte los n pasos entre 0 y el
    // alcance con espaciado geometrico. Con g -> 1 degenera en s/n, o sea lineal.
    float g = max(u_stepGrowth, 1.0001);
    float invSpan = 1.0 / (pow(g, u_raySteps) - 1.0);
    float gStart = pow(g, rot);

    vec3  gi   = vec3(0.0);
    float occ  = 0.0;
    float wsum = 0.0;

    for (int i = 0; i < kMaxRays; i++) {
        if (float(i) >= u_rayCount) break;

        float a = (float(i) + rot) / u_rayCount * kTau;
        vec2 dir = vec2(cos(a), sin(a));

        // Peso coseno: la luz que llega de frente a la normal cuenta entera y la
        // que llega por detras se apaga, sin deformar el reparto de direcciones.
        float w = max(dot(dir, n.xy) * 0.5 + 0.5, 0.05);
        wsum += w;

        float gPow = gStart;

        for (int s = 0; s < kMaxSteps; s++) {
            if (float(s) >= u_raySteps) break;

            float t = u_rayDistance * (gPow - 1.0) * invSpan;
            gPow *= g;

            vec2 p = uv + dir * t;
            if (outside(p)) break;

            vec3 c = texture2D(u_scene, p).rgb;
            if (heightOf(c) > h0 + n.z * t * u_thickness) {
                float fall = exp(-t * u_bounceFalloff / max(u_rayDistance, 0.001));
                vec3 tinted = mix(vec3(luma(c)), c, u_giSaturation);
                gi  += tinted * emissiveOf(c) * fall * w;
                occ += (1.0 - smoothstep(0.0, u_aoRadius, t)) * w;
                break;
            }
        }
    }

    float inv = 1.0 / max(wsum, 0.0001);
    gi  *= inv;
    occ *= inv;
    float ao = 1.0 - pow(clamp(occ, 0.0, 1.0), max(u_aoPower, 0.05));

    vec3 refl = vec3(0.0);
    if (u_reflectStrength > 0.001) {
        vec2 rd = reflect(vec3(0.0, 0.0, -1.0), n).xy;
        if (length(rd) > 0.0001) {
            float ra = ign(gl_FragCoord.yx, u_frame + 31.0) * kTau;
            rd = normalize(mix(normalize(rd), vec2(cos(ra), sin(ra)), u_reflectRoughness * 0.5));

            // El reflejo se recorre con menos pasos que el rebote difuso: un
            // impacto un poco tarde apenas se nota y se ahorra medio trazado.
            // El reparto geometrico se recalcula para esos pasos, o el rayo se
            // quedaria a una fraccion del alcance en vez de cubrirlo entero.
            float reflSteps = max(4.0, u_raySteps * 0.6);
            float reflSpan = 1.0 / (pow(g, reflSteps) - 1.0);
            float gPow = gStart;
            for (int s = 0; s < kMaxSteps; s++) {
                if (float(s) >= reflSteps) break;

                float t = u_rayDistance * (gPow - 1.0) * reflSpan;
                gPow *= g;

                vec2 p = uv + rd * t;
                if (outside(p)) break;

                vec3 c = texture2D(u_scene, p).rgb;
                if (heightOf(c) > h0 + n.z * t * u_thickness) {
                    refl = c * (1.0 - smoothstep(0.0, max(u_reflectFade, 0.01), t));
                    break;
                }
            }

            float fres = mix(1.0, pow(1.0 - clamp(n.z, 0.0, 1.0), 3.0), u_reflectFresnel);
            refl *= fres * u_reflectStrength;
        }
    }

    gl_FragColor = vec4(gi + refl, ao);
}
