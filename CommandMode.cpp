/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "csurf.h"
#include "CommandMode.h"
#include "CCSManager.h"
#include "DisplayTrackMeter.h"
#include "csurf_mcu.h"
#include "swell\swell.h"
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include "CommandModeMainComponent.h"

CommandMode::Page::Page(CommandMode* pMode, int index) :
m_pMode(pMode),
m_iIndex(index)
{ 
	m_strPageName = JUCE_T("Page ") + String(index + 1) ;
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 8; j++) {
			m_bRelative[i][j] = false;
			m_iNormalSpeed[i][j] = 1;
			m_iPressedSpeed[i][j] = 5;
			m_strCommandName[i][j] = juce::String::empty;
		}
	}  
	
	// when to xml file is found, display a warning via the command names of the first page
	// with a correct installation, the names will be overwritten while loading the xml file
	if (index == 0) {
		m_strCommandName[0][0] = "Please";
		m_strCommandName[0][1] = " read ";
		m_strCommandName[0][2] = " the  ";
		m_strCommandName[0][3] = "instal";
		m_strCommandName[0][4] = "lation";
		m_strCommandName[0][5] = " part ";
		m_strCommandName[0][6] = "of the";
		m_strCommandName[0][7] = "manual";
	}
}

#define CM_NODE_PAGE JUCE_T("PAGE")
#define CM_NODE_VPOT JUCE_T("VPOT")

#define CM_ATT_NAME JUCE_T("name") 
#define CM_ATT_SHIFT JUCE_T("shift")
#define CM_ATT_INDEX JUCE_T("index")
#define CM_ATT_RELATIVE JUCE_T("relative")
#define CM_ATT_NORMAL JUCE_T("normal")
#define CM_ATT_FAST JUCE_T("fast")
#define CM_ATT_VERSION JUCE_T("version")

void CommandMode::Page::writeToXml(XmlElement* pDoc)
{
	XmlElement* pPageNode = new XmlElement(CM_NODE_PAGE);
	pDoc->addChildElement(pPageNode);
	pPageNode->setAttribute(CM_ATT_NAME, m_strPageName);
	pPageNode->setAttribute(CM_ATT_INDEX, m_iIndex);
	pPageNode->setAttribute(CM_ATT_VERSION, 1);
	
	for (int shift = 0; shift < 2; shift++) {
		for (int channel = 0; channel < 8; channel++) {
			XmlElement* pVpot = new XmlElement(CM_NODE_VPOT);
			pPageNode->addChildElement(pVpot);

			pVpot->setAttribute(CM_ATT_INDEX, channel);
			if (shift == 1)
				pVpot->setAttribute(CM_ATT_SHIFT, "true");

			pVpot->setAttribute(CM_ATT_NAME, m_strCommandName[shift][channel]);

			if (m_bRelative[shift][channel]) 
				pVpot->setAttribute(CM_ATT_RELATIVE, "true");
			pVpot->setAttribute(CM_ATT_NORMAL, m_iNormalSpeed[shift][channel]);
			pVpot->setAttribute(CM_ATT_FAST, m_iPressedSpeed[shift][channel]); 			
		}
	}
}

bool CommandMode::Page::readFromXML(XmlElement* pPageNode)
{
	if (!pPageNode || pPageNode->getTagName() != CM_NODE_PAGE) 
		return false;

	m_strPageName = pPageNode->getStringAttribute(CM_ATT_NAME);

	XmlElement* pVpot = pPageNode->getFirstChildElement();
	int nodesRead = 0;
	while (pVpot && pVpot->getTagName() == CM_NODE_VPOT) {
		int shift = pVpot->getBoolAttribute(CM_ATT_SHIFT);
		int channel = pVpot->getIntAttribute(CM_ATT_INDEX);
		m_strCommandName[shift][channel] = pVpot->getStringAttribute(CM_ATT_NAME);
		m_bRelative[shift][channel] = pVpot->getBoolAttribute(CM_ATT_RELATIVE);
		m_iNormalSpeed[shift][channel] = pVpot->getIntAttribute(CM_ATT_NORMAL);
		m_iPressedSpeed[shift][channel] = pVpot->getIntAttribute(CM_ATT_FAST);
		nodesRead++;
		pVpot = pVpot->getNextElement();
	}

	return(nodesRead == 16);
}

CommandMode::CommandMode(CCSManager* pManager) : MultiTrackMode(pManager)
{
	for (int i = 0; i < 8; i++) {
		m_pPage[i] = new Page(this, i);
	}
	m_pActivePage = m_pPage[0];

	readConfigFile();

	m_pDisplay = new Display(pManager->getDisplayHandler(), 2);
	m_pSelector = new CommandPageSelector(pManager->getDisplayHandler(), this);

	m_pMainComponent = NULL;
}

CommandMode::~CommandMode(void) 
{
	safe_delete(m_pMainComponent); // main component must be delete first

	for (int i = 0; i < 8; i++) {
		safe_delete(m_pPage[i]);
	}

	safe_delete(m_pSelector);
	safe_delete(m_pDisplay);
}

bool CommandMode::readConfigFile() {
	XmlDocument* pXmlFile = new XmlDocument(getConfigFile(true));
	if (!pXmlFile)
		return false;

	XmlElement* pRootElement = pXmlFile->getDocumentElement();
	if (!pRootElement) 
		return false;

	forEachXmlChildElement (*pRootElement, pPage) {
		bool success = m_pPage[pPage->getIntAttribute(CM_ATT_INDEX)]->readFromXML(pPage);
		if (!success) {
			safe_delete(pRootElement);   
			safe_delete(pXmlFile);
			return false;			
		}
	}  

	safe_delete(pRootElement);  
	safe_delete(pXmlFile);
	return true;
}

void CommandMode::writeConfigFile() {
	XmlElement* pRootElement = new XmlElement("ACTIVE_MODE_CONFIG");
	pRootElement->setAttribute(CM_ATT_VERSION, 1);
	
	for (int iPage = 0; iPage < 8; iPage++) {
		m_pPage[iPage]->writeToXml(pRootElement);
	}
	
	pRootElement->writeToFile(getConfigFile(false), "", JUCE_T("UTF-8"));
	
	safe_delete(pRootElement);
}


void CommandMode::activate() {
	CCSMode::activate();
	m_pCCSManager->getDisplayHandler()->switchTo(m_pDisplay);
	m_pCCSManager->getDisplayHandler()->enableMeter(false);
}

// command mode: Channel 1-8, 0x06-0x0c, 0x16-0x1c, 0x26-0x2c ... 0x76-0x7c (the channel is defined thru the page)
// (0xab : a is channel, b has six different state, 
//											 8 left while not pressed,
//											 9 right while not pressed, 
//											 a left while pressed,
//											 b right while pressed, 
//                       c relativ)
bool CommandMode::vpotMoved(int channel, int numSteps) {
	channel--;
	unsigned char midi_byte0 = 0xb0 + m_pActivePage->m_iIndex;
	int shift = m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT) ? 1 : 0;
	if (shift) 
		midi_byte0 += 0x08;
	unsigned char midi_byte1 = 0x10 * channel;
	unsigned char midi_byte2;
	VPOT_LED* pVpot = m_pCCSManager->getVPOT(channel + 1);
	int numSends = 1;

	if (m_pActivePage->m_bRelative[shift][channel] == true) {
		midi_byte1 += 0x0c;
		int relative = 0x40 + numSteps * (pVpot->isPressed() ? m_pActivePage->m_iPressedSpeed[shift][channel] : m_pActivePage->m_iNormalSpeed[shift][channel]);
		midi_byte2 = (unsigned char) min(255, max(0, relative));
	} else {
		bool left = (numSteps < 0);
		midi_byte1 += left ? 0x08 : 0x09;
		if (pVpot->isPressed()) midi_byte1 += 0x02;
		midi_byte2 = 0x7f;
		numSends = left ? -numSteps : numSteps;
	}	

	MIDI_event_t evt={0,3,{midi_byte0,midi_byte1,midi_byte2}};
	for (int i = 0; i < numSends; i++)
		kbd_OnMidiEvent(&evt,-1);
	
	return true;
}

// command mode: Channel 1-8, 0x06-0x0c, 0x16-0x1c, 0x26-0x2c ... 0x76-0x7c (the channel is defined thru the page)
// (0xab : a is channel, b has six different state, 
//											 6 vpot pressed,
//											 7 vpot released)
bool CommandMode::vpotPressed(int channel, bool pressed) {
	channel--;
	unsigned char midi_byte0 = 0xb0 + m_pActivePage->m_iIndex;
	if (m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT)) 
		midi_byte0 += 0x08;
	unsigned char midi_byte1 = 0x10 * channel + (pressed ? 0x06 : 0x07);

	MIDI_event_t evt={0,3,{midi_byte0,midi_byte1,0x07}};
	kbd_OnMidiEvent(&evt,-1);
	 
	return true;
}

// while the fader is touched the level is written to the display
// overwrite it with the page names
bool CommandMode::faderTouched(int channel, bool touched) {
	if (!touched)
		updateDisplay();

	return true;
}

// Disable the VPOT leds
void CommandMode::updateVPOTs() {
	m_pCCSManager->setVPOTMode(VPOT_LED::OFF);
	for (int channel = 1; channel < 9; channel++) {
		if (MediaTrack*  tr = getMediaTrackForChannel(channel)) {
			m_pCCSManager->getVPOT(channel)->setBottom(Tracks::instance()->hasChilds(tr));
		}
	}
}

// write the page names to the second row
void CommandMode::updateDisplay() {
	for (int x = 1; x < 9; x++)
	{
		MediaTrack* tr = getMediaTrackForChannel(x); 
		if (tr)
		{
			TrackState* pTS = Tracks::instance()->getTrackStateForMediaTrack(tr);
			if (pTS) {
				m_pDisplay->changeField(0, x, pTS->showInDisplay());
			}
		}
		else {
			m_pDisplay->changeField(0, x, ""); 
		}
	}

	int shift = m_pCCSManager->getMCU()->IsModifierPressed(VK_SHIFT) ? 1 : 0;

	for (int i = 0; i < 8; i++) 
		m_pDisplay->changeField(1, i+1, m_pActivePage->getCommandName(shift,i));
}

Component** CommandMode::createEditorComponent() {
	if (!m_pMainComponent)
		m_pMainComponent = new CommandModeMainComponent(this);

	return reinterpret_cast<Component**>(&m_pMainComponent);
}

File CommandMode::getConfigFile(boolean bLookAtProgramDir) {
	File configDir = File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() + JUCE_T("\\Reaper\\MCU\\Config\\");
	if (!configDir.exists() || !File(configDir.getFullPathName() + JUCE_T("\\ActionMode.xml")).exists()) {
		if (bLookAtProgramDir) {
			File configDirDll = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getFullPathName() + JUCE_T("\\Plugins\\MCU\\Config\\");
			if (configDirDll.exists()) {
				return File(configDirDll.getFullPathName() + JUCE_T("\\ActionMode.xml")); 
			}
		}
		configDir.createDirectory();
	}
	return File(configDir.getFullPathName() + JUCE_T("\\ActionMode.xml")); 
}

