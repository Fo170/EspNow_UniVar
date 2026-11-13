// ============================================================================
//  Emetteur.ino — publier des variables ESP-NOW mono-variable
//
//  Chaque variable part dans SON paquet, à SA période. Ajouter une variable
//  ne demande de reflasher aucun récepteur.
// ============================================================================
#include <EspNow_UniVar.h>

EspNowUniVar env;

// --- valeurs métier (l'application les met à jour, la lib les diffuse) ------
float    pzem_s      = 0.0f;   // VA
uint32_t pzem_wh     = 0;      // Wh  — uint32 : 23 296 626 Wh ne tient pas en float
float    pzem_pf     = 0.0f;   // facteur de puissance
bool     relais      = false;

void setup()
{
  Serial.begin(115200);
  delay(200);

  if (!env.begin(UVAR_IOT_PZEM004T)) {
    Serial.println("ESP-NOW : init impossible");
    return;
  }

  // (id_var, pointeur, unité, période_ms)
  env.publier(UVAR_PZEM_S_APPARENTE, &pzem_s,  _Unit_VA_,   1000);
  env.publier(UVAR_PZEM_PF,          &pzem_pf, _Unit_PF_,   1000);
  env.publier(UVAR_PZEM_WH_INDEX,    &pzem_wh, _Unit_Wh_,  60000);
  env.publier(UVAR_CHF_EAU_ETAT,     &relais,  _Unit_Aucune_, 0);   // manuel

  // Une chaîne : longueur mesurée à l'envoi, jamais de NUL transmis.
  env.publierTexte(UVAR_NOM, "PZEM-004T", _Unit_Aucune_, 300000);

  Serial.println("Emetteur pret");
}

void loop()
{
  // Simulation de mesure (à remplacer par la vraie lecture).
  static uint32_t t0 = millis();
  if (millis() - t0 >= 1000) {
    t0 = millis();
    pzem_s  = 1200.0f + (float)(millis() % 200);
    pzem_pf = 0.92f;
    pzem_wh += 1;
  }

  env.poll();   // n'émet que ce qui est dû

  // Changement d'état : envoi immédiat, hors période.
  static bool dernier = false;
  if (relais != dernier) {
    dernier = relais;
    env.envoyer(UVAR_CHF_EAU_ETAT);
  }

  // Diagnostic : sans ces compteurs, un rejet resterait invisible.
  static uint32_t dernierLog = 0;
  if (millis() - dernierLog >= 60000) {
    dernierLog = millis();
    const UniVarStats& s = env.stats();
    Serial.printf("envoyes=%lu echecs=%lu plafond=%lu\n",
                  (unsigned long)s.envoyes, (unsigned long)s.echecs_radio,
                  (unsigned long)s.plafond_ko);
  }
}
