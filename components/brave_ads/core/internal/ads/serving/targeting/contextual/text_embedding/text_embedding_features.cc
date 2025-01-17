/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/ads/serving/targeting/contextual/text_embedding/text_embedding_features.h"

#include "base/metrics/field_trial_params.h"

namespace brave_ads::targeting::features {

namespace {

constexpr char kResourceVersionFieldTrialParamName[] = "resource_version";
constexpr int kResourceVersionDefaultValue = 1;

constexpr char kHistorySizeFieldTrialParamName[] = "history_size";
constexpr int kHistorySizeDefaultValue = 10;

}  // namespace

BASE_FEATURE(kTextEmbedding,
             "TextEmbedding",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTextEmbeddingEnabled() {
  return base::FeatureList::IsEnabled(kTextEmbedding);
}

int GetTextEmbeddingResourceVersion() {
  return GetFieldTrialParamByFeatureAsInt(kTextEmbedding,
                                          kResourceVersionFieldTrialParamName,
                                          kResourceVersionDefaultValue);
}

int GetTextEmbeddingsHistorySize() {
  return GetFieldTrialParamByFeatureAsInt(kTextEmbedding,
                                          kHistorySizeFieldTrialParamName,
                                          kHistorySizeDefaultValue);
}

}  // namespace brave_ads::targeting::features
