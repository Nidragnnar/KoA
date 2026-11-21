// Parte do World: combate: ataque, inimigos, derrota e equipamento
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Combate ----------

int World::danoArma() const {
    const auto it = dados().equipamentos.find(equip.arma);
    return (it == dados().equipamentos.end() ? 1 : it->second.dano) + efeitosReliquias().dano + buffDano + efeitosTratados().dano +
           efeitosHerdeiro().dano;  // relíquias, pratos, a Chama Compartilhada e os traços de quem reina (fase 21)
}

int World::defesaArmadura() const {
    const auto it = dados().equipamentos.find(equip.armadura);
    return (it == dados().equipamentos.end() ? 0 : it->second.defesa) + efeitosReliquias().defesa + buffDefesa;
}

int World::nivelForja() const { return static_cast<int>(efeitoMaximo("nivel_forja")); }

bool World::forjar(const std::string& id) {
    const auto it = dados().equipamentos.find(id);
    if (it == dados().equipamentos.end() || it->second.forja <= 0) return false;
    const EquipDef& d = it->second;
    auto aviso = [&](std::string t) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, id, "", 0, std::move(t)});
        return false;
    };
    if (equip.possuidos.contains(id)) return aviso("Você já tem " + d.nome + ".");
    if (nivelForja() < d.forja) return aviso("A forja precisa estar no nível " + std::to_string(d.forja) + ".");
    if (!inventario.pagar(d.custo)) return aviso("Faltam recursos para " + d.nome + ".");
    equip.possuidos.insert(id);
    // Equipa na hora se for melhor que o atual
    if (d.tipo == "arma" && d.dano > dados().equipamentos.at(equip.arma).dano) equip.arma = id;
    if (d.tipo == "armadura" && d.defesa > dados().equipamentos.at(equip.armadura).defesa) equip.armadura = id;
    eventos_.push_back({EventoJogo::Tipo::Forjou, jogador.pos, id, "", 0, d.nome});
    return true;
}

int World::nivelMercado() const { return static_cast<int>(efeitoMaximo("nivel_mercado")); }

void World::criarInimigo(const std::string& tipo, glm::ivec2 tile) {
    criarInimigoEm(tipo, {tile.x * LADO + LADO / 2.0f, tile.y * LADO + 12.0f});
}

entt::entity World::criarInimigoEm(const std::string& tipo, glm::vec2 p) {
    const InimigoDef& d = dados().inimigos.at(tipo);
    const auto e = reg.create();
    reg.emplace<Posicao>(e, p);
    Inimigo& i = reg.emplace<Inimigo>(e);
    i.tipo = tipo;
    i.vida = i.vidaMax = static_cast<int>(std::round(d.vida * estacaoAtual().inimigos));  // no inverno, mais resistentes
    i.origem = i.destino = p;
    i.espera = 0.5f + (static_cast<int>(p.x) / LADO % 5) * 0.4f;
    return e;
}

void World::renascerInimigos() {
    // Em todas as regiões: some quem sobrou e todos voltam ao lugar de origem
    // criarInimigo usa `reg`; para as regiões guardadas, a guardada entra em `reg` por um instante
    auto refazerAtivo = [&](const InfoRegiao& info) {
        std::vector<entt::entity> velhos;
        for (auto [e, i] : reg.view<Inimigo>().each()) velhos.push_back(e);
        reg.destroy(velhos.begin(), velhos.end());
        // Fase 11: quanto mais corrompida a região, mais criaturas voltam (sempre as mesmas, pelo lugar)
        const auto est = estadoRegioes.find(info.id);
        const bool viva = est != estadoRegioes.end();
        const float corrupcao = viva ? est->second.corrupcao : 100.0f;
        const TransformacaoRegiao* tr = viva ? transformacao(info.id) : nullptr;
        const int fracao = static_cast<int>(std::round(fracaoCriaturas(corrupcao) * 100.0f));
        for (const auto& [tipo, tile] : info.spawns) {
            const InimigoDef& d = dados().inimigos.at(tipo);
            if (d.chefe && marcos.contains("chefe_" + tipo)) continue;  // o chefe não volta
            if (tr && tr->semInimigos.contains(tipo) && corrupcao < tr->voltamAcima) continue;  // a terra purificada as afastou
            if (!d.chefe && static_cast<int>(hashTile(tile.x, tile.y) % 100) >= fracao) continue;
            criarInimigo(tipo, tile);
        }
        // Região tomada: chegam mais criaturas pelas bordas
        if (viva && corrupcao >= regra::INVASAO)
            for (const auto& [tipo, tile] : info.vida.invasao) criarInimigo(tipo, tile);
    };
    refazerAtivo(infoAtual());
    for (auto& [id, g] : guardadas_) {
        std::swap(reg, g.reg);
        refazerAtivo(regioes.at(id));
        std::swap(reg, g.reg);
    }
}

std::string World::efeitoArma() const {
    const auto it = dados().equipamentos.find(equip.arma);
    return it == dados().equipamentos.end() ? "" : it->second.efeito;
}

void World::golpearComEspada(bool carregado) {
    jogador.acao = Jogador::DURACAO_ATAQUE;
    jogador.atacando = true;
    jogador.carregado = carregado;
    ++jogador.golpeId;
    // 1.16: o golpe carregado e a Lâmina de Cristal alcançam mais longe
    const std::string efeito = efeitoArma();
    const float alcance = (carregado ? 1.5f : 1.0f) * (efeito == "atravessa" ? 1.4f : 1.0f);
    const glm::vec2 frente = vetor(jogador.dir);
    const glm::vec2 centro = jogador.pos + glm::vec2(0, -7) + frente * (13.0f * (alcance > 1.0f ? 1.25f : 1.0f));
    const glm::vec2 meia = ((jogador.dir == Direcao::Esquerda || jogador.dir == Direcao::Direita) ? glm::vec2(11, 10) : glm::vec2(12, 9)) * alcance;
    const ke::AABB area{centro - meia, centro + meia};
    eventos_.push_back({carregado ? EventoJogo::Tipo::GolpeCarregado : EventoJogo::Tipo::Ataque, centro});
    std::vector<entt::entity> mortos;
    for (auto [e, p, i] : reg.view<Posicao, Inimigo>().each()) {
        if (i.ultimoGolpe == jogador.golpeId || i.vida <= 0 || i.rendido) continue;
        const InimigoDef& d = dados().inimigos.at(i.tipo);
        const glm::vec2 meiaIni = glm::vec2(d.tamanho) * glm::vec2(0.35f, 0.4f);
        const glm::vec2 ci = p.v - glm::vec2(0, d.tamanho.y * 0.4f);
        if (!area.intersecta({ci - meiaIni, ci + meiaIni})) continue;
        i.ultimoGolpe = jogador.golpeId;
        const int dano = danoContra(danoArma() * (carregado ? 2 : 1), efeito == "atravessa" ? 0 : d.defesa);
        i.vida -= dano;
        i.piscar = 0.12f;
        // 1.19: o chefe aguenta alguns golpes sem recuar (e nenhum enquanto prepara o golpe em área); quebrada a postura, cambaleia
        bool recua = true;
        if (d.postura > 0) {
            i.golpesSofridos += carregado ? 2 : 1;
            recua = i.golpesSofridos >= d.postura && i.preparo <= 0.0f;
            if (recua) {
                i.golpesSofridos = 0;
                eventos_.push_back({EventoJogo::Tipo::Cambaleou, ci, i.tipo});
            }
        }
        if (recua) i.empurrao = frente * (d.chefe ? 110.0f : 160.0f) * (carregado ? 1.6f : 1.0f);
        if (efeito == "queima") i.queimando = 3.0f, i.tickQueima = 0.5f;  // a Lâmina da Chama
        i.perseguindo = true;
        eventos_.push_back({EventoJogo::Tipo::AcertouInimigo, ci, i.tipo, "", dano});
        // Fase 11: chefe com escolha se rende com 30% da vida e espera a decisão do herdeiro
        if (!d.escolha.empty() && i.vida <= i.vidaMax * 3 / 10) {
            i.vida = std::max(1, i.vida);
            i.rendido = true;
            i.perseguindo = false;
            i.empurrao = {0, 0};
            i.preparo = 0.0f;
            eventos_.push_back({EventoJogo::Tipo::AbrirEscolha, ci, d.escolha});
            continue;
        }
        if (i.vida <= 0) mortos.push_back(e);
    }
    for (const entt::entity e : mortos) derrotarInimigo(e);
    if (!mortos.empty()) atualizarMissoes();
}

void World::derrotarInimigo(entt::entity e) {
    const Inimigo& i = reg.get<Inimigo>(e);
    const InimigoDef& d = dados().inimigos.at(i.tipo);
    const glm::vec2 p = reg.get<Posicao>(e).v;
    std::uniform_int_distribution<int> ouro(d.ouroMin, std::max(d.ouroMin, d.ouroMax));
    int o = ouro(rng_);
    if (const EventoDef* ev = eventoHoje(); ev && eventoAgora(ev->id)) o = static_cast<int>(std::round(o * ev->ouroInimigos));
    inventario.adicionar("ouro", o);
    ++abates[i.tipo];
    if (i.horda) ++vencidasNaBatalha_;
    // Cada criatura vencida enfraquece a corrupção da região
    if (const auto it = estadoRegioes.find(regiaoAtual); it != estadoRegioes.end()) registrarAbate(it->second, transformacao(regiaoAtual));
    eventos_.push_back({EventoJogo::Tipo::InimigoDerrotado, p, i.tipo, "ouro", o, d.nome});
    if (d.chefe) {
        reino.magiaLiberta = true;  // com o Cavaleiro Caído derrotado, a magia pode voltar ao reino
        marcar("chefe_" + i.tipo, d.nome);
    }
    reg.destroy(e);
}

void World::ferirHeroi(int dano, glm::vec2 de) {
    if (jogador.vida <= 0) return;
    if (jogador.esquivando()) {  // 1.16: a esquiva certa anula o golpe
        eventos_.push_back({EventoJogo::Tipo::Esquivou, jogador.pos, "desviou"});
        return;
    }
    if (jogador.invulneravel > 0.0f) return;
    const int final = danoContra(dano, defesaArmadura());
    jogador.vida = std::max(0, jogador.vida - final);
    jogador.invulneravel = 0.9f;
    const glm::vec2 d = jogador.pos - de;
    jogador.empurrao = (glm::length(d) > 0.1f ? glm::normalize(d) : glm::vec2(0, 1)) * 170.0f;
    eventos_.push_back({EventoJogo::Tipo::HeroiFerido, jogador.pos, "", "", final});
    if (jogador.vida == 0 && !derrotaPendente_) {
        derrotaPendente_ = true;  // a cena escurece e chama reviver()
        eventos_.push_back({EventoJogo::Tipo::HeroiDerrotado, jogador.pos});
    }
}

void World::atualizarInimigos(float dt, const std::vector<ke::AABB>& obst) {
    std::uniform_real_distribution<float> u(-1.0f, 1.0f), espera(1.0f, 3.5f);
    // Fase 13: a horda só esbarra na muralha e no portão (passa por cima do resto, como uma enxurrada)
    std::vector<ke::AABB> barreiras;
    if (noVilarejo())
        if (const BuildingData* m = muralha(); m && m->level >= 2) {
            const ke::AABB c = caixaConstrucao(*m), g = caixaPortao();
            barreiras.push_back({c.min, {c.max.x, g.min.y}});
            barreiras.push_back({{c.min.x, g.max.y}, c.max});
            if (portaoDePe()) barreiras.push_back(g);
        }
    const glm::vec2 praca = glm::vec2(dados().defesa.praca) * static_cast<float>(LADO) + glm::vec2(8, 12);
    std::vector<entt::entity> saqueadores, soldadosCaidos, queimados;
    // 1.16: o que nasce durante o laço só é criado depois dele (criar entidades no meio do laço atrapalha a visão)
    std::vector<std::pair<glm::vec2, Projetil>> tiros;
    std::vector<glm::vec2> brasas;
    std::vector<std::pair<std::string, glm::vec2>> chamados;  // bando: quem viu o herói chama os iguais por perto
    for (auto [e, p, i] : reg.view<Posicao, Inimigo>().each()) {
        const InimigoDef& d = dados().inimigos.at(i.tipo);
        i.piscar = std::max(0.0f, i.piscar - dt);
        i.recarga = std::max(0.0f, i.recarga - dt);
        // 1.16: queimando (Lâmina da Chama); o chefe que se rende não morre queimado antes da escolha
        if (i.queimando > 0.0f && !i.rendido) {
            i.queimando -= dt;
            i.tickQueima -= dt;
            if (!d.escolha.empty() && i.vida <= i.vidaMax * 3 / 10 + 2) i.queimando = 0.0f;
            else if (i.tickQueima <= 0.0f && i.vida > 0) {
                i.tickQueima = 0.5f;
                i.vida -= 2;
                i.piscar = 0.08f;
                eventos_.push_back({EventoJogo::Tipo::Queimou, p.v - glm::vec2(0, d.tamanho.y * 0.5f), i.tipo, "", 2});
                if (i.vida <= 0) queimados.push_back(e);
            }
        }
        // 1.16: o chefe muda de padrão quando perde vida o bastante
        if (d.fase2 > 0.0f && i.fase == 1 && i.vida > 0 && i.vida <= i.vidaMax * d.fase2) {
            i.fase = 2;
            eventos_.push_back({EventoJogo::Tipo::FaseChefe, p.v, i.tipo, "", 0,
                                d.efeitoFase2 == "brasa" ? d.nome + " racha: o chão em volta vira brasa!" : d.nome + " enfurece e investe sem parar!"});
        }
        if (i.rendido) {  // de joelhos: não anda nem fere
            i.andando = false;
            continue;
        }
        const ke::AABB caixa{p.v - glm::vec2(5, 5), p.v + glm::vec2(5, 1)};
        // Recuo do golpe tem prioridade sobre andar
        if (glm::length(i.empurrao) > 1.0f) {
            p.v += ke::moverComColisao(mapa, caixa, i.empurrao * dt, obst, 0.0f);
            i.empurrao *= std::max(0.0f, 1.0f - 9.0f * dt);
            continue;
        }
        if (i.horda) {
            // Alvo vivo mais perto (o herói ou um soldado); sem ninguém por perto, marcha para o portão ou para a praça
            glm::vec2 alvo{0.0f};
            float distAlvo = 1e9f;
            entt::entity soldadoAlvo = entt::null;
            if (jogador.vida > 0) alvo = jogador.pos, distAlvo = glm::distance(jogador.pos, p.v);
            for (auto [s, ps, so] : reg.view<const Posicao, const Soldado>().each())
                if (so.vida > 0 && glm::distance(ps.v, p.v) < distAlvo) alvo = ps.v, distAlvo = glm::distance(ps.v, p.v), soldadoAlvo = s;
            const bool persegue = distAlvo < d.visao;
            glm::vec2 destino = praca;
            const ke::AABB g = caixaPortao();
            if (persegue) destino = alvo;
            else if (portaoDePe()) destino = {g.max.x + 7.0f, std::clamp(p.v.y, g.min.y + 6.0f, g.max.y - 1.0f)};
            const glm::vec2 dirv = destino - p.v;
            i.andando = false;
            if (glm::length(dirv) > 2.0f) {
                const glm::vec2 feito = ke::moverComColisao(mapa, caixa, glm::normalize(dirv) * d.velocidade * tempoAqui().velocidade * dt, barreiras, 3.0f);  // fase 18: a neve segura a horda
                p.v += feito;
                i.andando = glm::length(feito) > 0.01f;
                if (std::abs(dirv.x) > std::abs(dirv.y)) i.dir = dirv.x < 0 ? Direcao::Esquerda : Direcao::Direita;
                else i.dir = dirv.y < 0 ? Direcao::Cima : Direcao::Baixo;
            }
            if (i.andando) i.tempoAnim += dt;
            // Encostada no portão: golpeia até ele ceder
            if (!persegue && portaoDePe() && p.v.x - g.max.x < 14.0f && p.v.y > g.min.y && p.v.y < g.max.y + 6.0f && i.recarga <= 0.0f) {
                portaoVida = std::max(0, portaoVida - static_cast<int>(std::round(d.dano * forcaInimigos())));
                i.recarga = 1.2f;
                eventos_.push_back({EventoJogo::Tipo::PortaoGolpeado, {g.max.x, p.v.y - 8.0f}, i.tipo, "", portaoVida});
                if (portaoVida == 0) {
                    eventos_.push_back({EventoJogo::Tipo::PortaoCaiu, (g.min + g.max) * 0.5f, "", "", 0, "O portão leste caiu! As criaturas avançam para a praça!"});
                    bloqueio_.clear();
                }
            }
            // Chegou à praça: saqueia e foge com o que pegou
            if (!persegue && glm::distance(p.v, praca) < 22.0f) saqueadores.push_back(e);
            // Fere quem estiver encostado
            if (persegue && distAlvo < d.tamanho.x * 0.45f + 6.0f && i.recarga <= 0.0f) {
                const int dano = static_cast<int>(std::round(d.dano * forcaInimigos()));
                if (soldadoAlvo != entt::null) {
                    Soldado& so = reg.get<Soldado>(soldadoAlvo);
                    so.vida -= danoContra(dano, 2);
                    so.piscar = 0.12f;
                    if (so.vida <= 0) soldadosCaidos.push_back(soldadoAlvo);
                } else {
                    ferirHeroi(dano, p.v);
                }
                i.recarga = 1.0f;
            }
            continue;
        }
        const glm::vec2 paraHeroi = jogador.pos - p.v;
        const float dist = glm::length(paraHeroi);
        const bool longeDeCasa = glm::distance(p.v, i.origem) > 220.0f;
        if (!i.perseguindo && dist < d.visao && jogador.vida > 0) {
            i.perseguindo = true;
            if (d.comportamento == "bando") chamados.push_back({i.tipo, p.v});
        }
        if (i.perseguindo && (dist > d.visao * 2.2f || longeDeCasa || jogador.vida <= 0)) i.perseguindo = false;

        glm::vec2 alvo = i.destino;
        float vel = d.velocidade * tempoAqui().velocidade;  // fase 18: na neve as criaturas também se arrastam
        if (i.perseguindo) {
            alvo = jogador.pos;
            if (d.chefe) {  // de tempos em tempos, o chefe arranca em direção ao herói (1.16: na fase 2 do Cavaleiro, mais vezes e mais rápido)
                const bool furia = i.fase == 2 && d.efeitoFase2 == "investida";
                i.recargaInvestida -= dt;
                if (i.recargaInvestida <= 0.0f && dist < 110.0f) i.investida = 0.55f, i.recargaInvestida = furia ? 1.6f : 3.2f;
                if (i.investida > 0.0f) i.investida -= dt, vel *= furia ? 4.2f : 3.4f;
                // 1.16: na fase 2 do Golem, o chão onde o herói pisa vira brasa
                if (i.fase == 2 && d.efeitoFase2 == "brasa" && (i.recargaBrasa -= dt) <= 0.0f) {
                    brasas.push_back(jogador.pos);
                    i.recargaBrasa = 2.5f;
                }
                // 1.19: golpe em área avisado: perto do herói, para e brilha; no fim do preparo acerta quem ficou no raio
                if (d.golpe.dano > 0) {
                    i.recargaGolpe = std::max(0.0f, i.recargaGolpe - dt);
                    if (i.preparo > 0.0f) {
                        i.investida = 0.0f;
                        if ((i.preparo -= dt) <= 0.0f) {
                            i.preparo = 0.0f;
                            if (dist < d.golpe.raio && jogador.vida > 0) ferirHeroi(static_cast<int>(std::round(d.golpe.dano * forcaInimigos())), p.v);
                            eventos_.push_back({EventoJogo::Tipo::GolpeChefe, p.v, i.tipo, "", static_cast<int>(d.golpe.raio)});
                            i.recargaGolpe = d.golpe.recarga * (i.fase == 2 ? 0.65f : 1.0f);  // enfurecido, golpeia mais vezes
                        }
                    } else if (i.recargaGolpe <= 0.0f && dist < d.golpe.raio * 0.85f && jogador.vida > 0) {
                        i.preparo = d.golpe.preparo;
                        eventos_.push_back({EventoJogo::Tipo::ChefePrepara, p.v, i.tipo, "", static_cast<int>(d.golpe.raio)});
                    }
                }
            }
            if (d.comportamento == "distancia") {
                // Fica longe: recua se o herói chega perto, para no meio do caminho e atira quando dá
                if (dist < 60.0f) alvo = p.v - paraHeroi;
                else if (dist < 100.0f) vel = 0.0f;
                i.recargaTiro -= dt;
                if (dist <= d.tiro.alcance && i.recargaTiro <= 0.0f && dist > 1.0f) {
                    const glm::vec2 boca = p.v - glm::vec2(0, d.tamanho.y * 0.5f);
                    const glm::vec2 mira = glm::normalize(jogador.pos - glm::vec2(0, 7) - boca);
                    tiros.push_back({boca, {mira * d.tiro.velocidade, static_cast<int>(std::round(d.tiro.dano * forcaInimigos())), d.tiro.alcance / d.tiro.velocidade, i.tipo}});
                    i.recargaTiro = d.tiro.recarga;
                }
                if (std::abs(paraHeroi.x) > std::abs(paraHeroi.y)) i.dir = paraHeroi.x < 0 ? Direcao::Esquerda : Direcao::Direita;
            }
            if (d.comportamento == "bando") {  // juntos, correm mais
                int perto = 0;
                for (auto [o, po, io] : reg.view<const Posicao, const Inimigo>().each())
                    if (o != e && io.tipo == i.tipo && glm::distance(po.v, p.v) < 40.0f) ++perto;
                if (perto >= 2) vel *= 1.25f;
            }
        } else {
            if (glm::distance(p.v, i.destino) < 3.0f) {
                i.espera -= dt;
                if (i.espera <= 0.0f) {
                    i.destino = i.origem + glm::vec2(u(rng_), u(rng_)) * 40.0f;
                    i.espera = espera(rng_);
                }
                vel = 0.0f;
            }
            vel *= 0.5f;  // vagando, sem pressa
            i.preparo = 0.0f;  // o herói fugiu: o golpe em área se desfaz
        }
        if (i.preparo > 0.0f) {  // preparando o golpe: parado, virado para o herói
            vel = 0.0f;
            if (std::abs(paraHeroi.x) > std::abs(paraHeroi.y)) i.dir = paraHeroi.x < 0 ? Direcao::Esquerda : Direcao::Direita;
        }
        const glm::vec2 dirv = alvo - p.v;
        i.andando = false;
        if (vel > 0.0f && glm::length(dirv) > 2.0f) {
            const glm::vec2 passo = glm::normalize(dirv) * vel * dt;
            const glm::vec2 feito = ke::moverComColisao(mapa, caixa, passo, obst, 3.0f);
            p.v += feito;
            i.andando = glm::length(feito) > 0.01f;
            if (std::abs(dirv.x) > std::abs(dirv.y)) i.dir = dirv.x < 0 ? Direcao::Esquerda : Direcao::Direita;
            else i.dir = dirv.y < 0 ? Direcao::Cima : Direcao::Baixo;
            if (!i.andando && !i.perseguindo) i.destino = p.v;  // travou em algo: escolhe outro rumo depois
        }
        if (i.andando) i.tempoAnim += dt;
        // Encostou no herói: fere (e espera para ferir de novo)
        const float alcance = d.tamanho.x * 0.45f + 6.0f;
        if (dist < alcance && i.recarga <= 0.0f) {
            ferirHeroi(static_cast<int>(std::round(d.dano * forcaInimigos())), p.v);
            i.recarga = 1.0f;
        }
    }
    // 1.16: o bando acorda junto; tiros e brasas nascem agora; os queimados caem
    for (const auto& [tipo, pos] : chamados)
        for (auto [o, po, io] : reg.view<const Posicao, Inimigo>().each())
            if (io.tipo == tipo && !io.rendido && glm::distance(po.v, pos) < 100.0f) io.perseguindo = true;
    for (const auto& [pos, pj] : tiros) {
        const auto t = reg.create();
        reg.emplace<Posicao>(t, pos);
        reg.emplace<Projetil>(t, pj);
        eventos_.push_back({EventoJogo::Tipo::Tiro, pos, pj.tipo});
    }
    for (const glm::vec2& pos : brasas) {
        const auto b = reg.create();
        reg.emplace<Posicao>(b, pos);
        reg.emplace<Brasa>(b);
    }
    for (const entt::entity q : queimados)
        if (reg.valid(q)) derrotarInimigo(q);
    if (!queimados.empty()) atualizarMissoes();
    // Fase 13: quem chegou à praça leva o saque e some; soldado ferido sai da batalha
    for (const entt::entity e : saqueadores) {
        saquear(reg.get<Posicao>(e).v);
        reg.destroy(e);
    }
    for (const entt::entity s : soldadosCaidos) {
        eventos_.push_back({EventoJogo::Tipo::SoldadoCaiu, reg.get<Posicao>(s).v, "", "", 0, "Um soldado caiu ferido."});
        reg.destroy(s);
    }
}

void World::reviver() {
    // Derrotado: alguém do vilarejo o leva para casa; acorda no dia seguinte
    derrotaPendente_ = false;
    dormir(true);
    ultimoRelatorio.notas.insert(ultimoRelatorio.notas.begin(), "Você caiu em combate. Oswin o trouxe de volta para casa.");
}

// 1.16: os tiros voam até acertar o herói (que pode esquivar), bater em algo sólido ou acabar o alcance
void World::atualizarProjeteis(float dt) {
    std::vector<entt::entity> fim;
    for (auto [e, p, pj] : reg.view<Posicao, Projetil>().each()) {
        p.v += pj.vel * dt;
        pj.vida -= dt;
        const glm::ivec2 t = glm::ivec2(glm::floor(p.v / static_cast<float>(LADO)));
        const bool solido = t.x < 0 || t.y < 0 || t.x >= mapa.largura() || t.y >= mapa.altura() || mapa.em(t.x, t.y).solido;
        if (glm::distance(p.v, jogador.pos - glm::vec2(0, 7)) < 8.0f && jogador.vida > 0) {
            ferirHeroi(pj.dano, p.v - pj.vel * 0.05f);
            fim.push_back(e);
        } else if (pj.vida <= 0.0f || solido) {
            fim.push_back(e);
        }
    }
    for (const entt::entity e : fim) reg.destroy(e);
}

// 1.16: o chão em brasa queima quem fica em cima (a esquiva protege)
void World::atualizarBrasas(float dt) {
    std::vector<entt::entity> fim;
    for (auto [e, p, b] : reg.view<const Posicao, Brasa>().each()) {
        b.vida -= dt;
        b.tick -= dt;
        if (b.tick <= 0.0f && glm::distance(p.v, jogador.pos) < b.raio) {
            b.tick = 0.6f;
            ferirHeroi(5, p.v);
        }
        if (b.vida <= 0.0f) fim.push_back(e);
    }
    for (const entt::entity e : fim) reg.destroy(e);
}
}
