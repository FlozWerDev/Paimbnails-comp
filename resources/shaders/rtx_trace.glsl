// Paimon RTX - trazado en espacio de pantalla.
//
// El unico dato de entrada es el color ya rasterizado, asi que la superficie se
// reconstruye a partir de el: la luminancia mas la saturacion hacen de campo de
// altura, sus derivadas dan la normal, y el brillo por encima del umbral marca
// que pixeles emiten luz. Sobre esa altura se marchan rayos: cada uno avanza en
// pantalla hasta que la altura del pixel muestreado supera la del rayo, y ese
// impacto aporta rebote de color, oclusion y reflejo.
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
const float kGolden   = 2.39996323;
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

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
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

    // El jitter por pixel y por fotograma rota el abanico de rayos: con pocos
    // rayos el ruido resultante lo limpia el pase temporal en vez de costar GPU.
    float jitter = hash12(gl_FragCoord.xy + u_frame * 17.13);
    float stepLen = u_rayDistance / max(u_raySteps, 1.0);

    vec3  gi   = vec3(0.0);
    float occ  = 0.0;
    float rays = 0.0;

    for (int i = 0; i < kMaxRays; i++) {
        if (float(i) >= u_rayCount) break;
        rays += 1.0;

        float a = (float(i) + jitter) * kGolden;
        vec2 dir = vec2(cos(a), sin(a));
        dir = normalize(mix(dir, normalize(n.xy + dir * 0.35 + vec2(0.0001)), 0.55));

        float t = stepLen * (0.5 + jitter * 0.5);

        for (int s = 0; s < kMaxSteps; s++) {
            if (float(s) >= u_raySteps) break;

            vec2 p = uv + dir * t;
            if (outside(p)) break;

            vec3 c = texture2D(u_scene, p).rgb;
            float rayHeight = h0 + n.z * t * u_thickness;

            if (heightOf(c) > rayHeight) {
                float fall = exp(-t * u_bounceFalloff / max(u_rayDistance, 0.001));
                vec3 tinted = mix(vec3(luma(c)), c, u_giSaturation);
                gi  += tinted * emissiveOf(c) * fall;
                occ += 1.0 - smoothstep(0.0, u_aoRadius, t);
                break;
            }

            t += stepLen;
        }
    }

    float inv = 1.0 / max(rays, 1.0);
    gi  *= inv;
    occ *= inv;
    float ao = 1.0 - pow(clamp(occ, 0.0, 1.0), max(u_aoPower, 0.05));

    vec3 refl = vec3(0.0);
    if (u_reflectStrength > 0.001) {
        vec2 rd = reflect(vec3(0.0, 0.0, -1.0), n).xy;
        if (length(rd) > 0.0001) {
            float ra = hash12(gl_FragCoord.yx + u_frame * 7.71) * kTau;
            rd = normalize(mix(normalize(rd), vec2(cos(ra), sin(ra)), u_reflectRoughness * 0.5));

            float rt = stepLen * (0.5 + jitter * 0.5);
            for (int s = 0; s < kMaxSteps; s++) {
                if (float(s) >= u_raySteps) break;

                vec2 p = uv + rd * rt;
                if (outside(p)) break;

                vec3 c = texture2D(u_scene, p).rgb;
                if (heightOf(c) > h0 + n.z * rt * u_thickness) {
                    refl = c * (1.0 - smoothstep(0.0, max(u_reflectFade, 0.01), rt));
                    break;
                }

                rt += stepLen;
            }

            float fres = mix(1.0, pow(1.0 - clamp(n.z, 0.0, 1.0), 3.0), u_reflectFresnel);
            refl *= fres * u_reflectStrength;
        }
    }

    gl_FragColor = vec4(gi + refl, ao);
}
