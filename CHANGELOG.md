# Changelog — EspNow_UniVar

Toutes les dates sont au format AAAA-MM-JJ.
Le versionnage suit [SemVer](https://semver.org/lang/fr/).

## [2.0.0] - 2026-11-13

### 💥 Rupture — la librairie devient agnostique

`src/EspNow_UniVar_Vars.h` est **retiré de la librairie**. Il contenait les
identifiants d'un parc précis (PZEM, Linky, radiateurs, zones de chauffage) :
ce sont des spécificités d'application, pas de la mécanique de transport.

La librairie ne connaît plus que les **règles du protocole** :

| Constante | Rôle |
|---|---|
| `UVAR_ID_IOT_INVALIDE` (0) | `id_iot` non identifié → trame refusée |
| `UVAR_ID_VAR_INVALIDE` (0) | `id_var` non déclarée → trame refusée |

Tout le reste — valeurs de `id_iot`, valeurs de `id_var`, unité par variable —
appartient à l'application, dans **son** fichier de contrat.

### ✏️ Modifié

- `examples/Emetteur` et `examples/Recepteur` définissent leur propre bloc de
  contrat, commenté, avec les trois règles à respecter (0 réservé, append-only,
  une unité par variable).
- `test/host/test_uni_var.cpp` idem : plus aucune référence à un parc réel.

### Migration depuis 1.1.x

```diff
- #include <EspNow_UniVar.h>        // fournissait aussi les identifiants
+ #include <EspNow_UniVar.h>
+ #include "mon_contrat.h"           // vos id_iot / id_var, chez vous
```

Si vous utilisiez `UVAR_IOT_INDEFINI`, remplacez-le par `UVAR_ID_IOT_INVALIDE`
(même valeur, 0, mais désormais définie par le protocole et non par un parc).

### Non fonctionnel

Inchangé : trame `21 + len`, CRC-16/CCITT-FALSE, zéro allocation dynamique,
ESP8266 + ESP32 (IDF 4.x et 5.x). Recette : 158 vérifications, 0 échec.

---

## [1.1.1] - 2026-11-13

### 🐛 Corrigé

- `library.json` / `library.properties` ne déclaraient que `Unity_Dictionary`,
  alors que l'exemple `Recepteur` utilise `<Unity.h>` pour l'affichage SI.
  Un projet qui n'ajoutait que `EspNow_UniVar` en `lib_deps` échouait sur
  `Unity.h: No such file or directory`. `Unity` est désormais une dépendance
  déclarée.

Détecté par le test de publication : `pio run` sur un projet vierge ne déclarant
que `https://github.com/Fo170/EspNow_UniVar.git@^1.1.0`.

### Précision sur les dépendances

| Dépendance | Rôle |
|---|---|
| `Unity_Dictionary` | **code** : les codes `_Unit_*` et `Str_Unit()` sont utilisés par la librairie |
| `Unity` | **affichage** : préfixes SI (`C_UNITY::valeurAvecUnite`), utilisé par les exemples |

---

## [1.1.0] - 2026-11-13

### 🔄 Contrat revu — modèle d'échange uniforme

**Principe** : chaque iOT diffuse **tout ce qu'il mesure**, en broadcast, à tout
le monde ; chaque iOT **s'abonne** (`surveiller()`) à ce qui l'intéresse.

Le contrat `EspNow_UniVar_Vars.h` est réorganisé en cinq plages, avec une
séparation explicite du sens de circulation :

| Plage | Rôle |
|---|---|
| 1 – 99 | énergie et comptage |
| 100 – 199 | **mesures** d'environnement |
| 200 – 249 | **commandes** — du maître vers les appareils |
| 250 – 299 | **états** — des appareils vers tout le monde |
| 300 – 399 | identité et diagnostic |

### ✨ Ajouté

- Bloc **commandes chauffage** : `UVAR_RAD_ORDRE_ZONES` (mot 8 bits historique de
  `commande_rad`), `UVAR_RAD_CONSIGNE_Z1..Z8`, `UVAR_RAD_MODE_ZONES`
  (2 bits/zone : Arrêt · Hors gel · Éco · Confort).
- Bloc **états radiateur** : `UVAR_RAD_ETAT`, `UVAR_RAD_T_MESURE`,
  `UVAR_RAD_T_MARCHE`, `UVAR_RAD_PUISSANCE_NOM`, `UVAR_RAD_DIFF_DEG_H`.
- Bloc **chauffe-eau** : `UVAR_CHF_EAU_ORDRE` / `_ETAT` / `_W` / `_WH`.
- Mesures manquantes : `UVAR_T_ESCALIER`, `_GRENIER`, `_SALLE_DE_BAIN`,
  `UVAR_RH_EXT`, `UVAR_ALIM_TENSION`.
- `UVAR_PZEM_Q_REACTIVE` (kVAR) — la puissance réactive n'était pas exposée.

### 🔧 Modifié — unités

**L'unité est désormais fixée par le contrat**, pas par l'émetteur : deux nœuds
qui publient la même grandeur publient la même unité.

**Dérogation assumée** : les grandeurs d'énergie du PZEM partent **avec leur
magnitude** (`kVA`, `kVAR`, `kWh`) parce que les capteurs Domoticz correspondants
sont configurés sur ces magnitudes précises. Cela nécessite `Unity_Dictionary`
**≥ 1.2.0** (`_Unit_kVA_` 43, `_Unit_kVAR_` 44).

Partout ailleurs l'unité de **base** s'applique : `UVAR_LINKY_P_APP` en **VA**,
`UVAR_LINKY_WH_TOTAL` en **Wh `uint32`**, `UVAR_SOL_RESISTANCE` en **Ω**.

### 📌 Conséquence sur les projets

Les `id_var` du catalogue ont été **renumérotés avant tout déploiement** (rien
n'était encore flashé) pour laisser la place aux blocs commandes/états.
Aucun appareil en service n'est concerné.

---

## [1.0.0] - 2026-11-13

### ✨ Création

Nouveau format de trame ESP-NOW **mono-variable**, destiné à remplacer les deux
structs monolithiques du parc :

- `ESP_NOW_ECHANGE.h` → `EspNowEchange` (40 o, `struct_id 0xE51F`)
- `espnow_message.h` → `struct_message_iOT_espnow` (~90 o)

Ces deux formats se rejetaient mutuellement (`_findVarByName() < 0`), ce qui
produisait **48 % de trames perdues** sur l'Ordonnanceur sans aucun signalement.

### Format

- En-tête auto-descriptif : `magic 'F','V'`, `ver`, `id_iot`, `id_var`, `type`,
  `unit`, `len`.
- Pied de trame : `t_fct`, `epoch` (UTC), `seq`, `crc` (CRC-16/CCITT-FALSE).
- Taille `21 + len` octets : 25 o pour un `float32`, 29 o pour un `double`,
  250 o au maximum.
- `len` permet `float32`, `int32`, `uint32`, `bool`, `double` et chaîne dans le
  même format.
- Pas de `t_local` : l'heure locale se dérive de `epoch` et de l'offset du
  récepteur (4 octets économisés par trame).

### Émission

- `publier()` / `publierTexte()` : une **période par variable**.
- `poll()` non bloquant, plafonné par `UVAR_MAX_PKT_S` (airtime).
- `envoyer()` pour un envoi immédiat hors période.
- Métadonnées remplies automatiquement ; `setHeure()` pour un nœud avec NTP.

### Réception

- Table de slots **construite par l'application** (`surveiller()`) : aucune
  collision possible, mémoire déterministe.
- Les paires non surveillées sont **comptées**, pas écartées en silence.
- `lire()` expose `age_ms` — la donnée qui manquait pour comparer deux mesures
  honnêtement (PZEM vs Linky).
- `perdus` calculé par slot à partir des trous de `seq`.
- Aucune file d'attente : la valeur la plus récente gagne.

### Non fonctionnel

- Header-only, **zéro allocation dynamique** (aucune `String`, aucun `new`).
- ESP8266, ESP32 IDF 4.x (core Arduino 2.x) et IDF 5.x (core Arduino 3.x) :
  compilé et vérifié sur les trois.
- `sizeof(EspNowUniVar)` = **1 756 o** avec la configuration par défaut.
- Aucune interprétation par cast de pointeur : tout passe par `memcpy` et par
  lecture octet par octet (alignement de `double` non garanti).

### Contrat

- `EspNow_UniVar_Vars.h` : identifiants `id_iot` et `id_var`, **append-only**.
  Un numéro retiré est brûlé, jamais recyclé.
- Les index d'énergie voyagent en **Wh** (`uint32`) : la nouvelle trame n'a
  aucune dette de magnitude, contrairement aux codes `_Unit_kWh_` hérités.

### Recette

`test/host/run.sh` — 12 exigences, **158 vérifications**, exécutables sur
l'ordinateur sans carte ni flash.
