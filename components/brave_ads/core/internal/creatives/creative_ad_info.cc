/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/creatives/creative_ad_info.h"

#include <limits>

#include "base/numerics/ranges.h"

namespace brave_ads {

CreativeAdInfo::CreativeAdInfo() = default;

CreativeAdInfo::CreativeAdInfo(const CreativeAdInfo& other) = default;

CreativeAdInfo& CreativeAdInfo::operator=(const CreativeAdInfo& other) =
    default;

CreativeAdInfo::CreativeAdInfo(CreativeAdInfo&& other) noexcept = default;

CreativeAdInfo& CreativeAdInfo::operator=(CreativeAdInfo&& other) noexcept =
    default;

CreativeAdInfo::~CreativeAdInfo() = default;

// TODO(https://github.com/brave/brave-browser/issues/23087):
// |base::IsApproximatelyEqual| can be removed for timestamp comparisons once
// timestamps are persisted using |ToDeltaSinceWindowsEpoch| and
// |FromDeltaSinceWindowsEpoch| in microseconds.
bool CreativeAdInfo::operator==(const CreativeAdInfo& other) const {
  return creative_instance_id == other.creative_instance_id &&
         creative_set_id == other.creative_set_id &&
         campaign_id == other.campaign_id &&
         base::IsApproximatelyEqual(start_at.ToDoubleT(),
                                    other.start_at.ToDoubleT(),
                                    std::numeric_limits<double>::epsilon()) &&
         base::IsApproximatelyEqual(end_at.ToDoubleT(),
                                    other.end_at.ToDoubleT(),
                                    std::numeric_limits<double>::epsilon()) &&
         daily_cap == other.daily_cap && advertiser_id == other.advertiser_id &&
         priority == other.priority &&
         base::IsApproximatelyEqual(ptr, other.ptr,
                                    std::numeric_limits<double>::epsilon()) &&
         has_conversion == other.has_conversion && per_day == other.per_day &&
         per_week == other.per_week && per_month == other.per_month &&
         total_max == other.total_max &&
         base::IsApproximatelyEqual(value, other.value,
                                    std::numeric_limits<double>::epsilon()) &&
         split_test_group == other.split_test_group &&
         segment == other.segment && embedding == other.embedding &&
         geo_targets == other.geo_targets && target_url == other.target_url &&
         dayparts == other.dayparts;
}

bool CreativeAdInfo::operator!=(const CreativeAdInfo& other) const {
  return !(*this == other);
}

}  // namespace brave_ads
