#ifndef _KEYBIND_H_
#define _KEYBIND_H_

#ifdef MYDLL_RELEASE_BUILD
#define KSERV_WINDOW_TITLE "KitServer 6 Key Configurator"
#else
#define KSERV_WINDOW_TITLE "KitServer 6 Key Configurator (debug build)"
#endif
#define CREDITS "About: v6.8.0 (04/2020) by Juce."

#define LOG(f,x) if (f != NULL) fprintf x

#define WM_APP_KEYDEF WM_APP + 1

#endif

