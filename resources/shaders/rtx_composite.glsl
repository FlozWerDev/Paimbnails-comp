// Paimon RTX - composicion final sobre el back buffer.
//
// El orden importa mas que las formulas. La escena entra en sRGB, se lleva a luz
// lineal y se expande con la inversa del mapeo de tonos; en ese espacio se suman
// la luz trazada, la oclusion, el bloom y los rayos; luego se comprime con el
// mapeo elegido y se vuelve a sRGB. Recien ahi, sobre valores de pantalla, van
// el balance de color y la lente, que es donde tienen sentido.
//
// Antes esto pasaba ACES sobre una imagen que ya venia mapeada por el juego:
// eso sube los medios tonos y baja el blanco a 0.8, o sea que apagar todos los
// efectos seguia dejando la pantalla gris y lechosa. Con el par expandir/
// comprimir la ida y la vuelta se cancelan y RTX sin efectos no toca nada.
//
// u_mix mezcla contra la imagen intacta, asi que la intensidad global sigue
// siendo un solo desvanecido en vez de tocar cada termino.

varying vec2 v_texCoord;

uniform sampler2D u_scene;
uniform sampler2D u_gi;
uniform sampler2D u_bloom;
uniform sampler2D u_rays;
uniform sampler2D u_adapt;
uniform vec2  u_texel;
uniform float u_time;

uniform float u_mix;
uniform float u_giStrength;
uniform float u_aoStrength;
uniform float u_bloomStrength;
uniform float u_rayStrength;

uniform float u_tonemap;
uniform float u_exposure;
uniform float u_adaptKey;
uniform float u_contrast;
uniform float u_saturation;
uniform float u_temperature;
uniform float u_tint;
uniform float u_gammaV;

uniform float u_ca;
uniform float u_vignette;
uniform float u_grain;
uniform float u_sharpen;

// Nucleo de CAS (AMD): el realce se apaga solo donde el vecindario ya esta cerca
// del blanco o del negro, que es justo donde un unsharp normal deja el cerco
// blanco alrededor de cada borde.
vec3 sharpenCAS(vec2 uv, vec3 c, float amount) {
    vec3 n = texture2D(u_scene, uv + vec2(0.0,  u_texel.y)).rgb;
    vec3 s = texture2D(u_scene, uv - vec2(0.0,  u_texel.y)).rgb;
    vec3 e = texture2D(u_scene, uv + vec2(u_texel.x, 0.0)).rgb;
    vec3 w = texture2D(u_scene, uv - vec2(u_texel.x, 0.0)).rgb;

    vec3 mn = min(min(min(n, s), min(e, w)), c);
    vec3 mx = max(max(max(n, s), max(e, w)), c);
    vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, 0.0001), 0.0, 1.0));
    vec3 k = -amp * amount * 0.2;

    return ((c + (n + s + e + w) * k) / (1.0 + 4.0 * k)) - c;
}

void main() {
    vec2 uv = v_texCoord;
    vec3 original = texture2D(u_scene, uv).rgb;

    vec3 shown = original;
    if (u_ca > 0.0) {
        // La separacion crece con el cuadrado del radio, como en una lente de
        // verdad: el centro queda limpio y solo se abre en las esquinas. Con el
        // desplazamiento lineal de antes toda la pantalla salia con doble borde.
        vec2 d = uv - 0.5;
        vec2 off = d * dot(d, d) * u_ca * 0.24;
        shown.r = texture2D(u_scene, uv + off).r;
        shown.b = texture2D(u_scene, uv - off).b;
    }

    vec3 lin = toLinear(shown);
    vec3 hdr = tonemapInverse(lin, u_tonemap);

    vec4 traced = texture2D(u_gi, uv);

    // La oclusion no toca lo que emite: un cartel encendido no se ensucia porque
    // tenga una pared cerca, y aplicarsela a todo era lo que dejaba la imagen
    // apagada en general en vez de marcar los rincones.
    float aoMask = 1.0 - smoothstep(0.35, 1.0, luma(lin));
    hdr *= mix(1.0, clamp(traced.a, 0.0, 1.0), u_aoStrength * aoMask);
    hdr += traced.rgb * u_giStrength;
    hdr += texture2D(u_bloom, uv).rgb * u_bloomStrength;
    hdr += texture2D(u_rays, uv).rgb * u_rayStrength;

    float ev = exp2(u_exposure);
    if (u_adaptKey > 0.0) {
        ev *= clamp(u_adaptKey / max(texture2D(u_adapt, vec2(0.5)).r, 0.0005), 0.35, 3.0);
    }
    hdr *= ev;

    vec3 col = toDisplay(clamp(tonemapApply(hdr, u_tonemap), 0.0, 1.0));

    col.r *= 1.0 + u_temperature * 0.25;
    col.b *= 1.0 - u_temperature * 0.25;
    col.g *= 1.0 + u_tint * 0.15;

    col = (col - 0.5) * u_contrast + 0.5;
    col = mix(vec3(luma(col)), col, u_saturation);
    col = pow(max(col, 0.0), vec3(1.0 / max(u_gammaV, 0.05)));

    if (u_sharpen > 0.0) col += sharpenCAS(uv, original, u_sharpen);

    if (u_vignette > 0.0) {
        vec2 vd = (uv - 0.5) * 2.0 * vec2(u_texel.y / max(u_texel.x, 0.000001), 1.0);
        col *= 1.0 - (1.0 - smoothstep(1.55, 0.30, length(vd))) * min(u_vignette, 1.6) * 0.55;
    }

    if (u_grain > 0.0) {
        // El grano de pelicula vive en los medios tonos; repartirlo por igual lo
        // hace saltar en los negros, que es donde mas se nota que es ruido.
        float resp = 1.0 - abs(luma(col) * 2.0 - 1.0);
        col += (hash12(gl_FragCoord.xy + fract(u_time) * 977.0) - 0.5) * u_grain * 0.09 * resp;
    }

    col = mix(original, col, u_mix);

    // Ruido triangular de un LSB: sin el, los degradados del bloom y el vineteado
    // se cuantizan a 8 bits y salen a bandas.
    col += (hash12(gl_FragCoord.xy + 0.5) - hash12(gl_FragCoord.yx + 7.3)) * 0.0039;

    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
