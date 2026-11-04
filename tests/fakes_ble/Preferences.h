#pragma once
#include "Arduino.h"
#include <map>
extern std::map<std::string,std::string> bleTestSettings;
extern bool bleTestWriteFail;
class Preferences {
public:
    bool begin(const char *,bool) { return true; }
    bool isKey(const char *key) { return bleTestSettings.count(key)!=0; }
    String getString(const char *key,const char *fallback) { auto it=bleTestSettings.find(key); return it==bleTestSettings.end()?fallback:it->second; }
    size_t putString(const char *key,const char *value) { if(bleTestWriteFail) return 0; bleTestSettings[key]=value; return std::strlen(value)+1; }
    bool remove(const char *key) { return bleTestSettings.erase(key)!=0; }
};
