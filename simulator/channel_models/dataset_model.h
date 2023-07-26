/**
 * @file dataset_model.h
 * @brief Declaration file for the dataset channel model.  Can be imported with an HDF5 file
 */
#ifndef DATASET_MODEL_H_
#define DATASET_MODEL_H_

#include "armadillo"
#include "channel_model.h"
#include "config.h"

#if defined(ENABLE_HDF5)
#include "H5Cpp.h"
#endif

class DatasetModel : public ChannelModel {
 public:
  DatasetModel(size_t bs_ant_num, size_t ue_ant_num, size_t samples_per_sym,
               const std::string& dataset_path);

  void UpdateModel() final;
  void UpdateMatrixByIndex( int sc_index ) final;

 private:
  arma::cx_fmat GetMatricesByFrame(hsize_t frame);
  void InstantiateDataset(const std::string& dataset_path);
  
  int dataset_rank;

  #if defined(ENABLE_HDF5)
  H5::DataSet re_dataset;
  H5::DataSet im_dataset;
  H5::DataSpace dataspace;
  #endif
  
  hsize_t current_frame_num_;
  hsize_t dataset_dims[];
};

#endif  //DATASET_MODEL_H_