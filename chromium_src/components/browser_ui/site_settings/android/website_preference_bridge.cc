/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_manager.h"

#define BACKGROUND_SYNC \
  AUTOPLAY:             \
  case ContentSettingsType::BACKGROUND_SYNC

#define CLIPBOARD_READ_WRITE \
  BRAVE_GOOGLE_SIGN_IN:      \
  case ContentSettingsType::CLIPBOARD_READ_WRITE

#include "src/components/browser_ui/site_settings/android/website_preference_bridge.cc"
#undef BACKGROUND_SYNC
#undef CLIPBOARD_READ_WRITE
