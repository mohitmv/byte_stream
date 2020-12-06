// Copyright: 2019 Mohit Saini
// Author: Mohit Saini (mohitsaini1196@gmail.com)

#include "quick/byte_stream.hpp"

#include <map>
#include <utility>
#include <vector>
#include <set>
#include <string>
#include <tuple>

#include "quick/debug.hpp"

#include "gtest/gtest.h"

using std::make_pair;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;
using std::tuple;
using quick::ByteStream;
using quick::OByteStream;
using quick::IByteStream;
using std::cout;
using std::endl;
using std::make_tuple;
using std::unordered_map;


TEST(ByteStream, Basic) {
  int x1 = 11, y1 = 555333, z1 = 3, x2, y2, z2;
  double d1 = 44.55, d2;
  string s1 = "byte_stream", s2;
  enum A {AA, BB};
  A e1 = AA, e2;
  vector<vector<string>> v1 = {{"1.1", "1.2"}, {"2.1", "2.2"}}, v2;
  vector<set<string>> vs1 = {{"1.111", "1.222"}, {"2.111", "2.222"}}, vs2;
  vector<pair<int, tuple<int, float, bool>>> vp1 =
                               {{444, std::make_tuple(33, 44.8f, true)}}, vp2;
  unordered_map<int, map<int, string>> m2, m1 = {
    {11, {{100, "aa"}, {200, "bb"}}},
    {22, {{300, "cc"}, {400, "dd"}}}};


  OByteStream obs;
  obs << v1 << x1 << s1 << d1 << z1 << y1 << s1 << e1 << vp1 << m1;
  IByteStream ibs;
  ibs.str(obs.str());
  ibs >> v2 >> x2 >> s2 >> d2 >> z2 >> y2 >> s2 >> e2 >> vp2 >> m2;

  EXPECT_EQ(v1, v2);
  EXPECT_EQ(x1, x2);
  EXPECT_EQ(s1, s2);
  EXPECT_EQ(d1, d2);
  EXPECT_EQ(z1, z2);
  EXPECT_EQ(y1, y2);
  EXPECT_EQ(s1, s2);
  EXPECT_EQ(e1, e2);
  EXPECT_EQ(vp1, vp2);
  EXPECT_EQ(m1, m2);
  // cout << obs.str().size() << endl;
  // cout << ibs.str().size() << endl;
  // cout << ibs.read_ptr << endl;
  EXPECT_TRUE(ibs.end());
}


TEST(ByteStream, ClassMethod) {
  struct S {
    int x;
    string s;
    tuple<int, string, vector<int>> p;
    void Serialize(quick::OByteStream& bs) const {  // NOLINT
      bs << x << s << p;
    }
    void Deserialize(quick::IByteStream& bs) {  // NOLINT
      bs >> x >> s >> p;
    }
    bool operator==(const S& o) const {
      return (x == o.x && s == o.s && p == o.p);
    }
  };
  OByteStream obs;
  S s1, s2 = {100, "Mohit Saini", make_tuple(1000,
                                             "Mohit",
                                             vector<int> {10, 20, 30})};
  obs << s2 << 200 << "Abc";
  IByteStream ibs;
  ibs.str(obs.str());
  int num;
  string tmp_str;
  ibs >> s1 >> num >> tmp_str;
  EXPECT_EQ(s1, s2);
  EXPECT_EQ(num, 200);
  EXPECT_EQ(tmp_str, "Abc");
}

