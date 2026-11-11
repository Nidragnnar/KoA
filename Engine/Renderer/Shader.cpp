#include "Engine/Renderer/Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Logger.h"
#include "Engine/Platform/FileSystem.h"

namespace ke {

namespace {

unsigned compilarEstagio(GLenum tipo, const std::string& fonte, const std::string& nome) {
    const unsigned s = glCreateShader(tipo);
    const char* p = fonte.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char msg[1024];
        glGetShaderInfoLog(s, sizeof msg, nullptr, msg);
        log::erro("shader {} ({}): {}", nome, tipo == GL_VERTEX_SHADER ? "vértice" : "fragmento", msg);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

}

Shader::~Shader() {
    if (programa_) glDeleteProgram(programa_);
}

bool Shader::compilar(const std::string& vert, const std::string& frag, const std::string& nome) {
    const unsigned v = compilarEstagio(GL_VERTEX_SHADER, vert, nome);
    const unsigned f = compilarEstagio(GL_FRAGMENT_SHADER, frag, nome);
    if (!v || !f) return false;
    programa_ = glCreateProgram();
    glAttachShader(programa_, v);
    glAttachShader(programa_, f);
    glLinkProgram(programa_);
    glDeleteShader(v);
    glDeleteShader(f);
    int ok = 0;
    glGetProgramiv(programa_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char msg[1024];
        glGetProgramInfoLog(programa_, sizeof msg, nullptr, msg);
        log::erro("shader {} (ligação): {}", nome, msg);
        return false;
    }
    return true;
}

bool Shader::carregar(const std::filesystem::path& vert, const std::filesystem::path& frag) {
    const auto v = fs::lerTexto(vert);
    const auto f = fs::lerTexto(frag);
    if (!v || !f) {
        log::erro("shader não encontrado: {} / {}", vert.string(), frag.string());
        return false;
    }
    return compilar(*v, *f, vert.stem().string());
}

void Shader::usar() const { glUseProgram(programa_); }
void Shader::definir(const char* n, int v) const { glUniform1i(glGetUniformLocation(programa_, n), v); }
void Shader::definir(const char* n, float v) const { glUniform1f(glGetUniformLocation(programa_, n), v); }
void Shader::definir(const char* n, glm::vec2 v) const { glUniform2fv(glGetUniformLocation(programa_, n), 1, glm::value_ptr(v)); }
void Shader::definir(const char* n, glm::vec3 v) const { glUniform3fv(glGetUniformLocation(programa_, n), 1, glm::value_ptr(v)); }
void Shader::definir(const char* n, glm::vec4 v) const { glUniform4fv(glGetUniformLocation(programa_, n), 1, glm::value_ptr(v)); }
void Shader::definir(const char* n, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(programa_, n), 1, GL_FALSE, glm::value_ptr(m));
}

}
