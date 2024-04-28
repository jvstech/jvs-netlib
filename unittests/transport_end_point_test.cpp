#include <iostream>

#include <gtest/gtest.h>

#include <jvs-netlib/transport_end_point.h>

TEST(TransportEndPointTest, ParseTcpTransportEndPoint)
{
  auto ep = jvs::net::TransportEndPoint::parse("192.168.123.114:8088/tcp");
  ASSERT_TRUE(ep);
  EXPECT_EQ(ep->transport(), jvs::net::Socket::Transport::Tcp);
  EXPECT_EQ(jvs::net::to_string(ep->ip_end_point()), "192.168.123.114:8088");
  auto ep2 = jvs::net::TransportEndPoint::parse("192.168.123.114:8088");
  ASSERT_TRUE(ep2);
  EXPECT_EQ(jvs::net::to_string(*ep), jvs::net::to_string(*ep2));
}

TEST(TransportEndPointTest, ParseUdpTransportEndPoint)
{
  auto ep =
    jvs::net::TransportEndPoint::parse("[::FFFF:192.168.201.232]:1234/UDP");
  ASSERT_TRUE(ep);
  EXPECT_EQ(ep->transport(), jvs::net::Socket::Transport::Udp);
}

TEST(TransportEndPointTest, ParseRawTransportEndPoint)
{
  auto ep =
    jvs::net::TransportEndPoint::parse("224.255.255.0:8765/Raw");
  ASSERT_TRUE(ep);
  EXPECT_EQ(ep->transport(), jvs::net::Socket::Transport::Raw);
}

TEST(TransportEndPointTest, ParseTransportEndpoint_Bad)
{
  auto ep = jvs::net::TransportEndPoint::parse("[fc00::1234:89AB]:54321/sctp");
  EXPECT_FALSE(ep);
  auto ep2 = jvs::net::TransportEndPoint::parse("192.168.123.114:8088/");
  EXPECT_FALSE(ep2);
}
