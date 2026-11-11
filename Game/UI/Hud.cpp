#include "Game/UI/Hud.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "Game/App/Contexto.h"

namespace koa {

namespace {

// Recursos sempre na mesma ordem na interface
const char* ORDEM_RECURSOS[] = {"madeira", "pedra", "ouro"};

// Quebra o texto em linhas que caibam em `largura` pixels
std::vector<std::string> quebrarLinhas(const ke::BitmapFont& f, const std::string& texto, int largura) {
    std::vector<std::string> linhas;
    std::string atual, palavra;
    auto fechar = [&] {
        if (palavra.empty()) return;
        const std::string teste = atual.empty() ? palavra : atual + " " + palavra;
        if (!atual.empty() && f.largura(teste) > largura) {
            linhas.push_back(atual);
            atual = palavra;
        } else {
            atual = teste;
        }
        palavra.clear();
    };
    for (char c : texto) {
        if (c == ' ') fechar();
        else palavra += c;
    }
    fechar();
    if (!atual.empty()) linhas.push_back(atual);
    return linhas;
}

}

void Hud::atualizar(float dt) {
    for (auto& a : avisos_) a.vida -= dt;
    std::erase_if(avisos_, [](const Aviso& a) { return a.vida <= 0.0f; });
    for (auto& [k, v] : pulso_) v = std::max(0.0f, v - dt);
    tempoDica_ = std::max(0.0f, tempoDica_ - dt);
}

void Hud::aviso(std::string texto, glm::vec4 c) {
    avisos_.push_back({std::move(texto), c, 3.0f});
    if (avisos_.size() > 4) avisos_.erase(avisos_.begin());
}

void Hud::desenhar(ke::Renderer2D& r, const World& w, float tempo) {
    // Recursos (canto superior esquerdo)
    desenharPainel(r, {6, 6}, {104, 46});
    for (int i = 0; i < 3; ++i) {
        const std::string id = ORDEM_RECURSOS[i];
        const float y = 11.0f + i * 13.0f;
        const float p = pulso_.contains(id) ? pulso_[id] : 0.0f;
        const auto it = dados_.recursos.find(id);
        r.sprite(rec_.regiao(it != dados_.recursos.end() ? it->second.icone : "icone_" + id), {12, y - 1 - std::round(p * 5)});
        f_.desenhar(r, dados_.nomeRecurso(id), {25, y}, cor::apagado);
        const std::string n = std::to_string(w.inventario.quantidade(id));
        f_.desenhar(r, n, {104.0f - f_.largura(n), y}, p > 0 ? cor::ouro : cor::texto);
    }

    // Lugar e era (canto superior direito)
    const std::string era = nomeEra(w.reino.eraAtual());
    const float l = static_cast<float>(std::max(f_.largura(era), f_.largura("Vilarejo de Valdren"))) + 12.0f;
    desenharPainel(r, {634 - l, 6}, {l, 30});
    f_.desenhar(r, "Vilarejo de Valdren", {634 - l + 6, 11}, cor::texto);
    f_.desenhar(r, era, {634 - l + 6, 22}, cor::ouro);

    // Avisos (centro, em cima)
    for (size_t i = 0; i < avisos_.size(); ++i) {
        const Aviso& a = avisos_[i];
        glm::vec4 c = a.cor;
        c.a = std::min(1.0f, a.vida * 2.0f);
        const float y = 44.0f + i * 12.0f;
        const float lt = static_cast<float>(f_.largura(a.texto));
        r.retangulo({320 - lt / 2 - 6, y - 2}, {lt + 12, 12}, {0.05f, 0.04f, 0.04f, 0.7f * c.a});
        f_.desenharCentralizado(r, a.texto, {320, y + 4}, c);
    }

    // Dica de controles nos primeiros segundos
    if (tempoDica_ > 0.0f) {
        const float a = std::min(1.0f, tempoDica_);
        const std::string dica = "WASD/setas: andar   E: interagir   Esc: menu   F5: salvar";
        f_.desenharCentralizado(r, dica, {320, 348}, {0.85f, 0.82f, 0.75f, a});
    }
    (void)tempo;
}

void Hud::painelConstrucao(ke::Renderer2D& r, const World& w, const std::string& id, float tempo) {
    const BuildingData* b = w.reino.construcao(id);
    const BuildingDef* d = b ? w.def(*b) : nullptr;
    if (!b || !d) return;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const NivelConstrucao* atual = d->nivel(b->level);
    const ResultadoMelhoria m = avaliarMelhoria(*b, *d, w.inventario);
    const int larguraTexto = 300;
    const auto descAtual = quebrarLinhas(f_, atual ? atual->descricao : "", larguraTexto);
    const auto descProx = quebrarLinhas(f_, m.proximo ? m.proximo->descricao : "", larguraTexto);
    // A altura do painel acompanha o texto
    const float alt = 96.0f + (descAtual.size() + descProx.size()) * 11.0f + (m.proximo ? m.custo.size() * 12.0f + 18.0f : 0.0f);
    const glm::vec2 s{328, alt}, p{320 - s.x / 2, std::floor(180 - alt / 2)};
    desenharPainel(r, p, s);
    f_.desenharCentralizado(r, b->name, {320, p.y + 12}, cor::ouro, true, 1);
    f_.desenharCentralizado(r, std::format("Nível {} de {}", b->level, d->nivelMax()), {320, p.y + 24}, cor::apagado);
    float y = p.y + 38;
    for (const auto& l : descAtual) f_.desenharCentralizado(r, l, {320, y}, cor::texto), y += 11;
    r.retangulo({p.x + 12, y}, {s.x - 24, 1}, cor::borda);
    y += 12;

    if (!m.proximo) {
        f_.desenharCentralizado(r, "A construção chegou ao nível máximo.", {320, y + 4}, cor::texto);
        f_.desenharCentralizado(r, "Esc  Fechar", {320, p.y + s.y - 13}, cor::apagado);
        return;
    }
    f_.desenharCentralizado(r, "Próximo: " + m.proximo->nome, {320, y}, cor::texto);
    y += 12;
    for (const auto& l : descProx) f_.desenharCentralizado(r, l, {320, y}, cor::apagado), y += 11;
    y += 8;
    std::vector<std::string> ids;
    for (const char* recurso : ORDEM_RECURSOS)
        if (m.custo.contains(recurso)) ids.push_back(recurso);
    for (const auto& [rid, q] : m.custo)
        if (std::ranges::find(ids, rid) == ids.end()) ids.push_back(rid);
    for (const auto& rid : ids) {
        const int q = m.custo.at(rid);
        const int tem = w.inventario.quantidade(rid);
        const bool ok = tem >= q;
        const auto it = dados_.recursos.find(rid);
        r.sprite(rec_.regiao(it != dados_.recursos.end() ? it->second.icone : "icone_" + rid), {320 - 64, y - 5});
        f_.desenhar(r, dados_.nomeRecurso(rid), {320 - 50, y - 4}, cor::texto);
        const std::string n = std::format("{}/{}", tem, q);
        f_.desenhar(r, n, {320.0f + 64 - f_.largura(n), y - 4}, ok ? cor::bom : cor::ruim);
        y += 12;
    }
    const bool pode = m.possivel;
    const float brilho = pode ? 0.8f + 0.2f * std::sin(tempo * 4.0f) : 1.0f;
    const glm::vec4 corBotao = pode ? glm::vec4(cor::ouro.r * brilho, cor::ouro.g * brilho, cor::ouro.b * brilho, 1) : cor::apagado;
    f_.desenharCentralizado(r, pode ? "E  Reconstruir" : "Faltam recursos", {320, p.y + s.y - 26}, corBotao);
    f_.desenharCentralizado(r, "Esc  Fechar", {320, p.y + s.y - 13}, cor::apagado);
}

void Hud::menuPausa(ke::Renderer2D& r, const std::vector<std::string>& opcoes, int sel) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.55f});
    const float alt = 40.0f + opcoes.size() * 16.0f;
    const glm::vec2 p{240, 180 - alt / 2};
    desenharPainel(r, p, {160, alt});
    f_.desenharCentralizado(r, "Pausa", {320, p.y + 12}, cor::ouro);
    for (size_t i = 0; i < opcoes.size(); ++i) {
        const bool s = static_cast<int>(i) == sel;
        const float y = p.y + 32 + i * 16.0f;
        if (s) r.retangulo({p.x + 10, y - 5}, {140, 12}, {0.45f, 0.36f, 0.22f, 0.45f});
        f_.desenharCentralizado(r, opcoes[i], {320, y}, s ? cor::ouro : cor::texto);
    }
}

}
