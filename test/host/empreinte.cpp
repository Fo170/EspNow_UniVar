// empreinte.cpp — taille statique réelle avec la configuration PAR DÉFAUT.
#define UVAR_HOST_TEST 1
#include <EspNow_UniVar.h>
#include <cstdio>

int main()
{
  printf("UniVarSlot      : %2u o  x %2u slots = %5u o\n",
         (unsigned)sizeof(UniVarSlot), UVAR_MAX_SLOTS,
         (unsigned)sizeof(UniVarSlot) * UVAR_MAX_SLOTS);
  printf("UniVarPub       : %2u o  x %2u pubs  = %5u o\n",
         (unsigned)sizeof(UniVarPub), UVAR_MAX_PUB,
         (unsigned)sizeof(UniVarPub) * UVAR_MAX_PUB);
  printf("UniVarStats     : %2u o\n", (unsigned)sizeof(UniVarStats));
  printf("sizeof(void*)   : %u o  (4 sur ESP8266/ESP32, 8 sur hote 64 bits)\n",
         (unsigned)sizeof(void*));
  printf("empreinteRam()  : %u o\n", (unsigned)EspNowUniVar::empreinteRam());
  printf("sizeof(instance): %u o  (classe complete, sans radio)\n",
         (unsigned)sizeof(EspNowUniVar));
  return 0;
}
