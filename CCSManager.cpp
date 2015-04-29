/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include <windows.h>
#include "CCSManager.h"
#include "multitrackmode.h"
#include "panmode.h"
#include "performancemode.h"
#include "CommandMode.h"
#include "SendMode.h"
#include "ReceiveMode.h"
#include "PlugMode.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "DisplayTrackMeter.h"
#include "ccsmodeseditor.h"
#include "Options.h"
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but

#define TOUCHED_MS 2000;

CCSManager::CCSManager(CSurf_MCU* pMCU) {
  m_pMCU = pMCU;
  m_pCommandMode = new CommandMode(this);
  m_pPanMode = new PanMode(this);
  m_pPerformanceMode = new PerformanceMode(this);
  m_pSendMode = new SendMode(this);
  m_pReceiveMode = new ReceiveMode(this);
  m_pPlugMode = new PlugMode(this);

  m_pEditor = new CCSModesEditor(this);

  m_pActualMode = m_pPanMode;

  m_pSwitchBackMode = NULL;
  m_switchBack = false;
  m_selectorActive = false;
  m_optionActive = NULL;

  m_pVPOTS = new VPOT_LED[9];
  for (int i = 0; i < 9; i++) {
    m_pVPOTS[i].init(getMCU(), i);
    m_stateRec[i] = LED_UNKNOWN;
    m_stateSolo[i] = LED_UNKNOWN;
    m_stateMute[i] = LED_UNKNOWN;
    m_stateSelect[i] = LED_UNKNOWN;
    m_faderPos[i] = -1;
    m_faderTouched[i] = false;
    m_vpotTouchedTill[i] = 0;
    m_faderTouchedTill[i] = 0;
    m_vpotTouched[i] = false;
  }
  m_stateFlip = LED_UNKNOWN;
  m_stateGlobalView = LED_UNKNOWN;
  memset(m_stateAssignmentDisplay, 1, 2);

  m_iNumSoloButtonsPressed = 0;
  m_iNumMuteButtonsPressed = 0;
  m_iNumSelectButtonsPressed = 0;

  m_lastTime = 0;
}

CCSManager::~CCSManager(void) {
  safe_delete(m_pEditor); // editor must be delete before modes
  safe_delete(m_pCommandMode);
  safe_delete(m_pPanMode);
  safe_delete(m_pPerformanceMode);
  safe_delete(m_pSendMode);
  safe_delete(m_pReceiveMode);
  safe_delete(m_pPlugMode);
  safe_delete_array(m_pVPOTS);
  m_pActualMode = NULL;
}

void CCSManager::init() {
#if EASY_DEBUG
  if (!m_pMCU->IsExtender()) {
    m_pEditor->setMainComponent(m_pPlugMode->createEditorComponent(), true);
    changeMode(m_pPlugMode);
  }
#endif

  m_pActualMode->activate();
}

void CCSManager::setVPOTMode( VPOT_LED::MODE mode )
{
  for (int iChannel = 0; iChannel < 9; iChannel++)
    m_pVPOTS[iChannel].setMode(mode);
}

bool CCSManager::buttonVPOTassign(int button, bool pressed) {
  // open editor (in the case that one exist) when the assign button is pressed in combination with alt
  if (m_pMCU->IsModifierPressed(VK_ALT)) {
    if (pressed) { 
      switch (button) {
        case B_VPOT_EQ:
          m_pEditor->setMainComponent(m_pCommandMode->createEditorComponent(), true);
          break;
        case B_VPOT_PLUG:
          m_pEditor->setMainComponent(m_pPlugMode->createEditorComponent(), true);
          break;
        case B_VPOT_PAN:
          m_pEditor->setMainComponent(m_pPanMode->createEditorComponent(), true);
          break;
      }
    }
    return true;
  }


  CCSMode* pNewMode = m_pActualMode;
  MediaTrack* pSingleTrack = Tracks::instance()->getSelectedSingleTrack();
  
  if (pressed) {
    m_pSwitchBackMode = m_pActualMode;
    m_switchBack = false;

    switch(button) {
      case B_VPOT_PAN:
        pNewMode = m_pPanMode;
        break;
      case B_VPOT_EQ:
        pNewMode = m_pCommandMode;
        break;
//      case B_VPOT_INSTRUMENT:
//        pNewMode = m_pPerformanceMode;
//        break;
      case B_VPOT_SEND:
        if (pSingleTrack && m_pActualMode == m_pSendMode && m_pReceiveMode->getNumSends() > 0)
          pNewMode = m_pReceiveMode;
        else {
          if (pSingleTrack && m_pSendMode->getNumSends() > 0) {
            pNewMode = m_pSendMode;
          } else if (pSingleTrack && m_pReceiveMode->getNumSends() > 0) {
            pNewMode = m_pReceiveMode;
          }
        }
        break;
      case B_VPOT_PLUG:
        if (pSingleTrack && m_pPlugMode->getNumPlugsInSelectedTrack())
          pNewMode = m_pPlugMode;
        break;
    }
  } else if (m_selectorActive) {
    m_selectorActive = false;
    m_pActualMode->activate();
  } else if (m_switchBack) {
    pNewMode = m_pSwitchBackMode;
  }

  if (pressed && pNewMode == m_pSwitchBackMode && pNewMode->getSelector() != NULL) {
    pNewMode->getSelector()->activateSelector();
    m_selectorActive = true;
  } else {
    changeMode(pNewMode);
  }

  return true;
}

void CCSManager::switchToDisplay(CCSMode* pMode, Display* pDisplay) {
  if (pMode == m_pActualMode && !m_selectorActive && !m_optionActive && !m_pMCU->IsButtonPressed(B_NAME_VALUE)) {
    m_pMCU->GetDisplayHandler()->switchTo(pDisplay);
  }
}

void CCSManager::updateVPOTLeds() {
  MediaTrack* m_pSelectedTrack = Tracks::instance()->getSelectedSingleTrack();

  m_pMCU->SetLED(B_VPOT_EQ, m_pActualMode == m_pCommandMode ? LED_BLINK : LED_ON);
  m_pMCU->SetLED(B_VPOT_PAN, m_pActualMode == m_pPanMode ? LED_BLINK : LED_ON);

  // B_VPOT_SEND
  if (m_pActualMode == m_pReceiveMode || m_pActualMode == m_pSendMode) {
    m_pMCU->SetLED(B_VPOT_SEND, LED_BLINK);
  } else if (m_pSelectedTrack && m_pReceiveMode->getNumSends() > 0 || m_pSendMode->getNumSends() > 0) {
    m_pMCU->SetLED(B_VPOT_SEND, LED_ON);
  } else {
    m_pMCU->SetLED(B_VPOT_SEND, LED_OFF);
  }

  // B_VPOT_PLUG
  if (m_pActualMode == m_pPlugMode) {
    m_pMCU->SetLED(B_VPOT_PLUG, LED_BLINK);
  } else if (m_pSelectedTrack && m_pPlugMode->getNumPlugsInSelectedTrack()) { 
    m_pMCU->SetLED(B_VPOT_PLUG, LED_ON);
  } else {
    m_pMCU->SetLED(B_VPOT_PLUG, LED_OFF);
  }
}

bool CCSManager::buttonFaderBanks(int button, bool pressed) {
  ASSERT(button >= B_BANK_DOWN && button <= B_CHANNEL_UP);
  m_switchBack = true;
  return m_pActualMode->buttonFaderBanks(button, pressed);
}

bool CCSManager::buttonFlip(bool pressed) {
  m_switchBack = true;
  return m_pActualMode->buttonFlip(pressed);
}

bool CCSManager::buttonGView(bool pressed) {
  m_switchBack = true;
  return m_pActualMode->buttonGView(pressed);
}

bool CCSManager::buttonNameValue(bool pressed) {
  m_switchBack = true;
  return m_pActualMode->buttonNameValue(pressed);
}

bool CCSManager::buttonRec(int channel, bool pressed) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  return m_pActualMode->buttonRec(channel, pressed);
}

bool CCSManager::buttonRecDC(int channel, bool pressed) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  return m_pActualMode->buttonRecDC(channel, pressed);
}


bool CCSManager::buttonSelect(int channel, bool pressed) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  pressed ? m_iNumSelectButtonsPressed++ : m_iNumSelectButtonsPressed--;
  return m_pActualMode->buttonSelect(channel, pressed);
}

bool CCSManager::buttonSelectDC(int channel) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  return m_pActualMode->buttonSelectDC(channel);
}

bool CCSManager::buttonSelectLong(int channel) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  return m_pActualMode->buttonSelectLong(channel);
}


bool CCSManager::buttonMute(int channel, bool pressed) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  pressed ? m_iNumMuteButtonsPressed++ : m_iNumMuteButtonsPressed--;
  return m_pActualMode->buttonMute(channel, pressed);
}

bool CCSManager::buttonSolo(int channel, bool pressed) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  pressed ? m_iNumSoloButtonsPressed++ : m_iNumSoloButtonsPressed--;
  return m_pActualMode->buttonSolo(channel, pressed);
}

bool CCSManager::buttonSoloDC(int channel) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;
  return m_pActualMode->buttonSoloDC(channel);
}

bool CCSManager::fader(int channel, int value) {
  ASSERT(channel >= 0 && channel <= 8);
  m_switchBack = true;

  if (m_pMCU->IsFlagSet(CONFIG_FLAG_FADER_TOUCH_FAKE)) {
    m_faderTouchedTill[channel] = m_lastTime + TOUCHED_MS;
    elementTouched(FADER, channel, true);
    m_pActualMode->faderTouched(channel, true);
  }

  return m_pActualMode->fader(channel, value);
}

bool CCSManager::faderTouched(int channel, bool touched) {
  ASSERT(channel >= 0 && channel <= 8);
  m_switchBack = true;

  if (m_pMCU->IsFlagSet(CONFIG_FLAG_FADER_TOUCH_FAKE)) {
    return true;
  }

  elementTouched(FADER, channel, touched);
  return m_pActualMode->faderTouched(channel, touched);
}

bool CCSManager::vpotMoved(int channel, int numSteps) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;

  m_vpotTouchedTill[channel] = m_lastTime + TOUCHED_MS;

  elementTouched(VPOT, channel, true);

  if (m_optionActive) {
    m_optionActive->move(channel, numSteps);
    return true;
  }

  if (!m_selectorActive) {
    return m_pActualMode->vpotMoved(channel, numSteps);
  } 

  return false;
}

bool CCSManager::vpotPressed(int channel, bool pressed) {
  ASSERT(channel >= 1 && channel <= 8);
  m_switchBack = true;


  // always send touched, but at a end time if VPOT was released
  elementTouched(VPOT, channel, true);
  if (pressed) {
    m_vpotTouchedTill[channel] = 0;
  } else {
    m_vpotTouchedTill[channel] = m_lastTime + TOUCHED_MS;
  }

  m_pVPOTS[channel].setPressed(pressed);

  if (m_selectorActive) {
    if (!pressed) {
      if (!m_pActualMode->getSelector()->select(channel - 1)) {
        m_pActualMode->activate();
        m_selectorActive = false;
      }
    }
    return true;
  }

  if (m_optionActive && pressed) {
    m_optionActive->select(channel - 1);
    return true;
  }

  return m_pActualMode->vpotPressed(channel, pressed);
}

void CCSManager::elementTouched(EElement element, int channel, bool touched) {
  bool* touchedArray = (element == FADER) ? m_faderTouched : m_vpotTouched;

  ASSERT(channel >= 0 && channel <= 8);
  if (touched) {
    if (getElementsTouched() == 0) {
      touchedArray[channel] = touched;
      m_pActualMode->somethingTouched(true);
    }
  }
  else {
    if (getElementsTouched() == 1) {
      touchedArray[channel] = touched;
      m_pActualMode->somethingTouched(false);
    }
  }

  touchedArray[channel] = touched;

  if (element == FADER) {
    if (getNumFadersTouched() == 1) {
      for (int iChannel = 1; iChannel < 9; iChannel++) 
        if (touchedArray[iChannel]) 
          m_pActualMode->singleFaderTouched(iChannel);
    } else {
      m_pActualMode->singleFaderTouched(0);
    }
  } else {
    if (getNumVPotTouched() == 1) {
      for (int iChannel = 1; iChannel < 9; iChannel++) 
        if (touchedArray[iChannel]) 
          m_pActualMode->singleVPotTouched(iChannel);
    } else {
      m_pActualMode->singleVPotTouched(0);
    }
  }


//  if (getElementsTouched() == 1) {
//    for (int iChannel = 1; iChannel < 9; iChannel++) 
//      if (touchedArray[iChannel]) 
//        (element == FADER) ? m_pActualMode->singleFaderTouched(iChannel) : m_pActualMode->singleVPotTouched(iChannel);
//  } else {
//    (element == FADER) ? m_pActualMode->singleFaderTouched(0) : m_pActualMode->singleVPotTouched(0);
//  }
}

void CCSManager::updateEverything() {
  m_pActualMode->updateFaders();
  updateAllLEDs();
  m_pActualMode->updateVPOTs();
  m_pActualMode->updateAssignmentDisplay();
  m_pActualMode->updateDisplay();
}

void CCSManager::updateFader() {
  m_pActualMode->updateFaders();
}

void CCSManager::updateVPOTs() {
  m_pActualMode->updateVPOTs();
}

void CCSManager::updateAllLEDs() {
  m_pActualMode->updateFlipLED();
  m_pActualMode->updateGlobalViewLED();
  m_pActualMode->updateMuteLEDs();
  m_pActualMode->updateRecLEDs();
  m_pActualMode->updateSelectLEDs();
  m_pActualMode->updateSoloLEDs();
}

void CCSManager::updateFlipLED() {
  m_pActualMode->updateFlipLED();
}

void CCSManager::updateGlobalViewLED() {
  m_pActualMode->updateGlobalViewLED();
}

void CCSManager::updateMuteLEDs() {
  m_pActualMode->updateMuteLEDs();
}

void CCSManager::updateRecLEDs() {
  m_pActualMode->updateRecLEDs();
}

void CCSManager::updateSelectLEDs() {
  m_pActualMode->updateSelectLEDs();  
}

void CCSManager::updateSoloLEDs() {
  m_pActualMode->updateSoloLEDs();
}

void CCSManager::updateAssignmentDisplay() {
  m_pActualMode->updateAssignmentDisplay();
}

void CCSManager::setFader(CCSMode* pCaller, int channel, int value) {
  CHECKMODEANDCHANNEL

  if (m_faderPos[channel] != value) {
    if (channel == 0) {
      m_pMCU->SendMidi(0xe8,value&0x7f,(value>>7)&0x7f,-1);
    } else {
      m_pMCU->SendMidi(0xdf + channel,value&0x7f,(value>>7)&0x7f,-1);
    }
    m_faderPos[channel] = value;
  }
}

void CCSManager::setRecLED(CCSMode* pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL

  if (m_stateRec[channel] != state) {
    m_pMCU->SetLED(channel - 1, state);
    m_stateRec[channel] = state;
  }
}

void CCSManager::setSoloLED(CCSMode* pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL

  if (m_stateSolo[channel] != state) {
    m_pMCU->SetLED(0x07 + channel, state);
    m_stateSolo[channel] = state;
  }
}

void CCSManager::setMuteLED(CCSMode* pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL

  if (m_stateMute[channel] != state) {
    m_pMCU->SetLED(0x0F + channel, state);
    m_stateMute[channel] = state;
  }
}

void CCSManager::setSelectLED(CCSMode* pCaller, int channel, int state) {
  CHECKMODEANDCHANNEL

  if (m_stateSelect[channel] != state) {
    m_pMCU->SetLED(0x17 + channel, state);
    m_stateSelect[channel] = state;
  }
}

void CCSManager::setFlipLED(CCSMode* pCaller, int state) {
  CHECKMODE

  if (m_stateFlip != state) {
    m_pMCU->SetLED(B_FLIP, state ? LED_ON : LED_OFF);
    m_stateFlip = state;
  }
}

void CCSManager::setGlobalViewLED(CCSMode* pCaller, int state) {
  CHECKMODE

  if (m_stateGlobalView != state) {
    m_pMCU->SetLED(B_GLOBAL_VIEW, state ? LED_ON : LED_OFF);
    m_stateGlobalView = state;
  }
}

void CCSManager::setAssignmentDisplay(CCSMode* pCaller, char text[2]) {
  CHECKMODE

  if (memcmp(text, m_stateAssignmentDisplay, 2) != 0) {
    if (!m_pMCU->IsExtender()) {
      m_pMCU->SendMidi(0xB0,0x40+11,text[0],-1);
      m_pMCU->SendMidi(0xB0,0x40+10,text[1],-1);
    }
  }
}

void CCSManager::trackListChange() {
  m_pActualMode->trackListChange();
}

bool CCSManager::isSelectedTrack(MediaTrack* tr) {
  SelectedTrack *i = m_pMCU->GetSelectedTracks();
  while(i) {
    MediaTrack *track = i->track();
    if (track == tr) 
      return true;
    i = i->next;
  }
  return false;
}

void CCSManager::trackSelected(MediaTrack *trackid, bool selected) {
  if ( selected ) 
    selectTrack(trackid);
  else
    deselectTrack(trackid);

  m_pMCU->UpdateAutoModes();
}

void CCSManager::selectTrack( MediaTrack *trackid ) {
  const GUID *guid = GetTrackGUID(trackid);

  // Empty list, start new list
  SelectedTrack* pTracks = m_pMCU->GetSelectedTracks();
  if ( pTracks == NULL ) {
    pTracks = new SelectedTrack(trackid);
    m_pMCU->SetSelectedTracks(pTracks);
    return;
  }

  // This track is head of list
  if ( guid && !memcmp(&pTracks->guid,guid,sizeof(GUID)) )
    return;

  // Scan for track already selected
  SelectedTrack *i = pTracks;
  while ( i->next ) {
    i = i->next;
    if ( guid && !memcmp(&i->guid,guid,sizeof(GUID)) )
      return;
  }

  // Append at end of list if not already selected
  i->next = new SelectedTrack(trackid);
}

void CCSManager::deselectTrack( MediaTrack *trackid ) {
  const GUID *guid = GetTrackGUID(trackid);

  // Empty list?
  SelectedTrack* pTracks = m_pMCU->GetSelectedTracks();
  if ( pTracks ) {
    // This track is head of list?
    if ( guid && !memcmp(&pTracks->guid,guid,sizeof(GUID)) ) {
      SelectedTrack *tmp = pTracks;
      pTracks = pTracks->next;
      m_pMCU->SetSelectedTracks(pTracks);
      delete tmp;
    }

    // Search for this track
    else {
      SelectedTrack *i = pTracks;
      while( i->next ) {
        if ( guid && !memcmp(&i->next->guid,guid,sizeof(GUID)) ) {
          SelectedTrack *tmp = i->next;
          i->next = i->next->next;
          delete tmp;
          break;
        }
        i = i->next;
      }
    }
  }
}


void CCSManager::trackName(MediaTrack* trackid, const char* pName) {
  m_pActualMode->trackName(trackid, pName);
}

DisplayHandler* CCSManager::getDisplayHandler() {
  return m_pMCU->GetDisplayHandler();
}

int CCSManager::getElementsTouched() {
  return getNumFadersTouched() + getNumVPotTouched();
}

int CCSManager::getNumFadersTouched() {
  return getNumTrueArrayEntries(m_faderTouched, 9);
}

int CCSManager::getNumVPotTouched() {
  return getNumTrueArrayEntries(m_vpotTouched, 9);
}

int CCSManager::getNumTrueArrayEntries(bool* pArray, int size) {
  int numTrue = 0;
  for (int i = 0; i < size; i++) {
    if (pArray[i]) {
      numTrue++;
    }
  }

  return numTrue;
}

void CCSManager::frameUpdate(DWORD time) {
  m_lastTime = time;

  for (int i = 1; i < 9; i++) {
    if (m_vpotTouchedTill[i] > 0 && time > m_vpotTouchedTill[i]) {
      elementTouched(VPOT, i, false);
      m_vpotTouchedTill[i] = 0;
    }
  }

  if (m_pMCU->IsFlagSet(CONFIG_FLAG_FADER_TOUCH_FAKE)) {
    for (int i = 1; i < 9; i++) {
      if (m_faderTouchedTill[i] > 0 && time > m_faderTouchedTill[i]) {
        elementTouched(FADER, i, false);
        m_faderTouchedTill[i] = 0;
        m_pActualMode->faderTouched(i, false);  
      }
    }
  }

  checkOption();

  updateVPOTLeds();
  
  m_pActualMode->frameUpdate();
}

void CCSManager::checkOption() {
  if (m_optionActive != NULL && m_pMCU->IsModifierPressed(VK_OPTION) || m_pMCU->IsButtonPressed(B_NAME_VALUE))
    return;


  if (m_pMCU->IsModifierPressed(VK_OPTION)) {
    if (m_pMCU->IsModifierPressed(VK_SHIFT)) {
      if (m_pActualMode->get2ndOptions() != NULL) {
        m_optionActive = m_pActualMode->get2ndOptions();
        m_optionActive->activateSelector();
      }
    } else {
      if (m_pActualMode->getOptions() != NULL) {
        m_optionActive = m_pActualMode->getOptions();
        m_optionActive->activateSelector();
      }
    }
  } else if (m_optionActive != NULL) {
    m_pActualMode->activate();
    m_optionActive = NULL;
  }
}

void CCSManager::setMode(EMode mode) {
  switch (mode) {
    case SEND:
      changeMode(m_pSendMode);
      break;
    case RECEIVE:
      changeMode(m_pReceiveMode);
      break;
    default:
      ASSERT_M(false, "direct mode change must be added for this mode");
      break;
  }
}

void CCSManager::changeMode(CCSMode* pNewMode) {
  if (pNewMode && pNewMode != m_pActualMode) {
    getDisplayHandler()->waitForMoreChanges(true);
    m_pActualMode->deactivate();
    m_pActualMode = pNewMode;
    m_pActualMode->activate();
    getDisplayHandler()->waitForMoreChanges(false);
  }
}

void CCSManager::closeEditorIfOpen(Component* pComponent)
{
  m_pEditor->closeWindowAndRemoveComponent(pComponent);
}
