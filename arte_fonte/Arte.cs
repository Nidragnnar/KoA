// Processamento de imagem da arte final (carregado pelo arte.ps1 com Add-Type; C# 5)
// Monta a imagem base para o NidraIA e converte o resultado no sprite exato do atlas.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class Arte {
    // ---------- Leitura e gravação (pixels ARGB em int[]) ----------
    public static int[] Ler(string arquivo, out int w, out int h) {
        using (var bmp0 = new Bitmap(arquivo))
        using (var bmp = new Bitmap(bmp0.Width, bmp0.Height, PixelFormat.Format32bppArgb)) {
            using (var g = Graphics.FromImage(bmp)) {
                g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
                g.DrawImage(bmp0, 0, 0, bmp0.Width, bmp0.Height);
            }
            w = bmp.Width; h = bmp.Height;
            var px = new int[w * h];
            var d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            for (int y = 0; y < h; y++) Marshal.Copy(d.Scan0 + y * d.Stride, px, y * w, w);
            bmp.UnlockBits(d);
            return px;
        }
    }

    public static void Gravar(string arquivo, int[] px, int w, int h) {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(arquivo)));
        using (var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb)) {
            var d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            for (int y = 0; y < h; y++) Marshal.Copy(px, y * w, d.Scan0 + y * d.Stride, w);
            bmp.UnlockBits(d);
            bmp.Save(arquivo, ImageFormat.Png);
        }
    }

    static int A(int c) { return (c >> 24) & 255; }
    static int R(int c) { return (c >> 16) & 255; }
    static int G(int c) { return (c >> 8) & 255; }
    static int B(int c) { return c & 255; }
    static int Cor(int r, int g, int b, int a) {
        Func<int, int> f = v => Math.Max(0, Math.Min(255, v));
        return (f(a) << 24) | (f(r) << 16) | (f(g) << 8) | f(b);
    }
    static int Cor(double r, double g, double b) { return Cor((int)Math.Round(r), (int)Math.Round(g), (int)Math.Round(b), 255); }
    static double Lum(double r, double g, double b) { return 0.299 * r + 0.587 * g + 0.114 * b; }

    // ---------- Imagem base para o NidraIA ----------
    // Procedural ampliado sem borrar, centralizado sobre fundo sólido. Pixels semitransparentes
    // (sombras no chão) viram fundo. Devolve a escala e a posição para a conversão desfazer depois.
    public static int[] MontarBase(string proc, int larguraTela, int alturaTela, int fundo, string saida) {
        int w, h;
        var p = Ler(proc, out w, out h);
        int escala = Math.Min((larguraTela - 64) / w, (alturaTela - 64) / h);
        int ox = (larguraTela - w * escala) / 2, oy = (alturaTela - h * escala) / 2;
        var tela = new int[larguraTela * alturaTela];
        for (int i = 0; i < tela.Length; i++) tela[i] = fundo | unchecked((int)0xFF000000);
        for (int y = 0; y < h * escala; y++)
            for (int x = 0; x < w * escala; x++) {
                int c = p[(y / escala) * w + x / escala];
                if (A(c) == 255) tela[(oy + y) * larguraTela + ox + x] = c;
            }
        Gravar(saida, tela, larguraTela, alturaTela);
        return new int[] { escala, ox, oy };
    }

    // Prancha de quadros (herói): todos na mesma imagem para o NidraIA desenhar com o mesmo estilo.
    // Devolve [escala, ox0, oy0, ox1, oy1, ...] (posição de cada quadro na imagem base)
    public static int[] MontarPrancha(string[] procs, int colunas, int larguraTela, int alturaTela, int fundo, string saida) {
        int w, h;
        var quadros = new List<int[]>();
        int qw = 0, qh = 0;
        foreach (var p in procs) { quadros.Add(Ler(p, out w, out h)); qw = w; qh = h; }
        int linhas = (procs.Length + colunas - 1) / colunas, folga = 4;  // folga entre quadros, em pixels do sprite
        int escala = Math.Min((larguraTela - 64) / (colunas * (qw + folga)), (alturaTela - 64) / (linhas * (qh + folga)));
        int largura = colunas * (qw + folga) * escala, altura = linhas * (qh + folga) * escala;
        int bx = (larguraTela - largura) / 2, by = (alturaTela - altura) / 2;
        var tela = new int[larguraTela * alturaTela];
        for (int i = 0; i < tela.Length; i++) tela[i] = fundo | unchecked((int)0xFF000000);
        var resultado = new List<int> { escala };
        for (int q = 0; q < quadros.Count; q++) {
            int ox = bx + ((q % colunas) * (qw + folga) + folga / 2) * escala, oy = by + ((q / colunas) * (qh + folga) + folga / 2) * escala;
            resultado.Add(ox); resultado.Add(oy);
            for (int y = 0; y < qh * escala; y++)
                for (int x = 0; x < qw * escala; x++) {
                    int c = quadros[q][(y / escala) * qw + x / escala];
                    if (A(c) == 255) tela[(oy + y) * larguraTela + ox + x] = c;
                }
        }
        Gravar(saida, tela, larguraTela, alturaTela);
        return resultado.ToArray();
    }

    // Fundo por chroma key: perto da cor de fundo, ou com o mesmo matiz dominante (magenta ou verde)
    static bool EhFundo(int c, int fundo, int tolerancia) {
        int dr = R(c) - R(fundo), dg = G(c) - G(fundo), db = B(c) - B(fundo);
        if (dr * dr + dg * dg + db * db < tolerancia * tolerancia) return true;
        bool magenta = R(fundo) > 200 && B(fundo) > 200 && G(fundo) < 60;
        if (magenta) return R(c) - G(c) > 30 && B(c) - G(c) > 30 && Math.Abs(R(c) - B(c)) < 90;
        bool verde = G(fundo) > 200 && R(fundo) < 60 && B(fundo) < 60;
        if (verde) return G(c) - R(c) > 70 && G(c) - B(c) > 70;
        return false;
    }
    // ---------- Conversão de objetos e construções ----------
    // bruta: imagem do NidraIA; procs: sprites procedurais (tamanho alvo, silhueta e brilho de referência)
    // posicoes: [escala, ox0, oy0, ox1, oy1, ...] como devolvido por MontarBase/MontarPrancha
    // mascara: "ia" (silhueta da IA) ou "procedural" (recorta pela silhueta do procedural)
    // Vários quadros dividem a mesma paleta (herói: quadros consistentes entre si)
    public static void ConverterObjetos(string bruta, string[] procs, string[] saidas, int[] posicoes, int fundo,
                                        string mascara, int cores, double saturacao, double ajusteBrilho, int contorno, bool caixa) {
        int bw, bh;
        var b = Ler(bruta, out bw, out bh);
        int escala = posicoes[0];
        var saidasPx = new List<int[]>(); var refs = new List<int[]>(); var ws = new List<int>(); var hs = new List<int>();
        for (int q = 0; q < procs.Length; q++) {
            int w, h;
            var p = Ler(procs[q], out w, out h);
            int ox = posicoes[1 + q * 2], oy = posicoes[2 + q * 2];
            // Região da bruta que vira o sprite: a mesma do procedural ampliado, ou (caixa) a caixa do
            // objeto desenhado, encaixada no sprite sem deformar e ancorada embaixo no centro
            double s = escala, sx = ox, sy = oy;
            if (caixa) {
                int x0 = int.MaxValue, y0 = int.MaxValue, x1 = -1, y1 = -1;
                for (int y = oy; y < Math.Min(bh, oy + h * escala); y++)
                    for (int x = ox; x < Math.Min(bw, ox + w * escala); x++)
                        if (!EhFundo(b[y * bw + x], fundo, 90)) { x0 = Math.Min(x0, x); x1 = Math.Max(x1, x); y0 = Math.Min(y0, y); y1 = Math.Max(y1, y); }
                if (x1 > x0 && y1 > y0) {
                    s = Math.Max((x1 - x0 + 1) / (double)w, (y1 - y0 + 1) / (double)h);
                    sx = (x0 + x1 + 1) / 2.0 - w * s / 2; sy = y1 + 1 - h * s;
                }
            }
            var o = new int[w * h];
            // Redução por área: cada pixel do sprite é a média do seu bloco na bruta (só o que não é fundo)
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    double sr = 0, sg = 0, sb = 0; int n = 0, total = 0;
                    int xa = (int)Math.Floor(sx + x * s), xb = (int)Math.Floor(sx + (x + 1) * s);
                    int ya = (int)Math.Floor(sy + y * s), yb = (int)Math.Floor(sy + (y + 1) * s);
                    for (int by = ya; by < yb; by++)
                        for (int bx = xa; bx < xb; bx++) {
                            total++;
                            if (bx < 0 || by < 0 || bx >= bw || by >= bh) continue;
                            int c = b[by * bw + bx];
                            if (EhFundo(c, fundo, 90)) continue;
                            sr += R(c); sg += G(c); sb += B(c); n++;
                        }
                    bool opaco = mascara == "procedural" ? A(p[y * w + x]) == 255 && n > 0 : n > 0 && n * 2 >= total;
                    o[y * w + x] = opaco ? Cor(sr / n, sg / n, sb / n) : 0;
                }
            if (mascara != "procedural") RemoverIlhas(o, w, h);
            saidasPx.Add(o); refs.Add(p); ws.Add(w); hs.Add(h);
        }
        // Graduação pela média do conjunto, para os quadros ficarem iguais entre si
        var todas = new List<int>(); var todasRef = new List<int>();
        for (int q = 0; q < saidasPx.Count; q++) { todas.AddRange(saidasPx[q]); todasRef.AddRange(refs[q]); }
        double lo = Media(todas.ToArray()), lr = Media(todasRef.ToArray());
        foreach (var o in saidasPx) Graduar(o, lo, lr, saturacao, ajusteBrilho);
        Quantizar(saidasPx, cores);
        for (int q = 0; q < saidasPx.Count; q++) {
            var o = saidasPx[q]; var p = refs[q];
            if (contorno != 0) ContornoInterno(o, ws[q], hs[q], contorno);
            // Sombras no chão do procedural (semitransparentes) voltam onde a arte deixou vazio
            for (int i = 0; i < o.Length; i++)
                if (A(o[i]) == 0 && A(p[i]) > 0 && A(p[i]) < 255) o[i] = p[i];
            Gravar(saidas[q], o, ws[q], hs[q]);
        }
    }

    // Tira pedaços soltos (sujeira do recorte): fica só o que é grande perto da maior peça
    static void RemoverIlhas(int[] o, int w, int h) {
        var rotulo = new int[w * h];
        var tamanhos = new List<int> { 0 };
        for (int i = 0; i < o.Length; i++) {
            if (A(o[i]) == 0 || rotulo[i] != 0) continue;
            int id = tamanhos.Count, n = 0;
            var pilha = new Stack<int>();
            pilha.Push(i); rotulo[i] = id;
            while (pilha.Count > 0) {
                int k = pilha.Pop(); n++;
                int x = k % w, y = k / w;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        int j = ny * w + nx;
                        if (A(o[j]) != 0 && rotulo[j] == 0) { rotulo[j] = id; pilha.Push(j); }
                    }
            }
            tamanhos.Add(n);
        }
        int maior = 0;
        foreach (int n in tamanhos) maior = Math.Max(maior, n);
        int minimo = Math.Max(4, maior / 25);
        for (int i = 0; i < o.Length; i++) if (rotulo[i] != 0 && tamanhos[rotulo[i]] < minimo) o[i] = 0;
    }

    // Leva o brilho médio para perto do procedural (mantém o clima escuro do jogo) e tira saturação
    // lo/lr: brilho médio da arte e da referência; ajusteBrilho: 0 = brilho da IA, 1 = igual ao procedural
    static void Graduar(int[] o, double lo, double lr, double saturacao, double ajusteBrilho) {
        double fator = (lo > 1 && lr > 1) ? Math.Pow(lr / lo, ajusteBrilho) : 1;
        for (int i = 0; i < o.Length; i++) {
            if (A(o[i]) == 0) continue;
            double r = R(o[i]) * fator, g = G(o[i]) * fator, b = B(o[i]) * fator, l = Lum(r, g, b);
            o[i] = Cor(l + (r - l) * saturacao, l + (g - l) * saturacao, l + (b - l) * saturacao);
        }
    }

    static double Media(int[] px) {
        double s = 0; int n = 0;
        foreach (int c in px) if (A(c) == 255) { s += Lum(R(c), G(c), B(c)); n++; }
        return n > 0 ? s / n : 0;
    }

    // Paleta limitada por k-means (determinístico), compartilhada entre as imagens da lista
    static void Quantizar(List<int[]> imagens, int k) {
        var amostras = new List<int>();
        foreach (var img in imagens) foreach (int c in img) if (A(c) == 255) amostras.Add(c);
        if (amostras.Count == 0 || k <= 0) return;
        var ordenadas = new List<int>(amostras);
        ordenadas.Sort((x, y) => Lum(R(x), G(x), B(x)).CompareTo(Lum(R(y), G(y), B(y))));
        k = Math.Min(k, ordenadas.Count);
        var cr = new double[k]; var cg = new double[k]; var cb = new double[k];
        for (int i = 0; i < k; i++) {
            int c = ordenadas[(int)((i + 0.5) * ordenadas.Count / k)];
            cr[i] = R(c); cg[i] = G(c); cb[i] = B(c);
        }
        for (int it = 0; it < 16; it++) {
            var sr = new double[k]; var sg = new double[k]; var sb = new double[k]; var n = new int[k];
            foreach (int c in amostras) {
                int m = Mais(c, cr, cg, cb);
                sr[m] += R(c); sg[m] += G(c); sb[m] += B(c); n[m]++;
            }
            for (int i = 0; i < k; i++) if (n[i] > 0) { cr[i] = sr[i] / n[i]; cg[i] = sg[i] / n[i]; cb[i] = sb[i] / n[i]; }
        }
        foreach (var img in imagens)
            for (int i = 0; i < img.Length; i++)
                if (A(img[i]) == 255) { int m = Mais(img[i], cr, cg, cb); img[i] = Cor(cr[m], cg[m], cb[m]); }
    }

    static int Mais(int c, double[] cr, double[] cg, double[] cb) {
        int melhor = 0; double dMelhor = double.MaxValue;
        for (int i = 0; i < cr.Length; i++) {
            double dr = R(c) - cr[i], dg = G(c) - cg[i], db = B(c) - cb[i];
            double d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
            if (d < dMelhor) { dMelhor = d; melhor = i; }
        }
        return melhor;
    }

    // Borda escura de 1 px por dentro da silhueta (dá leitura, como nos procedurais)
    static void ContornoInterno(int[] o, int w, int h, int cor) {
        var copia = (int[])o.Clone();
        Func<int, int, bool> vazio = (x, y) => x < 0 || y < 0 || x >= w || y >= h || A(copia[y * w + x]) == 0;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                if (!vazio(x, y) && (vazio(x - 1, y) || vazio(x + 1, y) || vazio(x, y - 1) || vazio(x, y + 1)))
                    o[y * w + x] = cor | unchecked((int)0xFF000000);
    }

    // ---------- Tiles de terreno ----------
    // Reduz a textura do NidraIA por área, recorta 4 variantes e torna cada uma periódica (emenda sem costura).
    // agua: uma variante só, deslocada a cada quadro (a animação também emenda).
    public static void ConverterTiles(string bruta, string[] procs, string[] saidas, int reducao, int cores,
                                      double saturacao, double ajusteBrilho, bool agua) {
        int bw, bh;
        var b = Ler(bruta, out bw, out bh);
        int tw = bw / reducao, th = bh / reducao;
        var t = new int[tw * th];
        for (int y = 0; y < th; y++)
            for (int x = 0; x < tw; x++) {
                double sr = 0, sg = 0, sb = 0; int n = reducao * reducao;
                for (int yy = 0; yy < reducao; yy++)
                    for (int xx = 0; xx < reducao; xx++) {
                        int c = b[(y * reducao + yy) * bw + x * reducao + xx];
                        sr += R(c); sg += G(c); sb += B(c);
                    }
                t[y * tw + x] = Cor(sr / n, sg / n, sb / n);
            }
        var tiles = new List<int[]>();
        var referencia = new List<int>();
        for (int v = 0; v < saidas.Length; v++) {
            int pw, ph;
            referencia.AddRange(Ler(procs[v], out pw, out ph));
            int x0, y0;
            if (agua) { x0 = (tw - 24) / 2; y0 = (th - 24) / 2; }
            else { x0 = 4 + (v % 2) * (tw - 32); y0 = 4 + (v / 2) * (th - 32); }
            var tile = Periodico(t, tw, x0, y0);
            if (agua) tile = Deslocar(tile, v * 4, v * 2 % 16);
            tiles.Add(tile);
        }
        // Manchas grandes denunciam a grade: cada tile fica com a média do conjunto e as variações
        // largas (média local num raio de 4 px, dando a volta no tile) caem para 30%
        Suavizar(tiles);
        var todos = new List<int>();
        foreach (var tile in tiles) todos.AddRange(tile);
        // mesma graduação para as 4 variantes (média do conjunto)
        double lo = Media(todos.ToArray()), lr = Media(referencia.ToArray());
        foreach (var tile in tiles) Graduar(tile, lo, lr, saturacao, ajusteBrilho);
        Quantizar(tiles, cores);
        for (int v = 0; v < saidas.Length; v++) Gravar(saidas[v], tiles[v], 16, 16);
    }


    static void Suavizar(List<int[]> tiles) {
        double gr = 0, gg = 0, gb = 0; int n = 0;
        foreach (var t in tiles) foreach (int c in t) { gr += R(c); gg += G(c); gb += B(c); n++; }
        gr /= n; gg /= n; gb /= n;
        for (int k = 0; k < tiles.Count; k++) {
            var t = tiles[k]; var o = new int[256];
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) {
                    double lr = 0, lg = 0, lb = 0; int m = 0;
                    for (int dy = -4; dy <= 4; dy++)
                        for (int dx = -4; dx <= 4; dx++) {
                            int c = t[((y + dy + 16) % 16) * 16 + (x + dx + 16) % 16];
                            lr += R(c); lg += G(c); lb += B(c); m++;
                        }
                    lr /= m; lg /= m; lb /= m;
                    int p = t[y * 16 + x];
                    o[y * 16 + x] = Cor(gr + (lr - gr) * 0.3 + (R(p) - lr), gg + (lg - gg) * 0.3 + (G(p) - lg), gb + (lb - gb) * 0.3 + (B(p) - lb));
                }
            tiles[k] = o;
        }
    }

    // Recorte 24×24 vira um tile 16×16 periódico: as 8 colunas/linhas que sobram se fundem com o começo
    static int[] Periodico(int[] t, int tw, int x0, int y0) {
        var linhas = new double[24 * 16 * 3];
        for (int y = 0; y < 24; y++)
            for (int x = 0; x < 16; x++) {
                int c = t[(y0 + y) * tw + x0 + x];
                double r = R(c), g = G(c), b = B(c);
                if (x < 8) {
                    int c2 = t[(y0 + y) * tw + x0 + x + 16];
                    double peso = (x + 0.5) / 8.0;
                    r = r * peso + R(c2) * (1 - peso); g = g * peso + G(c2) * (1 - peso); b = b * peso + B(c2) * (1 - peso);
                }
                int i = (y * 16 + x) * 3;
                linhas[i] = r; linhas[i + 1] = g; linhas[i + 2] = b;
            }
        var o = new int[256];
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int i = (y * 16 + x) * 3;
                double r = linhas[i], g = linhas[i + 1], b = linhas[i + 2];
                if (y < 8) {
                    int j = ((y + 16) * 16 + x) * 3;
                    double peso = (y + 0.5) / 8.0;
                    r = r * peso + linhas[j] * (1 - peso); g = g * peso + linhas[j + 1] * (1 - peso); b = b * peso + linhas[j + 2] * (1 - peso);
                }
                o[y * 16 + x] = Cor(r, g, b);
            }
        return o;
    }

    static int[] Deslocar(int[] tile, int dx, int dy) {
        var o = new int[256];
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) o[((y + dy) % 16) * 16 + (x + dx) % 16] = tile[y * 16 + x];
        return o;
    }

    // Prancha de conferência: sprites lado a lado ampliados (antes à esquerda, depois à direita)
    public static void Prancha(string[] antes, string[] depois, int zoom, string saida) {
        int largura = 0, altura = 8;
        var imgs = new List<int[]>(); var ws = new List<int>(); var hs = new List<int>();
        for (int i = 0; i < antes.Length; i++) {
            foreach (var arq in new[] { antes[i], depois[i] }) {
                int w = 16, h = 16;
                imgs.Add(File.Exists(arq) ? Ler(arq, out w, out h) : null);
                ws.Add(w); hs.Add(h);
            }
            largura = Math.Max(largura, (ws[ws.Count - 2] + ws[ws.Count - 1]) * zoom + 24);
            altura += Math.Max(hs[hs.Count - 2], hs[hs.Count - 1]) * zoom + 8;
        }
        var tela = new int[largura * altura];
        for (int i = 0; i < tela.Length; i++) tela[i] = Cor(40, 44, 40, 255);
        int yAtual = 8;
        for (int i = 0; i < antes.Length; i++) {
            int xAtual = 8;
            for (int k = 0; k < 2; k++) {
                int idx = i * 2 + k; var img = imgs[idx];
                if (img != null)
                    for (int y = 0; y < hs[idx] * zoom; y++)
                        for (int x = 0; x < ws[idx] * zoom; x++) {
                            int c = img[(y / zoom) * ws[idx] + x / zoom];
                            if (A(c) > 0) tela[(yAtual + y) * largura + xAtual + x] = c | unchecked((int)0xFF000000);
                        }
                xAtual += ws[idx] * zoom + 8;
            }
            yAtual += Math.Max(hs[i * 2], hs[i * 2 + 1]) * zoom + 8;
        }
        Gravar(saida, tela, largura, altura);
    }
}
