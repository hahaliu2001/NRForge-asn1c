#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-s4.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
printf '%s\n' '#include <runtime.hpp>' > "$work/generated.cpp"
./check_asn1typed_owned_slice >> "$work/generated.cpp"
cat >> "$work/generated.cpp" <<'CPP'
#include <cassert>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>
#ifdef NDEBUG
#error "S4 codec checks require active assertions"
#endif
using namespace nrforge::synthetic::cpp_aper_slice;
using nrforge::aper::ErrorCode;
using nrforge::aper::Limits;
using nrforge::aper::Result;
using nrforge::aper::CompleteEncoding;

static void expect_bytes(const Result<CompleteEncoding>& result,
                         std::initializer_list<unsigned> expected) {
  assert(result);
  assert(result.value().octets.size() == expected.size());
  std::size_t i = 0;
  for(unsigned byte : expected)
    assert(result.value().octets[i++] == std::byte{static_cast<unsigned char>(byte)});
}

template<class T>
static void expect_error(const Result<T>& result, ErrorCode code, std::size_t bit) {
  assert(!result);
  assert(result.error().code == code);
  assert(result.error().bit_offset == bit);
}

static void expect_metrics(const CompleteEncoding& e, std::size_t end,
                          unsigned padding, bool substitution,
                          std::size_t complete_bits, std::size_t octets) {
  assert(e.last_field_end_bit == end);
  assert(e.final_padding_bits == padding);
  assert(e.empty_encoding_substitution == substitution);
  assert(e.complete_encoding_bits == complete_bits);
  assert(e.octet_count == octets);
  assert(e.octets.size() == octets);
}

template<class Decode>
static void check_prefixes(const std::vector<std::byte>& wire,
                           std::initializer_list<std::size_t> missing_offsets,
                           Decode decode) {
  assert(wire.size() == missing_offsets.size());
  std::size_t prefix = 0;
  for(std::size_t expected_offset : missing_offsets) {
    auto result = decode(std::span<const std::byte>(wire.data(), prefix));
    expect_error(result, ErrorCode::truncated_input, expected_offset);
    ++prefix;
  }
}

static Packet packet(bool count_branch, int presence) {
  Packet p{};
  p.count = 1;
  if(presence) p.enabled = (presence == 2);
  p.selection = count_branch ? Selection{Selection_count{255}}
                             : Selection{Selection_flag{true}};
  return p;
}

int main() {
  for(Count value : {Count{0}, Count{1}, Count{255}, Count{256}, Count{65535}}) {
    auto encoded = encode_count(value);
    expect_bytes(encoded, {static_cast<unsigned>(value >> 8),
                           static_cast<unsigned>(value & 0xff)});
    expect_metrics(encoded.value(), 16, 0, false, 16, 2);
    auto decoded = decode_count(encoded.value().octets);
    assert(decoded && decoded.value() == value);
  }
  for(std::uint64_t value : {UINT64_C(65536), UINT64_C(1) << 32, UINT64_MAX})
    expect_error(encode_count(value), ErrorCode::constraint_violation, 0);
  Packet overflow = packet(false, 0);
  overflow.count = UINT64_C(65536);
  expect_error(encode_packet(overflow), ErrorCode::constraint_violation, 1);

  Selection flag = Selection_flag{true};
  auto flag_wire = encode_selection(flag);
  expect_bytes(flag_wire, {0x40});
  expect_metrics(flag_wire.value(), 2, 6, false, 8, 1);
  auto flag_decoded = decode_selection(flag_wire.value().octets);
  assert(flag_decoded && flag_decoded.value().index() == 0);
  assert(std::get<Selection_flag>(flag_decoded.value()).value);

  Selection integer = Selection_count{255};
  auto integer_wire = encode_selection(integer);
  expect_bytes(integer_wire, {0x80, 0x00, 0xff});
  expect_metrics(integer_wire.value(), 24, 0, false, 24, 3);
  auto integer_decoded = decode_selection(integer_wire.value().octets);
  assert(integer_decoded && integer_decoded.value().index() == 1);
  assert(std::get<Selection_count>(integer_decoded.value()).value == 255);

  const std::array<std::array<unsigned, 4>, 3> flag_packets{{
      {{0x00, 0x00, 0x01, 0x40}},
      {{0x80, 0x00, 0x01, 0x20}},
      {{0x80, 0x00, 0x01, 0xa0}}}};
  const std::array<std::array<unsigned, 6>, 3> integer_packets{{
      {{0x00, 0x00, 0x01, 0x80, 0x00, 0xff}},
      {{0x80, 0x00, 0x01, 0x40, 0x00, 0xff}},
      {{0x80, 0x00, 0x01, 0xc0, 0x00, 0xff}}}};
  for(bool count_branch : {false, true}) {
    for(int presence = 0; presence != 3; ++presence) {
      Packet value = packet(count_branch, presence);
      auto encoded = encode_packet(value);
      if(count_branch) {
        expect_bytes(encoded, {integer_packets[presence][0], integer_packets[presence][1],
                               integer_packets[presence][2], integer_packets[presence][3],
                               integer_packets[presence][4], integer_packets[presence][5]});
      } else {
        expect_bytes(encoded, {flag_packets[presence][0], flag_packets[presence][1],
                               flag_packets[presence][2], flag_packets[presence][3]});
      }
      auto decoded = decode_packet(encoded.value().octets);
      assert(decoded);
      assert(decoded.value().count == value.count);
      assert(decoded.value().enabled == value.enabled);
      assert(decoded.value().selection.index() == value.selection.index());
      if(count_branch)
        assert(std::get<Selection_count>(decoded.value().selection).value == 255);
      else
        assert(std::get<Selection_flag>(decoded.value().selection).value);
      const std::size_t end = count_branch ? 48 : (presence ? 27 : 26);
      const unsigned padding = count_branch ? 0 : (presence ? 5 : 6);
      expect_metrics(encoded.value(), end, padding, false, end + padding,
                     count_branch ? 6 : 4);

      const std::vector<std::byte> copy = encoded.value().octets;
      if(count_branch)
        check_prefixes(copy, {0, 8, 16, 24, 32, 40}, [] (auto input) { return decode_packet(input, Limits{}); });
      else
        check_prefixes(copy, {0, 8, 16, 24}, [] (auto input) { return decode_packet(input, Limits{}); });
    }
  }

  check_prefixes(flag_wire.value().octets, {0}, [] (auto input) { return decode_selection(input, Limits{}); });
  check_prefixes(integer_wire.value().octets, {0, 8, 16}, [] (auto input) { return decode_selection(input, Limits{}); });
  auto count_wire = encode_count(1);
  check_prefixes(count_wire.value().octets, {0, 8}, [] (auto input) { return decode_count(input, Limits{}); });

  {
    namespace reversed = nrforge::synthetic::reversed;
    reversed::Selection reversed_flag = reversed::Selection_flag{true};
    reversed::Selection reversed_count = reversed::Selection_count{255};
    auto rf = reversed::encode_selection(reversed_flag);
    expect_bytes(rf, {0x40});
    auto rfd = reversed::decode_selection(rf.value().octets);
    assert(rfd && rfd.value().index() == 1);
    assert(std::get<reversed::Selection_flag>(rfd.value()).value);
    auto rc = reversed::encode_selection(reversed_count);
    expect_bytes(rc, {0x80, 0x00, 0xff});
    auto rcd = reversed::decode_selection(rc.value().octets);
    assert(rcd && rcd.value().index() == 0);
    assert(std::get<reversed::Selection_count>(rcd.value()).value == 255);
    check_prefixes(rf.value().octets, {0}, [] (auto input) { return reversed::decode_selection(input, Limits{}); });
    check_prefixes(rc.value().octets, {0, 8, 16}, [] (auto input) { return reversed::decode_selection(input, Limits{}); });

    reversed::Packet rp{};
    rp.count = 1;
    rp.selection = reversed::Selection_flag{true};
    auto re = reversed::encode_packet(rp);
    expect_bytes(re, {0x00, 0x00, 0x01, 0x40});
    auto rd = reversed::decode_packet(re.value().octets);
    assert(rd && rd.value().count == 1 && !rd.value().enabled);
    assert(rd.value().selection.index() == 1);
    assert(std::get<reversed::Selection_flag>(rd.value().selection).value);
  }

  const std::array<std::byte, 3> selection_bad_alignment{
      std::byte{0xc0}, std::byte{0}, std::byte{0xff}};
  expect_error(decode_selection(selection_bad_alignment), ErrorCode::nonzero_padding, 1);
  const std::array<std::byte, 4> packet_bad_count_alignment{
      std::byte{0x40}, std::byte{0}, std::byte{1}, std::byte{0x40}};
  expect_error(decode_packet(packet_bad_count_alignment), ErrorCode::nonzero_padding, 1);
  const std::array<std::byte, 6> packet_bad_choice_alignment{
      std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0xc0}, std::byte{0}, std::byte{0xff}};
  expect_error(decode_packet(packet_bad_choice_alignment), ErrorCode::nonzero_padding, 25);
  const std::array<std::byte, 1> selection_bad_final_padding{std::byte{0x60}};
  expect_error(decode_selection(selection_bad_final_padding), ErrorCode::nonzero_padding, 2);
  const std::array<std::byte, 4> packet_bad_final_padding{
      std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x60}};
  expect_error(decode_packet(packet_bad_final_padding), ErrorCode::nonzero_padding, 26);
  const std::array<std::byte, 4> packet_present_bad_final_padding{
      std::byte{0x80}, std::byte{0}, std::byte{1}, std::byte{0x30}};
  expect_error(decode_packet(packet_present_bad_final_padding), ErrorCode::nonzero_padding, 27);

  const std::array<std::byte, 3> count_trailing{std::byte{0}, std::byte{1}, std::byte{0}};
  expect_error(decode_count(count_trailing), ErrorCode::trailing_data, 16);
  const std::array<std::byte, 2> selection_trailing{std::byte{0x40}, std::byte{0}};
  expect_error(decode_selection(selection_trailing), ErrorCode::trailing_data, 8);
  const std::array<std::byte, 5> packet_trailing{
      std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x40}, std::byte{0}};
  expect_error(decode_packet(packet_trailing), ErrorCode::trailing_data, 32);

  Limits exact{};
  exact.max_input_octets = 2;
  exact.max_output_octets = 2;
  exact.max_wire_bits = 16;
  auto bounded_count = encode_count(1, exact);
  expect_bytes(bounded_count, {0, 1});
  auto bounded_decode = decode_count(bounded_count.value().octets, exact);
  assert(bounded_decode && bounded_decode.value() == 1);
  Limits short_input = exact;
  short_input.max_input_octets = 1;
  expect_error(decode_count(bounded_count.value().octets, short_input), ErrorCode::resource_limit, 0);
  Limits short_output = exact;
  short_output.max_output_octets = 1;
  expect_error(encode_count(1, short_output), ErrorCode::resource_limit, 0);
  Limits short_wire = exact;
  short_wire.max_wire_bits = 15;
  expect_error(encode_count(1, short_wire), ErrorCode::resource_limit, 0);
  expect_error(decode_count(bounded_count.value().octets, short_wire), ErrorCode::resource_limit, 0);

  Limits sticky_encode_limits{};
  sticky_encode_limits.max_output_octets = 1;
  auto ignored_encode_error = nrforge::aper::encode_complete(
      std::uint64_t{0}, sticky_encode_limits, [](nrforge::aper::FieldWriter& fields) {
        auto prefix = fields.write_bit(true);
        assert(prefix);
        auto first = codec::put_Count(fields, UINT64_C(65536));
        expect_error(first, ErrorCode::constraint_violation, 1);
        auto second = fields.write_bit(false);
        expect_error(second, ErrorCode::constraint_violation, 1);
        auto third = fields.write_aligned_u16_be(1);
        expect_error(third, ErrorCode::constraint_violation, 1);
        return Result<void>::success();
      });
  expect_error(ignored_encode_error, ErrorCode::constraint_violation, 1);

  const std::array<std::byte, 1> short_count{std::byte{0}};
  auto ignored_decode_error = nrforge::aper::decode_complete<Count>(
      short_count, Limits{}, [](nrforge::aper::FieldReader& fields) {
        auto first = codec::get_Count(fields);
        expect_error(first, ErrorCode::truncated_input, 8);
        auto second = fields.read_aligned_u16_be();
        expect_error(second, ErrorCode::truncated_input, 8);
        return Result<Count>::failure({ErrorCode::resource_limit, 77});
      });
  expect_error(ignored_decode_error, ErrorCode::truncated_input, 8);
}
CPP
${CXX:-c++} -UNDEBUG -std=c++20 -Wall -Wextra -Werror -pedantic -I../libaper "$work/generated.cpp" ../libaper/runtime.cpp -o "$work/check"
"$work/check"
echo 'PASS S4 generated codec vectors, error matrix, limits, and sticky checks'
