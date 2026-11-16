#include "Game/Kingdom/Cotidiano.h"

#include <format>

#include <nlohmann/json.hpp>

namespace koa {

namespace {

Estacao estacaoDe(const std::string& s) {
    if (s == "verao") return Estacao::Verao;
    if (s == "outono") return Estacao::Outono;
    if (s == "inverno") return Estacao::Inverno;
    if (s == "primavera") return Estacao::Primavera;
    throw std::runtime_error("estação desconhecida: " + s);
}

std::set<Estacao> estacoes(const nlohmann::json& j) {
    std::set<Estacao> v;
    for (const auto& s : j) v.insert(estacaoDe(s.get<std::string>()));
    return v;
}

}

const CulturaDef* DadosCotidiano::cultura(const std::string& id) const {
    for (const auto& c : culturas)
        if (c.id == id) return &c;
    return nullptr;
}

const ReceitaDef* DadosCotidiano::receita(const std::string& id) const {
    for (const auto& r : receitas)
        if (r.id == id) return &r;
    return nullptr;
}

const PeixeDef* sortearPeixe(const DadosCotidiano& d, const std::string& regiao, Estacao e, const std::vector<float>& sortes) {
    size_t k = 0;
    for (const auto& p : d.peixes) {
        if (!p.regioes.contains(regiao) || (!p.estacoes.empty() && !p.estacoes.contains(e))) continue;
        const float sorte = k < sortes.size() ? sortes[k] : 0.0f;
        ++k;
        if (sorte < p.chance) return &p;
    }
    return nullptr;
}

std::string textoPrato(const EfeitosPrato& e) {
    std::string t;
    auto junta = [&](const std::string& s) { t += (t.empty() ? "" : ", ") + s; };
    if (e.vida) junta(std::format("+{} de vida", e.vida));
    if (e.dano) junta(std::format("+{} de dano até dormir", e.dano));
    if (e.defesa) junta(std::format("+{} de defesa até dormir", e.defesa));
    if (e.felicidade != 0.0f) junta(std::format("+{:.0f} de felicidade para o povo", e.felicidade));
    return t.empty() ? "nada" : t;
}

bool lerCotidiano(const nlohmann::json& j, DadosCotidiano& d, std::string* erro) {
    try {
        for (const auto& t : j.at("canteiros")) d.canteiros.push_back({t.at(0).get<int>(), t.at(1).get<int>()});
        for (const auto& jc : j.at("culturas")) {
            CulturaDef c;
            c.id = jc.at("id").get<std::string>();
            c.nome = jc.value("nome", c.id);
            c.semente = jc.at("semente").get<std::string>();
            c.dias = std::max(1, jc.value("dias", 2));
            c.rendimento = std::max(1, jc.value("rendimento", 2));
            c.estacoes = estacoes(jc.at("estacoes"));
            d.culturas.push_back(std::move(c));
        }
        const auto& jp = j.at("pesca");
        d.esperaMin = jp.value("espera_min", 1.5f);
        d.esperaMax = std::max(d.esperaMin, jp.value("espera_max", 4.0f));
        d.janela = jp.value("janela", 0.8f);
        for (const auto& jx : jp.at("peixes")) {
            PeixeDef p;
            p.id = jx.at("id").get<std::string>();
            p.chance = jx.value("chance", 1.0f);
            for (const auto& r : jx.at("regioes")) p.regioes.insert(r.get<std::string>());
            if (jx.contains("estacoes")) p.estacoes = estacoes(jx["estacoes"]);
            d.peixes.push_back(std::move(p));
        }
        const nlohmann::json animais = j.value("animais", nlohmann::json::object());  // variável: .items() de temporário quebra
        for (const auto& [construcao, ja] : animais.items()) d.animais[construcao] = {ja.at("animal").get<std::string>(), ja.at("produto").get<std::string>()};
        for (const auto& jr : j.at("receitas")) {
            ReceitaDef r;
            r.id = jr.at("id").get<std::string>();
            r.nome = jr.value("nome", r.id);
            r.descricao = jr.value("descricao", "");
            const nlohmann::json ing = jr.at("ingredientes");
            for (const auto& [item, q] : ing.items()) r.ingredientes[item] = q.get<int>();
            const nlohmann::json ef = jr.value("efeitos", nlohmann::json::object());
            r.efeitos = {ef.value("vida", 0), ef.value("dano", 0), ef.value("defesa", 0), ef.value("felicidade", 0.0f)};
            d.receitas.push_back(std::move(r));
        }
        const nlohmann::json presentes = j.value("presentes", nlohmann::json::object());
        for (const auto& [npc, lista] : presentes.items())
            if (lista.is_array()) d.presentes[npc] = lista.get<std::vector<std::string>>();
        const nlohmann::json itens = j.value("itens", nlohmann::json::object());
        for (const auto& [id, nome] : itens.items())
            if (nome.is_string() && id != "_") d.itens[id] = nome.get<std::string>();
        if (d.canteiros.empty() || d.culturas.empty() || d.receitas.empty()) throw std::runtime_error("vida cotidiana incompleta");
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("Data/World/vida.json: ") + ex.what();
        return false;
    }
    return true;
}

}
