/* Painel de bolso da Tropa do Fogo Amigo, para o M5StickC Plus2.

   Quatro telas, que passam sozinhas a cada 8 segundos:
     0  contagem até o desbloqueio de GTA 6 no Brasil (19/11, meia-noite)
     1  quantos estão em call no Discord, e quantos online
     2  a manchete mais nova do site
     3  o relógio do bump (2 horas, marcado à mão com o botão lateral)

   Botão da frente (A): próxima tela, e pausa a troca automática por 60 s.
   Botão lateral (B): na tela do bump, marca "bumpei agora".

   Quem manda em quê:
     - Discord: a placa pergunta DIRETO, de casa, como o navegador do leitor
       faz. Não passa pelo site.
     - Site: só as manchetes e a data de lançamento, em /api/painel.

   ⚠️ A conexão com o Discord e com o site usa TLS sem conferir o certificado
   (setInsecure). Os dados são públicos e só vão da rede para a tela; o pior
   que um intruso conseguiria é mostrar um número errado. Não coloque senha
   nem token neste programa. */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "config.h"  // copie de config.example.h

static const char* URL_DISCORD =
    "https://discord.com/api/guilds/808386314894311515/widget.json";
static const char* URL_PAINEL = "https://tdfa.com.br/api/painel";

// Meia-noite de Brasília de 19/11/2026, em UTC (03:00).
static const time_t LANCAMENTO = 1795057200;
static const uint32_t BUMP_ESPERA_S = 2 * 60 * 60;  // DISBOARD: 2 horas

static const uint32_t TROCA_AUTOMATICA_MS = 8000;
static const uint32_t PAUSA_APOS_BOTAO_MS = 60000;
static const uint32_t ATUALIZA_DISCORD_MS = 60000;
static const uint32_t ATUALIZA_SITE_MS = 15UL * 60 * 1000;
static const uint32_t TELA_APAGA_MS = 5UL * 60 * 1000;
static const int NUM_TELAS = 4;

// Laranja da Tropa (#FF7900) e preto.
static uint16_t LARANJA, PRETO, BRANCO, VERDE, CINZA;

static int tela = 0;
static uint32_t ultimaTroca = 0, pausaAte = 0, ultimaAtividade = 0;
static bool telaApagada = false;

// Dados
static int emCall = -1, online = -1;
static char manchete[130] = "";
static uint32_t ultimoDiscord = 0, ultimoSite = 0;
static bool wifiOk = false;
static bool horaOk = false;

// Quadro de memória 240 x 135 (uns 65 KB): evita piscar ao atualizar a tela.
static M5Canvas canvas(&M5.Display);

static Preferences prefs;
static uint32_t bumpEm = 0;  // epoch do último bump marcado; 0 = nunca
static bool bumpAvisou = true;

/* ------------------------------ rede ------------------------------ */

static bool conectaWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  for (const Rede& r : REDES) {
    if (strncmp(r.ssid, "NOME_DO_", 8) == 0) continue;  // não preenchido
    WiFi.begin(r.ssid, r.senha);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(250);
    if (WiFi.status() == WL_CONNECTED) return true;
    WiFi.disconnect(true);
  }
  return false;
}

static void sincronizaHora() {
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 40; i++) {
    if (time(nullptr) > 1700000000) {
      horaOk = true;
      return;
    }
    delay(250);
  }
}

static bool buscaJson(const char* url, JsonDocument& doc, JsonDocument& filtro) {
  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;
  http.useHTTP10(true);  // sem resposta "chunked", mais fácil de ler em fluxo
  http.setTimeout(8000);
  if (!http.begin(cliente, url)) return false;
  int codigo = http.GET();
  bool ok = false;
  if (codigo == 200) {
    DeserializationError e = deserializeJson(doc, http.getStream(),
                                             DeserializationOption::Filter(filtro));
    ok = !e;
  }
  http.end();
  return ok;
}

static void atualizaDiscord() {
  JsonDocument filtro;
  filtro["presence_count"] = true;
  filtro["members"][0]["channel_id"] = true;
  JsonDocument doc;
  if (!buscaJson(URL_DISCORD, doc, filtro)) return;  // mantém o número antigo
  online = doc["presence_count"] | -1;
  int voz = 0;
  for (JsonObject m : doc["members"].as<JsonArray>()) {
    if (m["channel_id"].is<const char*>()) voz++;
  }
  emCall = voz;
}

static void atualizaSite() {
  JsonDocument filtro;
  filtro["manchetes"][0]["titulo"] = true;
  JsonDocument doc;
  if (!buscaJson(URL_PAINEL, doc, filtro)) return;
  const char* t = doc["manchetes"][0]["titulo"] | "";
  if (t[0]) {
    strncpy(manchete, t, sizeof(manchete) - 1);
    manchete[sizeof(manchete) - 1] = 0;
    if (strlen(t) > sizeof(manchete) - 4) strcpy(manchete + sizeof(manchete) - 4, "...");
  }
}

/* ------------------------------ telas ------------------------------ */

static void cabecalho(const char* titulo) {
  auto& d = canvas;
  d.fillRect(0, 0, d.width(), 24, LARANJA);
  d.setTextColor(PRETO, LARANJA);
  d.setTextSize(2);
  d.setCursor(6, 4);
  d.print(titulo);
}

static void rodape() {
  auto& d = canvas;
  int y = d.height() - 12;
  d.setTextSize(1);
  d.setTextColor(CINZA, PRETO);
  d.setCursor(4, y);
  d.printf("%s  bat %d%%  tela %d/%d", wifiOk ? "wifi ok" : "sem wifi",
           (int)M5.Power.getBatteryLevel(), tela + 1, NUM_TELAS);
}

static void desenhaContagem() {
  auto& d = canvas;
  cabecalho("GTA VI no Brasil");
  d.setTextColor(BRANCO, PRETO);
  if (!horaOk) {
    d.setTextSize(2);
    d.setCursor(6, 50);
    d.print("sem hora ainda");
    return;
  }
  long falta = (long)(LANCAMENTO - time(nullptr));
  if (falta <= 0) {
    d.setTextColor(VERDE, PRETO);
    d.setTextSize(3);
    d.setCursor(6, 50);
    d.print("JA CHEGOU!");
    return;
  }
  long dias = falta / 86400, horas = (falta % 86400) / 3600, mins = (falta % 3600) / 60;
  d.setTextColor(LARANJA, PRETO);
  d.setTextSize(5);
  d.setCursor(6, 34);
  d.printf("%ld", dias);
  d.setTextSize(2);
  d.setTextColor(BRANCO, PRETO);
  d.setCursor(d.getCursorX() + 6, 50);
  d.print(dias == 1 ? "dia" : "dias");
  d.setTextSize(2);
  d.setCursor(6, 84);
  d.printf("%02ldh %02ldmin", horas, mins);
  d.setTextSize(1);
  d.setTextColor(CINZA, PRETO);
  d.setCursor(6, 108);
  d.print("19/11, meia-noite de Brasilia");
}

static void desenhaCall() {
  auto& d = canvas;
  cabecalho("No Discord agora");
  d.setTextColor(BRANCO, PRETO);
  d.setTextSize(2);
  d.setCursor(6, 36);
  d.print("em call");
  d.setTextColor(emCall > 0 ? VERDE : LARANJA, PRETO);
  d.setTextSize(5);
  d.setCursor(6, 54);
  if (emCall >= 0) d.printf("%d", emCall); else d.print("--");
  d.setTextColor(BRANCO, PRETO);
  d.setTextSize(2);
  d.setCursor(120, 70);
  d.printf("%s online", online >= 0 ? String(online).c_str() : "--");
  d.setTextSize(1);
  d.setTextColor(CINZA, PRETO);
  d.setCursor(6, 104);
  d.print("discord.gg/RJNmEVgDRM");
}

static void desenhaManchete() {
  auto& d = canvas;
  cabecalho("Ultima noticia");
  d.setTextColor(BRANCO, PRETO);
  d.setTextSize(2);
  d.setTextWrap(true);
  d.setCursor(4, 30);
  d.print(manchete[0] ? manchete : "buscando...");
  d.setTextWrap(false);
  d.setTextSize(1);
  d.setTextColor(CINZA, PRETO);
  d.setCursor(4, d.height() - 24);
  d.print("tdfa.com.br");
}

static void desenhaBump() {
  auto& d = canvas;
  cabecalho("Relogio do bump");
  d.setTextSize(2);
  if (!horaOk || bumpEm == 0) {
    d.setTextColor(BRANCO, PRETO);
    d.setCursor(6, 40);
    d.print("aperte o botao");
    d.setCursor(6, 60);
    d.print("lateral ao dar");
    d.setCursor(6, 80);
    d.print("o /bump");
    return;
  }
  long falta = (long)(bumpEm + BUMP_ESPERA_S) - (long)time(nullptr);
  if (falta <= 0) {
    d.setTextColor(VERDE, PRETO);
    d.setTextSize(3);
    d.setCursor(6, 44);
    d.print("BUMP!");
    d.setTextSize(2);
    d.setCursor(6, 80);
    d.print("ja pode dar");
    return;
  }
  d.setTextColor(LARANJA, PRETO);
  d.setTextSize(4);
  d.setCursor(6, 44);
  d.printf("%02ld:%02ld", falta / 3600, (falta % 3600) / 60);
  d.setTextSize(1);
  d.setTextColor(CINZA, PRETO);
  d.setCursor(6, 90);
  d.print("tempo para o proximo bump");
}

static void desenha() {
  // Desenha tudo num quadro na memória e só então manda para a tela, de uma
  // vez. Apagar a tela e redesenhar na frente é o que fazia ela piscar.
  canvas.fillScreen(PRETO);
  switch (tela) {
    case 0: desenhaContagem(); break;
    case 1: desenhaCall(); break;
    case 2: desenhaManchete(); break;
    default: desenhaBump(); break;
  }
  rodape();
  canvas.pushSprite(0, 0);
}

/* ------------------------------ som e brilho ------------------------------ */

static void apita() {
  for (int i = 0; i < 3; i++) {
    M5.Speaker.tone(2200, 150);
    delay(220);
  }
}

static void acordaTela() {
  ultimaAtividade = millis();
  if (telaApagada) {
    M5.Display.setBrightness(80);
    telaApagada = false;
  }
}

/* ------------------------------ ciclo ------------------------------ */

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);  // paisagem, 240 x 135
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  M5.Display.setBrightness(80);
  M5.Speaker.setVolume(120);
  LARANJA = M5.Display.color565(255, 121, 0);
  PRETO = M5.Display.color565(0, 0, 0);
  BRANCO = M5.Display.color565(255, 255, 255);
  VERDE = M5.Display.color565(40, 200, 90);
  CINZA = M5.Display.color565(140, 140, 140);

  M5.Display.fillScreen(PRETO);
  M5.Display.setTextColor(LARANJA, PRETO);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(6, 40);
  M5.Display.print("Tropa do Fogo");
  M5.Display.setCursor(6, 60);
  M5.Display.print("Amigo");
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, 100);
  M5.Display.print("conectando ao wifi...");

  prefs.begin("painel", false);
  bumpEm = prefs.getUInt("bump", 0);

  wifiOk = conectaWifi();
  if (wifiOk) {
    sincronizaHora();
    atualizaDiscord();
    atualizaSite();
    ultimoDiscord = ultimoSite = millis();
  }
  ultimaTroca = ultimaAtividade = millis();
  desenha();
}

void loop() {
  M5.update();
  uint32_t agora = millis();

  if (M5.BtnA.wasPressed()) {
    acordaTela();
    tela = (tela + 1) % NUM_TELAS;
    ultimaTroca = agora;
    pausaAte = agora + PAUSA_APOS_BOTAO_MS;
    desenha();
  }
  if (M5.BtnB.wasPressed()) {
    acordaTela();
    if (tela == 3 && horaOk) {
      bumpEm = (uint32_t)time(nullptr);
      prefs.putUInt("bump", bumpEm);
      bumpAvisou = false;
      M5.Speaker.tone(1500, 120);
      desenha();
    }
    pausaAte = agora + PAUSA_APOS_BOTAO_MS;
  }

  // Troca automática, para quando a placa está filmando ou em cima da mesa.
  if (!telaApagada && agora > pausaAte && agora - ultimaTroca > TROCA_AUTOMATICA_MS) {
    tela = (tela + 1) % NUM_TELAS;
    ultimaTroca = agora;
    desenha();
  } else if (!telaApagada && agora - ultimaTroca > 1000) {
    // Atualiza o relógio da tela atual sem trocar de tela.
    static uint32_t ultimoDesenho = 0;
    if (agora - ultimoDesenho > 1000) {
      ultimoDesenho = agora;
      desenha();
    }
  }

  // Avisa quando o bump fica liberado.
  if (horaOk && bumpEm && !bumpAvisou &&
      (long)time(nullptr) >= (long)(bumpEm + BUMP_ESPERA_S)) {
    bumpAvisou = true;
    acordaTela();
    apita();
  }

  // Atualiza os dados de tempos em tempos.
  if (WiFi.status() != WL_CONNECTED) {
    wifiOk = false;
    static uint32_t ultimaTentativa = 0;
    if (agora - ultimaTentativa > 60000) {
      ultimaTentativa = agora;
      wifiOk = conectaWifi();
      if (wifiOk && !horaOk) sincronizaHora();
    }
  } else {
    wifiOk = true;
    if (agora - ultimoDiscord > ATUALIZA_DISCORD_MS) {
      ultimoDiscord = agora;
      atualizaDiscord();
    }
    if (agora - ultimoSite > ATUALIZA_SITE_MS) {
      ultimoSite = agora;
      atualizaSite();
    }
  }

  // Apaga a tela depois de 5 minutos sem botão, para poupar a bateria.
  if (!telaApagada && agora - ultimaAtividade > TELA_APAGA_MS) {
    M5.Display.setBrightness(0);
    telaApagada = true;
  }

  delay(20);
}
