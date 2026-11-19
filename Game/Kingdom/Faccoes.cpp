#include "Game/Kingdom/Faccoes.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <tuple>

#include <nlohmann/json.hpp>

namespace koa {

float Reputacao::eixo(const std::string& nome) const {
    if (nome == "justica") return justica;
    if (nome == "generosidade") return generosidade;
    if (nome == "devocao") return devocao;
    return 0.0f;
}

float& Reputacao::eixo(const std::string& nome) {
    static float nenhum = 0.0f;
    if (nome == "justica") return justica;
    if (nome == "generosidade") return generosidade;
    if (nome == "devocao") return devocao;
    nenhum = 0.0f;
    return nenhum;
}

EfeitosTratado& EfeitosTratado::operator+=(const EfeitosTratado& o) {
    pedraDia += o.pedraDia;
    ouroDia += o.ouroDia;
    seguranca += o.seguranca;
    soldados += o.soldados;
    expedicao += o.expedicao;
    cristalDia += o.cristalDia;
    dano += o.dano;
    felicidade += o.felicidade;
    forcaHorda += o.forcaHorda;
    return *this;
}

const FaccaoDef* DadosFaccoes::faccao(const std::string& id) const {
    const auto it = std::ranges::find(faccoes, id, &FaccaoDef::id);
    return it == faccoes.end() ? nullptr : &*it;
}

const TratadoDef* DadosFaccoes::tratado(const std::string& id) const {
    const auto it = std::ranges::find(tratados, id, &TratadoDef::id);
    return it == tratados.end() ? nullptr : &*it;
}

std::string DadosFaccoes::faixa(float relacao) const {
    for (const auto& [min, nome] : faixas)
        if (relacao >= static_cast<float>(min)) return nome;
    return faixas.empty() ? "" : faixas.back().second;
}

ReacaoMarco DadosFaccoes::reacaoA(const std::string& marco) const {
    ReacaoMarco r;
    r.marco = marco;
    for (const ReacaoMarco& x : reacoes) {
        const bool casa = x.marco.ends_with('*') ? marco.starts_with(x.marco.substr(0, x.marco.size() - 1)) : marco == x.marco;
        if (!casa) continue;
        for (const auto& [f, v] : x.faccoes) r.faccoes[f] += v;
        r.reputacao.justica += x.reputacao.justica;
        r.reputacao.generosidade += x.reputacao.generosidade;
        r.reputacao.devocao += x.reputacao.devocao;
    }
    return r;
}

std::string textoTratado(const EfeitosTratado& e) {
    std::string t;
    auto junta = [&](const std::string& s) { t += (t.empty() ? "" : ", ") + s; };
    auto sinal = [](int v) { return v > 0 ? std::format("+{}", v) : std::to_string(v); };
    if (e.pedraDia) junta(sinal(e.pedraDia) + " de pedra por dia");
    if (e.ouroDia) junta(sinal(e.ouroDia) + " de ouro por dia");
    if (e.cristalDia) junta(sinal(e.cristalDia) + " de cristal por dia");
    if (e.seguranca) junta(sinal(e.seguranca) + " de segurança");
    if (e.soldados) junta(sinal(e.soldados) + " soldados no portão");
    if (e.expedicao) junta(sinal(e.expedicao) + " de força nas expedições");
    if (e.dano) junta(sinal(e.dano) + " de dano");
    if (e.felicidade != 0.0f) junta(sinal(static_cast<int>(std::round(e.felicidade))) + " de felicidade");
    if (e.forcaHorda != 0.0f) junta(std::format("hordas {}% {}", static_cast<int>(std::round(std::abs(e.forcaHorda) * 100)), e.forcaHorda < 0 ? "mais fracas" : "mais fortes"));
    return t.empty() ? "nada" : t;
}

bool lerFaccoes(const nlohmann::json& j, DadosFaccoes& d, std::string* erro) {
    try {
        for (const auto& f : j.at("faixas")) d.faixas.push_back({f.at("min").get<int>(), f.at("nome").get<std::string>()});
        std::ranges::sort(d.faixas, std::greater{}, &std::pair<int, std::string>::first);
        auto lerReputacao = [](const nlohmann::json& r) {
            Reputacao x;
            x.justica = r.value("justica", 0.0f);
            x.generosidade = r.value("generosidade", 0.0f);
            x.devocao = r.value("devocao", 0.0f);
            return x;
        };
        for (const auto& f : j.at("faccoes"))
            d.faccoes.push_back({f.at("id").get<std::string>(), f.value("nome", ""), f.value("lider", ""), f.value("territorio", ""), f.value("descricao", ""),
                                 lerReputacao(f.value("gosta", nlohmann::json::object())), f.value("inicial", 0.0f)});
        for (const auto& r : j.value("rivais", nlohmann::json::array()))
            d.rivais.emplace_back(r.at("a").get<std::string>(), r.at("b").get<std::string>(), r.value("perda", 20.0f));
        for (const auto& t : j.at("tratados")) {
            TratadoDef td;
            td.id = t.at("id").get<std::string>();
            td.faccao = t.at("faccao").get<std::string>();
            td.nome = t.value("nome", td.id);
            td.descricao = t.value("descricao", "");
            td.requer = t.value("requer", 0);
            td.custo = t.value("custo", Custo{});
            const auto& e = t.value("efeitos", nlohmann::json::object());
            td.efeitos = {e.value("pedra_dia", 0), e.value("ouro_dia", 0), e.value("seguranca", 0), e.value("soldados", 0), e.value("expedicao", 0),
                          e.value("cristal_dia", 0), e.value("dano", 0), e.value("felicidade", 0.0f), e.value("forca_horda", 0.0f)};
            if (!d.faccao(td.faccao)) {
                if (erro) *erro = "faccoes.json: tratado " + td.id + " de facção desconhecida";
                return false;
            }
            d.tratados.push_back(std::move(td));
        }
        for (const auto& r : j.at("reacoes")) {
            ReacaoMarco x;
            x.marco = r.at("marco").get<std::string>();
            x.reputacao = lerReputacao(r);
            for (const auto& f : d.faccoes)
                if (r.contains(f.id)) x.faccoes[f.id] = r[f.id].get<float>();
            d.reacoes.push_back(std::move(x));
        }
        const auto& fama = j.at("fama");
        d.limiteFama = fama.value("limite", 30.0f);
        for (const auto& [eixo, par] : fama.at("marcos").items()) d.marcosFama[eixo] = {par.at(0).get<std::string>(), par.at(1).get<std::string>()};
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("faccoes.json: ") + e.what();
        return false;
    }
    return true;
}

}
