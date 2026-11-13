// Arduino.h — redirection vers le shim pour la compilation hôte.
// La librairie inclut <Arduino.h> ; ce fichier est placé AVANT le framework
// dans le chemin d'inclusion (voir run.sh).
#ifndef ARDUINO_H_HOST_REDIRECT
#define ARDUINO_H_HOST_REDIRECT
#include "arduino_shim.h"
#endif
