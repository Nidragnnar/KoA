// Parte do World: a linhagem do herdeiro (casamento, filhos, tutores, envelhecimento e sucessão) (fase 21)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

void World::iniciarLinhagem() {
    const DadosLinhagem& d = dados().linhagem;
    linhagem = {};
    Membro f;
    f.id = linhagem.proximoId++;
    f.nome = d.fundadorNome;
    f.tracos = d.fundadorTracos;
    f.reinado = 1;
    f.reinou = 1;
    linhagem.membros.push_back(std::move(f));
    linhagem.atual = 1;
    linhagem.inicioReinado = 1;
    aplicarTracos();
}

const Membro& World::herdeiro() const {
    static const Membro vazio;
    const Membro* m = linhagem.membro(linhagem.atual);
    return m ? *m : vazio;
}

EfeitosTracos World::efeitosHerdeiro() const { return dados_ ? efeitosDe(dados().linhagem, herdeiro().tracos) : EfeitosTracos{}; }

void World::aplicarTracos() {
    jogador.vidaMax = std::max(50, 100 + efeitosHerdeiro().vida);
    jogador.vida = std::clamp(jogador.vida, 1, jogador.vidaMax);
}

int World::afinidadeCom(const std::string& npc) const {
    for (auto [e, m, rel] : reg.view<const Morador, const Relacionamento>().each())
        if (m.id == npc) return rel.afinidade;
    for (const auto& [id, g] : guardadas_)
        for (auto [e, m, rel] : g.reg.view<const Morador, const Relacionamento>().each())
            if (m.id == npc) return rel.afinidade;
    return 0;
}

std::string World::nomeConjuge(const ConjugeDef& c) const {
    if (!c.npc.empty()) return dados().npcs.contains(c.npc) ? dados().npcs.at(c.npc).nome : c.npc;
    return c.nome;
}

std::string World::podeCasar(const std::string& id) const {
    const DadosLinhagem& d = dados().linhagem;
    const ConjugeDef* c = d.conjuge(id);
    if (!c) return "Ninguém com esse nome.";
    if (const Membro* atual = linhagem.membro(herdeiro().conjuge); atual && atual->vivo) return "O herdeiro já tem cônjuge.";
    if (std::ranges::any_of(linhagem.membros, [&](const Membro& m) { return !c->npc.empty() ? m.npc == c->npc : m.faccao == c->faccao; }))
        return "Já faz parte da família.";
    if (!c->npc.empty()) {
        if (const int a = afinidadeCom(c->npc); a < d.amizadeMin)
            return std::format("Pede {} corações de amizade (hoje {}).", d.amizadeMin / 100, a / 100);
    } else if (const float r = relacaoCom(c->faccao); r < d.relacaoMin) {
        return std::format("Pede relação {:.0f} com {} (hoje {:.0f}).", d.relacaoMin, dados().faccoes.faccao(c->faccao)->nome, r);
    }
    if (!inventario.tem(d.festa)) return "Faltam recursos para a festa.";
    return "";
}

bool World::casar(const std::string& id) {
    const std::string falta = podeCasar(id);
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    const DadosLinhagem& d = dados().linhagem;
    const ConjugeDef& c = *d.conjuge(id);
    inventario.pagar(d.festa);
    for (const auto& [r, q] : c.dote) inventario.adicionar(r, q);
    Membro m;
    m.id = linhagem.proximoId++;
    m.nome = nomeConjuge(c);
    m.tracos = {c.traco};
    m.npc = c.npc;
    m.faccao = c.faccao;
    m.conjuge = linhagem.atual;
    m.reinado = linhagem.reinado;
    linhagem.membro(linhagem.atual)->conjuge = m.id;
    linhagem.membros.push_back(m);
    linhagem.proximoNascimento = relogio.dia + d.gestacao;
    estado.felicidade = std::min(100.0f, estado.felicidade + 5.0f);  // festa na praça
    if (!c.npc.empty()) {
        if (Relacionamento* rel = relacao(c.npc)) rel->afinidade = std::min(1000, rel->afinidade + 100);
    } else {
        // Casamento político: o povo da noiva (ou do noivo) vira família; o rival não gosta
        mudarRelacao(c.faccao, 15.0f);
        for (const auto& [a, b, perda] : dados().faccoes.rivais) {
            if (c.faccao == a) mudarRelacao(b, -perda);
            if (c.faccao == b) mudarRelacao(a, -perda);
        }
    }
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0,
                        std::format("{} e {} se casaram. Valdren inteira foi à festa (+5 de felicidade).", herdeiro().nome, m.nome)});
    marcar("casamento:" + id, m.nome);
    marcar("casou", m.nome, true);
    return true;
}

bool World::definirTutor(int filho, const std::string& cargo) {
    Membro* m = linhagem.membro(filho);
    if (!m || m->adulto || !m->vivo) return false;
    if (!cargo.empty() && (!dados().linhagem.tutores.contains(cargo) || ocupante(cargo).empty())) return false;
    m->tutor = cargo;
    return true;
}

std::string World::podeSuceder(int filho) const {
    const Membro* m = linhagem.membro(filho);
    if (!m || !m->vivo) return "Ninguém com esse nome.";
    if (m->genitor != linhagem.atual && m->outroGenitor != linhagem.atual) return "Só um filho de quem reina herda a coroa.";
    if (!m->adulto) return std::format("Ainda é criança (adulto em {} dias).", std::max(0, m->nasceu + dados().linhagem.maioridade - relogio.dia));
    return "";
}

int World::maisVelho() const {
    for (const Membro* f : linhagem.filhosDe(linhagem.atual))  // já vêm na ordem do nascimento
        if (f->vivo && f->adulto) return f->id;
    return 0;
}

bool World::passarCoroa(int filho, bool natural) {
    const std::string falta = podeSuceder(filho);
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    const DadosLinhagem& d = dados().linhagem;
    const int velho = maisVelho();
    Membro& antigo = *linhagem.membro(linhagem.atual);
    Membro& novo = *linhagem.membro(filho);
    const int anosReinou = (relogio.dia - linhagem.inicioReinado) / 28 + 1;
    antigo.vivo = false;
    // O conselho: quem educou o novo herdeiro fica feliz; pular o mais velho incomoda
    for (const auto& [cargo, npc] : conselho) {
        if (npc.empty()) continue;
        const int delta = novo.tutor == cargo ? 10 : filho == velho ? 3 : -6;
        lealdade[npc] = std::clamp(lealdade[npc] + delta, 0, 100);
    }
    // Os vizinhos: o que cada povo acha do jeito de quem sobe
    for (const auto& t : novo.tracos)
        if (const auto it = d.reacoesTraco.find(t); it != d.reacoesTraco.end())
            for (const auto& [f, v] : it->second) mudarRelacao(f, v);
    // O salto: os anos passam; o povo esquece um pouco a fama antiga e os vizinhos voltam um pouco ao que eram
    linhagem.anosSaltados += d.saltoAnos;
    const float m = d.saltoMemoria;
    reputacao.justica *= 1.0f - m;
    reputacao.generosidade *= 1.0f - m;
    reputacao.devocao *= 1.0f - m;
    for (const FaccaoDef& f : dados().faccoes.faccoes) relacoes[f.id] += (f.inicial - relacoes[f.id]) * m;
    desvios.clear();
    suspeito.clear();
    mercado.pressao.clear();
    linhagem.atual = filho;
    ++linhagem.reinado;
    novo.reinou = linhagem.reinado;
    linhagem.inicioReinado = relogio.dia;
    linhagem.proximoNascimento = 0;
    aplicarTracos();
    jogador.vida = jogador.vidaMax;
    const std::string texto = std::format("{} {} depois de {} ano{} de reinado. {} anos se passaram: {} é {} de Valdren ({}).", antigo.nome,
                                          natural ? "morreu em paz" : "passou a coroa", anosReinou, anosReinou == 1 ? "" : "s", d.saltoAnos, novo.nome,
                                          linhagem.reinado == 2 ? "a segunda geração" : "o novo herdeiro", textoTracos(d, novo.tracos));
    ultimoRelatorio.notas.push_back(texto);
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, texto});
    marcar(std::format("sucessao:{}", linhagem.reinado), novo.nome);
    return true;
}

// Depois da virada do dia: nascimentos, maioridade, velhice e a morte natural de quem reinou demais
void World::noiteDaLinhagem(std::vector<std::string>& notas) {
    const DadosLinhagem& d = dados().linhagem;
    const Membro& h = herdeiro();
    const std::string nomeHerdeiro = h.nome;  // h e c deixam de valer quando a família cresce (o vetor muda de lugar)
    const Membro* c = linhagem.membro(h.conjuge);
    if (c && c->vivo && linhagem.proximoNascimento > 0 && relogio.dia >= linhagem.proximoNascimento) {
        const auto filhos = std::ranges::count_if(linhagem.filhosDe(h.id), [&](const Membro* f) { return f->outroGenitor == c->id; });
        if (filhos < d.filhosMax) {
            Membro f;
            f.id = linhagem.proximoId++;
            const auto& nomes = dados().povo.nomes;
            f.nome = nomes.empty() ? std::format("Criança {}", f.id) : nomes[misturar(sementeBase_, static_cast<uint32_t>(f.id) * 977u) % nomes.size()];
            f.genitor = h.id;
            f.outroGenitor = c->id;
            f.nasceu = relogio.dia;
            f.reinado = linhagem.reinado;
            f.adulto = false;
            notas.push_back(std::format("Nasceu {}, filho de {} e {}. Escolha um tutor no conselho (Y: a família).", f.nome, h.nome, c->nome));
            const std::string nome = f.nome;
            const int id = f.id;
            linhagem.membros.push_back(std::move(f));
            linhagem.proximoNascimento = filhos + 1 < d.filhosMax ? relogio.dia + d.entreFilhos : 0;
            marcar(std::format("nasceu:{}", id), nome);
            marcar("primeiro_filho", nome, true);
        } else {
            linhagem.proximoNascimento = 0;
        }
    }
    for (Membro& m : linhagem.membros) {
        if (m.adulto || !m.vivo || relogio.dia - m.nasceu < d.maioridade) continue;
        // Maioridade: um traço herdado e um do tutor (se ele for leal)
        std::string doTutor;
        if (!m.tutor.empty()) {
            const std::string npc = ocupante(m.tutor);
            const auto it = lealdade.find(npc);
            if (!npc.empty() && it != lealdade.end() && it->second >= d.tutorLealdade && d.tutores.contains(m.tutor)) doTutor = d.tutores.at(m.tutor);
        }
        const Membro* g1 = linhagem.membro(m.genitor);
        const Membro* g2 = linhagem.membro(m.outroGenitor);
        m.tracos = tracosDaMaioridade(d, g1 ? g1->tracos : std::vector<std::string>{}, g2 ? g2->tracos : std::vector<std::string>{}, doTutor,
                                      misturar(sementeBase_, static_cast<uint32_t>(m.id) * 7919u));
        m.adulto = true;
        notas.push_back(std::format("{} chegou à maioridade: {}.", m.nome, textoTracos(d, m.tracos)));
        marcar(std::format("maioridade:{}", m.id), m.nome);
    }
    // Velhice: um aviso; no fim, a coroa passa sozinha para o filho adulto mais velho
    const int dias = relogio.dia - linhagem.inicioReinado;
    if (dias >= d.reinadoVelho && marcar(std::format("envelheceu:{}", linhagem.reinado), nomeHerdeiro))
        notas.push_back(std::format("{} já sente o peso dos anos. É hora de pensar em quem herda a coroa.", nomeHerdeiro));
    if (dias >= d.reinadoMax)
        if (const int f = maisVelho()) passarCoroa(f, true);
    // Traço devoto: a fama de devoto cresce um pouco por noite
    if (const float dv = efeitosHerdeiro().devocaoNoite; dv != 0.0f) {
        reputacao.devocao = std::clamp(reputacao.devocao + dv, -100.0f, 100.0f);
        verificarFama();
    }
}

}
