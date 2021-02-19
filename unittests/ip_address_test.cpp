#include <cstdint>

#include "gtest/gtest.h"

#include "ip_address.h"

TEST(IpAddressTest, ParseIPv4)
{
  auto ipAddr = jvs::net::IpAddress::parse("192.168.0.1");
  ASSERT_TRUE(ipAddr);
  EXPECT_EQ(ipAddr->address_bytes()[0], 192);
  EXPECT_EQ(ipAddr->address_bytes()[1], 168);
  EXPECT_EQ(ipAddr->address_bytes()[2], 0);
  EXPECT_EQ(ipAddr->address_bytes()[3], 1);
}

TEST(IpAddressTest, ParseIPv6)
{
  auto ipAddr = jvs::net::IpAddress::parse("fc00::1234:89AB");
  ASSERT_TRUE(ipAddr);
  EXPECT_EQ(ipAddr->address_bytes()[0], 0xfc);
  EXPECT_EQ(ipAddr->address_bytes()[1], 0);
  EXPECT_EQ(ipAddr->address_bytes()[2], 0);
  EXPECT_EQ(ipAddr->address_bytes()[3], 0);
  EXPECT_EQ(ipAddr->address_bytes()[4], 0);
  EXPECT_EQ(ipAddr->address_bytes()[5], 0);
  EXPECT_EQ(ipAddr->address_bytes()[6], 0);
  EXPECT_EQ(ipAddr->address_bytes()[7], 0);
  EXPECT_EQ(ipAddr->address_bytes()[8], 0);
  EXPECT_EQ(ipAddr->address_bytes()[9], 0);
  EXPECT_EQ(ipAddr->address_bytes()[10], 0);
  EXPECT_EQ(ipAddr->address_bytes()[11], 0);
  EXPECT_EQ(ipAddr->address_bytes()[12], 0x12);
  EXPECT_EQ(ipAddr->address_bytes()[13], 0x34);
  EXPECT_EQ(ipAddr->address_bytes()[14], 0x89);
  EXPECT_EQ(ipAddr->address_bytes()[15], 0xab);
}

TEST(IpAddressTest, ConstructIPv4)
{
  jvs::net::IpAddress ipAddr(
    static_cast<std::uint32_t>((192 << 24) | (168 << 16) | 1));
  EXPECT_EQ(ipAddr.address_bytes()[0], 192);
  EXPECT_EQ(ipAddr.address_bytes()[1], 168);
  EXPECT_EQ(ipAddr.address_bytes()[2], 0);
  EXPECT_EQ(ipAddr.address_bytes()[3], 1);
  EXPECT_EQ(jvs::net::to_string(ipAddr), "192.168.0.1");
}

TEST(IpAddressTest, ConstructIPv6)
{
  jvs::net::IpAddress ipAddr(
    0xfc00000000000000, static_cast<std::uint64_t>(0x123489AB));
  EXPECT_EQ(ipAddr.address_bytes()[0], 0xfc);
  EXPECT_EQ(ipAddr.address_bytes()[1], 0);
  EXPECT_EQ(ipAddr.address_bytes()[2], 0);
  EXPECT_EQ(ipAddr.address_bytes()[3], 0);
  EXPECT_EQ(ipAddr.address_bytes()[4], 0);
  EXPECT_EQ(ipAddr.address_bytes()[5], 0);
  EXPECT_EQ(ipAddr.address_bytes()[6], 0);
  EXPECT_EQ(ipAddr.address_bytes()[7], 0);
  EXPECT_EQ(ipAddr.address_bytes()[8], 0);
  EXPECT_EQ(ipAddr.address_bytes()[9], 0);
  EXPECT_EQ(ipAddr.address_bytes()[10], 0);
  EXPECT_EQ(ipAddr.address_bytes()[11], 0);
  EXPECT_EQ(ipAddr.address_bytes()[12], 0x12);
  EXPECT_EQ(ipAddr.address_bytes()[13], 0x34);
  EXPECT_EQ(ipAddr.address_bytes()[14], 0x89);
  EXPECT_EQ(ipAddr.address_bytes()[15], 0xab);
  EXPECT_EQ(jvs::net::to_string(ipAddr), "fc00::1234:89ab");
}

TEST(IpAddressTest, Map4To6)
{
  auto addr = jvs::net::IpAddress::parse("192.168.0.1");
  EXPECT_EQ(
    jvs::net::to_string(jvs::net::map_to_ipv6(*addr)), "::ffff:192.168.0.1");
}

TEST(IpAddressTest, Map6To4)
{
  auto addr = jvs::net::IpAddress::parse("fc00::1234:89AB");
  EXPECT_EQ(
    jvs::net::to_string(jvs::net::map_to_ipv4(*addr)), "18.52.137.171");
}

TEST(IpAddressTest, CidrMasking)
{
  auto addr = jvs::net::IpAddress::parse("192.168.2.117");
  EXPECT_EQ(jvs::net::to_string(*addr / 24), "192.168.2.0");
  EXPECT_EQ(jvs::net::to_string(*addr / 16), "192.168.0.0");
  EXPECT_EQ(jvs::net::to_string(*addr / 8), "192.0.0.0");
}
