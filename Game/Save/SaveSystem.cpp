#include "Game/Save/SaveSystem.h"

#include <chrono>
#include <thread>

#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"

namespace koa {

using nlohmann::json;

bool SaveSystem::existe(const std::string& slot) const {
    std::error_code ec;
    return std::filesystem::exists(pasta_ / slot / "metadata.json", ec);
}

namespace {

// Fase 17: arquivos que só existem a partir de certa versão do save (o resto existe desde a v1)
struct ModuloSave {
    const char* nome;
    int desde;
};
constexpr ModuloSave MODULOS[] = {{"npcs", 2},     {"relationships", 2}, {"quests", 4},  {"chronicles", 5}, {"regions", 6},
                                  {"council", 7},  {"defense", 8},       {"guild", 9},   {"life", 10},      {"castle", 11},
                                  {"people", 13}, {"factions", 14}, {"market", 15}};

std::filesystem::path comSufixo(std::filesystem::path p, const char* sufixo) {
    p += sufixo;
    return p;
}

// Renomear pasta no Windows falha se outro programa (antivírus, indexador) estiver lendo um arquivo: tenta de novo
bool renomearComPaciencia(const std::filesystem::path& de, const std::filesystem::path& para) {
    for (int tentativa = 0; tentativa < 5; ++tentativa) {
        std::error_code ec;
        std::filesystem::rename(de, para, ec);
        if (!ec) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    return false;
}

// Uma gravação interrompida entre as duas trocas deixa só a pasta ".novo" completa: ela vira o save
void terminarTrocaPendente(const std::filesystem::path& dir) {
    std::error_code ec;
    const auto novo = comSufixo(dir, ".novo");
    if (!std::filesystem::exists(dir / "metadata.json", ec) && std::filesystem::exists(novo / "metadata.json", ec)) {
        std::filesystem::remove_all(dir, ec);
        renomearComPaciencia(novo, dir);
    }
}

}

bool SaveSystem::salvar(const DadosSave& d, const std::string& slot, std::string* erro) {
    const auto dir = pasta_ / slot;
    const auto novo = comSufixo(dir, ".novo"), bak = comSufixo(dir, ".bak");
    std::error_code ec;
    terminarTrocaPendente(dir);

    json meta = {{"versao_formato", VERSAO_SAVE}, {"versao_jogo", d.versaoJogo}, {"data_hora", d.dataHora}, {"tempo_jogado", d.tempoJogado},
                 {"semente", d.semente}};
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
    json jAvent = json::object();
    for (const auto& [id, a] : d.aventureiros)
        jAvent[id] = {{"nivel", a.nivel}, {"ferido_ate", a.feridoAte}, {"fora", a.fora}, {"sumido", a.sumido}, {"volta_dia", a.voltaDia}};
    json jReliq = json::array();
    for (const auto& r : d.reliquias)
        jReliq.push_back({{"id", r.id}, {"exposta", r.exposta}, {"conhecida", r.conhecida}, {"purificada", r.purificada}});
    json guilda = {{"aventureiros", jAvent},
                   {"expedicao", {{"destino", d.expedicao.destino}, {"grupo", d.expedicao.grupo}, {"partida", d.expedicao.partida},
                                  {"volta", d.expedicao.volta}}},
                   {"reliquias", jReliq}};
    json jCanteiros = json::array();
    for (const auto& c : d.canteiros)
        jCanteiros.push_back({{"cultura", c.cultura}, {"crescidos", c.crescidos}, {"regado_dia", c.regadoDia}, {"maduro", c.maduro}});
    json vida = {{"canteiros", jCanteiros}, {"buff_dano", d.buffDano}, {"buff_defesa", d.buffDefesa},
                 {"presente_dia", d.presenteDia}, {"carinhos", d.carinhos}};
    const Brasao& b = d.brasao;
    json castelo = {{"alas", d.alas},
                    {"brasao_criado", d.brasaoCriado},
                    {"brasao", {{"forma", b.forma}, {"divisao", b.divisao}, {"cor1", b.cor1}, {"cor2", b.cor2}, {"simbolo", b.simbolo},
                                {"cor_simbolo", b.corSimbolo}, {"lema", b.lema}}},
                    {"acoes", d.ultimaAcaoAla},  // fase 20
                    {"guarda_treinada", d.guardaTreinada},
                    {"emprestimo", {{"volta", d.emprestimoVolta}, {"valor", d.emprestimoValor}, {"dia", d.emprestimoDia}}}};
    json jPovo = json::array();
    for (const Habitante& p : d.povo)
        jPovo.push_back({{"id", p.id}, {"nome", p.nome}, {"familia", p.familia}, {"profissao", p.profissao}, {"traco", p.traco},
                         {"desejo", p.desejo}, {"cumprido", p.cumprido}, {"chegou", p.chegou}, {"esperando", p.esperando}, {"variante", p.variante}});
    json pessoas = {{"habitantes", jPovo}, {"proximo", d.proximoHabitante}};
    json vizinhos = {{"relacoes", d.relacoes}, {"tratados", d.tratados}, {"desvios", d.desvios}, {"suspeito", d.suspeito},
                     {"reputacao", {{"justica", d.reputacao.justica}, {"generosidade", d.reputacao.generosidade}, {"devocao", d.reputacao.devocao}}}};

    json encs = json::array();
    for (const EncomendaFeita& e : d.mercado.encomendas) encs.push_back({{"id", e.id}, {"dia", e.dia}});
    json mercado = {{"pressao", d.mercado.pressao}, {"historico", d.mercado.historico}, {"negociados_hoje", d.mercado.negociadosHoje},
                    {"encomendas", encs}, {"vendido", d.mercado.vendido}, {"comprado", d.mercado.comprado}};

    const std::pair<const char*, const json*> arquivos[] = {
        {"player.json", &player}, {"inventory.json", &inv}, {"kingdom.json", &reino}, {"world.json", &mundo},
        {"npcs.json", &jNpcs}, {"relationships.json", &jRel}, {"quests.json", &quests}, {"chronicles.json", &cronicas},
        {"regions.json", &regioes}, {"council.json", &conselho}, {"defense.json", &defesa}, {"guild.json", &guilda}, {"life.json", &vida},
        {"castle.json", &castelo}, {"people.json", &pessoas}, {"factions.json", &vizinhos}, {"market.json", &mercado},
        {"metadata.json", &meta}};  // metadata por último: só existe save "válido" quando tudo foi gravado
    // Fase 17: o save inteiro vai para uma pasta nova e só então troca de lugar com o antigo (que vira o backup).
    // Uma queda no meio nunca mistura arquivos de dois momentos diferentes
    std::filesystem::remove_all(novo, ec);
    for (const auto& [nome, conteudo] : arquivos) {
        if (!ke::fs::gravarTextoSeguro(novo / nome, conteudo->dump(2))) {
            if (erro) *erro = std::string("não consegui gravar ") + nome;
            return false;
        }
    }
    if (std::filesystem::exists(dir, ec)) {
        std::filesystem::remove_all(bak, ec);
        if (!renomearComPaciencia(dir, bak)) {
            // Sem conseguir mover o antigo: copia por cima, como antes (mais lento, mas não perde nada)
            ke::fs::copiarPasta(dir, bak);
            std::filesystem::remove_all(dir, ec);
        }
    }
    if (!renomearComPaciencia(novo, dir)) {
        if (erro) *erro = "não consegui trocar a pasta do save";
        return false;
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
    if (deVersao < 9) {
        // v8 não tinha guilda: sem aventureiros, sem expedição e sem relíquias
        tudo["guild"] = json::object();
        deVersao = 9;
    }
    if (deVersao < 10) {
        // v9 não tinha vida cotidiana: horta vazia (o jogo dá sementes para começar)
        tudo["life"] = {{"padrao", true}};
        deVersao = 10;
    }
    if (deVersao < 11) {
        // v10 não tinha castelo: nenhuma ala e o brasão antigo da linhagem (o castelo continua nas ruínas)
        tudo["castle"] = {{"padrao", true}};
        deVersao = 11;
    }
    // v11 → v12: só a semente do sorteio entrou (no metadata; sem ela, o jogo usa a de sempre)
    if (deVersao < 12) deVersao = 12;
    if (deVersao < 13) {
        // v12 não tinha o povo com nome: o jogo dá nome aos habitantes ao carregar
        tudo["people"] = {{"padrao", true}};
        deVersao = 13;
    }
    if (deVersao < 14) {
        // v13 não tinha os vizinhos: as relações e a fama são refeitas pelo que já aconteceu
        tudo["factions"] = {{"padrao", true}};
        deVersao = 14;
    }
    if (deVersao < 15) {
        // v14 não tinha o mercado vivo: começa sem pressão nem histórico
        tudo["market"] = {{"padrao", true}};
        deVersao = 15;
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
        // Os arquivos de cada módulo, a partir da versão em que ele apareceu
        for (const ModuloSave& mod : MODULOS) {
            if (versao < mod.desde) continue;
            const auto texto = ke::fs::lerTexto(dir / (std::string(mod.nome) + ".json"));
            tudo[mod.nome] = texto ? json::parse(*texto, nullptr, false) : json();
            if (tudo[mod.nome].is_discarded() || !tudo[mod.nome].is_object()) {
                if (erro) *erro = std::string("arquivo ausente ou corrompido: ") + mod.nome + ".json";
                return std::nullopt;
            }
        }
        if (versao < VERSAO_SAVE) tudo = migrar(std::move(tudo), versao);

        DadosSave d;
        const json& m = tudo["metadata"];
        d.versao = VERSAO_SAVE;
        d.versaoJogo = m.value("versao_jogo", "");
        d.dataHora = m.value("data_hora", "");
        d.tempoJogado = std::max(0.0, m.value("tempo_jogado", 0.0));
        d.semente = m.value("semente", 0u);
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
        const json& jg = tudo["guild"];
        const json avent = jg.value("aventureiros", json::object());  // variável: .items() de temporário quebra
        for (const auto& [id, ja] : avent.items()) {
            EstadoAventureiro a;
            a.nivel = std::clamp(ja.value("nivel", 1), 1, 3);
            a.feridoAte = std::max(0, ja.value("ferido_ate", 0));
            a.fora = ja.value("fora", false);
            a.sumido = ja.value("sumido", false);
            a.voltaDia = std::max(0, ja.value("volta_dia", 0));
            d.aventureiros[id] = a;
        }
        const json jexp = jg.value("expedicao", json::object());
        d.expedicao.destino = jexp.value("destino", "");
        d.expedicao.grupo = jexp.value("grupo", std::vector<std::string>{});
        d.expedicao.partida = jexp.value("partida", 0);
        d.expedicao.volta = jexp.value("volta", 0);
        for (const auto& jr : jg.value("reliquias", json::array()))
            d.reliquias.push_back({jr.value("id", ""), jr.value("exposta", false), jr.value("conhecida", false), jr.value("purificada", false)});
        const json& jvida = tudo["life"];
        d.temCotidiano = !jvida.value("padrao", false);
        for (const auto& jcan : jvida.value("canteiros", json::array()))
            d.canteiros.push_back({jcan.value("cultura", ""), std::max(0, jcan.value("crescidos", 0)), std::max(0, jcan.value("regado_dia", 0)), jcan.value("maduro", false)});
        d.buffDano = std::max(0, jvida.value("buff_dano", 0));
        d.buffDefesa = std::max(0, jvida.value("buff_defesa", 0));
        d.presenteDia = jvida.value("presente_dia", std::map<std::string, int>{});
        d.carinhos = jvida.value("carinhos", std::map<std::string, int>{});
        const json& jpov = tudo["people"];
        d.temPovoNomeado = !jpov.value("padrao", false);
        d.proximoHabitante = std::max(1, jpov.value("proximo", 1));
        for (const auto& jp : jpov.value("habitantes", json::array()))
            d.povo.push_back({jp.value("id", 0), jp.value("nome", ""), jp.value("familia", ""), jp.value("profissao", ""), jp.value("traco", ""),
                              jp.value("desejo", ""), jp.value("cumprido", false), jp.value("chegou", 1), std::max(0, jp.value("esperando", 0)),
                              std::clamp(jp.value("variante", 0), 0, 3)});
        const json& jfac = tudo["factions"];
        d.temFaccoes = !jfac.value("padrao", false);
        d.relacoes = jfac.value("relacoes", std::map<std::string, float>{});
        d.tratados = jfac.value("tratados", std::vector<std::string>{});
        d.desvios = jfac.value("desvios", std::map<std::string, int>{});
        d.suspeito = jfac.value("suspeito", "");
        if (jfac.contains("reputacao")) {
            const json& jr = jfac["reputacao"];
            d.reputacao = {jr.value("justica", 0.0f), jr.value("generosidade", 0.0f), jr.value("devocao", 0.0f)};
        }
        const json& jmer = tudo["market"];
        d.temMercado = !jmer.value("padrao", false);
        d.mercado.pressao = jmer.value("pressao", std::map<std::string, float>{});
        d.mercado.historico = jmer.value("historico", std::map<std::string, std::vector<float>>{});
        d.mercado.negociadosHoje = std::max(0, jmer.value("negociados_hoje", 0));
        d.mercado.vendido = std::max(0, jmer.value("vendido", 0));
        d.mercado.comprado = std::max(0, jmer.value("comprado", 0));
        for (const auto& je : jmer.value("encomendas", json::array())) d.mercado.encomendas.push_back({je.value("id", ""), je.value("dia", 0)});
        const json& jcas = tudo["castle"];
        d.ultimaAcaoAla = jcas.value("acoes", std::map<std::string, int>{});
        d.guardaTreinada = std::max(0, jcas.value("guarda_treinada", 0));
        if (jcas.contains("emprestimo")) {
            const json& je = jcas["emprestimo"];
            d.emprestimoVolta = std::max(0, je.value("volta", 0));
            d.emprestimoValor = std::max(0, je.value("valor", 0));
            d.emprestimoDia = je.value("dia", 0);
        }
        d.alas = jcas.value("alas", std::vector<std::string>{});
        d.brasaoCriado = jcas.value("brasao_criado", false);
        if (jcas.contains("brasao")) {
            const json& jb = jcas["brasao"];
            d.brasao = {jb.value("forma", ""), jb.value("divisao", ""), jb.value("cor1", ""), jb.value("cor2", ""), jb.value("simbolo", ""),
                        jb.value("cor_simbolo", ""), jb.value("lema", 0)};
            d.temBrasao = true;
        }
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
    terminarTrocaPendente(pasta_ / slot);
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
