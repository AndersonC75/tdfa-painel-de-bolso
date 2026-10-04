# Painel de bolso da Tropa do Fogo Amigo

Projeto da comunidade [Tropa do Fogo Amigo](https://tdfa.com.br). O passo a passo completo, com os problemas que apareceram no caminho, está na matéria do site: https://tdfa.com.br/noticias/como-montar-painel-de-bolso-contagem-gta-6-m5stickc-plus2

Programa para o **M5StickC Plus2**. Quatro telas que passam sozinhas a cada 8
segundos:

| Tela | Mostra |
|---|---|
| 1 | Dias, horas e minutos até o desbloqueio de GTA 6 no Brasil (19/11, meia-noite de Brasília) |
| 2 | Quantos estão em call no Discord e quantos online |
| 3 | A manchete mais nova do site |
| 4 | O relógio do bump (2 horas), marcado à mão |

**Botões**

- **A (frente):** próxima tela, e pausa a troca automática por 60 segundos
- **B (lateral direito):** na tela do bump, marca "dei o /bump agora". A placa
  apita quando passam 2 horas e o bump fica liberado
- Depois de 5 minutos sem apertar nada, a tela apaga para poupar a bateria.
  Qualquer botão acorda

## Como gravar na placa

Precisa de: o cabo USB-C, o Python instalado e um Wi-Fi de **2,4 GHz**.

1. Copie `src/config.example.h` para `src/config.h` e preencha o nome e a senha
   do seu Wi-Fi (pode pôr duas redes: a de casa e o roteamento do celular).
   ⚠️ **`config.h` é a senha do seu Wi-Fi. Fica só no seu computador** (o
   `.gitignore` impede de ir para o repositório). Não cole em chat.
2. Instale o PlatformIO uma vez: `python -m pip install platformio`
3. Com a placa ligada por USB, nesta pasta: `python -m platformio run -t upload`
4. Para ver o que a placa está fazendo: `python -m platformio device monitor`

No Windows, se a placa não aparecer como porta COM, instale o driver
**CH9102** (da WCH) e tente de novo.

## Funciona fora de casa?

Sim, em qualquer lugar com internet e uma das redes da lista. Não funciona em
Wi-Fi de empresa com login nem em Wi-Fi com página de entrada (hotel,
aeroporto). O roteamento do celular funciona.

## De onde vêm os dados

- **Discord:** a placa pergunta direto ao widget público do servidor, de casa,
  como o navegador do leitor faz. Não passa pelo site, de propósito: passar
  pelo servidor do site gasta a cota de gravação do armazenamento dele e
  esbarra no limite de pedidos do Discord. O widget do servidor (Configurações
  do servidor, Widget) precisa estar ligado. Para usar no SEU servidor, troque
  o número do servidor em `URL_DISCORD` no `main.cpp`.
- **Site:** `https://tdfa.com.br/api/painel` (só leitura): a data de lançamento
  e as manchetes, sem acento (a fonte da tela não desenha acento).

## Cuidados

- A conexão usa TLS sem conferir o certificado (`setInsecure`). Os dados são
  públicos. **Não coloque senha nem token** neste programa.
- A contagem usa a hora da internet (NTP). Sem Wi-Fi na partida, a tela mostra
  "sem hora ainda".
- O relógio do bump guarda o último `/bump` na memória da placa, e sobrevive a
  desligar. Ele é manual: a placa não sabe se alguém deu o bump no Discord.
