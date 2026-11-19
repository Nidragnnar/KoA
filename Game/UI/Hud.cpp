#include "Game/UI/Hud.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "Game/App/Contexto.h"
#include "Game/UI/Brasao.h"

namespace koa {

namespace {

// Recursos sempre na mesma ordem na interface (o cristal só depois das minas)
const char* ORDEM_RECURSOS[] = {"madeira", "pedra", "ouro", "comida", "cristal"};

// Quebra o texto em linhas que caibam em `largura` pixels
std::vector<std::string> quebrarLinhas(const ke::BitmapFont& f, const std::string& texto, int largura) {
    std::vector<std::string> linhas;
    std::string atual, palavra;
    auto fechar = [&] {
        if (palavra.empty()) return;
        const std::string teste = atual.empty() ? palavra : atual + " " + palavra;
        if (!atual.empty() && f.largura(teste) > largura) {
            linhas.push_back(atual);
            atual = palavra;
        } else {
            atual = teste;
        }
        palavra.clear();
    };
    for (char c : texto) {
        if (c == ' ') fechar();
        else palavra += c;
    }
    fechar();
    if (!atual.empty()) linhas.push_back(atual);
    return linhas;
}

}

void Hud::atualizar(float dt) {
    for (auto& a : avisos_) a.vida -= dt;
    std::erase_if(avisos_, [](const Aviso& a) { return a.vida <= 0.0f; });
    for (auto& [k, v] : pulso_) v = std::max(0.0f, v - dt);
    tempoDica_ = std::max(0.0f, tempoDica_ - dt);
}

void Hud::acertarRelogio(const Relogio& r) {
    const int m = static_cast<int>(r.minutos);
    quandoVisto_ = std::format("{} {}, {:02}:{:02}", nomeEstacao(r.estacao()), r.diaDaEstacao(), (m / 60) % 24, m % 60);
}

void Hud::aviso(std::string texto, glm::vec4 c) {
    // O mesmo aviso de novo só renova o que já está na tela
    for (auto& a : avisos_)
        if (a.texto == texto) {
            a.vida = 3.0f;
            return;
        }
    // Fase 17: cada aviso novo também vai para o histórico (N), com dia e hora
    historico_.push_front({texto, c, quandoVisto_});
    if (historico_.size() > 60) historico_.pop_back();
    avisos_.push_back({std::move(texto), c, 3.0f});
    if (avisos_.size() > 4) avisos_.erase(avisos_.begin());
}

bool Hud::mostraCristal(const World& w) { return w.inventario.quantidade("cristal") > 0 || w.visitadas.contains("minas"); }

void Hud::desenhar(ke::Renderer2D& r, const World& w, float tempo) {
    // Recursos (canto superior esquerdo)
    const int linhas = mostraCristal(w) ? 5 : 4;
    const float extra = (linhas - 4) * 13.0f;
    desenharPainel(r, {6, 6}, {116, 72 + extra});
    {
        const ResumoReino rr = w.resumo();
        const std::string povo = std::format("{}/{}", w.estado.populacao, rr.populacaoMax);
        f_.desenhar(r, "Povo", {12, 63 + extra}, cor::apagado);
        const glm::vec4 c = w.estado.felicidade >= 50 ? cor::bom : w.estado.felicidade < 25 ? cor::ruim : cor::texto;
        f_.desenhar(r, povo, {116.0f - f_.largura(povo), 63 + extra}, c);
    }
    for (int i = 0; i < linhas; ++i) {
        const std::string id = ORDEM_RECURSOS[i];
        const float y = 11.0f + i * 13.0f;
        const float p = pulso_.contains(id) ? pulso_[id] : 0.0f;
        const auto it = dados_.recursos.find(id);
        r.sprite(rec_.regiao(it != dados_.recursos.end() ? it->second.icone : "icone_" + id), {12, y - 1 - std::round(p * 5)});
        f_.desenhar(r, dados_.nomeRecurso(id), {25, y}, cor::apagado);
        const std::string n = std::to_string(w.inventario.quantidade(id));
        f_.desenhar(r, n, {116.0f - f_.largura(n), y}, p > 0 ? cor::ouro : cor::texto);
    }

    // Data, hora, era e o título do herdeiro (canto superior direito)
    const std::string era = nomeEra(w.reino.eraAtual());
    const std::string data = w.relogio.textoData();
    const std::string titulo = w.titulo().nome;
    // Fase 10: o que o dia reserva (festival ou acontecimento da noite)
    std::string hoje;
    glm::vec4 corHoje = cor::ouro;
    if (w.festivalHoje()) hoje = "Hoje: " + dados_.estacoes.festival.nome;
    else if (const EventoDef* ev = w.eventoHoje()) {
        hoje = "Esta noite: " + ev->nome;
        if (ev->id == "lua_sangue") corHoje = cor::ruim;
    }
    // Fase 13: a horda avisada (contagem regressiva)
    std::string horda;
    if (w.emBatalha()) horda = "Batalha no portão!";
    else if (w.hordaDia > 0) {
        const int faltam = w.hordaDia - w.relogio.dia;
        horda = faltam <= 0 ? std::format("Horda esta noite ({}:00)", dados_.defesa.hora) : std::format("Horda em {} dia{}", faltam, faltam > 1 ? "s" : "");
    }
    // Fase 14: a expedição na estrada
    std::string expedicao;
    if (!w.expedicao.destino.empty()) {
        const int faltam = w.expedicao.volta - w.relogio.dia;
        expedicao = faltam <= 1 ? "Expedição volta amanhã" : std::format("Expedição volta em {} dias", faltam);
    }
    // Fase 18: o tempo de hoje (fora de casa)
    const std::string tempoDia = w.tempoAqui().id == "limpo" ? "" : "Tempo: " + w.tempoAqui().nome;
    const float l = static_cast<float>(std::max({f_.largura(era), f_.largura(data) + 50, f_.largura(titulo), f_.largura(hoje), f_.largura(horda),
                                                 f_.largura(expedicao), f_.largura(tempoDia)})) + 12.0f;
    const float alt = 41.0f + (hoje.empty() ? 0.0f : 11.0f) + (horda.empty() ? 0.0f : 11.0f) + (expedicao.empty() ? 0.0f : 11.0f) +
                      (tempoDia.empty() ? 0.0f : 11.0f);
    desenharPainel(r, {634 - l, 6}, {l, alt});
    float yLinha = 44;
    if (!hoje.empty()) f_.desenhar(r, hoje, {634 - l + 6, yLinha}, corHoje), yLinha += 11;
    if (!tempoDia.empty()) f_.desenhar(r, tempoDia, {634 - l + 6, yLinha}, cor::apagado), yLinha += 11;
    if (!horda.empty()) {
        const float a = 0.8f + 0.2f * std::sin(tempo * 4.0f);
        f_.desenhar(r, horda, {634 - l + 6, yLinha}, cor::ruim * glm::vec4(a, a, a, 1));
        yLinha += 11;
    }
    if (!expedicao.empty()) f_.desenhar(r, expedicao, {634 - l + 6, yLinha}, cor::apagado);
    f_.desenhar(r, data, {634 - l + 6, 11}, cor::texto);
    const std::string hora = w.relogio.textoHora();
    f_.desenhar(r, hora, {628.0f - f_.largura(hora), 11}, w.relogio.escuridao() > 0.5f ? glm::vec4(0.6f, 0.7f, 1.0f, 1) : cor::ouro);
    f_.desenhar(r, era, {634 - l + 6, 22}, cor::ouro);
    f_.desenhar(r, titulo, {634 - l + 6, 33}, cor::apagado);

    // Avisos: no canto de baixo, à esquerda, o mais novo embaixo (fase 17: o meio da tela fica para o herói)
    acertarRelogio(w.relogio);
    for (size_t k = 0; k < avisos_.size(); ++k) {
        const Aviso& a = avisos_[avisos_.size() - 1 - k];
        glm::vec4 c = a.cor;
        c.a = std::min(1.0f, a.vida * 2.0f);
        const float y = 296.0f - k * 12.0f;  // acima da barra do chefe e da dica de controles
        const float lt = static_cast<float>(f_.largura(a.texto));
        r.retangulo({6, y - 2}, {lt + 12, 12}, {0.05f, 0.04f, 0.04f, 0.7f * c.a});
        f_.desenhar(r, a.texto, {12, y}, c);
    }
    if (!historico_.empty() && avisos_.empty() && tempoDica_ <= 0.0f) f_.desenhar(r, "N: mensagens", {12, 296}, cor::apagado * glm::vec4(1, 1, 1, 0.6f));

    // Dica de controles nos primeiros segundos
    if (tempoDica_ > 0.0f) {
        const float a = std::min(1.0f, tempoDica_);
        const std::string dica = "WASD andar  E interagir  clique atacar  B obras  K reino  R terras  G conselho  L diário  C crônicas";
        f_.desenharCentralizado(r, dica, {320, 348}, {0.85f, 0.82f, 0.75f, a});
    }
    (void)tempo;
}

void Hud::painelConstrucao(ke::Renderer2D& r, const World& w, const std::string& id, const std::vector<std::string>& opcoes, int sel,
                           float tempo) {
    const BuildingData* b = w.reino.construcao(id);
    const BuildingDef* d = b ? w.def(*b) : nullptr;
    if (!b || !d) return;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const NivelConstrucao* atual = d->nivel(b->level);
    const ResultadoMelhoria m = avaliarMelhoria(*b, *d, w.inventario);
    const int larguraTexto = 300;
    auto descAtual = quebrarLinhas(f_, atual ? atual->descricao : "", larguraTexto);
    // Monumento: a inscrição conta a história real desta campanha
    if (d->monumento && b->constructed) {
        descAtual.push_back("");
        for (const auto& linha : w.inscricao(*b))
            for (const auto& l : quebrarLinhas(f_, linha, larguraTexto)) descAtual.push_back(l);
    }
    const auto descProx = quebrarLinhas(f_, m.proximo ? m.proximo->descricao : "", larguraTexto);
    const std::string falta = w.requisitoProximoNivel(*b);  // fase 16: nível que espera uma era
    // A altura do painel acompanha o texto
    const float alt = (d->monumento ? 56.0f : 70.0f) + (descAtual.size() + descProx.size()) * 11.0f +
                      (m.proximo ? m.custo.size() * 12.0f + 26.0f : 14.0f) + opcoes.size() * 13.0f + (falta.empty() ? 0.0f : 13.0f);
    const glm::vec2 s{328, alt}, p{320 - s.x / 2, std::floor(180 - alt / 2)};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, b->name, {320, p.y + 12}, cor::ouro, true, 1);
    f_.desenharCentralizado(r, std::format("Nível {} de {}", b->level, d->nivelMax()), {320, p.y + 24}, cor::apagado);
    float y = p.y + 38;
    for (const auto& l : descAtual) f_.desenharCentralizado(r, l, {320, y}, cor::texto), y += 11;
    r.retangulo({p.x + 12, y}, {s.x - 24, 1}, cor::borda);
    y += 12;

    // Opções (W/S + E), sempre no pé do painel
    auto desenharOpcoes = [&] {
        const float y0 = p.y + s.y - 8.0f - opcoes.size() * 13.0f;
        for (size_t i = 0; i < opcoes.size(); ++i) {
            const bool s1 = static_cast<int>(i) == sel;
            const float yy = y0 + i * 13.0f + 6.0f;
            const bool indisponivel = opcoes[i] == "Reconstruir" && (!m.possivel || !falta.empty());
            if (s1) r.retangulo({p.x + 90, yy - 6}, {s.x - 180, 12}, {0.45f, 0.36f, 0.22f, 0.45f});
            cliques.area({p.x + 40, yy - 6}, {s.x - 80, 13}, static_cast<int>(i));
            const float brilho = s1 ? 0.85f + 0.15f * std::sin(tempo * 4.0f) : 1.0f;
            glm::vec4 c = indisponivel ? cor::apagado : s1 ? cor::ouro * glm::vec4(brilho, brilho, brilho, 1) : cor::texto;
            f_.desenharCentralizado(r, indisponivel ? opcoes[i] + (falta.empty() ? " (faltam recursos)" : " (ainda não)") : opcoes[i], {320, yy}, c);
        }
    };
    if (!m.proximo) {
        if (!d->monumento) f_.desenharCentralizado(r, "A construção chegou ao nível máximo.", {320, y + 4}, cor::texto);
        desenharOpcoes();
        return;
    }
    f_.desenharCentralizado(r, "Próximo: " + m.proximo->nome, {320, y}, cor::texto);
    y += 12;
    for (const auto& l : descProx) f_.desenharCentralizado(r, l, {320, y}, cor::apagado), y += 11;
    if (!falta.empty()) f_.desenharCentralizado(r, falta, {320, y + 2}, cor::ruim), y += 13;
    y += 8;
    std::vector<std::string> ids;
    for (const char* recurso : ORDEM_RECURSOS)
        if (m.custo.contains(recurso)) ids.push_back(recurso);
    for (const auto& [rid, q] : m.custo)
        if (std::ranges::find(ids, rid) == ids.end()) ids.push_back(rid);
    for (const auto& rid : ids) {
        const int q = m.custo.at(rid);
        const int tem = w.inventario.quantidade(rid);
        const bool ok = tem >= q;
        const auto it = dados_.recursos.find(rid);
        r.sprite(rec_.regiao(it != dados_.recursos.end() ? it->second.icone : "icone_" + rid), {320 - 64, y - 5});
        f_.desenhar(r, dados_.nomeRecurso(rid), {320 - 50, y - 4}, cor::texto);
        const std::string n = std::format("{}/{}", tem, q);
        f_.desenhar(r, n, {320.0f + 64 - f_.largura(n), y - 4}, ok ? cor::bom : cor::ruim);
        y += 12;
    }
    desenharOpcoes();
}

int Hud::tamanhoUtf8(const std::string& s) { return static_cast<int>(ke::decodificarUtf8(s).size()); }

void Hud::custoEmLinha(ke::Renderer2D& r, const Custo& c, const Inventory& inv, glm::vec2 pos) {
    // Recursos na ordem do painel e, depois, os itens da vida cotidiana (nabo, ovo, peixe...)
    std::vector<std::string> ordem(std::begin(ORDEM_RECURSOS), std::end(ORDEM_RECURSOS));
    for (const auto& [id, q] : c)
        if (std::ranges::find(ordem, id) == ordem.end()) ordem.push_back(id);
    for (const std::string& rid : ordem) {
        const auto it = c.find(rid);
        if (it == c.end()) continue;
        const auto def = dados_.recursos.find(rid);
        r.sprite(rec_.regiao(def != dados_.recursos.end() ? def->second.icone : std::string("icone_") + rid), pos - glm::vec2(0, 1));
        const std::string n = std::to_string(it->second);
        f_.desenhar(r, n, pos + glm::vec2(12, 0), inv.quantidade(rid) >= it->second ? cor::texto : cor::ruim);
        pos.x += 16.0f + f_.largura(n) + 6.0f;
    }
}

void Hud::barra(ke::Renderer2D& r, glm::vec2 p, float l, float fr, glm::vec4 c) {
    r.retangulo(p, {l, 6}, {0.12f, 0.1f, 0.09f, 1});
    r.retangulo(p + 1.0f, {(l - 2) * std::clamp(fr, 0.0f, 1.0f), 4}, c);
    r.contorno(p, {l, 6}, {0.3f, 0.25f, 0.18f, 1});
}

void Hud::menuObras(ke::Renderer2D& r, const World& w, const std::vector<std::string>& defs, int sel) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    // A linha encolhe um pouco quando há muitas obras (o painel cabe na tela)
    const float lh = std::min(26.0f, std::floor(300.0f / std::max<size_t>(1, defs.size())));
    const float alt = 48.0f + defs.size() * lh;
    const glm::vec2 p{100, std::floor(176 - alt / 2)}, s{440, alt};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Obras do Reino", {320, p.y + 12}, cor::ouro);
    for (size_t i = 0; i < defs.size(); ++i) {
        const BuildingDef& d = dados_.construcoes.at(defs[i]);
        const NivelConstrucao& n1 = *d.nivel(1);
        const float y = p.y + 28 + i * lh;
        const bool s1 = static_cast<int>(i) == sel;
        const int tem = w.contarConstrucoes(d.id);
        const bool esgotada = d.limite > 0 && tem >= d.limite;
        const bool bloqueada = !w.monumentoLiberado(d);
        if (s1) r.retangulo({p.x + 8, y - 3}, {s.x - 16, lh - 1}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 3}, {s.x - 16, lh - 1}, static_cast<int>(i));
        f_.desenhar(r, n1.nome, {p.x + 14, y}, esgotada || bloqueada ? cor::apagado : s1 ? cor::ouro : cor::texto);
        if (d.monumento) f_.desenhar(r, "monumento", {p.x + 22.0f + f_.largura(n1.nome), y}, cor::apagado);
        const std::string qtd = d.limite > 0 ? std::format("{}/{}", tem, d.limite) : std::to_string(tem);
        f_.desenhar(r, qtd, {p.x + s.x - 14 - f_.largura(qtd), y}, cor::apagado);
        if (bloqueada) f_.desenhar(r, d.requerTexto, {p.x + 24, y + 11}, cor::apagado);
        else if (esgotada) f_.desenhar(r, "limite atingido", {p.x + 24, y + 11}, cor::apagado);
        else custoEmLinha(r, n1.custo, w.inventario, {p.x + 24, y + 11});
    }
    // Descrição da selecionada
    const NivelConstrucao& n1 = *dados_.construcoes.at(defs[sel]).nivel(1);
    f_.desenharCentralizado(r, n1.descricao, {320, p.y + s.y - 14}, cor::apagado);
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E ou clique posicionar   Esc fechar", {320, p.y + s.y + 10}, cor::texto);
}

void Hud::dicaColocacao(ke::Renderer2D& r, const std::string& nome, bool valido, const std::string& motivo) {
    const std::string l1 = "Construir: " + nome;
    const std::string l2 = valido ? "E ou clique confirmar   Esc cancelar   (ande para escolher o lugar)" : motivo;
    const float l = static_cast<float>(std::max(f_.largura(l1), f_.largura(l2))) + 16;
    desenharPainel(r, {320 - l / 2, 300}, {l, 30});
    f_.desenharCentralizado(r, l1, {320, 309}, cor::ouro);
    f_.desenharCentralizado(r, l2, {320, 321}, valido ? cor::texto : cor::ruim);
}

void Hud::painelReino(ke::Renderer2D& r, const World& w) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const ResumoReino rr = w.resumo();
    const glm::vec2 p{120, 36}, s{400, 288};  // largo o bastante para os dois escudos não tocarem no subtítulo
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Reino de Valdren", {320, p.y + 12}, cor::ouro);
    // Fase 16: o brasão da linhagem nos dois cantos de cima
    desenharEscudo(r, rec_, w.dadosCastelo(), w.brasao, {p.x + 8, p.y + 6});
    desenharEscudo(r, rec_, w.dadosCastelo(), w.brasao, {p.x + s.x - 30, p.y + 6});
    f_.desenharCentralizado(r, std::string(nomeEra(w.reino.eraAtual())) + "  -  " + w.titulo().nome, {320, p.y + 24}, cor::apagado);
    float y = p.y + 42;
    const float xR = p.x + 16, xV = p.x + 150;
    auto linha = [&](const std::string& rot, const std::string& val, glm::vec4 c = cor::texto) {
        f_.desenhar(r, rot, {xR, y}, cor::apagado);
        f_.desenhar(r, val, {xV, y}, c);
        y += 13;
    };
    linha("População", std::format("{} de {} (moradias)", w.estado.populacao, rr.populacaoMax));
    f_.desenhar(r, "Felicidade", {xR, y}, cor::apagado);
    const glm::vec4 cf = w.estado.felicidade >= 50 ? cor::bom : w.estado.felicidade < 25 ? cor::ruim : cor::ouro;
    barra(r, {xV, y + 1}, 100, w.estado.felicidade / 100.0f, cf);
    f_.desenhar(r, std::format("{:.0f} -> {:.0f}", w.estado.felicidade, rr.felicidadeAlvo), {xV + 108, y}, cor::texto);
    y += 13;
    linha("Segurança", std::to_string(rr.seguranca), rr.seguranca >= w.estado.populacao ? cor::bom : cor::texto);
    // Fase 13: portão e a próxima horda numa linha só
    {
        const int max = w.portaoVidaMax();
        std::string def = max > 0 ? std::format("portão {}/{}", w.portaoVida, max) : "muralha em ruínas";
        if (w.hordaDia > 0) def += std::format(", horda em {} dia(s)", std::max(0, w.hordaDia - w.relogio.dia));
        linha("Defesa", def, max > 0 && w.portaoVida == max ? cor::texto : cor::ruim);
    }
    const int saldo = rr.comidaProduzida - rr.comidaConsumida;
    linha("Comida", std::format("{} guardada, {:+} por dia", w.inventario.quantidade("comida"), saldo), saldo < 0 ? cor::ruim : cor::bom);
    linha("Ouro por dia", std::format("{} impostos + {} construções", rr.impostos, rr.ouroProducao));
    y += 4;
    f_.desenhar(r, "Impostos", {xR, y}, cor::apagado);
    const std::string imposto = std::string("< ") + nomeImposto(w.estado.imposto) + " >";
    f_.desenhar(r, imposto, {xV, y}, cor::ouro);
    // As setas também se clicam (200 = menos, 201 = mais)
    cliques.area({xV - 4, y - 3}, {14, 13}, 200);
    cliques.area({xV + f_.largura(imposto) - 10.0f, y - 3}, {14, 13}, 201);
    static const char* efeito[] = {"1 ouro/morador, +10 feliz", "2 ouro/morador", "3 ouro/morador, -12 feliz"};
    y += 11;
    f_.desenhar(r, efeito[static_cast<int>(w.estado.imposto)], {xV, y}, cor::apagado);
    y += 17;
    // Fase 10: o efeito da estação na noite do reino
    const EstacaoDef& est = w.estacaoAtual();
    f_.desenhar(r, "Estação", {xR, y}, cor::apagado);
    f_.desenhar(r, est.nome, {xV, y}, est.id == "inverno" ? cor::ruim : cor::ouro);
    y += 11;
    for (const auto& l : quebrarLinhas(f_, est.resumo, static_cast<int>(s.x) - 150 - 14)) f_.desenhar(r, l, {xV, y}, cor::apagado), y += 10;
    y += 6;
    const int dev = w.reino.desenvolvimento();
    static const int limites[] = {8, 22, 40};
    const int era = static_cast<int>(w.reino.eraAlvo());
    if (era == 2 && dev >= limites[2] && !w.reino.magiaLiberta)
        linha("Desenvolvimento", std::format("{} (falta derrotar o Cavaleiro Caído)", dev), cor::ouro);
    else linha("Desenvolvimento", era < 3 ? std::format("{} (próxima era em {})", dev, limites[era]) : std::to_string(dev));
    y += 2;
    f_.desenhar(r, "Construções", {xR, y}, cor::apagado);
    std::string lista;
    int n = 0;
    for (const auto& b : w.reino.construcoes) {
        lista += (n++ ? ", " : "") + b.name + (b.constructed ? "" : " (obra)");
    }
    float yl = y;
    for (const auto& l : quebrarLinhas(f_, lista, static_cast<int>(s.x) - 150 - 14)) f_.desenhar(r, l, {xV, yl}, cor::texto), yl += 10;
    f_.desenharCentralizado(r, "A/D ou < > mudar impostos   K ou Esc fechar", {320, p.y + s.y - 12}, cor::apagado);
    rodapeFechar("K ou Esc fechar", {320 + f_.largura("A/D ou < > mudar impostos   ") / 2.0f, p.y + s.y - 12});
}

void Hud::combate(ke::Renderer2D& r, const World& w, float tempo) {
    const Jogador& j = w.jogador;
    const float dy = mostraCristal(w) ? 13.0f : 0.0f;  // o painel de recursos cresceu com a linha do cristal
    // Vida do herói: aparece em região hostil ou quando ferido
    if (w.regiaoHostil() || j.vida < j.vidaMax) {
        desenharPainel(r, {6, 82 + dy}, {116, 22});
        f_.desenhar(r, "Vida", {12, 89 + dy}, cor::apagado);
        const float fr = static_cast<float>(j.vida) / j.vidaMax;
        const glm::vec4 c = fr > 0.5f ? cor::bom : fr > 0.25f ? cor::ouro : cor::ruim;
        const float pulso = fr <= 0.25f ? 0.75f + 0.25f * std::sin(tempo * 10.0f) : 1.0f;
        barra(r, {46, 90 + dy}, 70, fr, c * glm::vec4(pulso, pulso, pulso, 1));
        if ((w.inventario.quantidade("comida") >= 3 || w.temPrato()) && j.vida < j.vidaMax) f_.desenhar(r, "H: comer", {128, 89 + dy}, cor::apagado);
    }
    // Fase 15: o que o prato do dia ainda dá (até dormir)
    if (w.buffDano > 0 || w.buffDefesa > 0) {
        std::string b = "Bem alimentado:";
        if (w.buffDano > 0) b += std::format(" +{} dano", w.buffDano);
        if (w.buffDefesa > 0) b += std::format(" +{} defesa", w.buffDefesa);
        r.retangulo({126, 66 + dy}, {static_cast<float>(f_.largura(b)) + 10, 11}, {0.05f, 0.04f, 0.04f, 0.6f});
        f_.desenhar(r, b, {131, 68 + dy}, cor::bom);
    }
    // Chefe: barra grande no alto enquanto ele persegue
    for (auto [e, i] : w.reg.view<const Inimigo>().each()) {
        const InimigoDef& d = dados_.inimigos.at(i.tipo);
        if (!d.chefe || !i.perseguindo) continue;
        desenharPainel(r, {170, 312}, {300, 26});  // embaixo, longe dos avisos
        f_.desenharCentralizado(r, d.nome, {320, 320}, cor::ruim);
        barra(r, {180, 327}, 280, static_cast<float>(i.vida) / i.vidaMax, {0.75f, 0.15f, 0.2f, 1});
    }
    // Guia dos primeiros dias, logo acima das missões
    if (const std::string passo = w.proximoPasso(); !passo.empty()) {
        const std::string texto = "Próximo passo: " + passo;
        const float a = 0.8f + 0.2f * std::sin(tempo * 3.0f);
        r.retangulo({6, 108 + dy}, {static_cast<float>(f_.largura(texto)) + 10, 11}, {0.05f, 0.04f, 0.04f, 0.7f});
        f_.desenhar(r, texto, {11, 110 + dy}, cor::ouro * glm::vec4(a, a, a, 1));
    }
    // Fase 12: caso do conselho esperando (canto direito, embaixo da data)
    if (w.decisaoAtual()) {
        const std::string t = "O conselho espera sua decisão (G)";
        const float a = 0.8f + 0.2f * std::sin(tempo * 3.0f);
        const float l = static_cast<float>(f_.largura(t)) + 10;
        const float y = 63.0f + (w.hordaDia > 0 || w.emBatalha() ? 11.0f : 0.0f) + (w.expedicao.destino.empty() ? 0.0f : 11.0f);
        r.retangulo({634 - l, y}, {l, 11}, {0.05f, 0.04f, 0.04f, 0.7f});
        f_.desenhar(r, t, {639 - l, y + 2}, cor::ouro * glm::vec4(a, a, a, 1));
    }
    // Fase 13: a batalha no portão (embaixo, como a barra de chefe)
    if (w.emBatalha()) {
        const int n = w.criaturasDaHorda();
        desenharPainel(r, {170, 312}, {300, 26});
        f_.desenharCentralizado(r, std::format("Batalha do Portão: {} criatura{}", n, n == 1 ? "" : "s"), {320, 320}, cor::ruim);
        const int max = w.portaoVidaMax();
        if (max > 0) barra(r, {180, 327}, 280, static_cast<float>(w.portaoVida) / max, {0.75f, 0.55f, 0.25f, 1});
        else f_.desenharCentralizado(r, "sem portão: elas vão direto para a praça", {320, 330}, cor::apagado);
    }
    // Missões ativas (até 3), na coluna da esquerda
    const auto ativas = w.missoesAtivas();
    float y = 124 + dy;
    for (size_t k = 0; k < ativas.size() && k < 3; ++k) {
        const MissaoDef& m = *ativas[k];
        const std::string prog = std::format("{}/{}", std::min(w.progressoMissao(m), m.objetivo.quantidade), m.objetivo.quantidade);
        const float l = static_cast<float>(f_.largura(m.nome + "  " + prog)) + 10;
        r.retangulo({6, y - 2}, {l, 11}, {0.05f, 0.04f, 0.04f, 0.6f});
        f_.desenhar(r, m.nome, {11, y}, cor::texto);
        f_.desenhar(r, prog, {1.0f + l - f_.largura(prog), y}, cor::ouro);
        y += 12;
    }
}

void Hud::lojaForja(ke::Renderer2D& r, const World& w, const std::vector<std::string>& itens, int sel) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const float alt = 70.0f + itens.size() * 26.0f;
    const glm::vec2 p{100, std::floor(180 - alt / 2)}, s{440, alt};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, std::format("Forja de Bram (nível {})", w.nivelForja()), {320, p.y + 12}, cor::ouro);
    f_.desenharCentralizado(r, std::format("Espada: {} de dano   Armadura: {} de defesa", w.danoArma(), w.defesaArmadura()), {320, p.y + 24},
                            cor::apagado);
    for (size_t i = 0; i < itens.size(); ++i) {
        const EquipDef& d = dados_.equipamentos.at(itens[i]);
        const float y = p.y + 40 + i * 26.0f;
        const bool s1 = static_cast<int>(i) == sel;
        const bool tem = w.equip.possuidos.contains(d.id);
        const bool bloqueado = w.nivelForja() < d.forja;
        if (s1) r.retangulo({p.x + 8, y - 3}, {s.x - 16, 25}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 3}, {s.x - 16, 25}, static_cast<int>(i));
        const std::string atributo = d.tipo == "arma" ? std::format("dano {}", d.dano) : std::format("defesa {}", d.defesa);
        f_.desenhar(r, d.nome, {p.x + 14, y}, tem || bloqueado ? cor::apagado : s1 ? cor::ouro : cor::texto);
        f_.desenhar(r, atributo, {p.x + 200, y}, cor::texto);
        const std::string estado = tem ? "você já tem" : bloqueado ? std::format("forja nível {}", d.forja) : "";
        f_.desenhar(r, estado, {p.x + s.x - 14 - f_.largura(estado), y}, cor::apagado);
        if (!tem && !bloqueado) custoEmLinha(r, d.custo, w.inventario, {p.x + 24, y + 11});
    }
    const std::string desc = dados_.equipamentos.at(itens[sel]).descricao;
    f_.desenharCentralizado(r, desc, {320, p.y + s.y - 14}, cor::apagado);
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E ou clique forjar   Esc fechar", {320, p.y + s.y + 10}, cor::texto);
}

void Hud::diario(ke::Renderer2D& r, const World& w) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.5f});
    const glm::vec2 p{90, 40}, s{460, 280};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Diário do Herdeiro", {320, p.y + 12}, cor::ouro);
    float y = p.y + 30;
    f_.desenhar(r, "Missões", {p.x + 14, y}, cor::ouro);
    y += 13;
    const auto ativas = w.missoesAtivas();
    if (ativas.empty()) f_.desenhar(r, "Nenhuma missão ativa. Converse com os moradores.", {p.x + 20, y}, cor::apagado), y += 12;
    for (const MissaoDef* m : ativas) {
        const int pr = std::min(w.progressoMissao(*m), m->objetivo.quantidade);
        f_.desenhar(r, std::format("{} ({}/{})", m->nome, pr, m->objetivo.quantidade), {p.x + 20, y}, cor::texto);
        y += 11;
        for (const auto& l : quebrarLinhas(f_, m->descricao, static_cast<int>(s.x) - 50)) f_.desenhar(r, l, {p.x + 30, y}, cor::apagado), y += 10;
        y += 3;
    }
    int concluidas = 0;
    for (const auto& [id, e] : w.missoes) concluidas += e.concluida;
    f_.desenhar(r, std::format("Cumpridas: {}", concluidas), {p.x + 20, y}, cor::bom);
    y += 18;
    f_.desenhar(r, "Equipamento", {p.x + 14, y}, cor::ouro);
    y += 13;
    const EquipDef& arma = dados_.equipamentos.at(w.equip.arma);
    const EquipDef& armadura = dados_.equipamentos.at(w.equip.armadura);
    f_.desenhar(r, std::format("{} (dano {})", arma.nome, arma.dano), {p.x + 20, y}, cor::texto);
    y += 11;
    f_.desenhar(r, std::format("{} (defesa {})", armadura.nome, armadura.defesa), {p.x + 20, y}, cor::texto);
    y += 18;
    f_.desenhar(r, "Criaturas derrotadas", {p.x + 14, y}, cor::ouro);
    y += 13;
    std::string abates;
    for (const auto& [tipo, n] : w.abates) abates += (abates.empty() ? "" : ",  ") + dados_.inimigos.at(tipo).nome + " " + std::to_string(n);
    f_.desenhar(r, abates.empty() ? "Nenhuma ainda." : abates, {p.x + 20, y}, abates.empty() ? cor::apagado : cor::texto);
    f_.desenharCentralizado(r, "J ou clique atacar   H comer   L ou Esc fechar", {320, p.y + s.y - 12}, cor::apagado);
    rodapeFechar("L ou Esc fechar", {320 + f_.largura("J ou clique atacar   H comer   ") / 2.0f, p.y + s.y - 12});
}

void Hud::cronicas(ke::Renderer2D& r, const World& w, int aba, int rolagem, int selFragmento) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.55f});
    const glm::vec2 p{60, 24}, s{520, 312};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Crônicas de Valdren", {320, p.y + 12}, cor::ouro, true, 1);
    const TituloDef& t = w.titulo();
    const std::string sub = t.nome + ": " + t.descricao;
    f_.desenharCentralizado(r, f_.largura(sub) <= s.x - 24 ? sub : t.nome, {320, p.y + 25}, cor::apagado);
    // Abas
    const int total = static_cast<int>(dados_.lore.pistas.size());
    const std::string abas[2] = {"Crônicas", std::format("Fragmentos da Queda ({}/{})", w.pistasEncontradas(), total)};
    float xa = 320.0f - (f_.largura(abas[0]) + f_.largura(abas[1]) + 30) / 2.0f;
    for (int i = 0; i < 2; ++i) {
        const float la = static_cast<float>(f_.largura(abas[i]));
        if (i == aba) r.retangulo({xa - 5, p.y + 36}, {la + 10, 12}, {0.45f, 0.36f, 0.22f, 0.5f});
        cliques.area({xa - 5, p.y + 34}, {la + 10, 15}, 100 + i);  // abas: 100 e 101
        f_.desenhar(r, abas[i], {xa, p.y + 38}, i == aba ? cor::ouro : cor::apagado);
        xa += la + 30;
    }
    r.retangulo({p.x + 12, p.y + 52}, {s.x - 24, 1}, cor::borda);
    const int largura = static_cast<int>(s.x) - 60;
    float y = p.y + 60;
    const float fim = p.y + s.y - 22;
    if (aba == 0) {
        // Páginas em ordem, a partir da rolagem
        const auto& c = w.cronicas;
        for (int i = std::max(0, rolagem); i < static_cast<int>(c.size()); ++i) {
            const auto linhas = quebrarLinhas(f_, c[i].texto, largura - 70);
            if (y + 11.0f * linhas.size() > fim) break;
            f_.desenhar(r, c[i].dia > 0 ? std::format("Dia {}", c[i].dia) : "Antes", {p.x + 20, y}, cor::ouro);
            for (const auto& l : linhas) f_.desenhar(r, l, {p.x + 90, y}, cor::texto), y += 11;
            y += 6;
        }
        if (c.empty()) f_.desenhar(r, "Nada foi escrito ainda.", {p.x + 20, y}, cor::apagado);
        f_.desenharCentralizado(r, std::format("A/D abas   W/S ou roda folhear   C ou Esc fechar   ({} páginas)", c.size()), {320, p.y + s.y - 14},
                                cor::apagado);
    } else {
        // Fragmentos: os achados podem ser relidos; os outros ficam em branco
        for (int i = 0; i < total; ++i) {
            const PistaDef& pd = dados_.lore.pistas[i];
            const bool achou = w.marcos.contains("pista:" + pd.id);
            const bool sel = i == selFragmento;
            if (sel) r.retangulo({p.x + 14, y - 3}, {s.x - 28, 13}, {0.45f, 0.36f, 0.22f, 0.45f});
            cliques.area({p.x + 14, y - 3}, {s.x - 28, 14}, i);
            f_.desenhar(r, achou ? pd.titulo : "? ? ?", {p.x + 24, y}, achou ? (sel ? cor::ouro : cor::texto) : cor::apagado);
            y += 15;
        }
        y += 8;
        const bool completo = w.marcos.contains("misterio:primeiro_ato");
        const std::string nota = completo ? "Todas as peças reunidas. Mas para onde foi a Chama?"
                                          : "Procure no castelo, nos túmulos, na floresta... e escute os viajantes.";
        for (const auto& l : quebrarLinhas(f_, nota, largura)) f_.desenhar(r, l, {p.x + 24, y}, completo ? cor::ouro : cor::apagado), y += 11;
        f_.desenharCentralizado(r, "A/D abas   W/S escolher   E ou clique ler   C ou Esc fechar", {320, p.y + s.y - 14}, cor::apagado);
    }
}

void Hud::leitura(ke::Renderer2D& r, const PistaDef& pd) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.6f});
    std::vector<std::vector<std::string>> blocos;
    size_t linhas = 0;
    for (const auto& paragrafo : pd.texto) {
        blocos.push_back(quebrarLinhas(f_, paragrafo, 360));
        linhas += blocos.back().size();
    }
    const float alt = 58.0f + linhas * 11.0f + blocos.size() * 7.0f;
    const glm::vec2 p{120, std::floor(180 - alt / 2)}, s{400, alt};
    // Papel envelhecido em vez do painel de pedra
    r.retangulo(p, s, {0.32f, 0.27f, 0.2f, 1});
    r.retangulo(p + 3.0f, s - 6.0f, {0.86f, 0.79f, 0.64f, 1});
    r.contorno(p + 3.0f, s - 6.0f, {0.55f, 0.43f, 0.3f, 1});
    const glm::vec4 tinta{0.22f, 0.15f, 0.1f, 1};
    f_.desenharCentralizado(r, pd.titulo, {320, p.y + 16}, {0.45f, 0.18f, 0.12f, 1});
    float y = p.y + 32;
    for (const auto& b : blocos) {
        for (const auto& l : b) f_.desenhar(r, l, {p.x + 20, y}, tinta), y += 11;
        y += 7;
    }
    f_.desenharCentralizado(r, "Guardado nas Crônicas (C)    E fechar", {320, p.y + s.y - 14}, {0.45f, 0.36f, 0.26f, 1});
}

void Hud::lojaMercador(ke::Renderer2D& r, const World& w, const NpcDef& npc, int sel) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const float alt = 64.0f + npc.loja.size() * 26.0f;
    const glm::vec2 p{110, std::floor(180 - alt / 2)}, s{420, alt};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Banca de " + npc.nome, {320, p.y + 12}, cor::ouro);
    f_.desenharCentralizado(r, std::format("Seu ouro: {}", w.inventario.quantidade("ouro")), {320, p.y + 24}, cor::apagado);
    for (size_t i = 0; i < npc.loja.size(); ++i) {
        const ItemLoja& it = npc.loja[i];
        const float y = p.y + 40 + i * 26.0f;
        const bool s1 = static_cast<int>(i) == sel;
        const bool esgotado = w.itemEsgotado(it);
        if (s1) r.retangulo({p.x + 8, y - 3}, {s.x - 16, 25}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 3}, {s.x - 16, 25}, static_cast<int>(i));
        f_.desenhar(r, it.nome, {p.x + 14, y}, esgotado ? cor::apagado : s1 ? cor::ouro : cor::texto);
        if (esgotado) f_.desenhar(r, "vendido", {p.x + 24, y + 11}, cor::apagado);
        else custoEmLinha(r, it.custo, w.inventario, {p.x + 24, y + 11});
    }
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E ou clique comprar   Esc fechar", {320, p.y + s.y + 10}, cor::texto);
}

void Hud::relatorioManha(ke::Renderer2D& r, const World& w) {
    const RelatorioDia& rel = w.ultimoRelatorio;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.35f});
    std::vector<std::string> notas;
    for (const auto& n : rel.notas)
        for (const auto& l : quebrarLinhas(f_, n, 360)) notas.push_back(l);
    const float alt = 92.0f + rel.variacoes.size() * 11.0f + notas.size() * 11.0f;
    const glm::vec2 p{130, std::floor(176 - alt / 2)}, s{380, alt};
    desenharPainel(r, p, s);
    f_.desenharCentralizado(r, "Relatório da manhã", {320, p.y + 12}, cor::ouro);
    f_.desenharCentralizado(r, w.relogio.textoData(), {320, p.y + 24}, cor::apagado);
    float y = p.y + 40;
    for (const auto& [rid, v] : rel.variacoes) {
        // "madeira|Floresta Encantada": o que uma região rendeu
        const auto sep = rid.find('|');
        const std::string nome = rid == "ouro (desmaio)" ? "Ouro (desmaio)"
                                 : sep != std::string::npos ? dados_.nomeRecurso(rid.substr(0, sep)) + " (" + rid.substr(sep + 1) + ")"
                                                            : dados_.nomeRecurso(rid);
        f_.desenhar(r, nome, {p.x + 40, y}, cor::texto);
        const std::string val = std::format("{:+}", v);
        f_.desenhar(r, val, {p.x + s.x - 40 - f_.largura(val), y}, v > 0 ? cor::bom : v < 0 ? cor::ruim : cor::apagado);
        y += 11;
    }
    y += 4;
    f_.desenhar(r, "Povo", {p.x + 40, y}, cor::texto);
    const std::string pv = std::format("{} -> {}", rel.populacaoAntes, rel.populacaoDepois);
    f_.desenhar(r, pv, {p.x + s.x - 40 - f_.largura(pv), y}, rel.populacaoDepois > rel.populacaoAntes ? cor::bom : cor::texto);
    y += 11;
    f_.desenhar(r, "Felicidade", {p.x + 40, y}, cor::texto);
    const std::string fv = std::format("{:.0f} -> {:.0f}", rel.felicidadeAntes, rel.felicidadeDepois);
    f_.desenhar(r, fv, {p.x + s.x - 40 - f_.largura(fv), y}, rel.felicidadeDepois >= rel.felicidadeAntes ? cor::bom : cor::ruim);
    y += 15;
    for (const auto& n : notas) f_.desenharCentralizado(r, n, {320, y + 3}, cor::ouro), y += 11;
    f_.desenharCentralizado(r, "E continuar", {320, p.y + s.y - 11}, cor::apagado);
}

void Hud::caixaDialogo(ke::Renderer2D& r, const Conversa& c, int linha, int letras, float tempo) {
    const glm::vec2 p{20, 262}, s{600, 90};
    desenharPainel(r, p, s);
    // Retrato: cabeça e ombros do morador, ampliados 4×
    const ke::SpriteRegion& sp = rec_.regiao("npc_" + c.npc + "_baixo_0");
    if (sp.textura) {
        const float vMeio = sp.uv.y + (sp.uv.w - sp.uv.y) * (15.0f / 24.0f);
        r.retangulo(p + glm::vec2(10, 10), {68, 70}, {0.12f, 0.10f, 0.09f, 1});
        r.contorno(p + glm::vec2(10, 10), {68, 70}, cor::borda);
        r.quad(*sp.textura, p + glm::vec2(12, 14), {64, 60}, {sp.uv.x, sp.uv.y, sp.uv.z, vMeio});
    }
    f_.desenhar(r, c.nome, p + glm::vec2(90, 10), cor::ouro);
    f_.desenhar(r, c.profissao, p + glm::vec2(90.0f + f_.largura(c.nome) + 10, 10), cor::apagado);
    // Amizade: 10 corações
    const ke::SpriteRegion& cheio = rec_.regiao("coracao_cheio");
    const ke::SpriteRegion& vazio = rec_.regiao("coracao_vazio");
    for (int i = 0; i < 10; ++i) r.sprite(i < c.coracoes ? cheio : vazio, p + glm::vec2(s.x - 92 + i * 8, 11));
    if (c.ganhouAmizade && linha == 0) f_.desenhar(r, "+ amizade", p + glm::vec2(s.x - 172, 10), cor::bom);
    // Fase 15: presente que o herdeiro pode dar (F ou clique)
    if (!c.presente.empty()) {
        const std::string t = "F  dar " + dados_.nomeRecurso(c.presente);
        const glm::vec2 pt = p + glm::vec2(90, s.y - 14);
        cliques.area(pt - glm::vec2(4, 3), {static_cast<float>(f_.largura(t)) + 8, 13}, 600);
        f_.desenhar(r, t, pt, cor::bom);
    }

    // Fala atual, aparecendo letra a letra
    if (linha < static_cast<int>(c.falas.size())) {
        const std::u32string tudo = ke::decodificarUtf8(c.falas[linha]);
        // Quebra pelo texto inteiro (as palavras não pulam de linha enquanto aparecem)
        const auto linhasCheias = quebrarLinhas(f_, c.falas[linha], static_cast<int>(s.x) - 110);
        size_t usado = 0;
        float y = p.y + 30;
        const size_t total = std::min<size_t>(static_cast<size_t>(std::max(0, letras)), tudo.size());
        for (const auto& l : linhasCheias) {
            const size_t n = ke::decodificarUtf8(l).size();
            if (usado >= total) break;
            const size_t mostrar = std::min(n, total - usado);
            std::string pedaco;
            size_t k = 0;
            for (char32_t ch : ke::decodificarUtf8(l)) {
                if (k++ >= mostrar) break;
                if (ch < 0x80) pedaco += static_cast<char>(ch);
                else if (ch < 0x800) pedaco += static_cast<char>(0xC0 | (ch >> 6)), pedaco += static_cast<char>(0x80 | (ch & 0x3F));
                else pedaco += static_cast<char>(0xE0 | (ch >> 12)), pedaco += static_cast<char>(0x80 | ((ch >> 6) & 0x3F)),
                     pedaco += static_cast<char>(0x80 | (ch & 0x3F));
            }
            f_.desenhar(r, pedaco, {p.x + 90, y}, cor::texto);
            usado += n + 1;  // +1: o espaço que virou quebra de linha
            y += 11;
        }
        // Indicador "continua" piscando quando a fala terminou
        if (static_cast<size_t>(letras) >= tudo.size() && std::fmod(tempo, 0.8f) < 0.5f) {
            const bool ultima = linha + 1 >= static_cast<int>(c.falas.size());
            f_.desenhar(r, ultima ? "E  fechar" : "E  >", {p.x + s.x - (ultima ? 80.0f : 40.0f), p.y + s.y - 14}, cor::ouro);
        }
    }
}

void Hud::menuPausa(ke::Renderer2D& r, const std::vector<std::string>& opcoes, int sel) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.55f});
    const float alt = 40.0f + opcoes.size() * 16.0f;
    const glm::vec2 p{240, 180 - alt / 2};
    desenharPainel(r, p, {160, alt});
    f_.desenharCentralizado(r, "Pausa", {320, p.y + 12}, cor::ouro);
    for (size_t i = 0; i < opcoes.size(); ++i) {
        const bool s = static_cast<int>(i) == sel;
        const float y = p.y + 32 + i * 16.0f;
        if (s) r.retangulo({p.x + 10, y - 5}, {140, 12}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 10, y - 7}, {140, 16}, static_cast<int>(i));
        f_.desenharCentralizado(r, opcoes[i], {320, y}, s ? cor::ouro : cor::texto);
    }
}

// ---------- Fase 12: o Conselho Real e as decisões ----------

std::string Hud::textoEfeito(const CargoDef& c, float v) const {
    if (c.efeito == "seguranca") return std::format("+{} de segurança", static_cast<int>(std::round(v)));
    if (c.efeito == "impostos") return std::format("+{}% de impostos", static_cast<int>(std::round(v * 100)));
    if (c.efeito == "comida") return std::format("+{}% de comida", static_cast<int>(std::round(v * 100)));
    return std::format("+{} de madeira e pedra por noite", static_cast<int>(std::round(v)));
}

void Hud::painelConselho(ke::Renderer2D& r, const World& w, int sel, const std::string& proposto, float tempo) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const glm::vec2 p{80, 22}, s{480, 316};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Conselho Real", {320, p.y + 12}, cor::ouro, true, 1);
    const std::string rodape = "W/S cargo   A/D conselheiro   E nomear   G ou Esc fechar";
    if (!w.conselhoFormado()) {
        float y = p.y + 50;
        const std::string t = std::format("Valdren ainda não tem um conselho. Quando o reino tiver {} obras de pé, o povo pedirá um.",
                                          dados_.conselho.obrasParaFormar);
        for (const auto& l : quebrarLinhas(f_, t, static_cast<int>(s.x) - 60)) f_.desenharCentralizado(r, l, {320, y}, cor::texto), y += 12;
        f_.desenharCentralizado(r, "G ou Esc fechar", {320, p.y + s.y - 12}, cor::apagado);
        rodapeFechar("G ou Esc fechar", {320, p.y + s.y - 12});
        return;
    }
    f_.desenharCentralizado(r, "Contente, rende muito. Descontente, faz o mínimo.", {320, p.y + 25}, cor::apagado);
    const float xR = p.x + 16, xO = p.x + 190;
    float y = p.y + 42;
    const auto& cargos = dados_.conselho.cargos;
    for (size_t i = 0; i < cargos.size(); ++i) {
        const CargoDef& c = cargos[i];
        const bool marcada = static_cast<int>(i) == sel;
        const std::string atual = w.ocupante(c.id);
        // Na linha selecionada aparece o candidato que o herdeiro está olhando
        const std::string quem = marcada ? proposto : atual;
        const bool mudando = marcada && proposto != atual;
        if (marcada) r.retangulo({p.x + 8, y - 4}, {s.x - 16, 45}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 4}, {s.x - 16, 45}, static_cast<int>(i));
        f_.desenhar(r, c.nome, {xR, y}, cor::ouro);
        const std::string nome = quem.empty() ? "vago" : dados_.npcs.at(quem).nome;
        const std::string seletor = "< " + nome + " >";
        const float brilho = mudando ? 0.85f + 0.15f * std::sin(tempo * 5.0f) : 1.0f;
        f_.desenhar(r, seletor, {xO, y}, mudando ? cor::ouro * glm::vec4(brilho, brilho, brilho, 1) : quem.empty() ? cor::apagado : cor::texto);
        cliques.area({xO - 4, y - 3}, {14, 13}, 300 + static_cast<int>(i));
        cliques.area({xO + f_.largura(seletor) - 10.0f, y - 3}, {14, 13}, 310 + static_cast<int>(i));
        if (mudando) f_.desenhar(r, "E nomeia", {xO + f_.largura(seletor) + 10.0f, y}, cor::ouro);
        y += 12;
        f_.desenhar(r, c.descricao, {xR + 6, y}, cor::apagado);
        if (!quem.empty()) {
            const float apt = c.aptidaoDe(quem);
            const std::string oficio = apt >= 0.8f ? "no ofício certo" : apt <= 0.4f ? "fora do ofício" : "dá conta";
            f_.desenhar(r, oficio, {p.x + s.x - 16 - f_.largura(oficio), y}, apt >= 0.8f ? cor::bom : cor::apagado);
        }
        y += 12;
        if (!quem.empty()) {
            // Lealdade em palavra e barra (sem número: só o efeito tem número)
            const auto it = w.lealdade.find(quem);
            const Relacionamento* rel = nullptr;
            for (auto [e, m, rr] : w.reg.view<const Morador, const Relacionamento>().each())
                if (m.id == quem) rel = &rr;
            const int l = it != w.lealdade.end() ? it->second : lealdadeInicial(rel ? rel->afinidade : 0);
            const glm::vec4 cl = l >= 50 ? cor::bom : l >= regra::LEALDADE_INSATISFEITO ? cor::ouro : cor::ruim;
            f_.desenhar(r, palavraLealdade(l), {xR + 6, y}, cl);
            barra(r, {xR + 16.0f + f_.largura("Insatisfeito"), y + 1}, 60, l / 100.0f, cl);
            const std::string ef = textoEfeito(c, efeitoNoCargo(c, quem, l));
            f_.desenhar(r, ef, {p.x + s.x - 16 - f_.largura(ef), y}, fatorLealdade(l) > 0.0f ? cor::bom : cor::ruim);
        } else {
            f_.desenhar(r, "Ninguém no cargo: ele não rende nada.", {xR + 6, y}, cor::apagado);
        }
        y += 22;
    }
    // Leis valendo e o caso pendente
    std::string leis;
    for (const auto& id : w.leis)
        if (const auto it = dados_.conselho.leis.find(id); it != dados_.conselho.leis.end()) leis += (leis.empty() ? "" : ", ") + it->second.nome;
    f_.desenhar(r, "Leis", {xR, y}, cor::ouro);
    for (const auto& l : quebrarLinhas(f_, leis.empty() ? "nenhuma" : leis, static_cast<int>(s.x) - 90))
        f_.desenhar(r, l, {xR + 60, y}, leis.empty() ? cor::apagado : cor::texto), y += 11;
    y += 6;
    const int n = static_cast<int>(cargos.size());
    if (const DecisaoDef* d = w.decisaoAtual()) {
        const bool marcada = sel == n;
        if (marcada) r.retangulo({p.x + 8, y - 4}, {s.x - 16, 14}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 4}, {s.x - 16, 14}, n);
        const float pulso = 0.8f + 0.2f * std::sin(tempo * 4.0f);
        f_.desenhar(r, "Caso esperando: " + d->titulo + " (E)", {xR, y}, cor::ouro * glm::vec4(pulso, pulso, pulso, 1));
    } else {
        const int faltam = std::clamp(dados_.conselho.diasEntreDecisoes - (w.relogio.dia - w.ultimaDecisao), 1, dados_.conselho.diasEntreDecisoes);
        f_.desenhar(r, std::format("Nenhum caso esperando. O conselho se reúne em uns {} dia{}.", faltam, faltam > 1 ? "s" : ""), {xR, y},
                    cor::apagado);
    }
    f_.desenharCentralizado(r, rodape, {320, p.y + s.y - 12}, cor::apagado);
    rodapeFechar("G ou Esc fechar", {320 + f_.largura("W/S cargo   A/D conselheiro   E nomear   ") / 2.0f, p.y + s.y - 12});
}

void Hud::painelDecisao(ke::Renderer2D& r, const World& w, const DecisaoDef& d, int sel, float tempo) {
    r.retangulo({0, 0}, {640, 360}, {0.03f, 0.02f, 0.02f, 0.5f});
    const int largura = 400;
    std::vector<std::string> texto;
    for (const auto& par : d.texto)
        for (const auto& l : quebrarLinhas(f_, par, largura)) texto.push_back(l);
    // Cada opção: nome, descrição e as falas de quem a apoia no conselho
    struct Bloco {
        std::vector<std::string> desc, falas;
    };
    std::vector<Bloco> blocos;
    float altOpcoes = 0.0f;
    for (const auto& o : d.opcoes) {
        Bloco b;
        b.desc = quebrarLinhas(f_, o.descricao, largura - 20);
        for (const auto& [cargo, op] : d.conselho) {
            const std::string npc = w.ocupante(cargo);
            if (op.first != o.id || npc.empty()) continue;
            const auto it = w.lealdade.find(npc);
            const bool fala = it == w.lealdade.end() || it->second >= regra::LEALDADE_INSATISFEITO;  // descontente nem opina
            for (const auto& l : quebrarLinhas(f_, dados_.npcs.at(npc).nome + ": " + (fala ? "\"" + op.second + "\"" : "(dá de ombros)"), largura - 30))
                b.falas.push_back(l);
        }
        altOpcoes += 15.0f + b.desc.size() * 10.0f + b.falas.size() * 10.0f + 7.0f;
        blocos.push_back(std::move(b));
    }
    const float alt = std::min(352.0f, 46.0f + texto.size() * 11.0f + 8.0f + altOpcoes + 20.0f);
    const glm::vec2 p{100, std::floor(180 - alt / 2)}, s{440, alt};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    const float pulso = 0.85f + 0.15f * std::sin(tempo * 3.0f);
    f_.desenharCentralizado(r, "O conselho pede sua decisão", {320, p.y + 11}, cor::apagado);
    f_.desenharCentralizado(r, d.titulo, {320, p.y + 25}, cor::ouro * glm::vec4(pulso, pulso, pulso, 1), true, 1);
    float y = p.y + 40;
    for (const auto& l : texto) f_.desenhar(r, l, {p.x + 20, y}, cor::texto), y += 11;
    y += 8;
    for (size_t i = 0; i < d.opcoes.size(); ++i) {
        const Bloco& b = blocos[i];
        const bool marcada = static_cast<int>(i) == sel;
        const float h = 15.0f + b.desc.size() * 10.0f + b.falas.size() * 10.0f;
        if (marcada) r.retangulo({p.x + 10, y - 4}, {s.x - 20, h + 2}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 10, y - 4}, {s.x - 20, h + 2}, static_cast<int>(i));
        f_.desenhar(r, d.opcoes[i].nome, {p.x + 18, y}, marcada ? cor::ouro : cor::texto);
        y += 13;
        for (const auto& l : b.desc) f_.desenhar(r, l, {p.x + 28, y}, cor::apagado), y += 10;
        for (const auto& l : b.falas) f_.desenhar(r, l, {p.x + 32, y}, cor::bom), y += 10;
        y += 9;
    }
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E ou clique decidir   Esc decidir depois (G)", {320, p.y + s.y - 11}, cor::apagado);
}

// ---------- Fase 14: a Guilda dos Aventureiros ----------

std::vector<std::string> Hud::aventureirosRecrutados(const World& w) const {
    std::vector<std::string> v;
    for (const auto& a : dados_.guilda.aventureiros)
        if (w.aventureiros.contains(a.id)) v.push_back(a.id);
    return v;
}

namespace {

const char* palavraPerigo(int p) { return p <= 2 ? "baixo" : p <= 4 ? "médio" : p <= 6 ? "alto" : "mortal"; }
const char* palavraTipo(const std::string& t) { return t == "combate" ? "combate" : t == "saber" ? "saber" : "exploração"; }

}

void Hud::painelGuilda(ke::Renderer2D& r, const World& w, int aba, int sel, int passo, const std::string& destino,
                       const std::vector<std::string>& grupo, float tempo) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.5f});
    const glm::vec2 p{60, 18}, s{520, 324};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    const DadosGuilda& dg = dados_.guilda;
    f_.desenharCentralizado(r, "Guilda dos Aventureiros", {320, p.y + 12}, cor::ouro, true, 1);
    const auto recrutados = aventureirosRecrutados(w);
    // Abas (100, 101, 102)
    const std::string abas[3] = {"Expedição", std::format("Aventureiros ({})", recrutados.size()),
                                 std::format("Relíquias ({}/{} expostas)", w.reliquiasExpostas(), w.vagasReliquias())};
    float xa = 320.0f;
    for (const auto& a : abas) xa -= (f_.largura(a) + 24) / 2.0f;
    for (int i = 0; i < 3; ++i) {
        const float la = static_cast<float>(f_.largura(abas[i]));
        if (i == aba) r.retangulo({xa - 5, p.y + 26}, {la + 10, 12}, {0.45f, 0.36f, 0.22f, 0.5f});
        cliques.area({xa - 5, p.y + 24}, {la + 10, 15}, 100 + i);
        f_.desenhar(r, abas[i], {xa, p.y + 28}, i == aba ? cor::ouro : cor::apagado);
        xa += la + 24;
    }
    r.retangulo({p.x + 12, p.y + 42}, {s.x - 24, 1}, cor::borda);
    const float xR = p.x + 18;
    float y = p.y + 50;
    auto linhaSel = [&](int i, float h) {
        if (i == sel) r.retangulo({p.x + 10, y - 3}, {s.x - 20, h}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 10, y - 3}, {s.x - 20, h}, i);
    };
    auto paragrafo = [&](const std::string& t, glm::vec4 c, int recuo = 0) {
        for (const auto& l : quebrarLinhas(f_, t, static_cast<int>(s.x) - 40 - recuo)) f_.desenhar(r, l, {xR + recuo, y}, c), y += 11;
    };
    std::string rodape;
    if (aba == 0) {
        if (!w.expedicao.destino.empty()) {
            // Expedição em curso
            const DestinoDef* d = dg.destino(w.expedicao.destino);
            const int faltam = std::max(0, w.expedicao.volta - w.relogio.dia);
            f_.desenhar(r, "Em expedição: " + (d ? d->nome : w.expedicao.destino), {xR, y}, cor::ouro);
            y += 14;
            paragrafo(faltam == 0 ? "Volta amanhã cedo, no relatório da manhã." : std::format("Volta em {} dia{}, no relatório da manhã.", faltam, faltam > 1 ? "s" : ""),
                      cor::texto);
            std::string nomes;
            for (const auto& id : w.expedicao.grupo) nomes += (nomes.empty() ? "" : ", ") + dg.aventureiro(id)->nome;
            paragrafo("Grupo: " + nomes, cor::texto);
            if (d) paragrafo(std::string("Antes de partir: ") + chanceEmPalavras(w.margemSemSorte(*d, w.expedicao.grupo)) + ".", cor::apagado);
            rodape = "A/D abas   Esc fechar";
        } else if (passo == 0) {
            for (size_t i = 0; i < dg.destinos.size(); ++i) {
                const DestinoDef& d = dg.destinos[i];
                const bool aberto = w.destinoAberto(d);
                linhaSel(static_cast<int>(i), 25);
                f_.desenhar(r, aberto ? d.nome : "? ? ?", {xR, y}, !aberto ? cor::apagado : static_cast<int>(i) == sel ? cor::ouro : cor::texto);
                f_.desenhar(r, aberto ? std::format("{} dias   perigo {}   {}", d.dias, palavraPerigo(d.perigo), palavraTipo(d.tipo))
                                      : "Ninguém conhece o caminho. Talvez um mapa antigo...",
                            {xR + 10, y + 11}, cor::apagado);
                y += 27;
            }
            y += 4;
            if (sel >= 0 && sel < static_cast<int>(dg.destinos.size()) && w.destinoAberto(dg.destinos[sel])) {
                const DestinoDef& d = dg.destinos[sel];
                paragrafo(d.descricao, cor::texto);
                std::string ganhos;
                for (const auto& [rec, faixa] : d.ganhos) ganhos += (ganhos.empty() ? "" : ", ") + std::format("{} a {} de {}", faixa.first, faixa.second, dados_.nomeRecurso(rec));
                paragrafo("Pode render: " + ganhos + (d.reliquias.empty() ? "." : ", e talvez uma relíquia."), cor::apagado);
            }
            rodape = "A/D abas   W/S destino   E montar o grupo   Esc fechar";
        } else {
            const DestinoDef* d = dg.destino(destino);
            f_.desenhar(r, "Grupo para " + (d ? d->nome : destino) + std::format(" (até {})", dg.grupoMax), {xR, y}, cor::ouro);
            y += 15;
            for (size_t i = 0; i < recrutados.size(); ++i) {
                const std::string& id = recrutados[i];
                const AventureiroDef& a = *dg.aventureiro(id);
                const bool no = std::ranges::find(grupo, id) != grupo.end();
                const bool pronto = w.aventureiroPronto(id);
                linhaSel(static_cast<int>(i), 13);
                f_.desenhar(r, no ? "[x]" : "[ ]", {xR, y}, no ? cor::bom : cor::apagado);
                f_.desenhar(r, a.nome, {xR + 26, y}, !pronto ? cor::apagado : static_cast<int>(i) == sel ? cor::ouro : cor::texto);
                const std::string info = d ? std::format("força {}", poderAventureiro(a, w.nivelAventureiro(id), *d)) : "";
                f_.desenhar(r, info, {xR + 230, y}, cor::texto);
                const std::string sit = w.situacaoAventureiro(id);
                f_.desenhar(r, sit, {p.x + s.x - 18 - f_.largura(sit), y}, pronto ? cor::bom : cor::ruim);
                y += 14;
            }
            y += 6;
            const int n = static_cast<int>(recrutados.size());
            linhaSel(n, 14);
            if (d) {
                const int comida = w.comidaExpedicao(*d, static_cast<int>(grupo.size()));
                const std::string t = grupo.empty() ? "Partir (escolha ao menos um aventureiro)"
                                                    : std::format("Partir: {} de comida, {} dias.  {}", comida, d->dias, chanceEmPalavras(w.margemSemSorte(*d, grupo)));
                f_.desenhar(r, t, {xR, y}, grupo.empty() ? cor::apagado : sel == n ? cor::ouro : cor::texto);
            }
            y += 20;
            paragrafo("Especialidade do destino vale +1. Cada expedição que dá certo deixa o grupo mais forte.", cor::apagado);
            rodape = "W/S escolher   E põe ou tira, E em Partir   Esc voltar";
        }
    } else if (aba == 1) {
        if (recrutados.empty()) paragrafo("Ninguém ainda. Os aventureiros chegam de manhã depois que a guilda fica pronta.", cor::apagado);
        for (size_t i = 0; i < recrutados.size(); ++i) {
            const std::string& id = recrutados[i];
            const AventureiroDef& a = *dg.aventureiro(id);
            linhaSel(static_cast<int>(i), 13);
            f_.desenhar(r, a.nome, {xR, y}, static_cast<int>(i) == sel ? cor::ouro : cor::texto);
            f_.desenhar(r, std::format("força {}   nível {}   {}", a.forca + w.nivelAventureiro(id) - 1, w.nivelAventureiro(id), palavraTipo(a.especialidade)),
                        {xR + 200, y}, cor::texto);
            const std::string sit = w.situacaoAventureiro(id);
            f_.desenhar(r, sit, {p.x + s.x - 18 - f_.largura(sit), y}, w.aventureiroPronto(id) ? cor::bom : cor::ruim);
            y += 15;
        }
        if (sel >= 0 && sel < static_cast<int>(recrutados.size())) {
            y += 8;
            paragrafo(dg.aventureiro(recrutados[sel])->descricao, cor::apagado);
        }
        rodape = "A/D abas   W/S escolher   Esc fechar";
    } else {
        if (w.reliquias.empty()) paragrafo("Nenhuma relíquia ainda. Expedições podem trazê-las das ruínas da antiga Valdren.", cor::apagado);
        for (size_t i = 0; i < w.reliquias.size(); ++i) {
            const EstadoReliquia& e = w.reliquias[i];
            const ReliquiaDef& d = *dg.reliquia(e.id);
            linhaSel(static_cast<int>(i), 13);
            f_.desenhar(r, d.nome, {xR, y}, static_cast<int>(i) == sel ? cor::ouro : cor::texto);
            const std::string marca = e.exposta ? "exposta" : "guardada";
            f_.desenhar(r, marca, {p.x + s.x - 18 - f_.largura(marca), y}, e.exposta ? cor::bom : cor::apagado);
            y += 14;
        }
        if (sel >= 0 && sel < static_cast<int>(w.reliquias.size())) {
            const EstadoReliquia& e = w.reliquias[sel];
            const ReliquiaDef& d = *dg.reliquia(e.id);
            y = std::max(y + 6, p.y + 180.0f);
            paragrafo(d.texto, cor::texto);
            y += 3;
            paragrafo("Efeito exposta: " + textoEfeitos(d.efeitos), cor::bom);
            if (d.amaldicoada && e.purificada) paragrafo("Purificada por Maelis: a maldição se foi.", cor::bom);
            else if (d.amaldicoada && e.conhecida) paragrafo("Maldição: " + textoEfeitos(d.maldicao), cor::ruim);
            // Botões (500 expor/recolher, 501 purificar)
            y += 4;
            const std::string b1 = e.exposta ? "[E] Recolher" : "[E] Expor";
            f_.desenhar(r, b1, {xR, y}, cor::ouro);
            cliques.area({xR - 4, y - 3}, {f_.largura(b1) + 8.0f, 13}, 500);
            if (d.amaldicoada && e.conhecida && !e.purificada) {
                const std::string b2 = "[P] Purificar com Maelis (3 cristais)";
                const float x2 = xR + f_.largura(b1) + 30.0f;
                f_.desenhar(r, b2, {x2, y}, cor::ouro);
                cliques.area({x2 - 4, y - 3}, {f_.largura(b2) + 8.0f, 13}, 501);
            }
        }
        rodape = "A/D abas   W/S escolher   E expor ou recolher   Esc fechar";
    }
    (void)tempo;
    f_.desenharCentralizado(r, rodape, {320, p.y + s.y - 11}, cor::apagado);
}

// ---------- Fase 11: terras do reino e chefes com escolha ----------

void Hud::painelRegioes(ke::Renderer2D& r, const World& w) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const glm::vec2 p{110, 30}, s{420, 300};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Terras do Reino", {320, p.y + 12}, cor::ouro);
    f_.desenharCentralizado(r, "O que as cinzas ainda dominam fora do vilarejo", {320, p.y + 24}, cor::apagado);
    float y = p.y + 42;
    const float xR = p.x + 16, xV = p.x + 130;
    int mostradas = 0;
    for (const auto& id : w.regioesComVida()) {
        if (!w.visitadas.contains(id)) continue;  // terra que o herdeiro ainda não pisou não aparece
        ++mostradas;
        const EstadoRegiao& e = w.estadoRegioes.at(id);
        const TransformacaoRegiao* t = w.transformacao(id);
        const VidaRegiaoDef& v = w.regioes.at(id).vida;
        // Nome e situação, em palavras
        std::string sit;
        glm::vec4 cs = cor::bom;
        if (e.corrupcao >= regra::INVASAO) sit = "Tomada pelas criaturas", cs = cor::ruim;
        else if (e.corrupcao >= 60.0f) sit = t ? "Ameaçada" : "Dominada pelas cinzas", cs = cor::ruim;
        else if (e.corrupcao >= 30.0f) sit = "Disputada", cs = cor::ouro;
        else sit = t ? "Em paz" : "Calma";
        if (abandonada(e, w.relogio.dia)) sit += std::format(", sem visita há {} dias", w.relogio.dia - e.ultimaVisita);
        // Nome e, ao lado (ou embaixo, se não couber), a situação
        const std::string nome = w.nomeRegiao(id);
        f_.desenhar(r, nome, {xR, y}, cor::ouro);
        if (f_.largura(nome) + f_.largura(sit) + 20 > s.x - 32) {
            y += 12;
            f_.desenhar(r, sit, {xR + 6, y}, cs);
        } else {
            f_.desenhar(r, sit, {p.x + s.x - 16 - f_.largura(sit), y}, cs);
        }
        y += 14;
        auto linhaBarra = [&](const char* rotulo, float valor, glm::vec4 c) {
            f_.desenhar(r, rotulo, {xR + 6, y}, cor::apagado);
            barra(r, {xV, y + 1}, 160, valor / 100.0f, c);
            f_.desenhar(r, std::format("{:.0f}", valor), {xV + 168, y}, cor::texto);
            y += 12;
        };
        linhaBarra("Corrupção", e.corrupcao, e.corrupcao >= 60.0f ? cor::ruim : e.corrupcao >= 30.0f ? cor::ouro : cor::bom);
        linhaBarra("Segurança", e.seguranca, cor::bom);
        linhaBarra("Prosperidade", e.prosperidade, cor::ouro);
        std::string rende;
        for (const auto& [rec, n] : producaoRegiao(e, v, t)) rende += (rende.empty() ? "" : ", ") + std::format("+{} {}", n, dados_.nomeRecurso(rec));
        f_.desenhar(r, "Por noite", {xR + 6, y}, cor::apagado);
        f_.desenhar(r, rende.empty() ? "nada ainda" : rende, {xV, y}, rende.empty() ? cor::apagado : cor::texto);
        y += 12;
        for (const auto& l : quebrarLinhas(f_, t ? t->dica : v.dica, static_cast<int>(s.x) - 44)) f_.desenhar(r, l, {xR + 6, y}, cor::apagado), y += 10;
        y += 10;
    }
    if (mostradas == 0)
        for (const auto& l : quebrarLinhas(f_, "Nenhuma terra explorada ainda. A estrada leste leva à floresta.", static_cast<int>(s.x) - 40))
            f_.desenhar(r, l, {xR, y}, cor::apagado), y += 11;
    f_.desenharCentralizado(r, "Vencer criaturas baixa a corrupção. Terra abandonada piora a cada noite.", {320, p.y + s.y - 26}, cor::apagado);
    f_.desenharCentralizado(r, "R ou Esc fechar", {320, p.y + s.y - 12}, cor::texto);
    rodapeFechar("R ou Esc fechar", {320, p.y + s.y - 12});
}

void Hud::painelEscolha(ke::Renderer2D& r, const World& w, const EscolhaDef& d, int sel, float tempo) {
    r.retangulo({0, 0}, {640, 360}, {0.08f, 0.02f, 0.02f, 0.55f});
    const int largura = 392;
    std::vector<std::string> texto;
    for (const auto& par : d.texto)
        for (const auto& l : quebrarLinhas(f_, par, largura)) texto.push_back(l);
    std::vector<std::vector<std::string>> descricoes;
    float altOpcoes = 0.0f;
    for (const auto& o : d.opcoes) {
        descricoes.push_back(quebrarLinhas(f_, o.descricao, largura - 20));
        altOpcoes += 16.0f + descricoes.back().size() * 10.0f + (w.opcaoDisponivel(o) ? 0.0f : 10.0f) + 6.0f;
    }
    const float alt = 46.0f + texto.size() * 11.0f + 10.0f + altOpcoes + 22.0f;
    const glm::vec2 p{100, std::floor(180 - alt / 2)}, s{440, alt};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    const float pulso = 0.85f + 0.15f * std::sin(tempo * 4.0f);
    f_.desenharCentralizado(r, d.titulo, {320, p.y + 14}, cor::ruim * glm::vec4(pulso, pulso, pulso, 1), true, 1);
    float y = p.y + 34;
    for (const auto& l : texto) f_.desenhar(r, l, {p.x + 24, y}, cor::texto), y += 11;
    y += 10;
    for (size_t i = 0; i < d.opcoes.size(); ++i) {
        const OpcaoEscolha& o = d.opcoes[i];
        const bool marcada = static_cast<int>(i) == sel;
        const bool pode = w.opcaoDisponivel(o);
        const float h = 16.0f + descricoes[i].size() * 10.0f + (pode ? 0.0f : 10.0f);
        if (marcada) r.retangulo({p.x + 10, y - 4}, {s.x - 20, h + 2}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 10, y - 4}, {s.x - 20, h + 2}, static_cast<int>(i));
        f_.desenhar(r, o.nome, {p.x + 20, y}, !pode ? cor::apagado : marcada ? cor::ouro : cor::texto);
        y += 13;
        for (const auto& l : descricoes[i]) f_.desenhar(r, l, {p.x + 30, y}, cor::apagado), y += 10;
        if (!pode) f_.desenhar(r, o.requerTexto, {p.x + 30, y}, cor::ruim), y += 10;
        y += 9;
    }
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E ou clique decidir   Esc pensar mais um pouco", {320, p.y + s.y - 12}, cor::apagado);
}


// ---------- Fase 15: o fogão ----------

void Hud::painelCozinha(ke::Renderer2D& r, const World& w, int sel, float tempo) {
    const auto& receitas = dados_.cotidiano.receitas;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const float alt = 84.0f + receitas.size() * 26.0f;
    const glm::vec2 p{70, std::floor(180 - alt / 2)}, s{500, alt};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Fogão do Herdeiro", {320, p.y + 12}, cor::ouro);
    const Jogador& j = w.jogador;
    f_.desenharCentralizado(r, std::format("Vida {}/{}   Comida {}", j.vida, j.vidaMax, w.inventario.quantidade("comida")), {320, p.y + 24},
                            cor::apagado);
    for (size_t i = 0; i < receitas.size(); ++i) {
        const ReceitaDef& d = receitas[i];
        const float y = p.y + 40 + i * 26.0f;
        const bool s1 = static_cast<int>(i) == sel;
        const bool pode = w.podeCozinhar(d);
        const int tem = w.inventario.quantidade(d.id);
        if (s1) r.retangulo({p.x + 8, y - 3}, {s.x - 16, 25}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 3}, {s.x - 16, 25}, static_cast<int>(i));
        f_.desenhar(r, d.nome, {p.x + 14, y}, s1 ? cor::ouro : pode ? cor::texto : cor::apagado);
        f_.desenhar(r, textoPrato(d.efeitos), {p.x + 188, y}, d.efeitos.felicidade != 0.0f ? cor::ouro : cor::bom);
        const std::string t = tem > 0 ? std::format("você tem {}", tem) : "";
        f_.desenhar(r, t, {p.x + s.x - 14 - f_.largura(t), y + 11}, cor::texto);
        custoEmLinha(r, d.ingredientes, w.inventario, {p.x + 24, y + 11});
    }
    // Botão de comer o prato selecionado (se houver)
    const ReceitaDef& d = receitas[std::clamp(sel, 0, static_cast<int>(receitas.size()) - 1)];
    const bool temEste = w.inventario.quantidade(d.id) > 0;
    const std::string comer = d.efeitos.felicidade != 0.0f ? "F  servir ao povo" : "F  comer agora";
    const glm::vec2 bc{320, p.y + s.y - 30};
    const float lb = static_cast<float>(f_.largura(comer)) + 16;
    r.retangulo(bc - glm::vec2(lb / 2, 6), {lb, 14}, temEste ? glm::vec4(0.25f, 0.4f, 0.22f, 0.8f + 0.1f * std::sin(tempo * 4.0f)) : glm::vec4(0.15f, 0.13f, 0.12f, 0.8f));
    if (temEste) cliques.area(bc - glm::vec2(lb / 2, 6), {lb, 14}, 500);
    f_.desenharCentralizado(r, comer, bc + glm::vec2(0, 1), temEste ? cor::texto : cor::apagado);
    f_.desenharCentralizado(r, d.descricao, {320, p.y + s.y - 14}, cor::apagado);
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E ou clique cozinhar   Esc fechar", {320, p.y + s.y + 10}, cor::texto);
    rodapeFechar("Esc fechar", {320 + f_.largura("W/S ou mouse escolher   E ou clique cozinhar   ") / 2.0f, p.y + s.y + 10});
}

// ---------- Fase 16: o castelo e o brasão ----------

void Hud::painelAlas(ke::Renderer2D& r, const World& w, int sel, float tempo) {
    const auto& alas = dados_.castelo.alas;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.45f});
    const float alt = 70.0f + alas.size() * 30.0f;
    const glm::vec2 p{70, std::floor(180 - alt / 2)}, s{500, alt};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    const int vagas = w.vagasAlas(), usadas = static_cast<int>(w.alas.size());
    f_.desenharCentralizado(r, "Alas do Castelo", {320, p.y + 12}, cor::ouro);
    f_.desenharCentralizado(r, vagas == 0 ? "Reconstrua a Torre de Menagem para abrir a primeira vaga."
                                          : std::format("{} de {} vagas usadas (cada nível do castelo abre mais uma)", usadas, vagas),
                            {320, p.y + 24}, vagas > usadas ? cor::bom : cor::apagado);
    for (size_t i = 0; i < alas.size(); ++i) {
        const AlaDef& a = alas[i];
        const float y = p.y + 40 + i * 30.0f;
        const bool s1 = static_cast<int>(i) == sel;
        const bool erguida = std::ranges::find(w.alas, a.id) != w.alas.end();
        if (s1) r.retangulo({p.x + 8, y - 3}, {s.x - 16, 29}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 3}, {s.x - 16, 29}, static_cast<int>(i));
        f_.desenhar(r, a.nome, {p.x + 14, y}, erguida ? cor::bom : s1 ? cor::ouro : cor::texto);
        f_.desenhar(r, textoAla(a.efeitos), {p.x + 150, y}, cor::bom);
        if (erguida) f_.desenhar(r, "erguida", {p.x + s.x - 14 - f_.largura("erguida"), y + 12}, cor::bom);
        else custoEmLinha(r, a.custo, w.inventario, {p.x + 24, y + 12});
    }
    const AlaDef& d = alas[std::clamp(sel, 0, static_cast<int>(alas.size()) - 1)];
    const float a = 0.8f + 0.2f * std::sin(tempo * 3.0f);
    f_.desenharCentralizado(r, d.descricao, {320, p.y + s.y - 14}, cor::apagado * glm::vec4(a, a, a, 1));
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E ou clique erguer   Esc fechar", {320, p.y + s.y + 10}, cor::texto);
    rodapeFechar("Esc fechar", {320 + f_.largura("W/S ou mouse escolher   E ou clique erguer   ") / 2.0f, p.y + s.y + 10});
}

void Hud::editorBrasao(ke::Renderer2D& r, const World& w, const Brasao& b, int sel, float tempo) {
    const DadosCastelo& dc = dados_.castelo;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.55f});
    const glm::vec2 p{50, 26}, s{540, 300};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Brasão da Casa de Valdren", {320, p.y + 12}, cor::ouro, true, 1);
    static const char* rotulos[LINHAS_BRASAO] = {"Forma", "Divisão", "Cor do campo", "Segunda cor", "Símbolo", "Cor do símbolo", "Lema"};
    const std::string valores[LINHAS_BRASAO] = {dc.nomePeca(0, b.forma),   dc.nomePeca(1, b.divisao), dc.nomePeca(2, b.cor1),
                                                dc.nomePeca(3, b.cor2),    dc.nomePeca(4, b.simbolo), dc.nomePeca(5, b.corSimbolo),
                                                std::format("{} de {}", b.lema + 1, dc.lemas.size())};
    const std::string* cores[LINHAS_BRASAO] = {nullptr, nullptr, &b.cor1, &b.cor2, nullptr, &b.corSimbolo, nullptr};
    const float xR = p.x + 18, xV = p.x + 142;
    for (int i = 0; i < LINHAS_BRASAO; ++i) {
        const float y = p.y + 44 + i * 26.0f;
        const bool s1 = i == sel;
        if (s1) r.retangulo({p.x + 10, y - 6}, {286, 22}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 10, y - 6}, {286, 22}, i);
        f_.desenhar(r, rotulos[i], {xR, y}, s1 ? cor::ouro : cor::apagado);
        // < valor >, com as setas clicáveis
        f_.desenhar(r, "<", {xV, y}, cor::ouro);
        cliques.area({xV - 5, y - 5}, {16, 18}, 700 + i);
        float xv = xV + 14;
        if (cores[i]) {
            r.retangulo({xv, y - 1}, {10, 10}, corTintura(dc, *cores[i]));
            r.contorno({xv, y - 1}, {10, 10}, cor::borda);
            xv += 14;
        }
        f_.desenhar(r, valores[i], {xv, y}, cor::texto);
        f_.desenhar(r, ">", {xV + 140, y}, cor::ouro);
        cliques.area({xV + 135, y - 5}, {16, 18}, 720 + i);
    }
    // Prévia grande do escudo, a descrição dos arautos e o lema
    const float esc = 5.0f;
    const glm::vec2 pe{p.x + 410 - TAMANHO_ESCUDO.x * esc / 2, p.y + 34};
    desenharEscudo(r, rec_, dc, b, pe, esc);
    float y = pe.y + TAMANHO_ESCUDO.y * esc + 10;
    for (const auto& l : quebrarLinhas(f_, textoBrasao(dc, b) + ".", 210)) f_.desenharCentralizado(r, l, {p.x + 410, y}, cor::texto), y += 11;
    if (b.lema >= 0 && b.lema < static_cast<int>(dc.lemas.size()))
        for (const auto& l : quebrarLinhas(f_, "\"" + dc.lemas[b.lema] + "\"", 210)) f_.desenharCentralizado(r, l, {p.x + 410, y + 4}, cor::ouro), y += 11;
    // Os estandartes da praça mostram como fica no pano
    desenharPano(r, rec_, dc, b, {p.x + 30, p.y + 236}, 2.0f);
    f_.desenhar(r, "Nos estandartes do reino", {p.x + 66, p.y + 250}, cor::apagado);
    f_.desenhar(r, "e nas bandeiras do castelo", {p.x + 66, p.y + 262}, cor::apagado);
    // Botões
    const glm::vec2 bs{p.x + 170, p.y + s.y - 26};
    const float pulso = 0.85f + 0.15f * std::sin(tempo * 4.0f);
    r.retangulo(bs, {90, 16}, {0.25f, 0.4f, 0.22f, 0.85f * pulso});
    cliques.area(bs, {90, 16}, 800);
    f_.desenharCentralizado(r, "Salvar (E)", bs + glm::vec2(45, 8), cor::texto);
    const glm::vec2 bc{p.x + 280, p.y + s.y - 26};
    r.retangulo(bc, {90, 16}, {0.3f, 0.15f, 0.12f, 0.85f});
    cliques.area(bc, {90, 16}, Cliques::FECHAR);
    f_.desenharCentralizado(r, "Cancelar (Esc)", bc + glm::vec2(45, 8), cor::texto);
    f_.desenharCentralizado(r, w.brasaoCriado ? "W/S linha   A/D ou setas trocar" : "W/S linha   A/D ou setas trocar   (o brasão ainda é o dos reis antigos)",
                            {320, p.y + s.y + 10}, cor::texto);
}

// ---------- Fase 17: o histórico das mensagens ----------

void Hud::painelMensagens(ke::Renderer2D& r, int rolagem) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.5f});
    const glm::vec2 p{80, 28}, s{480, 300};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Mensagens", {320, p.y + 12}, cor::ouro);
    float y = p.y + 32;
    const float fim = p.y + s.y - 24;
    if (historico_.empty()) f_.desenharCentralizado(r, "Nenhuma mensagem ainda.", {320, y + 20}, cor::apagado);
    for (int i = std::max(0, rolagem); i < static_cast<int>(historico_.size()); ++i) {
        const Mensagem& m = historico_[i];
        const auto linhas = quebrarLinhas(f_, m.texto, static_cast<int>(s.x) - 160);
        if (y + 11.0f * linhas.size() > fim) break;
        f_.desenhar(r, m.quando, {p.x + 14, y}, cor::apagado);
        for (const auto& l : linhas) f_.desenhar(r, l, {p.x + 140, y}, m.cor), y += 11;
        y += 3;
    }
    const std::string rodape = std::format("W/S ou roda rolar   N ou Esc fechar   ({} mensagens)", historico_.size());
    f_.desenharCentralizado(r, rodape, {320, p.y + s.y - 12}, cor::apagado);
    rodapeFechar("N ou Esc fechar", {320 + (f_.largura("W/S ou roda rolar   ") - f_.largura(std::format("   ({} mensagens)", historico_.size()))) / 2.0f, p.y + s.y - 12});
}

// ---------- Fase 18: o povo com nome ----------

namespace {

// Corta o texto na largura (sem partir letra acentuada) e põe reticências
std::string caber(const ke::BitmapFont& f, std::string t, int largura) {
    if (f.largura(t) <= largura) return t;
    while (!t.empty() && f.largura(t + "...") > largura) {
        t.pop_back();
        while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back();
        if (!t.empty() && static_cast<unsigned char>(t.back()) >= 0xC0) t.pop_back();
    }
    return t + "...";
}

// "Ouro" vira "ouro" (os nomes dos recursos começam com letra sem acento)
std::string minusculo(std::string t) {
    if (!t.empty()) t[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(t[0])));
    return t;
}

std::string oficioDe(const DadosPovo& d, const Habitante& h) {
    const ProfissaoDef* p = d.profissao(h.profissao);
    const TracoDef* t = d.traco(h.traco);
    return std::format("{}, {}", p ? p->nome : "sem ofício", t ? t->nome : "comum");
}

}

void Hud::fichaHabitante(ke::Renderer2D& r, const World& w, int id) {
    const Habitante* h = w.habitante(id);
    if (!h) return;
    const DadosPovo& dp = dados_.povo;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.35f});
    const glm::vec2 p{150, 74}, s{340, 196};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    // Retrato: o aldeão com a roupa dele, ampliado
    const ke::SpriteRegion& sp = rec_.regiao(std::format("aldeao_{}_baixo_0", h->variante));
    if (sp.textura) {
        r.retangulo(p + glm::vec2(12, 12), {52, 60}, {0.12f, 0.10f, 0.09f, 1});
        r.contorno(p + glm::vec2(12, 12), {52, 60}, cor::borda);
        r.sprite(sp, p + glm::vec2(14, 14), glm::vec4(1.0f), false, 2.0f);
    }
    const float x = p.x + 76;
    float y = p.y + 14;
    f_.desenhar(r, h->nomeCompleto(), {x, y}, cor::ouro);
    y += 12;
    f_.desenhar(r, oficioDe(dp, *h), {x, y}, cor::apagado);
    y += 12;
    const Relogio chegada{h->chegou};
    f_.desenhar(r, std::format("Mora aqui desde {}, dia {}", nomeEstacao(chegada.estacao()), chegada.diaDaEstacao()), {x, y}, cor::apagado);
    y += 16;
    const auto familia = w.parentes(*h);
    std::string nomes;
    for (size_t i = 0; i < familia.size() && i < 4; ++i) nomes += (i ? ", " : "") + familia[i]->nome;
    if (familia.size() > 4) nomes += std::format(" e mais {}", familia.size() - 4);
    for (const auto& l : quebrarLinhas(f_, familia.empty() ? "Família: chegou sozinho." : "Família: " + nomes, static_cast<int>(p.x + s.x - x - 12)))
        f_.desenhar(r, l, {x, y}, cor::texto), y += 11;
    y = std::max(y + 6, p.y + 84);
    const DesejoDef* d = dp.desejo(h->desejo);
    const TracoDef* t = dp.traco(h->traco);
    if (d) {
        f_.desenhar(r, "Deseja " + d->texto + ".", {p.x + 14, y}, cor::texto);
        y += 12;
        const bool impaciente = t && h->esperando > t->paciencia;
        const std::string estado = h->cumprido ? "Desejo realizado." : std::format("Esperando há {} dia{}{}", h->esperando, h->esperando == 1 ? "" : "s",
                                                                                        impaciente ? " (perdendo a paciência)" : "");
        f_.desenhar(r, estado, {p.x + 14, y}, h->cumprido ? cor::bom : impaciente ? cor::ruim : cor::ouro);
        y += 16;
    }
    const std::string fala = h->cumprido || !d ? "\"Valdren é a minha casa agora.\"" : "\"" + d->fala + "\"";
    for (const auto& l : quebrarLinhas(f_, fala, static_cast<int>(s.x) - 28)) f_.desenhar(r, l, {p.x + 14, y}, cor::ouro), y += 11;
    f_.desenharCentralizado(r, "E ou Esc fechar", {320, p.y + s.y - 12}, cor::apagado);
    rodapeFechar("E ou Esc fechar", {320, p.y + s.y - 12});
}

void Hud::painelPovo(ke::Renderer2D& r, const World& w, int sel, int rolagem) {
    const DadosPovo& dp = dados_.povo;
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.5f});
    const glm::vec2 p{50, 22}, s{540, 316};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    int realizados = 0;
    for (const Habitante& h : w.povo) realizados += h.cumprido;
    f_.desenharCentralizado(r, std::format("O povo de Valdren: {} pessoas, {} desejos realizados", w.povo.size() + w.moradoresComNome(), realizados),
                            {320, p.y + 12}, cor::ouro);
    f_.desenharCentralizado(r, "Os moradores do conselho e da praça não entram na lista", {320, p.y + 24}, cor::apagado);
    float y = p.y + 42;
    const int n = static_cast<int>(w.povo.size());
    for (int i = std::max(0, rolagem); i < n && i < rolagem + linhasPovo(); ++i) {
        const Habitante& h = w.povo[i];
        const bool s1 = i == sel;
        if (s1) r.retangulo({p.x + 8, y - 3}, {s.x - 16, 13}, {0.45f, 0.36f, 0.22f, 0.45f});
        cliques.area({p.x + 8, y - 3}, {s.x - 16, 13}, i);
        f_.desenhar(r, caber(f_, h.nomeCompleto(), 150), {p.x + 14, y}, s1 ? cor::ouro : cor::texto);
        f_.desenhar(r, caber(f_, oficioDe(dp, h), 140), {p.x + 172, y}, cor::apagado);
        const DesejoDef* d = dp.desejo(h.desejo);
        const std::string desejo = d ? (h.cumprido ? "realizou: " : "deseja ") + d->texto : "";
        f_.desenhar(r, caber(f_, desejo, static_cast<int>(s.x) - 330), {p.x + 320, y}, h.cumprido ? cor::bom : cor::apagado);
        y += 14;
    }
    if (n == 0) f_.desenharCentralizado(r, "Ainda não chegou ninguém além dos sobreviventes.", {320, y + 20}, cor::apagado);
    f_.desenharCentralizado(r, "W/S ou roda   E ou clique: ficha   P ou Esc fechar", {320, p.y + s.y - 12}, cor::apagado);
    rodapeFechar("P ou Esc fechar", {320 + f_.largura("W/S ou roda   E ou clique: ficha   ") / 2.0f, p.y + s.y - 12});
}

// ---------- Fase 19: os vizinhos ----------

std::vector<std::string> Hud::acoesVizinhos() const {
    std::vector<std::string> v;
    for (const FaccaoDef& f : dados_.faccoes.faccoes) {
        for (const TratadoDef& t : dados_.faccoes.tratados)
            if (t.faccao == f.id) v.push_back("t:" + t.id);
        for (const MissaoDef& m : dados_.missoes)
            if (m.faccao == f.id) v.push_back("m:" + m.id);
    }
    return v;
}

void Hud::painelVizinhos(ke::Renderer2D& r, const World& w, int sel, float tempo) {
    r.retangulo({0, 0}, {640, 360}, {0, 0, 0, 0.5f});
    const glm::vec2 p{36, 14}, s{568, 332};
    desenharPainel(r, p, s);
    painelClicavel(p, s);
    f_.desenharCentralizado(r, "Os Vizinhos de Valdren", {320, p.y + 11}, cor::ouro);
    const DadosFaccoes& df = dados_.faccoes;
    // A reputação: três barras de -100 a 100, com o nome de cada ponta (centralizadas)
    static const std::tuple<const char*, const char*, const char*> eixos[] = {
        {"justica", "Tirano", "Justo"}, {"generosidade", "Avarento", "Generoso"}, {"devocao", "Arcano", "Devoto"}};
    constexpr float LB = 44.0f, GAP = 18.0f;
    float total = -GAP;
    for (const auto& [eixo, neg, pos] : eixos) total += f_.largura(neg) + 5 + LB + 5 + f_.largura(pos) + GAP;
    float x = 320 - total / 2;
    for (const auto& [eixo, neg, pos] : eixos) {
        const float v = w.reputacao.eixo(eixo);
        f_.desenhar(r, neg, {x, p.y + 26}, v <= -df.limiteFama ? cor::ruim : cor::apagado);
        const float bx = x + f_.largura(neg) + 5;
        r.retangulo({bx, p.y + 28}, {LB, 4}, {0.12f, 0.1f, 0.09f, 1});
        r.retangulo({bx + LB / 2, p.y + 26}, {1, 8}, cor::borda);
        r.retangulo({std::floor(bx + LB / 2 + v / 100.0f * LB / 2) - 1, p.y + 25}, {3, 10}, cor::ouro);
        f_.desenhar(r, pos, {bx + LB + 5, p.y + 26}, v >= df.limiteFama ? cor::bom : cor::apagado);
        x = bx + LB + 5 + f_.largura(pos) + GAP;
    }
    r.retangulo({p.x + 12, p.y + 40}, {s.x - 24, 1}, cor::borda);
    // Cada povo: relação e, em duas colunas, tratados (esquerda) e missões (direita); W/S segue a ordem de acoesVizinhos
    const auto acoes = acoesVizinhos();
    const float cw = (s.x - 28) / 2;
    float y = p.y + 46;
    int k = 0;
    for (const FaccaoDef& f : df.faccoes) {
        const float rel = w.relacaoCom(f.id);
        const glm::vec4 cr = rel >= 10 ? cor::bom : rel <= -10 ? cor::ruim : cor::texto;
        f_.desenhar(r, f.nome, {p.x + 14, y}, cor::ouro);
        const std::string faixa = std::format("{:+.0f} {}", rel, w.faixaCom(f.id));
        const float fx = p.x + s.x - 14 - f_.largura(faixa);
        f_.desenhar(r, faixa, {fx, y}, cr);
        barra(r, {fx - 78, y + 1}, 70, (rel + 100.0f) / 200.0f, cr);
        y += 13;
        int linhas = 0, nt = 0, nm = 0;
        while (k < static_cast<int>(acoes.size())) {
            const bool tratado = acoes[k].starts_with("t:");
            const std::string id = acoes[k].substr(2);
            std::string nome, detalhe, estado;
            glm::vec4 corEstado = cor::apagado;
            if (tratado) {
                const TratadoDef* t = df.tratado(id);
                if (!t || t->faccao != f.id) break;
                nome = t->nome;
                detalhe = textoTratado(t->efeitos);
                if (w.tratados.contains(id)) estado = "em vigor", corEstado = cor::bom;
                else if (w.relacaoCom(f.id) < t->requer) estado = std::format("pede {:+d}", t->requer);
                else if (!w.inventario.tem(t->custo)) estado = "sem recursos", corEstado = cor::ruim;
                else estado = "E assina", corEstado = cor::ouro;
            } else {
                const auto it = std::ranges::find(dados_.missoes, id, &MissaoDef::id);
                if (it == dados_.missoes.end() || it->faccao != f.id) break;
                nome = it->nome;
                detalhe = it->descricao;
                const auto em = w.missoes.find(id);
                if (em == w.missoes.end()) estado = "E aceita", corEstado = cor::ouro;
                else if (em->second.concluida) estado = "cumprida", corEstado = cor::bom;
                else estado = std::format("{}/{}", std::min(w.progressoMissao(*it), it->objetivo.quantidade), it->objetivo.quantidade);
            }
            const int linha = tratado ? nt++ : nm++;
            linhas = std::max(linhas, linha + 1);
            const glm::vec2 o{p.x + 10 + (tratado ? 0.0f : cw + 8), y + linha * 22.0f};
            const bool s1 = k == sel;
            if (s1) r.retangulo({o.x, o.y - 2}, {cw, 22}, {0.45f, 0.36f, 0.22f, 0.4f + 0.05f * std::sin(tempo * 4.0f)});
            cliques.area({o.x, o.y - 2}, {cw, 22}, k);
            f_.desenhar(r, estado, {o.x + cw - 4 - f_.largura(estado), o.y}, corEstado);
            f_.desenhar(r, caber(f_, nome, static_cast<int>(cw) - 16 - f_.largura(estado)), {o.x + 4, o.y}, s1 ? cor::ouro : cor::texto);
            f_.desenhar(r, caber(f_, detalhe, static_cast<int>(cw) - 14), {o.x + 10, o.y + 10}, cor::apagado);
            ++k;
        }
        y += linhas * 22.0f + 6;
    }
    // Detalhes da linha escolhida: descrição, efeitos, custo e o que falta
    r.retangulo({p.x + 12, y}, {s.x - 24, 1}, cor::borda);
    y += 5;
    if (sel >= 0 && sel < static_cast<int>(acoes.size())) {
        const std::string id = acoes[sel].substr(2);
        const float lx = p.x + 16;
        const int larg = static_cast<int>(s.x) - 32;
        auto descricao = [&](const std::string& t) {
            const auto ls = quebrarLinhas(f_, t, larg);
            for (size_t i = 0; i < std::min<size_t>(2, ls.size()); ++i, y += 11) f_.desenhar(r, ls[i], {lx, y}, cor::texto);
        };
        const FaccaoDef* f = nullptr;
        if (acoes[sel].starts_with("t:")) {
            const TratadoDef* t = df.tratado(id);
            f = df.faccao(t->faccao);
            f_.desenhar(r, f->nome + " - Tratado: " + t->nome, {lx, y}, cor::ouro);
            y += 12;
            descricao(t->descricao);
            f_.desenhar(r, "Todo dia: " + textoTratado(t->efeitos), {lx, y}, cor::bom);
            y += 12;
            const std::string falta = w.podeAssinar(id);
            float cx = lx;
            if (!t->custo.empty()) {
                f_.desenhar(r, "Custa:", {lx, y}, cor::apagado);
                custoEmLinha(r, t->custo, w.inventario, {lx + f_.largura("Custa:") + 6, y});
                cx = lx + 200;
            }
            const std::string estado = w.tratados.contains(id) ? "Em vigor. Romper custa 15 de relação."
                                       : falta.empty()         ? "Pronto para assinar (E)."
                                                               : falta;
            f_.desenhar(r, caber(f_, estado, static_cast<int>(p.x + s.x - 16 - cx)), {cx, y}, falta.empty() ? cor::bom : cor::apagado);
            y += 12;
        } else {
            const auto it = std::ranges::find(dados_.missoes, id, &MissaoDef::id);
            f = df.faccao(it->faccao);
            f_.desenhar(r, f->nome + " - Missão: " + it->nome, {lx, y}, cor::ouro);
            y += 12;
            descricao(it->descricao);
            std::string premio;
            for (const auto& [rid, q] : it->recompensa) premio += std::format("{}{} de {}", premio.empty() ? "" : ", ", q, minusculo(dados_.nomeRecurso(rid)));
            premio += std::format("{}+{} de relação", premio.empty() ? "" : " e ", it->relacao);
            f_.desenhar(r, "Recompensa: " + premio, {lx, y}, cor::bom);
            y += 12;
        }
        f_.desenhar(r, caber(f_, std::format("{}, em {}. {}", f->lider, w.nomeRegiao(f->territorio), f->descricao), larg), {lx, y}, cor::apagado);
    }
    f_.desenharCentralizado(r, "W/S ou mouse escolher   E assina, rompe ou aceita   T ou Esc fechar", {320, p.y + s.y - 10}, cor::apagado);
    rodapeFechar("T ou Esc fechar", {320 + (f_.largura("W/S ou mouse escolher   E assina, rompe ou aceita   ")) / 2.0f, p.y + s.y - 10});
}

}
