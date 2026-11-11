#include "MoreSettingsDialog.h"
#include "../../LawnApp.h"
#include "../../ConstEnums.h"
#include "../../SexyAppFramework/Slider.h"
#include "../../SexyAppFramework/Font.h"
#include "../../Resources.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../Net/NetManager.h"
#include "../../Sexy.TodLib/Reanimator.h"

using namespace Sexy;
using namespace LawnNet;

namespace
{
int AlphaFromSlider(double theValue)
{
	return max(0, min(255, (int)(theValue * 255.0 + 0.5)));
}

double SliderFromAlpha(int theAlpha)
{
	return max(0, min(255, theAlpha)) / 255.0;
}
}

MoreSettingsDialog::MoreSettingsDialog(LawnApp* theApp) :
	LawnDialog(theApp, Dialogs::DIALOG_MORESETTINGS, true, _S("CO-OP APPEARANCE"), _S(""), _S("CLOSE"), Dialog::BUTTONS_FOOTER)
{
	mApp = theApp;
	mSavePending = false;
	Plant::PreloadPlantResources(SEED_SUNFLOWER);
	mPreviewReanim = new Reanimation();
	mPreviewReanim->ReanimationInitializeType(365.0f, 278.0f, REANIM_SUNFLOWER);
	mPreviewReanim->PlayReanim("anim_idle", REANIM_LOOP, 0, 12.0f);
	mPreviewReanim->OverrideScale(0.85f, 0.85f);
	mHeldPlantTintSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, MoreSettingsDialog_HeldPlantTint, this);
	mHotbarTintSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, MoreSettingsDialog_HotbarTint, this);
	mHeldPlantOutlineSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, MoreSettingsDialog_HeldPlantOutline, this);

	NetConfig aDefaults;
	const NetConfig& aConfig = mApp->mNetManager ? mApp->mNetManager->mConfig : aDefaults;
	mHeldPlantTintSlider->SetValue(SliderFromAlpha(aConfig.mHeldPlantTintAlpha));
	mHotbarTintSlider->SetValue(SliderFromAlpha(aConfig.mHotbarTintAlpha));
	mHeldPlantOutlineSlider->SetValue(SliderFromAlpha(aConfig.mHeldPlantOutlineAlpha));

	Resize(0, 0, 600, 450);
	LawnApp::CenterDialog(this, mWidth, mHeight);
}

MoreSettingsDialog::~MoreSettingsDialog() 
{
	SaveConfig();
	delete mPreviewReanim;
	delete mHeldPlantTintSlider;
	delete mHotbarTintSlider;
	delete mHeldPlantOutlineSlider;
}

void MoreSettingsDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager) 
{
	LawnDialog::AddedToManager(theWidgetManager);
	AddWidget(mHeldPlantTintSlider);
	AddWidget(mHotbarTintSlider);
	AddWidget(mHeldPlantOutlineSlider);
}

void MoreSettingsDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mHeldPlantTintSlider);
	RemoveWidget(mHotbarTintSlider);
	RemoveWidget(mHeldPlantOutlineSlider);
}

void MoreSettingsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);
	mHeldPlantTintSlider->Resize(275, 112, 150, 40);
	mHotbarTintSlider->Resize(275, 162, 150, 40);
	mHeldPlantOutlineSlider->Resize(275, 212, 150, 40);
}

void MoreSettingsDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);
	const Color aTextColor(107, 109, 145);
	const NetColor aPreviewColor = GetPlayerColorBySlot(1);
	NetConfig aDefaults;
	const NetConfig& aConfig = mApp->mNetManager ? mApp->mNetManager->mConfig : aDefaults;

	TodDrawString(g, _S("HELD PLANT TINT"), 255, 138, FONT_DWARVENTODCRAFT18, aTextColor, DS_ALIGN_RIGHT);
	TodDrawString(g, _S("HOTBAR TINT"), 255, 188, FONT_DWARVENTODCRAFT18, aTextColor, DS_ALIGN_RIGHT);
	TodDrawString(g, _S("DARK OUTLINE"), 255, 238, FONT_DWARVENTODCRAFT18, aTextColor, DS_ALIGN_RIGHT);
	TodDrawString(g, StrFormat(_S("%d%%"), aConfig.mHeldPlantTintAlpha * 100 / 255), 445, 138,
		FONT_DWARVENTODCRAFT18, aTextColor, DS_ALIGN_LEFT);
	TodDrawString(g, StrFormat(_S("%d%%"), aConfig.mHotbarTintAlpha * 100 / 255), 445, 188,
		FONT_DWARVENTODCRAFT18, aTextColor, DS_ALIGN_LEFT);
	TodDrawString(g, StrFormat(_S("%d%%"), aConfig.mHeldPlantOutlineAlpha * 100 / 255), 445, 238,
		FONT_DWARVENTODCRAFT18, aTextColor, DS_ALIGN_LEFT);

	TodDrawString(g, _S("LIVE PREVIEW"), 300, 276, FONT_DWARVENTODCRAFT18, aTextColor, DS_ALIGN_CENTER);
	DrawSeedPacket(g, 155.0f, 292.0f, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE,
		0.0f, 255, false, false);
	g->PushState();
	g->SetColor(Color(aPreviewColor.r, aPreviewColor.g, aPreviewColor.b, aConfig.mHotbarTintAlpha));
	g->FillRect(157, 294, 46, 66);
	g->PopState();

	if (aConfig.mHeldPlantOutlineAlpha > 0)
	{
		mPreviewReanim->mColorOverride = Color(aPreviewColor.r * 3 / 5,
			aPreviewColor.g * 3 / 5, aPreviewColor.b * 3 / 5, aConfig.mHeldPlantOutlineAlpha);
		static const int aOffsets[4][2] = { { -2, 0 }, { 2, 0 }, { 0, -2 }, { 0, 2 } };
		for (int i = 0; i < 4; i++)
		{
			Graphics anOutline(*g);
			anOutline.Translate(aOffsets[i][0], aOffsets[i][1]);
			mPreviewReanim->Draw(&anOutline);
		}
	}
	mPreviewReanim->mColorOverride = Color(aPreviewColor.r, aPreviewColor.g,
		aPreviewColor.b, aConfig.mHeldPlantTintAlpha);
	mPreviewReanim->Draw(g);
}

void MoreSettingsDialog::SliderVal(int theId, double theVal)
{
	if (!mApp->mNetManager)
		return;

	const int anAlpha = AlphaFromSlider(theVal);
	switch (theId)
	{
	case MoreSettingsDialog_HeldPlantTint:
		mApp->mNetManager->mConfig.mHeldPlantTintAlpha = anAlpha;
		break;
	case MoreSettingsDialog_HotbarTint:
		mApp->mNetManager->mConfig.mHotbarTintAlpha = anAlpha;
		break;
	case MoreSettingsDialog_HeldPlantOutline:
		mApp->mNetManager->mConfig.mHeldPlantOutlineAlpha = anAlpha;
		break;
	default:
		return;
	}
	mSavePending = true;
	MarkDirty();
}

void MoreSettingsDialog::SaveConfig()
{
	if (!mSavePending || !mApp || !mApp->mNetManager)
		return;
	NetSaveConfig(mApp->mNetManager->mConfig);
	mSavePending = false;
}

void MoreSettingsDialog::ButtonDepress(int theId)
{
	SaveConfig();
	LawnDialog::ButtonDepress(theId);
}

void MoreSettingsDialog::Update()
{
	LawnDialog::Update();
	mPreviewReanim->Update();
	MarkDirty();
	if (mSavePending && !mHeldPlantTintSlider->mDragging &&
		!mHotbarTintSlider->mDragging && !mHeldPlantOutlineSlider->mDragging)
	{
		SaveConfig();
	}
}
