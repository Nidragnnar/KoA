#version 330 core
// Pós-processamento da tela interna: a "cor da era" do reino
in vec2 vUV;
uniform sampler2D uTela;
uniform float uSaturacao;
uniform float uContraste;
uniform float uBrilho;
uniform vec3 uTinta;
uniform float uVinheta;
uniform float uTempo;
out vec4 corFinal;
void main() {
    vec3 c = texture(uTela, vUV).rgb;
    float luma = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(luma), c, uSaturacao);
    c = (c - 0.5) * uContraste + 0.5 + uBrilho;
    c *= uTinta;
    vec2 d = vUV - 0.5;
    c *= 1.0 - uVinheta * smoothstep(0.25, 0.75, dot(d, d) * 2.2);
    corFinal = vec4(clamp(c, 0.0, 1.0), 1.0);
}
