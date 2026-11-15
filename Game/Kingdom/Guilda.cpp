#include "Game/Kingdom/Guilda.h"

#include <algorithm>
#include <cmath>
#include <format>

#include <nlohmann/json.hpp>

namespace koa {

const AventureiroDef* DadosGuilda::aventureiro(const std::string& id) const {
    for (const auto& a : aventureiros)
        if (a.id == id) return &a;
    return nullptr;
}

const DestinoDef* DadosGuilda::destino(const std::string& id) const {
    for (const auto& d : destinos)
        if (d.id == id) return &d;
    return nullptr;
}

const ReliquiaDef* DadosGuilda::reliquia(const std::string& id) const {
    for (const auto& r : reliquias)
        if (r.id == id) return &r;
    return nullptr;
}

const char* nomeDesfecho(Desfecho d) {
    switch (d) {
    case Desfecho::Triunfo: return "um triunfo";
    case Desfecho::Sucesso: return "um sucesso";
    case Desfecho::DurasPenas: return "a duras penas";
    case Desfecho::Desastre: return "um desastre";
    }
    return "?";
}

int poderAventureiro(const AventureiroDef& a, int nivel, const DestinoDef& d) {
    return a.forca + std::max(0, nivel - 1) + (a.especialidade == d.tipo ? 1 : 0);
}

const char* chanceEmPalavras(int m) {
    if (m >= 3) return "Boas chances";
    if (m >= 0) return "Arriscado";
    if (m >= -2) return "Muito arriscado";
    return "Suicida";
}

Desfecho desfechoPorMargem(int m) {
    if (m >= 3) return Desfecho::Triunfo;
    if (m >= 0) return Desfecho::Sucesso;
    if (m >= -2) return Desfecho::DurasPenas;
    return Desfecho::Desastre;
}

std::string textoEfeitos(const EfeitosReliquia& e) {
    std::vector<std::string> v;
    auto num = [&](int n, const char* o) {
        if (n) v.push_back(std::format("{:+} de {}", n, o));
    };
    num(static_cast<int>(e.felicidade), "felicidade");
    num(e.seguranca, "segurança");
    num(e.dano, "dano");
    num(e.defesa, "defesa");
    if (e.ouroDia) v.push_back(std::format("{:+} de ouro por dia", e.ouroDia));
    if (e.comida != 0.0f) v.push_back(std::format("{:+}% de comida", static_cast<int>(std::round(e.comida * 100))));
    if (e.luaSangue > 1.0f) v.push_back("Lua de Sangue mais frequente");
    std::string t;
    for (const auto& s : v) t += (t.empty() ? "" : ", ") + s;
    return t.empty() ? "nenhum efeito" : t;
}

int sorteExpedicao(const std::string& destino, int partida) {
    uint32_t h = static_cast<uint32_t>(partida) * 2654435761u;
    for (const char c : destino) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
    h ^= h >> 15;
    return static_cast<int>(h % 5) - 2;
}

namespace {

EfeitosReliquia lerEfeitos(const nlohmann::json& j) {
    EfeitosReliquia e;
    e.felicidade = j.value("felicidade", 0.0f);
    e.comida = j.value("comida", 0.0f);
    e.luaSangue = j.value("lua_sangue", 1.0f);
    e.ouroDia = j.value("ouro_dia", 0);
    e.seguranca = j.value("seguranca", 0);
    e.dano = j.value("dano", 0);
    e.defesa = j.value("defesa", 0);
    return e;
}

}

bool lerGuilda(const nlohmann::json& j, DadosGuilda& d, std::string* erro) {
    try {
        d.felicidadeMin = j.value("felicidade_min", 55.0f);
        d.populacaoMin = j.value("populacao_min", 14);
        d.grupoMax = std::max(1, j.value("grupo_max", 3));
        d.comidaPorDia = j.value("comida_por_dia", 2);
        d.diasFerido = j.value("dias_ferido", std::vector<int>{3, 4, 5});
        if (d.diasFerido.size() < 3) d.diasFerido.resize(3, 4);
        d.chanceVolta = j.value("chance_volta", 0.4f);
        if (j.contains("dias_para_voltar")) d.voltaMin = j["dias_para_voltar"][0].get<int>(), d.voltaMax = j["dias_para_voltar"][1].get<int>();
        d.voltaMax = std::max(d.voltaMin, d.voltaMax);
        for (const auto& ja : j.at("aventureiros")) {
            AventureiroDef a;
            a.id = ja.at("id").get<std::string>();
            a.nome = ja.at("nome").get<std::string>();
            a.descricao = ja.value("descricao", "");
            a.forca = ja.value("forca", 1);
            a.especialidade = ja.value("especialidade", "");
            d.aventureiros.push_back(std::move(a));
        }
        for (const auto& jr : j.at("reliquias")) {
            ReliquiaDef r;
            r.id = jr.at("id").get<std::string>();
            r.nome = jr.at("nome").get<std::string>();
            r.texto = jr.value("texto", "");
            r.amaldicoada = jr.value("amaldicoada", false);
            r.efeitos = lerEfeitos(jr.value("efeitos", nlohmann::json::object()));
            r.maldicao = lerEfeitos(jr.value("maldicao", nlohmann::json::object()));
            d.reliquias.push_back(std::move(r));
        }
        for (const auto& jd : j.at("destinos")) {
            DestinoDef x;
            x.id = jd.at("id").get<std::string>();
            x.nome = jd.at("nome").get<std::string>();
            x.descricao = jd.value("descricao", "");
            x.tipo = jd.value("tipo", "");
            x.requer = jd.value("requer", "");
            x.dias = std::max(1, jd.value("dias", 2));
            x.perigo = std::max(0, jd.value("perigo", 1));
            const nlohmann::json ganhos = jd.value("ganhos", nlohmann::json::object());  // variável: .items() de temporário quebra
            for (const auto& [rec, faixa] : ganhos.items()) x.ganhos[rec] = {faixa.at(0).get<int>(), faixa.at(1).get<int>()};
            const nlohmann::json rel = jd.value("reliquias", nlohmann::json::object());
            for (const auto& [id, chance] : rel.items()) {
                if (!d.reliquia(id)) throw std::runtime_error("destino " + x.id + " cita relíquia inexistente: " + id);
                x.reliquias.push_back({id, chance.get<float>()});
            }
            d.destinos.push_back(std::move(x));
        }
        if (d.aventureiros.empty() || d.destinos.empty()) throw std::runtime_error("guilda sem aventureiros ou sem destinos");
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("Data/Kingdom/guilda.json: ") + ex.what();
        return false;
    }
    return true;
}

}
