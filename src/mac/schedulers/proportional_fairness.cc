/**
 * @file proportional_fairness.cc
 * @brief Implementation file for the Proportional Fairness scheduling algorithm 
 */
#include "proportional_fairness.h"
#include "logger.h"

static constexpr bool kPrintCombination = true;
static constexpr size_t kMaxProcessedFrames = 1000;

void ProportionalFairness::Combination( int k, int offset = 0) {
  if (k == 0) {
    combination_vector.push_back( combination );
    return;
  }
  for (size_t i = offset; i <= ues_vector.size() - k; ++i) {
    combination.push_back(ues_vector[i]);
    Combination(k-1, i+1);
    combination.pop_back();
  }
}

ProportionalFairness::ProportionalFairness( const size_t spatial_streams, const size_t bss_num, const size_t ues_num , size_t ofdm_data_num_ ) : 
    spatial_streams_(spatial_streams),
    bss_num_(bss_num),
    ues_num_(ues_num),
    //selected_action_(0),
    lamda_(0.5F),
    last_SE_(arma::zeros(ues_num))
{
    selected_action_ = 0;
    std::printf("============== Proportional Fairness Algorithm ============== \n\n");

    //Define UE Index vector
    for( size_t ue_idx = 0; ue_idx < ues_num_; ue_idx++){
        ues_vector.push_back(ue_idx);
        ues_flags_.push_back(false);
        pf_ues_history.push_back(0.01F);
    }

    //Possible actions
    Combination(spatial_streams_);
    actions_num_ = combination_vector.size();
    
    schedule_buffer_.Calloc(actions_num_, ues_num_ * ofdm_data_num_,
                            Agora_memory::Alignment_t::kAlign64);
    schedule_buffer_index_.Calloc(actions_num_, spatial_streams_ * ofdm_data_num_,
                            Agora_memory::Alignment_t::kAlign64);

    //Schedule Buffer Process
    for( size_t gp = 0; gp < actions_num_; gp++ )
    {
        for (size_t sc = 0; sc < ofdm_data_num_; sc++) 
        {
            std::vector<size_t> UEs_idx = combination_vector[gp];

            for( size_t ue_idx = 0; ue_idx < spatial_streams_; ue_idx++ )
            {
                schedule_buffer_[gp][UEs_idx[ue_idx] + ues_num_ * sc] = 1;
            }

            for( size_t ue = gp; ue < gp + spatial_streams_; ue++ )
            {
                schedule_buffer_index_[gp][(ue - gp) + spatial_streams_ * sc] = UEs_idx[ue - gp];
            }
            
        }

    }

   for (size_t row = 0; row < actions_num_; row++) {
        
        std::cout <<  "======================================= \n";

        std::printf("schedule_PF_buffer_index_ %ld \n\n", row);
        // Iterate through the columns
        for (size_t col = 0; col < spatial_streams_ * ofdm_data_num_; col++) {
            std::cout << schedule_buffer_index_[row][col] << " ";
        }
        std::cout <<  "\n\n";

        std::printf("schedule_PF_buffer_ %ld \n\n", row);
        // Iterate through the columns
        for (size_t col = 0; col < spatial_streams_ * ofdm_data_num_; col++) {
            std::cout << schedule_buffer_[row][col] << " ";
        }
        std::cout <<  "\n\n";

    }

    std::printf("PF Scheduling Groups \n");
    for( size_t gp = 0; gp < actions_num_; gp++)
    {
        std::printf("Action %ld [%d %d %d %d]\n", gp,
        schedule_buffer_[gp][0],
        schedule_buffer_[gp][1],
        schedule_buffer_[gp][2],
        schedule_buffer_[gp][3]);
    }

    std::printf("\n\n============== Proportional Fairness Algorithm FINISHED ============== \n\n");

}

size_t ProportionalFairness::UpdateScheduler(size_t frame )
{

    std::vector<float> csi_ = { 0.68226f,0.678098f,0.671843f,0.68403f };

    if( false )//( current_frame != frame )
    {
        current_frame = frame;
        Schedule( frame , csi_);
        UpdatePF( frame , csi_);

        //std::printf("Scheduled PF Frame %ld [%ld] \n", frame, selected_action_);

        /*
        std::vector<size_t> option = combination_vector[selected_action_];
        std::printf("Scheduled PF Frame %ld [%ld] with [ ", frame, selected_action_);

        std::stringstream s_;

        for( int i = 0; i < 4; i++)
        {
            std::string a = ( ues_flags_[i] ) ? "1 " : "0 ";
            s_ << a;
        }

        std::cout << s_.str() << "] \n ";
        */

    }

    selected_action_ = ( frame % 2 == 0 )? 1: 4;
    //return 1;
    //return 4;

    return selected_action_;

}

void ProportionalFairness::Schedule( size_t frame , std::vector<float> csi_ )
{
    arma::vec pf_;
    pf_.zeros(actions_num_);

    arma::fmat ues_capacity_;
    ues_capacity_.zeros( actions_num_, spatial_streams_ ); 

    arma::vec selected_ues_idx;

    //From all actions, select the maximum capacity 
    float max_pf = 0;
    
    for( size_t action = 0; action < actions_num_; action++ )
    {
        
        std::vector<size_t> selected_ues = combination_vector[action];

        if( frame > 0 )
        {
        
            for( size_t i = 0; i < selected_ues.size(); i++)
            {

                float tp_history = 0;
                size_t ue_idx = selected_ues[i];

                if( ues_flags_[ ue_idx ] )
                {

                    tp_history = lamda_*pf_ues_history[ue_idx]/frame + (1-lamda_)*last_SE_[ue_idx];

                }
                else
                {
                    
                    tp_history = lamda_*pf_ues_history[ue_idx]/frame;

                }

                //std::printf("tp_history [%ld] = %f \n", action, tp_history );

                pf_[action] += csi_[ue_idx] / tp_history;

                if( pf_[action] >= max_pf )
                {
                    max_pf = pf_[action];
                    selected_action_ = action;
                }

            }
        
        } 
    }
}

void ProportionalFairness::UpdatePF( size_t frame, std::vector<float> csi_ )
{
    
    for( size_t i = 0; i < combination_vector[selected_action_].size(); i++)
    {
        size_t idx_ = combination_vector[selected_action_][i];
        pf_ues_history[idx_] += csi_[idx_];
    }

    for( size_t ue = 0; ue < ues_num_; ue++)
    {
        if(std::find(combination_vector[selected_action_].begin(), combination_vector[selected_action_].end(), ue) != combination_vector[selected_action_].end()) {
            ues_flags_[ue] = true;
            last_SE_[ue] = csi_[ue];
        } else {
            ues_flags_[ue] = false;
            last_SE_[ue] = 0.0F;
        }
    }
}
