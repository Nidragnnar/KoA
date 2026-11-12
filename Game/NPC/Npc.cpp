#include "Game/NPC/Npc.h"

#include <algorithm>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace koa {

int lerHora(const std::string& s) {
    const auto dp = s.find(':');
    if (dp == std::string::npos) throw std::runtime_error("hora inválida: " + s);
    int h = std::stoi(s.substr(0, dp));
    const int m = std::stoi(s.substr(dp + 1));
    if (h < 6) h += 24;  // madrugada pertence ao dia anterior (o dia do jogo vai até 2:00)
    return h * 60 + m;
}

const ItemRotina* NpcDef::rotinaEm(float minuto) const {
    if (rotina.empty()) return nullptr;
    const ItemRotina* atual = &rotina.back();
    for (const auto& r : rotina)
        if (r.minuto <= minuto) atual = &r;
    return atual;
}

namespace {

Periodo periodoDe(const std::string& s) {
    if (s == "manha") return Periodo::Manha;
    if (s == "tarde") return Periodo::Tarde;
    if (s == "noite") return Periodo::Noite;
    throw std::runtime_error("período inválido: " + s);
}

Estacao estacaoDe(const std::string& s) {
    if (s == "primavera") return Estacao::Primavera;
    if (s == "verao") return Estacao::Verao;
    if (s == "outono") return Estacao::Outono;
    if (s == "inverno") return Estacao::Inverno;
    throw std::runtime_error("estação inválida: " + s);
}

}

bool lerNpcs(const nlohmann::json& jn, const nlohmann::json& jd, std::map<std::string, NpcDef>& saida, std::string* erro) {
    try {
        for (const auto& n : jn.at("npcs")) {
            NpcDef d;
            d.id = n.at("id").get<std::string>();
            d.nome = n.value("nome", d.id);
            d.profissao = n.value("profissao", "");
            d.personalidade = n.value("personalidade", "");
            d.casa = n.at("casa").get<std::string>();
            d.gosta = n.value("gosta", std::vector<std::string>{});
            d.odeia = n.value("odeia", std::vector<std::string>{});
            for (const auto& r : n.at("rotina"))
                d.rotina.push_back({lerHora(r.at("hora").get<std::string>()), r.at("local").get<std::string>(), r.value("acao", "")});
            std::ranges::sort(d.rotina, {}, &ItemRotina::minuto);
            saida[d.id] = std::move(d);
        }
        const auto& todos = jd.at("dialogos");
        for (auto& [id, d] : saida) {
            if (!todos.contains(id)) continue;
            for (const auto& e : todos[id]) {
                EntradaFala f;
                f.id = e.value("id", "");
                f.umaVez = e.value("uma_vez", false);
                if (f.umaVez && f.id.empty()) throw std::runtime_error("fala 'uma_vez' sem id em " + id);
                const auto& se = e.value("se", nlohmann::json::object());
                if (se.contains("primeiro_encontro")) f.se.primeiroEncontro = se["primeiro_encontro"].get<bool>();
                if (se.contains("ja_falou_hoje")) f.se.jaFalouHoje = se["ja_falou_hoje"].get<bool>();
                if (se.contains("periodo")) f.se.periodo = periodoDe(se["periodo"].get<std::string>());
                if (se.contains("estacao")) f.se.estacao = estacaoDe(se["estacao"].get<std::string>());
                f.se.casaNivelMin = se.value("casa_nivel_min", 0);
                f.se.afinidadeMin = se.value("afinidade_min", 0);
                f.se.eraMin = se.value("era_min", 0);
                f.falas = e.at("falas").get<std::vector<std::string>>();
                if (f.falas.empty()) throw std::runtime_error("entrada sem falas em " + id);
                d.dialogos.push_back(std::move(f));
            }
        }
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("moradores/diálogos: ") + ex.what();
        return false;
    }
    return true;
}

const EntradaFala* escolherFala(const NpcDef& def, const Relacionamento& rel, const ContextoFala& ctx) {
    for (const auto& e : def.dialogos) {
        const CondicoesFala& c = e.se;
        if (e.umaVez && rel.memoria.contains(e.id)) continue;
        if (c.primeiroEncontro && *c.primeiroEncontro == rel.conheceu) continue;
        if (c.jaFalouHoje && *c.jaFalouHoje != rel.falouHoje) continue;
        if (c.periodo && *c.periodo != ctx.periodo) continue;
        if (c.estacao && *c.estacao != ctx.estacao) continue;
        if (ctx.nivelCasa < c.casaNivelMin) continue;
        if (rel.afinidade < c.afinidadeMin) continue;
        if (ctx.era < c.eraMin) continue;
        return &e;
    }
    return nullptr;
}

}
