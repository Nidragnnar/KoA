#include "Game/Kingdom/MundoVivo.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include "Game/Kingdom/Povo.h"

namespace koa {

const AldeiaDef* DadosMundo::aldeia(const std::string& id) const {
    const auto it = std::ranges::find(aldeias, id, &AldeiaDef::id);
    return it == aldeias.end() ? nullptr : &*it;
}

const AjudaDef* DadosMundo::ajuda(const std::string& id) const {
    const auto it = std::ranges::find(ajudas, id, &AjudaDef::id);
    return it == ajudas.end() ? nullptr : &*it;
}

const ViajanteDef* DadosMundo::viajante(const std::string& id) const {
    const auto it = std::ranges::find(viajantes, id, &ViajanteDef::id);
    return it == viajantes.end() ? nullptr : &*it;
}

bool lerMundo(const nlohmann::json& j, DadosMundo& d, std::string* erro) {
    try {
        d.requer = j.value("requer", "");
        d.noticiasPorManha = std::max(1, j.value("noticias_por_manha", d.noticiasPorManha));
        d.cresceAcima = j.value("cresce_acima", d.cresceAcima);
        d.minguaAbaixo = j.value("mingua_abaixo", d.minguaAbaixo);
        d.someAbaixo = j.value("some_abaixo", d.someAbaixo);
        d.abandono = j.value("abandono", d.abandono);
        d.tributo = j.value("tributo", d.tributo);
        d.chanceViajante = j.value("chance_viajante", d.chanceViajante);
        const auto& av = j.value("avanco", nlohmann::json::object());
        d.avancoPressao = av.value("pressao", d.avancoPressao);
        d.avancoAldeia = av.value("aldeia", d.avancoAldeia);
        d.vizinhanca = j.value("vizinhanca", std::map<std::string, std::vector<std::string>>{});
        for (const auto& a : j.at("aldeias")) {
            AldeiaDef x;
            x.id = a.at("id").get<std::string>();
            x.nome = a.value("nome", x.id);
            x.perto = a.value("perto", "");
            x.faccao = a.value("faccao", "");
            x.descricao = a.value("descricao", "");
            x.povo = std::max(1, a.value("povo", x.povo));
            x.base = a.value("base", x.base);
            x.corrupcaoFixa = a.value("corrupcao_fixa", -1.0f);
            const auto& c = a.value("caravana", nlohmann::json::object());
            x.caravana = {c.value("nome", "uma caravana"), std::max(1, c.value("a_cada", 5)), c.value("mercado", std::map<std::string, float>{}),
                          c.value("pedagio", 0)};
            d.aldeias.push_back(std::move(x));
        }
        for (const auto& a : j.at("ajudas"))
            d.ajudas.push_back({a.at("id").get<std::string>(), a.value("texto", ""), a.value("quando", "sorte"), a.value("valor", 0.0f),
                                a.value("ajuda", 10.0f), a.value("limpa", 0.0f), a.value("custo", Custo{}), std::max(1, a.value("prazo", 2))});
        for (const auto& v : j.at("viajantes"))
            d.viajantes.push_back({v.at("id").get<std::string>(), v.value("texto", ""), v.value("requer", ""), v.value("custo", Custo{}),
                                   v.value("felicidade", 0.0f), v.value("generosidade", 0.0f), v.value("soldados", 0),
                                   v.value("corrupcao", std::map<std::string, float>{})});
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("mundo.json: ") + e.what();
        return false;
    }
    return true;
}

float sorteMundo(uint32_t semente, int dia, const std::string& assunto) {
    uint32_t h = 2166136261u;  // FNV-1a do assunto
    for (const char c : assunto) h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
    return static_cast<float>(misturar(misturar(semente, static_cast<uint32_t>(dia) * 40503u), h) % 10000u) / 10000.0f;
}

float alvoProsperidade(const AldeiaDef& a, float corrupcao) { return std::clamp(a.base - std::max(0.0f, corrupcao - 50.0f) * 0.6f, 0.0f, 100.0f); }

void noiteDaAldeia(const DadosMundo& d, EstadoAldeia& e, const AldeiaDef& a, float corrupcao, float sorte) {
    if (e.sumiu) return;
    e.prosperidade = std::clamp(e.prosperidade + (alvoProsperidade(a, corrupcao) - e.prosperidade) * 0.2f, 0.0f, 100.0f);
    // Gente chega quando a vida é boa (um pouco por noite); vai embora quando falta tudo
    if (e.prosperidade >= d.cresceAcima && sorte < 0.35f) ++e.povo;
    if (e.prosperidade < d.minguaAbaixo && sorte < 0.5f) e.povo = std::max(0, e.povo - 1 - e.povo / 15);
}

const AjudaDef* ajudaNecessaria(const DadosMundo& d, const EstadoAldeia& e, float corrupcao, float sorte) {
    if (e.sumiu) return nullptr;
    for (const AjudaDef& a : d.ajudas) {
        if (a.quando == "prosperidade_abaixo" && e.prosperidade < a.valor && sorte < 0.6f) return &a;
        if (a.quando == "corrupcao_acima" && corrupcao >= a.valor && sorte < 0.4f) return &a;
        if (a.quando == "sorte" && sorte < a.valor / 100.0f) return &a;
    }
    return nullptr;
}

float riscoCaravana(float corrupcaoRota, int segurancaReino) {
    return std::clamp(corrupcaoRota / 100.0f * 0.5f - segurancaReino / 300.0f, 0.05f, 0.6f);
}

std::vector<std::string> ordenarNoticias(std::vector<Noticia> n) {
    std::ranges::stable_sort(n, std::greater{}, &Noticia::prioridade);
    std::vector<std::string> v;
    for (auto& x : n) v.push_back(std::move(x.texto));
    return v;
}

}
