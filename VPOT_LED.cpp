/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "VPOT_LED.h"
#include "csurf_mcu.h"
#include "Assert.h"

VPOT_LED::VPOT_LED() : 
  m_value(0),
  m_mode(SINGLE),
  m_bottom(false),
  m_track(-1),
  m_pMCU(NULL),
  m_pressed(false),
  m_mustUpdate(false)
{
}

VPOT_LED::~VPOT_LED(void)
{
}

void VPOT_LED::updateLEDs()
{
  if (!m_mustUpdate)
    return;
// LED Kraenze:
// CC Kanal 1
// 0x20 + nr-1 fuer Position
// Drittes Byte Anzeige
// Byte 1-4: Wert
// Byte 5-6: Art
// Byte 7: Punkt unten  
  if (m_mode == OFF) {
    if (m_track > 0) { // m_track == 0 exist only to have also the VPOTs 1 based (with m_track 0 for the master channel, but this has no VPOT)
      m_pMCU->SendMidi(0xB0, 0x2F + m_track, m_bottom ? 1 << 6 : 0, -1);
      m_mustUpdate = false;
      return;
    }
  }

  int byte3 = m_bottom ? 1 << 6 : 0;
  switch (m_mode) {
    case FROM_LEFT:
      byte3 += 2 << 4;
      break;
    case FROM_MIDDLE_POINT:
      byte3 += 1 << 4;
      break;
  }
  if (m_track > 0) { // m_track == 0 exist only to have also the VPOTs 1 based (with m_track 0 for the master channel, but this has no VPOT)
    m_pMCU->SendMidi(0xB0, 0x2F + m_track, byte3 + m_value, -1);
  }

  m_mustUpdate = false;
}

void VPOT_LED::init( CSurf_MCU* pMCU, int track )
{
  m_pMCU = pMCU;
  m_track = track;
}

void VPOT_LED::setMode(const MODE mode) {
  changeState(&m_mode, mode);
  updateLEDs();
}

void VPOT_LED::setValue(const int value) { 
  ASSERT(value >= 0 && value <= 11);
  changeState(&m_value, value);
  updateLEDs();
}


void VPOT_LED::setBottom(const bool bot) {
  changeState(&m_bottom, bot);
  updateLEDs();
}

void VPOT_LED::setAll(const MODE mode, const int value, const bool bot) {
  ASSERT(value >= 0 && value <= 15);
  changeState(&m_mode, mode);
  changeState(&m_value, value);
  changeState(&m_bottom, bot);
  updateLEDs();
}

template<class T>
void VPOT_LED::changeState(T* pDest, const T source) {
  if (*pDest != source) {
    *pDest = source;
    m_mustUpdate = true;
  }
}

