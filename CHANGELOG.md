# Changelog — EspNow_UniVar

Toutes les dates sont au format AAAA-MM-JJ.
Le versionnage suit [SemVer](https://semver.org/lang/fr/).

## [2.3.0] - 2026-11-16

### ✨ Ajouté — crochet de DÉCOUVERTE `surInconnue`

**Besoin.** Une trame valide dont la paire `(id_iot, id_var)` n'est pas dans la
table des slots était **décodée puis jetée** : `_stats.non_surveille`
s'incrémentait, mais rien ne disait *quelle* variable circulait. Impossible de
détecter qu'un nœud s'est mis à publier une variable inattendue, ou qu'un
nouvel émetteur est apparu.

**Ajout** (rétro-compatible, `nullptr` par défaut) :

```cpp
// membre public de EspNowUniVar
void (*surInconnue)(const UniVarVue& vue) = nullptr;
```

Appelé pour chaque trame `UVAR_OK` dont la paire n'est pas surveillée, avec la
vue complète : `id_iot`, `id_var`, `type`, `unit`, `len`, `valeur`, `seq`,
`epoch`, `t_fct`.

```cpp
env.surInconnue = [](const UniVarVue& v) {
  // tâche WiFi : ne pas bloquer, ne pas allouer, RECOPIER v.valeur
};
```

**⚠️ Contrat d'appel.** Le crochet s'exécute dans la **tâche WiFi** (callback
de réception), pas dans `loop()`. Il ne doit donc :

- ni bloquer, ni allouer ;
- **ne jamais appeler `surveiller()`** : la table `_slots` est en cours de
  parcours par la même tâche. L'application marque la découverte et abonne
  depuis `loop()` ;
- recopier `vue.valeur` si elle veut garder la donnée (pointeur sur le tampon
  radio, invalidé au retour).

**Preuve** : recette hôte `test/host/run.sh` → **175 vérifications, 0 échec**,
dont le nouveau test 13 (crochet non déclenché sur une paire surveillée,
champs et valeur corrects, `nullptr` = comportement 2.2.1).

## [2.2.1] - 2026-11-14

### 🐛 Corrigé — `rearm()` coupait la connexion WiFi qu'il devait préserver

**Symptôme observé** sur le BME280 Garage (14/11/2026) : la carte se connectait,
puis perdait la connexion, rebasculait sur un autre réseau, se reconnectait, et
recommençait — indéfiniment. Résultat : injoignable (ni ICMP, ni ARP, ni mDNS).

**Cause.** `rearm()` appelait `_demarrerRadio()`, qui commençait par :

```cpp
WiFi.mode(WIFI_STA);
WiFi.disconnect(false);      // ← coupe l'association en cours
```

Or les gestionnaires WiFi rappellent `rearm()` **à chaque (re)connexion**,
précisément parce que leur reset (`WiFi.mode(WIFI_OFF)`) a détruit le stack
ESP-NOW. D'où la boucle :

```
connecté → rearm() → déconnecté → reconnecté → rearm() → …
```

Second défaut : `esp_now_add_peer()` sur un peer **déjà présent** échoue, donc le
canal n'était même pas mis à jour.

**Correction.**

| Avant | Après |
|---|---|
| `rearm()` → `_demarrerRadio()` (touche WiFi) | `rearm()` → `_demarrerRadio(false)` : **le mode WiFi n'est jamais modifié** |
| `_initialise` conservé → stack jamais ré-initialisé | `_initialise = false` puis ré-init complète (init + callbacks + peer) |
| `esp_now_add_peer()` seul | `esp_now_del_peer()` **puis** `esp_now_add_peer()` → canal réellement appliqué |

`begin()` garde le comportement d'origine (`toucherWifi = true`) : il est appelé
au démarrage, avant `WiFi.begin()`, où forcer le mode station est légitime.

**Impact** : tous les nœuds qui appelaient `rearm()` après une reconnexion
(Thermometre, Radiateur, Station_Meteo, Surveillance_Batterie, LinkysTIC,
ESP32_Ordonnanceur) retrouvent une connexion stable.

## [2.2.0] - 2026-11-13

### 🔧 Simplifié — le seqlock devient un verrou court

Le seqlock de la 2.1.0 garantissait bien l'atomicité, mais imposait une **boucle
de relecture dans trois fonctions** (`lire`, `lireTexte`, `infos`) et un compteur
de version par slot. Trop de mécanique pour une copie de quelques octets.

Remplacé par un **verrou court** : `_verrou()` / `_deverrou()`, une section
critique qui ne couvre que la copie.

| | Seqlock (2.1.0) | Verrou court (2.2.0) |
|---|---|---|
| Code | ~40 lignes, 3 boucles de relecture | **~10 lignes**, aucune boucle |
| Blocage | aucun | ~1 µs (une copie de ≤ 24 o) |
| Mémoire | 2 o/slot (dans le remplissage) | 4 o (un mutex pour toute la librairie) |
| Champ public | `stats().lecture_conflit` | *(supprimé — sans objet)* |

Le principe reste inchangé : **aucune file d'attente**, la valeur reçue remplace
simplement l'ancienne, et `valide` est posé **en dernier** dans la section
critique — un lecteur ne peut jamais voir un slot marqué valide avec une valeur
pas encore écrite.

### ⚠️ Rupture mineure

`UniVarStats::lecture_conflit` disparaît (il n'a de sens qu'avec un seqlock).
Aucun consommateur : le champ datait de la 2.1.0, publiée quelques minutes plus tôt.

### Recette

Retour à **12 exigences / 158 vérifications** : le test 13 testait
spécifiquement le seqlock. La garantie du verrou est structurelle (exclusion
mutuelle) et ne se simule pas sur un hôte mono-thread.

Vérifié : ESP8266, ESP32 IDF 4.4 et ESP32 IDF 5.5 — `noInterrupts()` et
`portENTER_CRITICAL` compilent sur les trois.

---

## [2.1.0] - 2026-11-13

### 🐛 Corrigé — lectures cohérentes sans buffer ni verrou

**Le défaut.** Le callback de réception (tâche WiFi) écrit dans le slot pendant
que l'application le lit dans `loop()`. Sur ESP32 ils sont sur **deux cœurs** ;
sur ESP8266 la tâche WiFi **préempte** `loop()`. Deux conséquences :

1. `valide = true` était posé **avant** la copie de la valeur → un lecteur
   pouvait voir un slot marqué valide avec l'**ancienne** valeur ;
2. un `double` (8 o) ou une chaîne (jusqu'à 24 o) pouvait être lu **à moitié
   écrit** — un `memcpy` de 8 octets n'est pas atomique sur un MCU 32 bits.

**La correction.** Un **seqlock** par slot : compteur impair pendant l'écriture,
pair quand la valeur est stable. Le lecteur recommence si le compteur a bougé.

| | |
|---|---|
| File d'attente | **aucune** — le principe « pas de buffer » est conservé |
| Verrou bloquant | **aucun** — pas de risque d'inverser la priorité avec la tâche WiFi |
| Coût mémoire | **0 octet** : le compteur tient dans le remplissage existant du slot (60 o inchangé) |
| Coût CPU | 2 incréments par réception, une relecture occasionnelle |

`valide` est désormais posé **en dernier**.

### ✨ Ajouté

- `UniVarStats::lecture_conflit` — nombre de lectures abandonnées faute de
  stabilité. Un compteur non nul signale une contention anormale : c'est un
  diagnostic, pas un fonctionnement normal.
- `EspNowUniVar::slotPourTest()` (sous `UVAR_HOST_TEST`) — permet de simuler une
  écriture concurrente dans la recette.

### Recette

**13 exigences, 172 vérifications** (contre 12 / 158). Le nouveau test 13 prouve
qu'une lecture pendant une écriture ne renvoie **rien** : ni valeur partielle,
ni valeur périmée marquée valide.

---

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
