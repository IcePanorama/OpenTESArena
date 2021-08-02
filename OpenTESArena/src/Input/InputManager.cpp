#include <algorithm>
#include <array>
#include <optional>
#include <type_traits>

#include "InputActionType.h"
#include "InputManager.h"
#include "InputStateType.h"
#include "../Game/Game.h"
#include "../Rendering/Renderer.h"
#include "../UI/Button.h"

#include "components/debug/Debug.h"

namespace
{
	// Supported mouse buttons used by the game.
	constexpr std::array<MouseButtonType, 2> MouseButtonTypes =
	{
		MouseButtonType::Left,
		MouseButtonType::Right
	};

	uint16_t GetFilteredSdlKeymod(uint16_t keymod)
	{
		// Ignore Num/Caps/Scroll Lock.
		return keymod & 0x0FFF;
	}

	int GetSdlMouseButton(MouseButtonType buttonType)
	{
		switch (buttonType)
		{
		case MouseButtonType::Left:
			return SDL_BUTTON_LEFT;
		case MouseButtonType::Right:
			return SDL_BUTTON_RIGHT;
		default:
			DebugUnhandledReturnMsg(int, std::to_string(static_cast<int>(buttonType)));
		}
	}

	std::optional<MouseButtonType> GetMouseButtonType(int sdlMouseButton)
	{
		switch (sdlMouseButton)
		{
		case SDL_BUTTON_LEFT:
			return MouseButtonType::Left;
		case SDL_BUTTON_RIGHT:
			return MouseButtonType::Right;
		default:
			return std::nullopt;
		}
	}
}

void InputManager::ListenerLookupEntry::init(ListenerType type, int index)
{
	this->type = type;
	this->index = index;
}

void InputManager::InputActionListenerEntry::init(const std::string_view &actionName, const InputActionCallback &callback)
{
	this->actionName = std::string(actionName);
	this->callback = callback;
	this->enabled = true;
}

void InputManager::InputActionListenerEntry::reset()
{
	this->actionName.clear();
	this->callback = [](const InputActionCallbackValues&) { };
	this->enabled = false;
}

void InputManager::MouseButtonChangedListenerEntry::init(const MouseButtonChangedCallback &callback)
{
	this->callback = callback;
	this->enabled = true;
}

void InputManager::MouseButtonChangedListenerEntry::reset()
{
	this->callback = [](Game&, MouseButtonType, const Int2&, bool) { };
	this->enabled = false;
}

void InputManager::MouseButtonHeldListenerEntry::init(const MouseButtonHeldCallback &callback)
{
	this->callback = callback;
	this->enabled = true;
}

void InputManager::MouseButtonHeldListenerEntry::reset()
{
	this->callback = [](Game&, MouseButtonType, const Int2&, double) { };
	this->enabled = false;
}

void InputManager::MouseScrollChangedListenerEntry::init(const MouseScrollChangedCallback &callback)
{
	this->callback = callback;
	this->enabled = true;
}

void InputManager::MouseScrollChangedListenerEntry::reset()
{
	this->callback = [](Game&, MouseWheelScrollType, const Int2&) { };
	this->enabled = false;
}

void InputManager::MouseMotionListenerEntry::init(const MouseMotionCallback &callback)
{
	this->callback = callback;
	this->enabled = true;
}

void InputManager::MouseMotionListenerEntry::reset()
{
	this->callback = [](Game&, int, int) { };
	this->enabled = false;
}

void InputManager::ApplicationExitListenerEntry::init(const ApplicationExitCallback &callback)
{
	this->callback = callback;
	this->enabled = true;
}

void InputManager::ApplicationExitListenerEntry::reset()
{
	this->callback = []() { };
	this->enabled = false;
}

void InputManager::WindowResizedListenerEntry::init(const WindowResizedCallback &callback)
{
	this->callback = callback;
	this->enabled = true;
}

void InputManager::WindowResizedListenerEntry::reset()
{
	this->callback = [](int, int) { };
	this->enabled = false;
}

InputManager::InputManager()
	: mouseDelta(0, 0)
{
	this->nextListenerID = 0;
}

void InputManager::init()
{
	// Add input action maps to be enabled/disabled as needed.
	this->inputActionMaps = InputActionMap::loadDefaultMaps();

	// Disable text input mode (for some reason it's on by default)?
	SDL_StopTextInput();
}

bool InputManager::keyPressed(const SDL_Event &e, SDL_Keycode keycode) const
{
	return (e.type == SDL_KEYDOWN) && (e.key.keysym.sym == keycode) && (e.key.repeat == 0);
}

bool InputManager::keyReleased(const SDL_Event &e, SDL_Keycode keycode) const
{
	return (e.type == SDL_KEYUP) && (e.key.keysym.sym == keycode);
}

bool InputManager::keyIsDown(SDL_Scancode scancode) const
{
	const uint8_t *keys = SDL_GetKeyboardState(nullptr);
	return keys[scancode] != 0;
}

bool InputManager::isKeyEvent(const SDL_Event &e) const
{
	return ((e.type == SDL_KEYDOWN) || (e.type == SDL_KEYUP)) && (e.key.repeat == 0);
}

bool InputManager::keyIsUp(SDL_Scancode scancode) const
{
	const uint8_t *keys = SDL_GetKeyboardState(nullptr);
	return keys[scancode] == 0;
}

bool InputManager::mouseButtonPressed(const SDL_Event &e, uint8_t button) const
{
	return (e.type == SDL_MOUSEBUTTONDOWN) && (e.button.button == button);
}

bool InputManager::mouseButtonReleased(const SDL_Event &e, uint8_t button) const
{
	return (e.type == SDL_MOUSEBUTTONUP) && (e.button.button == button);
}

bool InputManager::mouseButtonIsDown(uint8_t button) const
{
	const uint32_t mouse = SDL_GetMouseState(nullptr, nullptr);
	return (mouse & SDL_BUTTON(button)) != 0;
}

bool InputManager::mouseButtonIsUp(uint8_t button) const
{
	const uint32_t mouse = SDL_GetMouseState(nullptr, nullptr);
	return (mouse & SDL_BUTTON(button)) == 0;
}

bool InputManager::isMouseButtonEvent(const SDL_Event &e) const
{
	return (e.type == SDL_MOUSEBUTTONDOWN) || (e.type == SDL_MOUSEBUTTONUP);
}

bool InputManager::isMouseWheelEvent(const SDL_Event &e) const
{
	return e.type == SDL_MOUSEWHEEL;
}

bool InputManager::isMouseMotionEvent(const SDL_Event &e) const
{
	return e.type == SDL_MOUSEMOTION;
}

bool InputManager::mouseWheeledUp(const SDL_Event &e) const
{
	return (e.type == SDL_MOUSEWHEEL) && (e.wheel.y > 0);
}

bool InputManager::mouseWheeledDown(const SDL_Event &e) const
{
	return (e.type == SDL_MOUSEWHEEL) && (e.wheel.y < 0);
}

bool InputManager::windowResized(const SDL_Event &e) const
{
	return (e.type == SDL_WINDOWEVENT) && (e.window.event == SDL_WINDOWEVENT_RESIZED);
}

bool InputManager::applicationExit(const SDL_Event &e) const
{
	return e.type == SDL_QUIT;
}

InputManager::ListenerID InputManager::getNextListenerID()
{
	ListenerID listenerID;
	if (!this->freedListenerIDs.empty())
	{
		listenerID = this->freedListenerIDs.back();
		this->freedListenerIDs.pop_back();
	}
	else
	{
		listenerID = this->nextListenerID;
		this->nextListenerID++;
	}

	return listenerID;
}

Int2 InputManager::getMousePosition() const
{
	int x, y;
	SDL_GetMouseState(&x, &y);
	return Int2(x, y);
}

Int2 InputManager::getMouseDelta() const
{
	return this->mouseDelta;
}

int InputManager::getEventCount() const
{
	return static_cast<int>(this->cachedEvents.size());
}

const SDL_Event &InputManager::getEvent(int index) const
{
	DebugAssertIndex(this->cachedEvents, index);
	return this->cachedEvents[index];
}

bool InputManager::setInputActionMapActive(const std::string &name, bool active)
{
	const auto iter = std::find_if(this->inputActionMaps.begin(), this->inputActionMaps.end(),
		[&name](const InputActionMap &map)
	{
		return map.name == name;
	});

	if (iter != this->inputActionMaps.end())
	{
		InputActionMap &map = *iter;
		map.active = active;
		return true;
	}
	else
	{
		DebugLogWarning("Couldn't find input action map \"" + name + "\".");
		return false;
	}
}

template <typename EntryType, typename CallbackType>
InputManager::ListenerID InputManager::addListenerInternal(CallbackType &&callback, ListenerType listenerType,
	std::vector<EntryType> &listeners, std::vector<int> &freedListenerIndices)
{
	int insertIndex;
	if (!freedListenerIndices.empty())
	{
		insertIndex = freedListenerIndices.back();
		freedListenerIndices.pop_back();
	}
	else
	{
		insertIndex = static_cast<int>(listeners.size());
		listeners.emplace_back(EntryType());
	}

	DebugAssertIndex(listeners, insertIndex);
	EntryType &listenerEntry = listeners[insertIndex];
	listenerEntry.init(callback);

	const ListenerID listenerID = this->getNextListenerID();

	ListenerLookupEntry lookupEntry;
	lookupEntry.init(listenerType, insertIndex);
	this->listenerLookupEntries.emplace(listenerID, std::move(lookupEntry));

	return listenerID;
}

InputManager::ListenerID InputManager::addInputActionListener(const std::string_view &actionName,
	const InputActionCallback &callback)
{
	int insertIndex;
	if (!this->freedInputActionListenerIndices.empty())
	{
		insertIndex = this->freedInputActionListenerIndices.back();
		this->freedInputActionListenerIndices.pop_back();
	}
	else
	{
		insertIndex = static_cast<int>(this->inputActionListeners.size());
		this->inputActionListeners.emplace_back(InputActionListenerEntry());
	}

	DebugAssertIndex(this->inputActionListeners, insertIndex);
	InputActionListenerEntry &listenerEntry = this->inputActionListeners[insertIndex];
	listenerEntry.init(actionName, callback);

	const ListenerID listenerID = this->getNextListenerID();

	ListenerLookupEntry lookupEntry;
	lookupEntry.init(ListenerType::InputAction, insertIndex);
	this->listenerLookupEntries.emplace(listenerID, std::move(lookupEntry));

	return listenerID;
}

InputManager::ListenerID InputManager::addMouseButtonChangedListener(const MouseButtonChangedCallback &callback)
{
	return this->addListenerInternal(callback, ListenerType::MouseButtonChanged,
		this->mouseButtonChangedListeners, this->freedMouseButtonChangedListenerIndices);
}

InputManager::ListenerID InputManager::addMouseButtonHeldListener(const MouseButtonHeldCallback &callback)
{
	return this->addListenerInternal(callback, ListenerType::MouseButtonHeld,
		this->mouseButtonHeldListeners, this->freedMouseButtonHeldListenerIndices);
}

InputManager::ListenerID InputManager::addMouseScrollChangedListener(const MouseScrollChangedCallback &callback)
{
	return this->addListenerInternal(callback, ListenerType::MouseScrollChanged,
		this->mouseScrollChangedListeners, this->freedMouseScrollChangedListenerIndices);
}

InputManager::ListenerID InputManager::addMouseMotionListener(const MouseMotionCallback &callback)
{
	return this->addListenerInternal(callback, ListenerType::MouseMotion,
		this->mouseMotionListeners, this->freedMouseMotionListenerIndices);
}

InputManager::ListenerID InputManager::addApplicationExitListener(const ApplicationExitCallback &callback)
{
	return this->addListenerInternal(callback, ListenerType::ApplicationExit,
		this->applicationExitListeners, this->freedApplicationExitListenerIndices);
}

InputManager::ListenerID InputManager::addWindowResizedListener(const WindowResizedCallback &callback)
{
	return this->addListenerInternal(callback, ListenerType::WindowResized,
		this->windowResizedListeners, this->freedWindowResizedListenerIndices);
}

void InputManager::removeListener(ListenerID id)
{
	auto resetListenerEntry = [](auto &listeners, auto &freedIndices, int removeIndex)
	{
		// Need to reset the entry itself so we don't have to add isValid conditions when iterating them
		// in update().
		DebugAssertIndex(listeners, removeIndex);
		auto &entry = listeners[removeIndex];
		entry.reset();

		freedIndices.emplace_back(removeIndex);
	};

	const auto iter = this->listenerLookupEntries.find(id);
	if (iter != this->listenerLookupEntries.end())
	{
		const ListenerLookupEntry &lookupEntry = iter->second;
		const ListenerType listenerType = lookupEntry.type;
		const int index = lookupEntry.index;
		if (listenerType == ListenerType::InputAction)
		{
			resetListenerEntry(this->inputActionListeners, this->freedInputActionListenerIndices, index);
		}
		else if (listenerType == ListenerType::MouseButtonChanged)
		{
			resetListenerEntry(this->mouseButtonChangedListeners, this->freedMouseButtonChangedListenerIndices, index);
		}
		else if (listenerType == ListenerType::MouseButtonHeld)
		{
			resetListenerEntry(this->mouseButtonHeldListeners, this->freedMouseButtonHeldListenerIndices, index);
		}
		else if (listenerType == ListenerType::MouseScrollChanged)
		{
			resetListenerEntry(this->mouseScrollChangedListeners, this->freedMouseScrollChangedListenerIndices, index);
		}
		else if (listenerType == ListenerType::MouseMotion)
		{
			resetListenerEntry(this->mouseMotionListeners, this->freedMouseMotionListenerIndices, index);
		}
		else if (listenerType == ListenerType::ApplicationExit)
		{
			resetListenerEntry(this->applicationExitListeners, this->freedApplicationExitListenerIndices, index);
		}
		else if (listenerType == ListenerType::WindowResized)
		{
			resetListenerEntry(this->windowResizedListeners, this->freedWindowResizedListenerIndices, index);
		}
		else
		{
			DebugNotImplementedMsg(std::to_string(static_cast<int>(listenerType)));
		}

		this->freedListenerIDs.emplace_back(id);

		// Remove the means of looking up this entry.
		this->listenerLookupEntries.erase(iter);
	}
	else
	{
		DebugLogWarning("No entry to remove for listener " + std::to_string(id) + ".");
	}
}

void InputManager::setListenerEnabled(ListenerID id, bool enabled)
{
	const auto iter = this->listenerLookupEntries.find(id);
	if (iter == this->listenerLookupEntries.end())
	{
		DebugLogError("Couldn't find listener " + std::to_string(id) + " to set enabled/disabled.");
		return;
	}

	auto setEnabled = [enabled](auto &vec, int index)
	{
		DebugAssertIndex(vec, index);
		vec[index].enabled = enabled;
	};

	const ListenerLookupEntry &lookupEntry = iter->second;
	const ListenerType listenerType = lookupEntry.type;
	const int index = lookupEntry.index;
	if (listenerType == ListenerType::InputAction)
	{
		setEnabled(this->inputActionListeners, index);
	}
	else if (listenerType == ListenerType::MouseButtonChanged)
	{
		setEnabled(this->mouseButtonChangedListeners, index);
	}
	else if (listenerType == ListenerType::MouseButtonHeld)
	{
		setEnabled(this->mouseButtonHeldListeners, index);
	}
	else if (listenerType == ListenerType::MouseScrollChanged)
	{
		setEnabled(this->mouseScrollChangedListeners, index);
	}
	else if (listenerType == ListenerType::MouseMotion)
	{
		setEnabled(this->mouseMotionListeners, index);
	}
	else if (listenerType == ListenerType::ApplicationExit)
	{
		setEnabled(this->applicationExitListeners, index);
	}
	else if (listenerType == ListenerType::WindowResized)
	{
		setEnabled(this->windowResizedListeners, index);
	}
	else
	{
		DebugNotImplementedMsg(std::to_string(static_cast<int>(listenerType)));
	}
}

void InputManager::setRelativeMouseMode(bool active)
{
	const SDL_bool enabled = active ? SDL_TRUE : SDL_FALSE;
	SDL_SetRelativeMouseMode(enabled);
}

void InputManager::cacheSdlEvents()
{
	this->cachedEvents.clear();
	SDL_Event e;
	while (SDL_PollEvent(&e) != 0)
	{
		this->cachedEvents.emplace_back(std::move(e));
	}
}

bool InputManager::isInTextEntryMode() const
{
	const SDL_bool inTextEntryMode = SDL_IsTextInputActive();
	return inTextEntryMode == SDL_TRUE;
}

void InputManager::handleHeldInputs(Game &game, uint32_t mouseState, const Int2 &mousePosition, double dt)
{
	auto handleHeldMouseButton = [this, &game, mouseState, &mousePosition, dt](MouseButtonType buttonType)
	{
		const int sdlMouseButton = GetSdlMouseButton(buttonType);
		const bool isButtonHeld = (mouseState & SDL_BUTTON(sdlMouseButton)) != 0;
		if (isButtonHeld)
		{
			for (const MouseButtonHeldListenerEntry &entry : this->mouseButtonHeldListeners)
			{
				if (entry.enabled)
				{
					entry.callback(game, buttonType, mousePosition, dt);
				}
			}
		}
	};

	for (const MouseButtonType buttonType : MouseButtonTypes)
	{
		handleHeldMouseButton(buttonType);
	}

	const uint8_t *keyboardState = SDL_GetKeyboardState(nullptr);
	const SDL_Keymod keyboardMod = SDL_GetModState();

	for (const InputActionMap &map : this->inputActionMaps)
	{
		if (map.active && (!this->isInTextEntryMode() || map.allowedDuringTextEntry))
		{
			for (const InputActionDefinition &def : map.defs)
			{
				if (def.stateType == InputStateType::Performing)
				{
					if (def.type == InputActionType::MouseButton)
					{
						const InputActionDefinition::MouseButtonDefinition &mouseButtonDef = def.mouseButtonDef;
						handleHeldMouseButton(mouseButtonDef.type);
					}
					else if (def.type == InputActionType::Key)
					{
						const InputActionDefinition::KeyDefinition &keyDef = def.keyDef;
						const SDL_Scancode scancode = SDL_GetScancodeFromKey(keyDef.keycode);
						const bool isKeyHeld = (keyboardState[scancode] != 0) && (keyDef.keymod == keyboardMod);
						if (isKeyHeld)
						{
							for (const InputActionListenerEntry &entry : this->inputActionListeners)
							{
								if (entry.enabled && (entry.actionName == def.name))
								{
									InputActionCallbackValues values(game, false, true, false);
									entry.callback(values);
								}
							}
						}
					}
				}
			}
		}
	}
}

void InputManager::update(Game &game, double dt, const BufferView<const ButtonProxy> &buttonProxies,
	const std::function<void()> &onFinishedProcessingEvent)
{
	// @temp: need to allow panel SDL_Events to be processed twice for compatibility with the
	// old event handling in Game::handleEvents().
	this->cacheSdlEvents();

	// @todo: don't save mouse delta as member, just keep local variable here once we can.
	SDL_GetRelativeMouseState(&this->mouseDelta.x, &this->mouseDelta.y);

	// Handle held mouse buttons and keys.
	Int2 mousePosition;
	const uint32_t mouseState = SDL_GetMouseState(&mousePosition.x, &mousePosition.y);
	this->handleHeldInputs(game, mouseState, mousePosition, dt);

	// Handle SDL events.
	// @todo: make sure to not fire duplicate callbacks for the same input action if it is registered to multiple
	// keys/mouse buttons like Skip.
	for (const SDL_Event &e : this->cachedEvents)
	{
		if (this->applicationExit(e))
		{
			for (const ApplicationExitListenerEntry &entry : this->applicationExitListeners)
			{
				if (entry.enabled)
				{
					entry.callback();
				}
			}
		}
		else if (this->windowResized(e))
		{
			const int width = e.window.data1;
			const int height = e.window.data2;
			for (const WindowResizedListenerEntry &entry : this->windowResizedListeners)
			{
				if (entry.enabled)
				{
					entry.callback(width, height);
				}
			}
		}
		else if (this->isKeyEvent(e))
		{
			static_assert(std::is_same_v<InputActionDefinition::KeyDefinition::Keymod, decltype(e.key.keysym.mod)>);

			const SDL_Keycode keycode = e.key.keysym.sym;
			const InputActionDefinition::KeyDefinition::Keymod keymod = GetFilteredSdlKeymod(e.key.keysym.mod);
			const bool isKeyDown = e.type == SDL_KEYDOWN;
			const bool isKeyUp = e.type == SDL_KEYUP;

			for (const InputActionMap &map : this->inputActionMaps)
			{
				if (map.active && (!this->isInTextEntryMode() || map.allowedDuringTextEntry))
				{
					for (const InputActionDefinition &def : map.defs)
					{
						const bool matchesStateType = (isKeyDown && def.stateType == InputStateType::BeginPerform) ||
							(isKeyUp && def.stateType == InputStateType::EndPerform);

						if ((def.type == InputActionType::Key) && matchesStateType)
						{
							const InputActionDefinition::KeyDefinition &keyDef = def.keyDef;

							// Handle the keymod as an exact comparison; if the definition specifies LCtrl and RCtrl,
							// both must be held, so combinations like Ctrl + Alt + Delete are possible.
							if ((keyDef.keycode == keycode) && (keyDef.keymod == keymod))
							{
								for (const InputActionListenerEntry &entry : this->inputActionListeners)
								{
									if (entry.enabled && (entry.actionName == def.name))
									{
										InputActionCallbackValues values(game, isKeyDown, false, isKeyUp);
										entry.callback(values);
									}
								}
							}
						}
					}
				}
			}
		}
		else if (this->isMouseButtonEvent(e))
		{
			const std::optional<MouseButtonType> buttonType = GetMouseButtonType(e.button.button);

			if (buttonType.has_value())
			{
				const bool isButtonPress = e.type == SDL_MOUSEBUTTONDOWN;
				const bool isButtonRelease = e.type == SDL_MOUSEBUTTONUP;

				if (isButtonPress)
				{
					// Check for clicked buttons in the UI.
					for (int i = 0; i < buttonProxies.getCount(); i++)
					{
						const ButtonProxy &buttonProxy = buttonProxies.get(i);
						const bool isButtonActive = !buttonProxy.isActiveFunc || buttonProxy.isActiveFunc();
						if (isButtonActive)
						{
							const Int2 classicMousePos = game.getRenderer().nativeToOriginal(mousePosition);

							DebugAssert(buttonProxy.rectFunc);
							const Rect buttonRect = buttonProxy.rectFunc();
							const bool isValidMouseSelection = buttonRect.contains(classicMousePos);
							const bool matchesButtonType = *buttonType == buttonProxy.buttonType;
							if (isValidMouseSelection && matchesButtonType)
							{
								buttonProxy.callback();
								break;
							}
						}
					}
				}

				for (const MouseButtonChangedListenerEntry &entry : this->mouseButtonChangedListeners)
				{
					if (entry.enabled)
					{
						entry.callback(game, *buttonType, mousePosition, isButtonPress);
					}
				}

				for (const InputActionMap &map : this->inputActionMaps)
				{
					if (map.active)
					{
						for (const InputActionDefinition &def : map.defs)
						{
							const bool matchesStateType = (isButtonPress && def.stateType == InputStateType::BeginPerform) ||
								(isButtonRelease && def.stateType == InputStateType::EndPerform);

							if ((def.type == InputActionType::MouseButton) && matchesStateType)
							{
								const InputActionDefinition::MouseButtonDefinition &mouseButtonDef = def.mouseButtonDef;
								if (mouseButtonDef.type == *buttonType)
								{
									for (const InputActionListenerEntry &entry : this->inputActionListeners)
									{
										if (entry.enabled && (entry.actionName == def.name))
										{
											InputActionCallbackValues values(game, isButtonPress, false, isButtonRelease);
											entry.callback(values);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else if (this->isMouseWheelEvent(e))
		{
			const std::optional<MouseWheelScrollType> scrollType = [&e]() -> std::optional<MouseWheelScrollType>
			{
				if (e.wheel.y < 0)
				{
					return MouseWheelScrollType::Down;
				}
				else if (e.wheel.y > 0)
				{
					return MouseWheelScrollType::Up;
				}
				else
				{
					return std::nullopt;
				}
			}();

			if (scrollType.has_value())
			{
				for (const MouseScrollChangedListenerEntry &entry : this->mouseScrollChangedListeners)
				{
					if (entry.enabled)
					{
						entry.callback(game, *scrollType, mousePosition);
					}
				}

				for (const InputActionMap &map : this->inputActionMaps)
				{
					if (map.active)
					{
						for (const InputActionDefinition &def : map.defs)
						{
							const bool matchesStateType = !def.stateType.has_value();

							if ((def.type == InputActionType::MouseWheel) && matchesStateType)
							{
								const InputActionDefinition::MouseScrollDefinition &mouseScrollDef = def.mouseScrollDef;
								if (mouseScrollDef.type == *scrollType)
								{
									for (const InputActionListenerEntry &entry : this->inputActionListeners)
									{
										if (entry.enabled && (entry.actionName == def.name))
										{
											InputActionCallbackValues values(game, true, false, false);
											entry.callback(values);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else if (this->isMouseMotionEvent(e))
		{
			for (const MouseMotionListenerEntry &entry : this->mouseMotionListeners)
			{
				if (entry.enabled)
				{
					entry.callback(game, this->mouseDelta.x, this->mouseDelta.y);
				}
			}
		}

		onFinishedProcessingEvent();
	}
}
