// ============================================================================
//  EspNow_UniVar_Vars.h — CONTRAT des identifiants ESP-NOW (append-only)
//
//  MODÈLE D'ÉCHANGE — uniforme
//  ────────────────────────────────────────────────────────────────────────────
//  Chaque iOT diffuse TOUT ce qu'il mesure, en broadcast, à tout le monde.
//  Chaque iOT s'abonne (`surveiller()`) à CE QUI L'INTÉRESSE et ignore le reste.
//
//  Il n'y a donc plus de « qui parle à qui » : il y a un catalogue de variables
//  et des abonnements. Ajouter un appareil ne modifie aucun autre firmware.
//
//  ⚠️ RÈGLES ABSOLUES — toute violation casse le parc en silence :
//
//    1. APPEND-ONLY : un numéro attribué n'est JAMAIS renuméroté ni réutilisé.
//       Un numéro retiré est BRÛLÉ (laisser la ligne commentée, ne pas recycler).
//       Raison : un récepteur non reflashé lirait une valeur sous un autre sens.
//    2. Un numéro = une grandeur physique. Pas de « fourre-tout ».
//    3. Ce fichier est la SEULE source des identifiants : aucun projet ne doit
//       redéfinir un VAR_* localement.
//    4. L'UNITÉ est fixée PAR LE CONTRAT, pas par l'émetteur. Deux nœuds qui
//       publient la même grandeur publient la même unité. Un récepteur peut donc
//       se fier au contrat, et vérifier `unit` à la réception.
//
//  Auteur  : FOURNET Olivier (GPL-3.0)
//  Version : 1.1.0
// ============================================================================
#ifndef ESPNOW_UNIVAR_VARS_H
#define ESPNOW_UNIVAR_VARS_H

#include <stdint.h>

// ============================================================================
//  IDENTIFIANTS DES NŒUDS (id_iot, 1 octet)
//  Source de vérité : Identite_iOT.h du parc (__ID_iOT__*). 0 = non défini.
// ============================================================================
#define UVAR_IOT_INDEFINI                     0
#define UVAR_IOT_ESP32_TELEGRAM               1
#define UVAR_IOT_PZEM004T                     2
#define UVAR_IOT_LINKYSTIC                    3
#define UVAR_IOT_SURVEILLANCE_BATTERIE        4
#define UVAR_IOT_RADIATEUR_SALLE_A_MANGER     5
#define UVAR_IOT_RADIATEUR_ATELIER            6
#define UVAR_IOT_RADIATEUR_CHAMBRE_HAUT       7
#define UVAR_IOT_COULOMETRE                   8
#define UVAR_IOT_LUXMETRE                     9
#define UVAR_IOT_STATION_METEO               10
#define UVAR_IOT_DOIT_AM                     11
#define UVAR_IOT_ORDONNANCEUR                12
#define UVAR_IOT_BME280_GARAGE               13
#define UVAR_IOT_DS18X20_EXT                 14
#define UVAR_IOT_DATALOG                     15
#define UVAR_IOT_RADIATEUR_CHAMBRE_BAS       16
#define UVAR_IOT_RADIATEUR_SALLE_DE_BAIN     17
#define UVAR_IOT_SERVEUR_NTP                 18
#define UVAR_IOT_PWM                         19
#define UVAR_IOT_RENDEMENT                   20

// ============================================================================
//  PLAGES D'ID_VAR
// ============================================================================
//     0        réservé / invalide (jamais émis)
//     1  -  99 énergie et comptage        (PZEM, Linky, batterie)
//   100  - 199 MESURES d'environnement    (température, humidité, sol, lumière)
//   200  - 249 COMMANDES                  (maître -> appareils)
//   250  - 299 ÉTATS                      (appareils -> tout le monde)
//   300  - 399 identité et diagnostic     (nom, version, uptime, erreurs)
//   400  - 65535 réservé
#define UVAR_PLAGE_ENERGIE_DEBUT        1
#define UVAR_PLAGE_ENERGIE_FIN         99
#define UVAR_PLAGE_MESURES_DEBUT      100
#define UVAR_PLAGE_MESURES_FIN        199
#define UVAR_PLAGE_COMMANDES_DEBUT    200
#define UVAR_PLAGE_COMMANDES_FIN      249
#define UVAR_PLAGE_ETATS_DEBUT        250
#define UVAR_PLAGE_ETATS_FIN          299
#define UVAR_PLAGE_DIAG_DEBUT         300
#define UVAR_PLAGE_DIAG_FIN           399

// ============================================================================
//  ÉNERGIE ET COMPTAGE (1 - 99)
// ============================================================================
//  ⚠️ DÉROGATION ASSUMÉE à la règle « unité de base » : les grandeurs d'énergie
//     et de puissance apparente du PZEM partent AVEC leur magnitude (kWh, kVA,
//     kVAR), parce que les capteurs Domoticz correspondants sont configurés sur
//     ces magnitudes précises. Une conversion à la frontière HTTP serait une
//     source d'erreur de plus, sans bénéfice.
//
//     Conséquence : `_Unit_kVA_` et `_Unit_kVAR_` ont été ajoutés au dictionnaire.
//     Les autres grandeurs (températures, résistances, tensions) restent en unité
//     de BASE, sans préfixe.
// ============================================================================

// --- PZEM-004T (id_iot = 2) -------------------------------------------------
#define UVAR_PZEM_U_TENSION         1   // V     f32  tension RMS
#define UVAR_PZEM_I_COURANT         2   // A     f32  courant RMS
#define UVAR_PZEM_P_ACTIVE          3   // W     f32  puissance active (P)
#define UVAR_PZEM_S_APPARENTE       4   // kVA   f32  puissance apparente (S) — magnitude Domoticz
#define UVAR_PZEM_Q_REACTIVE        5   // kVAR  f32  puissance réactive (Q) — magnitude Domoticz
#define UVAR_PZEM_PF                6   // PF    f32  facteur de puissance
#define UVAR_PZEM_FREQUENCE         7   // Hz    f32
#define UVAR_PZEM_WH_INDEX          8   // kWh   f32  index d'énergie natif du PZEM — magnitude Domoticz
#define UVAR_PZEM_WH_LINKY          9   // Wh    u32  impulsions Linky comptées par le PZEM

// --- Linky TIC (id_iot = 3) -------------------------------------------------
#define UVAR_LINKY_I_INST          10   // A     f32  intensité instantanée
#define UVAR_LINKY_P_APP           11   // VA    f32  puissance apparente (base)
#define UVAR_LINKY_WH_TOTAL        12   // Wh    u32  index total
#define UVAR_LINKY_WH_HEURE        13   // Wh    u32  énergie de l'heure passée
#define UVAR_LINKY_WH_JOUR         14   // Wh    u32  énergie du jour

// --- Batterie / coulombmètre (id_iot = 4 ou 8) ------------------------------
#define UVAR_BATT_TENSION          20   // V     f32
#define UVAR_BATT_COURANT          21   // A     f32  négatif = décharge
#define UVAR_BATT_AH_DISPO         22   // Ah    f32
#define UVAR_BATT_SOC              23   // %     f32  état de charge
#define UVAR_BATT_DOD              24   // %     f32  profondeur de décharge

// ============================================================================
//  MESURES D'ENVIRONNEMENT (100 - 199)
// ============================================================================

// --- Températures (°C) ------------------------------------------------------
#define UVAR_T_SALON               100  // °C   f32
#define UVAR_T_ATELIER             101  // °C   f32
#define UVAR_T_CHAMBRE_HAUT        102  // °C   f32
#define UVAR_T_CHAMBRE_BAS         103  // °C   f32
#define UVAR_T_GARAGE              104  // °C   f32
#define UVAR_T_EXT                 105  // °C   f32
#define UVAR_T_CAVE                106  // °C   f32
#define UVAR_T_VERANDA             107  // °C   f32
#define UVAR_T_ESCALIER            108  // °C   f32
#define UVAR_T_GRENIER             109  // °C   f32
#define UVAR_T_SALLE_DE_BAIN       110  // °C   f32

// --- Humidités relatives (%) ------------------------------------------------
#define UVAR_RH_SALON              120  // %    f32
#define UVAR_RH_ATELIER            121  // %    f32
#define UVAR_RH_CHAMBRE_HAUT       122  // %    f32
#define UVAR_RH_GARAGE             123  // %    f32
#define UVAR_RH_EXT                124  // %    f32

// --- Pressions (hPa) --------------------------------------------------------
#define UVAR_P_GARAGE              130  // hPa  f32
#define UVAR_P_EXT                 131  // hPa  f32

// --- Capteur de sol résistif ------------------------------------------------
#define UVAR_SOL_TENSION           140  // V    f32  tension brute lue sur A0
#define UVAR_SOL_RESISTANCE        141  // Ohms f32  résistance calculée (base, pas de kΩ)
#define UVAR_SOL_HUMIDITE          142  // %    f32  humidité dérivée

// --- Lumière et alimentation ------------------------------------------------
#define UVAR_LUX                   150  // Lux  f32
#define UVAR_ALIM_TENSION          151  // V    f32  tension d'alimentation de l'iOT

// ============================================================================
//  COMMANDES (200 - 249) — du MAÎTRE vers les appareils
// ============================================================================
//  Le maître (Ordonnanceur) décide, les appareils appliquent. Une commande est
//  diffusée à tous : chaque appareil ne lit que la sienne.

// --- Chauffage : les 8 zones -------------------------------------------------
//  Zone 1 Salle à manger | 2 Escalier | 3 Atelier | 4 Chambre
//  Zone 5 Chambre du bas | 6 Salle de bain | 7 Grenier | 8 Chauffe-eau
//  (mapping historique de `commande_rad` dans 0_INCLUDE_/Arduino/Radiateur.h)
//
//  ⚠️ Mot de 8 bits historique (unsigned char `etat_generale`) : conservé tel
//     quel, bit 0 = zone 1 … bit 7 = zone 8. 1 = marche demandée.
#define UVAR_RAD_ORDRE_ZONES       200  // —    u32  bitfield 8 zones (bits 0-7)
#define UVAR_RAD_CONSIGNE_Z1       201  // °C   f32  consigne zone 1
#define UVAR_RAD_CONSIGNE_Z2       202  // °C   f32
#define UVAR_RAD_CONSIGNE_Z3       203  // °C   f32
#define UVAR_RAD_CONSIGNE_Z4       204  // °C   f32
#define UVAR_RAD_CONSIGNE_Z5       205  // °C   f32
#define UVAR_RAD_CONSIGNE_Z6       206  // °C   f32
#define UVAR_RAD_CONSIGNE_Z7       207  // °C   f32
#define UVAR_RAD_CONSIGNE_Z8       208  // °C   f32  consigne zone 8 (chauffe-eau)
// Mode fil pilote par zone, 2 bits par zone (bits 0-1 = zone 1 … 14-15 = zone 8)
//   0 = Arrêt · 1 = Hors gel · 2 = Éco · 3 = Confort
#define UVAR_RAD_MODE_ZONES        209  // —    u32  bitfield 8 × 2 bits

// --- Chauffe-eau ------------------------------------------------------------
#define UVAR_CHF_EAU_ORDRE         220  // —    bool marche demandée

// ============================================================================
//  ÉTATS (250 - 299) — des appareils vers tout le monde
// ============================================================================

// --- Radiateur --------------------------------------------------------------
#define UVAR_RAD_ETAT              250  // —    bool  relais fermé (chauffe en cours)
#define UVAR_RAD_T_MESURE          251  // °C   f32   température mesurée dans la pièce
#define UVAR_RAD_T_MARCHE          252  // s    u32   temps de marche cumulé
#define UVAR_RAD_PUISSANCE_NOM     253  // W    f32   puissance nominale de l'appareil
#define UVAR_RAD_DIFF_DEG_H        254  // °C/h f32   dérivée de la température

// --- Chauffe-eau ------------------------------------------------------------
#define UVAR_CHF_EAU_ETAT          260  // —    bool  chauffe en cours
#define UVAR_CHF_EAU_W             261  // W    f32   puissance instantanée
#define UVAR_CHF_EAU_WH            262  // Wh   u32   index d'énergie

// ============================================================================
//  IDENTITÉ ET DIAGNOSTIC (300 - 399)
// ============================================================================
#define UVAR_NOM                   300  // —    string nom réseau de l'appareil
#define UVAR_VERSION               301  // —    string version applicative
#define UVAR_T_FCT                 302  // s    u32    uptime de l'émetteur
#define UVAR_RSSI                  303  // dBm  i32    force du signal
#define UVAR_ERREUR                304  // —    u32    bitfield d'erreurs capteur
#define UVAR_HEURE_LOCALE          305  // h    f32    heure locale décimale
#define UVAR_NB_CYCLES             306  // —    u32    compteur de boucles

#endif // ESPNOW_UNIVAR_VARS_H
