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

DoBroadcast::DoBroadcast(Config* in_config, int in_tid,
                         char* in_dl_socket_buffer, MacScheduler* mac_sched, Stats* in_stats_manager)
    : Doer(in_config, in_tid), dl_socket_buffer_(in_dl_socket_buffer), mac_sched_( mac_sched ) {
  duration_stat_ =
      in_stats_manager->GetDurationStat(DoerType::kBroadcast, in_tid);

}

DoBroadcast::~DoBroadcast() = default;

void DoBroadcast::GenerateBroadcastSymbols(size_t frame_id) {
  const size_t num_control_syms = cfg_->Frame().NumDlControlSyms();
  RtAssert(num_control_syms > 0,
           "DoBroadcast: No downlink control symbols are scheduled!");
  std::vector<std::complex<int16_t>*> bcast_iq_samps(num_control_syms);
  
  BroadcastControlData ctrl_data;

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
    ///\todo change to BroadcastAnt()....?? it doesnt exists

    const size_t offset =
        (total_symbol_idx * cfg_->BsAntNum()) + cfg_->BeaconAnt();

    auto* pkt = reinterpret_cast<Packet*>(
        &dl_socket_buffer_[offset * cfg_->DlPacketLength()]);
    bcast_iq_samps.at(symbol_idx_dl) =
        reinterpret_cast<std::complex<int16_t>*>(pkt->data_);
    //SergioL: generate more control info
    ctrl_data.frame_id_ = frame_id + (kUseArgos ? TX_FRAME_DELTA : 0);
  }

  arma::uvec sched_ue_list =  mac_sched_->ScheduledUeList( frame_id, 0u );
  
  std::memset(ctrl_data.ue_map_, 0, sizeof(ctrl_data.ue_map_));
  for( size_t ue_idx = 0; ue_idx < cfg_->SpatialStreamsNum(); ue_idx++ ){
    ctrl_data.ue_map_[ sched_ue_list[ue_idx] ] = 1;//1 Scheduled, 0 Not Scheduled 
  }

  std::stringstream ss;
  ss << "Broadcast TX Scheduled List [ " ; //1 Scheduled, 0 Not Scheduled 
  for( size_t ue = 0; ue < cfg_->UeAntNum(); ue++ ){
    ss << ctrl_data.ue_map_[ue] << " ";
  }
  ss << "];" << std::endl;
  AGORA_LOG_INFO(ss.str());

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