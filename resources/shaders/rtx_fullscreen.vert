attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 v_texCoord;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    v_texCoord = aTexCoord;
}
