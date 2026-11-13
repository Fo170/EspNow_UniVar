// ============================================================================
//  test_uni_var.cpp — 12 exigences de recette de EspNow_UniVar
//
//  Compilation hôte (aucune carte, aucun flash) :
//      ./run.sh
//
//  Les 12 tests correspondent au §12 du cahier des charges. Un test qui ne
//  passe pas est un BLOQUANT : la librairie ne doit pas être flashée.
// ============================================================================

#define UVAR_HOST_TEST      1
#define UVAR_MAX_PKT_S      8     // plafond abaissé pour être testable
#define UVAR_MAX_SLOTS      8
#define UVAR_MAX_STRLEN     16

#include <EspNow_UniVar.h>

// ----------------------------------------------------------------------------
//  CONTRAT LOCAL du test — la librairie est agnostique, elle ne fournit aucun
//  identifiant. On reproduit ici ce qu'un projet ferait dans son propre
//  fichier de contrat (append-only, 0 réservé par le protocole).
// ----------------------------------------------------------------------------
#define IOT_A               1    // émetteur principal
#define IOT_B               2    // second émetteur
#define IOT_C               3    // troisième émetteur
#define IOT_D               4    // quatrième émetteur

#define VAR_P               1    // W      (et VAR_P+1.. pour le test du plafond)
#define VAR_WH              2    // Wh
#define VAR_I               3    // A
#define VAR_S               4    // VA
#define VAR_PF              5    // PF
#define VAR_PAPP            6    // VA
#define VAR_NOM             7    // chaîne

#include <cstdio>
#include <cstring>
#include <cmath>

// ----------------------------------------------------------------------------
//  Micro-framework
// ----------------------------------------------------------------------------
static int g_ok = 0;
static int g_ko = 0;
static const char* g_test = "";

#define VERIF(cond)                                                        \
  do {                                                                     \
    if (cond) { g_ok++; }                                                  \
    else { g_ko++; printf("  ECHEC [%s] %s:%d  %s\n", g_test, __FILE__,    \
                          __LINE__, #cond); }                              \
  } while (0)

static void titre(const char* t) { g_test = t; printf("- %s\n", t); }

static uint8_t trameFloat(uint8_t* buf, float v, uint16_t id_var = VAR_S,
                          uint8_t id_iot = IOT_A, uint16_t seq = 1)
{
  return uvarEncode(buf, id_iot, id_var, UVAR_FLOAT32, _Unit_VA_, &v, 4, 1234, 1700000000UL, seq);
}

// ============================================================================
int main()
{
  printf("=== EspNow_UniVar — recette ===\n");
  printf("empreinte RAM : %u o (%u slots + %u pubs)\n\n",
         (unsigned)EspNowUniVar::empreinteRam(),
         (unsigned)UVAR_MAX_SLOTS, (unsigned)UVAR_MAX_PUB);

  // --------------------------------------------------------------------------
  titre("1. aller-retour pour chaque type");
  {
    float    f = -12.75f;
    int32_t  i = -123456;
    uint32_t u = 23296626UL;      // index Linky réel : perdu en float32
    bool     b = true;
    double   d = -987654321.125;
    const char* s = "PZEM-004T";

    struct Cas { uint8_t type; const void* p; uint8_t len; };
    const Cas cas[] = {
      { UVAR_FLOAT32, &f, 4 }, { UVAR_INT32, &i, 4 }, { UVAR_UINT32, &u, 4 },
      { UVAR_BOOL,    &b, 1 }, { UVAR_DOUBLE, &d, 8 }, { UVAR_STRING, s, 9 }
    };

    for (const Cas& c : cas) {
      uint8_t buf[UVAR_TRAME_MAX];
      const uint8_t n = uvarEncode(buf, IOT_A, VAR_P, c.type,
                                   _Unit_VA_, c.p, c.len, 42, 1700000123UL, 7);
      VERIF(n == (uint8_t)(UVAR_SURCOUT + c.len));

      UniVarVue v;
      VERIF(uvarDecode(buf, n, v) == UVAR_OK);
      VERIF(v.type == c.type);
      VERIF(v.len == c.len);
      VERIF(v.id_iot == IOT_A);
      VERIF(v.id_var == VAR_P);
      VERIF(v.unit == _Unit_VA_);
      VERIF(v.t_fct == 42);
      VERIF(v.epoch == 1700000123UL);
      VERIF(v.seq == 7);

      if (c.type == UVAR_FLOAT32) { float r;  memcpy(&r, v.valeur, 4); VERIF(r == f); }
      if (c.type == UVAR_INT32)   { int32_t r; memcpy(&r, v.valeur, 4); VERIF(r == i); }
      if (c.type == UVAR_UINT32)  { uint32_t r; memcpy(&r, v.valeur, 4);
                                    VERIF(r == u);
                                    VERIF((float)u != (float)r || r == u); } // uint32 préservé
      if (c.type == UVAR_BOOL)    { VERIF(v.valeur[0] == 1); }
      if (c.type == UVAR_DOUBLE)  { double r; memcpy(&r, v.valeur, 8); VERIF(r == d); }
      if (c.type == UVAR_STRING)  { VERIF(memcmp(v.valeur, s, 9) == 0); }
    }
  }

  // --------------------------------------------------------------------------
  titre("2. CRC : 1 bit corrompu rejete");
  {
    float f = 230.5f;
    uint8_t buf[UVAR_TRAME_MAX];
    const uint8_t n = trameFloat(buf, f);
    UniVarVue v;
    VERIF(uvarDecode(buf, n, v) == UVAR_OK);

    for (int bit = 0; bit < 8; bit++) {
      uint8_t copie[UVAR_TRAME_MAX];
      memcpy(copie, buf, n);
      copie[9] ^= (uint8_t)(1u << bit);          // un bit de la valeur
      VERIF(uvarDecode(copie, n, v) == UVAR_CRC_KO);
    }
    // Corruption dans l'en-tête (id_var) : CRC couvre aussi l'en-tête
    uint8_t copie[UVAR_TRAME_MAX];
    memcpy(copie, buf, n);
    copie[4] ^= 0x01;
    VERIF(uvarDecode(copie, n, v) == UVAR_CRC_KO);
  }

  // --------------------------------------------------------------------------
  titre("3. len incoherent avec type -> TYPE_KO");
  {
    uint8_t buf[UVAR_TRAME_MAX];
    double d = 1.5;
    // type FLOAT32 annoncé mais 8 octets de valeur
    const uint8_t n = uvarEncode(buf, IOT_A, VAR_P,
                                 UVAR_FLOAT32, _Unit_VA_, &d, 8, 0, 0, 1);
    VERIF(n == 29);
    UniVarVue v;
    VERIF(uvarDecode(buf, n, v) == UVAR_TYPE_KO);

    // type inconnu (99)
    const uint8_t n2 = uvarEncode(buf, IOT_A, VAR_P,
                                  99, _Unit_VA_, &d, 4, 0, 0, 1);
    VERIF(uvarDecode(buf, n2, v) == UVAR_TYPE_KO);
  }

  // --------------------------------------------------------------------------
  titre("4. len > 229 refuse a l'emission");
  {
    uint8_t buf[UVAR_TRAME_MAX];
    uint8_t grosse[300];
    memset(grosse, 'A', sizeof(grosse));

    VERIF(uvarEncode(buf, IOT_A, VAR_NOM, UVAR_STRING, _Unit_Aucune_,
                     grosse, UVAR_LEN_MAX, 0, 0, 1) == (uint8_t)(UVAR_SURCOUT + UVAR_LEN_MAX));
    VERIF(uvarEncode(buf, IOT_A, VAR_NOM, UVAR_STRING, _Unit_Aucune_,
                     grosse, (uint8_t)(UVAR_LEN_MAX + 1), 0, 0, 1) == 0);
    VERIF(uvarEncode(buf, IOT_A, VAR_NOM, UVAR_STRING, _Unit_Aucune_,
                     grosse, 0, 0, 0, 1) == 0);
    // id_iot non defini / id_var nul : refuses
    VERIF(uvarEncode(buf, UVAR_ID_IOT_INVALIDE, VAR_NOM, UVAR_STRING, _Unit_Aucune_,
                     grosse, 4, 0, 0, 1) == 0);
    VERIF(uvarEncode(buf, IOT_A, 0, UVAR_STRING, _Unit_Aucune_,
                     grosse, 4, 0, 0, 1) == 0);
  }

  // --------------------------------------------------------------------------
  titre("5. magic / ver incorrects rejetes");
  {
    float f = 1.0f;
    uint8_t buf[UVAR_TRAME_MAX];
    const uint8_t n = trameFloat(buf, f);
    UniVarVue v;

    uint8_t c1[UVAR_TRAME_MAX]; memcpy(c1, buf, n); c1[0] = 0x99;
    VERIF(uvarDecode(c1, n, v) == UVAR_MAGIC_KO);

    uint8_t c2[UVAR_TRAME_MAX]; memcpy(c2, buf, n); c2[2] = 2;
    VERIF(uvarDecode(c2, n, v) == UVAR_VER_KO);

    // trame trop courte
    VERIF(uvarDecode(buf, 5, v) == UVAR_LEN_KO);
    VERIF(uvarDecode(buf, 0, v) == UVAR_LEN_KO);
    VERIF(uvarDecode(nullptr, 10, v) == UVAR_LEN_KO);

    // ancien format EspNowSharedVariable (type_id 0x09 = TYPE_FLOAT)
    uint8_t vieux[16] = { 0x09, 0x04, 'a', 'b', 'c', 'd', 0x04, 0x00, 0, 0, 0, 0, 0, 0, 0, 0 };
    VERIF(uvarDecode(vieux, 16, v) == UVAR_ANCIEN_FORMAT);
    vieux[0] = 0xFF;   // TYPE_STRUCT
    VERIF(uvarDecode(vieux, 16, v) == UVAR_ANCIEN_FORMAT);
  }

  // --------------------------------------------------------------------------
  titre("6. paire non surveillee : comptee, aucun slot ecrit");
  {
    EspNowUniVar env;
    float f = 3.5f;
    uint8_t buf[UVAR_TRAME_MAX];
    const uint8_t n = trameFloat(buf, f);

    env._traiterTrame(buf, n);
    VERIF(env.stats().non_surveille == 1);
    VERIF(env.stats().recus == 0);

    float lu; uint32_t age;
    VERIF(env.lire(IOT_A, VAR_S, lu, &age) == false);

    // surveillee mais jamais recue
    VERIF(env.surveiller(IOT_A, VAR_S) == true);
    VERIF(env.lire(IOT_A, VAR_S, lu, &age) == false);

    // surveillee et recue
    env._traiterTrame(buf, n);
    VERIF(env.stats().recus == 1);
    VERIF(env.lire(IOT_A, VAR_S, lu, &age) == true);
    VERIF(lu == f);

    // mauvais type demande : refus explicite, pas de reinterpretation
    uint32_t mauvais;
    VERIF(env.lire(IOT_A, VAR_S, mauvais, &age) == false);

    // table pleine : surveiller() echoue franchement
    for (int k = 0; k < UVAR_MAX_SLOTS + 4; k++)
      env.surveiller((uint8_t)(20 + k), (uint16_t)(500 + k));
    VERIF(env.surveiller(IOT_D, 999) == false);
  }

  // --------------------------------------------------------------------------
  titre("7. age_ms exact");
  {
    EspNowUniVar env;
    env.surveiller(IOT_B, VAR_PAPP);

    float f = 250.0f;
    uint8_t buf[UVAR_TRAME_MAX];
    const uint8_t n = uvarEncode(buf, IOT_B, VAR_PAPP,
                                 UVAR_FLOAT32, _Unit_VA_, &f, 4, 10, 0, 1);

    g_millis = 5000;
    env._traiterTrame(buf, n);

    float lu; uint32_t age = 0;
    VERIF(env.lire(IOT_B, VAR_PAPP, lu, &age) == true);
    VERIF(age == 0);

    g_millis = 5000 + 2750;
    VERIF(env.lire(IOT_B, VAR_PAPP, lu, &age) == true);
    VERIF(age == 2750);

    // robustesse au debordement de millis()
    g_millis = 0xFFFFFF00UL;
    env._traiterTrame(buf, n);
    g_millis = 0x00000100UL;   // 512 ms plus tard, apres wrap
    VERIF(env.lire(IOT_B, VAR_PAPP, lu, &age) == true);
    VERIF(age == 512);
  }

  // --------------------------------------------------------------------------
  titre("8. saut de seq de 3 -> perdus += 3");
  {
    EspNowUniVar env;
    env.surveiller(IOT_A, VAR_P);

    float f = 1.0f;
    uint8_t buf[UVAR_TRAME_MAX];

    // seq 10 : premiere trame, aucun trou possible
    uint8_t n = uvarEncode(buf, IOT_A, VAR_P,
                           UVAR_FLOAT32, _Unit_Watt_, &f, 4, 0, 0, 10);
    env._traiterTrame(buf, n);
    VERIF(env.stats().perdus == 0);

    // seq 11 : consecutive
    n = uvarEncode(buf, IOT_A, VAR_P,
                   UVAR_FLOAT32, _Unit_Watt_, &f, 4, 0, 0, 11);
    env._traiterTrame(buf, n);
    VERIF(env.stats().perdus == 0);

    // seq 15 : attendu 12 -> 3 perdus
    n = uvarEncode(buf, IOT_A, VAR_P,
                   UVAR_FLOAT32, _Unit_Watt_, &f, 4, 0, 0, 15);
    env._traiterTrame(buf, n);
    VERIF(env.stats().perdus == 3);

    uint32_t perdus_slot = 0; uint32_t age = 0; uint8_t type = 0xFF, unit = 0xFF;
    VERIF(env.infos(IOT_A, VAR_P, &type, &unit, &age, &perdus_slot));
    VERIF(perdus_slot == 3);
    VERIF(type == UVAR_FLOAT32);
    VERIF(unit == _Unit_Watt_);
  }

  // --------------------------------------------------------------------------
  titre("9. plafond UVAR_MAX_PKT_S respecte sous rafale");
  {
    // millis() est monotone : on part d'une base, on ne revient jamais en arriere.
    g_millis = 100000;
    EspNowUniVar env;
    VERIF(env.begin(IOT_A) == false);   // aucune radio sur l'hote
    VERIF(env.idIot() == IOT_A);        // l'identite est posee quand meme

    // UVAR_MAX_PUB variables dues d'un coup : le plafond doit en refuser.
    float v[UVAR_MAX_PUB];
    for (int k = 0; k < UVAR_MAX_PUB; k++) {
      v[k] = (float)k;
      VERIF(env.publier((uint16_t)(VAR_P + k), &v[k], _Unit_Watt_, 100));
    }

    env.poll();
    VERIF(env.stats().envoyes == UVAR_MAX_PKT_S);
    VERIF(env.stats().plafond_ko == (uint32_t)(UVAR_MAX_PUB - UVAR_MAX_PKT_S));

    g_millis = 101000;                           // nouvelle fenetre
    env.poll();
    VERIF(env.stats().envoyes == UVAR_MAX_PKT_S * 2);
  }

  // --------------------------------------------------------------------------
  titre("10. periodes independantes");
  {
    g_millis = 200000;
    EspNowUniVar env;
    env.begin(IOT_A);                // radio absente : identite seule
    float rapide = 10.0f, lent = 20.0f;

    VERIF(env.publier(VAR_P,  &rapide, _Unit_Watt_, 100));
    VERIF(env.publier(VAR_WH,  &lent,   _Unit_Wh_,   1000));

    // periode sous le plancher : relevee a UVAR_PERIODE_MIN_MS
    float tiers = 30.0f;
    VERIF(env.publier(VAR_I, &tiers, _Unit_Ampere_, 1));

    // t=200000 : les 3 sont dues (prochaine_ms = millis() a la declaration)
    g_millis = 200000; env.poll();
    VERIF(env.stats().envoyes == 3);

    // t=200100 : rapide (100 ms) + tiers (1 ms releve au plancher de 100 ms)
    g_millis = 200100; env.poll();
    VERIF(env.stats().envoyes == 5);

    // t=200200 : rapide + tiers, toujours pas de lent
    g_millis = 200200; env.poll();
    VERIF(env.stats().envoyes == 7);

    // t=200250 : rien n'est du — le plancher de 100 ms a bien ete applique
    //            (avec une periode de 1 ms, tiers serait du ici)
    g_millis = 200250; env.poll();
    VERIF(env.stats().envoyes == 7);

    // t=201000 : rapide + tiers + lent (1000 ms)
    g_millis = 201000; env.poll();
    VERIF(env.stats().envoyes == 10);

    // envoi manuel d'une variable a periode 0
    float manuel = 42.0f;
    VERIF(env.publier(VAR_PF, &manuel, _Unit_PF_, 0));
    const uint32_t avant = env.stats().envoyes;
    g_millis = 201001; env.poll();
    VERIF(env.stats().envoyes == avant);         // periode 0 => jamais dans poll()
    VERIF(env.envoyer(VAR_PF) == true);
    VERIF(env.stats().envoyes == avant + 1);
  }

  // --------------------------------------------------------------------------
  titre("11. chaine sans NUL, longueur exacte");
  {
    EspNowUniVar env;
    env.surveiller(IOT_C, VAR_NOM);

    const char* nom = "Station-Meteo";   // 13 caracteres
    uint8_t buf[UVAR_TRAME_MAX];
    const uint8_t n = uvarEncode(buf, IOT_C, VAR_NOM,
                                 UVAR_STRING, _Unit_Aucune_, nom, 13, 0, 0, 1);
    VERIF(n == 34);                       // 21 + 13
    env._traiterTrame(buf, n);

    char lu[32];
    uint32_t age;
    VERIF(env.lireTexte(IOT_C, VAR_NOM, lu, sizeof(lu), &age) == true);
    VERIF(strcmp(lu, nom) == 0);
    VERIF(strlen(lu) == 13);

    // chaine plus longue que UVAR_MAX_STRLEN : tronquee proprement, NUL garanti
    char long_nom[80];
    memset(long_nom, 'X', sizeof(long_nom));
    long_nom[79] = '\0';
    const uint8_t n2 = uvarEncode(buf, IOT_C, VAR_NOM,
                                  UVAR_STRING, _Unit_Aucune_, long_nom, 60, 0, 0, 2);
    VERIF(n2 == 81);
    env._traiterTrame(buf, n2);

    char lu2[64];
    VERIF(env.lireTexte(IOT_C, VAR_NOM, lu2, sizeof(lu2), &age) == true);
    VERIF(strlen(lu2) == UVAR_MAX_STRLEN - 1);
    VERIF(lu2[UVAR_MAX_STRLEN - 1] == '\0');

    // une chaine n'est pas lisible comme un float
    float pas_une_chaine;
    VERIF(env.lire(IOT_C, VAR_NOM, pas_une_chaine, &age) == false);
  }

  // --------------------------------------------------------------------------
  titre("12. trame 250 o acceptee, 251 refusee");
  {
    uint8_t buf[UVAR_TRAME_MAX];
    char grosse[300];
    memset(grosse, 'Z', sizeof(grosse));

    const uint8_t n = uvarEncode(buf, IOT_D, VAR_NOM, UVAR_STRING,
                                 _Unit_Aucune_, grosse, UVAR_LEN_MAX, 0, 0, 1);
    VERIF(n == UVAR_TRAME_MAX);          // 21 + 229 = 250

    UniVarVue v;
    VERIF(uvarDecode(buf, n, v) == UVAR_OK);
    VERIF(v.len == UVAR_LEN_MAX);

    // 251 octets : refuse a l'encodage, et rejete au decodage
    VERIF(uvarEncode(buf, IOT_D, VAR_NOM, UVAR_STRING, _Unit_Aucune_,
                     grosse, (uint8_t)(UVAR_LEN_MAX + 1), 0, 0, 1) == 0);

    uint8_t trop[251];
    memset(trop, 'Z', sizeof(trop));
    VERIF(uvarDecode(trop, 251, v) == UVAR_MAGIC_KO);
  }

  // --------------------------------------------------------------------------
  printf("\n%d verifications OK, %d echec(s)\n", g_ok, g_ko);
  if (g_ko == 0) printf("RESULTAT : OK\n");
  else           printf("RESULTAT : ECHEC\n");
  return g_ko == 0 ? 0 : 1;
}
