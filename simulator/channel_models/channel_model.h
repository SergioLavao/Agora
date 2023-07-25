/**
 * @file channel_model.h
 * @brief Declaration file for the channel model API
 */
#ifndef CHANNEL_MODEL_H_
#define CHANNEL_MODEL_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "armadillo"
#include "config.h"

class ChannelModel {
 public:
  enum FadingType { kFlat, kSelective };

  ChannelModel(size_t bs_ant_num, size_t ue_ant_num, size_t samples_per_sym,
               FadingType fading_type);
  virtual ~ChannelModel() = default;

  //Function called every frame
  virtual void UpdateModel() = 0;
  virtual void UpdateMatrixByIndex( int index ) = 0;
  
  /*
  * Returns H Matrix, if selective fading apply h_slice_index for each subcarrier
  * @param index = -1 if flat fading, Subcarrier or OFDM sample index if selective fading
  */
  virtual arma::cx_fmat GetMatrix(bool is_downlink);

  inline FadingType GetFadingType() const { return fading_type_; }
  //Factory function
  static std::unique_ptr<ChannelModel> CreateChannelModel(
      const Config* const config, std::string& channel_type,
      std::string& dataset_path);

 protected:
  const size_t bss_num_;
  const size_t ues_num_;
  const size_t n_samps_;

  // Vector MUST be of size NSubcarriers or OFDMSamples or 1 if flat_fading
  // Not sure this makes sense for all models?
  arma::cx_fmat h_;

 private:
  FadingType fading_type_;
};

#endif  //CHANNEL_MODEL_H_
