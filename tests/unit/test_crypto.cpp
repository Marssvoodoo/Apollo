/**
 * @file tests/unit/test_crypto.cpp
 * @brief Regression tests for client-certificate trust validation.
 */
#include "../tests_common.h"

#include <src/crypto.h>

namespace {
  constexpr std::string_view TRUSTED_CERT = R"CERT(-----BEGIN CERTIFICATE-----
MIIDKzCCAhOgAwIBAgIUJRkWQ3NOfcXaDPgpSW134xc4vQ0wDQYJKoZIhvcNAQEL
BQAwJTEjMCEGA1UEAwwaQXBvbGxvIFRydXN0ZWQgVGVzdCBDbGllbnQwHhcNMjYw
ODE3MDQyNDMyWhcNMzYwODE0MDQyNDMyWjAlMSMwIQYDVQQDDBpBcG9sbG8gVHJ1
c3RlZCBUZXN0IENsaWVudDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB
AKNRd5RnqiDvTxjhMkKuFUaY0dRxmgRHN0eJTGX7GE+KjeFE64ezeQ8Jb3n2dPtg
eFV8UQeU3X5LTaR3GZjq2TRw0qtQexuRc6smDOhUn55UiZlxV7IpUskn9wGJm763
AN1DSjRla+cAuOdFwz10+j2+QGIWSTZUuoum6a7EQSj4ByUPPQqIpfB86Y6f2Nq2
pBzGE93rnmOdrf+7XeOlBrVCm3A6fiVPnrdUn3RIypK2puGRug6Oi4IGWSVD3eaE
mDSRtfkixkqdpmmbiA89pXSzXdOmlbhDCpm6FJUDweOHNnw2z1/0u2RXfrhfAKmq
ru5s3J97In2aCEetEPZX8YsCAwEAAaNTMFEwHQYDVR0OBBYEFESsDQDZ/o+AlWi+
48ftBiJWcIViMB8GA1UdIwQYMBaAFESsDQDZ/o+AlWi+48ftBiJWcIViMA8GA1Ud
EwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBAHN7yN+p/H+ag8Y8nl+5lIzA
NaxRqrT+3ETliCZTuKJ3EeFKQxHt7XRNWv1FETf2gP1wqSvh3j9C0mpQUlfV7XY1
9Rbycs5amMGywyTEk7BqEQfWB6dIXU8fN+TWQ3gIoge0wqgxdzxE1vKXcuat6BOr
QNQw+U+Pr7Fw2St1hFJD8+dBK3JfgV4rYFAQCnVxOW+8/yELH1ARSIUNOfY424k+
fmUeNsqyBxpXc7Kkkzo4GU+JWht+rOY8qTAMAR194u1bdvJifOUxc1wp+Xg4vjoB
0KnyeB8j7Wf8u0CJ6pNO24DY492KCpg3z3aZcPlCthXh8TGuAhTz4cFwzo3VNwI=
-----END CERTIFICATE-----)CERT";

  constexpr std::string_view UNTRUSTED_LEAF_CERT = R"CERT(-----BEGIN CERTIFICATE-----
MIIDDDCCAfSgAwIBAgIUOvhyhdA6+uw5PGjlFIyJVxN9+ncwDQYJKoZIhvcNAQEL
BQAwHjEcMBoGA1UEAwwTVW50cnVzdGVkIFRlc3QgUm9vdDAeFw0yNjA4MTcwNDI0
MzJaFw0zNjA4MTQwNDI0MzJaMB4xHDAaBgNVBAMME1VudHJ1c3RlZCBUZXN0IExl
YWYwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCunugWavdbnhSJ700D
xv37pX+acGSgKAWWuMUU3I+fRUhEUiHrqHVabekLYNJcXSq2wOP6d2YmlnaHkEpS
xatXPVf9x1Vy7N0mdGVw6C30p/qxv2AnYehGcNzPibHzIpneU95zy2YwXMJA9lR0
40B3TLohzvgxIMt8+TFjR4bBUSx6V1PuO40S5ELQoU671BBlcTtphUK1zma9iURM
/gik78fNrO/d8jEUIYlP7mxuySV7NqhwPfdaKEjHZD3A30jIcZc+/X+AysgOTUGD
JV1zqL1lwnbtIl1re29QKBht+OD+lZ+NgEAqtA0xWuy9tbtobmyxXAFnANJI0nhe
VgBtAgMBAAGjQjBAMB0GA1UdDgQWBBT+NPFQPcrcUT1+fWvDeEeFEh07wzAfBgNV
HSMEGDAWgBRdw0mKScDxe94oABCt+YLTC1verTANBgkqhkiG9w0BAQsFAAOCAQEA
WIlphVsP71CDiXd+1LCmxie+3Z/2h0bz7Pq2c4TTwlOMtgrjjera65oFzfqZVddP
s2uaRtEufOERSGhfMuStkWMKfzFTZ/plZ1QDwjck2G5Zq+wZ/PPvwj2yCHvzGKzj
LEVIgTybewRZMjQRDh1BAdYnefOTyTZhIfqQa86dTERJNkw378EBSVryD+ASJjju
lV/hA6ZGHH5DBwooysBv5QM1efngvJmHRzTdHmloTCL6u+BoV/765DNo2iJa5pHJ
HjMXsHhzZY9n0bM6AobjFVZ7G3WTfNiMxxRPRd9ml3Y8kCKMjxNNqUJ6MAT2btHG
5XbXoCbO8+2f7J9nEA8wrQ==
-----END CERTIFICATE-----)CERT";
}  // namespace

TEST(CertificateTrustTest, RejectsLeafSignedByUnknownIssuer) {
  crypto::cert_chain_t chain;
  auto trusted = std::make_shared<crypto::named_cert_t>();
  trusted->cert = TRUSTED_CERT;
  chain.add(trusted);

  auto untrusted_leaf = crypto::x509(UNTRUSTED_LEAF_CERT);
  crypto::p_named_cert_t matched;
  EXPECT_NE(chain.verify(untrusted_leaf.get(), matched), nullptr);
  EXPECT_EQ(matched, nullptr);
}

TEST(CertificateTrustTest, AcceptsExactTrustedCertificate) {
  crypto::cert_chain_t chain;
  auto trusted = std::make_shared<crypto::named_cert_t>();
  trusted->cert = TRUSTED_CERT;
  chain.add(trusted);

  auto trusted_x509 = crypto::x509(TRUSTED_CERT);
  crypto::p_named_cert_t matched;
  EXPECT_EQ(chain.verify(trusted_x509.get(), matched), nullptr);
  EXPECT_EQ(matched, trusted);
}
