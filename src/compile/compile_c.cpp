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

#include <unordered_map>
#include <vector>
#include <regex>
#include <boost/format.hpp>
#include <fstream>

#include "../../include/find_nodes.h"
#include "../../include/helper.h"

#include "compile_const.h"
#include "compile_p4.h"

using namespace std;

static int num_max_alts = 1;

// Process include/define macros in reaction
void extractReactionMacro(std::vector<AstNode*> nodeArray, ostringstream& oss_preprocessor, string out_fn_base) {
    string cinclude_str = "";
    string cdefine_str = "";
	for (auto node : nodeArray) {
        if(typeContains(node, "IncludeNode")) {
            IncludeNode* includenode = dynamic_cast<IncludeNode*>(node);
            if(includenode->macrotype_==IncludeNode::C) {
                if(includenode->toString().find("include")!=string::npos) {
                    cinclude_str += includenode->toString();
                    cinclude_str += "\n";
                } else if (includenode->toString().find("define")!=string::npos) {
                    cdefine_str += includenode->toString();
                    cdefine_str += "\n";
                }
            }
        }
    }

    // Keep preprocessor other than include
    oss_preprocessor << cdefine_str;
    // Include preprocessor goes to seperate tmp file
    std::ofstream os;
    os.open(out_fn_base+"_mantis.include");
    os << cinclude_str << endl;
    os.close();
}

UnanchoredNode* generatePrologueNode(std::vector<AstNode*> nodeArray, ostringstream& oss_mbl_init, ostringstream& oss_init_end) {

    P4RInitBlockNode * init_node = findInitBlock(nodeArray);

    string * prologue_str = NULL;
    if (init_node != 0) {
        prologue_str = new string(
            str(boost::format(kPrologueT) % oss_mbl_init.str() % init_node -> body_ -> toString() % oss_init_end.str()));
    } else {
        // if no init_block
        prologue_str = new string(
            str(boost::format(kPrologueT) % oss_mbl_init.str() % "" % oss_init_end.str()));
    }

    UnanchoredNode* prologue_cnode = new UnanchoredNode(prologue_str, new string ("prologue"), new string("pd_prologue"));
    return prologue_cnode;
}

UnanchoredNode* generateDialogueNode(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_mirror, ostringstream& oss_reaction_update) {

	// Assume single global reaction and initialization node (if any)
	P4RReactionNode * react_node = findReaction(nodeArray);
    string * dialogue_str = NULL;
    string p4r_reaction_user_str = "";
    if (react_node != 0) {
        p4r_reaction_user_str = react_node->body_->toString();
    }

    // Translate mantis style variable assignment to macros to be further processed by C preprocessor
    // regrex matching ${var} = <alter>;
    std::regex e_val ("\\$\\{([^ ]*)\\}\\s*[=]\\s*([^\\; ]*)\\s*\\;");
    p4r_reaction_user_str = std::regex_replace (p4r_reaction_user_str, e_val, "__mantis__mod_var_$1($2);");
    // regrex matching ${var} = <header>.<field>;
    std::regex e_field ("\\$\\{([^ ]*)\\}\\s*[=]\\s*([^\\. ]*)\\s*\\.\\s*([^\\; ]*)\\s*\\;");
    p4r_reaction_user_str = std::regex_replace (p4r_reaction_user_str, e_field, "__mantis__mod_var_$1_$2_$3;");

    // Change field arg format to C compatible variable
    vector<ReactionArgNode*> reaction_args = findReactionArgs(nodeArray);
    for (auto ra : reaction_args) {    
        if (ra->argType_==ReactionArgNode::INGRESS_FIELD) {
            FieldNode* fieldnode = dynamic_cast<FieldNode*>(ra->arg_);
            std::regex e_fieldarg (fieldnode->headerName_->toString()+"\\s*([^\\. ]*)\\s*\\.\\s*"+fieldnode->fieldName_->toString());
            p4r_reaction_user_str = std::regex_replace (p4r_reaction_user_str, e_fieldarg, fieldnode->headerName_->toString()+"_"+fieldnode->fieldName_->toString());
        }    
    }

    if (react_node != 0) {
        dialogue_str = new string(
            str(boost::format(kDialogueT) % oss_reaction_mirror.str() % p4r_reaction_user_str % oss_reaction_update.str()));
    } else {
        dialogue_str = new string(
            str(boost::format(kDialogueT) % oss_reaction_mirror.str() % "" % oss_reaction_update.str()));
    }

    UnanchoredNode * dialogue_cnode = new UnanchoredNode(dialogue_str, new string ("dialogue"), new string("pd_dialogue"));
    return dialogue_cnode;
}

UnanchoredNode * generateMacroNode(ostringstream& oss_preprocessor) {
    string* oss_preprocessor_p = new string(oss_preprocessor.str());
    UnanchoredNode * macro_cnode = new UnanchoredNode(oss_preprocessor_p, new string("macro"), new string("macro"));
    return macro_cnode;
}

// Currently not mirroring mbl field arg
void mirrorFieldArg(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_mirror, 
                    ostringstream& oss_preprocessor, vector<ReactionArgBin> bins,
                    string prefix_str, bool forIng) {
    for (int i = 0; i < bins.size(); ++i) {
        // Applies to both cases with/without isolation by indexing __mv
        if(forIng) {
            oss_reaction_mirror << str(boost::format(kIngFieldArgPollT) % std::to_string(bins[i].second) % std::to_string(i) % prefix_str);
        } else {
            oss_reaction_mirror << str(boost::format(kEgrFieldArgPollT) % std::to_string(bins[i].second) % std::to_string(i) % prefix_str);
        }

        // Reverse order
        for (int j = bins[i].first.size()-1; j >= 0; --j) {      
            AstNode* arg_node = bins[i].first[j].first->arg_;
            int width = bins[i].first[j].second;
            std::regex e_dot2underscore ("\\.");
            string field_arg_c = std::regex_replace(arg_node->toString(), e_dot2underscore, "_");

            ostringstream oss_mask_tmp;
            oss_mask_tmp << "0b";
            for (int k = 0; k < width; ++k) {
                oss_mask_tmp << "1";
            }
            if(forIng) {
                oss_reaction_mirror << "\n  uint"
                                    << width
                                    << "_t "
                                    << field_arg_c
                                    << "="
                                    << "__mantis__values_riSetArgs_"
                                    << i
                                    << "[1]&"
                                    << oss_mask_tmp.str()
                                    << ";";    
                oss_reaction_mirror << "\n  __mantis__values_riSetArgs_"
                                    << i
                                    << "[1]>>"
                                    << width
                                    << ";";                   
            } else {
                oss_reaction_mirror << "\n  uint"
                                    << width
                                    << "_t "
                                    << field_arg_c
                                    << "="
                                    << "__mantis__values_reSetArgs_"
                                    << i
                                    << "[1]&"
                                    << oss_mask_tmp.str()
                                    << ";";    
                oss_reaction_mirror << "\n  __mantis__values_reSetArgs_"
                                    << i
                                    << "[1]>>"
                                    << width
                                    << ";";   
            }                                       
        }
    }
}

void mirrorRegisterArgForIng(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_mirror, int iso_opt, string prefix_str, bool forIng) {

    vector<ReactionArgNode*> reaction_args = findReactionArgs(nodeArray);
    if(((unsigned int)iso_opt) & 0b1) {
        // Read replicas based on mv for each reg arg
        for (auto ra : reaction_args) {
            if(ra->argType_==ReactionArgNode::REGISTER) {
                if(forIng) {
                    if(!findRegargInIng(ra, nodeArray)) {
                        // Skip egr reg
                        continue;
                    }
                } else {
                    if(findRegargInIng(ra, nodeArray)) {
                        // Skip ing reg
                        continue;
                    }
                }
                bool is_valid_tmp = false;
                P4RegisterNode* target_reg = NULL;
                for (auto n : nodeArray) {
                    if (typeContains(n, "P4RegisterNode")) {
                        target_reg = dynamic_cast<P4RegisterNode*>(n);
                        if(target_reg->name_->toString().compare(ra->arg_->toString())==0) {
                            is_valid_tmp = true;
                            break;
                        }
                    }
                }    
                // Currently assuming non-64b reg to isolate
                if(is_valid_tmp) {
                    if(ra->index2_) {
                        if(forIng) {
                            oss_reaction_mirror << str(boost::format(kRegArgIsoMirrorT) % ra->arg_->toString() % std::to_string(target_reg->width_) % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(std::stoi(ra->index2_->toString())) % "__mantis__mv_ing");
                        } else {
                            oss_reaction_mirror << str(boost::format(kRegArgIsoMirrorT) % ra->arg_->toString() % std::to_string(target_reg->width_) % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(std::stoi(ra->index2_->toString())) % "__mantis__mv_egr");
                        }
                    } else {
                        if(forIng) {
                            oss_reaction_mirror << str(boost::format(kRegArgIsoMirrorT) % ra->arg_->toString() % std::to_string(target_reg->width_) % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(target_reg->instanceCount_) % "__mantis__mv_ing");
                        } else {
                            oss_reaction_mirror << str(boost::format(kRegArgIsoMirrorT) % ra->arg_->toString() % std::to_string(target_reg->width_) % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(target_reg->instanceCount_) % "__mantis__mv_egr");
                        }
                    }
                } else {
                    PANIC("WARNING: Invalid register argument\n");
                }
            }
        }

        // After all arg mirroring, flip mv
        if(forIng) {
            oss_reaction_mirror << "\n  __mantis__flip_mv_ing;\n\n";
        } else {
            oss_reaction_mirror << "\n  __mantis__flip_mv_egr;\n\n";
        }
    } else {
        // Directly mirror the value to the correponding control plane array
        for (auto ra : reaction_args) {
            if(ra->argType_==ReactionArgNode::REGISTER) {
                if(forIng) {
                    if(!findRegargInIng(ra, nodeArray)) {
                        // Skip egr reg
                        continue;
                    }
                } else {
                    if(findRegargInIng(ra, nodeArray)) {
                        // Skip ing reg
                        continue;
                    }
                }                
                bool is_valid_tmp = false;
                P4RegisterNode* target_reg = NULL;
                for (auto n : nodeArray) {
                    if (typeContains(n, "P4RegisterNode")) {
                        target_reg = dynamic_cast<P4RegisterNode*>(n);
                        if(target_reg->name_->toString().compare(ra->arg_->toString())==0) {
                            is_valid_tmp = true;
                            break;
                        }
                    }
                }              
                // Check the register width
                int width = findRegargWidth(ra, nodeArray);
                if(is_valid_tmp) {
                    if(ra->index2_) {
                        if (width==64) {
                            oss_reaction_mirror << str(boost::format(kRegArgMirrorT_64) % ra->arg_->toString() % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(std::stoi(ra->index2_->toString())) % ra->arg_->toString());
                        } else {
                            oss_reaction_mirror << str(boost::format(kRegArgMirrorT_32) % std::to_string(target_reg->width_) % ra->arg_->toString() % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(std::stoi(ra->index2_->toString())) % ra->arg_->toString());
                        }
                    } else {
                        if (width==64) {
                            oss_reaction_mirror << str(boost::format(kRegArgMirrorT_64) % ra->arg_->toString() % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(target_reg->instanceCount_) % ra->arg_->toString());
                        } else {
                            oss_reaction_mirror << str(boost::format(kRegArgMirrorT_32) % std::to_string(target_reg->width_) % ra->arg_->toString() % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(target_reg->instanceCount_) % ra->arg_->toString());
                        }                        
                    }
                } else {
                    PRINT_VERBOSE("WARNING: Invalid register argument\n");
                }
            }
        }
    }

    // Before reaction logic, flip __vv from working copy for prepare phase
    if(forIng) {
        if(((unsigned int)iso_opt) & 0b10) {
            oss_reaction_mirror << "\n  __mantis__flip_vv_ing;\n\n";
            // Note that __mantis__mbl_updated_ing=1 not appended as it is only triggered when
            // there are entries actually prepared for execution
        }
    } else {
        if(((unsigned int)iso_opt) & 0b10) {
            oss_reaction_mirror << "\n  __mantis__flip_vv_egr;\n\n";
        }        
    }
}

void generateMacroXorVersionBits(ostringstream& oss_reaction_mirror, ostringstream& oss_preprocessor, int ing_iso_opt, int egr_iso_opt) {
    // Remember the current version bit values for reaction and measurement
    // We always pack field args and use mantis_mv_bit to index the replicas
    oss_reaction_mirror << "  static unsigned int __mantis__mv_ing = 0;\n";
    oss_reaction_mirror << "  static unsigned int __mantis__mv_egr = 0;\n";
    if(((unsigned int)ing_iso_opt) & 0b1) {
        // Create macros for flip version bit
        oss_preprocessor << "\n#define "
                        << "__mantis__flip_mv_ing "
                        << "__mantis__mv_ing=__mantis__mv_ing^0x1"
                        << "\n";        
    } 
    if (((unsigned int)ing_iso_opt) & 0b10) {
        oss_reaction_mirror << "  static unsigned int __mantis__vv_ing = 0x0;\n";
        oss_preprocessor << "\n#define "
                        << "__mantis__flip_vv_ing "
                        << "__mantis__vv_ing=__mantis__vv_ing^0x1"
                        << "\n";         
    }
    if(((unsigned int)egr_iso_opt) & 0b1) {
        oss_preprocessor << "\n#define "
                        << "__mantis__flip_mv_egr "
                        << "__mantis__mv_egr=__mantis__mv_egr^0x1"
                        << "\n";        
    } 
    if (((unsigned int)egr_iso_opt) & 0b10) {
        oss_reaction_mirror << "  static unsigned int __mantis__vv_egr = 0x0;\n";
        oss_preprocessor << "\n#define "
                        << "__mantis__flip_vv_egr "
                        << "__mantis__vv_egr=__mantis__vv_egr^0x1"
                        << "\n";         
    }    
}

// Prepare helper vars and set mv gate bit
void generateDialogueArgStart(ostringstream& oss_reaction_mirror, int ing_iso_opt, int egr_iso_opt) {

    // Shared tmp variables
    oss_reaction_mirror << "  int __mantis__reg_flags=1;\n"
                       << "  int __mantis__value_count;\n"
                       << "  int __mantis__num_actually_read;\n"
                       << "  int __mantis__i;\n";
    // Take snapshots of ing/egr metrics
    if(((unsigned int)ing_iso_opt) & 0b1) {
        oss_reaction_mirror << "  __mantis__mbl_updated_ing=1;\n"
                           // Update data plane working copy
                           << "\n  __mantis__flip_mv_ing;\n"
                           << "\n  __mantis__mod_vars_ing;\n"
                           << "\n  __mantis__flip_mv_ing;\n"
                           << "\n  __mantis__mbl_updated_ing=0;\n";  
    }
    if(((unsigned int)egr_iso_opt) & 0b1) {
        oss_reaction_mirror << "  __mantis__mbl_updated_egr=1;\n"
                           // Update data plane working copy
                           << "\n  __mantis__flip_mv_egr;\n"
                           << "\n  __mantis__mod_vars_egr;\n"
                           << "\n  __mantis__flip_mv_egr;\n"
                           << "\n  __mantis__mbl_updated_egr=0;\n";  
    }    

}

// Synthesizing macros for non-mbl table manipulations
// These operations should only be used at prologue as no isolation provided
void generateMacroNonMblTable(std::vector<AstNode*> nodeArray, ostringstream& oss_preprocessor, string prefix_str) {
    // <TBL_MANIPULATION_SYNTAX> ::= <TBL_NAME>_<OPERATION_TYPE>(_[ACT_name])^{0,1}([ENTRY_INDEX], <ARGS>^{0,1})
    // <ARGS> ::= <MATCH_ARGS> <ACT_ARGS>
    // Match/action arguements are intepreted in the same sequential order as in P4 match/action code
    // If the table includes ternary match, priority set is required at the end of match arguments
    ostringstream oss_macro_tmp;
    ostringstream oss_replace_tmp;
    for (auto node : nodeArray) {
        if(typeContains(node, "TableNode") && !typeContains(node, "P4R")) {
            TableNode* table = dynamic_cast<TableNode*>(node);
            // Get rid of Table nodes that are actually child of variable P4R table
            if(table->isMalleable_) {
                continue;
            }
            string table_name = *(table->name_->word_);
            TableActionStmtsNode* actions = table->actions_;
            TableReadStmtsNode* reads = table->reads_;

            // Only applies to tables with reads 
            if(reads) {
                // Check if contains ternary match (needs priority)
                bool with_ternary = false;
                for (TableReadStmtNode* trs : *reads->list_) {
                    string match_field_name = trs->field_->toString();
                    TableReadStmtNode::MatchType matchType = trs->matchType_;
                    if(matchType==TableReadStmtNode::TERNARY) {
                        with_ternary = true;
                        break;
                    }
                }                

                // Operations are attached to a specific action
                for (TableActionStmtNode* tas : *actions->list_) {
                    string action_name = *tas->name_->word_;
                    oss_replace_tmp.str("");
                    oss_macro_tmp.str("");
                    
                    ////////////////////////////////
                    // Add: declare match spec data struct
                    oss_macro_tmp << table_name
                                << "_add_"
                                << action_name
                                << "(ARG_INDEX,";
                    oss_replace_tmp << "\t"
                                     << prefix_str
                                     << table_name
                                     << "_match_spec_t "
                                     << table_name
                                     << "_match_spec_##ARG_INDEX;\\\n"
                                     << kMantisNl;                                
                    int arg_index = 0;
                    for (TableReadStmtNode* trs : *reads->list_) {
                        string match_field_name = trs->field_->toString();
                        std::replace(match_field_name.begin(), match_field_name.end(), '.', '_');
                        TableReadStmtNode::MatchType matchType = trs->matchType_;
                        if(arg_index == 0) {
                            if(matchType==TableReadStmtNode::TERNARY) {
                                oss_macro_tmp << "ARG_"
                                            << std::to_string(arg_index)
                                            << ",ARG_"
                                            << std::to_string(arg_index)
                                            << "_MASK";
                                oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name
                                                << "=ARG_"
                                                << std::to_string(arg_index)
                                                << ";\\\n"
                                                << kMantisNl;
                                oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name+"_mask=ARG_"
                                                << std::to_string(arg_index)
                                                << "_MASK;\\\n"
                                                << kMantisNl;
                                arg_index++;
                            }
                            if(matchType==TableReadStmtNode::EXACT) {
                                oss_macro_tmp << ("ARG_"+std::to_string(arg_index));
                                oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name+"=ARG_"
                                                << std::to_string(arg_index)
                                                << ";\\\n"
                                                << kMantisNl;
                                arg_index++;
                            }
                        } else {
                            if(matchType==TableReadStmtNode::TERNARY) {
                                oss_macro_tmp << ",ARG_"
                                            << std::to_string(arg_index)
                                            << ",ARG_"
                                            << std::to_string(arg_index)
                                            << "_MASK";
                                oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name+"=ARG_"
                                                << std::to_string(arg_index)
                                                << ";\\\n"
                                                << kMantisNl;
                                oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name+"_mask=ARG_"
                                                << std::to_string(arg_index)
                                                << "_MASK;\\\n"
                                                << kMantisNl;
                                arg_index++;
                            }
                            if(matchType==TableReadStmtNode::EXACT) {
                                oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name+"=ARG_"
                                                << std::to_string(arg_index)
                                                << ";\\\n"
                                                << kMantisNl;
                                oss_macro_tmp << ",ARG_"
                                            << std::to_string(arg_index);
                                arg_index++;
                            }
                        }
                    }
                    if(with_ternary) {
                        oss_macro_tmp << ",ARG_PRIO";
                        oss_replace_tmp << "\tint priority_##ARG_INDEX=ARG_PRIO;\\\n"
                                        << kMantisNl;
                    }
                    // Find the ActionNode and add action arguments
                    int action_arg_index = 0;
                    for (auto tmp_node : nodeArray) {
                        if(typeContains(tmp_node, "ActionNode")) {
                            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                            string tmp_action_name = tmp_action_node->name_->toString();
                            if(tmp_action_name.compare(action_name)==0) {
                                ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                                for (ActionParamNode* apn : *tmp_action_params->list_) {
                                    if (action_arg_index==0) {
                                        // Declare action spec data struct
                                        oss_replace_tmp << "\t"
                                                        << prefix_str
                                                        << action_name
                                                        << "_action_spec_t "
                                                        << "__mantis__add_"
                                                        << action_name
                                                        << "_action_spec_##ARG_INDEX;\\\n"
                                                        << kMantisNl;
                                    }
                                    oss_macro_tmp << ",ARG_ACTION_"
                                                << std::to_string(action_arg_index);
                                    oss_replace_tmp << "\t"
                                                    << "__mantis__add_"
                                                    << action_name
                                                    << "_action_spec_##ARG_INDEX.action_"
                                                    << apn->toString()
                                                    << "=ARG_ACTION_"
                                                    << std::to_string(action_arg_index)
                                                    << ";\\\n"
                                                    << kMantisNl;
                                    action_arg_index++;
                                }
                                break;
                            }
                        }
                    }
                    oss_macro_tmp << ")";
                    // Finally call the entry modification api
                    oss_replace_tmp << "\t__mantis__status_tmp="
                                    << prefix_str
                                    << table_name
                                    << "_table_add_with_"
                                    << action_name
                                    << "(sess_hdl,pipe_mgr_dev_tgt,&"
                                    << table_name
                                    << "_match_spec_##ARG_INDEX,priority_##ARG_INDEX,";
                    if(action_arg_index!=0) {
                        oss_replace_tmp << "&__mantis__add_"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX,";
                    }
                    oss_replace_tmp << "&hdls["
                                    << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                    << std::to_string(kHandlerOffset)
                                    << "))]);\\\n"
                                    << kMantisNl;

                    oss_preprocessor << "\n#define "
                                    << oss_macro_tmp.str()
                                    << " "
                                    << oss_replace_tmp.str()
                                    << "\t"
                                    << kErrorCheckStr;

                    ////////////////////////////////
                    // Delete operation
                    oss_macro_tmp.str("");
                    oss_replace_tmp.str("");
                    oss_macro_tmp << table_name
                                << "_del_"
                                << action_name
                                << "(ARG_INDEX)";
                    oss_replace_tmp << "\t__mantis__status_tmp="
                                    << prefix_str
                                    << table_name
                                    << "_table_delete"
                                    << "(sess_hdl,pipe_mgr_dev_tgt.device_id,hdls["
                                    << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                    << std::to_string(kHandlerOffset)
                                    << "))]);\\\n"
                                    << kMantisNl;
                    oss_preprocessor << "\n#define "
                                    << oss_macro_tmp.str()
                                    << " "
                                    << oss_replace_tmp.str()
                                    << "\t"
                                    << kErrorCheckStr;  

                    ////////////////////////////////
                    // Modify operation
                    oss_macro_tmp.str("");
                    oss_replace_tmp.str("");
                    oss_macro_tmp << table_name
                                << "_mod_"
                                << action_name
                                << "(ARG_INDEX";
                    action_arg_index = 0;
                    for (auto tmp_node : nodeArray) {
                        if(typeContains(tmp_node, "ActionNode")) {
                            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                            string tmp_action_name = tmp_action_node->name_->toString();
                            if(tmp_action_name.compare(action_name)==0) {
                                ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                                for (ActionParamNode* apn : *tmp_action_params->list_) {
                                    if(action_arg_index==0) {
                                        oss_replace_tmp << "\t"
                                                        << prefix_str
                                                        << action_name
                                                        << "_action_spec_t "
                                                        << "__mantis__mod_"
                                                        << action_name
                                                        << "_action_spec_##ARG_INDEX;\\\n"
                                                        << kMantisNl;
                                    }
                                    oss_macro_tmp << ",ARG_ACTION_"
                                                << std::to_string(action_arg_index);
                                    oss_replace_tmp << "\t"
                                                    << "__mantis__mod_"
                                                    << action_name
                                                    << "_action_spec_##ARG_INDEX.action_"
                                                    << apn->toString()
                                                    << "=ARG_ACTION_"
                                                    << std::to_string(action_arg_index)
                                                    << ";\\\n"
                                                    << kMantisNl;
                                    action_arg_index += 1;
                                }
                                break;
                            }
                        }
                    }
                    oss_macro_tmp << ")";
                    // Finally call the entry modification api
                    oss_replace_tmp << "\t__mantis__status_tmp="
                                    << prefix_str
                                    << table_name
                                    << "_table_modify_with_"
                                    << action_name
                                    << "(sess_hdl,pipe_mgr_dev_tgt.device_id,hdls["
                                    << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                    << std::to_string(kHandlerOffset)
                                    << "))]";
                    if(action_arg_index!=0){
                        oss_replace_tmp << ",&__mantis__mod_"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX";
                    }
                    oss_replace_tmp << ");\\\n"
                                    << kMantisNl;

                    oss_preprocessor << "\n#define ";
                    oss_preprocessor << oss_macro_tmp.str()
                                    << " "
                                    << oss_replace_tmp.str()
                                    << "\t"
                                    << kErrorCheckStr;  
                }   
            }

            else {
                // Non-mbl table without reads is with a single default entry
                // Manipulation is solely <TBL_NAME>_<ACT_NAME>(<ENTRY_INDEX>, <ACT_ARG_LIST>)
                for (TableActionStmtNode* tas : *actions->list_) {
                    string action_name = *tas->name_->word_;
                    oss_replace_tmp.str("");
                    oss_macro_tmp.str("");

                    oss_macro_tmp << table_name
                                << "_"
                                << action_name
                                << "(ARG_INDEX";
                    int action_arg_index = 0;                                
                    for (auto tmp_node : nodeArray) {
                        if(typeContains(tmp_node, "ActionNode")) {
                            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                            string tmp_action_name = tmp_action_node->name_->toString();
                            if(tmp_action_name.compare(action_name)==0) {
                                ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                                for (ActionParamNode* apn : *tmp_action_params->list_) {
                                    if(action_arg_index==0) {
                                        // if no arguments, don't synthesize action_spec
                                        oss_replace_tmp << "\t"
                                                        << prefix_str
                                                        << action_name
                                                        << "_action_spec_t "
                                                        << action_name
                                                        << "_action_spec;\\\n"
                                                        << kMantisNl;                                        
                                    }
                                    oss_macro_tmp << ",ARG_ACTION_"
                                                << std::to_string(action_arg_index);                                        
                                    oss_replace_tmp << "\t"
                                                    << action_name
                                                    << "_action_spec.action_"
                                                    << apn->toString()
                                                    << "=ARG_ACTION_"
                                                    << std::to_string(action_arg_index)
                                                    << ";\\\n"
                                                    << kMantisNl;
                                    action_arg_index++;
                                }
                                break;
                            }
                        }
                    }
                    oss_macro_tmp << ")";
                    if(action_arg_index==0) {
                        oss_replace_tmp << "\t__mantis__status_tmp="
                                        << prefix_str
                                        << table_name
                                        << "_set_default_action_"
                                        << action_name
                                        << "(sess_hdl,pipe_mgr_dev_tgt,"
                                        << "&hdls["
                                        << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                        << std::to_string(kHandlerOffset)
                                        << "))]);\\\n"
                                        << kMantisNl;
                    } else {
                        oss_replace_tmp << "\t__mantis__status_tmp="
                                        << prefix_str
                                        << table_name
                                        << "_set_default_action_"
                                        << action_name
                                        << "(sess_hdl,pipe_mgr_dev_tgt,&"
                                        << action_name
                                        << "_action_spec,&hdls["
                                        << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                        << std::to_string(kHandlerOffset)
                                        << "))]);\\\n"
                                        << kMantisNl;
                    }

                    // Finally append to the preprocesser
                    oss_preprocessor << "\n#define "
                                    << oss_macro_tmp.str()
                                    << " "
                                    << oss_replace_tmp.str()
                                    << "\t"
                                    << kErrorCheckStr;                     
                }                
            }             
        }
	}

}

// For the user, same syntax as non-mbl table
void generateMacroMblTable(std::vector<AstNode*> nodeArray, ostringstream& oss_preprocessor, string prefix_str, int ing_iso_opt, int egr_iso_opt, ostringstream& oss_reaction_mirror) {

    // With isolation, we need an array indicating whether the handler is triggered in the dialogue for later mirroring
    if(((unsigned int)ing_iso_opt) & 0b10 || ((unsigned int)egr_iso_opt) & 0b10) {
        oss_reaction_mirror << "  static bool __mantis__indicator_hdls"
                            << "[" << std::to_string(kNumUserHdls) << "];\n";
    }

    ostringstream oss_macro_tmp;
    ostringstream oss_replace_tmp;
    // mbl table + mbl field in action is an uncommon usage, currently not supported
    for (auto node : nodeArray) {
        if(typeContains(node, "P4RMalleableTableNode")) {
            TableNode* table = dynamic_cast<P4RMalleableTableNode*>(node)->table_;
            string table_name = *(table->name_->word_);

            TableActionStmtsNode* actions = table->actions_;
            TableReadStmtsNode* reads = table->reads_;

            // Mbl table always has reads
            // Check if constains ternary match (needs priority)
            bool with_ternary = false;
            for (TableReadStmtNode* trs : *reads->list_) {
                string match_field_name = trs->field_->toString();
                TableReadStmtNode::MatchType matchType = trs->matchType_;
                if(matchType==TableReadStmtNode::TERNARY) {
                    with_ternary = true;
                    break;
                }
            }                

            // All operations are attached to a specific action
            for (TableActionStmtNode* tas : *actions->list_) {
                string action_name = *tas->name_->word_;
                oss_replace_tmp.str("");
                oss_macro_tmp.str("");
                
                ////////////////////////////////
                // Add: declare match spec data struct
                oss_macro_tmp << table_name
                            << "_add_"
                            << action_name
                            << "(ARG_INDEX,";
                oss_replace_tmp << prefix_str
                                << table_name
                                << "_match_spec_t "
                                << table_name
                                << "_match_spec_##ARG_INDEX;\\\n"
                                << kMantisNl;                                   
                int arg_index = 0;
                for (TableReadStmtNode* trs : *reads->list_) {
                    string match_field_name = trs->field_->toString();
                    std::replace(match_field_name.begin(), match_field_name.end(), '.', '_');
                    TableReadStmtNode::MatchType matchType = trs->matchType_;
                    if(arg_index == 0) {
                        if(matchType==TableReadStmtNode::TERNARY) {
                            oss_macro_tmp << "ARG_"
                                        << std::to_string(arg_index)
                                        << ",ARG_"
                                        << std::to_string(arg_index)
                                        << "_MASK";
                            oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name
                                            << "=ARG_"
                                            << std::to_string(arg_index)
                                            << ";\\\n"
                                            << kMantisNl;
                            oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"_mask=ARG_"
                                            << std::to_string(arg_index)
                                            << "_MASK;\\\n"
                                            << kMantisNl;
                            arg_index++;
                        }
                        if(matchType==TableReadStmtNode::EXACT) {
                            // Mbl table was transformed to add exact match on vv
                            // If the match field is the newly extended __vv
                            if(findTblInIng(table_name, nodeArray) && match_field_name.compare(string(kP4rIngMetadataName)+"_"+"__vv")==0) {
                                oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"="
                                            << "__mantis__vv_ing"
                                            << ";\\\n"
                                            << kMantisNl;
                                // Note programmar does not provide the argument
                            } 
                            else if(!findTblInIng(table_name, nodeArray) && match_field_name.compare(string(kP4rEgrMetadataName)+"_"+"__vv")==0) {
                                oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"="
                                            << "__mantis__vv_egr"
                                            << ";\\\n"
                                            << kMantisNl;
                            } else {
                                oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"=ARG_"
                                            << std::to_string(arg_index)
                                            << ";\\\n"
                                            << kMantisNl; 
                                oss_macro_tmp << ("ARG_"+std::to_string(arg_index));
                                arg_index++;
                            }
                        }
                    } else {
                        if(matchType==TableReadStmtNode::TERNARY) {
                            oss_macro_tmp << ",ARG_"
                                        << std::to_string(arg_index)
                                        << ",ARG_"
                                        << std::to_string(arg_index)
                                        << "_MASK";
                            oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"=ARG_"
                                            << std::to_string(arg_index)
                                            << ";\\\n"
                                            << kMantisNl;
                            oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"_mask=ARG_"
                                            << std::to_string(arg_index)
                                            << "_MASK;\\\n"
                                            << kMantisNl;
                            arg_index++;
                        }
                        if(matchType==TableReadStmtNode::EXACT) { 
                            if(findTblInIng(table_name, nodeArray) && match_field_name.compare(string(kP4rIngMetadataName)+"_"+"__vv")==0) {
                                oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"="
                                            << "__mantis__vv_ing"
                                            << ";\\\n"
                                            << kMantisNl;
                            } 
                            else if(!findTblInIng(table_name, nodeArray) && match_field_name.compare(string(kP4rEgrMetadataName)+"_"+"__vv")==0) {
                                oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"="
                                            << "__mantis__vv_egr"
                                            << ";\\\n"
                                            << kMantisNl;
                            } else {
                                oss_replace_tmp << "\t"
                                            << table_name
                                            << "_match_spec_##ARG_INDEX."
                                            << match_field_name+"=ARG_"
                                            << std::to_string(arg_index)
                                            << ";\\\n"
                                            << kMantisNl; 
                                oss_macro_tmp << (",ARG_"+std::to_string(arg_index));
                                arg_index++;
                            }                                                           
                        }
                    }
                }
                if(with_ternary) {
                    oss_macro_tmp << ",ARG_PRIO";
                    oss_replace_tmp << "\tint priority_##ARG_INDEX=ARG_PRIO;\\\n"
                                    << kMantisNl;
                }
                // Find the ActionNode and add action arguments
                int action_arg_index = 0;
                for (auto tmp_node : nodeArray) {
                    // Currently addressing action without mbl fields
                    if(typeContains(tmp_node, "ActionNode")) {
                        ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                        string tmp_action_name = tmp_action_node->name_->toString();
                        if(tmp_action_name.compare(action_name)==0) {
                            ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                            for (ActionParamNode* apn : *tmp_action_params->list_) {
                                if(action_arg_index==0) {
                                    oss_replace_tmp << "\t"
                                                    << prefix_str
                                                    << action_name
                                                    << "_action_spec_t "
                                                    << "__mantis__add_"
                                                    << action_name
                                                    << "_action_spec_##ARG_INDEX;\\\n"
                                                    << kMantisNl;
                                }
                                oss_macro_tmp << ",ARG_ACTION_"
                                            << std::to_string(action_arg_index);
                                oss_replace_tmp << "\t"
                                                << "__mantis__add_"
                                                << action_name
                                                << "_action_spec_##ARG_INDEX.action_"
                                                << apn->toString()
                                                << "=ARG_ACTION_"
                                                << std::to_string(action_arg_index)
                                                << ";\\\n"
                                                << kMantisNl;
                                action_arg_index++;
                            }
                            break;
                        }
                    }
                }
                oss_macro_tmp << ")";
                // Finally call the entry add api
                oss_replace_tmp << "\t__mantis__status_tmp="
                                << prefix_str
                                << table_name
                                << "_table_add_with_"
                                << action_name
                                << "(sess_hdl,pipe_mgr_dev_tgt,&"
                                << table_name
                                << "_match_spec_##ARG_INDEX,priority_##ARG_INDEX,";
                if(action_arg_index != 0) {
                    oss_replace_tmp  << "&__mantis__add_"
                                << action_name
                                << "_action_spec_##ARG_INDEX,";
                }
                oss_replace_tmp << "&hdls["
                                << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                << std::to_string(kHandlerOffset);
                if(findTblInIng(table_name, nodeArray)) {
                    oss_replace_tmp << ")+__mantis__vv_ing)]);\\\n"
                                    << kMantisNl
                                    << "\t"
                                    << "__mantis__mbl_updated_ing=1;\\\n"
                                    << kMantisNl;
                } else {
                    oss_replace_tmp << ")+__mantis__vv_egr]);\\\n"
                                    << kMantisNl
                                    << "\t"
                                    << "__mantis__mbl_updated_egr=1;\\\n"
                                    << kMantisNl;
                }

                // Mirror macro, wrap it with conditionals and reset
                oss_preprocessor << "\n#define "
                                << "__mantis__mirror_"
                                << oss_macro_tmp.str()
                                << " "
                                << "if(__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]==1) {\\\n"
                                << kMantisNl
                                << "\t"
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr
                                << "\t"
                                << "__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]=0;"
                                << kMantisNl
                                << "\t}"
                                << kMantisNl
                                << "\n";

                // Set the indicator array
                oss_replace_tmp << "\t__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]=1;\\\n"
                                << kMantisNl;
                // User macro during prepare phase
                oss_preprocessor << "\n#define "
                                << oss_macro_tmp.str()
                                << " "
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr;  

                ////////////////////////////////
                // Delete operation
                oss_macro_tmp.str("");
                oss_replace_tmp.str("");
                oss_macro_tmp << table_name
                            << "_del_"
                            << action_name
                            << "(ARG_INDEX)";
                oss_replace_tmp << "\t__mantis__status_tmp="
                                << prefix_str
                                << table_name
                                << "_table_delete"
                                << "(sess_hdl,pipe_mgr_dev_tgt.device_id,hdls["
                                << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                << std::to_string(kHandlerOffset);
                if(findTblInIng(table_name, nodeArray)) {
                    oss_replace_tmp << ")+__mantis__vv_ing)]);\\\n"
                                    << kMantisNl
                                    << "\t"
                                    << "__mantis__mbl_updated_ing=1;\\\n"
                                    << kMantisNl; 
                } else {
                    oss_replace_tmp << ")+__mantis__vv_egr]);\\\n"
                                    << kMantisNl
                                    << "\t"
                                    << "__mantis__mbl_updated_egr=1;\\\n"
                                    << kMantisNl;                 
                }
                // Mirror macro, wrap it with conditionals and reset
                oss_preprocessor << "\n#define "
                                << "__mantis__mirror_"
                                << oss_macro_tmp.str()
                                << " "
                                << "if(__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]==1) {\\\n"
                                << kMantisNl
                                << "\t"
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr
                                << "\t"
                                << "__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]=0;"
                                << kMantisNl
                                << "\t}"
                                << kMantisNl
                                << "\n";

                // Set the indicator array
                oss_replace_tmp << "\t__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]=1;\\\n"
                                << kMantisNl;
                // User macro during prepare phase
                oss_preprocessor << "\n#define "
                                << oss_macro_tmp.str()
                                << " "
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr;  

                ////////////////////////////////
                // Modify operation
                oss_macro_tmp.str("");
                oss_replace_tmp.str("");
                oss_macro_tmp << table_name
                            << "_mod_"
                            << action_name
                            << "(ARG_INDEX";
                action_arg_index = 0;                               
                for (auto tmp_node : nodeArray) {
                    if(typeContains(tmp_node, "ActionNode")) {
                        ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                        string tmp_action_name = tmp_action_node->name_->toString();
                        if(tmp_action_name.compare(action_name)==0) {
                            ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                            for (ActionParamNode* apn : *tmp_action_params->list_) {
                                if(action_arg_index==0) {
                                    oss_replace_tmp << "\t"
                                         << prefix_str
                                         << action_name
                                         << "_action_spec_t "
                                         << "__mantis__mod_"
                                         << action_name
                                         << "_action_spec_##ARG_INDEX;\\\n"
                                         << kMantisNl; 
                                }
                                oss_macro_tmp << ",ARG_ACTION_"
                                            << std::to_string(action_arg_index);
                                oss_replace_tmp << "\t"
                                                << "__mantis__mod_"
                                                << action_name
                                                << "_action_spec_##ARG_INDEX.action_"
                                                << apn->toString()
                                                << "=ARG_ACTION_"
                                                << std::to_string(action_arg_index)
                                                << ";\\\n"
                                                << kMantisNl;
                                action_arg_index += 1;
                            }
                            break;
                        }
                    }
                }
                oss_macro_tmp << ")";
                // Finally call the entry modification api
                oss_replace_tmp << "\t__mantis__status_tmp="
                                << prefix_str
                                << table_name
                                << "_table_modify_with_"
                                << action_name
                                << "(sess_hdl,pipe_mgr_dev_tgt.device_id,hdls["
                                << std::to_string(num_max_alts) << "*(2*(ARG_INDEX+"
                                << std::to_string(kHandlerOffset);
                if(findTblInIng(table_name, nodeArray)) {
                    oss_replace_tmp << ")+__mantis__vv_ing)]";
                    if (action_arg_index != 0) {
                        oss_replace_tmp << ",&__mantis__mod_"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX";
                    }
                    oss_replace_tmp << ");\\\n"
                                    << kMantisNl
                                    << "\t"
                                    << "__mantis__mbl_updated_ing=1;\\\n"
                                    << kMantisNl;
                } else {
                    oss_replace_tmp << ")+__mantis__vv_egr]";
                    if (action_arg_index != 0) {
                        oss_replace_tmp << ",&__mantis__mod_"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX";
                    }
                    oss_replace_tmp << ");\\\n"
                                    << kMantisNl
                                    << "\t"
                                    << "__mantis__mbl_updated_egr=1;\\\n"
                                    << kMantisNl;
                }
                                

                // Mirror macro, wrap it with conditionals and reset
                oss_preprocessor << "\n#define "
                                << "__mantis__mirror_"
                                << oss_macro_tmp.str()
                                << " "
                                << "if(__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]==1) {\\\n"
                                << kMantisNl
                                << "\t"
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr
                                << "\t"
                                << "__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]=0;"
                                << kMantisNl
                                << "\t}"
                                << kMantisNl
                                << "\n";

                // Set the indicator array
                oss_replace_tmp << "\t__mantis__indicator_hdls[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]=1;\\\n"
                                << kMantisNl;
                // User macro during prepare phase
                oss_preprocessor << "\n#define "
                                << oss_macro_tmp.str()
                                << " "
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr;  
            }   
        }
    }

}

// Generate mantis-syntax single mbl mod macros and __mantis__add_vars_ing, __mantis__mod_vars_ing, __mantis__add_vars_egr, __mantis__mod_vars_egr
void generateMacroInitMblsForIng(std::vector<AstNode*> nodeArray, unordered_map<string, int>* mblUsages, ostringstream& oss_mbl_init, ostringstream& oss_reaction_mirror, 
                             ostringstream& oss_preprocessor, int num_mbls, int iso_opt, string prefix_str, bool forIng) {

    // Both the same for monolithic master init table (even with update isolation)
    ostringstream oss_replace_mantis_add_vars;
    ostringstream oss_replace_mantis_mod_vars;

    string p4rInitActionName;
    int initEntryHandlerIndex;

    string mv;
    string vv;

    if(forIng) {
        oss_mbl_init << "  int __mantis__mbl_updated_ing=0;\n";
        oss_reaction_mirror << "  int __mantis__mbl_updated_ing=0;\n";
        oss_replace_mantis_add_vars << "if(__mantis__mbl_updated_ing==1) {\\\n"
                                    << kMantisNl;
        oss_replace_mantis_mod_vars << "if(__mantis__mbl_updated_ing==1) {\\\n"
                                    << kMantisNl;       
        p4rInitActionName = string(kP4rIngInitAction);
        initEntryHandlerIndex = kIngInitEntryHandlerIndex;
        mv =  "__mantis__mv_ing";
        vv =  "__mantis__vv_ing";
    } else {
        oss_mbl_init << "  int __mantis__mbl_updated_egr=0;\n";
        oss_reaction_mirror << "  int __mantis__mbl_updated_egr=0;\n";
        oss_replace_mantis_add_vars << "if(__mantis__mbl_updated_egr==1) {\\\n"
                                    << kMantisNl;
        oss_replace_mantis_mod_vars << "if(__mantis__mbl_updated_egr==1) {\\\n"
                                    << kMantisNl;  
        p4rInitActionName = string(kP4rEgrInitAction);   
        initEntryHandlerIndex = kEgrInitEntryHandlerIndex;
        mv =  "__mantis__mv_egr";
        vv =  "__mantis__vv_egr";
    }

    // num_mbls includes version bits as well
    if(num_mbls!=0) {
        // Declare p4r_init action variable
        oss_replace_mantis_add_vars << "\t"
                                    << prefix_str
                                    << p4rInitActionName
                                    << "_"
                                    << kActionSuffixStr
                                    << " "
                                    << p4rInitActionName
                                    << ";\\\n"
                                    << kMantisNl;

        oss_replace_mantis_mod_vars << "\t"
                                    << prefix_str
                                    << p4rInitActionName
                                    << "_"
                                    << kActionSuffixStr
                                    << " "
                                    << p4rInitActionName
                                    << ";\\\n"
                                    << kMantisNl;                                    
    }

    // Initialize version bits
    if(((unsigned int)iso_opt) & 0b1) {
        oss_mbl_init << "  unsigned int "<< mv << " = 0x0;\n";

        oss_replace_mantis_add_vars << "\t"
                                    << p4rInitActionName
                                    << ".action___mv"
                                    << "=" << mv
                                    << ";\\\n"
                                    << kMantisNl;  
                                    
        oss_replace_mantis_mod_vars << "\t"
                                    << p4rInitActionName
                                    << ".action___mv"
                                    << "=" << mv
                                    << ";\\\n"
                                    << kMantisNl;
    } 
    if(((unsigned int)iso_opt) & 0b10) {
        oss_mbl_init << "  unsigned int " << vv << " = 0x0;\n";

        oss_replace_mantis_add_vars << "\t"
                                    << p4rInitActionName
                                    << ".action___vv"
                                    << "=" << vv
                                    << ";\\\n"
                                    << kMantisNl;

        oss_replace_mantis_mod_vars << "\t"
                                    << p4rInitActionName
                                    << ".action___vv"
                                    << "=" << vv
                                    << ";\\\n"
                                    << kMantisNl;                                    
    }   

    // Find init value for each variable
    // Assign init value calling the macro
    // Synthesize mantis_mod_var macro along the way
    for (auto n : nodeArray) {
        if (typeContains(n, "P4RMalleableValueNode")) {
            auto v = dynamic_cast<P4RMalleableValueNode*>(n);
            string var_name = *v->name_->word_;

            if(forIng && mblUsages->at(var_name)==USAGE::EGRESS) {
                continue;
            } 
            if(!forIng && mblUsages->at(var_name)==USAGE::INGRESS) {
                continue;
            }             

            oss_replace_mantis_add_vars << "\t"
                                        << p4rInitActionName
                                        << ".action_p__"
                                        << var_name
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;
            oss_replace_mantis_mod_vars << "\t"
                                        << p4rInitActionName
                                        << ".action_p__"
                                        << var_name
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;

            oss_mbl_init << "  int __mantis__"
                              << var_name
                              << ";\n";

            // Initial value for the mbl value
            auto init_tmp = dynamic_cast<VarInitNode*>(v->varInit_);
            string var_init_val = init_tmp->val_->toString();

            // Prepare static C variables to remember the previous value
            // As each call for modification involves providing values for all variables            
            oss_reaction_mirror << "  static int __mantis__"
                               << var_name
                               << "="
                               << var_init_val
                               << ";\n";

            oss_mbl_init << "  __mantis__mod_var_"
                              << var_name
                              << "("
                              << var_init_val
                              << ");\n";

            oss_preprocessor << "\n#define ";
            // var used in ing
            if(mblUsages->at(var_name)==USAGE::INGRESS) {
                oss_preprocessor << "__mantis__mod_var_"
                                 << var_name
                                 << "("
                                 << "var_value)"
                                 << " __mantis__"
                                 << var_name
                                 << "="
                                 << "var_value"
                                 << ";\\\n"
                                 << kMantisNl
                                 << "\t__mantis__mbl_updated_ing=1;"
                                 << kMantisNl;  
            } else if (mblUsages->at(var_name)==USAGE::EGRESS) {
                oss_preprocessor << "__mantis__mod_var_"
                                 << var_name
                                 << "("
                                 << "var_value)"
                                 << " __mantis__"
                                 << var_name
                                 << "="
                                 << "var_value"
                                 << ";\\\n"
                                 << kMantisNl
                                 << "\t__mantis__mbl_updated_egr=1;"
                                 << kMantisNl;  
            } else if (mblUsages->at(var_name)==USAGE::BOTH) {
                oss_preprocessor << "__mantis__mod_var_"
                                 << var_name
                                 << "("
                                 << "var_value)"
                                 << " __mantis__"
                                 << var_name
                                 << "="
                                 << "var_value"
                                 << ";\\\n"
                                 << kMantisNl
                                 << "\t__mantis__mbl_updated_ing=1;\\\n"
                                 << kMantisNl
                                 << "\t__mantis__mbl_updated_egr=1;"
                                 << kMantisNl;
            } else {
                PANIC("Invalid usage of %s\n", var_name.c_str());
            }
        } else if (typeContains(n, "P4RMalleableFieldNode")) {
            auto v = dynamic_cast<P4RMalleableFieldNode*>(n);
            string var_name = *v->name_->word_;

            if(forIng && mblUsages->at(var_name)==USAGE::EGRESS) {
                continue;
            } 
            if(!forIng && mblUsages->at(var_name)==USAGE::INGRESS) {
                continue;
            }

            std::replace(var_name.begin(), var_name.end(), '.', '_');
            oss_replace_mantis_add_vars << "\t"
                                        << p4rInitActionName
                                        << ".action_p__"
                                        << var_name
                                        << "__alt"
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;            
            oss_replace_mantis_mod_vars << "\t"
                                        << p4rInitActionName
                                        << ".action_p__"
                                        << var_name
                                        << "__alt"
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;   

            // Get init value for mbl field
            auto init_tmp = dynamic_cast<VarInitNode*>(v->varInit_);
            string var_init_val = init_tmp->val_->toString();

            // mapAltToInt requires original field string
            oss_reaction_mirror << "  static int __mantis__"
                               << var_name
                               << "="
                               << std::to_string(v->mapAltToInt(var_init_val))
                               << ";\n";

            std::replace(var_init_val.begin(), var_init_val.end(), '.', '_');

            oss_mbl_init << "  int __mantis__"
                              << var_name
                              << ";\n";

            oss_mbl_init << "  __mantis__mod_var_"
                              << var_name
                              << "_"
                              << var_init_val
                              << ";\n";

            // Install macros for each possible alternatives
            for (FieldNode* tmp_alt : *v->varAlts_->fields_->list_) {
                oss_preprocessor << "\n#define ";
                string tmp_str = tmp_alt->toString();
                std::replace(tmp_str.begin(), tmp_str.end(), '.', '_');
                if(mblUsages->at(var_name)==USAGE::INGRESS) {
                    oss_preprocessor << "  __mantis__mod_var_"
                                     << var_name
                                     << "_"
                                     << tmp_str
                                     << " __mantis__"
                                     << var_name
                                     << "="
                                     << std::to_string(v->mapAltToInt(tmp_alt->toString()))
                                     << ";\\\n"
                                     << kMantisNl
                                     << "\t__mantis__mbl_updated_ing=1;"
                                     << kMantisNl;
                } else if (mblUsages->at(var_name)==USAGE::EGRESS) {
                    oss_preprocessor << "  __mantis__mod_var_"
                                     << var_name
                                     << "_"
                                     << tmp_str
                                     << " __mantis__"
                                     << var_name
                                     << "="
                                     << std::to_string(v->mapAltToInt(tmp_alt->toString()))
                                     << ";\\\n"
                                     << kMantisNl
                                     << "\t__mantis__mbl_updated_egr=1;"
                                     << kMantisNl;
                } else if (mblUsages->at(var_name)==USAGE::BOTH) {
                    oss_preprocessor << "  __mantis__mod_var_"
                                     << var_name
                                     << "_"
                                     << tmp_str
                                     << " __mantis__"
                                     << var_name
                                     << "="
                                     << std::to_string(v->mapAltToInt(tmp_alt->toString()))
                                     << ";\\\n"
                                     << kMantisNl
                                     << "\t__mantis__mbl_updated_ing=1;\\\n"
                                     << kMantisNl
                                     << "\t__mantis__mbl_updated_egr=1;"
                                     << kMantisNl;
                } else {
                    PANIC("Invalid usage of %s\n", var_name.c_str());
                }
            }
        }
    }

    // Init table is always there
    if(forIng) {
        oss_replace_mantis_add_vars << "\t__mantis__status_tmp="
                                    << prefix_str
                                    << "__tiSetVars_set_default_action_"
                                    << p4rInitActionName
                                    << "(sess_hdl, pipe_mgr_dev_tgt, ";
    } else {
        oss_replace_mantis_add_vars << "\t__mantis__status_tmp="
                                    << prefix_str
                                    << "__teSetVars_set_default_action_"
                                    << p4rInitActionName
                                    << "(sess_hdl, pipe_mgr_dev_tgt, ";
    }

    if(num_mbls!=0) {
        oss_replace_mantis_add_vars << "&"
                                    << p4rInitActionName
                                    << ", ";
    }
    oss_replace_mantis_add_vars << "&hdls["
                                << std::to_string(num_max_alts) << "*(2*"
                                << std::to_string(initEntryHandlerIndex)
                                << ")]);\\\n"    
                                << kMantisNl;
    // Define the macro mantis_add_vars     
    if(forIng) {
        oss_preprocessor << "\n#define "
                         << "__mantis__add_vars_ing "
                         << oss_replace_mantis_add_vars.str()
                         << "\t"
                         << kErrorCheckStr
                         << " }";
        oss_replace_mantis_mod_vars << "\t__mantis__status_tmp="
                                    << prefix_str
                                    << "__tiSetVars_set_default_action_"
                                    << p4rInitActionName
                                    << "(sess_hdl, pipe_mgr_dev_tgt, ";                         
    } else {
        oss_preprocessor << "\n#define "
                         << "__mantis__add_vars_egr "
                         << oss_replace_mantis_add_vars.str()
                         << "\t"
                         << kErrorCheckStr
                         << " }";
        oss_replace_mantis_mod_vars << "\t__mantis__status_tmp="
                                    << prefix_str
                                    << "__teSetVars_set_default_action_"
                                    << p4rInitActionName
                                    << "(sess_hdl, pipe_mgr_dev_tgt, ";                   
    }                   


    if(num_mbls!=0) {
        oss_replace_mantis_mod_vars << "&"
                                    << p4rInitActionName
                                    << ", ";
    }                                
    oss_replace_mantis_mod_vars << "&hdls["
                                << std::to_string(num_max_alts) << "*(2*"
                                << std::to_string(initEntryHandlerIndex)
                                << ")]);\\\n"
                                << kMantisNl;
    if(forIng) {
        oss_preprocessor << "\n#define "
                         << "__mantis__mod_vars_ing "
                         << oss_replace_mantis_mod_vars.str()
                         << "\t"
                         << kErrorCheckStr
                         << " }\n";
    } else {
        oss_preprocessor << "\n#define "
                         << "__mantis__mod_vars_egr "
                         << oss_replace_mantis_mod_vars.str()
                         << "\t"
                         << kErrorCheckStr
                         << " }\n";
    }                             
}

void generateHdlPool(std::vector<AstNode*> nodeArray, ostringstream& oss_mbl_init, int ing_iso_opt, int egr_iso_opt) {
    // Max number of alts
    for (auto n : nodeArray) {
        if (typeContains(n, "P4RMalleableFieldNode")) {
            auto v = dynamic_cast<P4RMalleableFieldNode*>(n);

            string var_name = *v->name_->word_;

            int num_alt = 0;

            for (FieldNode* tmp_alt : *v->varAlts_->fields_->list_) {
                num_alt += 1;
            }
            PRINT_VERBOSE("Number of alts for %s: %d \n", var_name.c_str(), num_alt);
            if(num_alt > num_max_alts) {
                num_max_alts = num_alt;
            }            
       }
    }

    if(((unsigned int)ing_iso_opt) & 0b10 || ((unsigned int)egr_iso_opt) & 0b10) {
        oss_mbl_init << "  bool __mantis__indicator_hdls"
                     << "[" << std::to_string(kNumUserHdls) << "];\n";
    }    
    // Malloc handler array over linked list for random access, memoization cache omitted
    // oss_mbl_init << "  hdls = (uint32_t *)calloc("
    //              << kNumUserHdls << "*2*" << std::to_string(num_max_alts)
    //              << ", sizeof(uint32_t)"
    //              << ");\n\n";
}

void generatePrologueEnd(std::vector<AstNode*> nodeArray, ostringstream& oss_init_end, int ing_iso_opt, int egr_iso_opt) {

    oss_init_end << "  __mantis__add_vars_ing;\n";
    oss_init_end << "  __mantis__add_vars_egr;\n";

    // Flip __vv to point to shallow copy
    if(((unsigned int)ing_iso_opt) & 0b10) {
        oss_init_end << "\n  __mantis__flip_vv_ing;\n\n";
    }
    if(((unsigned int)egr_iso_opt) & 0b10) {
        oss_init_end << "\n  __mantis__flip_vv_egr;\n\n";
    }    

    P4RInitBlockNode * init_node = findInitBlock(nodeArray);
    string p4r_init_user_str = "";
    if (init_node != 0) {
        p4r_init_user_str = init_node->body_->toString();
    }

    for (auto node : nodeArray) {
        // For each mbl table
        if(typeContains(node, "P4RMalleableTableNode")) {
            TableNode* table = dynamic_cast<P4RMalleableTableNode*>(node)->table_;
            string table_name = *(table->name_->word_);

            // Whenever mbl table(s) present, isolation required

            TableActionStmtsNode* actions = table->actions_;

            // Mbl tables are always with With matches (at least match on __vv)
            // For each action
            for (TableActionStmtNode* tas : *actions->list_) {
                string action_name = *tas->name_->word_;

                string str_prepare_macro_func;
                string str_mirror_macro_func;

                std::smatch result;

                // Construct match regrex for add
                str_prepare_macro_func = table_name + "_add_" + action_name;
                std::regex e_add (str_prepare_macro_func+"\\s*\\("+"([^\\)]*)"+"\\)");   // matches words beginning by "sub"

                // Stop after first match
                while (std::regex_search (p4r_init_user_str, result, e_add)) {
                    
                    str_prepare_macro_func = result[0];
                    str_mirror_macro_func = "__mantis__mirror_" + str_prepare_macro_func;
                    // Call corresponding mirror macros
                    oss_init_end << "\n  "
                                        << str_mirror_macro_func
                                        << ";\n\n";

                    // Search remaining string of p4r_reaction_user_str
                    p4r_init_user_str = result.suffix().str();
                }
                // Mod
                str_prepare_macro_func = table_name + "_mod_" + action_name;
                std::regex e_mod (str_prepare_macro_func+"\\s*\\("+"([^\\)]*)"+"\\)");   // matches words beginning by "sub"
                while (std::regex_search (p4r_init_user_str, result, e_mod)) {
                    str_prepare_macro_func = result[0];
                    str_mirror_macro_func = "__mantis__mirror_" + str_prepare_macro_func;
                    oss_init_end << "\n  "
                                        << str_mirror_macro_func
                                        << ";\n\n";
                    p4r_init_user_str = result.suffix().str();
                }
                // Del
                str_prepare_macro_func = table_name + "_del_" + action_name;
                std::regex e_del (str_prepare_macro_func+"\\s*\\("+"([^\\)]*)"+"\\)");   // matches words beginning by "sub"
                while (std::regex_search (p4r_init_user_str, result, e_del)) {
                    str_prepare_macro_func = result[0];
                    str_mirror_macro_func = "__mantis__mirror_" + str_prepare_macro_func;
                    oss_init_end << "\n  "
                                << str_mirror_macro_func
                                << ";\n\n";
                    p4r_init_user_str = result.suffix().str();
                }
            }   
        }
    }

    // Point __vv back to working copy for dialogue
    if(((unsigned int)ing_iso_opt) & 0b10) {
        oss_init_end<< "\n  __mantis__flip_vv_ing;\n\n"; 
    }
    if(((unsigned int)egr_iso_opt) & 0b10) {
        oss_init_end<< "\n  __mantis__flip_vv_egr;\n\n";
    }  
}


void generateDialogueEnd(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_update, int ing_iso_opt, int egr_iso_opt) {

    // __vv points to shallow copy under isolation, now update version bit commit together with other mbls)
    oss_reaction_update << "\n  __mantis__mod_vars_ing;\n";
    oss_reaction_update << "\n  __mantis__mod_vars_egr;\n";

    // Flip __vv to point to shallow copy
    if(((unsigned int)ing_iso_opt) & 0b10) {
        oss_reaction_update << "\n  __mantis__flip_vv_ing;\n\n";
    }
    if(((unsigned int)egr_iso_opt) & 0b10) {
        oss_reaction_update << "\n  __mantis__flip_vv_egr;\n\n";
    }

    // Under isolation, call mirror macros to mirror shallow copies for mbl table operations
    // Get the reaction string and find all user macros
    P4RReactionNode * react_node = findReaction(nodeArray);
    string p4r_reaction_user_str = "";
    if (react_node != 0) {
        p4r_reaction_user_str = react_node->body_->toString();
    }

    // No need to check isolation opt and ing/egr, just mirror user-specified operations in prepare
    for (auto node : nodeArray) {
        // For each mbl table
        if(typeContains(node, "P4RMalleableTableNode")) {
            TableNode* table = dynamic_cast<P4RMalleableTableNode*>(node)->table_;
            string table_name = *(table->name_->word_);

            TableActionStmtsNode* actions = table->actions_;

            // Mbl tables are always with With matches (at least match on __vv)
            // For each action
            for (TableActionStmtNode* tas : *actions->list_) {
                string action_name = *tas->name_->word_;

                string str_prepare_macro_func;
                string str_mirror_macro_func;

                std::smatch result;

                // Construct match regrex for add
                str_prepare_macro_func = table_name + "_add_" + action_name;
                std::regex e_add (str_prepare_macro_func+"\\s*\\("+"([^\\)]*)"+"\\)");   // matches words beginning by "sub"

                // Stop after first match
                while (std::regex_search (p4r_reaction_user_str, result, e_add)) {
                    
                    str_prepare_macro_func = result[0];
                    str_mirror_macro_func = "__mantis__mirror_" + str_prepare_macro_func;
                    // Call corresponding mirror macros
                    oss_reaction_update << "\n  "
                                        << str_mirror_macro_func
                                        << ";\n\n";

                    // Search remaining string of p4r_reaction_user_str
                    p4r_reaction_user_str = result.suffix().str();
                }
                // Mod
                str_prepare_macro_func = table_name + "_mod_" + action_name;
                std::regex e_mod (str_prepare_macro_func+"\\s*\\("+"([^\\)]*)"+"\\)");   // matches words beginning by "sub"
                while (std::regex_search (p4r_reaction_user_str, result, e_mod)) {
                    str_prepare_macro_func = result[0];
                    str_mirror_macro_func = "__mantis__mirror_" + str_prepare_macro_func;
                    oss_reaction_update << "\n  "
                                        << str_mirror_macro_func
                                        << ";\n\n";
                    p4r_reaction_user_str = result.suffix().str();
                }
                // Del
                str_prepare_macro_func = table_name + "_del_" + action_name;
                std::regex e_del (str_prepare_macro_func+"\\s*\\("+"([^\\)]*)"+"\\)");   // matches words beginning by "sub"
                while (std::regex_search (p4r_reaction_user_str, result, e_del)) {
                    str_prepare_macro_func = result[0];
                    str_mirror_macro_func = "__mantis__mirror_" + str_prepare_macro_func;
                    oss_reaction_update << "\n  "
                                        << str_mirror_macro_func
                                        << ";\n\n";
                    p4r_reaction_user_str = result.suffix().str();
                }
            }   
        }
    }

    // Point __vv back to working copy for next dialogue
    if(((unsigned int)ing_iso_opt) & 0b10) {
        oss_reaction_update << "\n  __mantis__flip_vv_ing;\n\n";
    }
    if(((unsigned int)egr_iso_opt) & 0b10) {
        oss_reaction_update << "\n  __mantis__flip_vv_egr;\n\n";
    }
}
