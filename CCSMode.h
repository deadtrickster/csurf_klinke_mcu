/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include "csurf.h"
#include "DisplayHandler.h"
#include "vpot_led.h"

#pragma once

class CCSManager;
class MediaTrack;
class Selector;
class Options;

class CCSMode
{
public:
        CCSMode(CCSManager* pManager);
        virtual ~CCSMode(void);

        virtual void activate();
        virtual void deactivate(){};

        // from MCU via CCSManager
        virtual bool buttonFaderBanks(int button, bool pressed){return false;}
        virtual bool buttonFlip(bool pressed){return false;}
        virtual bool buttonGView(bool pressed){return false;}
        virtual bool buttonNameValue(bool pressed){return false;}

        // channels are 1-based, channel 0 exist for fader, it is the Master-Fader
        virtual bool buttonRec(int channel, bool pressed){return false;}
        virtual bool buttonRecDC(int channel, bool pressed){return false;}
        virtual bool buttonMute(int channel, bool pressed){return false;}
        virtual bool buttonSolo(int channel, bool pressed){return false;}
        virtual bool buttonSoloDC(int channel){return false;}
        virtual bool buttonSelect(int channel, bool pressed){return false;}
        virtual bool buttonSelectDC(int channel){return false;}
        virtual bool buttonSelectLong(int channel){return false;}

        virtual bool fader(int channel, int value){return false;}
        virtual bool faderTouched(int channel, bool touched){return false;}
        virtual bool somethingTouched(bool touched){return false;} // is called when the first fader is touched or the last fader touch is released (incl. a short delay)
        virtual bool singleFaderTouched(int channel){return false;} // channels are 1-based, channel 0 is called, when no fader or more then one fader is touched
        virtual bool singleVPotTouched(int channel){return false;} // channels are 1-based, channel 0 is called, when no fader or more then one fader is touched


        virtual bool vpotMoved(int channel, int numSteps){return false;} // numSteps are negativ for left rotation
        virtual bool vpotPressed(int channel, bool pressed){return false;}

        // default update implementation turn LEDs off
        virtual void updateRecLEDs();
        virtual void updateSelectLEDs();
        virtual void updateSoloLEDs();
        virtual void updateMuteLEDs();
        virtual void updateFlipLED();  
        virtual void updateGlobalViewLED(); 
        virtual void updateAssignmentDisplay();
        virtual void updateDisplay() = 0;

        // default update implementation does nothing
        virtual void updateFaders();
        virtual void updateVPOTs();

        virtual void updateEverything();

        virtual void trackListChange(){updateEverything();}
//      virtual void trackVolume(MediaTrack *trackid, double volume){/*updateFaders();updateVPOTs();*/}
//      virtual void trackPan(MediaTrack *trackid, double pan){/*updateVPOTs();updateFaders();}
//      virtual void trackMute(MediaTrack *trackid, bool mute){updateMuteLEDs();}
//      virtual void trackSelected(MediaTrack *trackid, bool selected){/*updateSelectLEDs();*/}//add updateSelectLED(MediaTrack) und eigentlich gehoert das ganze nicht in die Basisklasse
//      virtual void trackSolo(MediaTrack *trackid, bool solo){updateSoloLEDs();}
//      virtual void trackRecArm(MediaTrack *trackid, bool recarm){updateRecLEDs();}
        virtual void trackName(MediaTrack* trackid, const char* pName){updateDisplay();}

        virtual void frameUpdate(){}

        virtual Selector* getSelector(){return NULL;}

        virtual Options* getOptions(){return NULL;}
        virtual Options* get2ndOptions(){return NULL;}

        CCSManager* getCCSManager(){return m_pCCSManager;}


        // Helper
        MediaTrack* selectedTrack(); // returns null if zero or more then one track is selected
        bool isModifierPressed(int modifier);

protected:
        CCSManager* m_pCCSManager;
};
