// ============================================================================
//  Recepteur.ino — écouter des variables ESP-NOW mono-variable
//
//  L'application DÉCLARE ce qu'elle veut (surveiller). Aucune collision
//  possible, mémoire déterministe, et les paires non écoutées sont comptées.
// ============================================================================
#include <EspNow_UniVar.h>
#include <Unity.h>

EspNowUniVar env;

void setup()
{
  Serial.begin(115200);
  delay(200);

  if (!env.begin(UVAR_IOT_ORDONNANCEUR)) {
    Serial.println("ESP-NOW : init impossible");
    return;
  }

  // Ce que CE nœud veut connaître. Rien d'autre n'occupe de mémoire.
  env.surveiller(UVAR_IOT_PZEM004T,  UVAR_PZEM_S_APPARENTE);
  env.surveiller(UVAR_IOT_PZEM004T,  UVAR_PZEM_WH_INDEX);
  env.surveiller(UVAR_IOT_LINKYSTIC, UVAR_LINKY_P_APP);
  env.surveiller(UVAR_IOT_PZEM004T,  UVAR_NOM);

  Serial.printf("Recette : %u slots surveilles, %u o de RAM\n",
                (unsigned)4, (unsigned)EspNowUniVar::empreinteRam());
}

void loop()
{
  // --- écart PZEM / Linky, avec contrôle de FRAÎCHEUR ----------------------
  // C'est ici que tout se joue : sans age_ms, une mesure figée produirait un
  // écart parfaitement crédible et parfaitement faux.
  float p_pzem = 0.0f, p_linky = 0.0f;
  uint32_t age_pzem = 0, age_linky = 0;

  const bool ok_pzem  = env.lire(UVAR_IOT_PZEM004T,  UVAR_PZEM_S_APPARENTE, p_pzem,  &age_pzem);
  const bool ok_linky = env.lire(UVAR_IOT_LINKYSTIC, UVAR_LINKY_P_APP,       p_linky, &age_linky);

  static uint32_t dernier = 0;
  if (millis() - dernier >= 5000) {
    dernier = millis();

    if (!ok_pzem || !ok_linky) {
      Serial.println("ecart : mesure absente");
    } else if (age_pzem > 3000 || age_linky > 3000) {
      Serial.printf("ecart : mesure PERIMEE (pzem %lu s, linky %lu s)\n",
                    (unsigned long)(age_pzem / 1000), (unsigned long)(age_linky / 1000));
    } else {
      // Affichage avec préfixe SI : Unity décide de kVA / MVA.
      Serial.printf("PZEM %s   Linky %s   ecart %s\n",
                    C_UNITY::valeurAvecUnite(p_pzem,  "VA", 2).c_str(),
                    C_UNITY::valeurAvecUnite(p_linky, "VA", 2).c_str(),
                    C_UNITY::valeurAvecUnite(p_pzem - p_linky, "VA", 2).c_str());
    }

    // Chaîne : toujours terminée par NUL, longueur bornée.
    char nom[24];
    if (env.lireTexte(UVAR_IOT_PZEM004T, UVAR_NOM, nom, sizeof(nom)))
      Serial.printf("emetteur PZEM : %s\n", nom);

    // Diagnostic : les rejets ne sont jamais silencieux.
    const UniVarStats& s = env.stats();
    Serial.printf("recus=%lu crc=%lu type=%lu non_surveille=%lu ancien=%lu perdus=%lu\n",
                  (unsigned long)s.recus,    (unsigned long)s.crc_ko,
                  (unsigned long)s.type_ko,  (unsigned long)s.non_surveille,
                  (unsigned long)s.ancien_format, (unsigned long)s.perdus);
  }
}
