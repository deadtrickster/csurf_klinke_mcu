/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "CCSModesEditor.h"

CCSModesEditor::CCSModesEditor(CCSManager* pManager) :
m_pManager(pManager),
m_pWindow(NULL),
m_pActiveComponent(NULL)
{
//  m_pWindow = new CCSModesEditorWindow("MCU Editor", Colours::white, false, true, pManager);
  m_pTooltipWindow = new TooltipWindow();
}

CCSModesEditor::~CCSModesEditor(void)
{
  safe_delete(m_pTooltipWindow);
}

void CCSModesEditor::setMainComponent(Component** ppComponent, bool visible) {
//  if (m_pWindow->getLastUsedComponent() != ppComponent) {
//    m_pWindow->removeComponent();
//    if (!visible)
//      m_pWindow->setVisible(visible);
// 
//    m_pWindow->setMainComponent(ppComponent);
//    m_pWindow->setUsingNativeTitleBar(true);
//  }
  deleteWindow();
  m_pWindow = new CCSModesEditorWindow("MCU Editor", Colours::white, false, true, m_pManager);
  m_pWindow->setUsingNativeTitleBar(true);

  m_pWindow->setContentComponent(*ppComponent, false, true);
  if (visible)
    m_pWindow->setVisible(visible);
  m_pWindow->setAlwaysOnTop(true);
  m_pWindow->setTopLeftPosition(20,60);
#ifdef EASY_DEBUG
  m_pWindow->setTopLeftPosition(1280,20);
#endif
  m_pActiveComponent = *ppComponent;
}

void CCSModesEditor::closeWindowAndRemoveComponent( Component* pComponent )
{
  if (m_pWindow && m_pActiveComponent && pComponent == m_pActiveComponent) {
    m_pWindow->setVisible(false);
    m_pWindow->removeComponent();
  }
}

void CCSModesEditor::deleteWindow()
{
  if (m_pWindow) {
    m_pWindow->removeFromDesktop();
    m_pWindow->removeComponent();
    safe_delete(m_pWindow);
  }
}
