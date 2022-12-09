// Original: https://github.com/mohitmv/quick @ include/quick/byte_stream.hpp
// Author: Mohit Saini (mohitsaini1196@gmail.com)

#ifndef QUICK_BYTE_STREAM_HPP_
#define QUICK_BYTE_STREAM_HPP_

// ByteStream is super intuitive, safe, reliable and easy to use utility for
// binary serialisation and deserialization of complex and deeply nested C++
// objects.
// Learn more at README.md and byte_stream_test.cpp

// Sample Use Case:
// 
// int x1 = 4;
// vector<std::set<std::string>> v1 = {{"ABC", "D"}, {"Q"}};
// quick::OByteStream obs;
// obs << x1 << v1;
// std::vector<std::byte> content = obs.Buffer();

// quick::IByteStream ibs(content);
// int x2;
// vector<std::set<std::string>> v2;
// ibs >> x2 >> v2;
// assert(x1 == x2 && v1 == v2);

#include <type_traits>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstring>
#include <tuple>
#include <utility>
#include <vector>

namespace quick {

static_assert(std::endian::native == std::endian::little, "");

namespace bytestream_impl {

enum class Status {OK, INVALID_READ};

template<typename T>
std::enable_if_t<(std::is_fundamental<T>::value ||
                  std::is_enum<T>::value)>
WritePrimitiveType(void* dst, T value) {
  std::memcpy(dst, &value, sizeof(T));
}

void WriteBuffer(void* dst, const void* buffer_ptr, size_t buffer_size) {
  std::memcpy(dst, buffer_ptr, buffer_size);
}

template<typename T, typename = void> struct has_iterator : std::false_type { };
template<typename T>
struct has_iterator<T, std::void_t<typename T::iterator>> : std::true_type { };

template<typename T, typename = void> struct has_to_bytestream : std::false_type { };
template<typename T>
struct has_to_bytestream<T, std::void_t<decltype(&T::ToByteStream)>> : std::true_type { };

template<typename T, typename = void> struct has_from_bytestream : std::false_type { };
template<typename T>
struct has_from_bytestream<T, std::void_t<decltype(&T::FromByteStream)>> : std::true_type { };

template<typename T, typename = void> struct can_insert : std::false_type { };
template<typename T>
struct can_insert<T, std::void_t<decltype(
                 std::declval<T>().insert(
                   std::declval<typename T::value_type&&>()))>> : std::true_type { };

template<typename T, typename = void> struct can_push_back : std::false_type { };
template<typename T>
struct can_push_back<T, std::void_t<decltype(
                 std::declval<T>().push_back(
                   std::declval<typename T::value_type&&>()))>> : std::true_type { };

template<typename T>
struct ConstCastValueType { using type = T; };
template<typename T1, typename T2>
struct ConstCastValueType<std::pair<const T1, T2>> { using type = std::pair<T1, T2>; };

}  // namespace bytestream_impl


class OByteStream {
 public:
  const std::vector<std::byte>& GetBytes() const {  return output_bytes; }
  std::vector<std::byte>& GetMutableBytes() { return output_bytes; }

  template<typename T>
  OByteStream& operator<<(const T& input) { Write(input); return *this; }

  const std::vector<std::byte>& Buffer() const { return output_bytes; }
  std::vector<std::byte>& Buffer() { return output_bytes; }

 private:
  void Write(const std::string_view& input) {
    size_t size0 = output_bytes.size();
    output_bytes.resize(size0 + sizeof(size_t) + input.size());
    bytestream_impl::WritePrimitiveType(&output_bytes[size0], input.size());
    bytestream_impl::WriteBuffer(&output_bytes[size0 + sizeof(size_t)], input.data(), input.size());
  }

  template<typename T>
  std::enable_if_t<(std::is_fundamental<T>::value ||
                    std::is_enum<T>::value)>
  Write(const std::vector<T>& input) {
    size_t size0 = output_bytes.size();
    size_t real_input_size = sizeof(T) * input.size();
    output_bytes.resize(size0 + sizeof(size_t) + real_input_size);
    bytestream_impl::WritePrimitiveType(&output_bytes[size0], input.size());
    bytestream_impl::WriteBuffer(&output_bytes[size0 + sizeof(size_t)], input.data(), real_input_size);
  }


  template<typename T>
  std::enable_if_t<bytestream_impl::has_iterator<T>::value>
  Write(const T& container) {
    Write(container.size());
    for (auto& item : container) {
      Write(item);
    }
  }

  template<typename T>
  std::enable_if_t<(std::is_fundamental<T>::value || std::is_enum<T>::value)>
  Write(T input) {
    size_t size0 = output_bytes.size();
    output_bytes.resize(size0 + sizeof(T));
    bytestream_impl::WritePrimitiveType(&output_bytes[size0], input);
  }

  template<typename... Ts>
  void WriteTuple(
      const std::tuple<Ts...>&,
      std::index_sequence<sizeof...(Ts)>) { }

  template<std::size_t I, typename... Ts>
  std::enable_if_t<(I < sizeof...(Ts))>
  WriteTuple(const std::tuple<Ts...>& input,
                 std::index_sequence<I>) {
    Write(std::get<I>(input));
    WriteTuple(input, std::index_sequence<I+1>());
  }

  template<typename... Ts>
  void Write(const std::tuple<Ts...>& input) {
    WriteTuple(input, std::index_sequence<0>());
  }

  template<typename T1, typename T2>
  void Write(const std::pair<T1, T2>& input) {
    Write(input.first);
    Write(input.second);
  }

  template<typename T>
  std::enable_if_t<bytestream_impl::has_to_bytestream<T>::value>
  Write(const T& input) {
    input.ToByteStream(*this);
  }

  std::vector<std::byte> output_bytes;
};


class IByteStream {
 public:
  using Status = bytestream_impl::Status;

  IByteStream(const std::byte* buffer, size_t len): buffer(buffer), buffer_len(len) { }

  IByteStream(const std::vector<std::byte>& buffer_vec)
    : buffer(buffer_vec.data()),
      buffer_len(buffer_vec.size()) { }

  IByteStream(const std::string_view& str_view)
      : buffer(reinterpret_cast<const std::byte*>(str_view.data())),
        buffer_len(str_view.size()) { }

  template<typename T>
  IByteStream& operator>>(T& output) {
    if (Read(output)) return *this;
    status = Status::INVALID_READ;
    return *this;
  }

  Status GetStatus() const { return status; }
  bool Ok() const { return status == Status::OK; }
  bool End() const { return read_ptr == buffer_len; }

 private:

  bool Read(std::string& output) {
    size_t string_size;
    if (not Read(string_size)) return false;
    if (read_ptr + string_size > buffer_len) return false;
    output.resize(string_size);
    std::memcpy(output.data(), buffer + read_ptr, string_size);
    read_ptr += string_size;
    return true;
  }

  bool Read(std::string_view& output) {
    const std::byte* buffer;
    size_t string_size;
    if (not Read(string_size)) return false;
    if (read_ptr + string_size > buffer_len) return false;
    output = std::string_view(reinterpret_cast<const char*>(buffer + read_ptr), string_size);
    read_ptr += string_size;
    return true;
  }

  template<typename T>
  bool Read(std::vector<T>& output) {
    size_t vec_size;
    if (not Read(vec_size)) return false;
    output.resize(vec_size);
    if constexpr (std::is_fundamental<T>::value || std::is_enum<T>::value) {
      // std::memcpy at once is faster than for-loop on individual item.
      size_t to_copy = vec_size * sizeof(T);
      if (read_ptr + to_copy > buffer_len) return false;
      std::memcpy(output.data(), buffer + read_ptr, to_copy);
      read_ptr += to_copy;
    } else {
      for (size_t i = 0; i < vec_size; ++i) {
        if (not Read(output[i])) return false;
      }
    }
    return true;
  }

  template<typename T>
  std::enable_if_t<(std::is_fundamental<T>::value ||
                    std::is_enum<T>::value), bool>
  Read(T& output) {
    if (read_ptr + sizeof(T) > buffer_len) return false;
    std::memcpy(&output, buffer + read_ptr, sizeof(T));
    read_ptr += sizeof(T);
    return true;
  }

  template<typename... Ts>
  bool ReadTuple(
      std::tuple<Ts...>&,
      std::index_sequence<sizeof...(Ts)>) { return true; }

  template<std::size_t I, typename... Ts>
  std::enable_if_t<(I < sizeof...(Ts)), bool>
  ReadTuple(std::tuple<Ts...>& output, std::index_sequence<I>) {
    return Read(std::get<I>(output)) && 
            ReadTuple(output, std::index_sequence<I+1>());
  }

  template<typename... Ts>
  bool Read(std::tuple<Ts...>& output) {
    return ReadTuple(output, std::index_sequence<0>());
  }

  template<typename T1, typename T2>
  bool Read(std::pair<T1, T2>& output) {
    return Read(output.first) && Read(output.second);
  }

  template<typename T>
  std::enable_if_t<bytestream_impl::has_from_bytestream<T>::value, bool>
  Read(T& output) {
    output.FromByteStream(*this);
    return status == Status::OK;
  }

  template<typename T>
  std::enable_if_t<bytestream_impl::can_insert<T>::value, bool>
  Read(T& output) {
    return ReadContainer(output, [](T& container, typename T::value_type&& value_type) {
      container.insert(std::move(value_type));
    });
  }

  template<typename T>
  std::enable_if_t<bytestream_impl::can_push_back<T>::value, bool>
  Read(T& output) {
    return ReadContainer(output, [&](T& container, typename T::value_type&& value_type) {
      container.push_back(std::move(value_type));
    });
  }

  template<typename T, typename Inserter>
  bool ReadContainer(T& output, Inserter inserter) {
    size_t container_size;
    if (not Read(container_size)) return false;
    for (size_t i = 0; i < container_size; ++i) {
      typename bytestream_impl::ConstCastValueType<typename T::value_type>::type value_type;
      if (not Read(value_type)) return false;
      inserter(output, std::move(value_type));
    }
    return true;
  }

  Status status = Status::OK;
  const std::byte* buffer = nullptr;
  size_t read_ptr = 0;
  size_t buffer_len = 0;
};

}  // namespace quick

#endif  // QUICK_BYTE_STREAM_HPP_
