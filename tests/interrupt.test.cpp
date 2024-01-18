// Copyright 2024 Khalil Estell
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <libhal-armcortex/interrupt.hpp>

#include <libhal-armcortex/system_control.hpp>

#include "helper.hpp"
#include "interrupt_reg.hpp"
#include "system_controller_reg.hpp"

#include <boost/ut.hpp>

namespace hal::cortex_m {
namespace {
void top_of_stack()
{
}
void reset_handler()
{
}
}  // namespace

void interrupt_test()
{
  using namespace boost::ut;

  static constexpr size_t expected_interrupt_count = 42;

  std::array<interrupt_pointer, 2> original_ivt{ top_of_stack, reset_handler };
  auto stub_out_nvic = stub_out_registers(&nvic);
  auto stub_out_scb = stub_out_registers(&scb);

  scb->vtor = reinterpret_cast<std::intptr_t>(original_ivt.data());

  expect(that % 16 == interrupt::core_interrupts);

  should("interrupt::initialize()") = [&] {
    // Setup
    expect(that % nullptr == interrupt::get_vector_table().data());
    expect(that % 0 == interrupt::get_vector_table().size());

    // Exercise
    interrupt::initialize<expected_interrupt_count>();

    auto pointer =
      reinterpret_cast<intptr_t>(interrupt::get_vector_table().data());

    // Verify
    expect(that % nullptr != interrupt::get_vector_table().data());
    expect(that % (expected_interrupt_count + interrupt::core_interrupts) ==
           interrupt::get_vector_table().size());
    expect(that % pointer == scb->vtor);

    // Verify: Nothing in the interrupt vector table should have changed
    auto top_of_stack_expected = reinterpret_cast<void*>(top_of_stack);
    auto reset_handler_expected = reinterpret_cast<void*>(reset_handler);

    auto ivt_0 = interrupt::get_vector_table()[0];
    auto ivt_1 = interrupt::get_vector_table()[1];
    auto top_of_stack_actual = reinterpret_cast<void*>(ivt_0);
    auto reset_handler_actual = reinterpret_cast<void*>(ivt_1);

    expect(that % top_of_stack_expected == top_of_stack_actual);
    expect(that % reset_handler_expected == reset_handler_actual);

    for (const auto interrupt_function :
         interrupt::get_vector_table().subspan(2)) {
      auto nop_address = reinterpret_cast<void*>(&interrupt::nop);
      auto function_address = reinterpret_cast<void*>(interrupt_function);
      expect(that % nop_address == function_address);
    }
  };

  should("interrupt::enable()") = [&] {
    interrupt_pointer dummy_handler = []() {};

    should("interrupt::enable(21)") = [&]() {
      // Setup
      static constexpr std::uint16_t expected_event_number = 21;
      static constexpr std::uint16_t shifted_event_number =
        (expected_event_number - interrupt::core_interrupts);
      unsigned index = shifted_event_number >> 5;
      unsigned bit_position = shifted_event_number & 0x1F;

      // Exercise
      interrupt(expected_event_number).enable(dummy_handler);

      // Verify
      expect(that % dummy_handler ==
             interrupt::get_vector_table()[expected_event_number]);
      std::uint32_t iser = (1U << bit_position) & nvic->iser.at(index);
      expect(that % (1 << shifted_event_number) == iser);
    };

    should("interrupt::enable(17)") = [&]() {
      // Setup
      static constexpr std::uint16_t expected_event_number = 17;
      static constexpr std::uint16_t shifted_event_number =
        (expected_event_number - interrupt::core_interrupts);
      unsigned index = shifted_event_number >> 5;
      unsigned bit_position = shifted_event_number & 0x1F;

      // Exercise
      interrupt(expected_event_number).enable(dummy_handler);

      // Verify
      expect(that % dummy_handler ==
             interrupt::get_vector_table()[expected_event_number]);
      std::uint32_t iser = (1U << bit_position) & nvic->iser.at(index);
      expect(that % (1 << shifted_event_number) == iser);
    };

    should("interrupt::enable(5)") = [&]() {
      // Setup
      static constexpr std::uint16_t expected_event_number = 5;
      const auto old_nvic = *nvic;

      // Exercise
      interrupt(expected_event_number).enable(dummy_handler);

      // Verify
      // Verify: That the dummy handler was added to the IVT (ISER)
      expect(that % dummy_handler ==
             interrupt::get_vector_table()[expected_event_number]);
      // Verify: ISER[] should not have changed when enable() succeeds but the
      // IRQ is less than 0.
      for (size_t i = 0; i < old_nvic.iser.size(); i++) {
        expect(old_nvic.iser.at(i) == nvic->iser.at(i));
      }
    };

    should("interrupt::enable(100) fail") = [&]() {
      // Setup
      // Setup: Re-initialize interrupts which will set each vector to "nop"
      interrupt::reinitialize<expected_interrupt_count>();
      static constexpr std::uint16_t expected_event_number = 100;
      const auto old_nvic = *nvic;

      // Exercise
      interrupt(expected_event_number).enable(dummy_handler);

      // Verify
      // Verify: Nothing in the interrupt vector table should have changed
      auto top_of_stack_expected = reinterpret_cast<void*>(top_of_stack);
      auto reset_handler_expected = reinterpret_cast<void*>(reset_handler);

      auto ivt_0 = interrupt::get_vector_table()[0];
      auto ivt_1 = interrupt::get_vector_table()[1];
      auto top_of_stack_actual = reinterpret_cast<void*>(ivt_0);
      auto reset_handler_actual = reinterpret_cast<void*>(ivt_1);

      expect(that % top_of_stack_expected == top_of_stack_actual);
      expect(that % reset_handler_expected == reset_handler_actual);

      for (const auto interrupt_function :
           interrupt::get_vector_table().subspan(2)) {
        auto nop_address = reinterpret_cast<void*>(&interrupt::nop);
        auto function_address = reinterpret_cast<void*>(interrupt_function);
        expect(that % nop_address == function_address);
      }

      // Verify: ISER[] should not have changed when enable() fails.
      for (size_t i = 0; i < old_nvic.iser.size(); i++) {
        expect(old_nvic.iser.at(i) == nvic->iser.at(i));
      }
    };
  };

  should("interrupt(expected_event_number).disable()") = [&] {
    should("interrupt(expected_event_number).disable(21)") = [&]() {
      // Setup
      static constexpr std::uint16_t expected_event_number = 21;
      static constexpr std::uint16_t shifted_event_number =
        (expected_event_number - interrupt::core_interrupts);
      unsigned index = static_cast<uint32_t>(shifted_event_number) >> 5;
      unsigned bit_position =
        static_cast<uint32_t>(shifted_event_number) & 0x1F;

      // Exercise
      interrupt(expected_event_number).disable();

      // Verify
      expect(that % &interrupt::nop ==
             interrupt::get_vector_table()[expected_event_number]);

      std::uint32_t icer = (1U << bit_position) & nvic->icer.at(index);
      expect(that % (1 << shifted_event_number) == icer);
    };

    should("interrupt(expected_event_number).disable(17)") = [&]() {
      // Setup
      static constexpr int expected_event_number = 17;
      static constexpr std::uint16_t shifted_event_number =
        (expected_event_number - interrupt::core_interrupts);
      unsigned index = static_cast<uint32_t>(shifted_event_number) >> 5;
      unsigned bit_position =
        static_cast<uint32_t>(shifted_event_number) & 0x1F;

      // Exercise
      interrupt(expected_event_number).disable();

      // Verify
      expect(that % &interrupt::nop ==
             interrupt::get_vector_table()[expected_event_number]);

      std::uint32_t icer = (1U << bit_position) & nvic->icer.at(index);
      expect(that % (1 << shifted_event_number) == icer);
    };

    should("interrupt(expected_event_number).disable(5)") = [&]() {
      // Setup
      static constexpr std::uint16_t expected_event_number = 5;
      const auto old_nvic = *nvic;

      // Exercise
      interrupt(expected_event_number).disable();

      // Verify
      // Verify: That the dummy handler was added to the IVT (icer )
      expect(that % &interrupt::nop ==
             interrupt::get_vector_table()[expected_event_number]);
      // Verify: icer[] should not have changed when disable() succeeds but the
      // IRQ is less than 0.
      for (size_t i = 0; i < old_nvic.icer.size(); i++) {
        expect(old_nvic.icer.at(i) == nvic->icer.at(i));
      }
    };

    should("interrupt(expected_event_number).disable(100) fail") = [&]() {
      // Setup
      // Setup: Re-initialize interrupts which will set each vector to "nop"
      interrupt::reinitialize<expected_interrupt_count>();
      static constexpr int expected_event_number = 100;
      const auto old_nvic = *nvic;

      // Exercise
      interrupt(expected_event_number).disable();

      // Verify: Nothing in the interrupt vector table should have changed
      auto top_of_stack_expected = reinterpret_cast<void*>(top_of_stack);
      auto reset_handler_expected = reinterpret_cast<void*>(reset_handler);

      auto ivt_0 = interrupt::get_vector_table()[0];
      auto ivt_1 = interrupt::get_vector_table()[1];
      auto top_of_stack_actual = reinterpret_cast<void*>(ivt_0);
      auto reset_handler_actual = reinterpret_cast<void*>(ivt_1);

      expect(that % top_of_stack_expected == top_of_stack_actual);
      expect(that % reset_handler_expected == reset_handler_actual);

      for (const auto interrupt_function :
           interrupt::get_vector_table().subspan(2)) {
        auto nop_address = reinterpret_cast<void*>(&interrupt::nop);
        auto function_address = reinterpret_cast<void*>(interrupt_function);
        expect(that % nop_address == function_address);
      }

      // Verify: icer[] should not have changed when disable() fails.
      for (size_t i = 0; i < old_nvic.icer.size(); i++) {
        expect(old_nvic.icer.at(i) == nvic->icer.at(i));
      }
    };
  };

  should("interrupt::get_vector_table()") = [&] {
    // Setup
    expect(that % nullptr != interrupt::get_vector_table().data());
    expect(that % 0 != interrupt::get_vector_table().size());

    // Exercise & Verify
    expect(interrupt::get_vector_table().data() ==
           interrupt::get_vector_table().data());
    expect(that % interrupt::get_vector_table().size() ==
           interrupt::get_vector_table().size());
  };
};
}  // namespace hal::cortex_m
