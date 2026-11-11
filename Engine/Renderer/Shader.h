#pragma once
#include <filesystem>
#include <string>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace ke {

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool compilar(const std::string& vert, const std::string& frag, const std::string& nome);
    bool carregar(const std::filesystem::path& vert, const std::filesystem::path& frag);
    void usar() const;

    void definir(const char* nome, int v) const;
    void definir(const char* nome, float v) const;
    void definir(const char* nome, glm::vec2 v) const;
    void definir(const char* nome, glm::vec3 v) const;
    void definir(const char* nome, glm::vec4 v) const;
    void definir(const char* nome, const glm::mat4& m) const;

private:
    unsigned programa_ = 0;
};

}
