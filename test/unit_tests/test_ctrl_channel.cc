/**
 * @file test_control_channel.cc
 * @brief Test function for generating and decoding the control channel symbols.
 */

#include <gtest/gtest.h>
// For some reason, gtest include order matters

#include "comms-lib.h"
#include "config.h"
#include "data_generator.h"
#include "datatype_conversion.h"
#include "gettime.h"
#include "memory_manage.h"
#include "utils.h"

TEST(TestControl, VerifyCorrectness) {
  auto cfg =
      std::make_unique<Config>("files/config/ci/tddconfig-sim-ctrl.json");
  cfg->GenPilots();
  std::vector<std::complex<int16_t>*> data_buffer(
      cfg->Frame().NumDlControlSyms());
  for (size_t i = 0; i < cfg->Frame().NumDlControlSyms(); i++) {
    data_buffer.at(i) =
        static_cast<std::complex<int16_t>*>(Agora_memory::PaddedAlignedAlloc(
            Agora_memory::Alignment_t::kAlign64,
            2 * cfg->SampsPerSymbol() * sizeof(int16_t)));
  }

  BroadcastControlData msg;

  msg.frame_id_ = 1501;

  for( size_t ue = 0; ue < cfg->UeAntNum(); ue++ )
  {
    msg.ue_map_[0] = ue;
  }

  cfg->GenBroadcastSlots(data_buffer, msg);

  complex_float* bcast_fft_buff = static_cast<complex_float*>(
      Agora_memory::PaddedAlignedAlloc(Agora_memory::Alignment_t::kAlign64,
                                       cfg->OfdmCaNum() * sizeof(float) * 2));

  for (size_t i = 0; i < cfg->Frame().NumDlControlSyms(); i++) {
    SimdConvertShortToFloat(reinterpret_cast<int16_t*>(data_buffer.at(i)),
                            reinterpret_cast<float*>(bcast_fft_buff),
                            cfg->OfdmCaNum() * 2);
    CommsLib::FFT(bcast_fft_buff, cfg->OfdmCaNum());
    CommsLib::FFTShift(bcast_fft_buff, cfg->OfdmCaNum());
    DataGenerator::GetNoisySymbol(bcast_fft_buff, cfg->OfdmCaNum(),
                                  cfg->NoiseLevel());
    CommsLib::FFTShift(bcast_fft_buff, cfg->OfdmCaNum());
    CommsLib::IFFT(bcast_fft_buff, cfg->OfdmCaNum(), false);
    CommsLib::Ifft2tx(bcast_fft_buff, data_buffer.at(i), cfg->OfdmCaNum(),
                      cfg->OfdmTxZeroPrefix(), cfg->CpLen(), 1.0);
  }
  BroadcastControlData decoded_msg =
      cfg->DecodeBroadcastSlots(reinterpret_cast<int16_t*>(data_buffer[0]));

  ASSERT_EQ(msg.frame_id_, decoded_msg.frame_id_);

  for( size_t ue = 0; ue < cfg->UeAntNum(); ue++ )
  {
    ASSERT_EQ(msg.ue_map_[ue], decoded_msg.ue_map_[ue]);
  }

  for (size_t i = 0; i < cfg->Frame().NumDlControlSyms(); i++) {
    FreeBuffer1d(&data_buffer.at(i));
  }
  FreeBuffer1d(&bcast_fft_buff);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
