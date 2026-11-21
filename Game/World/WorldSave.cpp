// Parte do World: o que vai para o save (capturar) e o que volta dele (aplicar)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

DadosSave World::capturar() const {
    DadosSave d;
    d.tempoJogado = tempoJogado;
    d.posJogador = jogador.pos;
    d.direcao = static_cast<int>(jogador.dir);
    d.inventario = inventario.todos();
    for (const auto& b : reino.construcoes) d.construcoes.push_back({b.id, b.defId, b.tile, true, b.level, b.constructed});
    d.eraVisual = reino.eraVisual;
    d.temPovo = true;
    d.populacao = estado.populacao;
    d.felicidade = estado.felicidade;
    d.imposto = static_cast<int>(estado.imposto);
    // Objetos e moradores de todas as regiões (a ativa e as guardadas)
    auto guardar = [&](const entt::registry& r) {
        for (auto [e, id] : r.view<const IdMundo>().each()) {
            if (const auto* c = r.try_get<Coletavel>(e); c && (c->hp < c->hpMax || c->esgotado))
                d.objetos.push_back({id.id, c->hp, c->esgotado, false, c->diaEsgotado});
            if (const auto* s = r.try_get<Saqueavel>(e); s && s->aberto) d.objetos.push_back({id.id, 0, false, true});
        }
        for (auto [e, m, rel] : r.view<const Morador, const Relacionamento>().each())
            d.moradores.push_back({m.id, rel.afinidade, rel.falouHoje, rel.conheceu, {rel.memoria.begin(), rel.memoria.end()}});
    };
    guardar(reg);
    for (const auto& [id, g] : guardadas_) guardar(g.reg);
    d.dia = relogio.dia;
    d.minutos = relogio.minutos;
    // Fase 6
    d.regiao = regiaoAtual;
    d.vida = jogador.vida;
    d.arma = equip.arma;
    d.armadura = equip.armadura;
    d.equipamentos = {equip.possuidos.begin(), equip.possuidos.end()};
    for (const auto& [id, m] : missoes) d.missoes[id] = {m.concluida, m.base};
    d.abates = abates;
    d.visitadas = {visitadas.begin(), visitadas.end()};
    d.marcos = {marcos.begin(), marcos.end()};
    d.temCronicas = true;
    d.cronicas = cronicas;
    d.temRegioes = true;
    d.regioes = estadoRegioes;
    d.conselho = conselho;
    d.lealdade = lealdade;
    d.leis = {leis.begin(), leis.end()};
    d.decisaoPendente = decisaoPendente;
    d.ultimaDecisao = ultimaDecisao;
    d.temDefesa = true;
    d.hordaDia = hordaDia;
    d.hordaNumero = hordaNumero;
    d.proximaHorda = proximaHorda;
    d.portaoVida = portaoVida;
    d.notasDefesa = notasDefesa;
    d.aventureiros = aventureiros;
    d.expedicao = expedicao;
    d.reliquias = reliquias;
    d.temCotidiano = true;
    d.canteiros = canteiros;
    d.buffDano = buffDano;
    d.buffDefesa = buffDefesa;
    d.presenteDia = presenteDia;
    d.carinhos = carinhos;
    // Fase 17: a semente do save muda com o momento (recarregar um save antigo não repete a sorte de outro)
    d.semente = sementeBase_ * 2654435761u ^ (static_cast<uint32_t>(relogio.dia) * 977u + static_cast<uint32_t>(relogio.minutos));
    if (d.semente == 0) d.semente = 1;
    // Fase 16
    d.alas = alas;
    d.temBrasao = true;
    d.brasaoCriado = brasaoCriado;
    d.brasao = brasao;
    // Fase 18
    d.temPovoNomeado = true;
    d.povo = povo;
    d.proximoHabitante = proximoHabitante;
    // Fase 19
    d.temFaccoes = true;
    d.relacoes = relacoes;
    d.tratados.assign(tratados.begin(), tratados.end());
    d.reputacao = reputacao;
    d.desvios = desvios;
    d.suspeito = suspeito;
    // Fase 20
    d.temMercado = true;
    d.mercado = mercado;
    d.ultimaAcaoAla = ultimaAcaoAla;
    d.guardaTreinada = guardaTreinada;
    d.emprestimoVolta = emprestimoVolta;
    d.emprestimoValor = emprestimoValor;
    d.emprestimoDia = emprestimoDia;
    // Fase 21
    d.temLinhagem = true;
    d.linhagem = linhagem;
    d.temMundo = true;
    d.mundo = mundoVivo;
    return d;
}

void World::aplicar(const DadosSave& d) {
    tempoJogado = d.tempoJogado;
    if (d.semente != 0) semear(d.semente);  // fase 17 (saves antigos seguem com a semente de sempre)
    // Posição fora do mapa ou dentro de algo sólido → volta ao ponto inicial
    jogador.pos = d.posJogador;
    const bool foraDaVila = d.regiao != "vilarejo" && regioes.contains(d.regiao);
    if (!foraDaVila && ke::colide(mapa, jogador.caixa(), {})) jogador.pos = spawn;
    jogador.dir = static_cast<Direcao>(std::clamp(d.direcao, 0, 3));
    inventario = {};
    for (const auto& [id, q] : d.inventario) inventario.definir(id, q);
    for (const EstadoConstrucao& c : d.construcoes) {
        BuildingData* b = reino.construcao(c.id);
        if (!b) {
            // Obra colocada pelo jogador: recria no lugar salvo (se a definição ainda existir)
            const auto it = dados().construcoes.find(c.def);
            if (!c.temPosicao || it == dados().construcoes.end()) continue;
            reino.construcoes.push_back({c.id, c.def, "", 1, c.tile, true, c.construida});
            b = &reino.construcoes.back();
            criarEntidadeConstrucao(*b);
        }
        const BuildingDef* df = def(*b);
        if (!df) continue;
        if (const auto* nv = df->nivel(std::clamp(c.nivel, 1, df->nivelMax()))) {
            b->level = nv->nivel;
            b->name = nv->nome;
        }
        b->constructed = c.construida;
    }
    if (d.temPovo) {
        estado.populacao = d.populacao;
        estado.felicidade = d.felicidade;
        estado.imposto = static_cast<Imposto>(std::clamp(d.imposto, 0, 2));
    }
    reino.eraVisual = d.eraVisual;
    eraAnunciada_ = static_cast<int>(reino.eraAlvo());  // ao carregar, não anuncia de novo a era em que já está
    std::unordered_map<std::string, const EstadoObjeto*> porId;
    for (const auto& o : d.objetos) porId[o.id] = &o;
    auto restaurar = [&](entt::registry& r) {
        for (auto [e, id] : r.view<const IdMundo>().each()) {
            const auto it = porId.find(id.id);
            if (it == porId.end()) continue;
            if (auto* c = r.try_get<Coletavel>(e)) {
                c->hp = std::clamp(it->second->hp, 0, c->hpMax);
                c->esgotado = it->second->esgotado || c->hp == 0;
                sujarObstaculos();
                c->diaEsgotado = it->second->diaEsgotado;
                if (c->esgotado) {
                    c->hp = 0;
                    r.get<Visual>(e).sprite = c->spriteEsgotado;
                }
            }
            if (auto* s = r.try_get<Saqueavel>(e); s && it->second->aberto) {
                s->aberto = true;
                r.get<Visual>(e).sprite = s->spriteAberto;
            }
        }
    };
    restaurar(reg);
    for (auto& [id, g] : guardadas_) restaurar(g.reg);
    // Fase 6: equipamento, missões e acontecimentos (validados contra os dados atuais)
    equip = {};
    for (const auto& id : d.equipamentos)
        if (dados().equipamentos.contains(id)) equip.possuidos.insert(id);
    if (equip.possuidos.contains(d.arma) && dados().equipamentos.at(d.arma).tipo == "arma") equip.arma = d.arma;
    if (equip.possuidos.contains(d.armadura) && dados().equipamentos.at(d.armadura).tipo == "armadura") equip.armadura = d.armadura;
    missoes.clear();
    for (const auto& [id, m] : d.missoes)
        if (std::ranges::any_of(dados().missoes, [&](const MissaoDef& x) { return x.id == id; })) missoes[id] = {m.first, m.second};
    abates = d.abates;
    visitadas = {d.visitadas.begin(), d.visitadas.end()};
    visitadas.insert("vilarejo");
    marcos = {d.marcos.begin(), d.marcos.end()};
    reino.magiaLiberta = marcos.contains("chefe_cavaleiro_caido");
    eraAnunciada_ = static_cast<int>(reino.eraAlvo());
    cronicas = d.cronicas;
    if (!d.temCronicas) reconstruirCronicas();
    verificarTitulos(true);  // títulos novos desta versão valem para o que já foi feito
    jogador.vida = std::clamp(d.vida, 1, jogador.vidaMax);
    relogio.dia = std::max(1, d.dia);
    relogio.minutos = std::clamp(d.minutos, static_cast<float>(Relogio::INICIO_DIA), static_cast<float>(Relogio::FIM_DIA) - 1.0f);
    // Fase 11: estado das regiões vivas (save antigo: como no mapa, visitadas hoje para não contar abandono)
    for (auto& [id, e] : estadoRegioes) {
        if (const auto it = d.regioes.find(id); d.temRegioes && it != d.regioes.end()) e = it->second;
        else e.ultimaVisita = relogio.dia;
        e.ultimaVisita = std::min(e.ultimaVisita, relogio.dia);
    }
    // Fase 12: conselho (validado contra os dados atuais: cargo, morador e lei precisam existir)
    conselho.clear();
    for (const auto& [cargo, npc] : d.conselho)
        if (dados().conselho.cargo(cargo) && dados().npcs.contains(npc) && !dados().npcs.at(npc).visitante()) conselho[cargo] = npc;
    lealdade.clear();
    for (const auto& [npc, l] : d.lealdade)
        if (dados().npcs.contains(npc)) lealdade[npc] = std::clamp(l, 0, 100);
    leis.clear();
    for (const auto& l : d.leis)
        if (dados().conselho.leis.contains(l)) leis.insert(l);
    decisaoPendente = dados().conselho.decisao(d.decisaoPendente) ? d.decisaoPendente : "";
    ultimaDecisao = std::min(d.ultimaDecisao, relogio.dia);
    // Fase 13: hordas e portão (a batalha em si não se salva: se a noite da horda for carregada depois das 20:00,
    // ela recomeça do portão)
    batalha_ = false;
    hordaDia = d.temDefesa && d.hordaDia >= relogio.dia ? d.hordaDia : 0;
    hordaNumero = d.hordaNumero;
    proximaHorda = d.proximaHorda;
    portaoVida = d.temDefesa ? std::clamp(d.portaoVida, 0, portaoVidaMax()) : portaoVidaMax();
    notasDefesa = d.notasDefesa;
    // Fase 14: guilda (validada contra os dados: aventureiro, destino e relíquia precisam existir)
    const DadosGuilda& dg = dados().guilda;
    aventureiros.clear();
    for (const auto& [id, a] : d.aventureiros)
        if (dg.aventureiro(id)) aventureiros[id] = a;
    expedicao = {};
    if (dg.destino(d.expedicao.destino)) {
        expedicao = d.expedicao;
        std::erase_if(expedicao.grupo, [&](const std::string& id) { return !aventureiros.contains(id); });
        if (expedicao.grupo.empty()) expedicao = {};
    }
    reliquias.clear();
    for (const auto& r : d.reliquias)
        if (dg.reliquia(r.id) && !estadoReliquia(r.id)) reliquias.push_back(r);
    // Fase 15: horta, efeitos de pratos, presentes e carinhos (save antigo: horta vazia e 4 sementes de nabo para começar)
    const DadosCotidiano& dc = dados().cotidiano;
    canteiros.assign(dc.canteiros.size(), {});
    for (size_t k = 0; k < canteiros.size() && k < d.canteiros.size(); ++k)
        if (d.canteiros[k].cultura.empty() || dc.cultura(d.canteiros[k].cultura)) canteiros[k] = d.canteiros[k];
    buffDano = std::max(0, d.buffDano);
    buffDefesa = std::max(0, d.buffDefesa);
    presenteDia = d.presenteDia;
    carinhos = d.carinhos;
    if (!d.temCotidiano) inventario.adicionar("semente_nabo", 4);
    atualizarCanteiros();
    // Fase 16: alas que ainda existem (no máximo as vagas do castelo) e o brasão validado
    alas.clear();
    for (const auto& id : d.alas)
        if (dados().castelo.ala(id) && std::ranges::find(alas, id) == alas.end() && static_cast<int>(alas.size()) < vagasAlas()) alas.push_back(id);
    brasao = dados().castelo.validar(d.temBrasao ? d.brasao : dados().castelo.inicial);
    brasaoCriado = d.temBrasao && d.brasaoCriado;
    // Fase 18: a era já conquistada (pelos marcos era:N) não volta atrás
    for (int e = 3; e >= 1; --e)
        if (marcos.contains(std::format("era:{}", e))) {
            reino.eraConquistada = e;
            break;
        }
    // Fase 18: o povo com nome (saves antigos: gerado agora, pela sorte do save, do tamanho da população)
    if (d.temPovoNomeado) {
        povo = d.povo;
        proximoHabitante = std::max(d.proximoHabitante, 1);
        for (const Habitante& h : povo) proximoHabitante = std::max(proximoHabitante, h.id + 1);
        std::vector<std::string> notas;
        ajustarPovo(false, notas);  // se a população salva não bate (save editado), acerta
    } else {
        gerarPovoInicial();
    }
    sincronizarAldeoes();
    sincronizarAnimais();
    renascerInimigos();  // o chefe derrotado não volta (e a corrupção salva decide quantas criaturas há)
    bloqueio_.clear();
    for (const auto& m : d.moradores) {
        const entt::entity e = morador(m.id);
        if (e == entt::null) continue;  // morador que não existe mais nesta versão
        auto& r = reg.get<Relacionamento>(e);
        r.afinidade = std::clamp(m.afinidade, 0, 1000);
        r.falouHoje = m.falouHoje;
        r.conheceu = m.conheceu;
        r.memoria = {m.memoria.begin(), m.memoria.end()};
    }
    posicionarMoradores();
    sincronizarAldeoes();
    // Fase 19: os vizinhos (saves antigos: refeitos repassando os marcos que já existem)
    iniciarFaccoes();
    if (d.temFaccoes) {
        for (const auto& [f, v] : d.relacoes)
            if (dados().faccoes.faccao(f)) relacoes[f] = std::clamp(v, -100.0f, 100.0f);
        for (const auto& t : d.tratados)
            if (dados().faccoes.tratado(t)) tratados.insert(t);
        reputacao = d.reputacao;
        for (const auto& [npc, q] : d.desvios)
            if (q > 0) desvios[npc] = q;
        suspeito = d.suspeito;
    } else {
        const std::set<std::string> jaAconteceu = marcos;
        for (const auto& m : jaAconteceu) reagirAoMarco(m);
    }
    // Fase 20: o mercado (só bens e encomendas que ainda existem) e as ações das alas
    mercado = {};
    if (d.temMercado) {
        const DadosMercado& dm = dados().mercado;
        for (const auto& [b, p] : d.mercado.pressao)
            if (dm.bem(b)) mercado.pressao[b] = std::clamp(p, dm.pressaoMin, dm.pressaoMax);
        for (const auto& [b, h] : d.mercado.historico)
            if (dm.bem(b)) mercado.historico[b] = h;
        for (const auto& e : d.mercado.encomendas)
            if (dm.encomenda(e.id)) mercado.encomendas.push_back(e);
        mercado.negociadosHoje = d.mercado.negociadosHoje;
        mercado.vendido = d.mercado.vendido;
        mercado.comprado = d.mercado.comprado;
    }
    ultimaAcaoAla = d.ultimaAcaoAla;
    guardaTreinada = d.guardaTreinada;
    emprestimoVolta = d.emprestimoVolta;
    emprestimoValor = d.emprestimoValor;
    emprestimoDia = d.emprestimoDia;
    // Fase 21: a família (saves antigos: o fundador, primeira geração; família quebrada também volta a ser só o fundador)
    if (d.temLinhagem && d.linhagem.membro(d.linhagem.atual)) {
        linhagem = d.linhagem;
        int maior = 0;
        for (const Membro& m : linhagem.membros) maior = std::max(maior, m.id);
        linhagem.proximoId = std::max(linhagem.proximoId, maior + 1);
        aplicarTracos();
    } else {
        const int vida = jogador.vida;
        iniciarLinhagem();
        jogador.vida = std::clamp(vida, 1, jogador.vidaMax);
    }
    // Mundo vivo: só as aldeias e pedidos que ainda existem nos dados (saves antigos: como nos dados)
    iniciarMundoVivo();
    if (d.temMundo) {
        const DadosMundo& dm = dados().mundo;
        for (const auto& [id, e] : d.mundo.aldeias)
            if (dm.aldeia(id)) mundoVivo.aldeias[id] = e;
        for (const PedidoAberto& p : d.mundo.pedidos)
            if (p.aldeia.empty() ? dm.viajante(p.def) != nullptr : dm.aldeia(p.aldeia) && dm.ajuda(p.def)) mundoVivo.pedidos.push_back(p);
        mundoVivo.noticias = d.mundo.noticias;
    }
    atualizarPistas();
    atualizarEspeciais();
    atualizarMeteorito();
    avisoEvento_ = avisoFestival_ = 0;
    // Salvou fora do vilarejo: volta para a mesma região e lugar
    if (foraDaVila) {
        const glm::vec2 p = d.posJogador;
        entrarRegiao(d.regiao, {static_cast<int>(p.x) / LADO, static_cast<int>(p.y) / LADO}, jogador.dir);
        jogador.pos = p;
        if (ke::colide(mapa, jogador.caixa(), {})) {  // lugar inválido: entrada da região
            const auto& s = infoAtual().saidas;
            if (!s.empty()) jogador.pos = {s.front().tile.x * LADO + 24.0f, s.front().tile.y * LADO + 12.0f};
        }
        tirarEventos();
    }
}

}
