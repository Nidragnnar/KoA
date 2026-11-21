#include "Game/Combat/Combate.h"

#include <nlohmann/json.hpp>

namespace koa {

bool lerCombate(const nlohmann::json& ji, const nlohmann::json& je, const nlohmann::json& jm, std::map<std::string, InimigoDef>& inimigos,
                std::map<std::string, EquipDef>& equip, std::vector<std::string>& ordemEquip, std::vector<MissaoDef>& missoes, std::string* erro) {
    try {
        for (const auto& i : ji.at("inimigos")) {
            InimigoDef d;
            d.id = i.at("id").get<std::string>();
            d.nome = i.value("nome", d.id);
            const std::string letra = i.value("letra", "?");
            d.letra = letra.empty() ? '?' : letra[0];
            d.vida = i.value("vida", 5);
            d.dano = i.value("dano", 5);
            d.defesa = i.value("defesa", 0);
            d.velocidade = i.value("velocidade", 30.0f);
            d.visao = i.value("visao", 100.0f);
            if (i.contains("ouro")) d.ouroMin = i["ouro"][0].get<int>(), d.ouroMax = i["ouro"][1].get<int>();
            if (i.contains("tamanho")) d.tamanho = {i["tamanho"][0].get<int>(), i["tamanho"][1].get<int>()};
            d.chefe = i.value("chefe", false);
            d.escolha = i.value("escolha", "");
            d.comportamento = i.value("comportamento", "");
            if (i.contains("tiro")) {
                const auto& jt = i["tiro"];
                d.tiro = {jt.value("dano", d.dano), jt.value("velocidade", 100.0f), jt.value("recarga", 2.5f), jt.value("alcance", 160.0f)};
            }
            d.aparencia = i.value("aparencia", "");
            if (i.contains("tinta")) d.tinta = {i["tinta"][0].get<float>(), i["tinta"][1].get<float>(), i["tinta"][2].get<float>()};
            d.escala = i.value("escala", 1.0f);
            d.fase2 = i.value("fase2", 0.0f);
            d.efeitoFase2 = i.value("efeito_fase2", "");
            d.postura = i.value("postura", 0);
            if (i.contains("golpe")) {
                const auto& jg = i["golpe"];
                d.golpe = {jg.value("dano", d.dano), jg.value("raio", 34.0f), jg.value("preparo", 0.6f), jg.value("recarga", 2.4f)};
            }
            inimigos[d.id] = d;
        }
        for (const auto& e : je.at("equipamentos")) {
            EquipDef d;
            d.id = e.at("id").get<std::string>();
            d.nome = e.value("nome", d.id);
            d.tipo = e.at("tipo").get<std::string>();
            if (d.tipo != "arma" && d.tipo != "armadura") throw std::runtime_error("tipo de equipamento inválido: " + d.tipo);
            d.descricao = e.value("descricao", "");
            d.dano = e.value("dano", 0);
            d.defesa = e.value("defesa", 0);
            d.forja = e.value("forja", 0);
            d.efeito = e.value("efeito", "");
            if (e.contains("custo"))
                for (const auto& [r, q] : e["custo"].items()) d.custo[r] = q.get<int>();
            ordemEquip.push_back(d.id);
            equip[d.id] = d;
        }
        for (const auto& m : jm.at("missoes")) {
            MissaoDef d;
            d.id = m.at("id").get<std::string>();
            d.nome = m.value("nome", d.id);
            d.npc = m.value("npc", "");
            d.faccao = m.value("faccao", "");
            d.relacao = m.value("relacao", 0);
            d.descricao = m.value("descricao", "");
            d.requer = m.value("requer", "");
            const auto& o = m.at("objetivo");
            d.objetivo = {o.at("tipo").get<std::string>(), o.at("alvo").get<std::string>(), o.value("quantidade", 1)};
            if (m.contains("recompensa")) {
                for (const auto& [r, q] : m["recompensa"].items()) {
                    if (r == "amizade") d.amizade = q.get<int>();
                    else if (r == "item") d.item = q.get<std::string>();
                    else d.recompensa[r] = q.get<int>();
                }
            }
            if (!d.item.empty() && !equip.contains(d.item)) throw std::runtime_error("missão " + d.id + " dá item inexistente: " + d.item);
            missoes.push_back(std::move(d));
        }
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("combate/missões: ") + ex.what();
        return false;
    }
    return true;
}

}
