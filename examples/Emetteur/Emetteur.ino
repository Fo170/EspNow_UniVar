// ============================================================================
//  Emetteur.ino — publier des variables ESP-NOW mono-variable
//
//  Chaque variable part dans SON paquet, à SA période. Ajouter une variable
//  ne demande de reflasher aucun récepteur.
// ============================================================================
#include <EspNow_UniVar.h>

// ----------------------------------------------------------------------------
//  CONTRAT LOCAL — à adapter à VOTRE parc.
//
//  ⚠️ La librairie est AGNOSTIQUE : elle ne connaît aucun identifiant métier.
//     C'est à l'application de définir ses `id_iot` et `id_var`, idéalement
//     dans un fichier partagé entre tous ses nœuds.
//
//  Règles :
//    - 0 est réservé par le protocole (émetteur ou variable non identifié) ;
//    - APPEND-ONLY : un numéro attribué n'est jamais renuméroté ni réutilisé,
//      sinon un récepteur non reflashé lirait une valeur sous un autre sens ;
//    - une unité par variable, fixée par le contrat et non par l'émetteur.
// ----------------------------------------------------------------------------
#define ID_IOT_CAPTEUR      1    // identifiant de CE nœud
#define VAR_TENSION         1    // V
#define VAR_COURANT         2    // A
#define VAR_PUISSANCE       3    // W
#define VAR_ENERGIE         4    // Wh  (uint32 — un float perdrait les unités)
#define VAR_NOM             5    // chaîne

EspNowUniVar env;

// --- valeurs métier (l'application les met à jour, la lib les diffuse) ------
float    tension  = 0.0f;
float    courant  = 0.0f;
float    puissance = 0.0f;
uint32_t energie  = 0;      // Wh : 23 296 626 ne tient pas dans un float
bool     relais   = false;

void setup()
{
  Serial.begin(115200);
  delay(200);

  if (!env.begin(ID_IOT_CAPTEUR)) {
    Serial.println("ESP-NOW : init impossible");
    return;
  }

  // (id_var, pointeur, unité, période_ms)
  env.publier(VAR_TENSION,   &tension,   _Unit_Volt_,   1000);
  env.publier(VAR_COURANT,   &courant,   _Unit_Ampere_, 1000);
  env.publier(VAR_PUISSANCE, &puissance, _Unit_Watt_,   1000);
  env.publier(VAR_ENERGIE,   &energie,   _Unit_Wh_,    60000);

  // Une chaîne : longueur mesurée à l'envoi, jamais de NUL transmis.
  env.publierTexte(VAR_NOM, "capteur-A", _Unit_Aucune_, 300000);

  Serial.println("Emetteur pret");
}

void loop()
{
  // Simulation de mesure (à remplacer par la vraie lecture).
  static uint32_t t0 = millis();
  if (millis() - t0 >= 1000) {
    t0 = millis();
    tension   = 230.0f + (float)(millis() % 5);
    courant   = 8.7f;
    puissance = tension * courant;
    energie  += 2;
  }

  env.poll();   // n'émet que ce qui est dû

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
