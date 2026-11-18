// Parte do World: hordas e a defesa do portão leste (fase 13)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Fase 13: hordas e a defesa do portão leste ----------

const BuildingData* World::muralha() const {
    return porFuncao("muralha");
}

int World::portaoVidaMax() const {
    const BuildingData* m = muralha();
    const BuildingDef* d = m ? def(*m) : nullptr;
    const NivelConstrucao* nv = d ? d->nivel(m->level) : nullptr;
    return nv && m->constructed ? static_cast<int>(nv->efeito("vida_portao")) : 0;
}

ke::AABB World::caixaPortao() const {
    const BuildingData* m = muralha();
    if (!m) return {{-100, -100}, {-99, -99}};
    // O vão do portão são a 4ª e a 5ª fileiras da muralha (a estrada)
    const ke::AABB c = caixaConstrucao(*m);
    return {{c.min.x, c.min.y + 3 * LADO}, {c.max.x, c.min.y + 5 * LADO}};
}

int World::danoTorres() const { return static_cast<int>(efeitoTotal("dano_torre")); }

int World::soldadosDosPostos() const {
    int n = static_cast<int>(efeitoTotal("soldados"));
    if (n > 0 && efeitoCargo("marechal") > 0.0f) ++n;  // o Marechal leva mais um para o portão
    n += efeitosAlas().soldados;                        // fase 16: o Quartel Real manda o seu
    return n;
}

int World::criaturasDaHorda() const {
    int n = 0;
    for (auto [e, i] : reg.view<const Inimigo>().each()) n += i.horda;
    return n;
}

int World::soldadosNoPortao() const {
    int n = 0;
    for (auto e : reg.view<const Soldado>()) n += e != entt::null;
    return n;
}

int World::forcaProximaHorda() const {
    const auto it = estadoRegioes.find("floresta");
    int f = forcaHorda(dados().defesa, hordaNumero + 1, it == estadoRegioes.end() ? 70.0f : it->second.corrupcao);
    if (const EventoDef* ev = eventoDoDia(hordaDia > 0 ? hordaDia : relogio.dia); ev && ev->id == "lua_sangue")
        f = f * 3 / 2;  // horda em noite de Lua de Sangue vem maior
    // Fase 18: na neblina a horda chega escondida (e maior)
    f = static_cast<int>(std::round(f * tempoDoDia(hordaDia > 0 ? hordaDia : relogio.dia).forcaHorda));
    return f;
}

int World::defesaDoReino() const {
    return portaoVida / 8 + danoTorres() * 3 + soldadosDosPostos() * 6 + resumo().seguranca / 2;
}

void World::agendarHorda(int dia) { hordaDia = std::max(dia, relogio.dia); }

void World::iniciarBatalha() {
    if (batalha_ || !noVilarejo()) return;
    const DadosDefesa& dd = dados().defesa;
    const int forca = forcaProximaHorda();
    ++hordaNumero;
    batalha_ = true;
    vencidasNaBatalha_ = saquesNaBatalha_ = 0;
    recargaTorre_.clear();
    // As criaturas saem da estrada leste, em fileiras
    const auto criaturas = composicaoHorda(dd, forca);
    for (size_t k = 0; k < criaturas.size(); ++k) {
        const glm::ivec2 t = dd.entrada[k % dd.entrada.size()];
        const float atras = static_cast<float>(k / dd.entrada.size()) * 10.0f;
        const glm::vec2 p{std::min(t.x * LADO + LADO / 2.0f + atras, mapa.largura() * LADO - 4.0f), t.y * LADO + 12.0f};
        reg.get<Inimigo>(criarInimigoEm(criaturas[k], p)).horda = true;
    }
    // Os soldados dos postos correm para o portão
    const SoldadoDef& sd = dd.soldado;
    const int nSoldados = soldadosDosPostos();
    for (int k = 0; k < nSoldados && !dd.postoSoldados.empty(); ++k) {
        const glm::ivec2 t = dd.postoSoldados[k % dd.postoSoldados.size()];
        const glm::vec2 p{t.x * LADO + LADO / 2.0f - (k / static_cast<int>(dd.postoSoldados.size())) * 12.0f, t.y * LADO + 12.0f};
        const auto e = reg.create();
        reg.emplace<Posicao>(e, p);
        Soldado& s = reg.emplace<Soldado>(e);
        s.vida = s.vidaMax = sd.vida;
        s.posto = p;
        s.dir = Direcao::Direita;
    }
    eventos_.push_back({EventoJogo::Tipo::HordaChegou, jogador.pos, "", "", static_cast<int>(criaturas.size()),
                        std::format("Uma horda de {} criaturas chegou ao portão leste!", criaturas.size())});
}

void World::atualizarSoldados(float dt) {
    const SoldadoDef& sd = dados().defesa.soldado;
    const auto& obst = obstaculos();
    std::vector<entt::entity> mortos;
    for (auto [e, p, s] : reg.view<Posicao, Soldado>().each()) {
        s.recarga = std::max(0.0f, s.recarga - dt);
        s.piscar = std::max(0.0f, s.piscar - dt);
        // A criatura da horda mais perto (dentro da vista); sem nenhuma, volta para o posto
        entt::entity alvo = entt::null;
        float dist = sd.visao;
        for (auto [ie, ip, ii] : reg.view<const Posicao, const Inimigo>().each())
            if (ii.horda && ii.vida > 0 && glm::distance(ip.v, p.v) < dist) dist = glm::distance(ip.v, p.v), alvo = ie;
        const glm::vec2 destino = alvo != entt::null ? reg.get<Posicao>(alvo).v : s.posto;
        const glm::vec2 dirv = destino - p.v;
        s.andando = false;
        if (glm::length(dirv) > (alvo != entt::null ? 12.0f : 3.0f)) {
            const ke::AABB caixa{p.v - glm::vec2(4, 4), p.v + glm::vec2(4, 1)};
            const glm::vec2 feito = ke::moverComColisao(mapa, caixa, glm::normalize(dirv) * sd.velocidade * dt, obst, 3.0f);
            p.v += feito;
            s.andando = glm::length(feito) > 0.01f;
            if (std::abs(dirv.x) > std::abs(dirv.y)) s.dir = dirv.x < 0 ? Direcao::Esquerda : Direcao::Direita;
            else s.dir = dirv.y < 0 ? Direcao::Cima : Direcao::Baixo;
        }
        if (s.andando) s.tempoAnim += dt;
        // Lança na criatura encostada
        if (alvo != entt::null && dist < 16.0f && s.recarga <= 0.0f) {
            Inimigo& i = reg.get<Inimigo>(alvo);
            const int dano = danoContra(sd.dano, dados().inimigos.at(i.tipo).defesa);
            i.vida -= dano;
            i.piscar = 0.12f;
            s.recarga = sd.recarga;
            eventos_.push_back({EventoJogo::Tipo::AcertouInimigo, reg.get<Posicao>(alvo).v - glm::vec2(0, 8), i.tipo, "", dano});
            if (i.vida <= 0 && std::ranges::find(mortos, alvo) == mortos.end()) mortos.push_back(alvo);
        }
    }
    for (const entt::entity e : mortos) derrotarInimigo(e);
}

void World::atualizarDefesa(float dt) {
    const DadosDefesa& dd = dados().defesa;
    // Torres: uma flecha na criatura mais perto, a cada poucos segundos
    std::vector<entt::entity> mortos;
    for (const auto& b : reino.construcoes) {
        const BuildingDef* d = def(b);
        const NivelConstrucao* nv = d && b.constructed ? d->nivel(b.level) : nullptr;
        if (!nv || nv->efeito("dano_torre") <= 0.0f) continue;  // só o que atira (as torres)
        float& rc = recargaTorre_[b.id];
        rc -= dt;
        if (rc > 0.0f) continue;
        const ke::AABB c = caixaConstrucao(b);
        const glm::vec2 origem{(c.min.x + c.max.x) * 0.5f, c.min.y - 12.0f};
        entt::entity alvo = entt::null;
        float dist = dd.alcanceTorre;
        for (auto [ie, ip, ii] : reg.view<const Posicao, const Inimigo>().each())
            if (ii.horda && ii.vida > 0 && glm::distance(ip.v, origem) < dist) dist = glm::distance(ip.v, origem), alvo = ie;
        if (alvo == entt::null) continue;
        Inimigo& i = reg.get<Inimigo>(alvo);
        const glm::vec2 p = reg.get<Posicao>(alvo).v;
        const int dano = danoContra(static_cast<int>(nv->efeito("dano_torre")), dados().inimigos.at(i.tipo).defesa);
        i.vida -= dano;
        i.piscar = 0.12f;
        rc = dd.recargaTorre;
        eventos_.push_back({EventoJogo::Tipo::Flecha, origem, b.id, "", dano, "", p - glm::vec2(0, 8)});
        eventos_.push_back({EventoJogo::Tipo::AcertouInimigo, p - glm::vec2(0, 8), i.tipo, "", dano});
        if (i.vida <= 0 && std::ranges::find(mortos, alvo) == mortos.end()) mortos.push_back(alvo);
    }
    for (const entt::entity e : mortos) derrotarInimigo(e);
    atualizarSoldados(dt);
    if (criaturasDaHorda() == 0) terminarBatalha();
}

void World::saquear(glm::vec2 onde) {
    const DadosDefesa& dd = dados().defesa;
    for (const auto& [r, q] : dd.saque) inventario.remover(r, std::min(q, inventario.quantidade(r)));
    estado.felicidade = std::max(0.0f, estado.felicidade - dd.saqueFelicidade);
    ++saquesNaBatalha_;
    eventos_.push_back({EventoJogo::Tipo::Saque, onde, "", "", 0, "Uma criatura saqueou a praça!"});
}

void World::terminarBatalha() {
    const DadosDefesa& dd = dados().defesa;
    batalha_ = false;
    std::vector<entt::entity> soldados;
    for (auto e : reg.view<Soldado>()) soldados.push_back(e);
    reg.destroy(soldados.begin(), soldados.end());
    const int bonus = vencidasNaBatalha_ * dd.ouroPorCriatura;
    inventario.adicionar("ouro", bonus);
    const bool limpa = saquesNaBatalha_ == 0;
    std::string texto = limpa ? std::format("Batalha do Portão: vitória! {} criaturas vencidas; o povo juntou {} de ouro para os defensores.",
                                            vencidasNaBatalha_, bonus)
                              : std::format("Batalha do Portão: a horda foi vencida, mas {} criatura{} saqueou a praça.", saquesNaBatalha_,
                                            saquesNaBatalha_ > 1 ? "s" : "");
    notasDefesa.push_back(texto);
    if (portaoVidaMax() > 0 && portaoVida < portaoVidaMax())
        notasDefesa.push_back(std::format("O portão ficou com {} de {} de vida. Repare no painel da muralha.", portaoVida, portaoVidaMax()));
    marcar(std::string(limpa ? "horda:" : "horda_saqueada:") + std::to_string(hordaNumero), std::to_string(relogio.dia));
    eventos_.push_back({EventoJogo::Tipo::BatalhaFim, jogador.pos, limpa ? "vitoria" : "saque", "ouro", bonus, texto});
    hordaDia = 0;
    proximaHorda = relogio.dia + dd.intervaloMin + static_cast<int>(hashTile(hordaNumero, 77) % (dd.intervaloMax - dd.intervaloMin + 1));
}

void World::resolverHordaSemHeroi(const std::string& motivo) {
    const DadosDefesa& dd = dados().defesa;
    // A força é a das criaturas que ainda estão na batalha, ou a da horda inteira (se ela nem começou com o herdeiro)
    int forca = 0;
    if (batalha_) {
        std::vector<entt::entity> v;
        for (auto [e, i] : reg.view<const Inimigo>().each())
            if (i.horda) {
                v.push_back(e);
                for (const auto& [tipo, pontos] : dd.custo)
                    if (tipo == i.tipo) forca += pontos;
            }
        reg.destroy(v.begin(), v.end());
        std::vector<entt::entity> soldados;
        for (auto e : reg.view<Soldado>()) soldados.push_back(e);
        reg.destroy(soldados.begin(), soldados.end());
    } else {
        forca = forcaProximaHorda();
        ++hordaNumero;
        vencidasNaBatalha_ = saquesNaBatalha_ = 0;
    }
    batalha_ = false;
    const ResultadoHorda r = resolverSemHeroi(defesaDoReino(), forca * 6, portaoVida);
    portaoVida = std::max(0, portaoVida - r.danoPortao);
    for (int k = 0; k < r.invasores; ++k) saquear(glm::vec2(dd.praca) * static_cast<float>(LADO));
    const char* onde = motivo == "longe" || motivo == "saiu" ? "Com o herdeiro longe, a horda atacou o portão leste."
                                                             : "A horda atacou o portão leste enquanto o herdeiro dormia.";
    notasDefesa.push_back(r.vitoria ? std::string(onde) + " As defesas seguraram sozinhas."
                                    : std::format("{} As defesas não bastaram: {} criatura{} saqueou a praça.", onde, saquesNaBatalha_,
                                                  saquesNaBatalha_ > 1 ? "s" : ""));
    if (portaoVidaMax() > 0 && portaoVida < portaoVidaMax())
        notasDefesa.push_back(std::format("O portão ficou com {} de {} de vida. Repare no painel da muralha.", portaoVida, portaoVidaMax()));
    marcar(std::string(saquesNaBatalha_ == 0 ? "horda:" : "horda_saqueada:") + std::to_string(hordaNumero), std::to_string(relogio.dia));
    if (motivo == "longe" || motivo == "saiu")
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, r.vitoria ? "bom" : "", "", 0,
                            r.vitoria ? "A horda atacou Valdren; as defesas seguraram sem você." : "A horda atacou Valdren sem você e saqueou a praça!"});
    hordaDia = 0;
    proximaHorda = relogio.dia + dd.intervaloMin + static_cast<int>(hashTile(hordaNumero, 77) % (dd.intervaloMax - dd.intervaloMin + 1));
}

void World::manhaDaDefesa() {
    const DadosDefesa& dd = dados().defesa;
    auto& notas = ultimoRelatorio.notas;
    for (const auto& n : notasDefesa) notas.push_back(n);
    notasDefesa.clear();
    // O Mestre das Obras conserta parte do portão durante a noite
    const int max = portaoVidaMax();
    if (portaoVida < max && efeitoCargo("obras") > 0.0f) {
        const int conserto = std::max(1, static_cast<int>(std::round((max - portaoVida) * dd.consertoMestreObras)));
        portaoVida = std::min(max, portaoVida + conserto);
        notas.push_back(std::format("O Mestre das Obras consertou parte do portão (+{} de vida).", conserto));
    }
    // Os batedores avistam a próxima horda
    if (hordaDia == 0 && relogio.dia >= std::max(dd.diaMin, proximaHorda) && estado.populacao >= dd.populacaoMin) {
        const int aviso = dd.avisoDias + (efeitoCargo("marechal") > 0.0f ? 1 : 0);  // o Marechal tem batedores melhores
        agendarHorda(relogio.dia + aviso);
        notas.push_back(std::format("Batedores avistaram uma horda na floresta: ela ataca o portão leste em {} dias, às {}:00. "
                                    "Reforce a muralha, as torres e os postos da guarda.",
                                    aviso, dd.hora));
        marcar("horda_avisada");
    } else if (hordaDia == relogio.dia) {
        notas.push_back(std::format("A horda chega hoje, às {}:00! Fique no vilarejo para lutar no portão.", dd.hora));
    }
}

Custo World::custoReparo() const {
    const int falta = portaoVidaMax() - portaoVida;
    if (falta <= 0) return {};
    return {{"pedra", (falta + 9) / 10}, {"madeira", (falta + 19) / 20}};
}

bool World::repararPortao() {
    const Custo c = custoReparo();
    if (c.empty() || batalha_) return false;
    if (!inventario.pagar(c)) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Faltam pedra e madeira para o reparo."});
        return false;
    }
    portaoVida = portaoVidaMax();
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, "O portão está inteiro de novo."});
    return true;
}

}
