#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aCor;
uniform mat4 uProjecao;
out vec2 vUV;
out vec4 vCor;
void main() {
    vUV = aUV;
    vCor = aCor;
    gl_Position = uProjecao * vec4(aPos, 0.0, 1.0);
}
