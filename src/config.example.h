#pragma once

// Copie este arquivo para config.h (na mesma pasta) e preencha.
// ⚠️ config.h NÃO vai para o repositório (está no .gitignore): é a senha do
// seu Wi-Fi. Nunca cole esse arquivo em chat, e-mail ou issue.
//
// A placa só usa Wi-Fi de 2,4 GHz. Rede de empresa com login (WPA-Enterprise)
// ou com página de entrada (portal) NÃO funciona; o roteador de casa e o
// roteamento do celular funcionam. Ela tenta as redes na ordem da lista.

struct Rede {
  const char* ssid;
  const char* senha;
};

static const Rede REDES[] = {
    {"NOME_DO_WIFI_DE_CASA", "SENHA_DO_WIFI_DE_CASA"},
    {"NOME_DO_ROTEADOR_DO_CELULAR", "SENHA_DO_ROTEADOR_DO_CELULAR"},
};
