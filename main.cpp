#include "SPRITE_FIREBOLL_IMG.hpp"
#include "cenario.hpp" // <-- Coloque o nome exato do arquivo que você gerou
#include "cenario2.hpp"
#include "entidades.hpp"
#include "motor_grafico.hpp"
#include "sprite_dragao_img.hpp"
#include "sprite_guerreiro_img.hpp"
#include "sprite_mago_img.hpp"
#include "sprite_orc_img.hpp"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using ListaTextos = std::vector<std::string>;

struct Particula {
  int x, y;
  int vx; // <-- Adicione esta linha para controlar movimento horizontal
  int vy;
  int vida;
  uint32_t cor;
};

enum EstadoJogo { TELA_INICIAL, TELA_RANKING, LEVEL_UP, BATALHA, FIM_DE_JOGO };

std::vector<std::string> carregarRanking(int maxLinhas = 10) {
  std::vector<std::string> linhas;
  std::ifstream arq("ranking.txt");
  if (!arq.is_open())
    return linhas;

  std::string linha;
  while (std::getline(arq, linha)) {
    if (!linha.empty())
      linhas.push_back(linha);
  }
  if ((int)linhas.size() > maxLinhas) {
    linhas.erase(linhas.begin(), linhas.end() - maxLinhas);
  }
  std::reverse(linhas.begin(), linhas.end()); // Mais recentes primeiro
  return linhas;
}

int executarModoFB() {
  srand(time(0));

  try {
    MotorGrafico gfx;
    int W = gfx.getLargura();
    int H = gfx.getAltura();

    EstadoJogo estado = TELA_INICIAL;

    // Variaveis da Tela Inicial
    std::string nomeHeroi = "ARTHAS";
    int classeIndex = 0; // 0 = Guerreiro, 1 = Mago
    int modoJogoIndex =
        0; // 0 = Campanha (Orc -> Dragao), 1 = Rapida: Orc, 2 = Rapida: Dragao
    int campoConfig =
        0; // 0 = Nome, 1 = Classe, 2 = Modo, 3 = Iniciar, 4 = Ranking

    // Entidades e Estado da Batalha
    std::unique_ptr<Jogador> heroi = nullptr;
    std::unique_ptr<Inimigo> monstro = nullptr;
    bool ehMago = false;
    bool isBossAtual = false;
    int nivelHeroi = 1;
    int rodada = 1;
    int opcaoBatalha = 0;
    int inimigosDerrotados = 0;

    // Logs e Efeitos Visuais
    std::string logLinha1 = "";
    std::string logLinha2 = "";
    int animHeroiOffsetX = 0;
    int animMonstroOffsetX = 0;
    int flashHeroiFrames = 0;
    int flashMonstroFrames = 0;
    int screenShakeFrames = 0;

    // Projetil Bola de Fogo
    bool fogoAtivo = false;
    int fogoX = 0, fogoY = 0, fogoAlvoX = 0;

    // Particulas de Cura/Mana
    std::vector<Particula> particulas;
    std::vector<std::string> historicoRanking;
    bool rankingSalvo = false;

    auto spawnParticulasCura = [&](int baseX, int baseY, uint32_t cor) {
      for (int i = 0; i < 18; ++i) {
        Particula p;
        p.x = baseX + (rand() % 110);
        p.y = baseY + (rand() % 110);
        p.vx = 0; // <-- Particulas de cura nao andam para os lados
        p.vy = -2 - (rand() % 3);
        p.vida = 25 + (rand() % 15);
        p.cor = cor;
        particulas.push_back(p);
      }
    };

    int frameContador = 0; // Conta os frames para animar a onda do cenario
    bool rodando = true;
    while (rodando) {
      frameContador++; // Avanca a animacao a cada quadro (60x por segundo)
      char tecla = gfx.lerTecla();
      if (tecla == 27)
        rodando = false; // ESC sai a qualquer momento

      // =====================================================================
      // 1. TELA INICIAL
      // =====================================================================
      if (estado == TELA_INICIAL) {
        if (tecla == 'U') {
          campoConfig = (campoConfig - 1 + 5) % 5;
        } else if (tecla == 'D') {
          campoConfig = (campoConfig + 1) % 5;
        } else if (campoConfig == 0) {
          if ((tecla == 127 || tecla == 8) && !nomeHeroi.empty()) {
            nomeHeroi.pop_back();
          } else if (tecla >= 32 && tecla <= 122 && nomeHeroi.size() < 14) {
            nomeHeroi.push_back(tecla);
          }
        } else if (campoConfig == 1 &&
                   (tecla == 'L' || tecla == 'R' || tecla == '\n')) {
          classeIndex = 1 - classeIndex;
        } else if (campoConfig == 2 &&
                   (tecla == 'L' || tecla == 'R' || tecla == '\n')) {
          modoJogoIndex = (modoJogoIndex + 1) % 3;
        } else if (campoConfig == 3 && (tecla == '\n' || tecla == '\r')) {
          if (nomeHeroi.empty())
            nomeHeroi = "HEROI";
          nivelHeroi = 1;
          inimigosDerrotados = 0;

          if (classeIndex == 1) {
            heroi = std::make_unique<Mago>(nomeHeroi, 90, 10);
            ehMago = true;
          } else {
            heroi = std::make_unique<Jogador>(nomeHeroi, 120, 22);
            ehMago = false;
          }

          if (modoJogoIndex == 2) {
            monstro = std::make_unique<Chefao>("DRAGAO NEGRO", 185, 16);
            isBossAtual = true;
          } else if (modoJogoIndex == 1) {
            // Modo Batalha Rapida: 1 unico Orc padrao, sem "(1/3)" no nome
            monstro = std::make_unique<Inimigo>("ORC GUERREIRO", 90, 13);
            isBossAtual = false;
          } else {
            // Modo Campanha (0): comeca a escalada de 3 Orcs
            monstro = std::make_unique<Inimigo>("ORC BATEDOR (1/3)", 65, 10);
            isBossAtual = false;
          }

          rodada = 1;
          opcaoBatalha = 0;
          rankingSalvo = false;
          particulas.clear();
          fogoAtivo = false;
          logLinha1 = "RODADA 1: " + monstro->getTipo() + " SURGIU!";
          logLinha2 = "ESCOLHA SUA ACAO NO MENU.";
          estado = BATALHA;
        } else if (campoConfig == 4 && (tecla == '\n' || tecla == '\r')) {
          historicoRanking = carregarRanking(10);
          estado = TELA_RANKING;
        }

        // Renderizacao da Tela Inicial
        gfx.limparTela(0x000F172A);
        int boxW = 700;
        int boxH = 490;
        int boxX = (W - boxW) / 2;
        int boxY = (H - boxH) / 2;

        gfx.desenharCaixaBorda(boxX, boxY, boxW, boxH, 0x001E293B, 0x00FFD700,
                               4);
        gfx.desenharTexto(boxX + 55, boxY + 28,
                          "MINI-RPG BARE-METAL FRAMEBUFFER", 0x00FFD700, 2);
        gfx.desenharTexto(boxX + 55, boxY + 55,
                          "SETAS: NAVEGAR | ENTER: SELECIONAR | ESC: SAIR",
                          0x0094A3B8, 1);

        // Campo 0: Nome
        uint32_t cor0 = (campoConfig == 0) ? 0x00FFD700 : 0x00FFFFFF;
        gfx.desenharTexto(boxX + 45, boxY + 95,
                          (campoConfig == 0 ? "> NOME: " : "  NOME: ") +
                              nomeHeroi + (campoConfig == 0 ? "_" : ""),
                          cor0, 2);

        // Campo 1: Classe
        uint32_t cor1 = (campoConfig == 1) ? 0x00FFD700 : 0x00FFFFFF;
        std::string txtClasse = (classeIndex == 0)
                                    ? "< GUERREIRO (HP 120 / ATK 22) >"
                                    : "< MAGO (HP 90 / MP 50 / ATK 10) >";
        gfx.desenharTexto(boxX + 45, boxY + 145,
                          (campoConfig == 1 ? "> CLASSE: " : "  CLASSE: "),
                          cor1, 2);
        gfx.desenharTexto(boxX + 65, boxY + 172, txtClasse, 0x004CC9F0, 2);

        // Preview do Sprite PNG da Classe selecionada
        if (classeIndex == 0) {
          gfx.desenharImagem(
              boxX + boxW - 140, boxY + 115, SPRITE_GUERREIRO_IMG_DATA,
              SPRITE_GUERREIRO_IMG_W, SPRITE_GUERREIRO_IMG_H, 2,
              false); // Escala 2 (ajuste para 1 ou 3 conforme o tamanho do PNG)
        } else {
          gfx.desenharImagem(
              boxX + boxW - 140, boxY + 115, SPRITE_MAGO_IMG_DATA,
              SPRITE_MAGO_IMG_W, SPRITE_MAGO_IMG_H, 2,
              false); // Escala 2 (ajuste para 1 ou 3 conforme o tamanho do PNG)
        }

        // Campo 2: Modo de Jogo
        uint32_t cor2 = (campoConfig == 2) ? 0x00FFD700 : 0x00FFFFFF;
        std::string txtModo = "< CAMPANHA: ORC + DRAGAO >";
        if (modoJogoIndex == 1)
          txtModo = "< BATALHA RAPIDA: ORC GUERREIRO >";
        else if (modoJogoIndex == 2)
          txtModo = "< DESAFIO BOSS: DRAGAO NEGRO >";

        gfx.desenharTexto(boxX + 45, boxY + 225,
                          (campoConfig == 2 ? "> MODO: " : "  MODO: "), cor2,
                          2);
        gfx.desenharTexto(boxX + 65, boxY + 252, txtModo, 0x00FF5555, 2);

        // Campo 3: Botao Iniciar
        uint32_t cor3 = (campoConfig == 3) ? 0x0016A34A : 0x00334155;
        gfx.desenharCaixaBorda(boxX + 180, boxY + 315, 340, 52, cor3,
                               0x00FFFFFF, 2);
        gfx.desenharTexto(boxX + 225, boxY + 332, "INICIAR BATALHA", 0x00FFFFFF,
                          2);

        // Campo 4: Botao Ver Ranking
        uint32_t cor4 = (campoConfig == 4) ? 0x002563EB : 0x00334155;
        gfx.desenharCaixaBorda(boxX + 180, boxY + 385, 340, 52, cor4,
                               0x00FFFFFF, 2);
        gfx.desenharTexto(boxX + 245, boxY + 402, "VER RANKING", 0x00FFFFFF, 2);

        // =====================================================================
        // 2. TELA DE RANKING (LEITURA DO RANKING.TXT)
        // =====================================================================
      } else if (estado == TELA_RANKING) {
        if (tecla == '\n' || tecla == '\r' || tecla == 'q' || tecla == 'Q') {
          estado = TELA_INICIAL;
        }
        gfx.limparTela(0x000F172A);
        int boxW = 860;
        int boxH = 480;
        int boxX = (W - boxW) / 2;
        int boxY = (H - boxH) / 2;
        gfx.desenharCaixaBorda(boxX, boxY, boxW, boxH, 0x001E293B, 0x003A86FF,
                               4);
        gfx.desenharTexto(boxX + 240, boxY + 25, "HISTORICO DE BATALHAS",
                          0x00FFD700, 2);
        if (historicoRanking.empty()) {
          gfx.desenharTexto(boxX + 210, boxY + 200,
                            "NENHUMA BATALHA REGISTRADA AINDA.", 0x0094A3B8, 2);
        } else {
          for (size_t i = 0; i < historicoRanking.size(); ++i) {
            uint32_t corLinha =
                (historicoRanking[i].find("VITORIA") != std::string::npos)
                    ? 0x002EC4B6
                    : 0x00FF5555;
            gfx.desenharTexto(boxX + 35, boxY + 75 + (i * 32),
                              historicoRanking[i], corLinha, 1);
          }
        }
        gfx.desenharTexto(boxX + 250, boxY + boxH - 45,
                          "(ENTER) VOLTAR AO MENU", 0x00FFD700, 2);
        // =====================================================================
        // 3. TELA DE LEVEL UP (MODO CAMPANHA)
        // =====================================================================
      } else if (estado == LEVEL_UP) {
        if (tecla == '\n' || tecla == '\r') {
          // Evolui o heroi recriando com atributos maiores mantendo o nome e
          // classe
          nivelHeroi = 2;
          if (ehMago) {
            heroi = std::make_unique<Mago>(nomeHeroi, 115, 15);
          } else {
            heroi = std::make_unique<Jogador>(nomeHeroi, 150, 28);
          }
          monstro = std::make_unique<Chefao>("DRAGAO NEGRO", 185, 16);
          isBossAtual = true;
          rodada = 1;
          opcaoBatalha = 0;
          particulas.clear();
          fogoAtivo = false;
          logLinha1 = "CHEFAO FINAL: DRAGAO NEGRO DESCEU DOS CEUS!";
          logLinha2 = "PREPARE-SE PARA O COMBATE DECISIVO!";
          estado = BATALHA;
        }
        gfx.limparTela(0x000F172A);
        int boxW = 680;
        int boxH = 360;
        int boxX = (W - boxW) / 2;
        int boxY = (H - boxH) / 2;
        gfx.desenharCaixaBorda(boxX, boxY, boxW, boxH, 0x001E293B, 0x00FFD700,
                               4);
        gfx.desenharTexto(boxX + 180, boxY + 35, "LEVEL UP! NIVEL 2",
                          0x00FFD700, 3);
        gfx.desenharTexto(boxX + 55, boxY + 100,
                          "ORC GUERREIRO DERROTADO COM SUCESSO!", 0x002EC4B6,
                          2);
        gfx.desenharTexto(boxX + 55, boxY + 145,
                          "+ VIDA MAXIMA AUMENTADA E RESTAURADA", 0x00FFFFFF,
                          2);
        gfx.desenharTexto(boxX + 55, boxY + 180,
                          "+ FORCA DE ATAQUE AUMENTADA (+5 ATK)", 0x00FFFFFF,
                          2);
        gfx.desenharTexto(boxX + 55, boxY + 215,
                          "+ MOCHILA REABASTECIDA PARA O BOSS!", 0x004CC9F0, 2);
        gfx.desenharTexto(boxX + 110, boxY + 290,
                          "(ENTER) ENFRENTAR O DRAGAO NEGRO", 0x00FFD700, 2);
        // =====================================================================
        // 4. BATALHA POR TURNOS COM ANIMACOES E PARTICULAS
        // =====================================================================
      } else if (estado == BATALHA) {
        if (tecla == 'q' || tecla == 'Q')
          rodando = false;
        ListaTextos opcoesMenu;
        if (ehMago) {
          Mago *m = static_cast<Mago *>(heroi.get());
          opcoesMenu = {"1. ATACAR CAJADO",
                        "2. BOLA DE FOGO (20 MP)",
                        "3. CURAR (15 MP)",
                        "4. POCAO VIDA (" +
                            std::to_string(heroi->qtdeItem("Pocao de Vida")) +
                            ")",
                        "5. ELIXIR MANA (" +
                            std::to_string(m->qtdeItem("Elixir de Mana")) + ")",
                        "6. DEFENDER"};
        } else {
          opcoesMenu = {"1. ATACAR ESPADA",
                        "2. POCAO VIDA (" +
                            std::to_string(heroi->qtdeItem("Pocao de Vida")) +
                            ")",
                        "3. DEFENDER"};
        }
        if (opcaoBatalha >= (int)opcoesMenu.size())
          opcaoBatalha = 0;
        if (tecla == 'U' || tecla == 'w' || tecla == 'W') {
          opcaoBatalha =
              (opcaoBatalha - 1 + opcoesMenu.size()) % opcoesMenu.size();
        } else if (tecla == 'D' || tecla == 's' || tecla == 'S') {
          opcaoBatalha = (opcaoBatalha + 1) % opcoesMenu.size();
        } else if (tecla == '\n' || tecla == '\r') {
          try {
            int acaoMapeada = opcaoBatalha + 1;
            if (!ehMago) {
              if (acaoMapeada == 2)
                acaoMapeada = 4;
              else if (acaoMapeada == 3)
                acaoMapeada = 6;
            }
            int danoHeroi = 0;
            bool defendeu = false;
            if (acaoMapeada == 1) {
              danoHeroi = heroi->atacar();
              monstro->setVida(monstro->getVida() - danoHeroi);
              logLinha1 = heroi->getNome() + " ATACOU: -" +
                          std::to_string(danoHeroi) + " HP!";
              animHeroiOffsetX = 50;
              flashMonstroFrames = 8;
            } else if (acaoMapeada == 2 && ehMago) {
              std::string msgMagia;
              int manaAntes = static_cast<Mago *>(heroi.get())->getMana();
              danoHeroi =
                  static_cast<Mago *>(heroi.get())->lancarBolaDeFogo(msgMagia);
              monstro->setVida(monstro->getVida() - danoHeroi);
              logLinha1 = msgMagia + " (-" + std::to_string(danoHeroi) + " HP)";
              animHeroiOffsetX = 40;
              flashMonstroFrames = 12;
              // Dispara projetil visual se tinha mana para Bola de Fogo
              if (manaAntes >= 20) {
                fogoAtivo = true;
                fogoX = 280;
                fogoY = (H / 2) + 40;
                fogoAlvoX = W - 420;
              }
            } else if (acaoMapeada == 3 && ehMago) {
              logLinha1 = static_cast<Mago *>(heroi.get())->curar();
              spawnParticulasCura(180, (H / 2) + 20, 0x002EC4B6);
            } else if (acaoMapeada == 4) {
              logLinha1 = heroi->usarPocao();
              spawnParticulasCura(180, (H / 2) + 20, 0x0016A34A);
            } else if (acaoMapeada == 5 && ehMago) {
              logLinha1 = static_cast<Mago *>(heroi.get())->usarElixir();
              spawnParticulasCura(180, (H / 2) + 20, 0x003A86FF);
            } else {
              logLinha1 = heroi->getNome() + " ASSUMIU POSTURA DEFENSIVA!";
              defendeu = true;
            }
            // Verifica derrota do inimigo
            if (monstro->getVida() <= 0) {
              logLinha2 = monstro->getTipo() + " FOI DERROTADO!";
              if (modoJogoIndex == 0 && !isBossAtual) {
                inimigosDerrotados++;

                if (inimigosDerrotados == 1) {
                  // Surge o 2o Orc (mais forte, sem curar o heroi!)
                  monstro =
                      std::make_unique<Inimigo>("ORC GUERREIRO (2/3)", 95, 14);
                  logLinha1 = "OUTRO ORC AVANCA DAS SOMBRAS!";
                  logLinha2 = "SUA VIDA E MANA CONTINUAM AS MESMAS!";
                } else if (inimigosDerrotados == 2) {
                  // Surge o 3o Orc (Mini-chefe)
                  monstro = std::make_unique<Inimigo>("ORC COMANDANTE (3/3)",
                                                      125, 18);
                  logLinha1 = "O COMANDANTE ORC ENTROU NA ARENA!";
                  logLinha2 = "SOBREVIVA PARA ENFRENTAR O DRAGAO!";
                } else {
                  // Derrotou os 3 Orcs -> Vai para o Level Up e depois Dragao!
                  estado = LEVEL_UP;
                }
              } else {
                estado = FIM_DE_JOGO;
              }
            } else {
              // Contra-ataque do Inimigo
              bool critico = false;
              int danoInimigo = monstro->atacar(critico);
              if (defendeu)
                danoInimigo /= 2;
              heroi->setVida(heroi->getVida() - danoInimigo);
              animMonstroOffsetX = -50;
              flashHeroiFrames = 8;
              if (critico) {
                logLinha2 = "CRITICO! " + monstro->getTipo() + " CAUSOU -" +
                            std::to_string(danoInimigo) + " HP!";
                screenShakeFrames = 14; // Tremor de tela!
              } else {
                logLinha2 = monstro->getTipo() + " ATACOU: -" +
                            std::to_string(danoInimigo) + " HP!";
              }
              if (heroi->getVida() <= 0) {
                estado = FIM_DE_JOGO;
              } else {
                rodada++;
              }
            }
          } catch (const std::exception &e) {
            logLinha1 = std::string("ERRO: ") + e.what();
            logLinha2 = "ESCOLHA OUTRA ACAO.";
          }
        }
        if (animHeroiOffsetX > 0)
          animHeroiOffsetX -= 5;
        if (animMonstroOffsetX < 0)
          animMonstroOffsetX += 5;
        if (flashHeroiFrames > 0)
          flashHeroiFrames--;
        if (flashMonstroFrames > 0)
          flashMonstroFrames--;
        int shakeX = 0, shakeY = 0;
        if (screenShakeFrames > 0) {
          shakeX = (rand() % 17) - 8;
          shakeY = (rand() % 17) - 8;
          screenShakeFrames--;
        }
        if (fogoAtivo) {
          fogoX += 38;
          fogoY -= 8;
          if (fogoX >= fogoAlvoX)
            fogoAtivo = false;
        }
        // Gera particulas leves de ambiente a cada 8 frames (sem mover o chao)
        if (frameContador % 8 == 0) {
          Particula envP;
          if (!isBossAtual) {
            // Na Grama: folha ou polen flutuando suave da esquerda para a
            // direita
            envP.x = 0;
            envP.y = rand() % (H - 260);
            envP.vx = 3 + (rand() % 3); // Velocidade horizontal suave
            envP.vy = (rand() % 3) - 1; // Oscila levemente para cima ou baixo
            envP.vida = 500;
            envP.cor = (rand() % 2 == 0) ? 0x0086EFAC : 0x00FDE047;
          } else {
            // Na Lava: fagulha ou brasa subindo devagar do chao
            envP.x = rand() % W;
            envP.y = H - 250;
            envP.vx = (rand() % 3) - 1;  // Oscila levemente para os lados
            envP.vy = -1 - (rand() % 2); // Sobe devagar
            envP.vida = 250;
            envP.cor = (rand() % 2 == 0) ? 0x00FB923C : 0x00EF4444;
          }
          particulas.push_back(envP);
        }

        // Atualiza todas as particulas (cura + ambiente)
        for (size_t i = 0; i < particulas.size();) {
          particulas[i].x += particulas[i].vx;
          particulas[i].y += particulas[i].vy;
          particulas[i].vida--;
          if (particulas[i].vida <= 0 || particulas[i].x > W ||
              particulas[i].y < 0)
            particulas.erase(particulas.begin() + i);
          else
            ++i;
        }
        // Renderizacao da Arena (com offset de Screen Shake)
        gfx.limparTela(0x00141824);

        // 1. Escolhe qual cenario usar (Lava para o Dragao, Grama para o Orc)
        const uint32_t *dadosCenario =
            isBossAtual ? CENARIO2_IMG_DATA : CENARIO_DATA;
        int cenarioW = isBossAtual ? CENARIO2_IMG_W : CENARIO_W;
        int cenarioH = isBossAtual ? CENARIO2_IMG_H : CENARIO_H;

        // 2. Desenha os ladrilhos
        int escalaTile = 2;
        int tileW = cenarioW * escalaTile;
        int tileH = cenarioH * escalaTile;

        for (int y = 0; y < H - 240; y += tileH) {
          for (int x = 0; x < W; x += tileW) {
            gfx.desenharImagem(x + shakeX, y + shakeY, dadosCenario, cenarioW,
                               cenarioH, escalaTile);
          }
        }
        // Caixa escura com borda dourada para destacar o texto no topo da lava
        // e da grama
        gfx.desenharCaixaBorda(20, 12, 530, 32, 0x000F172A, 0x00FFD700, 2);
        gfx.desenharTexto(30, 20,
                          "RODADA " + std::to_string(rodada) + "  |  NV." +
                              std::to_string(nivelHeroi) + "  |  ESC = SAIR",
                          0x00FFD700, 2);

        gfx.desenharCaixaBorda(30 + shakeX, 55 + shakeY, 420, 100, 0x001E2436,
                               0x00AAAAAA);
        gfx.desenharTexto(45 + shakeX, 70 + shakeY, monstro->getTipo(),
                          0x00FF5555, 2);
        gfx.desenharTexto(45 + shakeX, 97 + shakeY,
                          "HP: " + std::to_string(monstro->getVida()) + "/" +
                              std::to_string(monstro->getVidaMax()),
                          0x00FFFFFF, 2);
        gfx.desenharBarraStatus(45 + shakeX, 123 + shakeY, 380, 18,
                                monstro->getVida(), monstro->getVidaMax(),
                                0x00E63946, 0x00331111);
        int escalaDragao = 3;
        int larguraDragaoNaTela = SPRITE_DRAGAO_IMG_W * escalaDragao;

        // Posiciona o Dragao a 120 pixels de distancia da borda direita da tela
        int dragaoX = W - larguraDragaoNaTela - 120;
        int dragaoY = 140; // Aumente para descer o dragao, diminua para subir

        if (flashMonstroFrames % 2 == 0) {
          if (isBossAtual) {
            gfx.desenharImagem(dragaoX + animMonstroOffsetX + shakeX,
                               dragaoY + shakeY, SPRITE_DRAGAO_IMG_DATA,
                               SPRITE_DRAGAO_IMG_W, SPRITE_DRAGAO_IMG_H,
                               escalaDragao, false);
          } else {
            // 1. Escala cresce conforme a onda: 1o Orc = 3, 2o = 4, 3o
            // Comandante = 5
            int escalaOrc = 3 + inimigosDerrotados;

            // Se estiver no modo "Batalha Rapida: Orc", usa o tamanho medio (4)
            if (modoJogoIndex == 1)
              escalaOrc = 4;

            // 2. Mantem os pes do Orc sempre na mesma linha do chao (ex: Y =
            // 430)
            int linhaDoChaoY = 430;
            int orcY = linhaDoChaoY - (SPRITE_ORC_IMG_H * escalaOrc);

            // 3. Centraliza na direita sem cortar a borda
            int orcX = W - (SPRITE_ORC_IMG_W * escalaOrc) - 160;

            gfx.desenharImagem(orcX + animMonstroOffsetX + shakeX,
                               orcY + shakeY, SPRITE_ORC_IMG_DATA,
                               SPRITE_ORC_IMG_W, SPRITE_ORC_IMG_H, escalaOrc,
                               true);
          }
        }

        // Exemplo para desenhar o Heroi com PNG:
        if (flashHeroiFrames % 2 == 0) {
          if (ehMago) {
            gfx.desenharImagem(180 + animHeroiOffsetX + shakeX,
                               (H / 2) + 20 + shakeY, SPRITE_MAGO_IMG_DATA,
                               SPRITE_MAGO_IMG_W, SPRITE_MAGO_IMG_H, 4, false);
          } else {
            gfx.desenharImagem(180 + animHeroiOffsetX + shakeX,
                               (H / 2) + 20 + shakeY, SPRITE_GUERREIRO_IMG_DATA,
                               SPRITE_GUERREIRO_IMG_W, SPRITE_GUERREIRO_IMG_H,
                               4, false);
          }
        }

        // Renderiza Bola de Fogo e Particulas
        if (fogoAtivo) {
          gfx.desenharImagem(fogoX, fogoY, SPRITE_FIREBOLL_IMG_DATA,
                             SPRITE_FIREBOLL_IMG_W, SPRITE_FIREBOLL_IMG_H, 3,
                             false);
        }

        for (const auto &p : particulas) {
          gfx.desenharRetangulo(p.x, p.y, 6, 6, p.cor);
        }
        // HUD do Heroi
        int hudHeroiX = W - 460 + shakeX;
        int hudHeroiY = H - 390 + shakeY;
        gfx.desenharCaixaBorda(hudHeroiX, hudHeroiY, 430, 130, 0x001E2436,
                               0x00AAAAAA);
        gfx.desenharTexto(hudHeroiX + 15, hudHeroiY + 15,
                          heroi->getNome() +
                              (ehMago ? " (MAGO)" : " (GUERREIRO)"),
                          0x004CC9F0, 2);
        gfx.desenharTexto(hudHeroiX + 15, hudHeroiY + 45,
                          "HP: " + std::to_string(heroi->getVida()) + "/" +
                              std::to_string(heroi->getVidaMax()),
                          0x00FFFFFF, 2);
        gfx.desenharBarraStatus(hudHeroiX + 195, hudHeroiY + 45, 215, 16,
                                heroi->getVida(), heroi->getVidaMax(),
                                0x002EC4B6, 0x00113322);
        if (ehMago) {
          Mago *m = static_cast<Mago *>(heroi.get());
          gfx.desenharTexto(hudHeroiX + 15, hudHeroiY + 80,
                            "MP: " + std::to_string(m->getMana()) + "/" +
                                std::to_string(m->getManaMax()),
                            0x00FFFFFF, 2);
          gfx.desenharBarraStatus(hudHeroiX + 195, hudHeroiY + 80, 215, 16,
                                  m->getMana(), m->getManaMax(), 0x003A86FF,
                                  0x00112244);
        }
        // Painel Inferior: Menu + Log
        int painelY = H - 240;
        int larguraMenu = 440;
        gfx.desenharCaixaBorda(30, painelY, larguraMenu, 210, 0x000F172A,
                               0x00FFFFFF);
        for (size_t i = 0; i < opcoesMenu.size(); ++i) {
          int itemY = painelY + 18 + (i * 30);
          uint32_t corItem = ((int)i == opcaoBatalha) ? 0x00FFD700 : 0x00CBD5E1;
          std::string prefixo = ((int)i == opcaoBatalha) ? "> " : "  ";
          gfx.desenharTexto(45, itemY, prefixo + opcoesMenu[i], corItem, 2);
        }
        int logX = 30 + larguraMenu + 20;
        int logW = W - logX - 30;
        gfx.desenharCaixaBorda(logX, painelY, logW, 210, 0x000F172A,
                               0x00FFFFFF);
        gfx.desenharTexto(logX + 20, painelY + 20,
                          "LOG DE COMBATE:", 0x0094A3B8, 2);
        gfx.desenharTexto(logX + 20, painelY + 60, logLinha1, 0x00FFFFFF, 2);
        gfx.desenharTexto(logX + 20, painelY + 95, logLinha2, 0x00FACC15, 2);
        gfx.desenharTexto(logX + 20, painelY + 165, "SETAS CIMA/BAIXO + ENTER",
                          0x0064748B, 2);
        // =====================================================================
        // 5. TELA DE FIM DE JOGO
        // =====================================================================
      } else if (estado == FIM_DE_JOGO) {
        bool venceu = (heroi->getVida() > 0);
        if (!rankingSalvo) {
          std::ofstream arquivo_ranking("ranking.txt", std::ios::app);
          if (arquivo_ranking.is_open()) {
            std::string modoTxt = "Campanha";
            if (modoJogoIndex == 1)
              modoTxt = "Rapida (Orc)";
            else if (modoJogoIndex == 2)
              modoTxt = "Desafio Boss";
            arquivo_ranking << "Jogador: " << heroi->getNome()
                            << " | Classe: " << (ehMago ? "Mago" : "Guerreiro")
                            << " | Modo: " << modoTxt << " | Resultado: "
                            << (venceu ? "VITORIA" : "GAME OVER") << std::endl;
            arquivo_ranking.close();
          }
          rankingSalvo = true;
        }
        if (tecla == '\n' || tecla == '\r') {
          estado = TELA_INICIAL;
        } else if (tecla == 'q' || tecla == 'Q') {
          rodando = false;
        }
        gfx.limparTela(0x000F172A);
        int boxW = 760;
        int boxH = 320;
        int boxX = (W - boxW) / 2;
        int boxY = (H - boxH) / 2;
        uint32_t corTema = venceu ? 0x002EC4B6 : 0x00E63946;
        gfx.desenharCaixaBorda(boxX, boxY, boxW, boxH, 0x001E293B, corTema, 4);
        gfx.desenharTexto(boxX + 220, boxY + 40,
                          venceu ? "VITORIA GLORIOSA!" : "GAME OVER...",
                          corTema, 3);
        gfx.desenharTexto(boxX + 35, boxY + 115, logLinha1, 0x00FFFFFF, 2);
        gfx.desenharTexto(boxX + 35, boxY + 150, logLinha2, 0x00FACC15, 2);
        gfx.desenharTexto(boxX + 35, boxY + 200,
                          "RESULTADO SALVO EM RANKING.TXT", 0x0094A3B8, 2);
        gfx.desenharTexto(boxX + 64, boxY + 255,
                          "(ENTER) MENU INICIAL   |   (ESC) SAIR", 0x00FFD700,
                          2);
      }
      gfx.atualizarTela();
      usleep(16666); // ~60 FPS
    }
  } catch (const std::exception &e) {
    std::cerr << "Erro fatal: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
