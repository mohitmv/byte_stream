// Original: https://github.com/mohitmv/quick/blob/master/include/quick/byte_stream.hpp
// Author: Mohit Saini (mohitsaini1196@gmail.com)

#ifndef QUICK_BYTE_STREAM_HPP_
#define QUICK_BYTE_STREAM_HPP_

#include <iostream>
#include <type_traits>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstring>
#include <tuple>
#include <utility>
#include <vector>
#include <bit>

namespace quick {

static_assert(std::endian::native == std::endian::little, "");

namespace bytestream_impl {

enum class Status {OK, INVALID_READ};

template<typename T>
std::enable_if_t<(std::is_fundamental<T>::value ||
                  std::is_enum<T>::value)>
WritePremitiveType(void* dst, T value) {
  std::memcpy(dst, &value, sizeof(T));
}

void WriteBuffer(void* dst, const std::byte* buffer_ptr, size_t buffer_size) {
  std::memcpy(dst, buffer_ptr, buffer_size);
}

template<typename T, typename = void> struct has_iterator : std::false_type { };
template<typename T>
struct has_iterator<T, std::void_t<typename T::iterator>> : std::true_type { };

template<typename T, typename = void> struct has_to_bytestream : std::false_type { };
template<typename T>
struct has_to_bytestream<T, std::void_t<typename T::ToByteStream>> : std::true_type { };

}  // namespace bytestream_impl


class OByteStream {
public:
  const std::vector<std::byte>& GetBytes() const {  return output_bytes; }
  std::vector<std::byte>& GetMutableBytes() { return output_bytes; }

  template<typename T>
  OByteStream& operator<<(const T& input) { Write(input); return *this; }

 private:
  void Write(const std::string_view& input) {
    size_t size0 = output_bytes.size();
    output_bytes.resize(size0 + sizeof(size_t) + input.size());
    bytestream_impl::WritePremitiveType(&output_bytes[size0], input.size());
    bytestream_impl::WriteBuffer(&output_bytes[size0 + sizeof(size_t)], input.data(), input.size());
  }

  template<typename T>
  std::enable_if_t<(std::is_fundamental<T>::value ||
                    std::is_enum<T>::value)>
  Write(const std::vector<T>& input) {
    size_t size0 = output_bytes.size();
    size_t real_input_size = sizeof(T) * input.size();
    output_bytes.resize(size0 + sizeof(size_t) + real_input_size);
    bytestream_impl::WritePremitiveType(&output_bytes[size0], input.size());
    bytestream_impl::WriteBuffer(&output_bytes[size0 + sizeof(size_t)], input.data(), real_input_size);
    return *this;
  }

  template<typename T>
  std::void_t<typename T::iterator> Write(const T& container) {
    Write(container.size());
    for (auto& item : container) {
      Write(item);
    }
  }

  template<typename T>
  std::enable_if_t<(std::is_fundamental<T>::value || std::is_enum<T>::value)>
  Write(T input) {
    size_t size0 = output_bytes.size();
    str_value.resize(size0 + sizeof(T));
    bytestream_impl::WritePremitiveType(&str_value[size0], input);
    return *this;
  }

  std::vector<std::byte> output_bytes;
};


class IByteStream {
 public:
  using Status = bytestream_impl::Status;

  IByteStream(const std::byte* buffer, size_t len): buffer(buffer), buffer_len(len) { }

  IByteStream(const std::string_view& str_view)
      : buffer(static_cast<const std::byte*>(str_view.data())),
        buffer_len(str_view.size()) { }

  template<typename T>
  IByteStream& operator>>(T& output) {
    if (Read(output)) return *this;
    status = Status::INVALID_READ;
    return *this;
  }

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
    output = std::string_view(buffer + read_ptr, string_size);
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
    std::memcpy(&value, buffer + read_ptr, sizeof(T));
    read_ptr += sizeof(T);
    return true;
  }

  Status status;
  const std::byte* buffer;
  size_t read_ptr = 0;
  size_t buffer_len;
};


namespace detail {

template<typename... Ts>
void SerializeTuple(
    ByteStream&,
    const std::tuple<Ts...>&,
    std::index_sequence<sizeof...(Ts)>) {}

template<std::size_t I, typename... Ts>
std::enable_if_t<(I < sizeof...(Ts))>
SerializeTuple(ByteStream& bs,  // NOLINT
               const std::tuple<Ts...>& input,
               std::index_sequence<I>) {
  bs << std::get<I>(input);
  SerializeTuple(bs, input, std::index_sequence<I+1>());
}

template<typename... Ts>
void DeserializeTuple(
    ByteStream&,
    std::tuple<Ts...>&,
    std::index_sequence<sizeof...(Ts)>) {}

template<std::size_t I, typename... Ts>
std::enable_if_t<(I < sizeof...(Ts))>
DeserializeTuple(ByteStream& bs,  // NOLINT
               std::tuple<Ts...>& output,  // NOLINT
               std::index_sequence<I>) {
  bs >> std::get<I>(output);
  DeserializeTuple(bs, output, std::index_sequence<I+1>());
}

template<typename MapType>
ByteStream& SerializeMap(ByteStream& bs, const MapType& input) {  // NOLINT
  bs << static_cast<size_t>(input.size());
  for (const auto& item : input) {
    bs << item.first << item.second;
  }
  return bs;
}

}  // namespace detail

template<typename... Ts>
ByteStream& operator<<(ByteStream& bs, const std::tuple<Ts...>& input) {
  detail::SerializeTuple(bs, input, std::index_sequence<0>());
  return bs;
}

template<typename... Ts>
ByteStream& operator>>(ByteStream& bs, std::tuple<Ts...>& output) {
  detail::DeserializeTuple(bs, output, std::index_sequence<0>());
  return bs;
}

template<typename T1, typename T2>
ByteStream& operator<<(ByteStream& bs, const std::pair<T1, T2>& input) {
  bs << input.first << input.second;
  return bs;
}

template<typename T1, typename T2>
ByteStream& operator>>(ByteStream& bs, std::pair<T1, T2>& output) {
  bs >> output.first >> output.second;
  return bs;
}

template<typename T>
std::enable_if_t<
  std::is_same<void,
               decltype(
                 std::declval<const T&>().ToByteStream(
                   std::declval<OByteStream&>()))>::value,
  ByteStream>& operator<<(ByteStream& bs, const T& input) {
  input.ToByteStream(static_cast<OByteStream&>(bs));
  return bs;
}

template<typename T>
std::enable_if_t<
  std::is_same<void,
               decltype(
                 std::declval<T&>().FromByteStream(
                   std::declval<IByteStream&>()))>::value,
  ByteStream>& operator>>(ByteStream& bs, T& output) {
  output.FromByteStream(static_cast<IByteStream&>(bs));
  return bs;
}

template<typename T>
std::enable_if_t<(quick::is_specialization<T, std::vector>::value ||
                  quick::is_specialization<T, std::list>::value ||
                  quick::is_specialization<T, std::unordered_set>::value ||
                  quick::is_specialization<T, std::set>::value), ByteStream>&
operator<<(ByteStream& bs, const T& input) {
  bs << static_cast<size_t>(input.size());
  for (const auto& item : input) {
    bs << item;
  }
  return bs;
}

template<typename... Ts>
ByteStream& operator<<(ByteStream& bs, const std::unordered_map<Ts...>& input) {
  return detail::SerializeMap(bs, input);
}

template<typename... Ts>
ByteStream& operator<<(ByteStream& bs, const std::map<Ts...>& input) {
  return detail::SerializeMap(bs, input);
}

template<typename K, typename... Ts>
ByteStream& operator>>(ByteStream& bs, std::unordered_map<K, Ts...>& output) {
  size_t container_size;
  bs >> container_size;
  output.clear();
  output.reserve(container_size);
  K k;
  for (uint32_t i = 0; i < container_size; i++) {
    bs >> k;
    bs >> output[k];
  }
  return bs;
}

template<typename K, typename... Ts>
ByteStream& operator>>(ByteStream& bs, std::map<K, Ts...>& output) {
  size_t container_size;
  bs >> container_size;
  output.clear();
  K k;
  for (uint32_t i = 0; i < container_size; i++) {
    bs >> k;
    bs >> output[k];
  }
  return bs;
}

template<typename T>
ByteStream& operator>>(ByteStream& bs, std::vector<T>& output) {
  size_t vector_size;
  bs >> vector_size;
  output.resize(vector_size);
  for (uint32_t i = 0; i < vector_size; i++) {
    bs >> output[i];
  }
  return bs;
}

template<typename T>
std::enable_if_t<(quick::is_specialization<T, std::unordered_set>::value),
                 ByteStream>&
operator>>(ByteStream& bs, T& output) {
  size_t container_size;
  bs >> container_size;
  output.clear();
  output.reserve(container_size);
  for (uint32_t i = 0; i < container_size; i++) {
    typename T::value_type v;
    bs >> v;
    output.insert(std::move(v));
  }
  return bs;
}


template<typename T>
std::enable_if_t<(quick::is_specialization<T, std::list>::value ||
                  quick::is_specialization<T, std::set>::value), ByteStream>&
operator>>(ByteStream& bs, T& output) {
  size_t container_size;
  bs >> container_size;
  output.clear();
  for (uint32_t i = 0; i < container_size; i++) {
    typename T::value_type v;
    bs >> v;
    output.insert(std::move(v));
  }
  return bs;
}


}  // namespace quick

namespace qk = quick;


#endif  // QUICK_BYTE_STREAM_HPP_
