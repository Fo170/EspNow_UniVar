// ============================================================================
//  Recepteur.ino — écouter des variables ESP-NOW mono-variable
//
//  L'application DÉCLARE ce qu'elle veut (surveiller). Aucune collision
//  possible, mémoire déterministe, et les paires non écoutées sont comptées.
// ============================================================================
#include <EspNow_UniVar.h>
#include <Unity.h>

// ----------------------------------------------------------------------------
//  CONTRAT LOCAL — à adapter à VOTRE parc.
//  La librairie est agnostique : elle ne connaît aucun identifiant.
//  Ce bloc doit être IDENTIQUE chez l'émetteur et chez le récepteur.
// ----------------------------------------------------------------------------
#define ID_IOT_CE_NOEUD     2    // identifiant de CE nœud
#define ID_IOT_CAPTEUR      1    // le nœud dont on veut les mesures

#define VAR_TENSION         1    // V
#define VAR_COURANT         2    // A
#define VAR_PUISSANCE       3    // W
#define VAR_ENERGIE         4    // Wh
#define VAR_NOM             5    // chaîne

EspNowUniVar env;

void setup()
{
  Serial.begin(115200);
  delay(200);

  if (!env.begin(ID_IOT_CE_NOEUD)) {
    Serial.println("ESP-NOW : init impossible");
    return;
  }

  // Ce que CE nœud veut connaître. Rien d'autre n'occupe de mémoire.
  env.surveiller(ID_IOT_CAPTEUR, VAR_TENSION);
  env.surveiller(ID_IOT_CAPTEUR, VAR_PUISSANCE);
  env.surveiller(ID_IOT_CAPTEUR, VAR_ENERGIE);
  env.surveiller(ID_IOT_CAPTEUR, VAR_NOM);

  Serial.printf("Recette : 4 slots surveilles, %u o de RAM\n",
                (unsigned)EspNowUniVar::empreinteRam());
}

void loop()
{
  // --- lecture avec contrôle de FRAÎCHEUR ---------------------------------
  // C'est ici que tout se joue : sans age_ms, une mesure figée produirait une
  // valeur parfaitement crédible et parfaitement fausse.
  float puissance = 0.0f, tension = 0.0f;
  uint32_t age_p = 0, age_u = 0;

  const bool ok_p = env.lire(ID_IOT_CAPTEUR, VAR_PUISSANCE, puissance, &age_p);
  const bool ok_u = env.lire(ID_IOT_CAPTEUR, VAR_TENSION,  tension,   &age_u);

  static uint32_t dernier = 0;
  if (millis() - dernier >= 5000) {
    dernier = millis();

    if (!ok_p || !ok_u) {
      Serial.println("mesure absente");
    } else if (age_p > 3000 || age_u > 3000) {
      Serial.printf("mesure PERIMEE (P %lu s, U %lu s)\n",
                    (unsigned long)(age_p / 1000), (unsigned long)(age_u / 1000));
    } else {
      // Affichage avec préfixe SI : Unity décide de W / kW / MW.
      Serial.printf("P = %s   U = %s\n",
                    C_UNITY::valeurAvecUnite(puissance, "W", 2).c_str(),
                    C_UNITY::valeurAvecUnite(tension,   "V", 2).c_str());
    }

    // Chaîne : toujours terminée par NUL, longueur bornée.
    char nom[24];
    if (env.lireTexte(ID_IOT_CAPTEUR, VAR_NOM, nom, sizeof(nom)))
      Serial.printf("emetteur : %s\n", nom);

    // Diagnostic : les rejets ne sont jamais silencieux.
    const UniVarStats& s = env.stats();
    Serial.printf("recus=%lu crc=%lu type=%lu non_surveille=%lu ancien=%lu perdus=%lu\n",
                  (unsigned long)s.recus,    (unsigned long)s.crc_ko,
                  (unsigned long)s.type_ko,  (unsigned long)s.non_surveille,
                  (unsigned long)s.ancien_format, (unsigned long)s.perdus);
  }
}
