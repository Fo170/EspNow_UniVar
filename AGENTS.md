# AGENTS.md — EspNow_UniVar

Instructions pour les agents IA travaillant sur ce dépôt.

## Informations générales

- **Nom de la librairie** : EspNow_UniVar
- **Version** : 2.2.0
- **Compte GitHub** : https://github.com/Fo170/
- **Auteur** : Olivier FOURNET
- **Email** : olivier.fournet@free.fr
- **Licence** : GNU General Public License v3.0
- **Langue** : français pour les réponses

## Rôle — à ne jamais déborder

Cette librairie transporte **une variable par paquet ESP-NOW**. Elle ne fait
que ça.

## ⚠️ AGNOSTICISME — la règle la plus importante

**La librairie ne doit connaître AUCUN identifiant métier.** Pas de `UVAR_PZEM_*`,
pas de `UVAR_RAD_*`, pas de `UVAR_IOT_PZEM004T`. Rien qui parle d'un appareil,
d'une pièce ou d'une grandeur physique particulière.

| Ce qui appartient à la librairie | Ce qui appartient à l'application |
|---|---|
| magic, version, CRC, taille de trame | les valeurs de `id_iot` |
| `publier` / `poll` / `surveiller` / `lire` | les valeurs de `id_var` |
| la table de slots, les compteurs | le sens métier de chaque variable |
| les règles du protocole (0 = invalide) | les unités retenues par variable |

Un contrat d'application (identifiants du parc) vit **hors de ce dépôt**, dans
le dossier partagé du projet ou dans un dépôt séparé. Il ne doit **jamais**
être ajouté à `src/`.

Si une demande implique d'ajouter une constante métier dans `src/`, c'est le
signe qu'elle doit aller dans le contrat de l'application.

- ❌ Pas d'ACK, pas d'unicast, pas de retransmission (c'est de la télémétrie en
  diffusion, pas du commandement).
- ❌ Pas de chiffrement PMK/LMK (décidé : diffusion, aucune donnée sensible).
- ❌ Pas de découverte automatique de peers, pas de routage multi-sauts.
- ❌ Pas de stockage persistant.
- ❌ Pas de calcul métier : pas de conversion d'unité, pas d'arrondi.

## Dépendances

| Dépendance | Rôle |
|---|---|
| `Unity_Dictionary` | **code** — `EspNow_UniVar.h` en a besoin : les codes `_Unit_*` sont dans la trame |
| `Unity` | **affichage** — préfixes SI, utilisé par les exemples et les applications |

Les deux sont déclarées dans `library.json`. Oublier `Unity` fait échouer
`pio run` sur `Unity.h: No such file or directory` dès qu'un exemple est
compilé — c'est arrivé en v1.1.0, corrigé en v1.1.1.

## Règles de conception non négociables

1. **Zéro allocation dynamique.** Aucune `String`, aucun `new`, aucune
   `std::vector`. Header-only, tout en statique.
2. **Aucun accès par cast de pointeur sur la trame.** Toujours `memcpy` ou
   lecture octet par octet (`uvarGet16` / `uvarGet32`). L'alignement de `double`
   n'est pas garanti à l'offset 9.
3. **Little-endian explicite.** Ne jamais `memcpy` un `uint32_t` brut dans la
   trame : écrire les octets un par un.
4. **Aucun rejet silencieux.** Tout refus incrémente un compteur de
   `UniVarStats`. Si un nouveau cas de rejet apparaît, il lui faut son compteur.
5. **La table de réception est construite par l'application** (`surveiller()`),
   jamais par la réception. Un index dérivé de `(id_iot, id_var)` créerait des
   collisions qui écraseraient une variable par une autre.
6. **Le format ne change pas sans changer `UVAR_VER`.** La taille, l'ordre et la
   sémantique des champs sont un contrat inter-appareils.

## Concurrence — à ne jamais casser

Le callback de réception s'exécute dans la **tâche WiFi**, pas dans `loop()`.
Sur ESP32 ce sont deux cœurs ; sur ESP8266 la tâche WiFi préempte `loop()`.

**Toute écriture de slot doit être encadrée par `_verrou()` / `_deverrou()`**,
et `valide = true` doit rester **en dernier**, à l'intérieur de la section :

```cpp
_verrou();
// ... écrire tous les champs ...
s.valide = true;       // EN DERNIER
_deverrou();
```

Toute lecture doit faire de même autour de la copie. Ne jamais lire `valide`
puis la valeur hors section : c'est le bug corrigé en 2.1.0 (`valide` était
posé avant la copie).

Aucune file d'attente : la valeur reçue **remplace** simplement l'ancienne.
La section critique est volontairement minuscule (~1 µs) — ne rien y ajouter
qui puisse allonger ou bloquer.

## `EspNow_UniVar_Vars.h` — APPEND-ONLY

- Un `id_var` attribué n'est **jamais** renuméroté ni réutilisé.
- Une variable retirée laisse son numéro **brûlé** : commenter la ligne, ne pas
  recycler le numéro.
- Raison : un récepteur non reflashé lirait une valeur sous un autre sens, sans
  aucun moyen de le détecter.
- Ce fichier est la **seule** source des identifiants. Aucun projet ne doit
  redéfinir un `VAR_*` localement.

## Unités

- L'unité transportée est l'entier de `Unity_Dictionary` (`_Unit_*`).
- **Unités de BASE uniquement** : `_Unit_Watt_`, `_Unit_VA_`, `_Unit_VAR_`,
  `_Unit_Wh_`, `_Unit_Ohms_`… Le préfixe (k, M, m) relève de l'affichage, donc
  de `Unity` (`C_UNITY::valeurAvecUnite`).
- **Ne jamais émettre `_Unit_kWh_`, `_Unit_kW_` ou `_Unit_kOhms_`** dans ce
  format : ce sont des codes hérités portant une magnitude. Les index d'énergie
  partent en **Wh `uint32`**.

## Plateformes

Le callback d'ENVOI a changé deux fois de signature côté ESP-IDF :

| Cible | Signature |
|---|---|
| ESP8266 | `void(u8* mac, u8 status)` |
| ESP32 IDF 4.x → 5.3 | `void(const uint8_t* mac, esp_now_send_status_t)` |
| ESP32 IDF 5.4+ | `void(const esp_now_send_info_t* tx_info, esp_now_send_status_t)` |

La sélection se fait par `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)`.
**Vérifier les trois** avant de publier :

```bash
pio run    # ESP8266 (espressif8266)
pio run    # ESP32 core 3.x (IDF 5.x)
pio run    # ESP32 core 2.x (IDF 4.4, platform = espressif32@6.12.0)
```

## Recette obligatoire avant toute publication

```bash
./test/host/run.sh
```

Doit afficher `RESULTAT : OK` avec **158 vérifications**. Un échec est bloquant :
la librairie ne doit pas être flashée sur une carte.

Le test utilise un `Arduino.h` minimal (`test/host/arduino_shim.h`) et la **vraie**
`Unity_Dictionary` : le dictionnaire d'unités est donc réellement exercé.

## Convention de code

- Commentaires en **français**, expliquant le *pourquoi*, pas le *quoi*.
- Pas de commentaire décoratif ; en revanche toute décision contre-intuitive est
  documentée sur place (par exemple : pourquoi `val[8]` brut et non une union
  typée — un membre `double` imposerait un alignement de 8 et coûterait 96 o).
- Version exposée par `__ESPNOW_UNIVAR_H_VERSION__` (source unique, agrégée par
  `liste_lib.h` des projets).

## Publication

1. Mettre à jour `__ESPNOW_UNIVAR_H_VERSION__`, `library.json`, `library.properties`.
2. Renseigner `CHANGELOG.md`.
3. `git tag vX.Y.Z` puis push du tag.
4. `gh release create vX.Y.Z --title "..." --notes "..."`.
