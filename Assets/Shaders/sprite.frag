#version 330 core
in vec2 vUV;
in vec4 vCor;
uniform sampler2D uTextura;
out vec4 corFinal;
void main() {
    vec4 c = texture(uTextura, vUV) * vCor;
    if (c.a < 0.004) discard;
    corFinal = c;
}
