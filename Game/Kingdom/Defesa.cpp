#include "Game/Kingdom/Defesa.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace koa {

namespace {

std::vector<glm::ivec2> tiles(const nlohmann::json& j) {
    std::vector<glm::ivec2> v;
    for (const auto& t : j) v.push_back({t.at(0).get<int>(), t.at(1).get<int>()});
    return v;
}

}

bool lerDefesa(const nlohmann::json& j, DadosDefesa& d, std::string* erro) {
    try {
        d.diaMin = j.value("dia_min", 10);
        d.populacaoMin = j.value("populacao_min", 12);
        if (j.contains("intervalo")) d.intervaloMin = j["intervalo"][0].get<int>(), d.intervaloMax = j["intervalo"][1].get<int>();
        d.intervaloMax = std::max(d.intervaloMin, d.intervaloMax);
        d.avisoDias = std::max(1, j.value("aviso_dias", 3));
        d.hora = std::clamp(j.value("hora", 20), 7, 25);
        d.forcaBase = j.value("forca_base", 8);
        d.forcaPorHorda = j.value("forca_por_horda", 3);
        const nlohmann::json custo = j.at("custo");  // variável: .items() de temporário quebra
        for (const auto& [tipo, pontos] : custo.items()) d.custo.push_back({tipo, std::max(1, pontos.get<int>())});
        std::ranges::stable_sort(d.custo, [](const auto& a, const auto& b) { return a.second > b.second; });
        d.entrada = tiles(j.at("entrada"));
        d.postoSoldados = tiles(j.value("posto_soldados", nlohmann::json::array()));
        if (j.contains("praca")) d.praca = {j["praca"][0].get<int>(), j["praca"][1].get<int>()};
        const nlohmann::json saque = j.value("saque", nlohmann::json::object());
        for (const auto& [r, q] : saque.items()) {
            if (r == "felicidade") d.saqueFelicidade = q.get<float>();
            else d.saque[r] = q.get<int>();
        }
        d.ouroPorCriatura = j.value("ouro_por_criatura", 3);
        d.alcanceTorre = j.value("alcance_torre", 150.0f);
        d.recargaTorre = j.value("recarga_torre", 1.1f);
        if (j.contains("soldado")) {
            const auto& s = j["soldado"];
            d.soldado = {s.value("vida", 40), s.value("dano", 6), s.value("velocidade", 44.0f), s.value("recarga", 0.8f), s.value("visao", 170.0f)};
        }
        d.consertoMestreObras = j.value("conserto_mestre_obras", 0.3f);
        if (d.custo.empty() || d.entrada.empty()) throw std::runtime_error("horda sem criaturas ou sem entrada");
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("Data/Kingdom/defesa.json: ") + ex.what();
        return false;
    }
    return true;
}

int forcaHorda(const DadosDefesa& d, int numero, float corrupcaoFloresta) {
    return d.forcaBase + d.forcaPorHorda * std::max(0, numero - 1) + static_cast<int>(corrupcaoFloresta / 10.0f);
}

std::vector<std::string> composicaoHorda(const DadosDefesa& d, int forca) {
    std::vector<std::string> v;
    int resta = forca;
    // Rodízio da mais forte para a mais fraca; quando a mais forte não cabe mais, segue com as outras
    while (resta > 0) {
        bool coube = false;
        for (const auto& [tipo, pontos] : d.custo)
            if (pontos <= resta) {
                v.push_back(tipo);
                resta -= pontos;
                coube = true;
            }
        if (!coube) break;
    }
    return v;
}

ResultadoHorda resolverSemHeroi(int defesa, int forca, int vidaPortao) {
    ResultadoHorda r;
    if (defesa >= forca) {
        r.vitoria = true;
        r.danoPortao = std::min(vidaPortao, forca * 3 / 2);  // segurou, mas apanhou
        return r;
    }
    r.invasores = static_cast<int>(std::ceil((forca - defesa) / 10.0f));
    r.danoPortao = vidaPortao;  // o portão caiu
    return r;
}

}
