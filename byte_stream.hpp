// Copyright: 2019 Mohit Saini
// Author: Mohit Saini (mohitsaini1196@gmail.com)

#ifndef QUICK_BYTE_STREAM_HPP_
#define QUICK_BYTE_STREAM_HPP_

#include <iostream>
#include <type_traits>
#include <cstdint>
#include <string>
#include <cstring>
#include <vector>
#include <set>
#include <unordered_set>
#include <list>
#include <tuple>
#include <utility>
#include <map>
#include <unordered_map>

#include "quick/type_traits.hpp"

namespace quick {
namespace detail {
inline bool IsLittleEndianSystem() {
  uint32_t tmp = {0x01020304};
  return ((reinterpret_cast<uint8_t*>(&tmp))[0] == 4);
}
static const bool is_little_endian_system = IsLittleEndianSystem();
}


// Can Store at max 4G data.
class ByteStream {
  struct Error {
    enum Type {INVALID_READ};
    Type type;
  };
  static constexpr bool little_endian_storage = true;
  std::string str_value;
  uint32_t read_ptr = 0;

 public:
  const std::string& str() const {
    return str_value;
  }
  void str(const std::string& str_value) {
    this->str_value = str_value;
  }
  bool end() const {
    return (read_ptr >= str_value.size());
  }

  template<typename T>
  std::enable_if_t<(std::is_fundamental<T>::value ||
                    std::is_enum<T>::value), ByteStream>&
  operator<<(const T& input) {
    uint32_t len = str_value.size();
    str_value.resize(len + sizeof(T));
    const auto* input_ptr = reinterpret_cast<const uint8_t*>(&input);
    if (little_endian_storage == detail::is_little_endian_system) {
      std::memcpy(&str_value[len], input_ptr, sizeof(T));
    } else {
      for (uint32_t i = 0; i < sizeof(T); i++) {
        str_value[len + i] = input_ptr[sizeof(T) -i - 1];
      }
    }
    return *this;
  }

  template<typename T>
  std::enable_if_t<(std::is_fundamental<T>::value ||
                    std::is_enum<T>::value), ByteStream>&
  operator>>(T& output) {
    uint32_t len = str_value.size();
    if (read_ptr + sizeof(T) > len) {
      throw Error {Error::INVALID_READ};
    }
    auto* output_ptr = reinterpret_cast<uint8_t*>(&output);
    if (little_endian_storage == detail::is_little_endian_system) {
      std::memcpy(output_ptr, &str_value[read_ptr], sizeof(T));
    } else {
      for (uint32_t i = 0; i < sizeof(T); i++) {
        output_ptr[sizeof(T) -i - 1] = str_value[read_ptr + i];
      }
    }
    read_ptr += sizeof(T);
    return *this;
  }

  ByteStream& operator<<(const std::string& input) {
    auto& bs = *this;
    str_value.reserve(sizeof(uint64_t) + input.size());
    bs << static_cast<uint64_t>(input.size());
    str_value += input;
    return bs;
  }

  ByteStream& operator>>(std::string& output) {
    auto& bs = *this;
    uint64_t string_size;
    bs >> string_size;
    if (bs.read_ptr + string_size > bs.str_value.size()) {
      bs.read_ptr -= sizeof(uint64_t);
      throw Error {Error::INVALID_READ};
    }
    output = bs.str_value.substr(bs.read_ptr, string_size);
    bs.read_ptr += string_size;
    return bs;
  }
};

class OByteStream: public ByteStream {
  template<typename T>
  OByteStream& operator>>(T&) = delete;
};

class IByteStream: public ByteStream {
  template<typename T>
  OByteStream& operator<<(T&) = delete;
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
  bs << static_cast<uint64_t>(input.size());
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
                 std::declval<const T&>().Serialize(
                   std::declval<OByteStream&>()))>::value,
  ByteStream>& operator<<(ByteStream& bs, const T& input) {
  input.Serialize(static_cast<OByteStream&>(bs));
  return bs;
}

template<typename T>
std::enable_if_t<
  std::is_same<void,
               decltype(
                 std::declval<T&>().Deserialize(
                   std::declval<IByteStream&>()))>::value,
  ByteStream>& operator>>(ByteStream& bs, T& output) {
  output.Deserialize(static_cast<IByteStream&>(bs));
  return bs;
}

template<typename T>
std::enable_if_t<(quick::is_specialization<T, std::vector>::value ||
                  quick::is_specialization<T, std::list>::value ||
                  quick::is_specialization<T, std::unordered_set>::value ||
                  quick::is_specialization<T, std::set>::value), ByteStream>&
operator<<(ByteStream& bs, const T& input) {
  bs << static_cast<uint64_t>(input.size());
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
  uint64_t container_size;
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
  uint64_t container_size;
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
  uint64_t vector_size;
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
  uint64_t container_size;
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
  uint64_t container_size;
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
