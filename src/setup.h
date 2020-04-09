#ifndef _SETUP_MAIN_
#define _SETUP_MAIN_

#ifdef MYDLL_RELEASE_BUILD
#define SETUP_WINDOW_TITLE "KitServer 6 Setup"
#else
#define SETUP_WINDOW_TITLE "KitServer 6 Setup (debug build)"
#endif
#define CREDITS "About: v6.8.0 (04/2020) by Juce and Robbie."

#define LOG(f,x) if (f != NULL) fprintf x

#endif

