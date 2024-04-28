#include <iostream>

#include <gtest/gtest.h>

#include <jvs-netlib/ip_end_point.h>

TEST(IpEndPointTest, ParseGoodIpv4EndPoint)
{
  auto ep = jvs::net::IpEndPoint::parse("192.168.123.114:8088");
  ASSERT_TRUE(ep);
  EXPECT_EQ(jvs::net::to_string(ep->address()), "192.168.123.114");
  EXPECT_EQ(ep->port(), 8088);
}

TEST(IpEndPointTest, ParseGoodIpv6EndPoint)
{
  auto ep = jvs::net::IpEndPoint::parse("[fc00::1234:89AB]:22");
  ASSERT_TRUE(ep);
  EXPECT_EQ(jvs::net::to_string(ep->address()), "fc00::1234:89ab");
  EXPECT_EQ(ep->port(), 22);
}

TEST(IpEndPointTest, ParseIpv4EndPoint_BadPort)
{
  auto ep = jvs::net::IpEndPoint::parse("224.255.255.0:98765");
  EXPECT_FALSE(ep);
}

TEST(IpEndPointTest, ParseIpv4EndPoint_BadAddress)
{
  auto ep = jvs::net::IpEndPoint::parse("123.456.789.101:80");
  EXPECT_FALSE(ep);
}


TEST(IpEndPointTest, ParseIpv6EndPoint_BadPort)
{
  auto ep = jvs::net::IpEndPoint::parse("[fc00::1234:89AB]:123456");
  EXPECT_FALSE(ep);
}

TEST(IpEndPointTest, ParseIpv6EndPoint_BadAddress)
{
  auto ep = jvs::net::IpEndPoint::parse("[fc00::1234:89ABCD]:80");
  EXPECT_FALSE(ep);
}

TEST(IpEndPointTest, Parse4to6MappedEndPoint)
{
  auto ep = jvs::net::IpEndPoint::parse("[::FFFF:192.168.201.232]:1234");
  ASSERT_TRUE(ep);
  EXPECT_EQ(jvs::net::to_string(ep->address()), "::ffff:192.168.201.232");
  EXPECT_EQ(ep->port(), 1234);
}
