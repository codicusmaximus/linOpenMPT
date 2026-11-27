/*
 * PluginComboBox.cpp
 * ------------------
 * Purpose: Combo box utilities for plugin and plugin parameter lists
 * Notes  : mfcless twin of upstream's mptrack/PluginComboBox.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PluginComboBox.h"
#include "mfcless/Widgets.h"
#include "UpdateHints.h"
#include "Vstplug.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/plugins/PluginManager.h"
#include "../soundlib/plugins/PlugInterface.h"

static constexpr auto PluginShiftBits = (sizeof(PLUGINDEX) * CHAR_BIT);
static constexpr PLUGINDEX GetPluginIndex(uintptr_t x) { return static_cast<PLUGINDEX>(x & ((1 << PluginShiftBits) - 1)); }


int PluginComboBox::Update(const Config config, const CSoundFile &sndFile)
{
	if(config.m_flags)
		m_flags = *config.m_flags;

	const PLUGINDEX curSelection = config.m_curSelection ? *config.m_curSelection : GetSelection().value_or(PLUGINDEX_INVALID);
	PLUGINDEX updatePlugin = PLUGINDEX_INVALID;
	if(config.m_hint)
	{
		bool onlyUpdateSelection = false;
		if(config.m_updateHintSender == this)
			onlyUpdateSelection = true;
		PluginHint pluginHint = config.m_hint->ToType<PluginHint>();
		if(!pluginHint.GetType()[HINT_MODTYPE | HINT_PLUGINNAMES])
			onlyUpdateSelection = true;
		updatePlugin = pluginHint.GetPlugin() ? pluginHint.GetPlugin() - 1 : PLUGINDEX_INVALID;

		if(onlyUpdateSelection)
		{
			if(!config.m_curSelection)
				return currentIndex();
			int items = count();
			for(int i = 0; i < items; i++)
			{
				if(GetPluginIndex(mfcless::GetItemData(this, i)) == curSelection)
				{
					setCurrentIndex(i);
					return i;
				}
			}
			return currentIndex();
		}
	}

	int selectedItem = -1;
	setUpdatesEnabled(false);
	
	if(!m_flags[DoNotResetContent] && updatePlugin == PLUGINDEX_INVALID)
		clear();

	int insertAt = count();
	if(m_flags[ShowNoPlugin] && updatePlugin == PLUGINDEX_INVALID)
	{
		mfcless::AddItem(this, UL_("No Plugin"), PLUGINDEX_INVALID);
		insertAt++;
		if(curSelection == PLUGINDEX_INVALID)
		{
			setCurrentIndex(selectedItem = 0);
		}
	} else if(updatePlugin != PLUGINDEX_INVALID)
	{
		int items = count();
		bool firstItemOfPlugin = true;
		for(int i = 0; i < items; i++)
		{
			auto thisPlugin = GetPluginIndex(mfcless::GetItemData(this, i));
			if(thisPlugin == updatePlugin)
			{
				removeItem(i);
				if(firstItemOfPlugin)
					insertAt = i;
				firstItemOfPlugin = false;
				i--;
				items--;
			}
			if(thisPlugin < MAX_MIXPLUGINS && thisPlugin > updatePlugin)
			{
				if(firstItemOfPlugin)
					insertAt = i;
				break;
			}
		}
	}
#ifndef NO_PLUGINS
	const auto fxFormat = MPT_UFORMAT("FX{}: ");
	const auto inputFormat = MPT_UFORMAT("    Stereo Input {}");
	mpt::ustring str;
	str.reserve(80);
	for(PLUGINDEX plug = config.m_firstPlugin; plug < MAX_MIXPLUGINS; plug++)
	{
		if(updatePlugin != PLUGINDEX_INVALID && plug != updatePlugin)
			continue;
		const SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[plug];
		if(!plugin.IsValidPlugin() && !m_flags[ShowEmptySlots] && curSelection != plug)
			continue;

		str.clear();
		str += fxFormat(plug + 1);
		const auto plugName = plugin.GetName(), libName = plugin.GetLibraryName();
		str += mpt::ToUnicode(plugName);
		if(m_flags[ShowLibraryNames] && plugName != libName && !libName.empty())
			str += UL_(" (") + mpt::ToUnicode(libName) + UL_(")");
		else if(plugName.empty() && (!m_flags[ShowLibraryNames] || libName.empty()))
			str += UL_("--");
#ifdef MPT_WITH_VST
		if(auto *vstPlug = dynamic_cast<const CVstPlugin *>(plugin.pMixPlugin); vstPlug != nullptr && vstPlug->isBridged)
		{
			const VSTPluginLib &lib = vstPlug->GetPluginFactory();
			str += MPT_UFORMAT(" ({} Bridged)")(lib.GetDllArchNameUser());
		}
#endif  // MPT_WITH_VST

		const int firstItem = mfcless::InsertItem(this, insertAt, str);
		insertAt = firstItem + 1;
		mfcless::SetItemData(this, firstItem, plug);

		if(m_flags[ShowInputs] && plugin.pMixPlugin != nullptr && plugin.pMixPlugin->GetNumInputChannels() > 2)
		{
			for(int ch = 2; ch < plugin.pMixPlugin->GetNumInputChannels(); ch += 2)
			{
				str = inputFormat(1 + ch / 2);
				mfcless::SetItemData(this, mfcless::InsertItem(this, insertAt, str), plug | (ch << PluginShiftBits));
				insertAt++;
			}
		}

		if(plug == curSelection)
		{
			selectedItem = firstItem;
			setCurrentIndex(selectedItem);
		}
	}
#endif // NO_PLUGINS
	update();
	setUpdatesEnabled(true);
	return selectedItem;
}


void PluginComboBox::SetSelection(PLUGINDEX plugin)
{
	int i = 0;
	if(plugin != PLUGINDEX_INVALID || !m_flags[ShowNoPlugin])
	{
		const int numItems = count();
		for(; i < numItems; i++)
		{
			if(GetPluginIndex(mfcless::GetItemData(this, i)) == plugin)
				break;
		}
	}
	setCurrentIndex(i);
}


std::optional<PLUGINDEX> PluginComboBox::GetSelection() const
{
	const auto sel = currentIndex();
	if(sel == -1)
		return std::nullopt;
	
	const uintptr_t itemData = mfcless::GetItemData(this, sel);
	PLUGINDEX plugin = GetPluginIndex(itemData);
	if(plugin < MAX_MIXPLUGINS)
		return plugin;
	else
		return std::nullopt;
}


void AddPluginParameternamesToCombobox(QComboBox &CBox, SNDMIXPLUGIN &plug)
{
#ifndef NO_PLUGINS
	if(plug.pMixPlugin)
		AddPluginParameternamesToCombobox(CBox, *plug.pMixPlugin);
#endif // NO_PLUGINS
}


void AddPluginParameternamesToCombobox(QComboBox &CBox, IMixPlugin &plug)
{
#ifndef NO_PLUGINS
	const PlugParamIndex numParams = plug.GetNumVisibleParameters();
	plug.CacheParameterNames(0, numParams);
	for(PlugParamIndex i = 0; i < numParams; i++)
	{
		mfcless::AddItem(&CBox, plug.GetFormattedParamName(i), i);
	}
#endif // NO_PLUGINS
}


OPENMPT_NAMESPACE_END
