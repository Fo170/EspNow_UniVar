# EspNow_UniVar — ESP-NOW mono-variable

> **Un paquet = une variable.** La trame est auto-descriptive : le récepteur la
> décode sans partager le `struct` de l'émetteur. Ajouter une variable ne touche
> que son émetteur et les récepteurs qui l'écoutent.

[![GitHub release](https://img.shields.io/github/v/release/Fo170/EspNow_UniVar)](https://github.com/Fo170/EspNow_UniVar/releases)

---

## Le problème que ce format résout

Le parc utilisait deux formats **incompatibles**, tous deux monolithiques :

| Format | Taille | Porté par |
|---|---|---|
| `EspNowEchange` (`ESP_NOW_ECHANGE.h`) | 40 o | Ordonnanceur |
| `struct_message_iOT_espnow` (`espnow_message.h`) | ~90 o | PZEM, radiateurs… |

Chaque nœud diffusait **son** struct ; tout récepteur qui ne connaissait pas
exactement le même `struct_id` jetait les trames **silencieusement**. Mesure
relevée sur l'Ordonnanceur : **48 % de trames perdues** (`jete = 2429 / 5030`),
sans qu'aucun compteur ne le signale.

Conséquences du format monolithique :

- ajouter une variable ⇒ **reflasher tout le parc** (la taille du `struct` est un contrat global) ;
- **aucun horodatage** ⇒ impossible de distinguer une mesure fraîche d'une valeur figée ;
- **aucune séquence** ⇒ impossible de distinguer « émetteur muet » de « trames perdues ».

---

## La trame

**En-tête 9 o + valeur `len` + pied 12 o = `21 + len` octets.**

| Offset | Taille | Champ | Contenu |
|---|---|---|---|
| 0 | 2 | `magic` | `0x46 0x56` (`'F','V'`) |
| 2 | 1 | `ver` | version du format — **1** |
| 3 | 1 | `id_iot` | émetteur (0 = non défini, trame refusée) |
| 4 | 2 | `id_var` | variable, little-endian |
| 6 | 1 | `type` | voir ci-dessous |
| 7 | 1 | `unit` | code `Unity_Dictionary` ; `255` = `_NAN_` |
| 8 | 1 | `len` | longueur de la valeur (1 → **229**) |
| 9 | `len` | `valeur` | interprétée selon `type` |
| `9+len` | 4 | `t_fct` | uptime de l'émetteur (s) |
| `13+len` | 4 | `epoch` | heure **UTC** de la mesure (s) ; `0` = inconnu |
| `17+len` | 2 | `seq` | numéro de séquence |
| `19+len` | 2 | `crc` | CRC-16/CCITT-FALSE sur les octets `0 → 18+len` |

| Cas | `len` | Trame |
|---|---|---|
| `bool` | 1 | 22 o |
| `float32` / `int32` / `uint32` | 4 | **25 o** |
| `double` | 8 | 29 o |
| chaîne de 32 caractères | 32 | 53 o |

Plafond ESP-NOW : 250 o ⇒ `len ≤ 229`.

> ⚠️ La valeur commence à l'offset 9 : elle est **toujours lue par `memcpy`**,
> jamais par un cast de pointeur (alignement de `double` non garanti).

### Types

| Valeur | Type | `len` imposé |
|---|---|---|
| 0 | `float32` | 4 |
| 1 | `int32` | 4 |
| 2 | `uint32` | 4 |
| 3 | `bool` | 1 |
| 4 | `double` | 8 |
| 5 | `string` | 1 → 229 (sans NUL terminal) |

`uint32` est **obligatoire** pour les compteurs : l'index Linky vaut
`23 296 626 Wh` ; en `float32`, la mantisse de 24 bits plafonne à `16 777 216`
et les unités sont perdues.

### Pas de `t_local`

L'heure locale est **dérivée** de `epoch` et de l'offset du récepteur. La
transporter coûterait 4 octets par trame pour une information redondante sur un
parc entièrement métropolitain.

---

## Installation

```ini
lib_deps =
    https://github.com/Fo170/EspNow_UniVar.git@^1.1.0
    https://github.com/Fo170/Unity_Dictionary.git@^1.2.0
```

`Unity_Dictionary` est une dépendance **volontaire** : elle garantit qu'émetteur
et récepteur parlent le même dictionnaire d'unités — c'est exactement la leçon
des deux `struct` incompatibles.

---

## Émission

```cpp
#include <EspNow_UniVar.h>

EspNowUniVar env;
float    pzem_s  = 0.0f;    // VA
uint32_t pzem_wh = 0;       // Wh

void setup() {
  env.begin(UVAR_IOT_PZEM004T);

  env.publier(UVAR_PZEM_S_APPARENTE, &pzem_s,  _Unit_VA_,   1000);
  env.publier(UVAR_PZEM_WH_INDEX,    &pzem_wh, _Unit_Wh_,  60000);
  env.publierTexte(UVAR_NOM, "PZEM-004T", _Unit_Aucune_, 300000);
}

void loop() {
  env.poll();          // n'émet que ce qui est dû
}
```

| Membre | Rôle |
|---|---|
| `begin(id_iot, canal = 0)` | init radio, peer de diffusion |
| `setHeure(epoch, t_fct)` | métadonnées du pied ; sans appel, `epoch = 0` et `t_fct = millis()/1000` |
| `publier(id_var, &valeur, unité, periode_ms)` | déclare une variable ; `periode_ms = 0` ⇒ envoi manuel |
| `publierTexte(id_var, "texte", unité, periode_ms)` | idem pour une chaîne |
| `envoyer(id_var)` | envoi immédiat, hors période |
| `poll()` | émet les variables dues, plafonné à `UVAR_MAX_PKT_S` paquets/s |

La lib lit le pointeur **au moment de l'envoi** : l'application met simplement à
jour sa variable, elle n'appelle jamais la lib pour publier.

---

## Réception

```cpp
env.surveiller(UVAR_IOT_PZEM004T,  UVAR_PZEM_S_APPARENTE);
env.surveiller(UVAR_IOT_LINKYSTIC, UVAR_LINKY_P_APP);

float p; uint32_t age;
if (env.lire(UVAR_IOT_PZEM004T, UVAR_PZEM_S_APPARENTE, p, &age) && age < 3000) {
    // mesure fraîche de moins de 3 s
}
```

**La table de slots est construite par l'application**, pas par la réception :

- **aucune collision possible** (un index `f(id_iot, id_var)` en accès direct
  écraserait silencieusement une variable par une autre) ;
- **mémoire déterministe** ;
- les paires **non surveillées sont comptées** (`non_surveille`) → on saura
  quelles variables circulent sans être écoutées.

| Membre | Rôle |
|---|---|
| `surveiller(id_iot, id_var)` | réserve un slot ; `false` si la table est pleine |
| `lire(id_iot, id_var, T&, &age_ms)` | `false` si jamais reçue **ou si le type ne correspond pas** |
| `lireTexte(id_iot, id_var, buf, taille, &age_ms)` | chaîne toujours terminée par NUL |
| `infos(id_iot, id_var, &type, &unité, &age_ms, &perdus)` | métadonnées sans lire la valeur |

**Pas de file d'attente** : la valeur la plus récente gagne. 10 nœuds × 5
variables × 1 Hz = 50 paquets/s ; une file de 8 déborde en 160 ms — une table de
slots ne déborde jamais.

---

## Diagnostic — l'exigence de non-silence

```cpp
const UniVarStats& s = env.stats();
// envoyes, echecs_radio, plafond_ko
// recus, magic_ko, ver_ko, len_ko, type_ko, crc_ko, ancien_format,
// non_surveille, perdus
```

`perdus` est calculé par slot à partir des trous de `seq` : c'est la seule
mesure honnête de la perte par émetteur. Sans elle, `age_ms` seul ne distingue
pas « l'émetteur s'est tu » de « la moitié des trames est perdue ».

---

## Empreinte

Configuration par défaut (`UVAR_MAX_SLOTS 24`, `UVAR_MAX_PUB 12`, `UVAR_MAX_STRLEN 24`) :

| Élément | ESP8266 / ESP32 |
|---|---|
| `sizeof(UniVarSlot)` | 60 o × 24 = 1 440 o |
| `sizeof(UniVarPub)` | 20 o × 12 = 240 o |
| `sizeof(EspNowUniVar)` | **1 756 o** |

Aucune allocation dynamique. Pour réduire :

| Levier | Gain |
|---|---|
| `UVAR_MAX_SLOTS 24 → 16` | −480 o |
| `UVAR_MAX_STRLEN 24 → 16` | −192 o |
| `UVAR_MAX_PUB 12 → 8` | −80 o |

---

## Modèle d'échange — un catalogue, des abonnements

**Chaque iOT diffuse tout ce qu'il mesure, en broadcast, à tout le monde.**
**Chaque iOT s'abonne à ce qui l'intéresse et ignore le reste.**

Il n'y a donc pas de « qui parle à qui » : il y a un **catalogue** de variables
et des **abonnements**. Ajouter un appareil ne modifie le firmware d'aucun autre.

```
Émetteur ──► broadcast FF:FF:FF:FF:FF:FF ──► tous les nœuds
                                              │
              chaque nœud fait surveiller(id_iot, id_var) pour ce qu'il veut
```

---

## Contrat des identifiants — `EspNow_UniVar_Vars.h`

| Plage `id_var` | Domaine |
|---|---|
| 0 | réservé |
| 1 – 99 | énergie et comptage |
| 100 – 199 | **mesures** d'environnement (T, RH, P, sol, lux, alim) |
| 200 – 249 | **commandes** — du maître vers les appareils |
| 250 – 299 | **états** — des appareils vers tout le monde |
| 300 – 399 | identité et diagnostic |

La séparation **commandes / états** rend le sens de circulation explicite.

**APPEND-ONLY.** Un numéro attribué n'est jamais renuméroté ni réutilisé : un
récepteur non reflashé lirait une valeur sous un autre sens. Une variable
retirée laisse son numéro **brûlé**.

### L'unité est fixée par le contrat

Deux nœuds qui publient la même grandeur publient la **même** unité. Un récepteur
peut se fier au contrat — et vérifier `unit` à la réception au lieu de le supposer.

**Unité de base par défaut** (températures, tensions, résistances, durées, %) :
`_Unit_Centigrade_`, `_Unit_Volt_`, `_Unit_Ohms_`, `_Unit_Wh_`… Le préfixe
(k, M, m) relève de l'affichage, donc de `Unity`.

**Dérogation assumée — énergie du PZEM** : `UVAR_PZEM_S_APPARENTE` en **kVA**,
`UVAR_PZEM_Q_REACTIVE` en **kVAR**, `UVAR_PZEM_WH_INDEX` en **kWh**, parce que
les capteurs d'énergie Domoticz sont configurés sur ces magnitudes précises.
Convertir à la frontière HTTP ajouterait une source d'erreur sans bénéfice.

⚠️ Ces trois variables sont **les seules** concernées. `UVAR_LINKY_P_APP` est en
**VA**, `UVAR_LINKY_WH_TOTAL` en **Wh `uint32`**, `UVAR_SOL_RESISTANCE` en **Ω**.

### Commandes chauffage (200 – 209)

Le modèle de `commande_rad` (`0_INCLUDE_/Arduino/Radiateur.h`) est repris tel quel :

| `id_var` | Contenu |
|---|---|
| 200 `UVAR_RAD_ORDRE_ZONES` | mot **8 bits** historique, bit 0 = zone 1 … bit 7 = zone 8 |
| 201-208 `UVAR_RAD_CONSIGNE_Z1..Z8` | consigne **moyenne** visée par zone (°C) |
| 209 `UVAR_RAD_MODE_ZONES` | 2 bits/zone : 0 Arrêt · 1 Hors gel · 2 Éco · 3 Confort |

Le fil pilote n'a que **deux** positions (Confort 19 °C / Éco 17 °C). L'Ordonnanceur
module le **rapport cyclique** entre les deux : l'inertie du radiateur lisse et la
température moyenne se place entre 17 et 19 °C. Le thermostat interne du radiateur
ferme la boucle — aucun asservissement de température côté maître.

---

## Coexistence et migration

Le décodeur reconnaît trois cas, dans cet ordre :

1. `data[0..1] == 'F','V'` → trame mono-variable ;
2. `data[0] ∈ {0x00..0x0B, 0xFF}` → ancienne trame `EspNowSharedVariable`
   (comptée `ancien_format`, à déléguer ou ignorer) ;
3. sinon → `magic_ko`.

Les deux formats sont non ambigus : `0x46` (70) n'est pas un `type_id` valide de
l'ancienne lib.

| Étape | Action | Reflash |
|---|---|---|
| 1 | Spec + lib + tests hôte | **aucun** |
| 2 | Ordonnanceur : récepteur bi-format + émetteur | 1 carte |
| 3 | PZEM : émetteur | 1 carte |
| 4 | LinkysTIC + Surveillance Batterie | 2 cartes |
| 5 | Retrait de `espNow_SharedVar`, `ESP_NOW_ECHANGE.h`, `espnow_message.h` | — |

Chaque étape est indépendante : une carte migrée continue de comprendre les autres.

---

## Recette hôte

Aucune carte, aucun flash — la logique de trame est testée sur l'ordinateur :

```bash
./test/host/run.sh
```

12 exigences, 158 vérifications : aller-retour de chaque type, CRC (1 bit
corrompu rejeté), incohérence `len`/`type`, refus de `len > 229`, `magic`/`ver`,
paire non surveillée, `age_ms` exact (dont débordement de `millis()`), trous de
séquence, plafond d'émission, périodes indépendantes, chaînes sans NUL, trame de
250 o.

---

## Licence

GPL-3.0-only — Olivier FOURNET
