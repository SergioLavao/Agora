/**
 * @file radio_lib_uhd.h
 * @brief Declaration file for the BsRadioSet class.
 */
#ifndef RADIO_LIB_UHD_H_
#define RADIO_LIB_UHD_H_

#include <complex>
#include <cstdlib>
#include <memory>
#include <vector>

#include "SoapySDR/Device.hpp"
#include "armadillo"
#include "config.h"
#include "memory_manage.h"
#include "radio.h"
#include "uhd/usrp/multi_usrp.hpp"

class RadioUHDConfig {
 public:
  RadioUHDConfig(Config* cfg, Radio::RadioType radio_type);
  ~RadioUHDConfig();

  bool RadioStart();
  void RadioStop();
  void ReadSensors();
  int RadioTx(size_t radio_id, const void* const* buffs, Radio::TxFlags flags,
              long long& tx_time);
  int RadioTx(size_t radio_id,
              const std::vector<std::vector<std::complex<int16_t>>>& tx_data,
              Radio::TxFlags flags, long long& tx_time_ns);

  int RadioRx(size_t radio_id,
              std::vector<std::vector<std::complex<int16_t>>>& rx_data,
              size_t rx_size, Radio::RxFlags& out_flags, long long& rx_time_ns);

  int RadioRx(size_t radio_id,
              std::vector<std::vector<std::complex<int16_t>>*>& rx_buffs,
              size_t rx_size, Radio::RxFlags& out_flags, long long& rx_time_ns);

  int RadioRx(size_t radio_id, std::vector<void*>& rx_locs, size_t rx_size,
              Radio::RxFlags& out_flags, long long& rx_time_ns);

  bool DoCalib() const { return calib_; }
  void Go();
  arma::cx_float* GetCalibUl() { return init_calib_ul_processed_; }
  arma::cx_float* GetCalibDl() { return init_calib_dl_processed_; }

  // Thread functions
  void InitBsRadio(size_t radio_id);
  void ConfigureBsRadio(size_t radio_id);

 private:
  long long SyncArrayTime();

  void CalibrateSampleOffset();
  /* Transmit from each array antenna to ref antenna,
   * return the received signal vector at the ref antenna
  */
  auto TxArrayToRef(const std::vector<std::complex<int16_t>>& tx_vec);
  /* Transmit from ref antenna to the rest of the array
  * return the received signal vector at the array
  */
  auto TxRefToArray(const std::vector<std::complex<int16_t>>& tx_vec);
  bool FindTimeOffset(
      const std::vector<std::vector<std::complex<int16_t>>>& rx_mat,
      std::vector<int>& offset);
  bool InitialCalib();
  void AdjustDelays(std::vector<int> offset);
  static void DciqMinimize(Radio* target_dev, Radio* ref_dev, int direction,
                           size_t channel, double rx_center_tone,
                           double tx_center_tone);
  static void SetIqBalance(Radio* dev, int direction, size_t channel, int gcorr,
                           int iqcorr);

  // TODO: should not be a vector, instead should be a single object; Modify when testing
  static void AdjustCalibrationGains(std::vector<Radio*>& rx_devs,
                                     Radio* tx_dev, size_t channel,
                                     double fft_bin, bool plot = false);
  void DciqCalibrationProc(size_t channel);
  Config* cfg_;
  uhd::usrp::multi_usrp::sptr hubs_;
  std::unique_ptr<Radio> radios_;
  arma::cx_float* init_calib_ul_processed_;
  arma::cx_float* init_calib_dl_processed_;
  Table<arma::cx_float> init_calib_ul_;
  Table<arma::cx_float> init_calib_dl_;
  size_t radio_num_;
  size_t antenna_num_;
  bool calib_;
  size_t calib_meas_num_;

  std::atomic<size_t> num_radios_initialized_;
  std::atomic<size_t> num_radios_configured_;
};
#endif  // RADIO_LIB_UHD_H_
