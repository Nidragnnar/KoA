#include "Game/Save/SaveSystem.h"

#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"

namespace koa {

using nlohmann::json;

bool SaveSystem::existe(const std::string& slot) const {
    std::error_code ec;
    return std::filesystem::exists(pasta_ / slot / "metadata.json", ec);
}

bool SaveSystem::salvar(const DadosSave& d, const std::string& slot, std::string* erro) {
    const auto dir = pasta_ / slot;
    std::error_code ec;
    // Backup do save anterior antes de sobrescrever
    if (std::filesystem::exists(dir / "metadata.json", ec)) {
        auto bak = dir;
        bak += ".bak";
        ke::fs::copiarPasta(dir, bak);
    }

    json meta = {{"versao_formato", VERSAO_SAVE}, {"versao_jogo", d.versaoJogo}, {"data_hora", d.dataHora}, {"tempo_jogado", d.tempoJogado}};
    json player = {{"x", d.posJogador.x}, {"y", d.posJogador.y}, {"direcao", d.direcao}, {"regiao", d.regiao}, {"vida", d.vida},
                   {"arma", d.arma}, {"armadura", d.armadura}, {"equipamentos", d.equipamentos}};
    json jMissoes = json::object();
    for (const auto& [id, m] : d.missoes) jMissoes[id] = {{"concluida", m.first}, {"base", m.second}};
    json quests = {{"missoes", jMissoes}, {"abates", d.abates}, {"visitadas", d.visitadas}, {"marcos", d.marcos}};
    json inv = d.inventario;
    json obras = json::array();
    for (const auto& c : d.construcoes) {
        json o = {{"id", c.id}, {"def", c.def}, {"nivel", c.nivel}, {"construida", c.construida}};
        if (c.temPosicao) o["tile"] = {c.tile.x, c.tile.y};
        obras.push_back(o);
    }
    json reino = {{"construcoes", obras},
                  {"era_visual", d.eraVisual},
                  {"povo", {{"populacao", d.populacao}, {"felicidade", d.felicidade}, {"imposto", d.imposto}}}};
    json objs = json::array();
    for (const auto& o : d.objetos)
        objs.push_back({{"id", o.id}, {"hp", o.hp}, {"esgotado", o.esgotado}, {"aberto", o.aberto}, {"dia_esgotado", o.diaEsgotado}});
    json mundo = {{"relogio", {{"dia", d.dia}, {"minutos", d.minutos}}}, {"objetos", objs}};
    json npcs = json::array(), relacoes = json::object();
    for (const auto& m : d.moradores) {
        npcs.push_back({{"id", m.id}, {"conheceu", m.conheceu}, {"memoria", m.memoria}});
        relacoes[m.id] = {{"afinidade", m.afinidade}, {"falou_hoje", m.falouHoje}};
    }
    json jNpcs = {{"moradores", npcs}};
    json jRel = {{"afinidades", relacoes}};
    json paginas = json::array();
    for (const auto& c : d.cronicas) paginas.push_back({{"marco", c.marco}, {"dia", c.dia}, {"era", c.era}, {"texto", c.texto}});
    json cronicas = {{"cronicas", paginas}};
    json jRegioes = json::object();
    for (const auto& [id, e] : d.regioes)
        jRegioes[id] = {{"corrupcao", e.corrupcao}, {"seguranca", e.seguranca}, {"prosperidade", e.prosperidade}, {"ultima_visita", e.ultimaVisita}};
    json regioes = {{"regioes", jRegioes}};
    json conselho = {{"cargos", d.conselho}, {"lealdade", d.lealdade}, {"leis", d.leis}, {"decisao_pendente", d.decisaoPendente},
                     {"ultima_decisao", d.ultimaDecisao}};
    json defesa = {{"horda_dia", d.hordaDia}, {"horda_numero", d.hordaNumero}, {"proxima_horda", d.proximaHorda},
                   {"portao_vida", d.portaoVida}, {"notas", d.notasDefesa}};

    const std::pair<const char*, const json*> arquivos[] = {
        {"player.json", &player}, {"inventory.json", &inv}, {"kingdom.json", &reino}, {"world.json", &mundo},
        {"npcs.json", &jNpcs}, {"relationships.json", &jRel}, {"quests.json", &quests}, {"chronicles.json", &cronicas},
        {"regions.json", &regioes}, {"council.json", &conselho}, {"defense.json", &defesa},
        {"metadata.json", &meta}};  // metadata por último: só existe save "válido" quando tudo foi gravado
    for (const auto& [nome, conteudo] : arquivos) {
        if (!ke::fs::gravarTextoSeguro(dir / nome, conteudo->dump(2))) {
            if (erro) *erro = std::string("não consegui gravar ") + nome;
            return false;
        }
    }
    return true;
}

json SaveSystem::migrar(json tudo, int deVersao) {
    // Cada passo leva o save uma versão adiante
    if (deVersao < 2) {
        // v1 não tinha relógio nem moradores: começa no dia 1, às 6:00, sem conhecer ninguém
        tudo["world"]["relogio"] = {{"dia", 1}, {"minutos", 360.0}};
        tudo["npcs"] = {{"moradores", json::array()}};
        tudo["relationships"] = {{"afinidades", json::object()}};
        deVersao = 2;
    }
    if (deVersao < 3) {
        // v2 guardava só {"construção": nível} das construções fixas do mapa
        json lista = json::array();
        const json antigas = tudo["kingdom"].value("construcoes", json::object());  // variável: .items() de temporário quebra
        for (const auto& [id, nivel] : antigas.items())
            lista.push_back({{"id", id}, {"nivel", nivel}, {"construida", true}});
        tudo["kingdom"]["construcoes"] = lista;
        deVersao = 3;
    }
    if (deVersao < 4) {
        // v3 não tinha regiões, combate nem missões: começa no vilarejo, sem missões
        tudo["quests"] = {{"missoes", json::object()}, {"abates", json::object()}, {"visitadas", json::array({"vilarejo"})},
                          {"marcos", json::array()}};
        deVersao = 4;
    }
    if (deVersao < 5) {
        // v4 não tinha Crônicas: o jogo as reconstrói a partir do que já aconteceu (marcos, missões, obras)
        tudo["chronicles"] = {{"cronicas", json::array()}, {"reconstruir", true}};
        deVersao = 5;
    }
    if (deVersao < 6) {
        // v5 não tinha regiões vivas: cada região começa como no mapa (o jogo usa os valores iniciais)
        tudo["regions"] = {{"regioes", json::object()}, {"padrao", true}};
        deVersao = 6;
    }
    if (deVersao < 7) {
        // v6 não tinha conselho: começa sem conselheiros, sem leis e sem caso pendente
        tudo["council"] = {{"cargos", json::object()}, {"lealdade", json::object()}, {"leis", json::array()}};
        deVersao = 7;
    }
    if (deVersao < 8) {
        // v7 não tinha hordas: nenhuma avisada, e o portão (se já existisse) começa inteiro
        tudo["defense"] = {{"padrao", true}};
        deVersao = 8;
    }
    tudo["metadata"]["versao_formato"] = deVersao;
    return tudo;
}

std::optional<DadosSave> SaveSystem::lerPasta(const std::filesystem::path& dir, std::string* erro) const {
    json tudo;
    for (const char* nome : {"metadata", "player", "inventory", "kingdom", "world"}) {
        const auto texto = ke::fs::lerTexto(dir / (std::string(nome) + ".json"));
        if (!texto) {
            if (erro) *erro = std::string("arquivo ausente: ") + nome + ".json";
            return std::nullopt;
        }
        tudo[nome] = json::parse(*texto, nullptr, false);
        if (tudo[nome].is_discarded()) {
            if (erro) *erro = std::string("arquivo corrompido: ") + nome + ".json";
            return std::nullopt;
        }
    }
    try {
        const int versao = tudo["metadata"].value("versao_formato", 0);
        if (versao <= 0 || versao > VERSAO_SAVE) {
            if (erro) *erro = "versão de save desconhecida: " + std::to_string(versao);
            return std::nullopt;
        }
        // Arquivos que só existem a partir da v2
        if (versao >= 2) {
            for (const char* nome : {"npcs", "relationships", "quests", "chronicles", "regions", "council", "defense"}) {
                if (std::string(nome) == "defense" && versao < 8) continue;
                if (std::string(nome) == "quests" && versao < 4) continue;
                if (std::string(nome) == "chronicles" && versao < 5) continue;
                if (std::string(nome) == "regions" && versao < 6) continue;
                if (std::string(nome) == "council" && versao < 7) continue;
                const auto texto = ke::fs::lerTexto(dir / (std::string(nome) + ".json"));
                tudo[nome] = texto ? json::parse(*texto, nullptr, false) : json();
                if (tudo[nome].is_discarded() || !tudo[nome].is_object()) {
                    if (erro) *erro = std::string("arquivo ausente ou corrompido: ") + nome + ".json";
                    return std::nullopt;
                }
            }
        }
        if (versao < VERSAO_SAVE) tudo = migrar(std::move(tudo), versao);

        DadosSave d;
        const json& m = tudo["metadata"];
        d.versao = VERSAO_SAVE;
        d.versaoJogo = m.value("versao_jogo", "");
        d.dataHora = m.value("data_hora", "");
        d.tempoJogado = std::max(0.0, m.value("tempo_jogado", 0.0));
        const json& p = tudo["player"];
        d.posJogador = {p.value("x", 0.0f), p.value("y", 0.0f)};
        d.direcao = std::clamp(p.value("direcao", 0), 0, 3);
        d.regiao = p.value("regiao", std::string("vilarejo"));
        d.vida = p.value("vida", 100);
        d.arma = p.value("arma", d.arma);
        d.armadura = p.value("armadura", d.armadura);
        if (p.contains("equipamentos")) d.equipamentos = p["equipamentos"].get<std::vector<std::string>>();
        const json& jq = tudo["quests"];
        const json qMissoes = jq.value("missoes", json::object());  // variável: .items() de temporário quebra
        for (const auto& [id, jmi] : qMissoes.items()) d.missoes[id] = {jmi.value("concluida", false), jmi.value("base", 0)};
        if (jq.contains("abates"))
            for (const auto& [id, n] : jq["abates"].items()) d.abates[id] = std::max(0, n.get<int>());
        d.visitadas = jq.value("visitadas", std::vector<std::string>{"vilarejo"});
        d.marcos = jq.value("marcos", std::vector<std::string>{});
        const json& jc = tudo["chronicles"];
        d.temCronicas = !jc.value("reconstruir", false);
        for (const auto& c : jc.value("cronicas", json::array()))
            d.cronicas.push_back({c.value("marco", ""), std::max(0, c.value("dia", 0)), std::clamp(c.value("era", 0), 0, 3), c.value("texto", "")});
        const json& jreg = tudo["regions"];
        d.temRegioes = !jreg.value("padrao", false);
        const json regioes = jreg.value("regioes", json::object());  // variável: .items() de temporário quebra
        for (const auto& [id, je] : regioes.items()) {
            EstadoRegiao e;
            e.corrupcao = std::clamp(je.value("corrupcao", 70.0f), 0.0f, 100.0f);
            e.seguranca = std::clamp(je.value("seguranca", 0.0f), 0.0f, 100.0f);
            e.prosperidade = std::clamp(je.value("prosperidade", 0.0f), 0.0f, 100.0f);
            e.ultimaVisita = std::max(1, je.value("ultima_visita", 1));
            d.regioes[id] = e;
        }
        const json& jcons = tudo["council"];
        const json cargos = jcons.value("cargos", json::object());  // variável: .items() de temporário quebra
        for (const auto& [cargo, npc] : cargos.items())
            if (npc.is_string() && !npc.get<std::string>().empty()) d.conselho[cargo] = npc.get<std::string>();
        const json leal = jcons.value("lealdade", json::object());
        for (const auto& [npc, l] : leal.items())
            if (l.is_number_integer()) d.lealdade[npc] = std::clamp(l.get<int>(), 0, 100);
        d.leis = jcons.value("leis", std::vector<std::string>{});
        d.decisaoPendente = jcons.value("decisao_pendente", "");
        d.ultimaDecisao = std::max(0, jcons.value("ultima_decisao", 0));
        const json& jdef = tudo["defense"];
        d.temDefesa = !jdef.value("padrao", false);
        d.hordaDia = std::max(0, jdef.value("horda_dia", 0));
        d.hordaNumero = std::max(0, jdef.value("horda_numero", 0));
        d.proximaHorda = std::max(0, jdef.value("proxima_horda", 0));
        d.portaoVida = std::max(0, jdef.value("portao_vida", 0));
        d.notasDefesa = jdef.value("notas", std::vector<std::string>{});
        for (const auto& [id, q] : tudo["inventory"].items())
            if (q.is_number_integer()) d.inventario[id] = std::max(0, q.get<int>());
        const json& r = tudo["kingdom"];
        for (const auto& o : r.value("construcoes", json::array())) {
            EstadoConstrucao c;
            c.id = o.value("id", "");
            c.def = o.value("def", "");
            c.nivel = o.value("nivel", 1);
            c.construida = o.value("construida", true);
            if (o.contains("tile")) {
                c.tile = {o["tile"][0].get<int>(), o["tile"][1].get<int>()};
                c.temPosicao = true;
            }
            if (!c.id.empty()) d.construcoes.push_back(std::move(c));
        }
        d.eraVisual = std::clamp(r.value("era_visual", 0.0f), 0.0f, 3.0f);
        if (r.contains("povo")) {
            d.temPovo = true;
            d.populacao = std::max(1, r["povo"].value("populacao", 8));
            d.felicidade = std::clamp(r["povo"].value("felicidade", 40.0f), 0.0f, 100.0f);
            d.imposto = std::clamp(r["povo"].value("imposto", 1), 0, 2);
        }
        for (const auto& o : tudo["world"].value("objetos", json::array()))
            d.objetos.push_back({o.value("id", ""), o.value("hp", 0), o.value("esgotado", false), o.value("aberto", false), o.value("dia_esgotado", 0)});
        const json& rel = tudo["world"].value("relogio", json::object());
        d.dia = std::max(1, rel.value("dia", 1));
        d.minutos = std::clamp(rel.value("minutos", 360.0f), 360.0f, 26.0f * 60.0f - 1.0f);
        const json& afin = tudo["relationships"].value("afinidades", json::object());
        for (const auto& jm : tudo["npcs"].value("moradores", json::array())) {
            EstadoMorador e;
            e.id = jm.value("id", "");
            e.conheceu = jm.value("conheceu", false);
            e.memoria = jm.value("memoria", std::vector<std::string>{});
            if (afin.contains(e.id)) {
                e.afinidade = std::clamp(afin[e.id].value("afinidade", 0), 0, 1000);
                e.falouHoje = afin[e.id].value("falou_hoje", false);
            }
            if (!e.id.empty()) d.moradores.push_back(std::move(e));
        }
        return d;
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("save inválido: ") + ex.what();
        return std::nullopt;
    }
}

std::optional<DadosSave> SaveSystem::carregar(const std::string& slot, std::string* erro, std::string* aviso) {
    std::string e1;
    if (auto d = lerPasta(pasta_ / slot, &e1)) return d;
    auto bak = pasta_ / slot;
    bak += ".bak";
    std::string e2;
    if (auto d = lerPasta(bak, &e2)) {
        if (aviso) *aviso = "Save principal com problema (" + e1 + "); carreguei o backup.";
        return d;
    }
    if (erro) *erro = e1;
    return std::nullopt;
}

}
