/**
 * @file dobroadcast.h
 * @brief Declaration file for the DoBroadcast class.
 */
#ifndef DOBROADCAST_H_
#define DOBROADCAST_H_

#include "common_typedef_sdk.h"
#include "config.h"
#include "doer.h"
#include "memory_manage.h"
#include "stats.h"

#include "mac_scheduler.h"

class DoBroadcast : public Doer {
 public:
  DoBroadcast(Config* in_config, int in_tid, char* in_dl_socket_buffer,
              Stats* in_stats_manager, MacScheduler* mac_sched);
  ~DoBroadcast() override;

  EventData Launch(size_t tag) override;

 protected:
  void GenerateBroadcastSymbols(size_t frame_id);

 private:
  char* dl_socket_buffer_;
  DurationStat* duration_stat_;
  MacScheduler* mac_sched_;

};

#endif  // DOBROADCAST_H_
