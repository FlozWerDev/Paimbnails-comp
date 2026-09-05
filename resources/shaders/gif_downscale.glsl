// Reduccion a la mitad para el importador de GIF, video e imagen a objetos.
//
// El trazado trabaja sobre una rejilla de 48 a 128 celdas, asi que de un frame
// de 1080p sobran mas de dos millones de pixeles. Encadenando esta pasada se
// baja a la resolucion de trabajo en potencias de dos, que es una media de area
// exacta y no el desenfoque bilineal que saldria de una sola reduccion grande.
//
// El color va ponderado por alfa y devuelto sin premultiplicar: promediando el
// RGB a secas, el negro transparente del borde de un sprite se cuela en el color
// y el dibujo sale con una orla oscura alrededor.

#ifdef GL_ES
precision mediump float;
#endif

varying vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec2 u_texel;

void main() {
    vec2 base = v_texCoord - u_texel * 0.5;
    vec4 s0 = texture2D(u_texture, base);
    vec4 s1 = texture2D(u_texture, base + vec2(u_texel.x, 0.0));
    vec4 s2 = texture2D(u_texture, base + vec2(0.0, u_texel.y));
    vec4 s3 = texture2D(u_texture, base + u_texel);

    float weight = s0.a + s1.a + s2.a + s3.a;
    vec3 color = s0.rgb * s0.a + s1.rgb * s1.a + s2.rgb * s2.a + s3.rgb * s3.a;
    gl_FragColor = vec4(weight > 0.0 ? color / weight : vec3(0.0), weight * 0.25);
}
