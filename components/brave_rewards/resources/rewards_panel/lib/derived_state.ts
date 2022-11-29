/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { HostState } from './interfaces'
import { isExternalWalletProviderAllowed } from '../../shared/lib/external_wallet'

export function canConnectAccount (state: HostState) {
  const { declaredCountry } = state
  const { externalWalletRegions } = state.options
  return state.externalWalletProviders.some((provider) => {
    const regionInfo = externalWalletRegions.get(provider) || null
    return isExternalWalletProviderAllowed(declaredCountry, regionInfo)
  })
}
