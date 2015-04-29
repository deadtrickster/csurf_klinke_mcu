/*
** reaper_csurf
** MCU support
** Copyright (C) 2006-2008 Cockos Incorporated
** License: LGPL.
*/

#include "csurf_mcu.h"
#include "math.h"
#include "Transport.h"
#include "Region.h"
#include "Assert.h"
#include "DisplayHandler.h"
#include "DisplayTrackMeter.h"
#include "multitrackmode.h"
#include "UndoEnd.h"
#include "tracks.h"
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include "boost/bind.hpp"
#include "ProjectConfig.h"
#include "PlugMoveWatcher.h"
#include "ButtonManager.h"
#include "ActionsDisplay.h"
#include "ActionsDialogComponent.h"
#include "CCSModesEditor.h"
#include "Actions.h"

/*
MCU documentation:

MCU=>PC:
  The MCU seems to send, when it boots (or is reset) F0 00 00 66 14 01 58 59 5A 57 18 61 05 57 18 61 05 F7

  Ex vv vv    :   volume fader move, x=0..7, 8=master, vv vv is int14
  B0 1x vv    :   pan fader move, x=0..7, vv has 40 set if negative, low bits 0-31 are move amount
  B0 3C vv    :   jog wheel move, 01 or 41

  to the extent the buttons below have LEDs, you can set them by sending these messages, with 7f for on, 1 for blink, 0 for off.
  90 0x vv    :   rec arm push x=0..7 (vv:..)
  90 0x vv    :   solo push x=8..F (vv:..)
  90 1x vv    :   mute push x=0..7 (vv:..)
  90 1x vv    :   selected push x=8..F (vv:..)
  90 2x vv    :   pan knob push, x=0..7 (vv:..)
  90 28 vv    :   assignment track
  90 29 vv    :   assignment send
  90 2A vv    :   assignment pan/surround
  90 2B vv    :   assignment plug-in
  90 2C vv    :   assignment EQ
  90 2D vv    :   assignment instrument
  90 2E vv    :   bank down button (vv: 00=release, 7f=push)
  90 2F vv    :   channel down button (vv: ..)
  90 30 vv    :   bank up button (vv:..)
  90 31 vv    :   channel up button (vv:..)
  90 32 vv    :   flip button
  90 33 vv    :   global view button
  90 34 vv    :   name/value display button
  90 35 vv    :   smpte/beats mode switch (vv:..)
  90 36 vv    :   F1
  90 37 vv    :   F2
  90 38 vv    :   F3
  90 39 vv    :   F4
  90 3A vv    :   F5
  90 3B vv    :   F6
  90 3C vv    :   F7
  90 3D vv    :   F8
  90 3E vv    :   Global View : midi tracks
  90 3F vv    :   Global View : inputs
  90 40 vv    :   Global View : audio tracks
  90 41 vv    :   Global View : audio instrument
  90 42 vv    :   Global View : aux
  90 43 vv    :   Global View : busses
  90 44 vv    :   Global View : outputs
  90 45 vv    :   Global View : user
  90 46 vv    :   shift modifier (vv:..)
  90 47 vv    :   option modifier
  90 48 vv    :   control modifier
  90 49 vv    :   alt modifier
  90 4A vv    :   automation read/off
  90 4B vv    :   automation write
  90 4C vv    :   automation trim
  90 4D vv    :   automation touch
  90 4E vv    :   automation latch
  90 4F vv    :   automation group
  90 50 vv    :   utilities save
  90 51 vv    :   utilities undo
  90 52 vv    :   utilities cancel
  90 53 vv    :   utilities enter
  90 54 vv    :   marker
  90 55 vv    :   nudge
  90 56 vv    :   cycle
  90 57 vv    :   drop
  90 58 vv    :   replace
  90 59 vv    :   click
  90 5a vv    :   solo
  90 5b vv    :   transport rewind (vv:..)
  90 5c vv    :   transport ffwd (vv:..)
  90 5d vv    :   transport pause (vv:..)
  90 5e vv    :   transport play (vv:..)
  90 5f vv    :   transport record (vv:..)
  90 60 vv    :   up arrow button  (vv:..)
  90 61 vv    :   down arrow button 1 (vv:..)
  90 62 vv    :   left arrow button 1 (vv:..)
  90 63 vv    :   right arrow button 1 (vv:..)
  90 64 vv    :   zoom button (vv:..)
  90 65 vv    :   scrub button (vv:..)

  90 6x vv    :   fader touch x=8..f
  90 70 vv    :   master fader touch

PC=>MCU:

  F0 00 00 66 14 12 xx <data> F7   : update LCD. xx=offset (0-112), string. display is 55 chars wide, second line begins at 56, though.
  F0 00 00 66 14 08 00 F7          : reset MCU
  F0 00 00 66 14 20 0x 03 F7       : put track in VU meter mode, x=track  

  90 73 vv : rude solo light (vv: 7f=on, 00=off, 01=blink)

  B0 3x vv : pan display, x=0..7, vv=1..17 (hex) or so
  B0 4x vv : right to left of LEDs. if 0x40 set in vv, dot below char is set (x=0..11)

  D0 yx    : update VU meter, y=track, x=0..d=volume, e=clip on, f=clip off
  Ex vv vv : set volume fader, x=track index, 8=master


*/

#define SPLASH_MESSAGE "REAPER! Initializing... Please wait..."

int CSurf_MCU::s_iNumInstances = 0;

int CSurf_MCU::s_mackie_modifiers = 0;

int CSurf_MCU::s_cfg_flags = 0;

static double timeToBeats(double tpos, int *measures, int *cml, double *fullbeats, int *cdenom) {
  if (TimeMap_timeToBeats != NULL)
    return TimeMap_timeToBeats(tpos, measures, cml, fullbeats, cdenom);
  else
    return TimeMap2_timeToBeats(NULL, tpos, measures, cml, fullbeats, cdenom);
}
/*
static unsigned int get_midi_evt_code( MIDI_event_t *evt ) {
  unsigned int code = 0;
  code |= (evt->midi_message[0]<<24);
  code |= (evt->midi_message[1]<<16);
  code |= (evt->midi_message[2]<<8);
  code |= evt->size > 3 ? evt->midi_message[3] : 0;
  return code;
}
*/
int CSurf_MCU::FindTrackNr( MediaTrack* tr ) {
  int iNr = 1;
  for ( TrackIterator ti; !ti.end(); ++ti, ++ iNr ) {
    if ( *ti == tr )
      return iNr;
  }
  return 0;
}

const char* CSurf_MCU::GetTrackName( MediaTrack* tr) {
  if (!tr) {
    memset(trackName, ' ', 4);
    return trackName;
  }

  const char* pTrackName = (char*) (*GetSetMediaTrackInfo)(tr, "P_NAME", NULL);
  if (pTrackName != NULL && strnlen(pTrackName, 55) > 0)
    return pTrackName;

//  char buf[32];
  int trackno = m_pCCSManager->getMCU()->FindTrackNr(tr);
  if ( trackno < 100 ) 
    sprintf( trackName, "%02d", trackno );
  else
    sprintf( trackName, "%d", trackno );
  return trackName;
}

MediaTrack* CSurf_MCU::TrackFromGUID( const GUID &guid ) {
  for ( TrackIterator ti; !ti.end(); ++ti ) {
    MediaTrack *tr = *ti;
    const GUID *tguid=GetTrackGUID(tr);
    
    if (tr && tguid && !memcmp(tguid,&guid,sizeof(GUID)))
      return tr;
  }
  return NULL;
}

bool CSurf_MCU::SomethingSoloed() {
  for ( TrackIterator ti; !ti.end(); ++ti ) {
    MediaTrack *tr = *ti;
    int* OriginalState = (int*) GetSetMediaTrackInfo(tr, "I_SOLO", NULL);
    if (*OriginalState > 0) 
      return true;
  }

  return false;
}

void CSurf_MCU::ScheduleAction( DWORD time, ScheduleFunc func ) {
  ScheduledAction *action = new ScheduledAction( time, func );
  if ( m_schedule == NULL ) {
    m_schedule = action;
  }
  else if ( action->time < m_schedule->time ) {
    action->next = m_schedule;
    m_schedule = action;
  }
  else {
    ScheduledAction *curr = m_schedule;
    while( curr->next != NULL && curr->next->time < action->time )
      curr = curr->next;
    action->next = curr->next;
    curr->next = action;
  }
}
  
void CSurf_MCU::MCUReset()
{
  m_pButtonManager->reset();
  memset(m_mackie_lasttime,0,sizeof(m_mackie_lasttime));
  m_mackie_lasttime_mode=-1;
  s_mackie_modifiers=0;
  m_buttonstate_lastrun=0;
  m_button_states=0;
  m_mackie_arrow_states=0;

  m_dropstate.updateReaper();
  
  // code from Justin, i don't really understand what this is doing
  int sz;
  m_metronom_offset = projectconfig_var_getoffs("projmetroen", &sz); // this can be done once, and stored (for speed)
  if (sz != 4)
    m_metronom_offset = 0;

  if (m_midiout)
  {
    if (!m_is_mcuex)
    {
      m_dropstate.updateMCU(m_is_mcuex, m_midiout);

      m_pCCSManager->updateFlipLED();
      m_pCCSManager->updateGlobalViewLED();

      m_midiout->Send(0x90, 0x64,(m_mackie_arrow_states&ARROW_STATE_ZOOM)?0x7f:0,-1);
      m_midiout->Send(0x90, 0x65,(m_mackie_arrow_states&ARROW_STATE_SCRUB)?0x7f:0,-1);

      m_midiout->Send(0xB0,0x40+11,'0'+(((Tracks::instance()->getGlobalOffset()+1)/10)%10),-1);
      m_midiout->Send(0xB0,0x40+10,'0'+((Tracks::instance()->getGlobalOffset()+1)%10),-1);
    }

    m_pSplashDisplay->changeTextFullLine(0, SPLASH_MESSAGE);
    m_pSplashDisplay->clearLine(1);
    m_pDisplayHandler->switchTo(m_pSplashDisplay);
  }
}

void CSurf_MCU::CallTransportForward() {
  if (timeGetTime() - m_pTransport->getFfwdPressTime() > STARTING_REPEAT_TIME)
    m_pTransport->forward();
}

void CSurf_MCU::CallTransportRewind() {
  if (timeGetTime() - m_pTransport->getRewindPressTime() > STARTING_REPEAT_TIME)
    m_pTransport->rewind();
}

bool CSurf_MCU::OnMCUReset(MIDI_event_t *evt) {
  unsigned char onResetMsg[]={0xf0,0x00,0x00,0x66,0x14,0x01,0x58,0x59,0x5a,};
  onResetMsg[4]=m_is_mcuex ? 0x15 : 0x14; 
  if (evt->midi_message[0]==0xf0 && evt->size >= sizeof(onResetMsg) && !memcmp(evt->midi_message,onResetMsg,sizeof(onResetMsg)))
  {
    // on reset
    MCUReset();
    TrackList_UpdateAllExternalSurfaces();
    return true;
  }
  return false;
}

bool CSurf_MCU::OnFaderMove(MIDI_event_t *evt) {
  if ((evt->midi_message[0]&0xf0) == 0xe0) {// volume fader move
    int tid = evt->midi_message[0] & 0xf;
    if (tid == 8) 
      tid = 0; 
    else 
      tid++;

    return m_pCCSManager->fader(tid, msbLsbToInt(evt->midi_message[2], evt->midi_message[1]));
  } 
  return false;
}

bool CSurf_MCU::OnRotaryEncoder( MIDI_event_t *evt ) {
  if ( (evt->midi_message[0]&0xf0) == 0xb0 && 
      evt->midi_message[1] >= 0x10 && 
      evt->midi_message[1] < 0x18 ) { // pan
    int tid=evt->midi_message[1]-0x10;

    m_pan_lasttouch[Tracks::instance()->getMediaTrackForChannel(tid+1)]=timeGetTime();

    int adj=(evt->midi_message[2]&0x3f);
    if (evt->midi_message[2]&0x40) 
      adj=-adj;
    
    return m_pCCSManager->vpotMoved(tid + 1, adj);
  }
  return false;
}

bool CSurf_MCU::OnVPOTAssign(MIDI_event_t *evt) {
  return  m_pCCSManager->buttonVPOTassign(evt->midi_message[1], evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnJogWheel( MIDI_event_t *evt ) {
  if ( (evt->midi_message[0]&0xf0) == 0xb0 &&
        evt->midi_message[1] == 0x3c ) // jog wheel
  {
    int dir;
    if (evt->midi_message[2] == 0x41) { 
      dir = -1;
      if (IsNoModifierPressed()) {
        CSurf_OnRew(m_mackie_arrow_states&ARROW_STATE_SCRUB);
        return true;
      } 
    }
    else  if (evt->midi_message[2] == 0x01) {
      dir = 1;
      if (IsNoModifierPressed()) {
        CSurf_OnFwd(m_mackie_arrow_states&ARROW_STATE_SCRUB);
        return true;
      }
    }

    if (IsModifierPressed(VK_SHIFT) || IsModifierPressed(VK_OPTION)) {
      m_region.GetFromActualRegion(false);
      if (!Region::IsActive(Region::TIME)) {
        m_region.SetStart(::GetCursorPosition());
        m_region.SetEnd(::GetCursorPosition());
      }
      if (IsModifierPressed(VK_SHIFT)) {
        m_region.SetStart(CalcMovement(m_region.GetStart(), dir));
      }
      if (IsModifierPressed(VK_OPTION)) {
        m_region.SetEnd(CalcMovement(m_region.GetEnd(), dir));
      }
      m_region.Set(false);
    }

    if (IsModifierPressed(VK_CONTROL) || IsModifierPressed(VK_ALT)) {
      m_region.GetFromActualRegion(true);
      if (!Region::IsActive(Region::LOOP)) {
        m_region.SetStart(::GetCursorPosition());
        m_region.SetEnd(::GetCursorPosition());
      }
      if (IsModifierPressed(VK_CONTROL)) {
        m_region.SetStart(CalcMovement(m_region.GetStart(), dir));
      }
      if (IsModifierPressed(VK_ALT)) {
        m_region.SetEnd(CalcMovement(m_region.GetEnd(), dir));
        if ((GetPlayState() == 1) && (m_region.GetEnd() < GetPlayPosition())) {
          CSurf_OnStop();
          SetEditCurPos(m_region.GetStart(), false, false);
          CSurf_OnPlay();
        }
      }
      m_region.Set(true);
    }

    return true;
  }
  return false;
}

double CSurf_MCU::CalcMovement(double oldPos, int dir) {
  double bpm, bpi;
  GetProjectTimeSignature(&bpm, &bpi);
  double pixelPerBeat = GetHZoomLevel() * 60 / bpm;

  if (m_mackie_arrow_states&ARROW_STATE_SCRUB) {
    return oldPos + (1 / GetHZoomLevel()) * dir * 2;
  } else {
    if (pixelPerBeat < 20) {
      return MoveInBars(oldPos, dir);
    } else {
      return MoveInBeats(oldPos, dir);
    }
  }
}

bool CSurf_MCU::OnAutoMode( MIDI_event_t *evt ) {
  int mode=-1;
  int a=evt->midi_message[1]-0x4a;
  if (!a) mode=AUTO_MODE_READ;
  else if (a==1) mode=AUTO_MODE_WRITE;
  else if (a==2) mode=AUTO_MODE_TRIM;
  else if (a==3) mode=AUTO_MODE_TOUCH;
  else if (a==4) mode=AUTO_MODE_LATCH;

  if (mode>=0)
    SetAutomationMode(mode,!IsModifierPressed(VK_CONTROL));

  return true;
}

bool CSurf_MCU::OnBankChannel( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonFaderBanks(evt->midi_message[1], evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnSMPTEBeats( MIDI_event_t *evt ) {
  int *tmodeptr = (int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode2);
  if (tmodeptr && *tmodeptr>=0)
  {
    (*tmodeptr)++;
    if ((*tmodeptr)>5)
      (*tmodeptr)=0;
  }
  else
  {
    tmodeptr = (int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode);

    if (tmodeptr)
    {
      (*tmodeptr)++;
      if ((*tmodeptr)>5)
        (*tmodeptr)=0;
    }
  }
  UpdateTimeline();
  Main_UpdateLoopInfo(0);

  return true;
}

bool CSurf_MCU::OnRotaryEncoderPush( MIDI_event_t *evt ) {
  int trackid=evt->midi_message[1]-0x20;
  m_pan_lasttouch[Tracks::instance()->getMediaTrackForChannel(trackid + 1)]=timeGetTime();

  m_pCCSManager->vpotPressed(trackid + 1, evt->midi_message[2] > 0x3f);

  return true;
}

bool CSurf_MCU::OnRecArm( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonRec(evt->midi_message[1] + 1, evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnRecArmDC( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonRecDC(evt->midi_message[1] + 1, evt->midi_message[2] > 0x3f);
}


bool CSurf_MCU::OnMute( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonMute(evt->midi_message[1] - 0x0f, evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnSolo( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonSolo(evt->midi_message[1] - 0x07, evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnSoloDC( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonSoloDC(evt->midi_message[1] - 0x07);
}

bool CSurf_MCU::OnChannelSelect( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonSelect(evt->midi_message[1] - 0x17, evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnChannelSelectDC( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonSelectDC(evt->midi_message[1] - 0x17);
}

bool CSurf_MCU::OnChannelSelectLong( int channel ) {
  return m_pCCSManager->buttonSelectLong(channel);
}


bool CSurf_MCU::OnTransport( MIDI_event_t *evt ) {
  bool down = evt->midi_message[2]>0x40;
  switch(evt->midi_message[1]) {
  case B_RECORD:
    m_dropstate.updateReaper();
    m_pTransport->recordButton(down);
    break;
  case B_PLAY:
    m_pTransport->playButton(down);
    break;
  case B_STOP:
    m_pTransport->stopButton(down);
    break;
  case B_REWIND:
    m_pTransport->rewindButton(down);
    break;
  case B_FFWD:
    m_pTransport->forwardButton(down);
    break;
  }
  return true;
}

bool CSurf_MCU::OnTransportDC( MIDI_event_t *evt ) {
  switch(evt->midi_message[1]) {
  case B_RECORD:
    break;
  case B_PLAY:
    break;
  case B_STOP:
    if (IsModifierPressed(VK_ALT) || IsModifierPressed(VK_OPTION)) {
      double lStart, lEnd;
      bool asLoop = IsModifierPressed(VK_ALT);
      GetSet_LoopTimeRange(false, asLoop, &lStart, &lEnd, false);
      if (IsModifierPressed(VK_SHIFT)) {
        lStart = QuantizeTimeToBeat(lStart);
        lEnd = QuantizeTimeToBeat(lEnd);
      } else {
        lStart = QuantizeTimeToBar(lStart);
        lEnd = QuantizeTimeToBar(lEnd);
      }
      Undo_BeginBlock();
      GetSet_LoopTimeRange(true, asLoop, &lStart, &lEnd, false);
      SetEditCurPos(lStart, false, false);
      Undo_EndBlock("Time Selection Change (via Surface)",UNDO_STATE_MISCCFG);
    } else {
      if (IsModifierPressed(VK_SHIFT))
        SetEditCurPos(QuantizeTimeToBeat(GetCursorPosition()), false, false);
      else
        SetEditCurPos(QuantizeTimeToBar(GetCursorPosition()), false, false);
    }
    break;
  case B_REWIND:
    break;
  case B_FFWD:
    break;
  }
  return true;
}


bool CSurf_MCU::OnMarker( MIDI_event_t *evt ) {
  m_pTransport->handleButton( Transport::MARKER, evt->midi_message[2]>0x40);
  return true;
}

bool CSurf_MCU::OnNudge( MIDI_event_t *evt ) {
  m_pTransport->handleButton( Transport::NUDGE, evt->midi_message[2]>0x40);
  return true;
}


bool CSurf_MCU::OnCycle( MIDI_event_t *evt ) {
  SendMessage(g_hwnd,WM_COMMAND,IDC_REPEAT,0);
  return true;
}

bool CSurf_MCU::OnClick( MIDI_event_t *evt ) {
  SendMessage(g_hwnd,WM_COMMAND,ID_METRONOME,0);
  return true;
}

void CSurf_MCU::ClearSaveLed() {
  if (m_midiout) 
    m_midiout->Send(0x90,0x50,0,-1);    
}

bool CSurf_MCU::OnSave( MIDI_event_t *evt ) {
  if (m_midiout) 
    m_midiout->Send(0x90,0x50,0x7f,-1);
  SendMessage(g_hwnd,WM_COMMAND,IsModifierPressed(VK_SHIFT)|IsModifierPressed(VK_ALT)?ID_FILE_SAVEAS:ID_FILE_SAVEPROJECT,0);
  ScheduleAction( timeGetTime() + 1000, &CSurf_MCU::ClearSaveLed );
  return true;
}

void CSurf_MCU::ClearUndoLed() {
  if (m_midiout) 
    m_midiout->Send(0x90,0x51,0,-1);    
}

bool CSurf_MCU::OnUndo( MIDI_event_t *evt ) {
  if (m_midiout) 
    m_midiout->Send(0x90,0x51,0x7f,-1);
  SendMessage(g_hwnd,WM_COMMAND,IsModifierPressed(VK_SHIFT)?IDC_EDIT_REDO:IDC_EDIT_UNDO,0);
  ScheduleAction( timeGetTime() + 150, &CSurf_MCU::ClearUndoLed );
  return true;
}

bool CSurf_MCU::OnCancel( MIDI_event_t *evt ) {
  SendMessage(g_hwnd,WM_COMMAND,ID_STOP_AND_DELETE_MEDIA,0);
  return true;
}

bool CSurf_MCU::OnZoom( MIDI_event_t *evt ) {
  if (IsModifierPressed(VK_SHIFT)) {
    SendMessage(g_hwnd,WM_COMMAND,ID_ZOOM_OUT_PROJECT,0);
    return true;
  }
  m_mackie_arrow_states^=ARROW_STATE_ZOOM;
  if (m_midiout) 
    m_midiout->Send(0x90, 0x64,(m_mackie_arrow_states&ARROW_STATE_ZOOM)?0x7f:0,-1);
  return true;
}

bool CSurf_MCU::OnScrub( MIDI_event_t *evt ) {
  m_mackie_arrow_states^=ARROW_STATE_SCRUB;
  if (m_midiout) 
    m_midiout->Send(0x90, 0x65,(m_mackie_arrow_states&ARROW_STATE_SCRUB)?0x7f:0,-1);
  return true;
}

bool CSurf_MCU::OnFlip( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonFlip(evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnGlobal( MIDI_event_t *evt ) {
  return m_pCCSManager->buttonGView(evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnKeyModifier( MIDI_event_t *evt ) {  
  int mask=(1<<(evt->midi_message[1]-0x46));
  if (evt->midi_message[2] >= 0x40)
    s_mackie_modifiers|=mask;
  else
    s_mackie_modifiers&=~mask;

  m_pActionDisplay->switchTo(s_mackie_modifiers);

  return true;
}

bool CSurf_MCU::OnScroll( MIDI_event_t *evt ) {
  if (evt->midi_message[2]>0x40)
    m_mackie_arrow_states |= 1<<(evt->midi_message[1]-0x60);
  else
    m_mackie_arrow_states &= ~(1<<(evt->midi_message[1]-0x60));
  return true;
}

bool CSurf_MCU::OnTouch( MIDI_event_t *evt ) {
  int fader = evt->midi_message[1]-0x68;
  m_fader_touchstate[Tracks::instance()->getMediaTrackForChannel(fader+1)] = evt->midi_message[2]>=0x7f;

  return m_pCCSManager->faderTouched(fader!=8 ? fader + 1 : 0, evt->midi_message[2] > 0x3f);
}

bool CSurf_MCU::OnFunctionKey( MIDI_event_t *evt ) {
  int fkey = evt->midi_message[1] - 0x35; 

  if ( IsModifierPressed(VK_ALT) || IsModifierPressed(VK_OPTION)) {
    HandleFunctionKeyForRegionsOrLoops(fkey, IsModifierPressed(VK_ALT));
    return true;
  }

  if (IsModifierPressed(VK_SHIFT)) {
    HandleFunctionKeyForRegionsOrLoops(fkey, true);
    return true;
  }

  int command = ( IsModifierPressed(VK_CONTROL) ? ID_SET_MARKER1 : ID_GOTO_MARKER1 ) + fkey - 1;
  SendMessage(g_hwnd,WM_COMMAND, command, 0);
  return true;
}

void CSurf_MCU::HandleFunctionKeyForRegionsOrLoops( int fkey, bool loop )
{
  m_region.GetFromActualRegion(loop);
  if ( IsModifierPressed(VK_CONTROL)) {
    m_region.Store(fkey, loop);
  }
  else if ( IsModifierPressed(VK_SHIFT)) {
    Undo_BeginBlock();
    m_region.SetStart(GetCursorPosition());
    double markerPos = m_region.MarkerPos(fkey);
    if (markerPos >= 0) { 
      m_region.SetEnd(m_region.MarkerPos(fkey));
      m_region.Set(loop);
    }
    if (loop)
      Undo_EndBlock("Loop changed (via Surface)",UNDO_STATE_MISCCFG);
    else
      Undo_EndBlock("Region changed (via Surface)",UNDO_STATE_MISCCFG);
  }
  else if (m_region.FindRegion(fkey)) { 
    Undo_BeginBlock();
    m_region.Set(loop);
    if (loop)
      Undo_EndBlock("Loop changed (via Surface)",UNDO_STATE_MISCCFG);
    else
      Undo_EndBlock("Region changed (via Surface)",UNDO_STATE_MISCCFG);
  }
}

bool CSurf_MCU::OnGlobalSoloButton( MIDI_event_t *evt ) {
  int flags;
  GetTrackInfo(-1, &flags);

  if (flags & 8) {
    SendMessage(g_hwnd,WM_COMMAND,ID_TOGGLE_MASTER_MUTE,0);
  } else {
    SoloAllTracks(0);
    m_pCCSManager->updateSoloLEDs();
  }
  UpdateGlobalSoloLED();
  return true;
}

bool CSurf_MCU::OnDropButton( MIDI_event_t *evt ) {
  m_dropstate.toggleStateAndUpdate(m_is_mcuex, m_midiout);
  return true;
}


bool CSurf_MCU::OnButtonPress( MIDI_event_t *evt ) {
  return m_pButtonManager->dispatchMidiEvent(evt);
}

bool CSurf_MCU::OnPedalMove(MIDI_event_t* evt)
{
  if (evt->midi_message[0] == 0x90 && evt->midi_message[1] == 0x66) {
    MIDI_event_t sendEvent={0,3,{0xB0,0x05,evt->midi_message[2]}};
    kbd_OnMidiEvent(&sendEvent,-1);
    return true;
  } else if (evt->midi_message[0] == 0x90 && evt->midi_message[1] == 0x67) {
    MIDI_event_t sendEvent={0,3,{0xB0,0x06,evt->midi_message[2]}};
    kbd_OnMidiEvent(&sendEvent,-1);
    return true;
  } else if (evt->midi_message[0] == 0xB0 && evt->midi_message[1] == 0x2E) {
    MIDI_event_t sendEvent={0,3,{0xB0,0x04,evt->midi_message[2]}};
    kbd_OnMidiEvent(&sendEvent,-1);
    return true;
  }

  return false;
}

typedef bool (CSurf_MCU::*MidiHandlerFunc)(MIDI_event_t*);

void CSurf_MCU::OnMIDIEvent(MIDI_event_t *evt)
{
  #if 0
  char buf[512];
  sprintf(buf,"message %02x, %02x, %02x\n",evt->midi_message[0],evt->midi_message[1],evt->midi_message[2]);
  OutputDebugString(buf);
  #endif

  static const int nHandlers = 6;
  static const MidiHandlerFunc handlers[nHandlers] = {
      &CSurf_MCU::OnMCUReset,
      &CSurf_MCU::OnFaderMove,
      &CSurf_MCU::OnRotaryEncoder,
      &CSurf_MCU::OnJogWheel,
      &CSurf_MCU::OnButtonPress,
      &CSurf_MCU::OnPedalMove,
  };
  for ( int i = 0; i < nHandlers; i++ )
    if ( (this->*handlers[i])(evt) ) return;
}

CSurf_MCU::CSurf_MCU(bool ismcuex, int offset, int size, int indev, int outdev, int cfgflags, int *errStats) :
m_pActionsDialogComponent(NULL)
{
  //_CrtSetBreakAlloc(6938);
  if (s_iNumInstances == 0)
    initialiseJuce_GUI();
  s_iNumInstances++;

  m_pButtonManager = new ButtonManager(this);

  Tracks::instance()->setMCU(this);

  m_is_mcuex = ismcuex;
  m_offset=offset;
  m_size=size;
  m_midi_in_dev=indev;
  m_midi_out_dev=outdev;
  s_cfg_flags=cfgflags;
  //create midi hardware access
  m_midiin = m_midi_in_dev >= 0 ? CreateMIDIInput(m_midi_in_dev) : NULL;
  m_midiout = m_midi_out_dev >= 0 ? CreateThreadedMIDIOutput(CreateMIDIOutput(m_midi_out_dev,false,NULL)) : NULL;

  m_pDisplayHandler = new DisplayHandler(this, m_is_mcuex ? DisplayHandler::MCU_EX : DisplayHandler::MCU);
  m_pSplashDisplay = new Display(m_pDisplayHandler, 2); 
  m_pActionDisplay = new ActionsDisplay(m_pDisplayHandler);
  m_pCCSManager = new CCSManager(this); // m_pDisplayHandler must be constructed before

  m_repeatState = false;

  g_mcu_list.Add(this);

  m_selected_tracks = NULL;

  m_mcu_timedisp_lastforce=0;

  if (errStats)
  {
    if (m_midi_in_dev >=0  && !m_midiin) *errStats|=1;
    if (m_midi_out_dev >=0  && !m_midiout) *errStats|=2;
  }

  MCUReset();

  if (m_midiin)
    m_midiin->start();
  
  m_schedule = NULL;

  m_pCCSManager->init();

  for (int i = 0; i < 128; i++) {
    m_led_state[i] = LED_UNKNOWN;
  }

  m_pTransport = new Transport(this);
  m_pTransport->updateLeds();

  connect2FrameSignal(boost::bind(&UndoEnd::run, UndoEnd::instance(), _1));

  Actions::instance()->init(this);
}
    
CSurf_MCU::~CSurf_MCU() 
{
  if (m_midiout)
  {
#if 0 // NDEBUG  // reset MCU to stock!, fucko enable this in dist builds, maybe?
    struct
    {
      MIDI_event_t evt;
      char data[5];
    }
    poo;
    poo.evt.frame_offset=0;
    poo.evt.size=8;
    poo.evt.midi_message[0]=0xF0;
    poo.evt.midi_message[1]=0x00;
    poo.evt.midi_message[2]=0x00;
    poo.evt.midi_message[3]=0x66;
    poo.evt.midi_message[4]=m_is_mcuex ? 0x15 : 0x14;
    poo.evt.midi_message[5]=0x08;
    poo.evt.midi_message[6]=0x00;
    poo.evt.midi_message[7]=0xF7;
    Sleep(5);
    m_midiout->SendMsg(&poo.evt,-1);
    Sleep(5);

#else 
    char bla[11]={"          "};
    int x;
    for (x =0 ; x < sizeof(bla)-1; x ++)
      m_midiout->Send(0xB0,0x40+x,bla[x],-1);
#endif
  }

  delete m_pTransport;
  delete m_pSplashDisplay;
  delete m_pActionsDialogComponent;
  delete m_pActionDisplay;
  delete m_pDisplayHandler;
  delete m_pCCSManager;

  g_mcu_list.Delete(g_mcu_list.Find(this));
  delete m_midiout;
  delete m_midiin;
  while( m_schedule != NULL ) {
    ScheduledAction *temp = m_schedule;
    m_schedule = temp->next;
    delete temp;
  }

  SelectedTrack::FreeTrackList(m_selected_tracks);

  delete (m_pButtonManager);

  s_iNumInstances--;
  if (s_iNumInstances == 0) {
    shutdownJuce_GUI();
    delete(Tracks::instance());
    delete(PlugMoveWatcher::instance());
    delete(ProjectConfig::instance());
    delete(UndoEnd::instance());
//    delete(Actions::instance());
  }
}

void CSurf_MCU::CloseNoReset() 
{ 
  delete m_midiout;
  delete m_midiin;
  m_midiout=0;
  m_midiin=0;
}

void CSurf_MCU::Run() 
{ 
  DWORD now=timeGetTime();

  Tracks* m_pTracks = Tracks::instance();

  if (now >= m_frameupd_lastrun+(1000/max((*g_config_csurf_rate),1)) || now < m_frameupd_lastrun-250)
  {
    m_frameupd_lastrun=now;

    ProjectConfig::instance()->checkReaProjectChange();

    PlugMoveWatcher::instance()->checkMovement();

    signalFrame(now);

    Tracks::instance()->adjust(g_mcu_list.GetSize() * 8);

    UpdateGlobalSoloLED();
    UpdateMetronomLED();

    while( m_schedule && now >= m_schedule->time ) {
      ScheduledAction *action = m_schedule;
      m_schedule = m_schedule->next;
      (this->*(action->func))();
      delete action;
    }
    
    if (m_midiout)
    {
      if (!m_is_mcuex)
      {
        double pp=(GetPlayState()&1) ? GetPlayPosition() : GetCursorPosition();
        unsigned char bla[10];
  //      bla[-2]='A';//first char of assignment
    //    bla[-1]='Z';//second char of assignment

        // if 0x40 set, dot below item

        memset(bla,0,sizeof(bla));

        int *tmodeptr=(int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode2);

        int tmode=0;

        if (tmodeptr && (*tmodeptr)>=0) tmode = *tmodeptr;
        else
        {
          tmodeptr=(int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode);
          if (tmodeptr)
            tmode=*tmodeptr;
        }

        if (tmode==3) // seconds
        {
          double *toptr = (double*)projectconfig_var_addr(NULL,__g_projectconfig_timeoffs);

          if (toptr) pp+=*toptr;          
          char buf[64];
          sprintf(buf,"%d %02d",(int)pp, ((int)(pp*100.0))%100);
          if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
          else
            memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));

        }
        else if (tmode==4) // samples
        {
          char buf[128];
          format_timestr_pos(pp,buf,sizeof(buf),4);
          if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
          else
            memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));
        }
        else if (tmode==5) // frames
        {
          char buf[128];
          format_timestr_pos(pp,buf,sizeof(buf),5);
          char *p=buf;
          char *op=buf;
          int ccnt=0;
          while (*p)
          {
            if (*p == ':')
            {
              ccnt++;
              if (ccnt!=3) 
              {
                p++;
                continue;
              }
              *p=' ';
            }

            *op++=*p++;
          }
          *op=0;
          if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
          else
            memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));
        }
        else if (tmode>0)
        {
          int num_measures=0;
          double beats=TimeMap2_timeToBeats(NULL,pp,&num_measures,NULL,NULL,NULL)+ 0.000000000001;
          double nbeats = floor(beats);

          beats -= nbeats;

          int fracbeats = (int) (1000.0 * beats);

          int *measptr = (int*)projectconfig_var_addr(NULL,__g_projectconfig_measoffs);
          int nm=num_measures+1+(measptr ? *measptr : 0);
          if (nm >= 100) bla[0]='0'+(nm/100)%10;//bars hund
          if (nm >= 10) bla[1]='0'+(nm/10)%10;//barstens
          bla[2]='0'+(nm)%10;//bars

          int nb=(int)nbeats+1;
          if (nb >= 10) bla[3]='0'+(nb/10)%10;//beats tens
          bla[4]='0'+(nb)%10;//beats

          bla[7]='0' + (fracbeats/100)%10;
          bla[8]='0' + (fracbeats/10)%10;
          bla[9]='0' + (fracbeats%10); // frames
        }
        else
        {
          double *toptr = (double*)projectconfig_var_addr(NULL,__g_projectconfig_timeoffs);
          if (toptr) pp+=(*toptr);

          int ipp=(int)pp;
          int fr=(int)((pp-ipp)*1000.0);

          if (ipp >= 360000) bla[0]='0'+(ipp/360000)%10;//hours hundreds
          if (ipp >= 36000) bla[1]='0'+(ipp/36000)%10;//hours tens
          if (ipp >= 3600) bla[2]='0'+(ipp/3600)%10;//hours

          bla[3]='0'+(ipp/600)%6;//min tens
          bla[4]='0'+(ipp/60)%10;//min 
          bla[5]='0'+(ipp/10)%6;//sec tens
          bla[6]='0'+(ipp%10);//sec
          bla[7]='0' + (fr/100)%10;
          bla[8]='0' + (fr/10)%10;
          bla[9]='0' + (fr%10); // frames
        }

        if (m_mackie_lasttime_mode != tmode)
        {
          m_mackie_lasttime_mode=tmode;
          m_midiout->Send(0x90, 0x71, tmode==5?0x7F:0,-1); // set smpte light 
          m_midiout->Send(0x90, 0x72, m_mackie_lasttime_mode>0 && tmode<3?0x7F:0,-1); // set beats light 

        }

        //if (memcmp(m_mackie_lasttime,bla,sizeof(bla)))
        {
          bool force=false;
          if (now > m_mcu_timedisp_lastforce) 
          {
            m_mcu_timedisp_lastforce=now+2000;
            force=true;
          }
          int x;
          for (x =0 ; x < sizeof(bla) ; x ++)
          {
            int idx=sizeof(bla)-x-1;
            if (bla[idx]!=m_mackie_lasttime[idx]||force)
            {
              m_midiout->Send(0xB0,0x40+x,bla[idx],-1);
              m_mackie_lasttime[idx]=bla[idx];
            }
          }
        }

      }

      m_pCCSManager->frameUpdate(now);

      m_pCCSManager->getDisplayHandler()->waitForMoreChanges(false);
    }
  }

  if (m_midiin)
  {
    m_midiin->SwapBufs(timeGetTime());
    int l=0;
    MIDI_eventlist *list=m_midiin->GetReadBuf();
    MIDI_event_t *evts;
    while ((evts=list->EnumItems(&l))) OnMIDIEvent(evts);

    if (m_button_states||m_mackie_arrow_states)
    {
      DWORD now=timeGetTime();
      if (now >= m_buttonstate_lastrun + 100)
      {
        m_buttonstate_lastrun=now;

        if (m_mackie_arrow_states)
        {
          bool iszoom = ((m_mackie_arrow_states&ARROW_STATE_ZOOM) > 0);
          int left =  (iszoom && IsFlagSet(CONFIG_FLAG_SWAPZOOM)) ? 3 : 2;
          int right = (iszoom && IsFlagSet(CONFIG_FLAG_SWAPZOOM)) ? 2 : 3;

          if (m_mackie_arrow_states&1) 
            CSurf_OnArrow(0,iszoom);
          if (m_mackie_arrow_states&2) 
            CSurf_OnArrow(1,iszoom);
          if (m_mackie_arrow_states&4) 
            CSurf_OnArrow(left,iszoom);
          if (m_mackie_arrow_states&8) 
            CSurf_OnArrow(right,iszoom);
        }

        if ((m_button_states&3) != 3)
        {
          if (m_button_states&1)
          {
            CSurf_OnRew(1);
          }
          else if (m_button_states&2)
          {
            CSurf_OnFwd(1);
          }
        }
      }
    }
  }
}

void CSurf_MCU::SendMidi(unsigned char status, unsigned char d1, unsigned char d2, int frame_offset) {
  if (m_midiout)
    m_midiout->Send(status, d1, d2, frame_offset); 
}

void CSurf_MCU::SetLED(int button_nr, int led_state) {
  if (m_led_state[button_nr] != led_state) {
    SendMidi(0x90, button_nr, led_state, -1);
    m_led_state[button_nr] = led_state;
  }
}

void CSurf_MCU::SetTrackListChange() 
{ 
  ProjectConfig::instance()->checkReaProjectChange();
  Tracks::instance()->tracksStatesChanged();
  m_pCCSManager->trackListChange();
}

void CSurf_MCU::SetSurfaceVolume(MediaTrack *trackid, double volume) 
{ 
  m_surface_volume[trackid] = volume;
}

void CSurf_MCU::SetSurfacePan(MediaTrack *trackid, double pan) 
{ 
  m_surface_pan[trackid] = pan;
}

void CSurf_MCU::SetSurfaceMute(MediaTrack *trackid, bool mute) 
{
}

void CSurf_MCU::SetSurfaceSelected(MediaTrack *trackid, bool selected) 
{ 
  Tracks::instance()->selectionChanged();

  m_pCCSManager->trackSelected(trackid, selected);
}

void CSurf_MCU::SetSurfaceSolo(MediaTrack *trackid, bool solo) 
{
  UpdateGlobalSoloLED();
}

void CSurf_MCU::SetSurfaceRecArm(MediaTrack *trackid, bool recarm) 
{ 
}

void CSurf_MCU::SetPlayState(bool play, bool pause, bool rec) 
{ 
  if (m_midiout && !m_is_mcuex)
  {
    m_midiout->Send(0x90, 0x5f,rec?0x7f:0,-1);
    m_midiout->Send(0x90, 0x5e,play?0x7f:0,-1);
    m_midiout->Send(0x90, 0x5d,pause?0x7f:0,-1);
  }
} 

void CSurf_MCU::SetRepeatState(bool rep) 
{
  m_repeatState = rep;
  if (m_midiout && !m_is_mcuex)
  {
    m_midiout->Send(0x90, 0x56,rep?0x7f:0,-1);
  }
}

void CSurf_MCU::SetTrackTitle(MediaTrack *trackid, const char *title) 
{
}

bool CSurf_MCU::GetTouchState(MediaTrack *pMT, int isPan) 
{ 
  if (MultiTrackMode::getFlipMode() == !isPan) 
  {
    if (pMT && m_pan_lasttouch.find(pMT) != m_pan_lasttouch.end())
    {
      DWORD now=timeGetTime();
      if (m_pan_lasttouch[pMT]==1 || (now<m_pan_lasttouch[pMT]+3000)) // fake touch, go for 3s after last movement
      {
        return true;
      }
    }
    return false;
  }

  if (pMT && m_fader_touchstate.find(pMT) != m_fader_touchstate.end())
    return m_fader_touchstate[pMT];

  return false; 
}

void CSurf_MCU::SetAutoMode(int mode) 
{
  // wird von Reaper aufgerufen, wenn der automation-mode geaendert wurde, m_pCCSManager->updateAutoMode();
  UpdateAutoModes();
}

void CSurf_MCU::UpdateAutoModes() {
  if ( m_midiout && !m_is_mcuex ) {
    int modes[5] = { 0, 0, 0, 0, 0 };
    for ( SelectedTrack *i = m_selected_tracks; i; i = i->next ) {
      MediaTrack *track = i->track();
      if (!track) continue;
      int mode = GetTrackAutomationMode(track);
      if ( 0 <= mode && mode < 5 )
        modes[mode] = 1;
    }
    bool multi = ( modes[0] + modes[1] + modes[2] + modes[3] + modes[4] ) > 1;
    m_midiout->Send(0x90, 0x4A, modes[AUTO_MODE_READ] ? ( multi ? 1:0x7f ) : 0, -1 );
    m_midiout->Send(0x90, 0x4B, modes[AUTO_MODE_WRITE] ? ( multi ? 1:0x7f ) : 0, -1 );
    m_midiout->Send(0x90, 0x4C, modes[AUTO_MODE_TRIM] ? ( multi ? 1:0x7f ) : 0, -1 );
    m_midiout->Send(0x90, 0x4D, modes[AUTO_MODE_TOUCH] ? ( multi ? 1:0x7f ) : 0, -1 );
    m_midiout->Send(0x90, 0x4E, modes[AUTO_MODE_LATCH] ? ( multi ? 1:0x7f ) : 0, -1 );
  }
}

void CSurf_MCU::OnTrackSelection(MediaTrack *trackid) 
{
}

bool CSurf_MCU::IsModifierPressed(int key) 
{ 
  ModifierKeys keyboardModifiers = ModifierKeys::getCurrentModifiersRealtime();
  ASSERT_M( key == VK_SHIFT || key == VK_OPTION || key == VK_CONTROL || key == VK_ALT, "Only for Modifier");
  if (m_midiin && !m_is_mcuex)
  {
    if (key == VK_SHIFT) return (s_mackie_modifiers&1) || IsKeyboardPressed(VK_SHIFT);
    if (key == VK_OPTION) return !! (s_mackie_modifiers&2) || IsKeyboardPressed(VK_RMENU);
    if (key == VK_CONTROL) return !!(s_mackie_modifiers&4) || IsKeyboardPressed(VK_CONTROL);
    if (key == VK_ALT) return !!(s_mackie_modifiers&8) || IsKeyboardPressed(VK_LMENU);
  }

  return false; 
}

bool CSurf_MCU::IsKeyboardPressed(int key)
{
  return (GetAsyncKeyState(key) & 0x8000) && IsFlagSet(CONFIG_FLAG_KEYBOARD_MODIFIER);
}

bool CSurf_MCU::OnGlobalViewKeys( MIDI_event_t *evt )
{
  if ( evt->midi_message[2]>=0x40 ) {
    int a=evt->midi_message[1];
    if (IsButtonPressed(B_GLOBAL_VIEW))
      a -= 0x10;
    else if (IsButtonPressed(B_MARKER))
      a -= 0x20;
    else if (IsButtonPressed(B_NUDGE)) 
      a -= 0x30;

    int byte2 = 0xbf;
    if (IsModifierPressed(VK_SHIFT)) byte2 -= 1;
    if (IsModifierPressed(VK_OPTION)) byte2 -= 2;
    if (IsModifierPressed(VK_CONTROL)) byte2 -= 4;
    if (IsModifierPressed(VK_ALT)) byte2 -= 8;

    MIDI_event_t evt={0,3,byte2,a,0};
    kbd_OnMidiEvent(&evt,-1);
  }
  return true;
}

void CSurf_MCU::SendMsg( MIDI_event_t *message, int frame_offset )
{
  if (m_midiout) 
    m_midiout->SendMsg(message, frame_offset); 
}

bool CSurf_MCU::OnNameValue( MIDI_event_t *evt )
{
  if (IsModifierPressed(VK_ALT)) {
    if (!m_pActionsDialogComponent)
      m_pActionsDialogComponent = new ActionsDialogComponent(m_pActionDisplay);

    m_pCCSManager->getEditor()->setMainComponent(&m_pActionsDialogComponent, true);   
  } else {
    if (evt->midi_message[2]>=0x40) {
      m_pActionDisplay->activate(s_mackie_modifiers);
    } else {
      m_pActionDisplay->deactivate();
    }
  }
  return true;
}

bool CSurf_MCU::OnNameValueDC( MIDI_event_t *evt )
{
  m_pCCSManager->buttonNameValue(evt->midi_message[2]>=0x40);
  return true;
}


void CSurf_MCU::UnselectAllTracks() {
  // Clear master track
  CSurf_OnSelectedChange(CSurf_TrackFromID(0 ,false), 0);
  // Clear already selected tracks
  SelectedTrack *i = GetSelectedTracks();
  while(i) {
    // Call to OnSelectedChange will cause 'i' to be destroyed, so go ahead
    // and get 'next' now
    SelectedTrack *next = i->next;
    MediaTrack *track = i->track();

    if( track ) CSurf_OnSelectedChange( track, 0 );
    i = next;
  }
}

void CSurf_MCU::UpdateGlobalSoloLED() {
  // check master track flags, if master track is muted, blink the LED
  int flags;
  GetTrackInfo(-1, &flags);
  if (flags & 8) {
    SetLED(B_SOLO, LED_BLINK);
    return;  
  }
  if (SomethingSoloed()) {
    SetLED(B_SOLO, LED_ON);
    return;
  }
  SetLED(B_SOLO, LED_OFF);
}

void CSurf_MCU::UpdateMetronomLED() {
  if (m_metronom_offset)
  {
    int *metro_en = (int *)projectconfig_var_addr(NULL,m_metronom_offset);
    if (metro_en) { 
      int a = *metro_en;
      SetLED(B_CLICK, a & 1 ? LED_ON : LED_OFF);
    }
  }
}

bool CSurf_MCU::IsButtonPressed( int i )
{
  return m_pButtonManager->isButtonPressed(i);
}

int CSurf_MCU::IsLastButton( int button )
{
  return m_pButtonManager->isLastButton(button);
}


double CSurf_MCU::GetSurfaceVolume( MediaTrack* pMT )
{
  if (pMT && m_surface_volume.find(pMT) != m_surface_volume.end())
    return m_surface_volume[pMT];

  return 0;
}

double CSurf_MCU::GetSurfaceVolume( int channel )
{
  return GetSurfaceVolume(Tracks::instance()->getMediaTrackForChannel(channel));
}

double CSurf_MCU::GetSurfacePan( MediaTrack* pMT )
{
  if (pMT && m_surface_pan.find(pMT) != m_surface_pan.end())
    return m_surface_pan[pMT];

  return 0;

}

double CSurf_MCU::GetSurfacePan( int channel )
{
  return GetSurfacePan(Tracks::instance()->getMediaTrackForChannel(channel));
}


static void parseParms(const char *str, int parms[NUM_DLG_PARAMS]) {
  parms[0]=0;
  parms[1]=8;
  parms[2]=parms[3]=-1;
  parms[4]=0;

  const char *p=str;
  if (p)
  {
    int x=0;
    while (x<NUM_DLG_PARAMS)
    {
      while (*p == ' ') p++;
      if ((*p < '0' || *p > '9') && *p != '-') break;
      parms[x++]=atoi(p);
      while (*p && *p != ' ') p++;
    }
  }  

  // since v0.8 we don't support extender, so i fix this setting to 0 offset and a banksize of 8
  parms[0]=0;
  parms[1]=8;
}

static IReaperControlSurface *createFunc(const char *type_string, const char *configString, int *errStats)
{
  int parms[NUM_DLG_PARAMS];
  parseParms(configString,parms);

  if (CSurf_MCU::s_iNumInstances == 0)
    return new CSurf_MCU(!strcmp(type_string,EXT_ID),parms[0],parms[1],parms[2],parms[3],parms[4],errStats);
  else {
    ::MessageBox(NULL, "CSurf_MCU_Klinke instance is already active (and only a single instance can be used).", "CSurf_MCU_Klinke", MB_ICONSTOP);
    return NULL;
  }
}


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      {
        int parms[NUM_DLG_PARAMS];
        parseParms((const char *)lParam,parms);

        int n=GetNumMIDIInputs();
        int x=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)"None");
        SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETITEMDATA,x,-1);
        x=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)"None");
        SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETITEMDATA,x,-1);
        for (x = 0; x < n; x ++)
        {
          char buf[512];
          if (GetMIDIInputName(x,buf,sizeof(buf)))
          {
            int a=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)buf);
            SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETITEMDATA,a,x);
            if (x == parms[2]) SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETCURSEL,a,0);
          }
        }
        n=GetNumMIDIOutputs();
        for (x = 0; x < n; x ++)
        {
          char buf[512];
          if (GetMIDIOutputName(x,buf,sizeof(buf)))
          {
            int a=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)buf);
            SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETITEMDATA,a,x);
            if (x == parms[3]) SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETCURSEL,a,0);
          }
        }
        
        if (parms[4]&CONFIG_FLAG_FADER_TOUCH_FAKE)
          CheckDlgButton(hwndDlg,IDC_FAKE_TOUCH,BST_CHECKED);
        if (parms[4]&CONFIG_FLAG_SWAPZOOM)
          CheckDlgButton(hwndDlg,IDC_CHECK2,BST_CHECKED);
        if (parms[4]&CONFIG_FLAG_NO_LEVEL_METER)
          CheckDlgButton(hwndDlg,IDC_NOLEVEL,BST_CHECKED);
        if (parms[4]&CONFIG_FLAG_KEYBOARD_MODIFIER)
          CheckDlgButton(hwndDlg,IDC_KEYBOARD_MODIFIER,BST_CHECKED);
      }
    break;
    case WM_COMMAND:
      { 
        switch(LOWORD(wParam))
        {
          case BTN_DONATE:
            ShellExecute(NULL,
              "open",
              "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=LR54GZHGL6VHA",
              NULL,NULL,SW_SHOWDEFAULT);  
        }
      }
    case WM_USER+1024:
      if (wParam > 1 && lParam)
      {
        int indev=-1, outdev=-1, offs=0, size=8;
        int r=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETCURSEL,0,0);
        if (r != CB_ERR) indev = SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETITEMDATA,r,0);
        r=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETCURSEL,0,0);
        if (r != CB_ERR)  outdev = SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETITEMDATA,r,0);
        int cflags=0;
        if (IsDlgButtonChecked(hwndDlg,IDC_FAKE_TOUCH))
           cflags|=CONFIG_FLAG_FADER_TOUCH_FAKE;
        if (IsDlgButtonChecked(hwndDlg,IDC_CHECK2))
          cflags|=CONFIG_FLAG_SWAPZOOM;
        if (IsDlgButtonChecked(hwndDlg,IDC_NOLEVEL))
          cflags|=CONFIG_FLAG_NO_LEVEL_METER;
        if (IsDlgButtonChecked(hwndDlg,IDC_KEYBOARD_MODIFIER))
          cflags|=CONFIG_FLAG_KEYBOARD_MODIFIER;

        char tmp[512];
        sprintf(tmp,"%d %d %d %d %d",offs,size,indev,outdev,cflags);
        lstrcpyn((char *)lParam, tmp,wParam);
        
      }
    break;
  }
  return 0;
}

static HWND configFunc(const char *type_string, HWND parent, const char *initConfigString)
{
  if (!strcmp(type_string,EXT_ID)) 
    return CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_SURFACEEDIT_MCUEX),parent,dlgProc,(LPARAM)initConfigString);
  else
    return CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_SURFACEEDIT_MCUMAIN),parent,dlgProc,(LPARAM)initConfigString);

  return NULL;

  parent =NULL;
}

reaper_csurf_reg_t csurf_mcu_modified_reg = 
{
  MAIN_ID,
#ifdef EXT_B
  "Mackie Control B (Klinke)",
#else
  "Mackie Control (Klinke)",
#endif
  createFunc,
  configFunc,
};
reaper_csurf_reg_t csurf_mcuex_modified_reg = 
{
  EXT_ID,
  "Mackie Control Extender (Klinke)",
  createFunc,
  configFunc,
};
