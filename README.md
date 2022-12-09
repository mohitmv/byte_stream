# quick::ByteStream

Note: Moved out to independent repo from the original source at
https://github.com/mohitmv/quick/blob/master/include/quick/byte_stream.hpp

ByteStream is super intuitive, safe, reliable and easy to use utility for binary
serialisation and deserialization of complex and deeply nested C++ objects.

## A sample use case:

```C++
int x1 = 4;
vector<std::set<std::string>> v1 = {{"ABC", "D"}, {"Q"}};
quick::OByteStream obs;
obs << x1 << v1;
std::vector<std::byte> content = obs.Buffer();

quick::IByteStream ibs(content);
int x2;
vector<std::set<std::string>> v2;
ibs >> x2 >> v2;
assert(x1 == x2 && v1 == v2);
```

### Using ByteStream for custom C++ class

If a custom C++ class have `ToByteStream` member function, it can be used with
`operator<<` in addition to default supported types.
Similarly `operator>>` can be used if it have `FromByteStream` member.


```C++
struct S {
  double x1 = 5.7, x2 = 6.4;
  std::tuple<int, std::string, std::vector<int>> p;

  void ToByteStream(quick::OByteStream& bs) const {
    bs << x1 << p << x2;
  }
  void FromByteStream(quick::IByteStream& bs) {
    bs >> x1 >> p >> x2;
  }
};

quick::OByteStream obs;
std::vector<S> s1 {...};
float f1 = 0, f2;
obs << f1 << s1;

std::vector<std::byte> content = obs.Buffer();

std::vector<S> s2;
quick::IByteStream ibs(content);
ibs >> f2 >> s2;
assert(s1[0].p == s2[0].p && s1[0].x1 == s2[0].x1 && s1[0].x2 == s2[0].x2);
```

This enables deeply nested class objects to be used easily in byte stream using
`>>` and `<<`.

## More complex use case

```C++
struct A {
  int x1, x2;
  std::list<int> y1;

  void ToByteStream(quick::OByteStream& bs) const { bs << x1 << x2 << y1; }
  void FromByteStream(quick::IByteStream& bs) { bs >> x1 >> x2 >> y1; }
};

struct B {
  std::vector<A> a_list;
  std::tuple<A, float, std::string> t;

  void ToByteStream(quick::OByteStream& bs) const { bs << a_list << t; }
  void FromByteStream(quick::IByteStream& bs) { bs >> a_list >> t; }
}

quick::OByteStream obs;
std::map<std::string, B> b_map;
obs << b_map << 33;
```
