#ifndef __MORESETTINGSDIALOG_H__
#define __MORESETTINGSDIALOG_H__

#include "LawnDialog.h"
#include "../../SexyAppFramework/SliderListener.h"

class LawnApp;
namespace Sexy
{
	class Slider;
};

class MoreSettingsDialog : public LawnDialog, public Sexy::SliderListener
{
private:
	enum
	{
		MoreSettingsDialog_HeldPlantTint = 20000,
		MoreSettingsDialog_HotbarTint,
		MoreSettingsDialog_HeldPlantOutline
	};
public:
	LawnApp*			mApp;
	Sexy::Slider*		mHeldPlantTintSlider;
	Sexy::Slider*		mHotbarTintSlider;
	Sexy::Slider*		mHeldPlantOutlineSlider;
	bool				mSavePending;

public:
	MoreSettingsDialog(LawnApp* theApp);
	~MoreSettingsDialog();
	void				AddedToManager(Sexy::WidgetManager* theWidgetManager);
	void				RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	void				Resize(int theX, int theY, int theWidth, int theHeight);
	void				Draw(Sexy::Graphics* g);
	void				SliderVal(int theId, double theVal);
	void				ButtonDepress(int theId);
	void				Update();
	void				SaveConfig();
};
#endif
