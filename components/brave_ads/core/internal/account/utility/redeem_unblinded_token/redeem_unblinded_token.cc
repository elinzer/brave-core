/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/account/utility/redeem_unblinded_token/redeem_unblinded_token.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/values.h"
#include "brave/components/brave_ads/common/interfaces/ads.mojom.h"
#include "brave/components/brave_ads/core/internal/account/account_util.h"
#include "brave/components/brave_ads/core/internal/account/confirmations/confirmation_info.h"
#include "brave/components/brave_ads/core/internal/account/confirmations/confirmation_util.h"
#include "brave/components/brave_ads/core/internal/account/issuers/issuer_types.h"
#include "brave/components/brave_ads/core/internal/account/issuers/issuers_util.h"
#include "brave/components/brave_ads/core/internal/account/utility/redeem_unblinded_token/create_confirmation_url_request_builder.h"
#include "brave/components/brave_ads/core/internal/account/utility/redeem_unblinded_token/fetch_payment_token_url_request_builder.h"
#include "brave/components/brave_ads/core/internal/ads_client_helper.h"
#include "brave/components/brave_ads/core/internal/common/logging_util.h"
#include "brave/components/brave_ads/core/internal/common/net/http/http_status_code.h"
#include "brave/components/brave_ads/core/internal/common/url/url_request_string_util.h"
#include "brave/components/brave_ads/core/internal/common/url/url_response_string_util.h"
#include "brave/components/brave_ads/core/internal/privacy/challenge_bypass_ristretto/batch_dleq_proof.h"
#include "brave/components/brave_ads/core/internal/privacy/challenge_bypass_ristretto/blinded_token.h"
#include "brave/components/brave_ads/core/internal/privacy/challenge_bypass_ristretto/public_key.h"
#include "brave/components/brave_ads/core/internal/privacy/challenge_bypass_ristretto/signed_token.h"
#include "brave/components/brave_ads/core/internal/privacy/challenge_bypass_ristretto/token.h"
#include "brave/components/brave_ads/core/internal/privacy/challenge_bypass_ristretto/unblinded_token.h"
#include "brave/components/brave_ads/core/internal/privacy/tokens/unblinded_payment_tokens/unblinded_payment_token_info.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace brave_ads {

RedeemUnblindedToken::RedeemUnblindedToken() = default;

RedeemUnblindedToken::~RedeemUnblindedToken() {
  delegate_ = nullptr;
}

void RedeemUnblindedToken::Redeem(const ConfirmationInfo& confirmation) {
  DCHECK(IsValid(confirmation));

  BLOG(1, "Redeem unblinded token");

  if (ShouldRewardUser() && !HasIssuers()) {
    BLOG(1, "Failed to redeem unblinded token due to missing issuers");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  if (!confirmation.was_created) {
    return CreateConfirmation(confirmation);
  }

  FetchPaymentToken(confirmation);
}

///////////////////////////////////////////////////////////////////////////////

void RedeemUnblindedToken::CreateConfirmation(
    const ConfirmationInfo& confirmation) {
  BLOG(1, "CreateConfirmation");
  BLOG(2, "POST /v3/confirmation/{transactionId}/{credential}");

  CreateConfirmationUrlRequestBuilder url_request_builder(confirmation);
  mojom::UrlRequestInfoPtr url_request = url_request_builder.Build();
  BLOG(6, UrlRequestToString(url_request));
  BLOG(7, UrlRequestHeadersToString(url_request));

  AdsClientHelper::GetInstance()->UrlRequest(
      std::move(url_request),
      base::BindOnce(&RedeemUnblindedToken::OnCreateConfirmation,
                     base::Unretained(this), confirmation));
}

void RedeemUnblindedToken::OnCreateConfirmation(
    const ConfirmationInfo& confirmation,
    const mojom::UrlResponseInfo& url_response) {
  BLOG(1, "OnCreateConfirmation");

  BLOG(6, UrlResponseToString(url_response));
  BLOG(7, UrlResponseHeadersToString(url_response));

  if (!confirmation.opted_in) {
    if (url_response.status_code == net::kHttpImATeapot) {
      return OnDidSendConfirmation(confirmation);
    }

    const bool should_retry =
        url_response.status_code != net::HTTP_CONFLICT &&
        url_response.status_code != net::HTTP_BAD_REQUEST &&
        url_response.status_code != net::HTTP_CREATED;
    return OnFailedToSendConfirmation(confirmation, should_retry);
  }

  ConfirmationInfo new_confirmation = confirmation;
  new_confirmation.was_created = true;

  FetchPaymentToken(new_confirmation);
}

void RedeemUnblindedToken::FetchPaymentToken(
    const ConfirmationInfo& confirmation) {
  DCHECK(IsValid(confirmation));
  DCHECK(confirmation.opted_in);

  BLOG(1, "FetchPaymentToken");
  BLOG(2, "GET /v3/confirmation/{transactionId}/paymentToken");

  FetchPaymentTokenUrlRequestBuilder url_request_builder(confirmation);
  mojom::UrlRequestInfoPtr url_request = url_request_builder.Build();
  BLOG(6, UrlRequestToString(url_request));
  BLOG(7, UrlRequestHeadersToString(url_request));

  AdsClientHelper::GetInstance()->UrlRequest(
      std::move(url_request),
      base::BindOnce(&RedeemUnblindedToken::OnFetchPaymentToken,
                     base::Unretained(this), confirmation));
}

void RedeemUnblindedToken::OnFetchPaymentToken(
    const ConfirmationInfo& confirmation,
    const mojom::UrlResponseInfo& url_response) {
  BLOG(1, "OnFetchPaymentToken");

  BLOG(6, UrlResponseToString(url_response));
  BLOG(7, UrlResponseHeadersToString(url_response));

  if (url_response.status_code == net::HTTP_NOT_FOUND) {
    BLOG(1, "Confirmation not found");

    ConfirmationInfo new_confirmation = confirmation;
    new_confirmation.was_created = false;

    return OnFailedToRedeemUnblindedToken(new_confirmation,
                                          /*should_retry*/ true,
                                          /*should_backoff*/ false);
  }

  if (url_response.status_code == net::HTTP_BAD_REQUEST) {
    BLOG(1, "Credential is invalid");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ false,
                                          /*should_backoff*/ false);
  }

  if (url_response.status_code == net::HTTP_ACCEPTED) {
    BLOG(1, "Payment token is not ready");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ false);
  }

  if (url_response.status_code != net::HTTP_OK) {
    BLOG(1, "Failed to fetch payment token");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  // Parse JSON response
  const absl::optional<base::Value> root =
      base::JSONReader::Read(url_response.body);
  if (!root || !root->is_dict()) {
    BLOG(3, "Failed to parse response: " << url_response.body);
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  // Get id
  const std::string* const id = root->FindStringKey("id");
  if (!id) {
    BLOG(0, "Response is missing id");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  // Validate id
  if (*id != confirmation.transaction_id) {
    BLOG(0, "Response id " << *id
                           << " does not match confirmation transaction id "
                           << confirmation.transaction_id);
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ false,
                                          /*should_backoff*/ false);
  }

  // Get payment token
  const base::Value* const payment_token = root->FindDictKey("paymentToken");
  if (!payment_token) {
    BLOG(1, "Response is missing paymentToken");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  // Get public key
  const std::string* const public_key_base64 =
      payment_token->FindStringKey("publicKey");
  if (!public_key_base64) {
    BLOG(0, "Response is missing publicKey in paymentToken dictionary");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  const privacy::cbr::PublicKey public_key =
      privacy::cbr::PublicKey(*public_key_base64);
  if (!public_key.has_value()) {
    BLOG(0, "Invalid public key");
    NOTREACHED();
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  if (!PublicKeyExistsForIssuerType(IssuerType::kPayments,
                                    *public_key_base64)) {
    BLOG(0, "Response public key " << *public_key_base64 << " does not exist "
                                   << "in payments issuer public keys");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  // Get batch dleq proof
  const std::string* const batch_dleq_proof_base64 =
      payment_token->FindStringKey("batchProof");
  if (!batch_dleq_proof_base64) {
    BLOG(0, "Response is missing batchProof");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }
  privacy::cbr::BatchDLEQProof batch_dleq_proof =
      privacy::cbr::BatchDLEQProof(*batch_dleq_proof_base64);
  if (!batch_dleq_proof.has_value()) {
    BLOG(0, "Invalid batch DLEQ proof");
    NOTREACHED();
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  // Get signed tokens
  const base::Value* const signed_tokens_list =
      payment_token->FindListKey("signedTokens");
  if (!signed_tokens_list) {
    BLOG(0, "Response is missing signedTokens");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  std::vector<privacy::cbr::SignedToken> signed_tokens;
  for (const auto& item : signed_tokens_list->GetList()) {
    DCHECK(item.is_string());
    const std::string& signed_token_base64 = item.GetString();
    const privacy::cbr::SignedToken signed_token =
        privacy::cbr::SignedToken(signed_token_base64);
    if (!signed_token.has_value()) {
      BLOG(0, "Invalid signed token");
      NOTREACHED();
      continue;
    }

    signed_tokens.push_back(signed_token);
  }

  // Verify and unblind tokens
  if (!confirmation.opted_in) {
    BLOG(0, "Missing confirmation opted-in");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ false,
                                          /*should_backoff*/ false);
  }

  if (!confirmation.opted_in->token.has_value()) {
    BLOG(0, "Missing confirmation opted-in token");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ false,
                                          /*should_backoff*/ false);
  }
  const std::vector<privacy::cbr::Token> tokens = {
      confirmation.opted_in->token};

  if (!confirmation.opted_in->blinded_token.has_value()) {
    BLOG(0, "Missing confirmation opted-in blinded token");
    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ false,
                                          /*should_backoff*/ false);
  }
  const std::vector<privacy::cbr::BlindedToken> blinded_tokens = {
      confirmation.opted_in->blinded_token};

  const absl::optional<std::vector<privacy::cbr::UnblindedToken>>
      batch_dleq_proof_unblinded_tokens = batch_dleq_proof.VerifyAndUnblind(
          tokens, blinded_tokens, signed_tokens, public_key);
  if (!batch_dleq_proof_unblinded_tokens) {
    BLOG(1, "Failed to verify and unblind tokens");
    BLOG(1, "  Batch proof: " << *batch_dleq_proof_base64);
    BLOG(1, "  Public key: " << *public_key_base64);

    return OnFailedToRedeemUnblindedToken(confirmation, /*should_retry*/ true,
                                          /*should_backoff*/ true);
  }

  privacy::UnblindedPaymentTokenInfo unblinded_payment_token;
  unblinded_payment_token.transaction_id = confirmation.transaction_id;
  unblinded_payment_token.value = batch_dleq_proof_unblinded_tokens->front();
  unblinded_payment_token.public_key = public_key;
  unblinded_payment_token.confirmation_type = confirmation.type;
  unblinded_payment_token.ad_type = confirmation.ad_type;

  OnDidRedeemUnblindedToken(confirmation, unblinded_payment_token);
}

void RedeemUnblindedToken::OnDidSendConfirmation(
    const ConfirmationInfo& confirmation) {
  BLOG(1, "Successfully sent "
              << confirmation.type << " confirmation for "
              << confirmation.ad_type << " with transaction id "
              << confirmation.transaction_id << " and creative instance id "
              << confirmation.creative_instance_id);

  if (delegate_) {
    delegate_->OnDidSendConfirmation(confirmation);
  }
}

void RedeemUnblindedToken::OnFailedToSendConfirmation(
    const ConfirmationInfo& confirmation,
    const bool should_retry) {
  BLOG(1, "Failed to send " << confirmation.type << " confirmation for "
                            << confirmation.ad_type << " with transaction id "
                            << confirmation.transaction_id
                            << " and creative instance id "
                            << confirmation.creative_instance_id);

  if (delegate_) {
    delegate_->OnFailedToSendConfirmation(confirmation, should_retry);
  }
}

void RedeemUnblindedToken::OnDidRedeemUnblindedToken(
    const ConfirmationInfo& confirmation,
    const privacy::UnblindedPaymentTokenInfo& unblinded_payment_token) {
  BLOG(1, "Successfully redeemed unblinded token "
              << confirmation.type << " confirmation for "
              << confirmation.ad_type << " with transaction id "
              << confirmation.transaction_id << " and creative instance id "
              << confirmation.creative_instance_id);

  if (delegate_) {
    delegate_->OnDidRedeemUnblindedToken(confirmation, unblinded_payment_token);
  }
}

void RedeemUnblindedToken::OnFailedToRedeemUnblindedToken(
    const ConfirmationInfo& confirmation,
    const bool should_retry,
    const bool should_backoff) {
  BLOG(1, "Failed to redeem unblinded token "
              << confirmation.type << " confirmation for "
              << confirmation.ad_type << " with transaction id "
              << confirmation.transaction_id << " and creative instance id "
              << confirmation.creative_instance_id);

  if (delegate_) {
    delegate_->OnFailedToRedeemUnblindedToken(confirmation, should_retry,
                                              should_backoff);
  }
}

}  // namespace brave_ads
