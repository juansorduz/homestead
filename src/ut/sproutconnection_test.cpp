/**
 * @file sproutconnection_test.cpp UT for SproutConnection
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "sproutconnection.h"
#include "mock_httpclient.h"

using ::testing::Mock;
using ::testing::Return;
using ::testing::_;
using ::testing::AllOf;

const SAS::TrailId FAKE_TRAIL_ID = 0x12345678;

class SproutConnectionTest : public testing::Test
{
  static const std::string IMPU;
  static const std::string IMPI;
  static const std::vector<std::string> IMPIS;
  static const std::vector<std::string> IMPUS;
  static const std::string IMS_SUBSCRIPTION;
  static const std::string dereg_body;
  static const std::string dereg_body_no_impis;
  static const std::string change_ids_body;

  SproutConnectionTest()
  {
    _mock_http_client = new MockHttpClient();
    _http_conn = new HttpConnection("server", _mock_http_client);
    _sprout_conn = new SproutConnection(_http_conn);

    // If we don't override the default behaviour, return a nonsensical HTTP Code
    ON_CALL(*_mock_http_client, send_request(_)).WillByDefault(Return(HttpResponse(-1, "", {})));
  }

  virtual ~SproutConnectionTest()
  {
    delete _mock_http_client; _mock_http_client = nullptr;
    delete _sprout_conn; _sprout_conn = nullptr;
    delete _http_conn; _http_conn = nullptr;
  }

  MockHttpClient* _mock_http_client;
  HttpConnection* _http_conn;
  SproutConnection* _sprout_conn;
};

const std::string SproutConnectionTest::IMPU = "sip:impu@example.com";
const std::string SproutConnectionTest::IMPI = "_impi@example.com";
const std::vector<std::string> SproutConnectionTest::IMPIS = { "_impi1@example.com", "_impi2@example.com" };
const std::vector<std::string> SproutConnectionTest::IMPUS = { "sip:impu1@example.com", "sip:impu2@example.com" };
const std::string SproutConnectionTest::IMS_SUBSCRIPTION =
  "<?xml version=\"1.0\"?>"
  "<IMSSubscription>"
    "<PrivateID>" + IMPI + "</PrivateID>"
    "<ServiceProfile>"
      "<PublicIdentity>"
        "<Identity>" + IMPU + "</Identity>"
      "</PublicIdentity>"
    "</ServiceProfile>"
  "</IMSSubscription>";

// The body of the HTTP request we expect to create for an RTR specifying IMPIS
// and IMPUS
const std::string SproutConnectionTest::dereg_body =
"{\"registrations\":[{\"primary-impu\":\"sip:impu1@example.com\",\"impi\":\"_impi1@example.com\"},"
                    "{\"primary-impu\":\"sip:impu1@example.com\",\"impi\":\"_impi2@example.com\"},"
                    "{\"primary-impu\":\"sip:impu2@example.com\",\"impi\":\"_impi1@example.com\"},"
                    "{\"primary-impu\":\"sip:impu2@example.com\",\"impi\":\"_impi2@example.com\"}]}";

// The body of the HTTP request we expect to create for an RTR specifying just
// IMPUS
const std::string SproutConnectionTest::dereg_body_no_impis =
  "{\"registrations\":[{\"primary-impu\":\"sip:impu1@example.com\"},"
                      "{\"primary-impu\":\"sip:impu2@example.com\"}]}";

// The body of the HTTP request we expect to create for a PPR with the
// IMS_SUBSCRIPTION specified above
const std::string SproutConnectionTest::change_ids_body =
  "{\"user-data-xml\":\"<?xml version=\\\"1.0\\\"?>"
    "<IMSSubscription>"
      "<PrivateID>_impi@example.com</PrivateID>"
      "<ServiceProfile>"
        "<PublicIdentity>"
          "<Identity>sip:impu@example.com</Identity>"
        "</PublicIdentity>"
      "</ServiceProfile>"
    "</IMSSubscription>\"}";


TEST_F(SproutConnectionTest, DeregisterBindingsWithNotifications)
{
  // Create an OK response to be returned by the HttpClient
  HttpResponse resp(HTTP_OK, "", {});

  // Expect that the correct request is sent, and set it to return the response
  EXPECT_CALL(*_mock_http_client, send_request(AllOf(IsDelete(),
                                                     HasBody(dereg_body),
                                                     HasTrail(FAKE_TRAIL_ID),
                                                     HasPath("/registrations?send-notifications=true"))))
    .WillOnce(Return(resp));

  // Actually deregister_bindings
  HTTPCode result = _sprout_conn->deregister_bindings(true, IMPUS, IMPIS, FAKE_TRAIL_ID);

  // Expect that we get the correct return code
  EXPECT_EQ(HTTP_OK, result);
}

TEST_F(SproutConnectionTest, DeregisterBindingsWithoutNotifications)
{
  // Create an OK response to be returned by the HttpClient
  HttpResponse resp(HTTP_OK, "", {});

  // Expect that the correct request is sent, and set it to return the response
  EXPECT_CALL(*_mock_http_client, send_request(AllOf(IsDelete(),
                                                     HasPath("/registrations?send-notifications=false"))))
    .WillOnce(Return(resp));

  // Actually deregister_bindings
  HTTPCode result = _sprout_conn->deregister_bindings(false, IMPUS, IMPIS, FAKE_TRAIL_ID);

  // Expect that we get the correct return code
  EXPECT_EQ(HTTP_OK, result);
}

TEST_F(SproutConnectionTest, DeregisterBindingsEmptyImpis)
{
  // Create an OK response to be returned by the HttpClient
  HttpResponse resp(HTTP_OK, "", {});

  // Expect that the correct request is sent, and set it to return the response
  EXPECT_CALL(*_mock_http_client, send_request(AllOf(IsDelete(),
                                                     HasBody(dereg_body_no_impis))))
    .WillOnce(Return(resp));

  // Actually deregister_bindings
  HTTPCode result = _sprout_conn->deregister_bindings(false, IMPUS, {}, FAKE_TRAIL_ID);

  // Expect that we get the correct return code
  EXPECT_EQ(HTTP_OK, result);
}

TEST_F(SproutConnectionTest, DeregisterBindingsError)
{
  // Create an error response to be returned by the HttpClient
  HttpResponse resp(HTTP_SERVER_UNAVAILABLE, "", {});

  // Expect that the correct request is sent, and set it to return the response
  EXPECT_CALL(*_mock_http_client, send_request(IsDelete()))
    .WillOnce(Return(resp));

  // Actually deregister_bindings
  HTTPCode result = _sprout_conn->deregister_bindings(false, IMPUS, {}, FAKE_TRAIL_ID);

  // Expect that we get the correct return code
  EXPECT_EQ(HTTP_SERVER_UNAVAILABLE, result);
}

TEST_F(SproutConnectionTest, ChangeAssociatedIdentities)
{
  // Create an OK response to be returned by the HttpClient
  HttpResponse resp(HTTP_OK, "", {});

  // Expect that the correct request is sent, and set it to return the response
  EXPECT_CALL(*_mock_http_client, send_request(AllOf(IsPut(),
                                                     HasBody(change_ids_body),
                                                     HasTrail(FAKE_TRAIL_ID),
                                                     HasPath("/registrations/" + IMPU))))
    .WillOnce(Return(resp));
  // Change the identities
  HTTPCode result = _sprout_conn->change_associated_identities(IMPU, IMS_SUBSCRIPTION, FAKE_TRAIL_ID);

  // Expect that we get the correct return code
  EXPECT_EQ(HTTP_OK, result);
}
