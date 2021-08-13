#ifndef LANGUAGE_H
#define LANGUAGE_H

// src/lib/locale.c: init i18n locale
extern void lang_init_locale();

// src/lib/locale.c: load new locale
extern void lang_load_locale(const char*dir,const char*lang,const char*domain);

#endif
