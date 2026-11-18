#include "Game/Kingdom/Povo.h"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace koa {

namespace {

EfeitosPovo lerEfeitos(const nlohmann::json& j) {
    EfeitosPovo e;
    e.madeiraDia = j.value("madeira_dia", 0.0f);
    e.pedraDia = j.value("pedra_dia", 0.0f);
    e.comidaDia = j.value("comida_dia", 0.0f);
    e.ouroDia = j.value("ouro_dia", 0.0f);
    e.seguranca = j.value("seguranca", 0.0f);
    e.felicidade = j.value("felicidade", 0.0f);
    return e;
}

template <class T>
const T* acharPorId(const std::vector<T>& v, const std::string& id) {
    const auto it = std::ranges::find(v, id, &T::id);
    return it == v.end() ? nullptr : &*it;
}

}

EfeitosPovo& EfeitosPovo::operator+=(const EfeitosPovo& o) {
    madeiraDia += o.madeiraDia;
    pedraDia += o.pedraDia;
    comidaDia += o.comidaDia;
    ouroDia += o.ouroDia;
    seguranca += o.seguranca;
    felicidade += o.felicidade;
    return *this;
}

const ProfissaoDef* DadosPovo::profissao(const std::string& id) const { return acharPorId(profissoes, id); }
const TracoDef* DadosPovo::traco(const std::string& id) const { return acharPorId(tracos, id); }
const DesejoDef* DadosPovo::desejo(const std::string& id) const { return acharPorId(desejos, id); }

uint32_t misturar(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    return h ^ (h >> 15);
}

std::string sobrenomeSorteado(const DadosPovo& d, uint32_t sorte) {
    return d.sobrenomes.empty() ? std::string("do Vale") : d.sobrenomes[misturar(sorte, 11u) % d.sobrenomes.size()];
}

Habitante gerarHabitante(const DadosPovo& d, int id, const std::string& familia, int dia, uint32_t sorte, const std::set<std::string>& obrasProntas) {
    Habitante h;
    h.id = id;
    h.familia = familia;
    h.chegou = dia;
    h.nome = d.nomes.empty() ? std::string("Alguém") : d.nomes[misturar(sorte, 1u) % d.nomes.size()];
    // Profissão: só as que a vila já comporta, sorteadas pelo peso
    int total = 0;
    for (const ProfissaoDef& p : d.profissoes)
        if (p.requer.empty() || obrasProntas.contains(p.requer)) total += std::max(0, p.peso);
    if (total > 0) {
        int alvo = static_cast<int>(misturar(sorte, 2u) % static_cast<uint32_t>(total));
        for (const ProfissaoDef& p : d.profissoes) {
            if (!p.requer.empty() && !obrasProntas.contains(p.requer)) continue;
            alvo -= std::max(0, p.peso);
            if (alvo < 0) {
                h.profissao = p.id;
                break;
            }
        }
    }
    if (!d.tracos.empty()) h.traco = d.tracos[misturar(sorte, 3u) % d.tracos.size()].id;
    if (!d.desejos.empty()) h.desejo = d.desejos[misturar(sorte, 4u) % d.desejos.size()].id;
    h.variante = static_cast<int>(misturar(sorte, 5u) % 4u);
    return h;
}

EfeitosPovo efeitosDoPovo(const DadosPovo& d, const std::vector<Habitante>& povo) {
    EfeitosPovo total;
    int cumpridos = 0;
    for (const Habitante& h : povo) {
        cumpridos += h.cumprido;
        const ProfissaoDef* p = d.profissao(h.profissao);
        if (!p) continue;
        const TracoDef* t = d.traco(h.traco);
        const float k = t ? t->rende : 1.0f;
        EfeitosPovo e = p->efeitos;
        e.madeiraDia *= k, e.pedraDia *= k, e.comidaDia *= k, e.ouroDia *= k, e.seguranca *= k;  // a felicidade não depende de quanto se trabalha
        total += e;
    }
    total.felicidade += std::min(5.0f, cumpridos * 0.2f);
    return total;
}

bool lerPovo(const nlohmann::json& j, DadosPovo& d, std::string* erro) {
    try {
        d.nomes = j.at("nomes").get<std::vector<std::string>>();
        d.sobrenomes = j.at("sobrenomes").get<std::vector<std::string>>();
        for (const auto& jp : j.at("profissoes"))
            d.profissoes.push_back({jp.at("id").get<std::string>(), jp.value("nome", jp.at("id").get<std::string>()), jp.value("peso", 1),
                                    jp.value("requer", ""), lerEfeitos(jp.value("efeitos", nlohmann::json::object()))});
        for (const auto& jt : j.at("tracos"))
            d.tracos.push_back({jt.at("id").get<std::string>(), jt.value("nome", jt.at("id").get<std::string>()), jt.value("rende", 1.0f),
                                jt.value("paciencia", 10)});
        for (const auto& jd : j.at("desejos")) {
            DesejoDef de;
            de.id = jd.at("id").get<std::string>();
            de.texto = jd.value("texto", de.id);
            de.fala = jd.value("fala", "");
            const auto& c = jd.at("condicao");
            de.condicao = {c.at("tipo").get<std::string>(), c.value("alvo", ""), c.value("valor", 0.0f), c.value("nivel", 1)};
            d.desejos.push_back(std::move(de));
        }
        if (d.nomes.empty() || d.sobrenomes.empty() || d.profissoes.empty() || d.tracos.empty() || d.desejos.empty()) {
            if (erro) *erro = "povo.json: faltam nomes, sobrenomes, profissões, traços ou desejos";
            return false;
        }
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("povo.json: ") + e.what();
        return false;
    }
    return true;
}

}
