/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/account/user_data/odyssey_user_data.h"

#include "base/test/values_test_util.h"
#include "brave/components/brave_ads/common/interfaces/ads.mojom.h"
#include "brave/components/brave_ads/core/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

// npm run test -- brave_unit_tests --filter=BatAds*

namespace brave_ads::user_data {

TEST(BatAdsOdysseyUserDataTest, GetOdysseyForGuest) {
  // Arrange
  SysInfo().is_uncertain_future = true;

  // Act
  const base::Value::Dict user_data = GetOdyssey();

  // Assert
  const base::Value expected_user_data =
      base::test::ParseJson(R"({"odyssey":"guest"})");
  ASSERT_TRUE(expected_user_data.is_dict());

  EXPECT_EQ(expected_user_data, user_data);
}

TEST(BatAdsOdysseyUserDataTest, GetOdysseyForHost) {
  // Arrange
  SysInfo().is_uncertain_future = false;

  // Act
  const base::Value::Dict user_data = GetOdyssey();

  // Assert
  const base::Value expected_user_data =
      base::test::ParseJson(R"({"odyssey":"host"})");
  ASSERT_TRUE(expected_user_data.is_dict());

  EXPECT_EQ(expected_user_data, user_data);
}

}  // namespace brave_ads::user_data
