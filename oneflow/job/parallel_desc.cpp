#include "job/parallel_desc.h"
#include <algorithm>

namespace oneflow {

ParallelDesc::ParallelDesc(const ParallelConf& user_conf) {
  policy_ = user_conf.policy();
  device_type_ = JobDesc::Singleton().resource().device_type();
  for (int64_t i = 0; i < user_conf.device_set().device_name_size(); ++i){
    const std::string& device_name = user_conf.device_set().device_name(i);
    int64_t delimiter_pos = device_name.rfind(":");
    CHECK_NE(delimiter_pos, std::string::npos);
    std::string machine_name = device_name.substr(0, delimiter_pos);
    std::string device_id_str = device_name.substr(delimiter_pos + 1);
    uint64_t machine_id =
        IDMgr::Singleton().MachineID4MachineName(machine_name);
    sorted_machine_ids_.push_back(machine_id);
    // if the device_name format is "machine_xxx:0-3", add device_id {0,1,2,3}
    int64_t to_symbol_pos = device_id_str.rfind("-");
    if (device_id_str == "disk") {
      machine_id2sorted_device_phy_ids_[machine_id] = {};
      device_type_ = DeviceType::kCPU;
    } else if (to_symbol_pos == std::string::npos) {
      uint64_t device_id = StoullOrDie(device_id_str);
      machine_id2sorted_device_phy_ids_[machine_id].push_back(device_id);	
    } else {
      uint64_t begin_device_id = 
        StoullOrDie(device_id_str.substr(0, to_symbol_pos));
      uint64_t end_device_id =
        StoullOrDie(device_id_str.substr(to_symbol_pos + 1));
      CHECK_LT(begin_device_id, end_device_id);
      for (uint64_t i = begin_device_id; i <= end_device_id; ++i) {
        machine_id2sorted_device_phy_ids_[machine_id].push_back(i);
      }
    }
  }
  SortAndRemoveDuplication(&sorted_machine_ids_);
  for (auto&pair : machine_id2sorted_device_phy_ids_) {
    SortAndRemoveDuplication(&(pair.second));
  }
}

std::string ParallelDesc::VisualStr() const {
  std::stringstream ss;
  ss << "{policy:";
  if (policy_ == kDataParallel) {
    ss << "DataParallel";
  } else {
    ss << "ModelParallel";
  }
  ss << "}{device_type:";
  if (device_type_ == kGPU) {
    ss << "GPU";
  } else {
    ss << "CPU";
  }
  ss << "}{machine_id2sorted_device_phy_ids:";
  for (uint64_t machine_id : sorted_machine_ids_) {
    ss << "{" << machine_id << ":[";
    for (uint64_t device_phy_id : machine_id2sorted_device_phy_ids_.at(machine_id)) {
      ss << device_phy_id << ",";
    }
    ss << "]}";
  }
  ss << "}";
  return ss.str();
}

uint64_t ParallelDesc::parallel_num() const {
  uint64_t parallel_num = 0;
  for (auto const& pair : machine_id2sorted_device_phy_ids_) {
    parallel_num += pair.second.size();
  }
  return parallel_num;
}

} // namespace oneflow
