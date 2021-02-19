#include <array>
#include <cstdint>
#include <type_traits>

#include "gtest/gtest.h"

#include "network_integers.h"

TEST(NetworkIntegerTest, SameAddition)
{
  jvs::net::NetworkU16 a(0xab);
  jvs::net::NetworkU16 b(0x11);

  EXPECT_EQ(a, 0xab);
  EXPECT_EQ(b, 0x11);
  EXPECT_EQ(a + b, 0xbc);
}

TEST(NetworkIntegerTest, OtherAddition)
{
  jvs::net::NetworkI32 a(0xabcd1234);
  jvs::net::NetworkU16 b(0x89ab);
  auto c = a + b;

  EXPECT_EQ(c, 0xabcd9bdf);
  EXPECT_TRUE((std::is_same_v<decltype(c), jvs::net::NetworkI32>));
}
