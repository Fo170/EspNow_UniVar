// ============================================================================
//  arduino_shim.h — Arduino minimal pour exécuter les tests sur l'hôte.
//
//  Ne simule QUE ce dont la librairie a besoin hors radio :
//    - String        (utilisé par Unity_Dictionary::Str_Unit)
//    - millis()      horloge SIMULÉE, pilotée par le test via g_millis
//    - strnlen       absent de certaines libc
//
//  Aucune émulation ESP-NOW : la librairie n'atteint jamais la couche radio
//  sous UVAR_HOST_TEST (cf. _envoyerBrut).
// ============================================================================
#ifndef ARDUINO_SHIM_H
#define ARDUINO_SHIM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>

// ----------------------------------------------------------------------------
//  Horloge simulée
// ----------------------------------------------------------------------------
inline uint32_t g_millis = 0;
inline uint32_t millis() { return g_millis; }
inline void delay(uint32_t) {}

// ----------------------------------------------------------------------------
//  String minimal — suffisant pour Str_Unit()
// ----------------------------------------------------------------------------
class String {
public:
  String() {}
  String(const char* s) : _s(s ? s : "") {}
  String& operator=(const char* s) { _s = (s ? s : ""); return *this; }
  const char* c_str() const { return _s.c_str(); }
  size_t length() const { return _s.size(); }
  bool operator==(const char* s) const { return _s == (s ? s : ""); }
private:
  std::string _s;
};

#endif // ARDUINO_SHIM_H
