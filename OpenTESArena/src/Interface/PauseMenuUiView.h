#ifndef PAUSE_MENU_UI_VIEW_H
#define PAUSE_MENU_UI_VIEW_H

#include "../Math/Vector2.h"
#include "../Media/Color.h"
#include "../Media/TextureUtils.h"
#include "../UI/FontName.h"
#include "../UI/TextAlignment.h"

struct TextureAssetReference;

namespace PauseMenuUiView
{
	constexpr FontName VolumeFontName = FontName::Arena;
	const Color VolumeColor(12, 73, 16);
	constexpr TextAlignment VolumeTextAlignment = TextAlignment::Center;

	const Int2 SoundTextBoxCenterPoint(54, 96);
	const Int2 MusicTextBoxCenterPoint(127, 96);
	
	const Int2 OptionsTextBoxCenterPoint(234, 95);
	constexpr FontName OptionsButtonFontName = FontName::Arena;
	const Color OptionsButtonTextColor(215, 158, 4);
	constexpr TextAlignment OptionsButtonTextAlignment = TextAlignment::Center;
	constexpr TextureUtils::PatternType OptionsButtonPatternType = TextureUtils::PatternType::Custom1;
	const Color OptionsButtonTextShadowColor(101, 77, 24);
	const Int2 OptionsButtonTextShadowOffset(-1, 1);

	constexpr int NewGameButtonX = 0;
	constexpr int NewGameButtonY = 118;
	constexpr int NewGameButtonWidth = 65;
	constexpr int NewGameButtonHeight = 29;

	constexpr int LoadButtonX = 65;
	constexpr int LoadButtonY = 118;
	constexpr int LoadButtonWidth = 64;
	constexpr int LoadButtonHeight = 29;

	constexpr int SaveButtonX = 129;
	constexpr int SaveButtonY = 118;
	constexpr int SaveButtonWidth = 64;
	constexpr int SaveButtonHeight = 29;

	constexpr int ExitButtonX = 193;
	constexpr int ExitButtonY = 118;
	constexpr int ExitButtonWidth = 64;
	constexpr int ExitButtonHeight = 29;

	constexpr int ResumeButtonX = 257;
	constexpr int ResumeButtonY = 118;
	constexpr int ResumeButtonWidth = 64;
	constexpr int ResumeButtonHeight = 29;

	constexpr int OptionsButtonX = 162;
	constexpr int OptionsButtonY = 88;
	constexpr int OptionsButtonWidth = 145;
	constexpr int OptionsButtonHeight = 15;

	constexpr int SoundUpButtonX = 46;
	constexpr int SoundUpButtonY = 79;
	constexpr int SoundUpButtonWidth = 17;
	constexpr int SoundUpButtonHeight = 9;

	constexpr int SoundDownButtonX = 46;
	constexpr int SoundDownButtonY = 104;
	constexpr int SoundDownButtonWidth = 17;
	constexpr int SoundDownButtonHeight = 9;

	constexpr int MusicUpButtonX = 119;
	constexpr int MusicUpButtonY = 79;
	constexpr int MusicUpButtonWidth = 17;
	constexpr int MusicUpButtonHeight = 9;

	constexpr int MusicDownButtonX = 119;
	constexpr int MusicDownButtonY = 104;
	constexpr int MusicDownButtonWidth = 17;
	constexpr int MusicDownButtonHeight = 9;

	TextureAssetReference getBackgroundPaletteTextureAssetRef();
	TextureAssetReference getBackgroundTextureAssetRef();
}

#endif