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

    // Data, hora e era (canto superior direito)
    const std::string era = nomeEra(w.reino.eraAtual());
    const std::string data = w.relogio.textoData();
    const float l = static_cast<float>(std::max({f_.largura(era), f_.largura(data) + 50})) + 12.0f;
    desenharPainel(r, {634 - l, 6}, {l, 30});
    f_.desenhar(r, data, {634 - l + 6, 11}, cor::texto);
    const std::string hora = w.relogio.textoHora();
    f_.desenhar(r, hora, {628.0f - f_.largura(hora), 11}, w.relogio.escuridao() > 0.5f ? glm::vec4(0.6f, 0.7f, 1.0f, 1) : cor::ouro);
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

void Hud::painelConstrucao(ke::Renderer2D& r, const World& w, const std::string& id, const std::vector<std::string>& opcoes, int sel,
                           float tempo) {
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
    const float alt = 70.0f + (descAtual.size() + descProx.size()) * 11.0f + (m.proximo ? m.custo.size() * 12.0f + 26.0f : 14.0f) +
                      opcoes.size() * 13.0f;
    const glm::vec2 s{328, alt}, p{320 - s.x / 2, std::floor(180 - alt / 2)};
    desenharPainel(r, p, s);
    f_.desenharCentralizado(r, b->name, {320, p.y + 12}, cor::ouro, true, 1);
    f_.desenharCentralizado(r, std::format("Nível {} de {}", b->level, d->nivelMax()), {320, p.y + 24}, cor::apagado);
    float y = p.y + 38;
    for (const auto& l : descAtual) f_.desenharCentralizado(r, l, {320, y}, cor::texto), y += 11;
    r.retangulo({p.x + 12, y}, {s.x - 24, 1}, cor::borda);
    y += 12;

    // Opções (W/S + E), sempre no pé do painel
    auto desenharOpcoes = [&] {
        const float y0 = p.y + s.y - 8.0f - opcoes.size() * 13.0f;
        for (size_t i = 0; i < opcoes.size(); ++i) {
            const bool s1 = static_cast<int>(i) == sel;
            const float yy = y0 + i * 13.0f + 6.0f;
            const bool indisponivel = opcoes[i] == "Reconstruir" && !m.possivel;
            if (s1) r.retangulo({p.x + 90, yy - 6}, {s.x - 180, 12}, {0.45f, 0.36f, 0.22f, 0.45f});
            const float brilho = s1 ? 0.85f + 0.15f * std::sin(tempo * 4.0f) : 1.0f;
            glm::vec4 c = indisponivel ? cor::apagado : s1 ? cor::ouro * glm::vec4(brilho, brilho, brilho, 1) : cor::texto;
            f_.desenharCentralizado(r, indisponivel ? opcoes[i] + " (faltam recursos)" : opcoes[i], {320, yy}, c);
        }
    };
    if (!m.proximo) {
        f_.desenharCentralizado(r, "A construção chegou ao nível máximo.", {320, y + 4}, cor::texto);
        desenharOpcoes();
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
    desenharOpcoes();
}

int Hud::tamanhoUtf8(const std::string& s) { return static_cast<int>(ke::decodificarUtf8(s).size()); }

void Hud::caixaDialogo(ke::Renderer2D& r, const Conversa& c, int linha, int letras, float tempo) {
    const glm::vec2 p{20, 262}, s{600, 90};
    desenharPainel(r, p, s);
    // Retrato: cabeça e ombros do morador, ampliados 4×
    const ke::SpriteRegion& sp = rec_.regiao("npc_" + c.npc + "_baixo_0");
    if (sp.textura) {
        const float vMeio = sp.uv.y + (sp.uv.w - sp.uv.y) * (15.0f / 24.0f);
        r.retangulo(p + glm::vec2(10, 10), {68, 70}, {0.12f, 0.10f, 0.09f, 1});
        r.contorno(p + glm::vec2(10, 10), {68, 70}, cor::borda);
        r.quad(*sp.textura, p + glm::vec2(12, 14), {64, 60}, {sp.uv.x, sp.uv.y, sp.uv.z, vMeio});
    }
    f_.desenhar(r, c.nome, p + glm::vec2(90, 10), cor::ouro);
    f_.desenhar(r, c.profissao, p + glm::vec2(90.0f + f_.largura(c.nome) + 10, 10), cor::apagado);
    // Amizade: 10 corações
    const ke::SpriteRegion& cheio = rec_.regiao("coracao_cheio");
    const ke::SpriteRegion& vazio = rec_.regiao("coracao_vazio");
    for (int i = 0; i < 10; ++i) r.sprite(i < c.coracoes ? cheio : vazio, p + glm::vec2(s.x - 92 + i * 8, 11));
    if (c.ganhouAmizade && linha == 0) f_.desenhar(r, "+ amizade", p + glm::vec2(s.x - 172, 10), cor::bom);

    // Fala atual, aparecendo letra a letra
    if (linha < static_cast<int>(c.falas.size())) {
        const std::u32string tudo = ke::decodificarUtf8(c.falas[linha]);
        // Quebra pelo texto inteiro (as palavras não pulam de linha enquanto aparecem)
        const auto linhasCheias = quebrarLinhas(f_, c.falas[linha], static_cast<int>(s.x) - 110);
        size_t usado = 0;
        float y = p.y + 30;
        const size_t total = std::min<size_t>(static_cast<size_t>(std::max(0, letras)), tudo.size());
        for (const auto& l : linhasCheias) {
            const size_t n = ke::decodificarUtf8(l).size();
            if (usado >= total) break;
            const size_t mostrar = std::min(n, total - usado);
            std::string pedaco;
            size_t k = 0;
            for (char32_t ch : ke::decodificarUtf8(l)) {
                if (k++ >= mostrar) break;
                if (ch < 0x80) pedaco += static_cast<char>(ch);
                else if (ch < 0x800) pedaco += static_cast<char>(0xC0 | (ch >> 6)), pedaco += static_cast<char>(0x80 | (ch & 0x3F));
                else pedaco += static_cast<char>(0xE0 | (ch >> 12)), pedaco += static_cast<char>(0x80 | ((ch >> 6) & 0x3F)),
                     pedaco += static_cast<char>(0x80 | (ch & 0x3F));
            }
            f_.desenhar(r, pedaco, {p.x + 90, y}, cor::texto);
            usado += n + 1;  // +1: o espaço que virou quebra de linha
            y += 11;
        }
        // Indicador "continua" piscando quando a fala terminou
        if (static_cast<size_t>(letras) >= tudo.size() && std::fmod(tempo, 0.8f) < 0.5f) {
            const bool ultima = linha + 1 >= static_cast<int>(c.falas.size());
            f_.desenhar(r, ultima ? "E  fechar" : "E  >", {p.x + s.x - (ultima ? 80.0f : 40.0f), p.y + s.y - 14}, cor::ouro);
        }
    }
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
