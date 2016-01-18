/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "DisplayTrackMeter.h"
#include "csurf_mcu.h"
#include "Assert.h"
#include "Tracks.h"

DisplayTrackMeter::DisplayTrackMeter( DisplayHandler* pDisplayHandler, int numRows ) :
Display(pDisplayHandler, numRows)
{
  int x;
  for (x = 0; x < sizeof(m_mcu_meterpos)/sizeof(m_mcu_meterpos[0]); x ++)
    m_mcu_meterpos[x]=-100000.0;
  m_mcu_meter_lastrun=0;
}

void DisplayTrackMeter::updateTrackMeter(DWORD now)
{
  if (m_pDisplayHandler->getMCU()->IsFlagSet(CONFIG_FLAG_NO_LEVEL_METER))
    return;
  // 0xD0 = level meter, hi nibble = channel index, low = level (F=clip, E=top)
  int x;
  bool somethingSoloed = m_pDisplayHandler->getMCU()->SomethingSoloed();
#define VU_BOTTOM 70
  double decay=0.0;
  if (m_mcu_meter_lastrun) 
  {
    decay=VU_BOTTOM * (double) (now-m_mcu_meter_lastrun)/(1.4*1000.0);            // they claim 1.8s for falloff but we'll underestimate
  }
  m_mcu_meter_lastrun=now;
  for (x = 1; x < 9; x ++)
  {
    MediaTrack *t;

    if (t=Tracks::instance()->getMediaTrackForChannel(x))
    {
      // check mute/solo state of track(s), maybe the signal is muted
      bool isPlaying = true;
      if (somethingSoloed ) {
        int* soloState = (int*) GetSetMediaTrackInfo(t, "I_SOLO", NULL);
        if (*soloState == 0)
          isPlaying = false;
      }
      bool* muteState = (bool*) GetSetMediaTrackInfo(t, "B_MUTE", NULL);
      if(*muteState) {
        isPlaying = false;
      }

      int v=0x0;
      if (isPlaying) {
        v=0xd; // 0xe turns on clip indicator, 0xf turns it off
        // get peak
        double pp=VAL2DB((Track_GetPeakInfo(t,0)+Track_GetPeakInfo(t,1)) * 0.5);

        if (m_mcu_meterpos[x-1] > -VU_BOTTOM*2) m_mcu_meterpos[x-1] -= decay;

        if (pp < m_mcu_meterpos[x-1]) continue;
        m_mcu_meterpos[x-1]=pp;
        if (pp < 0.0)
        {
          if (pp <= -VU_BOTTOM)
            v=0x0;
          else v=(int) ((pp+VU_BOTTOM)*13.0/VU_BOTTOM);
        }
      } 
      
      m_pDisplayHandler->getMCU()->SendMidi(0xD0,((x-1)<<4)|v,0,-1);
    }
  }
}

void DisplayTrackMeter::changeText(int row, int pos, const char *text, int pad, bool updateDisplay) {
  ASSERT(row < m_numRows);

//  if (updateDisplay)
//    m_pDisplayHandler->updateDisplay(this, row, pos, text, pad);

  writeToBuffer(row, pos, text, pad);

  if (m_ppForwardToDisplay[row])
    m_ppForwardToDisplay[row]->changeText(m_pForwardToRow[row], pos, text, pad);
}

void DisplayTrackMeter::changeField(int row, int field, const char* pText) {
  ASSERT(field > 0 && field < 9);

  char pShortText[7];
  memset(pShortText, ' ', 7);
  strncpy(pShortText, pText, 6);

  changeText(row, (field-1) * 7, pShortText, 7, !m_pDisplayHandler->getMetersEnabled(field) || row == 0);
}
