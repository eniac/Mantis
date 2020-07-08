/* Copyright 2020-present University of Pennsylvania
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPILE_CONST_H
#define COMPILE_CONST_H

const char* const kOrigIngControlName = "originalIngress";
const char* const kSetmblIngControlName = "__ciSetMbls";
const char* const kSetargsIngControlName = "__ciSetArgs";
const char* const kSetmblEgrControlName = "__ceSetMbls";
const char* const kSetargsEgrControlName = "__ceSetArgs";
const char* const kOrigEgrControlName = "originalEgress";
const char* const kRegArgGateEgrControlName = "__ceRegArgGate";
const char* const kRegArgGateIngControlName = "__ciRegArgGate";
const char* const kP4rIngMetadataType = "__P4RIngMeta_t";
const char* const kP4rIngMetadataName = "__P4RIngMeta";
const char* const kP4rEgrMetadataType = "__P4REgrMeta_t";
const char* const kP4rEgrMetadataName = "__P4REgrMeta";
const char* const kP4rRegReplicasSuffix0 = "__P4Rreplicas0";
const char* const kP4rRegReplicasSuffix1 = "__P4Rreplicas1";
const char* const kP4rRegReplicasTablePrefix = "__tr__";
const char* const kP4rRegReplicasBlackboxPrefix = "__br__";
const char* const kP4rRegReplicasActionPrefix = "__ar__";
const char* const kP4rIngRegMetadataType = "__P4RIngRegMeta_t";
const char* const kP4rIngRegMetadataName = "__P4RIngRegMeta";
const char* const kP4rEgrRegMetadataType = "__P4REgrRegMeta_t";
const char* const kP4rEgrRegMetadataName = "__P4REgrRegMeta";
const char* const kP4rRegMetadataOutputSuffix = "__output";
const char* const kP4rRegMetadataIndexSuffix = "__index";
const char* const kP4rIndexSuffix = "__alt";
const char* const kP4rIngInitAction= "__aiSetVars";
const char* const kP4rEgrInitAction= "__aeSetVars";
const char* const kP4rIngArghdrType = "__packedIngArgs_t";
const char* const kP4rIngArghdrName = "__packedIngArgs";
const char* const kP4rEgrArghdrType = "__packedEgrArgs_t";
const char* const kP4rEgrArghdrName = "__packedEgrArgs";

const char* const kMantisNl = "_MANTIS_NL_";
const char* const kMatchSuffixStr = "match_spec_t";
const char* const kActionSuffixStr = "action_spec_t";
const char* const kErrorCheckStr = "if(__mantis__status_tmp!=0) {return false;}_MANTIS_NL_";

static int kHandlerOffset = 2;
static int kIngInitEntryHandlerIndex = 0;
static int kEgrInitEntryHandlerIndex = 1;
// Consistent with the agent malloc size of user_hdls
static int kNumUserHdls = 5000;

// %1%: reg width
// %2%: data plane reg name
// %3%: reg size
// %4%: prefix_str
// %5%: number of items to read
// %6%: reg arg name
const char * const kRegArgMirrorT_32 =
R"(
  uint%1%_t __mantis__values_%2%[4*%3%];
  __mantis__status_tmp = %4%register_range_read_%2%(sess_hdl, pipe_mgr_dev_tgt, 0, %5%, __mantis__reg_flags, &__mantis__num_actually_read, __mantis__values_%2%, &__mantis__value_count);
  if(__mantis__status_tmp!=0) {
    return false;
  }
  // Mirror %6%
  uint%1%_t %6%[%3%];
  for (__mantis__i=0; __mantis__i < %5%; __mantis__i++) {
    %6%[__mantis__i] = __mantis__values_%2%[1+__mantis__i*2];
  }
)";

// %1%: data plane reg name
// %2%: reg size
// %3%: prefix_str
// %4%: number of items to read
// %5%: reg arg name
const char * const kRegArgMirrorT_64 =
R"(
  %3%%1%_value_t __mantis__values_%1%[4*%2%];
  __mantis__status_tmp = %3%register_range_read_%1%(sess_hdl, pipe_mgr_dev_tgt, 0, %4%, __mantis__reg_flags, &__mantis__num_actually_read, __mantis__values_%1%, &__mantis__value_count);
  if(__mantis__status_tmp!=0) {
    return false;
  }
  // Mirror %5%
  %3%%1%_value_t %5%[%2%];
  for (__mantis__i=0; __mantis__i < %4%; __mantis__i++) {
    %5%[__mantis__i] = __mantis__values_%1%[1+__mantis__i*2];
  }
)";

// For 32b reg arg only
// %1%: reg arg name
// %2%: reg arg width
// %3%: reg arg size
// %4%: prefix str
// %5%: number of items to read
const char * const kRegArgIsoMirrorT =
R"(
  // Mirror %1%
  uint%2%_t %1%[%3%];
  static uint32_t %1%__tstamp__P4Rreplicas0[%3%];
  static uint32_t %1%__tstamp__P4Rreplicas1[%3%];
  if(__mantis__mv==0) {
    %4%%1%_value_t __mantis__values_%1%__P4Rreplicas0[4*%3%];
    __mantis__status_tmp = %4%register_range_read_%1%__P4Rreplicas0(sess_hdl, pipe_mgr_dev_tgt, 0, %5%, __mantis__reg_flags, &__mantis__num_actually_read, __mantis__values_%1%__P4Rreplicas0, &__mantis__value_count);
    if(__mantis__status_tmp!=0) {
      return false;
    }
    for (__mantis__i=0; __mantis__i < %5%; __mantis__i++) {
      if(__mantis__values_%1%__P4Rreplicas0[1+__mantis__i*2].hi > uint32_t %1%__tstamp__P4Rreplicas0[__mantis__i]) {
        %1%[__mantis__i] = __mantis__values_%1%__P4Rreplicas0[1+__mantis__i*2].lo;
        %1%__tstamp__P4Rreplicas0[__mantis__i] = __mantis__values_%1%__P4Rreplicas0[1+__mantis__i*2].hi;
      }
    }
  } else {
    %4%%1%_value_t __mantis__values_%1%__P4Rreplicas1[4*%3%];
    __mantis__status_tmp = %4%register_range_read_%1%__P4Rreplicas1(sess_hdl, pipe_mgr_dev_tgt, 0, %5%, __mantis__reg_flags, &__mantis__num_actually_read, __mantis__values_%1%__P4Rreplicas1, &__mantis__value_count);
    if(__mantis__status_tmp!=0) {
      return false;
    }
    for (__mantis__i=0; __mantis__i < %5%; __mantis__i++) {
      if(__mantis__values_%1%__P4Rreplicas1[1+__mantis__i*2].hi > uint32_t %1%__tstamp__P4Rreplicas1[__mantis__i]) {
        %1%[__mantis__i] = __mantis__values_%1%__P4Rreplicas1[1+__mantis__i*2].lo;
        %1%__tstamp__P4Rreplicas1[__mantis__i] = __mantis__values_%1%__P4Rreplicas1[1+__mantis__i*2].hi;
      }		
    }
  }
)";

// %1%: bin size
// %2%: bin index
// %3%: prefix_str
const char * const kIngFieldArgPollT = 
R"(
  uint%1%_t __mantis__values_riSetArgs_%2%[4];
  __mantis__status_tmp = %3%regiser_read___riSetArgs%2%(sess_hdl, pipe_mgr_dev_tgt, __mantis__mv, __mantis__reg_flags, __mantis__values_riSetArgs_%2%, &__mantis__value_count);
  if(__mantis__status_tmp!=0) {
    return false;
  }
)";

const char * const kEgrFieldArgPollT = 
R"(
  uint%1%_t __mantis__values_reSetArgs_%2%[4];
  __mantis__status_tmp = %3%regiser_read___reSetArgs%2%(sess_hdl, pipe_mgr_dev_tgt, __mantis__mv, __mantis__reg_flags, __mantis__values_reSetArgs_%2%, &__mantis__value_count);
  if(__mantis__status_tmp!=0) {
    return false;
  }
)";

const char * const kPrologueT = 
R"(
bool pd_prologue(uint32_t sess_hdl, dev_target_t pipe_mgr_dev_tgt, uint32_t* user_hdls, uint32_t* vv_shallow_hdls) {
  uint32_t __mantis__status_tmp;

%1%

%2%

%3%

  return true;
}
)";

const char * const kDialogueT = 
R"(
bool pd_dialogue(uint32_t sess_hdl, dev_target_t pipe_mgr_dev_tgt, uint32_t* user_hdls, uint32_t* vv_shallow_hdls) {
  uint32_t __mantis__status_tmp;

%1%
    
%2%

%3%

  return true;
}
)";

#endif