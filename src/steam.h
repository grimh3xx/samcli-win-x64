/* steam.h - camada fina sobre a API flat do Steamworks (steam_api64.dll / libsteam_api.so). */
#ifndef STEAM_H
#define STEAM_H

#include "platform.h"

typedef enum {
    STEAM_OK = 0,
    STEAM_ERR_DLL,          /* steam_api64.dll / libsteam_api.so ausente  */
    STEAM_ERR_EXPORT,       /* DLL sem alguma funcao obrigatoria          */
    STEAM_ERR_NOT_RUNNING,  /* cliente Steam fechado                      */
    STEAM_ERR_INIT,         /* SteamAPI_Init falhou                       */
    STEAM_ERR_IFACE,        /* sem ISteamUserStats                        */
    STEAM_ERR_NOT_OWNED,    /* jogo nao esta na conta                     */
    STEAM_ERR_NO_STATS      /* sem conquistas / stats nao carregaram      */
} steam_status;

typedef struct {
    const char *api;        /* nome interno, ex: ACH_WIN_ONE_GAME        */
    const char *name;       /* nome de exibicao (idioma da Steam)         */
    const char *desc;
    bool        achieved;
    bool        hidden;
    uint32_t    unlock_time; /* unix time, 0 se desconhecido               */
} achievement;

steam_status steam_load(void);
steam_status steam_init(uint32_t appid);
steam_status steam_wait_stats(int timeout_ms, void (*tick)(void));
const char  *steam_error_detail(void);

uint32_t steam_count(void);
bool     steam_get(uint32_t index, achievement *out);
int      steam_set(const char *api, bool unlock);   /* 1 ok, 0 falhou, -1 nome nao existe, 2 ja estava */
bool     steam_store(void);
void     steam_pump(unsigned ms);
void     steam_shutdown(void);

/* Nome do jogo lido do appmanifest local (so jogos instalados). */
bool steam_game_name(uint32_t appid, char *out, size_t cap);

#endif
