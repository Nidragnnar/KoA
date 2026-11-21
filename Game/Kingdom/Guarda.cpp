#include "Game/Kingdom/Guarda.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace koa {

const TipoEsquadra* DadosGuarda::tipo(const std::string& id) const {
    const auto it = std::ranges::find(tipos, id, &TipoEsquadra::id);
    return it == tipos.end() ? nullptr : &*it;
}

bool lerGuarda(const nlohmann::json& j, DadosGuarda& d, std::string* erro) {
    try {
        d.requerAla = j.value("requer_ala", d.requerAla);
        d.esquadrasMax = std::clamp(j.value("esquadras_max", d.esquadrasMax), 1, 3);  // o limite da auditoria: nada de exército
        d.soldadosMax = std::max(1, j.value("soldados_max", d.soldadosMax));
        d.nivelMax = std::max(0, j.value("nivel_max", d.nivelMax));
        d.reforco = j.value("reforco", Custo{});
        d.treino = j.value("treino", Custo{});
        d.limpeza = j.value("limpeza", d.limpeza);
        d.seguranca = j.value("seguranca", d.seguranca);
        d.ameaca = j.value("ameaca", d.ameaca);
        d.chanceLuta = j.value("chance_luta", d.chanceLuta);
        d.escoltaCaravana = j.value("escolta_caravana", d.escoltaCaravana);
        d.protecaoProsperidade = j.value("protecao_prosperidade", d.protecaoProsperidade);
        d.nomes = j.value("nomes", std::vector<std::string>{});
        for (const auto& t : j.at("tipos"))
            d.tipos.push_back({t.at("id").get<std::string>(), t.value("nome", ""), t.value("descricao", ""), t.value("forca", 1.5f), t.value("regiao", 1.0f),
                               t.value("escolta", 1.0f), t.value("custo", Custo{}), t.value("soldo", std::map<std::string, float>{}), t.value("requer_forja", 0)});
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("guarda.json: ") + e.what();
        return false;
    }
    if (d.tipos.empty() || d.nomes.empty()) {
        if (erro) *erro = "guarda.json: sem tipos ou sem nomes";
        return false;
    }
    return true;
}

float forcaEsquadra(const DadosGuarda& d, const Esquadra& e, const BonusGuarda& b) {
    const TipoEsquadra* t = d.tipo(e.tipo);
    if (!t || e.soldados <= 0) return 0.0f;
    float f = e.soldados * t->forca * (1.0f + 0.25f * e.nivel);
    f *= b.nivelForja >= 3 ? 1.4f : b.nivelForja >= 2 ? 1.2f : 1.0f;  // armas de ferro da forja
    f *= 1.0f + b.marechal;
    if (b.heroiPresente) f *= 1.5f;
    if (e.posto.starts_with("regiao:")) f *= t->regiao;
    if (e.posto == "escolta") f *= t->escolta;
    return f;
}

Custo soldoDaNoite(const DadosGuarda& d, const std::vector<Esquadra>& esquadras) {
    std::map<std::string, float> total;
    for (const Esquadra& e : esquadras)
        if (const TipoEsquadra* t = d.tipo(e.tipo))
            for (const auto& [r, v] : t->soldo) total[r] += v * e.soldados;
    Custo c;
    for (const auto& [r, v] : total)
        if (const int q = static_cast<int>(std::ceil(v - 1e-4f)); q > 0) c[r] = q;
    return c;
}

ResultadoLuta lutaNaRegiao(const DadosGuarda& d, float forca, float corrupcao) {
    ResultadoLuta r;
    r.forca = forca;
    r.ameaca = corrupcao * d.ameaca;
    r.vitoria = forca >= r.ameaca;
    // Vencendo, perde no máximo um; perdendo, um soldado a cada 5 pontos que faltaram (pelo menos um)
    r.perdas = r.vitoria ? (r.ameaca > forca * 0.8f ? 1 : 0) : std::max(1, static_cast<int>(std::ceil((r.ameaca - forca) / 5.0f)));
    return r;
}

}
