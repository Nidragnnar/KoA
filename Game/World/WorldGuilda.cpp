// Parte do World: a guilda, expedições e relíquias (fase 14)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

// ---------- Fase 14: a Guilda dos Aventureiros, expedições e relíquias ----------

int World::nivelGuilda() const { return static_cast<int>(efeitoMaximo("nivel_guilda")); }

namespace {

float efeitoGuilda(const World& w, const char* qual) { return w.efeitoMaximo(qual); }  // aventureiros, relíquias

// Número fixo a partir de um texto e de um dia (o mesmo plano sempre dá o mesmo resultado)
uint32_t sorteio(const std::string& texto, int dia) {
    uint32_t h = static_cast<uint32_t>(dia) * 2246822519u + 374761393u;
    for (const char c : texto) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
    return h ^ (h >> 13);
}

}

int World::vagasReliquias() const { return static_cast<int>(efeitoGuilda(*this, "reliquias")); }

int World::reliquiasExpostas() const {
    return static_cast<int>(std::ranges::count_if(reliquias, [](const EstadoReliquia& r) { return r.exposta; }));
}

const EstadoReliquia* World::estadoReliquia(const std::string& id) const {
    for (const auto& r : reliquias)
        if (r.id == id) return &r;
    return nullptr;
}

bool World::aventureiroPronto(const std::string& id) const {
    const auto it = aventureiros.find(id);
    return it != aventureiros.end() && !it->second.fora && !it->second.sumido && it->second.feridoAte <= relogio.dia;
}

int World::nivelAventureiro(const std::string& id) const {
    const auto it = aventureiros.find(id);
    return it == aventureiros.end() ? 1 : it->second.nivel;
}

std::string World::situacaoAventureiro(const std::string& id) const {
    const auto it = aventureiros.find(id);
    if (it == aventureiros.end()) return "ainda não chegou";
    const EstadoAventureiro& a = it->second;
    if (a.sumido) return "desaparecido";
    if (a.fora) return "em expedição";
    if (a.feridoAte > relogio.dia) {
        const int dias = a.feridoAte - relogio.dia;
        return std::format("ferido ({} dia{})", dias, dias > 1 ? "s" : "");
    }
    return "pronto";
}

bool World::destinoAberto(const DestinoDef& d) const { return d.requer.empty() || marcos.contains(d.requer); }

int World::comidaExpedicao(const DestinoDef& d, int tamanho) const { return dados().guilda.comidaPorDia * d.dias * tamanho; }

int World::margemSemSorte(const DestinoDef& d, const std::vector<std::string>& grupo) const {
    int poder = 0;
    for (const auto& id : grupo)
        if (const AventureiroDef* a = dados().guilda.aventureiro(id)) poder += poderAventureiro(*a, nivelAventureiro(id), d);
    if (!grupo.empty()) poder += efeitosAlas().expedicao;  // fase 16: os mapas da Biblioteca Real
    return poder - d.perigo * 2;
}

bool World::partir(const std::string& destinoId, const std::vector<std::string>& grupo) {
    const DadosGuilda& dg = dados().guilda;
    const DestinoDef* d = dg.destino(destinoId);
    auto falha = [&](std::string m) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, std::move(m)});
        return false;
    };
    if (nivelGuilda() == 0 || !d) return false;
    if (!expedicao.destino.empty()) return falha("Já há uma expedição na estrada.");
    if (!destinoAberto(*d)) return falha("Ninguém sabe o caminho para lá.");
    if (grupo.empty() || static_cast<int>(grupo.size()) > dg.grupoMax) return falha(std::format("Escolha de 1 a {} aventureiros.", dg.grupoMax));
    for (const auto& id : grupo)
        if (!aventureiroPronto(id)) return falha("Alguém do grupo não está pronto.");
    const int comida = comidaExpedicao(*d, static_cast<int>(grupo.size()));
    if (!inventario.remover("comida", comida)) return falha(std::format("A expedição precisa de {} de comida.", comida));
    for (const auto& id : grupo) aventureiros[id].fora = true;
    expedicao = {destinoId, grupo, relogio.dia, relogio.dia + d->dias};
    eventos_.push_back({EventoJogo::Tipo::Partiu, jogador.pos, destinoId, "", d->dias,
                        std::format("A expedição partiu para {}. Volta em {} dias.", d->nome, d->dias)});
    marcar("expedicao:" + destinoId, d->nome);
    return true;
}

void World::ganharReliquia(const std::string& id) {
    const ReliquiaDef* r = dados().guilda.reliquia(id);
    if (!r || estadoReliquia(id)) return;
    reliquias.push_back({id});
    marcar("reliquia:" + id, r->nome);
}

EfeitosReliquia World::efeitosReliquias() const {
    EfeitosReliquia soma;
    auto somar = [&](const EfeitosReliquia& e) {
        soma.felicidade += e.felicidade;
        soma.comida += e.comida;
        soma.luaSangue *= e.luaSangue;
        soma.ouroDia += e.ouroDia;
        soma.seguranca += e.seguranca;
        soma.dano += e.dano;
        soma.defesa += e.defesa;
    };
    if (!dados_) return soma;
    for (const auto& r : reliquias) {
        if (!r.exposta) continue;
        const ReliquiaDef* d = dados().guilda.reliquia(r.id);
        if (!d) continue;
        somar(d->efeitos);
        if (d->amaldicoada && !r.purificada) somar(d->maldicao);
    }
    return soma;
}

bool World::exporReliquia(const std::string& id, bool expor) {
    auto it = std::ranges::find(reliquias, id, &EstadoReliquia::id);
    const ReliquiaDef* d = dados().guilda.reliquia(id);
    if (it == reliquias.end() || !d || it->exposta == expor) return false;
    if (expor && reliquiasExpostas() >= vagasReliquias()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, "Não há mais lugar para expor relíquias. Melhore a guilda."});
        return false;
    }
    it->exposta = expor;
    if (!expor) return true;
    // A maldição se revela quando a relíquia é posta à mostra
    if (d->amaldicoada && !it->purificada && !it->conhecida) {
        it->conhecida = true;
        eventos_.push_back({EventoJogo::Tipo::Reliquia, jogador.pos, id, "maldicao", 0,
                            std::format("{} era amaldiçoada! {}.", d->nome, textoEfeitos(d->maldicao))});
        marcar("maldicao:" + id, d->nome);
    } else {
        eventos_.push_back({EventoJogo::Tipo::Reliquia, jogador.pos, id, "", 0, std::format("{} exposta na guilda: {}.", d->nome, textoEfeitos(d->efeitos))});
    }
    return true;
}

bool World::purificarReliquia(const std::string& id) {
    auto it = std::ranges::find(reliquias, id, &EstadoReliquia::id);
    const ReliquiaDef* d = dados().guilda.reliquia(id);
    if (it == reliquias.end() || !d || !d->amaldicoada || it->purificada) return false;
    auto falha = [&](const char* m) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, m});
        return false;
    };
    if (!marcos.contains("purificou:floresta")) return falha("Maelis só conhece o rito depois do Coração da Floresta.");
    if (!inventario.remover("cristal", 3)) return falha("O rito de Maelis pede 3 cristais.");
    it->purificada = it->conhecida = true;
    eventos_.push_back({EventoJogo::Tipo::Reliquia, jogador.pos, id, "purificada", 0, std::format("Maelis purificou {}. A maldição se foi.", d->nome)});
    marcar("purificou_reliquia:" + id, d->nome);
    return true;
}

void World::voltaDaExpedicao(std::vector<std::string>& notas) {
    const DadosGuilda& dg = dados().guilda;
    const DestinoDef* d = dg.destino(expedicao.destino);
    const Expedicao e = expedicao;
    expedicao = {};
    if (!d) return;
    // Quem foi: mais fortes primeiro (os feridos de um mau resultado são os da frente)
    std::vector<std::string> grupo = e.grupo;
    std::ranges::stable_sort(grupo, [&](const std::string& a, const std::string& b) {
        return poderAventureiro(*dg.aventureiro(a), nivelAventureiro(a), *d) > poderAventureiro(*dg.aventureiro(b), nivelAventureiro(b), *d);
    });
    const int margem = margemSemSorte(*d, grupo) + sorteExpedicao(d->id, e.partida);
    const Desfecho r = desfechoPorMargem(margem);
    const float mult = r == Desfecho::Triunfo ? 1.5f : r == Desfecho::Sucesso ? 1.0f : r == Desfecho::DurasPenas ? 0.5f : 0.0f;
    for (const auto& id : grupo) aventureiros[id].fora = false;
    std::string nomes;
    for (const auto& id : grupo) nomes += (nomes.empty() ? "" : ", ") + dg.aventureiro(id)->nome;
    notas.push_back(std::format("A expedição a {} voltou: foi {}. ({})", d->nome, nomeDesfecho(r), nomes));
    // O que trouxeram
    std::string ganhos;
    for (const auto& [rec, faixa] : d->ganhos) {
        const int base = faixa.first + static_cast<int>(sorteio(rec, e.partida) % (faixa.second - faixa.first + 1));
        const int n = static_cast<int>(std::round(base * mult));
        if (n <= 0) continue;
        inventario.adicionar(rec, n);
        ganhos += (ganhos.empty() ? "" : ", ") + std::format("{} de {}", n, dados().nomeRecurso(rec));
    }
    if (!ganhos.empty()) notas.push_back("Trouxeram " + ganhos + ".");
    // Relíquia: no máximo uma
    const float multReliquia = r == Desfecho::Triunfo ? 1.5f : r == Desfecho::Sucesso ? 1.0f : r == Desfecho::DurasPenas ? 0.5f : 0.0f;
    for (const auto& [id, chance] : d->reliquias) {
        if (estadoReliquia(id)) continue;
        if (static_cast<float>(sorteio(id, e.partida) % 1000) / 1000.0f >= chance * multReliquia) continue;
        ganharReliquia(id);
        notas.push_back(std::format("E uma relíquia da antiga Valdren: {}! (veja na guilda)", dg.reliquia(id)->nome));
        eventos_.push_back({EventoJogo::Tipo::Reliquia, jogador.pos, id, "nova", 0, dg.reliquia(id)->nome});
        break;
    }
    // Ferimentos, sumiço e experiência
    auto ferir = [&](const std::string& id, int dias) { aventureiros[id].feridoAte = relogio.dia + dias; };
    std::vector<std::string> feridos;
    if (r == Desfecho::Sucesso && margem == 0) ferir(grupo.back(), dg.diasFerido[0]), feridos.push_back(grupo.back());
    if (r == Desfecho::DurasPenas)
        for (size_t k = 0; k < (grupo.size() + 1) / 2; ++k) ferir(grupo[k], dg.diasFerido[1]), feridos.push_back(grupo[k]);
    if (r == Desfecho::Desastre) {
        const std::string sumiu = grupo[sorteio("sumico", e.partida) % grupo.size()];
        EstadoAventureiro& a = aventureiros[sumiu];
        a.sumido = true;
        const bool volta = static_cast<float>(sorteio(sumiu, e.partida) % 100) < dg.chanceVolta * 100.0f;
        a.voltaDia = volta ? relogio.dia + dg.voltaMin + static_cast<int>(sorteio("volta", e.partida) % (dg.voltaMax - dg.voltaMin + 1)) : 0;
        notas.push_back(std::format("{} desapareceu na expedição.", dg.aventureiro(sumiu)->nome));
        marcar("desaparecido:" + sumiu, dg.aventureiro(sumiu)->nome);
        for (const auto& id : grupo)
            if (id != sumiu) ferir(id, dg.diasFerido[2]), feridos.push_back(id);
    }
    if (!feridos.empty()) {
        std::string f;
        for (const auto& id : feridos) f += (f.empty() ? "" : ", ") + dg.aventureiro(id)->nome;
        notas.push_back("Voltaram feridos: " + f + ".");
    }
    if (r == Desfecho::Triunfo || r == Desfecho::Sucesso)
        for (const auto& id : grupo) aventureiros[id].nivel = std::min(3, aventureiros[id].nivel + 1);
}

void World::manhaDaGuilda(std::vector<std::string>& notas) {
    const DadosGuilda& dg = dados().guilda;
    // Com o reino em paz, aventureiros procuram Valdren
    if (!marcos.contains("estabilidade") && estado.felicidade >= dg.felicidadeMin && estado.populacao >= dg.populacaoMin &&
        resumo().seguranca >= estado.populacao) {
        marcar("estabilidade");
        notas.push_back("Com o reino em paz, aventureiros pedem um lugar para se reunir: a Guilda pode ser construída (B).");
    }
    // A guilda recruta conforme o nível
    const int vagas = static_cast<int>(efeitoGuilda(*this, "aventureiros"));
    for (int k = 0; k < vagas && k < static_cast<int>(dg.aventureiros.size()); ++k) {
        const AventureiroDef& a = dg.aventureiros[k];
        if (aventureiros.contains(a.id)) continue;
        aventureiros[a.id] = {};
        notas.push_back(a.nome + " entrou para a guilda.");
    }
    // Desaparecidos que acham o caminho de volta
    for (auto& [id, a] : aventureiros)
        if (a.sumido && a.voltaDia > 0 && relogio.dia >= a.voltaDia) {
            a.sumido = false;
            a.voltaDia = 0;
            a.feridoAte = relogio.dia + dg.diasFerido[0];
            a.nivel = std::min(3, a.nivel + 1);
            notas.push_back(std::format("{} voltou! Magro, ferido e cheio de histórias.", dg.aventureiro(id)->nome));
            marcar("voltou:" + id, dg.aventureiro(id)->nome);
        }
    // A expedição que chega hoje
    if (!expedicao.destino.empty() && relogio.dia >= expedicao.volta) voltaDaExpedicao(notas);
}

}
