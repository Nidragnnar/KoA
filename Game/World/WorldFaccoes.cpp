// Parte do World: os povos vizinhos, os tratados, a reputação do herdeiro e a intriga no conselho (fase 19)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

void World::iniciarFaccoes() {
    relacoes.clear();
    for (const FaccaoDef& f : dados().faccoes.faccoes) relacoes[f.id] = f.inicial;
    tratados.clear();
    reputacao = {};
    desvios.clear();
    suspeito.clear();
}

float World::relacaoCom(const std::string& faccao) const {
    const auto it = relacoes.find(faccao);
    return it == relacoes.end() ? 0.0f : it->second;
}

std::string World::faixaCom(const std::string& faccao) const { return dados().faccoes.faixa(relacaoCom(faccao)); }

void World::mudarRelacao(const std::string& faccao, float delta) {
    const FaccaoDef* f = dados().faccoes.faccao(faccao);
    if (!f || delta == 0.0f) return;
    float& r = relacoes[faccao];
    r = std::clamp(r + delta, -100.0f, 100.0f);
    if (r >= 50.0f) marcar("faccao_aliada:" + faccao, f->nome);
    if (r <= -50.0f) marcar("faccao_hostil:" + faccao, f->nome);
}

// Quem governa assim fica com essa fama (e a fama abre títulos e falas)
void World::verificarFama() {
    const DadosFaccoes& df = dados().faccoes;
    for (const auto& [eixo, par] : df.marcosFama) {
        const float v = reputacao.eixo(eixo);
        if (v >= df.limiteFama) marcar(par.first);
        if (v <= -df.limiteFama) marcar(par.second);
    }
}

void World::reagirAoMarco(const std::string& marco) {
    const ReacaoMarco r = dados().faccoes.reacaoA(marco);
    for (const auto& [f, v] : r.faccoes) mudarRelacao(f, v);
    if (r.reputacao.justica == 0.0f && r.reputacao.generosidade == 0.0f && r.reputacao.devocao == 0.0f) return;
    reputacao.justica = std::clamp(reputacao.justica + r.reputacao.justica, -100.0f, 100.0f);
    reputacao.generosidade = std::clamp(reputacao.generosidade + r.reputacao.generosidade, -100.0f, 100.0f);
    reputacao.devocao = std::clamp(reputacao.devocao + r.reputacao.devocao, -100.0f, 100.0f);
    verificarFama();
}

EfeitosTratado World::efeitosTratados() const {
    EfeitosTratado e;
    for (const auto& id : tratados)
        if (const TratadoDef* t = dados().faccoes.tratado(id)) e += t->efeitos;
    return e;
}

std::string World::podeAssinar(const std::string& id) const {
    const TratadoDef* t = dados().faccoes.tratado(id);
    if (!t) return "Tratado desconhecido.";
    if (tratados.contains(id)) return "Já assinado.";
    if (relacaoCom(t->faccao) < static_cast<float>(t->requer))
        return std::format("Pede relação {} (hoje {:.0f}).", t->requer, relacaoCom(t->faccao));
    if (!inventario.tem(t->custo)) return "Faltam recursos.";
    return "";
}

bool World::assinarTratado(const std::string& id) {
    const TratadoDef* t = dados().faccoes.tratado(id);
    const std::string falta = podeAssinar(id);
    if (!t || !falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    inventario.pagar(t->custo);
    tratados.insert(id);
    // Quem é rival de quem assinou não gosta nada disso
    for (const auto& [a, b, perda] : dados().faccoes.rivais) {
        if (t->faccao == a) mudarRelacao(b, -perda);
        if (t->faccao == b) mudarRelacao(a, -perda);
    }
    mudarRelacao(t->faccao, 5.0f);
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, "Tratado assinado: " + t->nome + "."});
    marcar("tratado:" + id, t->nome);
    return true;
}

bool World::romperTratado(const std::string& id) {
    const TratadoDef* t = dados().faccoes.tratado(id);
    if (!t || !tratados.erase(id)) return false;
    mudarRelacao(t->faccao, -15.0f);
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Tratado rompido: " + t->nome + "."});
    marcar("rompeu:" + id, t->nome);
    return true;
}

bool World::aceitarMissaoFaccao(const std::string& id) {
    for (const MissaoDef& m : dados().missoes) {
        if (m.id != id || m.faccao.empty()) continue;
        if (missoes.contains(id)) return false;
        if (relacaoCom(m.faccao) <= -50.0f) {
            eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Eles não confiam em Valdren para isso."});
            return false;
        }
        iniciarMissao(m);
        return true;
    }
    return false;
}

void World::noiteDasFaccoes(std::vector<std::string>& notas) {
    const DadosFaccoes& df = dados().faccoes;
    // O jeito de cobrar impostos também é fama
    if (estado.imposto == Imposto::Alto) reputacao.justica -= 0.5f, reputacao.generosidade -= 0.4f;
    if (estado.imposto == Imposto::Baixo) reputacao.justica += 0.2f, reputacao.generosidade += 0.3f;
    reputacao.justica = std::clamp(reputacao.justica, -100.0f, 100.0f);
    reputacao.generosidade = std::clamp(reputacao.generosidade, -100.0f, 100.0f);
    verificarFama();
    for (size_t k = 0; k < df.faccoes.size(); ++k) {
        const FaccaoDef& f = df.faccoes[k];
        // Devagar, a relação anda para o que a facção acha do jeito de governar
        const float alvo = std::clamp(f.gosta.justica * reputacao.justica + f.gosta.generosidade * reputacao.generosidade +
                                          f.gosta.devocao * reputacao.devocao,
                                      -100.0f, 100.0f);
        relacoes[f.id] += (alvo - relacoes[f.id]) * 0.02f;
        // Emissários: de vez em quando, uma proposta que já dá para aceitar
        if ((relogio.dia + static_cast<int>(k) * 2) % 6 == 0)
            for (const TratadoDef& t : df.tratados)
                if (t.faccao == f.id && !tratados.contains(t.id) && relacaoCom(f.id) >= t.requer) {
                    notas.push_back(std::format("Um emissário de {} propõe o tratado {} (T para ver).", f.nome, t.nome));
                    break;
                }
        // O Culto hostil suja a praça de cinzas
        if (f.id == "culto" && relacaoCom(f.id) <= -50.0f && relogio.dia % 3 == 0) {
            estado.felicidade = std::max(0.0f, estado.felicidade - 2.0f);
            notas.push_back("Símbolos de cinzas apareceram nas portas durante a noite. O povo dormiu mal (-2).");
        }
    }
    // Os tratados: rendem, seguram a relação e caem se ela despencar
    std::vector<std::string> rompidos;
    for (const auto& id : tratados) {
        const TratadoDef* t = df.tratado(id);
        if (!t) continue;
        relacoes[t->faccao] = std::min(100.0f, relacoes[t->faccao] + 0.3f);
        if (relacaoCom(t->faccao) < t->requer - 20.0f) rompidos.push_back(id);
    }
    for (const auto& id : rompidos) {
        const TratadoDef* t = df.tratado(id);
        tratados.erase(id);
        notas.push_back(std::format("{} romperam o tratado {}.", df.faccao(t->faccao)->nome, t->nome));
        marcar("rompeu:" + id, t->nome);
    }
    if (const int c = efeitosTratados().cristalDia; c > 0) {
        inventario.adicionar("cristal", c);
        ultimoRelatorio.variacoes.push_back({"cristal|tratados", c});
    }
    // Rivais se enfrentam: quem está aliado a um lado perde com o outro
    if (relogio.dia % 7 == 3)
        for (const auto& [a, b, perda] : df.rivais) {
            const FaccaoDef *fa = df.faccao(a), *fb = df.faccao(b);
            if (!fa || !fb) continue;
            notas.push_back(std::format("{} e {} se enfrentaram perto de {}.", fa->nome, fb->nome, nomeRegiao(fb->territorio)));
            if (relacaoCom(a) >= 50.0f) mudarRelacao(b, -5.0f);
            if (relacaoCom(b) >= 50.0f) mudarRelacao(a, -5.0f);
        }
    noiteDaIntriga(notas);
}

// Fase 19: conselheiro descontente desvia ouro em silêncio (o relatório não mostra); descobrir depende de olhos leais
void World::noiteDaIntriga(std::vector<std::string>& notas) {
    for (const auto& [cargo, npc] : conselho) {
        const auto it = lealdade.find(npc);
        if (npc.empty() || it == lealdade.end() || it->second >= regra::LEALDADE_INSATISFEITO) continue;
        const int leva = std::min(3 + estado.populacao / 10, inventario.quantidade("ouro"));
        if (leva <= 0) continue;
        inventario.remover("ouro", leva);
        desvios[npc] += leva;
        mudarRelacao("culto", 0.5f);  // o ouro vai parar em algum lugar
    }
    if (desvios.empty() || !decisaoPendente.empty()) return;
    int chance = 20;
    const std::string marechal = ocupante("marechal");
    if (!marechal.empty() && !desvios.contains(marechal) && lealdade[marechal] >= 50) chance += 25;
    if (std::ranges::find(alas, "biblioteca") != alas.end()) chance += 25;
    chance += efeitosHerdeiro().descobrir;  // fase 21: o Astuto
    if (static_cast<int>(misturar(sementeBase_, static_cast<uint32_t>(relogio.dia) * 7919u) % 100u) >= chance) return;
    const auto maior = std::ranges::max_element(desvios, {}, &std::map<std::string, int>::value_type::second);
    suspeito = maior->first;
    const std::string nome = dados().npcs.contains(suspeito) ? dados().npcs.at(suspeito).nome : suspeito;
    notas.push_back(std::format("Faltam {} de ouro no cofre: {} anda desviando. O conselho espera a sua decisão.", maior->second, nome));
    marcar("desvio:" + suspeito, nome);
    if (dados().conselho.decisao("desvio")) {
        decisaoPendente = "desvio";
        ultimaDecisao = relogio.dia;
    }
}

// O que cada saída do caso do desvio faz além dos efeitos dos dados
void World::resolverDesvio(const std::string& opcao) {
    if (suspeito.empty()) return;
    if (opcao == "prender") {
        const std::string cargo = cargoDe(suspeito);
        if (!cargo.empty()) nomear(cargo, "");
        inventario.adicionar("ouro", desvios[suspeito] / 2);
        for (auto& [npc, l] : lealdade)
            if (npc != suspeito) l = std::min(100, l + 5);
        desvios.erase(suspeito);
    } else if (opcao == "perdoar") {
        lealdade[suspeito] = std::min(100, lealdade[suspeito] + 30);
        desvios.erase(suspeito);
    }
    suspeito.clear();
}

}
