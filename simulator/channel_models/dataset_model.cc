/**
 * @file dataset_model.cc
 * @brief Implementation file for the dataset channel model.  Can be imported with an HDF5 file
 */

#include "dataset_model.h"

#include <iostream>

#include "H5Cpp.h"
#include "logger.h"

bool kPrintDatasetOutput = true;

#include "gettime.h"

DatasetModel::DatasetModel(size_t bs_ant_num, size_t ue_ant_num,
                           size_t samples_per_sym,
                           const std::string& dataset_path)
    : ChannelModel(bs_ant_num, ue_ant_num, samples_per_sym,
                   ChannelModel::kSelective) {

  //Create an empty H UEs x BSs Matrix 
  h_ = arma::cx_fmat(ues_num_,bss_num_);
  InstantiateDataset(dataset_path);
}

void DatasetModel::InstantiateDataset(const std::string& dataset_path) {
  hsize_t frames_num = 0;
  hsize_t scs_num = 0;
  hsize_t bss_num = 0;
  hsize_t ues_num = 0;

  current_frame_num_ = 0;

  try {

    H5::H5File file(dataset_path, H5F_ACC_RDONLY);
    re_dataset = file.openDataSet("H_r");
    im_dataset = file.openDataSet("H_i");

    dataspace = re_dataset.getSpace();
    dataset_rank = dataspace.getSimpleExtentNdims();
    dataspace.getSimpleExtentDims(dataset_dims, NULL);

    frames_num = dataset_dims[0];
    scs_num = dataset_dims[1];
    bss_num = dataset_dims[2];
    ues_num = dataset_dims[3];

  } catch (H5::FileIException& error) {
    AGORA_LOG_ERROR("Read Dataset %s, FileIException - %s\n",
                    dataset_path.c_str(), error.getCDetailMsg());
    H5::FileIException::printErrorStack();
    throw error;

  } catch (H5::DataSetIException& error) {
    AGORA_LOG_ERROR("Read Dataset %s, DataSetIException - %s\n",
                    dataset_path.c_str(), error.getCDetailMsg());

    H5::DataSetIException::printErrorStack();
    throw error;

  } catch (H5::DataSpaceIException& error) {
    AGORA_LOG_ERROR("CreateDataset %s, DataSpaceIException - %s\n",
                    dataset_path.c_str(), error.getCDetailMsg());

    H5::DataSpaceIException::printErrorStack();
    throw error;
  }

  AGORA_LOG_INFO(
      "Dataset Succesfully loaded\n"
      "Path: %s \n"
      "Frames: %lld \n"
      "Subcarriers: %lld \n"
      "BSs: %lld \n"
      "UEs: %lld \n",
      dataset_path.c_str(), frames_num, scs_num, bss_num, ues_num);
}

void DatasetModel::UpdateMatrixByIndex( int sc_index )
{

  if (current_frame_num_ >= dataset_dims[0]) {
    AGORA_LOG_ERROR("Invalid frame index selection!\n");
  }

  if( sc_index >= dataset_dims[1] ) {
    AGORA_LOG_ERROR("Invalid subcarrier index selection!\n");
  }

  hsize_t start[dataset_rank] = {current_frame_num_, sc_index, 0, 0};
  hsize_t count[dataset_rank] = {1, 1, bss_num_, ues_num_};
  dataspace.selectHyperslab(H5S_SELECT_SET, count, start);

  // Create a buffer to hold the data for the selected slice
  const size_t data_read_size = bss_num_ * ues_num_;
  std::vector<float> re_data_buffer(data_read_size);
  std::vector<float> im_data_buffer(data_read_size);

  H5::DataSpace memspace(dataset_rank, count);

  hsize_t start_mem[dataset_rank] = {0, 0, 0, 0};
  hsize_t count_mem[dataset_rank] = {1, 1, bss_num_, ues_num_};
  memspace.selectHyperslab(H5S_SELECT_SET, count_mem, start_mem);

  re_dataset.read(re_data_buffer.data(), H5::PredType::NATIVE_FLOAT, memspace, dataspace);
  im_dataset.read(im_data_buffer.data(), H5::PredType::NATIVE_FLOAT, memspace, dataspace);

  hsize_t data_index = 0;
  for (hsize_t bs = 0; bs < bss_num_; bs++) {
    for (hsize_t ue = 0; ue < ues_num_; ue++) {
      h_(ue, bs) = arma::cx_float( re_data_buffer.at(data_index), im_data_buffer.at(data_index) );
      data_index++;
      }
  }

}

void DatasetModel::UpdateModel() {
  current_frame_num_++;
}