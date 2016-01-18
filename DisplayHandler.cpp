/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "DisplayHandler.h"

#include "csurf_mcu.h"
#include "Assert.h"
#include "Display.h"

class MIDI_Message
{
public:
  MIDI_Message() {
    evt.frame_offset = 0;
    evt.size = 0;
    memset(data, 0, 512);
  }
  MIDI_event_t evt;
  char data[512];
};

DisplayHandler::DisplayHandler(CSurf_MCU* pMCU, EnumMCUType mcuType) 
{ 
  m_pMCU = pMCU; 
  m_pActualDisplay = NULL;
  m_mcuType = mcuType;  
  m_wait = false;
  for (int i = 0; i < 9; i++) {
    m_metersEnabled[i] = m_pMCU->IsFlagSet(CONFIG_FLAG_NO_LEVEL_METER); //enableMeter must be processed once
  } 

  m_pHardwareState = new Display(this, 2);
  char pInvalidText[55];
  memset(pInvalidText, 0x1, 55);
  m_pHardwareState->changeText(0,1,pInvalidText,55);
  m_pHardwareState->changeText(1,1,pInvalidText,55);
}

DisplayHandler::~DisplayHandler() {
  safe_delete(m_pHardwareState);
}
// void DisplayHandler::init(){ 
//  updateDisplay(m_pActualDisplay, 0, 0, "", DISPLAY_ROW_LENGTH);
//  updateDisplay(m_pActualDisplay, 0, 0, "", DISPLAY_ROW_LENGTH);
// }

void DisplayHandler::updateDisplay( Display* pDisplay, int row, int pos, const char *text, int pad, bool forceUpdate )
{
  if (pDisplay != m_pActualDisplay)
    return;

  if (m_pHardwareState->bufferIsEqualTo(row, pos, text, pad))// && !forceUpdate)
    return;
//  if (m_wait)
//    return;

  m_pHardwareState->changeText(row, pos, text, pad);

  ASSERT(row < 2); // support for C4 is missing at the moment
  if (row == 1)
    pos += DISPLAY_ROW_LENGTH + row; // + row because there is one unused byte at the end of each row

  MIDI_Message mm;
  addHeader(&mm);
  //  F0 00 00 66 14 12 xx <data> F7   : update LCD. xx=offset (0-112), string. display is 55 chars wide, second line begins at 56, though.

  //  mm.evt.frame_offset=0;
  //  mm.evt.size=0;

  mm.evt.midi_message[mm.evt.size++]=0x12;
  mm.evt.midi_message[mm.evt.size++]=pos;
  int l=strnlen(text, 55);
  if (pad<l)l=pad;
  if (l > 200)l=200;

  int cnt=0;
  while (cnt < l)
  {
    mm.evt.midi_message[mm.evt.size++]=*text++;
    cnt++;
  }
  while (cnt++<pad)  mm.evt.midi_message[mm.evt.size++]=' ';
  mm.evt.midi_message[mm.evt.size++]=0xF7;
  m_pMCU->SendMsg(&mm.evt,-1);
//  Sleep(5);
}

void DisplayHandler::switchTo( Display* pDisplay )
{
  if (m_pActualDisplay == pDisplay)
    return;

  enableMeter(pDisplay->hasMeter());

  if (m_mcuType == MCU_EX && !pDisplay->onlyOnMainUnit() || m_mcuType != MCU_EX) {
    m_pActualDisplay = pDisplay;
    pDisplay->activate();
  }
}

void DisplayHandler::enableMeter( int channel, bool enable ) // channel is 1 based
{
  ASSERT(channel > 0 && channel <= 9);

  if (m_pMCU->IsFlagSet(CONFIG_FLAG_NO_LEVEL_METER)) {
    enable = false;
  }

  if (enable == m_metersEnabled[channel])
    return;
  m_metersEnabled[channel] = enable;
  //  F0 00 00 66 14 20 0x 03 F7       : put track in VU meter mode, x=track  
  MIDI_Message mm;

  addHeader(&mm);

  mm.evt.midi_message[mm.evt.size++]=0x20;
  mm.evt.midi_message[mm.evt.size++]=0x00+channel-1;
  mm.evt.midi_message[mm.evt.size++]=enable ? 0x03 : 0x01;
  mm.evt.midi_message[mm.evt.size++]=0xF7;
  m_pMCU->SendMsg(&mm.evt,-1);
//  Sleep(50);
  //  D0 yx    : update VU meter, y=track, x=0..d=volume, e=clip on, f=clip off
//  if (enable) {
//    m_pMCU->SendMidi(0xD0,((channel-1)<<4)|0xF,0,-1);
//    Sleep(5);
//  }
}

void DisplayHandler::enableMeter( bool enable )
{
  for (int i = 1; i < 9; i++) {
    enableMeter(i, enable);
  }
}

void DisplayHandler::addHeader( MIDI_Message* pmm )
{
  //  F0 00 00 66 xx  
  pmm->evt.midi_message[pmm->evt.size++]=0xF0;
  pmm->evt.midi_message[pmm->evt.size++]=0x00;
  pmm->evt.midi_message[pmm->evt.size++]=0x00;
  pmm->evt.midi_message[pmm->evt.size++]=0x66;
  switch (m_mcuType) {
case MCU:
  pmm->evt.midi_message[pmm->evt.size++]=0x14;
  break;
case MCU_EX:
  pmm->evt.midi_message[pmm->evt.size++]=0x15;
  break;
case MCU_C4:
  ASSERT(false); // not implemented
  break;
  }
}

void DisplayHandler::waitForMoreChanges(bool block) {
  if (m_wait == block)
    return; 

  if (block) {
    m_wait = true;
  } else {
    m_wait = false;
    safe_call(m_pActualDisplay, resendAllRows());
  }
}
