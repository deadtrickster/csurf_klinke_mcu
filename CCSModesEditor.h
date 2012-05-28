/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#pragma once

#include "csurf_mcu.h"
#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but

class CCSManager;
class CCSModesEditorWindow;
class CCSMode;

class CCSModesEditor 
{
public:
	CCSModesEditor(CCSManager* pManager);
public:
	virtual ~CCSModesEditor(void);

	void setMainComponent(Component** ppComponent, bool visible);
	void closeWindowAndRemoveComponent(Component* pComponent);
	void deleteWindow();

private:
	CCSManager* m_pManager;
	CCSModesEditorWindow* m_pWindow;
	TooltipWindow* m_pTooltipWindow;
	Component* m_pActiveComponent;
};


class CCSModesEditorWindow :
	public DialogWindow
{
public:
	CCSModesEditorWindow(	const String& name,
												const Colour& backgroundColour,
												const bool escapeKeyTriggersCloseButton,
												const bool addToDesktop,
												CCSManager* pManager) : DialogWindow(name, backgroundColour, escapeKeyTriggersCloseButton, addToDesktop){
		m_ppLastUsedComponent = NULL;
		m_pManager = pManager;
	};

	~CCSModesEditorWindow() {
	}

	void closeButtonPressed() {
		setVisible(false);
		if (m_ppLastUsedComponent)
			safe_delete(*m_ppLastUsedComponent);
		m_pManager->getEditor()->deleteWindow();
	}

	void setMainComponent(Component** ppComponent) {
		m_ppLastUsedComponent = ppComponent;
	}

	void removeComponent() {
		if (m_ppLastUsedComponent)
			safe_delete(*m_ppLastUsedComponent);
		setContentComponent(NULL, false, false);
	}

	Component** getLastUsedComponent(){return m_ppLastUsedComponent;}

private:
	Component** m_ppLastUsedComponent;
	CCSManager* m_pManager;
};

