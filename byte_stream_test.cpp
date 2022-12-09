// Author: Mohit Saini (mohitsaini1196@gmail.com)

#include "byte_stream.hpp"

#include <iostream>
#include <map>
#include <utility>
#include <vector>
#include <set>
#include <string>
#include <unordered_map>
#include <tuple>
#include <cassert>

using std::string;
using std::vector;
using std::make_tuple;

void TestBasic() {
  int x1 = 11, y1 = 555333, z1 = 3, x2, y2, z2;
  double d1 = 44.55, d2;
  string s1 = "Abc", s2;
  enum A {AA, BB};
  A e1 = AA, e2;

  vector<vector<string>> v1 = {{"1.1", "1.2"}, {"2.1", "2.2"}}, v2;
  vector<std::set<string>> vs1 = {{"1.111", "1.222"}, {"2.111", "2.222"}}, vs2;
  vector<std::pair<int, std::tuple<int, float, bool>>> vp1 =
                               {{444, std::make_tuple(33, 44.8f, true)}}, vp2;
  vector<int> vi1 = {10, 3000, 400}, vi2;

  std::unordered_map<int, std::map<int, string>> m2, m1 = {
    {11, {{100, "aa"}, {200, "bb"}}},
    {22, {{300, "cc"}, {400, "dd"}}}};


  quick::OByteStream obs;
  obs << v1;
  obs << x1 << s1;
  obs << d1 << z1 << y1 << s1 << e1 << vp1;
  obs << m1 << vi1;

  quick::IByteStream ibs(obs.buffer());
  ibs >> v2;
  ibs >> x2 >> s2;
  ibs >> d2 >> z2 >> y2 >> s2 >> e2 >> vp2 >> m2 >> vi2;

  assert(v1 == v2);
  assert(x1 == x2);
  assert(s1 == s2);
  assert(d1 == d2);
  assert(z1 == z2);
  assert(y1 == y2);
  assert(s1 == s2);
  assert(e1 == e2);
  assert(vp1 == vp2);
  assert(m1 == m2);
  assert(vi1 == vi2);
  assert(ibs.End());
  assert(ibs.Ok());
  std::cout << "TestBasic Passed" << std::endl;
}

void TestClassMethod() {
  struct S {
    int x;
    string s;
    std::tuple<int, string, vector<int>> p;
    void ToByteStream(quick::OByteStream& bs) const {
      bs << x << s << p;
    }
    void FromByteStream(quick::IByteStream& bs) {
      bs >> x >> s >> p;
    }
    bool operator==(const S& o) const {
      return (x == o.x && s == o.s && p == o.p);
    }
  };
  quick::OByteStream obs;
  S s1, s2 = {100, "Str1", make_tuple(1000, "Str2", vector<int> {10, 20, 30})};
  obs << s2 << 200 << "Abc";
  quick::IByteStream ibs(obs.buffer());
  int num;
  string tmp_str;
  ibs >> s1 >> num >> tmp_str;
  assert(s1 == s2);
  assert(num == 200);
  assert(tmp_str == "Abc");
  assert(ibs.End());
  assert(ibs.Ok());
  std::cout << "TestClassMethod Passed" << std::endl;
}

int main() {
  TestBasic();
  TestClassMethod();
}
