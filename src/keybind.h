#ifndef _KEYBIND_H_
#define _KEYBIND_H_

#ifdef MYDLL_RELEASE_BUILD
#define KSERV_WINDOW_TITLE "KitServer 6 Key Binder"
#else
#define KSERV_WINDOW_TITLE "KitServer 6 Key Binder (debug build)"
#endif
#define CREDITS "About: v6.3.0 (11/2006) by Juce."

#define LOG(f,x) if (f != NULL) fprintf x

#define WM_APP_KEYDEF WM_APP + 1

#endif

