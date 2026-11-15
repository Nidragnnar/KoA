#include "Game/World/Regioes.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace koa {

namespace {

glm::vec3 cor3(const nlohmann::json& j, glm::vec3 padrao) {
    if (!j.is_array() || j.size() < 3) return padrao;
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

std::map<std::string, float> lerProducao(const nlohmann::json& j) {
    std::map<std::string, float> p;
    if (j.is_object())
        for (const auto& [r, v] : j.items()) p[r] = v.get<float>();
    return p;
}

}

void registrarAbate(EstadoRegiao& e, const TransformacaoRegiao* t) {
    const float piso = t ? t->pisoCorrupcao : 0.0f;
    e.corrupcao = std::max(piso, e.corrupcao - regra::CORRUPCAO_POR_ABATE);
    e.seguranca = std::min(100.0f, e.seguranca + regra::SEGURANCA_POR_ABATE);
}

bool abandonada(const EstadoRegiao& e, int dia) { return dia - e.ultimaVisita >= regra::DIAS_ABANDONO; }

float fracaoCriaturas(float corrupcao) { return 0.35f + 0.65f * std::clamp(corrupcao / 50.0f, 0.0f, 1.0f); }

std::map<std::string, int> producaoRegiao(const EstadoRegiao& e, const VidaRegiaoDef& v, const TransformacaoRegiao* t) {
    const auto& taxas = t && !t->producao.empty() ? t->producao : v.producao;
    std::map<std::string, int> p;
    for (const auto& [r, taxa] : taxas)
        if (const int n = static_cast<int>(std::round(e.prosperidade * taxa)); n > 0) p[r] = n;
    return p;
}

ResultadoNoite noiteDaRegiao(EstadoRegiao& e, const VidaRegiaoDef& v, const TransformacaoRegiao* t, int dia) {
    ResultadoNoite r;
    r.producao = producaoRegiao(e, v, t);  // rende o que a terra estava dando hoje
    const float antes = e.corrupcao;
    e.seguranca *= regra::SEGURANCA_FICA;
    float pressao = regra::PRESSAO_NOITE + (abandonada(e, dia) ? regra::PRESSAO_ABANDONO : 0.0f);
    if (t && !t->semInimigos.empty()) pressao *= 0.5f;  // terra purificada resiste melhor
    const float piso = t ? t->pisoCorrupcao : 0.0f;
    e.corrupcao = std::clamp(e.corrupcao + pressao - e.seguranca * regra::LIMPEZA_SEGURANCA, piso, 100.0f);
    const float alvo = (100.0f - e.corrupcao) * (t ? t->prosperidade : v.prosperidade);
    e.prosperidade = std::clamp(e.prosperidade + (alvo - e.prosperidade) * 0.3f, 0.0f, 100.0f);
    r.tomada = antes < regra::INVASAO && e.corrupcao >= regra::INVASAO;
    r.voltaram = t && !t->semInimigos.empty() && antes < t->voltamAcima && e.corrupcao >= t->voltamAcima;
    return r;
}

bool lerVidaRegiao(const nlohmann::json& j, VidaRegiaoDef& v) {
    if (!j.contains("vida")) return false;
    const auto& jv = j["vida"];
    v.ativa = true;
    v.corrupcaoInicial = jv.value("corrupcao_inicial", 70.0f);
    v.prosperidade = jv.value("prosperidade", 0.5f);
    v.producao = lerProducao(jv.value("producao", nlohmann::json::object()));
    v.dica = jv.value("dica", "");
    for (const auto& i : jv.value("invasao", nlohmann::json::array()))
        v.invasao.push_back({i.at("tipo").get<std::string>(), {i.at("tile")[0].get<int>(), i.at("tile")[1].get<int>()}});
    for (const auto& jt : jv.value("transformacoes", nlohmann::json::array())) {
        TransformacaoRegiao t;
        t.marco = jt.at("marco").get<std::string>();
        t.nome = jt.at("nome").get<std::string>();
        t.escuridaoMin = jt.value("escuridao_min", 0.0f);
        t.ambiente = cor3(jt.value("ambiente", nlohmann::json()), glm::vec3(1.0f));
        t.hostil = jt.value("hostil", true);
        t.era = std::clamp(jt.value("era", 0), 0, 3);
        for (const auto& s : jt.value("sem_inimigos", nlohmann::json::array())) t.semInimigos.insert(s.get<std::string>());
        t.voltamAcima = jt.value("voltam_acima", 60.0f);
        t.pisoCorrupcao = jt.value("piso_corrupcao", 0.0f);
        t.felicidade = jt.value("felicidade", 0.0f);
        t.luaSangue = jt.value("lua_sangue", 1.0f);
        t.rendimentoVeio = jt.value("rendimento_veio", 0);
        t.prosperidade = jt.value("prosperidade", 1.0f);
        t.producao = lerProducao(jt.value("producao", nlohmann::json::object()));
        t.dica = jt.value("dica", "");
        for (const auto& je : jt.value("enfeites", nlohmann::json::array())) {
            EnfeiteRegiaoDef en;
            en.sprite = je.at("sprite").get<std::string>();
            en.tile = {je.at("tile")[0].get<int>(), je.at("tile")[1].get<int>()};
            en.solido = je.value("solido", false);
            if (je.contains("luz")) {
                const auto& l = je["luz"];
                en.temLuz = true;
                en.corLuz = {l[0].get<float>(), l[1].get<float>(), l[2].get<float>()};
                en.raioLuz = l[3].get<float>();
            }
            t.enfeites.push_back(std::move(en));
        }
        v.transformacoes.push_back(std::move(t));
    }
    return true;
}

const OpcaoEscolha* EscolhaDef::opcao(const std::string& qual) const {
    for (const auto& o : opcoes)
        if (o.id == qual) return &o;
    return nullptr;
}

bool lerEscolhas(const nlohmann::json& j, std::map<std::string, EscolhaDef>& saida, std::string* erro) {
    try {
        for (const auto& [id, je] : j.at("escolhas").items()) {
            EscolhaDef d;
            d.id = id;
            d.titulo = je.at("titulo").get<std::string>();
            d.inimigo = je.value("inimigo", "");
            d.regiao = je.value("regiao", "");
            d.texto = je.value("texto", std::vector<std::string>{});
            for (const auto& jo : je.at("opcoes")) {
                OpcaoEscolha o;
                o.id = jo.at("id").get<std::string>();
                o.nome = jo.at("nome").get<std::string>();
                o.descricao = jo.value("descricao", "");
                o.requer = jo.value("requer", "");
                o.requerTexto = jo.value("requer_texto", "");
                o.item = jo.value("item", "");
                o.corrupcao = jo.value("corrupcao", -1.0f);
                if (jo.contains("recompensa"))
                    for (const auto& [r, q] : jo["recompensa"].items()) o.recompensa[r] = q.get<int>();
                d.opcoes.push_back(std::move(o));
            }
            if (d.opcoes.empty()) throw std::runtime_error("escolha sem opções: " + id);
            saida[id] = std::move(d);
        }
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("Data/Lore/escolhas.json: ") + ex.what();
        return false;
    }
    return true;
}

}
