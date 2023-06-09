/**
 * @file data_generator.h
 * @brief Implementation file for the Data generator class to generate binary
 * files as inputs to Agora, sender and correctness tests
 */
#ifndef DATA_GENERATOR_H_
#define DATA_GENERATOR_H_

#include <string>

#include "common_typedef_sdk.h"
#include "comms-lib.h"
#include "config.h"
#include "modulation.h"
#include "phy_ldpc_decoder_5gnr.h"
#include "utils_ldpc.h"

/**
 * @brief Building blocks for generating end-to-end or unit test workloads for
 * Agora
 */
class DataGenerator {
 public:
  // The profile of the input information bits
  enum class Profile {
    kRandom,  // The input information bytes are chosen at random

    // The input informatioon bytes are {1, 2, 3, 1, 2, 3, ...} for UE 0,
    // {4, 5, 6, 4, 5, 6, ...} for UE 1, and so on
    kProfile123
  };

  explicit DataGenerator(Config* cfg, uint64_t seed = 0,
                         Profile profile = Profile::kRandom)
      : cfg_(cfg), seed_(seed), profile_(profile) {
    if (seed != 0) {
      fast_rand_.seed_ = seed;
    }
  }

  void DoDataGeneration(const std::string& directory);

  /**
   * @brief                        Generate random Mac payload bit
   * sequence
   *
   * @param  information           The generated input bit sequence
   * @param  ue_id                 ID of the UE that this codeblock belongs to
   */
  void GenMacData(MacPacketPacked* mac, size_t ue_id) {
    for (size_t i = 0; i < mac->PayloadLength(); i++) {
      if (profile_ == Profile::kRandom) {
        mac->DataPtr()[i] = static_cast<int8_t>(fast_rand_.NextU32());
      } else if (profile_ == Profile::kProfile123) {
        mac->DataPtr()[i] = 1 + (ue_id * 3) + (i % 3);
      }
    }
  }

  /**
   * @brief                        Generate one raw information bit sequence
   *
   * @param  information           The generated input bit sequence
   * @param  ue_id                 ID of the UE that this codeblock belongs to
   */
  void GenRawData(Direction dir, std::vector<int8_t>& information,
                  size_t ue_id) {
    const LDPCconfig& lc = cfg_->LdpcConfig(dir);
    information.resize(
        LdpcEncodingInputBufSize(lc.BaseGraph(), lc.ExpansionFactor()));

    for (size_t i = 0; i < lc.NumInputBytes(); i++) {
      if (profile_ == Profile::kRandom) {
        information.at(i) = static_cast<int8_t>(fast_rand_.NextU32());
      } else if (profile_ == Profile::kProfile123) {
        information.at(i) = 1 + (ue_id * 3) + (i % 3);
      }
    }
  }

  /**
   * @brief                        Generate the encoded bit sequence for one
   * code block for the active LDPC configuration from the input bit sequence
   *
   * @param  input_ptr             The input bit sequence to be encoded
   * @param  encoded_codeword      The generated encoded codeword bit sequence
   */
  void GenCodeblock(Direction dir, const int8_t* input_ptr,
                    std::vector<int8_t>& encoded_codeword) {
    const LDPCconfig& lc = cfg_->LdpcConfig(dir);
    std::vector<int8_t> parity;
    parity.resize(
        LdpcEncodingParityBufSize(lc.BaseGraph(), lc.ExpansionFactor()));

    encoded_codeword.resize(
        LdpcEncodingEncodedBufSize(lc.BaseGraph(), lc.ExpansionFactor()));

    LdpcEncodeHelper(lc.BaseGraph(), lc.ExpansionFactor(), lc.NumRows(),
                     &encoded_codeword.at(0), &parity.at(0), input_ptr);

    encoded_codeword.resize(lc.NumEncodedBytes());
  }

  /**
   * @brief Return the output of modulating the encoded codeword
   * @param encoded_codeword The encoded LDPC codeword bit sequence
   * @return An array of complex floats with OfdmDataNum() elements
   */
  std::vector<complex_float> GetModulation(const int8_t* encoded_codeword,
                                           Table<complex_float> mod_table,
                                           const size_t num_bits,
                                           const size_t mod_order_bits) {
    size_t num_subcarrier_symbols = num_bits / mod_order_bits;
    std::vector<complex_float> modulated_codeword(num_subcarrier_symbols);
    std::vector<uint8_t> mod_input(num_subcarrier_symbols);

    AdaptBitsForMod(reinterpret_cast<const uint8_t*>(&encoded_codeword[0]),
                    &mod_input[0], BitsToBytes(num_bits), mod_order_bits);

    for (size_t i = 0; i < num_subcarrier_symbols; i++) {
      modulated_codeword[i] = ModSingleUint8(mod_input[i], mod_table);
    }
    return modulated_codeword;
  }

  std::vector<complex_float> MapOFDMSymbol(
      const std::vector<complex_float>& modulated_codeword,
      complex_float* pilot_seq, SymbolType symbol_type) {
    std::vector<complex_float> ofdm_symbol(cfg_->OfdmDataNum());  // Zeroed
    for (size_t i = 0; i < modulated_codeword.size(); i++) {
      if (symbol_type == SymbolType::kUL) {
        ofdm_symbol.at(i) = modulated_codeword.at(i);
      } else if (symbol_type == SymbolType::kDL) {
        if (cfg_->IsDataSubcarrier(i) == true) {
          ofdm_symbol.at(i) = modulated_codeword.at(cfg_->GetOFDMDataIndex(i));
        } else {
          ofdm_symbol.at(i) = pilot_seq[i];
        }
      } else if (symbol_type == SymbolType::kControl) {
        if (cfg_->IsControlDataSubcarrier(i) == true) {
          ofdm_symbol.at(i) = modulated_codeword.at(cfg_->GetOFDMDataIndex(i));
        } else {
          ofdm_symbol.at(i) = pilot_seq[i];
        }
      }
    }
    return ofdm_symbol;
  }

  void GetNoisySymbol(const std::vector<complex_float>& modulated_symbol,
                      std::vector<complex_float>& noisy_symbol,
                      float noise_level) {
    std::default_random_engine generator(seed_);
    std::normal_distribution<double> distribution(0.0, 1.0);
    for (size_t j = 0; j < modulated_symbol.size(); j++) {
      complex_float noise = {
          static_cast<float>(distribution(generator)) * noise_level,
          static_cast<float>(distribution(generator)) * noise_level};
      noisy_symbol.at(j).re = modulated_symbol.at(j).re + noise.re;
      noisy_symbol.at(j).im = modulated_symbol.at(j).im + noise.im;
    }
  }

  void GetNoisySymbol(const complex_float* modulated_symbol,
                      complex_float* noisy_symbol, size_t length,
                      float noise_level) {
    std::default_random_engine generator(seed_);
    std::normal_distribution<double> distribution(0.0, 1.0);
    for (size_t j = 0; j < length; j++) {
      complex_float noise = {
          static_cast<float>(distribution(generator)) * noise_level,
          static_cast<float>(distribution(generator)) * noise_level};
      noisy_symbol[j].re = modulated_symbol[j].re + noise.re;
      noisy_symbol[j].im = modulated_symbol[j].im + noise.im;
    }
  }

  /**
   * @param modulated_codeword The modulated codeword with OfdmDataNum()
   * elements
   * @brief An array with OfdmDataNum() elements with the OfdmDataNum()
   * modulated elements binned at the center
   */
  std::vector<complex_float> BinForIfft(
      const std::vector<complex_float>& modulated_codeword) const {
    std::vector<complex_float> pre_ifft_symbol(cfg_->OfdmCaNum());  // Zeroed
    std::memcpy(&pre_ifft_symbol[cfg_->OfdmDataStart()], &modulated_codeword[0],
                cfg_->OfdmDataNum() * sizeof(complex_float));

    return pre_ifft_symbol;
  }

  /// Return the frequency-domain pilot symbol with OfdmCaNum complex floats
  std::vector<complex_float> GetCommonPilotFreqDomain() const {
    const std::vector<std::complex<float>> zc_seq = Utils::DoubleToCfloat(
        CommsLib::GetSequence(cfg_->OfdmDataNum(), CommsLib::kLteZadoffChu));

    const std::vector<std::complex<float>> zc_common_pilot =
        CommsLib::SeqCyclicShift(zc_seq, M_PI / 4.0);  // Used in LTE SRS

    std::vector<complex_float> ret(cfg_->OfdmCaNum());  // Zeroed
    for (size_t i = 0; i < cfg_->OfdmDataNum(); i++) {
      ret[i + cfg_->OfdmDataStart()] = {zc_common_pilot[i].real(),
                                        zc_common_pilot[i].imag()};
    }

    return ret;
  }

  /// Return the user-spepcific frequency-domain pilot symbol with OfdmCaNum complex floats
  Table<complex_float> GetUeSpecificPilotFreqDomain() const {
    Table<complex_float> ue_specific_pilot;
    const std::vector<std::complex<float>> zc_seq =
        Utils::DoubleToCfloat(CommsLib::GetSequence(this->cfg_->OfdmDataNum(),
                                                    CommsLib::kLteZadoffChu));
    const std::vector<std::complex<float>> zc_common_pilot =
        CommsLib::SeqCyclicShift(zc_seq, M_PI / 4.0);  // Used in LTE SRS
    ue_specific_pilot.Malloc(this->cfg_->UeAntNum(), this->cfg_->OfdmDataNum(),
                             Agora_memory::Alignment_t::kAlign64);
    for (size_t i = 0; i < this->cfg_->UeAntNum(); i++) {
      auto zc_ue_pilot_i =
          CommsLib::SeqCyclicShift(zc_seq, i * M_PI / 6.0);  // LTE DMRS
      for (size_t j = 0; j < this->cfg_->OfdmDataNum(); j++) {
        ue_specific_pilot[i][j] = {zc_ue_pilot_i[j].real(),
                                   zc_ue_pilot_i[j].imag()};
      }
    }
    return ue_specific_pilot;
  }

  void GetDecodedData(Table<int8_t>& demoded_data,
                      Table<uint8_t>& decoded_codewords,
                      const LDPCconfig& ldpc_config, size_t num_codeblocks) {
    struct bblib_ldpc_decoder_5gnr_request ldpc_decoder_5gnr_request {};
    struct bblib_ldpc_decoder_5gnr_response ldpc_decoder_5gnr_response {};

    // Decoder setup
    ldpc_decoder_5gnr_request.numChannelLlrs = ldpc_config.NumCbCodewLen();
    ldpc_decoder_5gnr_request.numFillerBits = 0;
    ldpc_decoder_5gnr_request.maxIterations = ldpc_config.MaxDecoderIter();
    ldpc_decoder_5gnr_request.enableEarlyTermination =
        ldpc_config.EarlyTermination();
    ldpc_decoder_5gnr_request.Zc = ldpc_config.ExpansionFactor();
    ldpc_decoder_5gnr_request.baseGraph = ldpc_config.BaseGraph();
    ldpc_decoder_5gnr_request.nRows = ldpc_config.NumRows();
    ldpc_decoder_5gnr_response.numMsgBits = ldpc_config.NumCbLen();
    auto* resp_var_nodes = static_cast<int16_t*>(
        Agora_memory::PaddedAlignedAlloc(Agora_memory::Alignment_t::kAlign64,
                                         1024 * 1024 * sizeof(int16_t)));
    ldpc_decoder_5gnr_response.varNodes = resp_var_nodes;

    for (size_t i = 0; i < num_codeblocks; i++) {
      ldpc_decoder_5gnr_request.varNodes = demoded_data[i];
      ldpc_decoder_5gnr_response.compactedMessageBytes = decoded_codewords[i];
      bblib_ldpc_decoder_5gnr(&ldpc_decoder_5gnr_request,
                              &ldpc_decoder_5gnr_response);
    }
    std::free(resp_var_nodes);
  }

 private:
  FastRand fast_rand_;  // A fast random number generator
  Config* cfg_;         // The global Agora config
  uint64_t seed_;
  const Profile profile_;  // The pattern of the input byte sequence
};

#endif  // DATA_GENERATOR_H_
