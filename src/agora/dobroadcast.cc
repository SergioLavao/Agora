/**
 * @file dobroadcast.cc
 * @brief Implementation file for the DoBroadcast class.
 */
#include "dobroadcast.h"

#include "comms-lib.h"
#include "concurrent_queue_wrapper.h"
#include "datatype_conversion.h"
#include "logger.h"

static constexpr bool kPrintSocketOutput = false;

DoBroadcast::DoBroadcast(Config* in_config, int in_tid,char* in_dl_socket_buffer, Stats* in_stats_manager, MacScheduler* mac_sched)
    : Doer(in_config, in_tid), dl_socket_buffer_(in_dl_socket_buffer), mac_sched_(mac_sched) {
  duration_stat_ =
      in_stats_manager->GetDurationStat(DoerType::kBroadcast, in_tid);
}

DoBroadcast::~DoBroadcast() = default;

void DoBroadcast::GenerateBroadcastSymbols(size_t frame_id) {
  const size_t num_control_syms = cfg_->Frame().NumDlControlSyms();
  RtAssert(num_control_syms > 0,
           "DoBroadcast: No downlink control symbols are scheduled!");
  std::vector<std::complex<int16_t>*> bcast_iq_samps(num_control_syms);
//  std::vector<size_t> ctrl_data(num_control_syms);
  for (size_t symbol_idx_dl = 0; symbol_idx_dl < num_control_syms;
       symbol_idx_dl++) {
    size_t symbol_id = cfg_->Frame().GetDLControlSymbol(symbol_idx_dl);
    if (kDebugPrintInTask) {
      std::printf(
          "In doBroadcast thread %d: frame: %zu, symbol: %zu, antenna: %zu\n",
          tid_, frame_id, symbol_id, cfg_->BeaconAnt());
    }

    const size_t total_symbol_idx =
        cfg_->GetTotalSymbolIdxDl(frame_id, symbol_id);
    ///\todo change to BroadcastAnt()
    const size_t offset =
        (total_symbol_idx * cfg_->BsAntNum()) + cfg_->BeaconAnt();

    auto* pkt = reinterpret_cast<Packet*>(
        &dl_socket_buffer_[offset * cfg_->DlPacketLength()]);

    bcast_iq_samps.at(symbol_idx_dl) =
        reinterpret_cast<std::complex<int16_t>*>(pkt->data_);

    ///\todo: later ctrl data might include other info (MCS, UE Schedule)
    //SergioL: Can send the MSC Index and the UE Schedule
    mac_sched_->UpdateSchedule();
  }

  //Set Schedule desision //Clean up
  //SergioL
  std::vector<short> mcs_schdl_ = { 4, 8, 1, 9 };
  std::vector<short> ctrl_data(4);
  std::printf("TX Broadcast UE_Scheduling_MCS = [%d %d] \n",mcs_schdl_[0],mcs_schdl_[1]);
  ctrl_data.at(0) = mcs_schdl_[0];
  ctrl_data.at(1) = mcs_schdl_[1];
  ctrl_data.at(2) = mcs_schdl_[2];
  ctrl_data.at(3) = mcs_schdl_[3];

  cfg_->GenBroadcastSlots(bcast_iq_samps, ctrl_data);

  if (kPrintSocketOutput) {
    for (size_t symbol_idx_dl = 0; symbol_idx_dl < num_control_syms;
         symbol_idx_dl++) {
      std::stringstream ss;
      ss << "socket_tx_data" << cfg_->BeaconAnt() << "_" << symbol_idx_dl
         << "=[";
      for (size_t i = 0; i < cfg_->SampsPerSymbol(); i++) {
        ss << bcast_iq_samps.at(symbol_idx_dl)[i * 2] << "+1j*"
           << bcast_iq_samps.at(symbol_idx_dl)[i * 2 + 1] << " ";
      }
      ss << "];" << std::endl;
      std::cout << ss.str();
    }
  }
}

EventData DoBroadcast::Launch(size_t tag) {
  size_t start_tsc = GetTime::WorkerRdtsc();

  const size_t frame_id = gen_tag_t(tag).frame_id_;

  GenerateBroadcastSymbols(frame_id);

  duration_stat_->task_count_++;
  duration_stat_->task_duration_[0u] += GetTime::WorkerRdtsc() - start_tsc;
  return EventData(EventType::kBroadcast, tag);
}
