/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#include "ActionsDialogComponent.h"
#include "ActionsDisplay.h"

//==============================================================================
ActionsDialogComponent::ActionsDialogComponent(ActionsDisplay* pAD): 
m_shift (0),
m_option (0),
m_control (0),
m_alt (0),
m_pActionDisplay (pAD)
{
  addAndMakeVisible (m_shift = new ToggleButton (JUCE_T("Shift")));
  m_shift->setExplicitFocusOrder (1);
  m_shift->addButtonListener (this);
  
  addAndMakeVisible (m_option = new ToggleButton (JUCE_T("Option")));
  m_option->setExplicitFocusOrder (2);
  m_option->addButtonListener (this);
  
  addAndMakeVisible (m_control = new ToggleButton (JUCE_T("Control")));
  m_control->setExplicitFocusOrder (3);
  m_control->addButtonListener (this);
  
  addAndMakeVisible (m_alt = new ToggleButton (JUCE_T("Alt")));
  m_alt->setExplicitFocusOrder (4);
  m_alt->addButtonListener (this);
    
  for (int i = 0; i < 8; i++) {
    addAndMakeVisible (m_labelAction[i] = new Label (JUCE_T("Action"), String::empty));
    m_labelAction[i]->setExplicitFocusOrder (5 + i);
    m_labelAction[i]->setFont (Font (Font::getDefaultMonospacedFontName(), 15.0000f, Font::plain));
    m_labelAction[i]->setJustificationType (Justification::centredLeft);
    m_labelAction[i]->setEditable (true, true, false);
    m_labelAction[i]->setColour (Label::backgroundColourId, Colours::white);
    m_labelAction[i]->setColour (Label::outlineColourId, Colours::black);
    m_labelAction[i]->setColour (TextEditor::textColourId, Colours::black);
    m_labelAction[i]->setColour (TextEditor::backgroundColourId, Colour (0x0));
    m_labelAction[i]->addListener (this);
  }
  
  setSize (535, 114);
  
  fillActionLabels();
}

ActionsDialogComponent::~ActionsDialogComponent()
{
  for (int i = 0; i < 8; i++) {
    deleteAndZero (m_labelAction[i]);
  }

  deleteAndZero (m_shift);
  deleteAndZero (m_option);
  deleteAndZero (m_control);
  deleteAndZero (m_alt);
}

//==============================================================================
void ActionsDialogComponent::paint (Graphics& g)
{
    g.fillAll (Colours::white);
}

void ActionsDialogComponent::resized()
{
  m_shift->setBounds (20, 8, 70, 24);
  m_option->setBounds (110, 8, 70, 24);
  m_control->setBounds (200, 8, 70, 24);
  m_alt->setBounds (290, 8, 70, 24);

  for (int i = 0; i < 8; i++) {
    m_labelAction[i]->setBounds (16 + (i % 4) * 125, i < 4 ? 42 : 76, 126, 24);
  }
}

void ActionsDialogComponent::labelTextChanged (Label* labelThatHasChanged)
{
  int modifier = (m_alt->getToggleState() << 3) + (m_control->getToggleState() << 2) + (m_option->getToggleState() << 1) + m_shift->getToggleState();
  for (int i = 0; i < 8; i++) {
    if (labelThatHasChanged == m_labelAction[i])
    {
      String newText =  m_labelAction[i]->getText().substring(0, 13);
      m_labelAction[i]->setText(newText, false);
      m_pActionDisplay->setLabel(modifier, i, newText);
    }
  }
  m_pActionDisplay->writeConfigFile();
}

void ActionsDialogComponent::buttonClicked (Button* buttonThatWasClicked)
{
  fillActionLabels();
}

void ActionsDialogComponent::fillActionLabels()
{
  int modifier = (m_alt->getToggleState() << 3) + (m_control->getToggleState() << 2) + (m_option->getToggleState() << 1) + m_shift->getToggleState();
  for (int i = 0; i < 8; i++) {
    m_labelAction[i]->setText(m_pActionDisplay->getLabel(modifier, i), false);
  }
}
