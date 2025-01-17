/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/account/user_data/conversion_user_data_builder.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "brave/components/brave_ads/core/internal/account/user_data/conversion_user_data_util.h"
#include "brave/components/brave_ads/core/internal/conversions/conversion_queue_database_table.h"
#include "brave/components/brave_ads/core/internal/conversions/conversion_queue_item_info.h"
#include "brave/components/brave_ads/core/internal/conversions/verifiable_conversion_envelope_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace brave_ads::user_data::builder {

namespace {

constexpr char kAlgorithmKey[] = "alg";
constexpr char kAlgorithm[] = "crypto_box_curve25519xsalsa20poly1305";
constexpr char kCipherTextKey[] = "ciphertext";
constexpr char kEphemeralPublicKeyKey[] = "epk";
constexpr char kNonceKey[] = "nonce";
constexpr char kConversionEnvelopeKey[] = "conversionEnvelope";

void ReportConversionDoesNotExist(BuildConversionCallback callback) {
  std::move(callback).Run(base::Value::Dict());
}

}  // namespace

void BuildConversion(const std::string& creative_instance_id,
                     BuildConversionCallback callback) {
  DCHECK(!creative_instance_id.empty());

  const database::table::ConversionQueue database_table;
  database_table.GetForCreativeInstanceId(
      creative_instance_id,
      base::BindOnce(
          [](BuildConversionCallback callback, const bool success,
             const std::string& /*creative_instance_id*/,
             const ConversionQueueItemList& conversion_queue_items) {
            if (!success) {
              return ReportConversionDoesNotExist(std::move(callback));
            }

            if (conversion_queue_items.empty()) {
              return ReportConversionDoesNotExist(std::move(callback));
            }

            const ConversionQueueItemInfo& conversion_queue_item =
                conversion_queue_items.front();
            const absl::optional<security::VerifiableConversionEnvelopeInfo>
                verifiable_conversion_envelope =
                    GetEnvelope(conversion_queue_item);
            if (!verifiable_conversion_envelope) {
              return ReportConversionDoesNotExist(std::move(callback));
            }

            base::Value::Dict conversion_envelope;
            conversion_envelope.Set(kAlgorithmKey, kAlgorithm);
            conversion_envelope.Set(kCipherTextKey,
                                    verifiable_conversion_envelope->ciphertext);
            conversion_envelope.Set(
                kEphemeralPublicKeyKey,
                verifiable_conversion_envelope->ephemeral_public_key);
            conversion_envelope.Set(kNonceKey,
                                    verifiable_conversion_envelope->nonce);

            base::Value::Dict user_data;
            user_data.Set(kConversionEnvelopeKey,
                          std::move(conversion_envelope));

            std::move(callback).Run(std::move(user_data));
          },
          std::move(callback)));
}

}  // namespace brave_ads::user_data::builder
