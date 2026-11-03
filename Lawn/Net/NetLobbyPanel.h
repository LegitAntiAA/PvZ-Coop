#ifndef __NETLOBBYPANEL_H__
#define __NETLOBBYPANEL_H__

#include "NetCommon.h"
#include "../../SexyAppFramework/Widget.h"
#include "../../SexyAppFramework/EditListener.h"

namespace Sexy
{
	class EditWidget;
	class WidgetManager;
	class Graphics;
}

class LawnApp;

using namespace Sexy;

namespace LawnNet
{

/**
 * Compact co-op panel that hangs on the left side of the main menu.
 *
 * Not connected: nickname + IP + port inputs, HOST and CONNECT buttons.
 * Connected: player list (nick + ping + team), PvP team buttons, disconnect.
 * Also draws remote player cursors (with nick + ping labels) while in menus.
 * Hidden while a Board is active (the board renders cursors itself).
 */
class NetLobbyPanel : public Widget, public EditListener
{
public:
	enum
	{
		BTN_HOST = 1,
		BTN_CONNECT,
		BTN_DISCONNECT,
		BTN_TEAM_PLANTS,
		BTN_TEAM_ZOMBIES
	};

public:
	NetLobbyPanel(LawnApp* theApp);
	virtual ~NetLobbyPanel();

	virtual void Update() override;
	virtual void Draw(Graphics* g) override;
	virtual void MouseDown(int x, int y, int theClickCount) override;
	virtual void MouseMove(int x, int y) override;
	virtual void MouseLeave() override;
	virtual void AddedToManager(WidgetManager* theWidgetManager) override;
	virtual void RemovedFromManager(WidgetManager* theWidgetManager) override;

	// called by LawnApp on edit results
	void ApplyNick();

private:
	struct UiButton
	{
		int mId;
		Rect mRect;
		SexyString mLabel;
		bool mDisabled;
		bool mMouseOver;
	};

	void LayoutButtons();
	void DrawButton(Graphics* g, const UiButton& theButton);
	Rect GetPanelRect() const;

	LawnApp* mApp;
	EditWidget* mNickEdit;
	EditWidget* mIpEdit;
	EditWidget* mPortEdit;
	std::vector<UiButton> mButtons;
	int mHoverButtonId;
	uint32_t mLastCursorSend;
};

} // namespace LawnNet

#endif // __NETLOBBYPANEL_H__
