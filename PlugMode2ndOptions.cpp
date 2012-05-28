/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "PlugMode2ndOptions.h"

PlugMode2ndOptions::PlugMode2ndOptions(DisplayHandler* pDH) : Options(pDH)
{
	addOption(PMO2_MODE_CHANGE);
	addAttribute(PMO2_MODE_CHANGE, PMO2A_NOTHING, true);
	addAttribute(PMO2_MODE_CHANGE, PMO2A_OPEN_CLOSE);
	addAttribute(PMO2_MODE_CHANGE, PMO2A_OPEN_CLOSE_MIXER);

// 	addOption(PMO2_KEEP_IN_FRONT);
// 	addAttribute(PMO2_KEEP_IN_FRONT, PMO2A_OFF, true);
// 	addAttribute(PMO2_KEEP_IN_FRONT, PMO2A_ON);
//	addAttribute(PMO2_KEEP_IN_FRONT, PMO2A_MOVE);
	addOption(PMO2_MOVE);
	addAttribute(PMO2_MOVE, PMO2A_OFF, true);
	addAttribute(PMO2_MOVE, PMO2A_ON);

	readConfigFile();
}

PlugMode2ndOptions::~PlugMode2ndOptions(void)
{
	writeConfigFile();
}

String PlugMode2ndOptions::getConfigFileName()
{
	return String("PlugModeOptions2");
}
