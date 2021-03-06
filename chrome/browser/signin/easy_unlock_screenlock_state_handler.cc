// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/chromeos_utils.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

size_t kIconSize = 27u;
size_t kOpaqueIconOpacity = 50u;
size_t kSpinnerResourceWidth = 1215u;
size_t kSpinnerIntervalMs = 50u;

std::string GetIconURLForState(EasyUnlockScreenlockStateHandler::State state) {
  switch (state) {
    case EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH:
    case EasyUnlockScreenlockStateHandler::STATE_NO_PHONE:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_NEARBY:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE:
      return "chrome://theme/IDR_EASY_UNLOCK_LOCKED";
    case EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING:
      return "chrome://theme/IDR_EASY_UNLOCK_SPINNER";
    case EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED:
      return "chrome://theme/IDR_EASY_UNLOCK_UNLOCKED";
    default:
      return "";
  }
}

bool UseOpaqueIcon(EasyUnlockScreenlockStateHandler::State state) {
  return state == EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH ||
         state == EasyUnlockScreenlockStateHandler::STATE_NO_PHONE ||
         state == EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_NEARBY ||
         state == EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE;
}

bool HasAnimation(EasyUnlockScreenlockStateHandler::State state) {
  return state == EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING;
}

size_t GetTooltipResourceId(EasyUnlockScreenlockStateHandler::State state) {
  switch (state) {
    case EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_BLUETOOTH;
    case EasyUnlockScreenlockStateHandler::STATE_NO_PHONE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_PHONE;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_NOT_AUTHENTICATED;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_LOCKED;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_UNLOCKABLE;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_NEARBY:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_NOT_NEARBY;
    case EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED:
      // TODO(tbarzic): When hard lock is enabled change this to
      // IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_INSTRUCTIONS.
      return 0;
    default:
      return 0;
  }
}

}  // namespace


EasyUnlockScreenlockStateHandler::EasyUnlockScreenlockStateHandler(
    const std::string& user_email,
    PrefService* pref_service,
    ScreenlockBridge* screenlock_bridge)
    : state_(STATE_INACTIVE),
      user_email_(user_email),
      pref_service_(pref_service),
      screenlock_bridge_(screenlock_bridge) {
  DCHECK(screenlock_bridge_);
  screenlock_bridge_->AddObserver(this);
}

EasyUnlockScreenlockStateHandler::~EasyUnlockScreenlockStateHandler() {
  screenlock_bridge_->RemoveObserver(this);
  // Make sure the screenlock state set by this gets cleared.
  ChangeState(STATE_INACTIVE);
}

void EasyUnlockScreenlockStateHandler::ChangeState(State new_state) {
  if (state_ == new_state)
    return;

  state_ = new_state;

  // If lock screen is not active, just cache the current state.
  // The screenlock state will get refreshed in |ScreenDidLock|.
  if (!screenlock_bridge_->IsLocked())
    return;

  UpdateScreenlockAuthType();

  ScreenlockBridge::UserPodCustomIconOptions icon_options;

  std::string icon_url = GetIconURLForState(state_);
  if (icon_url.empty()) {
    screenlock_bridge_->lock_handler()->HideUserPodCustomIcon(user_email_);
    return;
  }
  icon_options.SetIconAsResourceURL(icon_url);

  UpdateTooltipOptions(&icon_options);

  if (UseOpaqueIcon(state_))
    icon_options.SetOpacity(kOpaqueIconOpacity);

  icon_options.SetSize(kIconSize, kIconSize);

  if (HasAnimation(state_))
    icon_options.SetAnimation(kSpinnerResourceWidth, kSpinnerIntervalMs);

  screenlock_bridge_->lock_handler()->ShowUserPodCustomIcon(user_email_,
                                                            icon_options);
}

void EasyUnlockScreenlockStateHandler::OnScreenDidLock() {
  State last_state = state_;
  // This should force updating screenlock state.
  state_ = STATE_INACTIVE;
  ChangeState(last_state);
}

void EasyUnlockScreenlockStateHandler::OnScreenDidUnlock() {
}

void EasyUnlockScreenlockStateHandler::UpdateTooltipOptions(
    ScreenlockBridge::UserPodCustomIconOptions* icon_options) {
  bool show_tutorial = ShouldShowTutorial();

  size_t resource_id = 0;
  base::string16 device_name;
  if (show_tutorial) {
    resource_id = IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_TUTORIAL;
  } else {
    resource_id = GetTooltipResourceId(state_);
    if (state_ == STATE_AUTHENTICATED || state_ == STATE_PHONE_UNLOCKABLE)
      device_name = GetDeviceName();
  }

  if (!resource_id)
    return;

  base::string16 tooltip;
  if (device_name.empty()) {
    tooltip = l10n_util::GetStringUTF16(resource_id);
  } else {
    tooltip = l10n_util::GetStringFUTF16(resource_id, device_name);
  }

  if (tooltip.empty())
    return;

  if (show_tutorial)
    MarkTutorialShown();

  icon_options->SetTooltip(tooltip, show_tutorial /* autoshow tooltip */);
}

bool EasyUnlockScreenlockStateHandler::ShouldShowTutorial() {
  if (state_ != STATE_AUTHENTICATED)
    return false;
  return pref_service_ &&
         pref_service_->GetBoolean(prefs::kEasyUnlockShowTutorial);
}

void EasyUnlockScreenlockStateHandler::MarkTutorialShown() {
  if (!pref_service_)
    return;
  pref_service_->SetBoolean(prefs::kEasyUnlockShowTutorial, false);
}

base::string16 EasyUnlockScreenlockStateHandler::GetDeviceName() {
#if defined(OS_CHROMEOS)
  return chromeos::GetChromeDeviceType();
#else
  // TODO(tbarzic): Figure out the name for non Chrome OS case.
  return base::ASCIIToUTF16("Chrome");
#endif
}

void EasyUnlockScreenlockStateHandler::UpdateScreenlockAuthType() {
  if (state_ == STATE_AUTHENTICATED) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        user_email_,
        ScreenlockBridge::LockHandler::USER_CLICK,
        l10n_util::GetStringUTF16(
            IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE));
  } else if (screenlock_bridge_->lock_handler()->GetAuthType(user_email_) !=
                 ScreenlockBridge::LockHandler::OFFLINE_PASSWORD) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        user_email_,
        ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
        base::string16());
  }
}
