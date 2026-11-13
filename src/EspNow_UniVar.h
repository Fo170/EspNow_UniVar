// ============================================================================
//  EspNow_UniVar.h — ESP-NOW mono-variable, trame auto-descriptive
//
//  Remplace les deux formats monolithiques du parc :
//      ESP_NOW_echange.h     (EspNowEchange, 40 o, struct_id 0xE51F)
//      espnow_message.h      (struct_message_iOT_espnow, ~90 o)
//  Ces deux structs se rejetaient mutuellement : 48 % des trames étaient jetées
//  par _findVarByName() < 0, sans que rien ne le signale.
//
//  PRINCIPE
//    Un paquet = une variable. La trame porte son type, son unité et sa
//    longueur : le récepteur la décode sans partager le struct de l'émetteur.
//    Ajouter une variable ne touche que son émetteur et ses abonnés.
//
//  TAILLE : 21 + len octets (float32 = 25 o, double = 29 o, chaîne 32 = 53 o)
//
//  NON FONCTIONNEL
//    - header-only, ZÉRO allocation dynamique (aucune String, aucun new)
//    - poll() non bloquant, aucun delay()
//    - ESP8266 et ESP32 (IDF 4.x / 5.x / 6.x), même code applicatif
//    - aucune interprétation par cast de pointeur : tout passe par memcpy et
//      par lecture octet par octet (alignement de double non garanti)
//
//  Auteur  : FOURNET Olivier (GPL-3.0)
//  Version : 1.1.1
// ============================================================================
#ifndef ESPNOW_UNIVAR_H
#define ESPNOW_UNIVAR_H

// Version de la librairie (source UNIQUE — agrégée par liste_lib.h)
#define __ESPNOW_UNIVAR_H_VERSION__ "1.1.1"

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// Codes d'unité : la trame transporte l'entier de Unity_Dictionary.
// Dépendance volontaire : elle garantit qu'émetteur et récepteur parlent
// le MÊME dictionnaire (c'est la leçon des deux structs incompatibles).
#include <Unity_Dictionary.h>

#include "EspNow_UniVar_Vars.h"

// ============================================================================
//  PLATEFORME
// ============================================================================
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  extern "C" {
    #include <espnow.h>
  }
#elif defined(ESP32)
  #include <WiFi.h>
  #include <esp_now.h>
  #include <esp_wifi.h>
  #include <esp_idf_version.h>
#endif
// (hors ESP8266/ESP32 la couche radio est absente : le codec reste utilisable
//  et testable sur l'hôte — voir test/host/)

// ============================================================================
//  CONSTANTES DU FORMAT — à ne jamais modifier sans changer UVAR_VER
// ============================================================================
#define UVAR_MAGIC0        0x46   // 'F'
#define UVAR_MAGIC1        0x56   // 'V'
#define UVAR_VER           1      // version du format

#define UVAR_ENTETE        9      // magic(2) ver(1) id_iot(1) id_var(2) type(1) unit(1) len(1)
#define UVAR_PIED          12     // t_fct(4) epoch(4) seq(2) crc(2)
#define UVAR_SURCOUT       (UVAR_ENTETE + UVAR_PIED)   // 21
#define UVAR_LEN_MAX       229    // 250 (limite ESP-NOW) - 21
#define UVAR_TRAME_MAX     250

#define UVAR_CRC_PORTEE(len)  ((uint16_t)(UVAR_ENTETE + (len) + 10))  // 0 .. 18+len

// ============================================================================
//  CONFIGURATION — définir AVANT l'include pour surcharger
// ============================================================================
#ifndef UVAR_MAX_PUB
  #define UVAR_MAX_PUB        12   // variables publiables par nœud
#endif
#ifndef UVAR_MAX_SLOTS
  #define UVAR_MAX_SLOTS      24   // variables surveillées (60 o chacune)
#endif
#ifndef UVAR_MAX_STRLEN
  #define UVAR_MAX_STRLEN     24   // texte stocké par slot (chaînes tronquées)
#endif
#ifndef UVAR_MAX_PKT_S
  #define UVAR_MAX_PKT_S      20   // plafond d'émission par seconde (airtime)
#endif
#ifndef UVAR_PERIODE_MIN_MS
  #define UVAR_PERIODE_MIN_MS 100  // plancher : protège d'une période absurde
#endif

// ============================================================================
//  TYPES DE VALEUR
// ============================================================================
enum UniVarType : uint8_t {
  UVAR_FLOAT32 = 0,
  UVAR_INT32   = 1,
  UVAR_UINT32  = 2,
  UVAR_BOOL    = 3,
  UVAR_DOUBLE  = 4,
  UVAR_STRING  = 5
};

// Statuts de décodage — chaque rejet est COMPTÉ, jamais silencieux.
enum UniVarStatut : uint8_t {
  UVAR_OK             = 0,
  UVAR_MAGIC_KO       = 1,
  UVAR_VER_KO         = 2,
  UVAR_LEN_KO         = 3,
  UVAR_TYPE_KO        = 4,
  UVAR_CRC_KO         = 5,
  UVAR_ANCIEN_FORMAT  = 6,   // trame EspNowSharedVariable : à déléguer ou ignorer
  UVAR_INCONNU        = 7    // id_iot ou id_var hors contrat
};

// Taille imposée par un type : >0 = fixe, -1 = variable (chaîne), 0 = inconnu.
inline int8_t uvarTailleType(uint8_t type)
{
  switch (type) {
    case UVAR_FLOAT32:
    case UVAR_INT32:
    case UVAR_UINT32:  return 4;
    case UVAR_BOOL:    return 1;
    case UVAR_DOUBLE:  return 8;
    case UVAR_STRING:  return -1;
    default:           return 0;
  }
}

// Un type_id de l'ancienne lib (EspNowSharedVariable) : 0x00..0x0B ou 0xFF.
// Sert à distinguer une vieille trame d'une trame corrompue.
inline bool uvarEstAncienFormat(uint8_t premier)
{
  return (premier <= 0x0B) || (premier == 0xFF);
}

// ============================================================================
//  OUTILS OCTETS — little-endian explicite, aucune dépendance à l'alignement
// ============================================================================
inline void uvarPut16(uint8_t* p, uint16_t v)
{
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

inline void uvarPut32(uint8_t* p, uint32_t v)
{
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

inline uint16_t uvarGet16(const uint8_t* p)
{
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

inline uint32_t uvarGet32(const uint8_t* p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// CRC-16/CCITT-FALSE : poly 0x1021, init 0xFFFF, ni réflexion ni XOR final.
// Version bitwise : aucune table de 512 o en flash, ~8 itérations par octet.
inline uint16_t uvarCrc16(const uint8_t* d, uint16_t n)
{
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < n; i++) {
    crc ^= (uint16_t)((uint16_t)d[i] << 8);
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

// ============================================================================
//  ENCODAGE
//  Renvoie la longueur de la trame écrite (21 + len), ou 0 si refusée.
// ============================================================================
inline uint8_t uvarEncode(uint8_t* buf, uint8_t id_iot, uint16_t id_var,
                          uint8_t type, uint8_t unit,
                          const void* valeur, uint8_t len,
                          uint32_t t_fct, uint32_t epoch, uint16_t seq)
{
  if (buf == nullptr || valeur == nullptr) return 0;
  if (len == 0 || len > UVAR_LEN_MAX)      return 0;
  if (id_iot == UVAR_IOT_INDEFINI)         return 0;
  if (id_var == 0)                         return 0;

  uint16_t i = 0;
  buf[i++] = UVAR_MAGIC0;
  buf[i++] = UVAR_MAGIC1;
  buf[i++] = UVAR_VER;
  buf[i++] = id_iot;
  uvarPut16(&buf[i], id_var); i = (uint16_t)(i + 2);
  buf[i++] = type;
  buf[i++] = unit;
  buf[i++] = len;
  memcpy(&buf[i], valeur, len); i = (uint16_t)(i + len);
  uvarPut32(&buf[i], t_fct);    i = (uint16_t)(i + 4);
  uvarPut32(&buf[i], epoch);    i = (uint16_t)(i + 4);
  uvarPut16(&buf[i], seq);      i = (uint16_t)(i + 2);

  // i vaut ici 19 + len : c'est exactement la portée du CRC.
  uvarPut16(&buf[i], uvarCrc16(buf, i)); i = (uint16_t)(i + 2);
  return (uint8_t)i;   // 21 + len
}

// ============================================================================
//  VUE DÉCODÉE — pointe DANS le tampon reçu (aucune copie de 229 o)
// ============================================================================
struct UniVarVue {
  const uint8_t* valeur;   // pointe sur buf[9], len octets
  uint8_t  id_iot;
  uint16_t id_var;
  uint8_t  type;
  uint8_t  unit;
  uint8_t  len;
  uint32_t t_fct;
  uint32_t epoch;
  uint16_t seq;
};

// ============================================================================
//  DÉCODAGE — l'ordre des contrôles est significatif (cf. §11 du cahier)
// ============================================================================
inline uint8_t uvarDecode(const uint8_t* buf, uint16_t n, UniVarVue& vue)
{
  if (buf == nullptr || n == 0) return UVAR_LEN_KO;

  // 1. Reconnaissance de format — avant tout accès au-delà de buf[1]
  if (n < 2 || buf[0] != UVAR_MAGIC0 || buf[1] != UVAR_MAGIC1) {
    if (uvarEstAncienFormat(buf[0])) return UVAR_ANCIEN_FORMAT;
    return UVAR_MAGIC_KO;
  }

  // 2. Taille minimale pour lire l'en-tête
  if (n < UVAR_SURCOUT + 1) return UVAR_LEN_KO;

  if (buf[2] != UVAR_VER) return UVAR_VER_KO;

  const uint8_t len = buf[8];
  if (len == 0 || len > UVAR_LEN_MAX) return UVAR_LEN_KO;
  if (n != (uint16_t)(UVAR_SURCOUT + len)) return UVAR_LEN_KO;

  const int8_t impose = uvarTailleType(buf[6]);
  if (impose == 0) return UVAR_TYPE_KO;                       // type inconnu
  if (impose > 0 && len != (uint8_t)impose) return UVAR_TYPE_KO;

  const uint16_t portee = UVAR_CRC_PORTEE(len);
  if (uvarGet16(&buf[portee]) != uvarCrc16(buf, portee)) return UVAR_CRC_KO;

  vue.valeur = &buf[UVAR_ENTETE];
  vue.id_iot = buf[3];
  vue.id_var = uvarGet16(&buf[4]);
  vue.type   = buf[6];
  vue.unit   = buf[7];
  vue.len    = len;
  vue.t_fct  = uvarGet32(&buf[9 + len]);
  vue.epoch  = uvarGet32(&buf[13 + len]);
  vue.seq    = uvarGet16(&buf[17 + len]);

  if (vue.id_iot == UVAR_IOT_INDEFINI || vue.id_var == 0) return UVAR_INCONNU;

  return UVAR_OK;
}

// ============================================================================
//  TRAITS — associe un type C++ à son code de trame
// ============================================================================
template<typename T> inline uint8_t uvarTypeDe() { return 0xFF; }   // non supporté
template<> inline uint8_t uvarTypeDe<float>()    { return UVAR_FLOAT32; }
template<> inline uint8_t uvarTypeDe<double>()   { return UVAR_DOUBLE;  }
template<> inline uint8_t uvarTypeDe<int32_t>()  { return UVAR_INT32;   }
template<> inline uint8_t uvarTypeDe<uint32_t>() { return UVAR_UINT32;  }
template<> inline uint8_t uvarTypeDe<bool>()     { return UVAR_BOOL;    }

// ============================================================================
//  SLOT DE RÉCEPTION — 60 o. Rempli par `surveiller()`, jamais par réception.
//  Aucune collision possible : la table est construite par l'application.
//
//  ⚠️ La valeur est stockée en octets BRUTS (val[8]), pas dans une union typée :
//     un membre `double` imposerait un alignement de 8 sur toute la structure
//     (64 o au lieu de 60 o, soit 96 o de plus sur 24 slots) pour un gain nul —
//     toutes les lectures passent déjà par memcpy.
// ============================================================================
struct UniVarSlot {
  uint8_t  id_iot;
  uint16_t id_var;
  uint8_t  type;
  uint8_t  unit;
  uint8_t  len;
  bool     valide;
  uint8_t  val[8];          // valeur brute : float, int32, uint32, double ou bool
  char     texte[UVAR_MAX_STRLEN];
  uint32_t epoch;
  uint32_t t_fct;
  uint32_t rx_ms;
  uint32_t perdus;
  uint16_t seq;
};

// ============================================================================
//  VARIABLE PUBLIÉE
// ============================================================================
struct UniVarPub {
  const void* ptr;
  uint32_t    periode_ms;
  uint32_t    prochaine_ms;
  uint16_t    id_var;
  uint8_t     type;
  uint8_t     unit;
  bool        actif;
};

// ============================================================================
//  COMPTEURS DE DIAGNOSTIC — l'exigence de NON-SILENCE
// ============================================================================
struct UniVarStats {
  // émission
  uint32_t envoyes;
  uint32_t echecs_radio;
  uint32_t plafond_ko;
  // réception
  uint32_t recus;
  uint32_t magic_ko;
  uint32_t ver_ko;
  uint32_t len_ko;
  uint32_t type_ko;
  uint32_t crc_ko;
  uint32_t ancien_format;
  uint32_t non_surveille;
  uint32_t perdus;          // somme des trous de séquence sur les slots surveillés
};

// ============================================================================
//  CLASSE PRINCIPALE
// ============================================================================
class EspNowUniVar {
public:
  EspNowUniVar()
    : _id_iot(UVAR_IOT_INDEFINI), _epoch(0), _t_fct(0),
      _seq(0), _initialise(false), _canal(0),
      _fenetre_debut(0), _fenetre_paquets(0), _heure_externe(false)
  {
    memset(_pubs,  0, sizeof(_pubs));
    memset(_slots, 0, sizeof(_slots));
    memset(&_stats, 0, sizeof(_stats));
    _globale = this;
  }

  // --------------------------------------------------------------------------
  //  ÉMISSION
  // --------------------------------------------------------------------------

  // Initialise la radio et enregistre le peer de diffusion.
  bool begin(uint8_t id_iot, uint8_t canal = 0)
  {
    if (id_iot == UVAR_IOT_INDEFINI) return false;
    _id_iot = id_iot;
    _canal  = canal;
    return _demarrerRadio();
  }

  // Métadonnées du pied de trame. Sans appel, epoch = 0 (inconnu) et
  // t_fct = millis()/1000. Un nœud sans NTP peut donc rester à epoch = 0 :
  // le récepteur se rabat sur rx_ms, qui est toujours juste.
  void setHeure(uint32_t epoch, uint32_t t_fct)
  {
    _epoch = epoch;
    _t_fct = t_fct;
    _heure_externe = true;
  }

  // Déclare une variable à publier périodiquement.
  //   periode_ms = 0  -> envoi manuel uniquement (envoyer())
  template<typename T>
  bool publier(uint16_t id_var, const T* ptr, uint8_t unit, uint32_t periode_ms)
  {
    const uint8_t type = uvarTypeDe<T>();
    if (type == 0xFF || ptr == nullptr) return false;
    return _enregistrerPub(id_var, (const void*)ptr, type, unit, periode_ms);
  }

  // Déclare une chaîne à publier (longueur mesurée à l'envoi).
  bool publierTexte(uint16_t id_var, const char* texte, uint8_t unit, uint32_t periode_ms)
  {
    if (texte == nullptr) return false;
    return _enregistrerPub(id_var, (const void*)texte, UVAR_STRING, unit, periode_ms);
  }

  // Envoi immédiat, hors période (changement d'état, bouton).
  bool envoyer(uint16_t id_var)
  {
    const int i = _trouverPub(id_var);
    if (i < 0) return false;
    return _emettre(_pubs[i]);
  }

  // À appeler dans loop() : n'émet que ce qui est dû, sans jamais dépasser
  // UVAR_MAX_PKT_S paquets par seconde.
  void poll()
  {
    const uint32_t maintenant = millis();
    _majFenetre(maintenant);

    for (int i = 0; i < UVAR_MAX_PUB; i++) {
      UniVarPub& p = _pubs[i];
      if (!p.actif || p.periode_ms == 0) continue;
      if ((int32_t)(maintenant - p.prochaine_ms) < 0) continue;
      if (_fenetre_paquets >= UVAR_MAX_PKT_S) { _stats.plafond_ko++; continue; }

      _emettre(p);
      // Pas de rattrapage : si la boucle a été bloquée, on repart de maintenant.
      p.prochaine_ms = maintenant + p.periode_ms;
    }
  }

  // --------------------------------------------------------------------------
  //  RÉCEPTION
  // --------------------------------------------------------------------------

  // Déclare une variable à écouter. C'EST L'APPLICATION QUI CONSTRUIT LA TABLE :
  // aucune collision de hachage possible, mémoire déterministe, et les paires
  // non surveillées sont comptées au lieu d'écraser un slot.
  bool surveiller(uint8_t id_iot, uint16_t id_var)
  {
    if (id_iot == UVAR_IOT_INDEFINI || id_var == 0) return false;
    if (_trouverSlot(id_iot, id_var) >= 0) return true;

    for (int i = 0; i < UVAR_MAX_SLOTS; i++) {
      if (_slots[i].id_var != 0) continue;
      memset(&_slots[i], 0, sizeof(UniVarSlot));
      _slots[i].id_iot = id_iot;
      _slots[i].id_var = id_var;
      _slots[i].valide = false;
      return true;
    }
    return false;   // table pleine : l'appelant doit augmenter UVAR_MAX_SLOTS
  }

  // Lecture typée. `age_ms` est LA donnée qui manquait pour comparer deux
  // mesures honnêtement (PZEM vs Linky). Renvoie false si jamais reçue ou si
  // le type demandé ne correspond pas à celui de l'émetteur.
  template<typename T>
  bool lire(uint8_t id_iot, uint16_t id_var, T& v, uint32_t* age_ms = nullptr)
  {
    const int i = _trouverSlot(id_iot, id_var);
    if (i < 0 || !_slots[i].valide) return false;
    if (_slots[i].type != uvarTypeDe<T>()) return false;

    const UniVarSlot& s = _slots[i];
    memcpy(&v, s.val, sizeof(T));
    if (age_ms) *age_ms = (uint32_t)(millis() - s.rx_ms);
    return true;
  }

  // Lecture d'une chaîne (toujours terminée par NUL).
  bool lireTexte(uint8_t id_iot, uint16_t id_var, char* dst, size_t taille,
                 uint32_t* age_ms = nullptr)
  {
    if (dst == nullptr || taille == 0) return false;
    const int i = _trouverSlot(id_iot, id_var);
    if (i < 0 || !_slots[i].valide) return false;
    if (_slots[i].type != UVAR_STRING) return false;

    const UniVarSlot& s = _slots[i];
    size_t n = s.len;
    if (n > taille - 1) n = taille - 1;
    if (n > UVAR_MAX_STRLEN - 1) n = UVAR_MAX_STRLEN - 1;
    memcpy(dst, s.texte, n);
    dst[n] = '\0';
    if (age_ms) *age_ms = (uint32_t)(millis() - s.rx_ms);
    return true;
  }

  // Métadonnées d'un slot, sans lire la valeur.
  bool infos(uint8_t id_iot, uint16_t id_var, uint8_t* type, uint8_t* unit,
             uint32_t* age_ms = nullptr, uint32_t* perdus = nullptr)
  {
    const int i = _trouverSlot(id_iot, id_var);
    if (i < 0 || !_slots[i].valide) return false;
    const UniVarSlot& s = _slots[i];
    if (type)   *type   = s.type;
    if (unit)   *unit   = s.unit;
    if (age_ms) *age_ms = (uint32_t)(millis() - s.rx_ms);
    if (perdus) *perdus = s.perdus;
    return true;
  }

  // --------------------------------------------------------------------------
  //  DIAGNOSTIC
  // --------------------------------------------------------------------------
  const UniVarStats& stats() const { return _stats; }
  uint8_t idIot() const { return _id_iot; }

#if defined(UVAR_HOST_TEST)
  // Accès de test : dernière trame réellement émise (aucune radio sur l'hôte).
  const uint8_t* derniereTrame(uint8_t* len) const
  {
    if (len) *len = _derniereLen;
    return _derniereTrame;
  }
#endif

  // Nombre d'octets statiques consommés (RAM) — utile pour arbitrer les tailles.
  static uint32_t empreinteRam()
  {
    return (uint32_t)sizeof(UniVarSlot) * UVAR_MAX_SLOTS +
           (uint32_t)sizeof(UniVarPub)  * UVAR_MAX_PUB;
  }

  // Ré-arme la radio après un reset WiFi (le gestionnaire WiFi peut appeler
  // WiFi.mode(WIFI_OFF), ce qui vide la table des peers et le callback).
  void rearm() { if (_initialise) _demarrerRadio(); }

  // --------------------------------------------------------------------------
  //  APPELÉ PAR LES CALLBACKS RADIO
  // --------------------------------------------------------------------------
  void _traiterTrame(const uint8_t* data, uint16_t len)
  {
    UniVarVue vue;
    const uint8_t st = uvarDecode(data, len, vue);

    switch (st) {
      case UVAR_OK:            break;
      case UVAR_ANCIEN_FORMAT: _stats.ancien_format++; return;
      case UVAR_MAGIC_KO:      _stats.magic_ko++;      return;
      case UVAR_VER_KO:        _stats.ver_ko++;        return;
      case UVAR_LEN_KO:        _stats.len_ko++;        return;
      case UVAR_TYPE_KO:       _stats.type_ko++;       return;
      case UVAR_CRC_KO:        _stats.crc_ko++;        return;
      default:                 _stats.non_surveille++; return;
    }

    const int i = _trouverSlot(vue.id_iot, vue.id_var);
    if (i < 0) { _stats.non_surveille++; return; }

    UniVarSlot& s = _slots[i];

    // Trous de séquence : la seule mesure honnête de la perte par émetteur.
    if (s.valide) {
      const uint16_t attendu = (uint16_t)(s.seq + 1);
      if (vue.seq != attendu) {
        const uint16_t saut = (uint16_t)(vue.seq - attendu);
        s.perdus += saut;
        _stats.perdus += saut;
      }
    }

    s.type   = vue.type;
    s.unit   = vue.unit;
    s.len    = vue.len;
    s.epoch  = vue.epoch;
    s.t_fct  = vue.t_fct;
    s.seq    = vue.seq;
    s.rx_ms  = millis();
    s.valide = true;
    _stats.recus++;

    if (vue.type == UVAR_STRING) {
      uint8_t n = vue.len;
      if (n > UVAR_MAX_STRLEN - 1) n = (uint8_t)(UVAR_MAX_STRLEN - 1);
      memcpy(s.texte, vue.valeur, n);
      s.texte[n] = '\0';
    } else {
      const uint8_t n = (vue.len <= 8) ? vue.len : 8;
      memset(s.val, 0, sizeof(s.val));
      memcpy(s.val, vue.valeur, n);
    }
  }

  // --------------------------------------------------------------------------
  //  INSTANCE COURANTE (les callbacks C sont statiques)
  // --------------------------------------------------------------------------
  static EspNowUniVar* _globale;

private:
  // --------------------------------------------------------------------------
  bool _demarrerRadio()
  {
#if defined(ESP8266) || defined(ESP32)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
#endif

#if defined(ESP8266)
    if (!_initialise) {
      if (esp_now_init() != 0) return false;
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
      esp_now_register_recv_cb(_cbRecv8266);
      esp_now_register_send_cb(_cbSend8266);
      _initialise = true;
    }
    // Idempotent : esp_now_add_peer renvoie une erreur si déjà présent.
    esp_now_add_peer((u8*)_BROADCAST, ESP_NOW_ROLE_COMBO, _canal, nullptr, 0);
    return true;

#elif defined(ESP32)
    if (!_initialise) {
      if (esp_now_init() != ESP_OK) return false;
      #if ESP_IDF_VERSION_MAJOR >= 5
        esp_now_register_recv_cb(_cbRecv32_idf5);
      #else
        esp_now_register_recv_cb(_cbRecv32_idf4);
      #endif
      esp_now_register_send_cb(_cbSend32);
      _initialise = true;
    }
    if (_canal > 0) esp_wifi_set_channel(_canal, WIFI_SECOND_CHAN_NONE);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, _BROADCAST, 6);
    peer.channel = _canal;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
    return true;
#else
    return false;   // hors ESP : le codec reste utilisable (tests hôte)
#endif
  }

  bool _enregistrerPub(uint16_t id_var, const void* ptr, uint8_t type,
                       uint8_t unit, uint32_t periode_ms)
  {
    if (id_var == 0 || ptr == nullptr) return false;
    if (periode_ms > 0 && periode_ms < UVAR_PERIODE_MIN_MS) periode_ms = UVAR_PERIODE_MIN_MS;

    int i = _trouverPub(id_var);
    if (i < 0) {
      for (int k = 0; k < UVAR_MAX_PUB; k++) {
        if (_pubs[k].actif) continue;
        i = k;
        break;
      }
    }
    if (i < 0) return false;

    UniVarPub& p = _pubs[i];
    p.ptr         = ptr;
    p.type        = type;
    p.unit        = unit;
    p.id_var      = id_var;
    p.periode_ms  = periode_ms;
    p.prochaine_ms = millis();   // première émission au prochain poll()
    p.actif       = true;
    return true;
  }

  bool _emettre(const UniVarPub& p)
  {
    uint8_t buf[UVAR_TRAME_MAX];
    uint8_t len;

    if (p.type == UVAR_STRING) {
      const char* s = (const char*)p.ptr;
      size_t n = strnlen(s, UVAR_LEN_MAX);
      if (n == 0) return false;
      len = uvarEncode(buf, _id_iot, p.id_var, p.type, p.unit, s,
                       (uint8_t)n, _tFct(), _epoch, _seq);
    } else {
      const int8_t t = uvarTailleType(p.type);
      if (t <= 0) return false;
      len = uvarEncode(buf, _id_iot, p.id_var, p.type, p.unit, p.ptr,
                       (uint8_t)t, _tFct(), _epoch, _seq);
    }
    if (len == 0) return false;

    const bool ok = _envoyerBrut(buf, len);
    if (ok) { _seq++; _stats.envoyes++; _fenetre_paquets++; }
    else    { _stats.echecs_radio++; }
    return ok;
  }

  bool _envoyerBrut(const uint8_t* data, uint8_t len)
  {
#if defined(UVAR_HOST_TEST)
    // Point d'entrée de test : capture la trame et simule une émission réussie.
    memcpy(_derniereTrame, data, len);
    _derniereLen = len;
    return true;
#elif defined(ESP8266)
    return esp_now_send((u8*)_BROADCAST, (u8*)data, (int)len) == 0;
#elif defined(ESP32)
    return esp_now_send(_BROADCAST, data, (int)len) == ESP_OK;
#else
    (void)data; (void)len;
    return false;
#endif
  }

  uint32_t _tFct() const { return _heure_externe ? _t_fct : (uint32_t)(millis() / 1000UL); }

  void _majFenetre(uint32_t maintenant)
  {
    if ((uint32_t)(maintenant - _fenetre_debut) >= 1000UL) {
      _fenetre_debut = maintenant;
      _fenetre_paquets = 0;
    }
  }

  int _trouverPub(uint16_t id_var) const
  {
    for (int i = 0; i < UVAR_MAX_PUB; i++)
      if (_pubs[i].actif && _pubs[i].id_var == id_var) return i;
    return -1;
  }

  int _trouverSlot(uint8_t id_iot, uint16_t id_var) const
  {
    for (int i = 0; i < UVAR_MAX_SLOTS; i++)
      if (_slots[i].id_var != 0 && _slots[i].id_iot == id_iot && _slots[i].id_var == id_var)
        return i;
    return -1;
  }

  // --------------------------------------------------------------------------
  //  CALLBACKS RADIO (statiques : signatures imposées par le SDK)
  // --------------------------------------------------------------------------
#if defined(ESP8266)
  static void _cbRecv8266(u8* mac, u8* data, u8 len)
  {
    (void)mac;
    if (_globale && data && len) _globale->_traiterTrame((const uint8_t*)data, (uint16_t)len);
  }
  static void _cbSend8266(u8* mac, u8 status)
  {
    (void)mac;
    if (_globale && status != 0) _globale->_stats.echecs_radio++;
  }
#elif defined(ESP32)
  #if ESP_IDF_VERSION_MAJOR >= 5
  static void _cbRecv32_idf5(const esp_now_recv_info_t* info, const uint8_t* data, int len)
  {
    (void)info;
    if (_globale && data && len > 0) _globale->_traiterTrame(data, (uint16_t)len);
  }
  #else
  static void _cbRecv32_idf4(const uint8_t* mac, const uint8_t* data, int len)
  {
    (void)mac;
    if (_globale && data && len > 0) _globale->_traiterTrame(data, (uint16_t)len);
  }
  #endif
  // ⚠️ La signature du callback d'ENVOI a changé deux fois côté ESP-IDF :
  //      IDF 4.x          : (const uint8_t* mac, esp_now_send_status_t)
  //      IDF 5.0 – 5.3    : idem
  //      IDF 5.4+         : (const esp_now_send_info_t* tx_info, esp_now_send_status_t)
  //    On sélectionne la bonne forme à la compilation. Le compteur n'est
  //    qu'un diagnostic : en cas de doute, mieux vaut ne rien compter que
  //    casser le build.
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
  static void _cbSend32(const esp_now_send_info_t* tx_info, esp_now_send_status_t status)
  {
    (void)tx_info;
    if (_globale && status != ESP_NOW_SEND_SUCCESS) _globale->_stats.echecs_radio++;
  }
  #else
  static void _cbSend32(const uint8_t* mac, esp_now_send_status_t status)
  {
    (void)mac;
    if (_globale && status != ESP_NOW_SEND_SUCCESS) _globale->_stats.echecs_radio++;
  }
  #endif
#endif
  // --------------------------------------------------------------------------
  uint8_t  _id_iot;
  uint32_t _epoch;
  uint32_t _t_fct;
  uint16_t _seq;
  bool     _initialise;
  uint8_t  _canal;
  uint32_t _fenetre_debut;
  uint16_t _fenetre_paquets;
  bool     _heure_externe;

  UniVarPub   _pubs[UVAR_MAX_PUB];
  UniVarSlot  _slots[UVAR_MAX_SLOTS];
  UniVarStats _stats;

#if defined(UVAR_HOST_TEST)
  uint8_t _derniereTrame[UVAR_TRAME_MAX];
  uint8_t _derniereLen = 0;
#endif

  static const uint8_t _BROADCAST[6];
};

inline const uint8_t EspNowUniVar::_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
inline EspNowUniVar* EspNowUniVar::_globale = nullptr;

#endif // ESPNOW_UNIVAR_H
