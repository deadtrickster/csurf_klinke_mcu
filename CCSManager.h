/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#pragma once

#include "CCSMode.h"

class CSurf_MCU;
class CCSMode;
class MultiTrackMode;
class PanMode;
class PerformanceMode;
class DisplayTrackMeter;
class CommandMode;
class SendMode;
class ReceiveMode;
class CCSModesEditor;
class PlugMode;
class Options;

#define CHECKMODE if (pCaller != m_pActualMode) return;
#define CHECKMODEANDCHANNEL     ASSERT(channel >= 0 && channel < 9);   CHECKMODE

class CCSManager
{
public:
        enum EMode {
                SEND = 0,
                RECEIVE
        }; //  only modes then can be set directly are enumerated here

        enum EElement {
                FADER = 0,
                VPOT
        };

        CCSManager(CSurf_MCU* pMCU);
        ~CCSManager(void);

        void init();

// from MCU 
        bool buttonVPOTassign(int button, bool pressed);

// pass to CCSMode
        bool buttonFaderBanks(int button, bool pressed);
        bool buttonFlip(bool pressed);
        bool buttonGView(bool pressed);
        bool buttonNameValue(bool pressed);

        bool buttonRec(int channel, bool pressed);
        bool buttonRecDC(int channel, bool pressed);
        bool buttonSelect(int channel, bool pressed);
        bool buttonSelectDC(int channel);
        bool buttonMute(int channel, bool pressed);
        bool buttonSolo(int channel, bool pressed);
        bool buttonSoloDC(int channel);
        bool buttonSelectLong(int channel);

        bool fader(int channel, int value);
        bool faderTouched(int channel, bool touched);

        bool vpotMoved(int channel, int numSteps); // numSteps are negativ for left rotation
        bool vpotPressed(int channel, bool pressed);

        void updateEverything();
        void updateFader();
        void updateVPOTs();

        void updateAllLEDs();
        void updateFlipLED();
        void updateGlobalViewLED();
        void updateMuteLEDs();
        void updateRecLEDs();
        void updateSelectLEDs();
        void updateSoloLEDs();
        void updateAssignmentDisplay();

        void trackListChange();
        void trackSelected(MediaTrack *trackid, bool selected);
        void trackName(MediaTrack* trackid, const char* pName);

        void frameUpdate(DWORD time); // called from CSurf::MCU.run each frame update

// to MCU
        void setFader(CCSMode* pCaller, int fader, int value); 
        void setRecLED(CCSMode* pCaller, int channel, int state);
        void setSelectLED(CCSMode* pCaller, int channel, int state);
        void setSoloLED(CCSMode* pCaller, int channel, int state);
        void setMuteLED(CCSMode* pCaller, int channel, int state);
        
        void setFlipLED(CCSMode* pCaller, int state);
        void setGlobalViewLED(CCSMode* pCaller, int state);

        void setAssignmentDisplay(CCSMode* pCaller, char text[2]);

// touched information
        int getNumFadersTouched();
        int getNumVPotTouched();
        int getElementsTouched();
        bool getFaderTouched(int channel){return m_faderTouched[channel];}
        bool getVPotTouched(int channel){return m_vpotTouched[channel];}

        int getNumSoloButtonsPressed(){return m_iNumSoloButtonsPressed;}
        int getNumMuteButtonsPressed(){return m_iNumMuteButtonsPressed;}
        int getNumSelectButtonsPressed(){return m_iNumSelectButtonsPressed;}

        // editor access
        void closeEditorIfOpen(Component* pComponent);

// getter/setter
        CSurf_MCU* getMCU() {return m_pMCU;} 
        DisplayHandler* getDisplayHandler();
        VPOT_LED* getVPOT(int channel) { return &m_pVPOTS[channel]; } // channel is also 1 based, modification of VPOT of channel 0 doesn't do anything
        void setMode(CCSManager::EMode mode);
        CCSMode* getActualMode(){return m_pActualMode;}
        DWORD getLastTime(){return m_lastTime;}
        CCSModesEditor* getEditor(){return m_pEditor;}

// helper
        void switchToDisplay(CCSMode* pMode, Display* pDisplay);
        void setVPOTMode(VPOT_LED::MODE mode); // set all VPOTs to the same mode


private:
        void elementTouched(EElement fader, int channel, bool touched);
        void checkOption();

        void updateVPOTLeds();
        void changeMode( CCSMode* pNewMode );

        bool isSelectedTrack(MediaTrack* tr);
        void selectTrack( MediaTrack *trackid );
        void deselectTrack( MediaTrack *trackid );
        int getNumTrueArrayEntries(bool* pArray, int size);
        CSurf_MCU* m_pMCU;

        CCSMode* m_pActualMode;
        CCSModesEditor* m_pEditor;
        CommandMode* m_pCommandMode;
        PanMode* m_pPanMode;
        PerformanceMode* m_pPerformanceMode;
        SendMode* m_pSendMode;
        ReceiveMode* m_pReceiveMode;
        PlugMode* m_pPlugMode;

        // all the following stuff is one based ([0] is only used for m_faderPos, their it's the master fader)
        VPOT_LED* m_pVPOTS;
        int m_stateRec[9];
        int m_stateSolo[9];
        int m_stateMute[9];
        int m_stateSelect[9];
        int m_stateFlip;
        int m_stateGlobalView;
        int m_faderPos[9];
        char m_stateAssignmentDisplay[2];

        bool m_faderTouched[9];
        DWORD m_faderTouchedTill[9];
        bool m_vpotTouched[9];
        DWORD m_vpotTouchedTill[9];
        DWORD m_lastTime;

        bool m_switchBack;
        CCSMode* m_pSwitchBackMode;

        int m_iNumSoloButtonsPressed;
        int m_iNumMuteButtonsPressed;
        int m_iNumSelectButtonsPressed;

        bool m_selectorActive;
        Options* m_optionActive;
};
