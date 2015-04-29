/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#pragma once

#include "PlugMode.h"
#include <set>

class Options;
class MediaTrack;

class FloatingWindowInfo {
public:
        FloatingWindowInfo() {
                hwnd = NULL;
                pMediaTrack = NULL;
                iSlot = -1;
        }
        FloatingWindowInfo(HWND hwnd, MediaTrack* pMediaTrack, int slot) {
                set(hwnd, pMediaTrack, slot);
        }
        void set(HWND hwnd, MediaTrack* pMediaTrack, int slot) {
                this->hwnd = hwnd;
                this->pMediaTrack = pMediaTrack;
                iSlot = slot;
        }
        HWND hwnd;
        MediaTrack* pMediaTrack;
        int iSlot;
};

class PlugWindowManager
{
public:
        PlugWindowManager(PlugMode* pPlugMode);
        ~PlugWindowManager(void);

        void switchedTo(MediaTrack* pMediaTrack, int iSlot);
        void allowOnlySelectedFloat(MediaTrack* pAllowTrack, int iAllowSlot);
        void closeAll();
        void moveWnd();

private:
        typedef std::set<HWND> tListHWND;
        tListHWND m_knownHWND;

        void moveWnd(HWND hwnd);

        void openFloating( MediaTrack* pMediaTrack, int iSlot );

        bool isOptionSetTo(wchar_t* optionName, wchar_t* attribute) { return m_pPlugMode->getOptions()->isOptionSetTo(optionName, attribute); }
        bool is2ndOptionSetTo(wchar_t* optionName, wchar_t* attribute) { return m_pPlugMode->get2ndOptions()->isOptionSetTo(optionName, attribute); }
        PlugMode* m_pPlugMode;

        FloatingWindowInfo m_lastFloat;
};
