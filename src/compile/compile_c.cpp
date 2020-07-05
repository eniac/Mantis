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

UnanchoredNode* generatePrologueNode(std::vector<AstNode*> nodeArray, ostringstream& oss_mbl_init) {

    P4RInitBlockNode * init_node = findInitBlock(nodeArray);

    string * prologue_str = NULL;
    if (init_node != 0) {
        prologue_str = new string(
            str(boost::format(kPrologueT) % oss_mbl_init.str() % init_node -> body_ -> toString()));
    } else {
        // if no init_block
        prologue_str = new string(
            str(boost::format(kPrologueT) % oss_mbl_init.str() % ""));
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
                    string prefix_str, int iso_opt) {
    for (int i = 0; i < bins.size(); ++i) {
        // Applies to both cases with/without isolation by indexing __mv
        oss_reaction_mirror << str(boost::format(kFieldArgPollT) % std::to_string(bins[i].second) % std::to_string(i) % prefix_str);

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
            oss_reaction_mirror << "\n  uint"
                                << width
                                << "_t "
                                << field_arg_c
                                << "="
                                << "__mantis__values_"
                                << i
                                << "[1]&"
                                << oss_mask_tmp.str()
                                << ";";    
            oss_reaction_mirror << "\n  __mantis__values_"
                                << i
                                << "[1]>>"
                                << width
                                << ";";                                          
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
                        oss_reaction_mirror << str(boost::format(kRegArgIsoMirrorT) % ra->arg_->toString() % std::to_string(target_reg->width_) % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(std::stoi(ra->index2_->toString())));
                    } else {
                        oss_reaction_mirror << str(boost::format(kRegArgIsoMirrorT) % ra->arg_->toString() % std::to_string(target_reg->width_) % std::to_string(target_reg->instanceCount_) % prefix_str % std::to_string(target_reg->instanceCount_));
                    }
                } else {
                    PANIC("WARNING: Invalid register argument\n");
                }
            }
        }

        // After all arg mirroring, flip mv
        oss_reaction_mirror << "  __mantis__flip_mv;\n";
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
            oss_reaction_mirror << "  __mantis__flip_vv;\n"
                                << "  __mantis__var_updated=1;\n";
        }
    }
}

void generateMacroXorVersionBits(ostringstream& oss_reaction_mirror, ostringstream& oss_preprocessor, int iso_opt) {
    // Remember the current version bit values for reaction and measurement
    // We always pack field args and use mantis_mv_bit to index the replicas
    oss_reaction_mirror << "  static unsigned int __mantis__mv = 0;\n";
    if(((unsigned int)iso_opt) & 0b1) {
        // Create macros for flip version bit
        oss_preprocessor << "\n#define "
                        << "__mantis__flip_mv "
                        << "__mantis__mv=__mantis__mv^0x1"
                        << "\n";        
    } 
    if (((unsigned int)iso_opt) & 0b10) {
        oss_reaction_mirror << "  static unsigned int __mantis__vv = 0x0;\n";
        oss_preprocessor << "\n#define "
                        << "__mantis__flip_vv "
                        << "__mantis__vv=__mantis__vv^0x1"
                        << "\n";         
    }
}

// Prepare helper vars and set mv gate bit
void generateDialogueArgStart(ostringstream& oss_reaction_mirror, int isolation_opt) {

    oss_reaction_mirror << "  int __mantis__reg_flags=1;\n"
                       << "  int __mantis__value_count;\n"
                       << "  int __mantis__num_actually_read;\n"
                       << "  int __mantis__i;\n";

    if(((unsigned int)isolation_opt) & 0b1) {
        oss_reaction_mirror << "  __mantis__var_updated=1;\n"
                           // Update data plane working copy
                           << "  __mantis__flip_mv;\n"
                           << "  __mantis__mod_vars;\n"
                           << "  __mantis__flip_mv;\n"
                           << "  __mantis__var_updated=0;\n";  
    }

}

// Synthesizing macros for non-mbl table manipulations
// These operations are only used at prologue as no isolation provided
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

                ////////////////////////////////
                // Delete operation: only need to specify the entry index
                oss_macro_tmp.str("");
                oss_replace_tmp.str("");
                oss_macro_tmp << table_name
                            << "_del(ARG_INDEX)";
                oss_replace_tmp << "\t__mantis__status_tmp="
                                << prefix_str
                                << table_name
                                << "_table_delete"
                                << "(sess_hdl,pipe_mgr_dev_tgt.device_id,handlers[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]);\\\n"
                                << kMantisNl;
                oss_preprocessor << "\n#define "
                                << oss_macro_tmp.str()
                                << " "
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr;   

                // Add and modify operations are attached to a specific action
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
                    // Declare action spec data struct
                    oss_replace_tmp << "\t"
                                    << prefix_str
                                    << action_name
                                    << "_action_spec_t "
                                    << action_name
                                    << "_action_spec_##ARG_INDEX;\\\n"
                                    << kMantisNl;
                    for (auto tmp_node : nodeArray) {
                        if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
                            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                            string tmp_action_name = tmp_action_node->name_->toString();
                            if(tmp_action_name.compare(action_name)==0) {
                                ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                                int action_arg_index = 0;
                                for (ActionParamNode* apn : *tmp_action_params->list_) {
                                    oss_macro_tmp << ",ARG_ACTION_"
                                                << std::to_string(action_arg_index);
                                    oss_replace_tmp << "\t"
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
                                    << "_match_spec_##ARG_INDEX,priority_##ARG_INDEX,&"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX,&handlers[ARG_INDEX+"
                                    << std::to_string(kHandlerOffset)
                                    << "]);\\\n"
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
                    oss_replace_tmp << "\t"
                                    << prefix_str
                                    << action_name
                                    << "_action_spec_t "
                                    << action_name
                                    << "_action_spec_##ARG_INDEX;\\\n"
                                    << kMantisNl;
                    for (auto tmp_node : nodeArray) {
                        if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
                            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                            string tmp_action_name = tmp_action_node->name_->toString();
                            if(tmp_action_name.compare(action_name)==0) {
                                ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                                int action_arg_index = 0;
                                for (ActionParamNode* apn : *tmp_action_params->list_) {
                                    oss_macro_tmp << ",ARG_ACTION_"
                                                << std::to_string(action_arg_index);
                                    oss_replace_tmp << "\t"
                                                    << action_name
                                                    << "_action_spec_##ARG_INDEX.action_"
                                                    << apn->toString()
                                                    << "=ARG_ACTION_"
                                                    << std::to_string(action_arg_index)
                                                    << ";\\\n"
                                                    << kMantisNl;
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
                                    << "(sess_hdl,pipe_mgr_dev_tgt.device_id,handlers[ARG_INDEX+"
                                    << std::to_string(kHandlerOffset)
                                    << "],&"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX);\\\n"
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
                        if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
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
                                        << "&handlers[ARG_INDEX+"
                                        << std::to_string(kHandlerOffset)
                                        << "]);\\\n"
                                        << kMantisNl;
                    } else {
                        oss_replace_tmp << "\t__mantis__status_tmp="
                                        << prefix_str
                                        << table_name
                                        << "_set_default_action_"
                                        << action_name
                                        << "(sess_hdl,pipe_mgr_dev_tgt,&"
                                        << action_name
                                        << "_action_spec,&handlers[ARG_INDEX+"
                                        << std::to_string(kHandlerOffset)
                                        << "]);\\\n"
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
void generateMacroMblTable(std::vector<AstNode*> nodeArray, ostringstream& oss_preprocessor, string prefix_str, int iso_opt) {

    ostringstream oss_macro_tmp;
    ostringstream oss_replace_tmp;
    for (auto node : nodeArray) {
        if(typeContains(node, "P4RMalleableTableNode")) {
            TableNode* table = dynamic_cast<P4RMalleableTableNode*>(node)->table_;
            string table_name = *(table->name_->word_);

            TableActionStmtsNode* actions = table->actions_;
            TableReadStmtsNode* reads = table->reads_;

            // Only applies to tables with reads 
            if(reads) {
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

                ////////////////////////////////
                // Delete operation
                oss_macro_tmp.str("");
                oss_replace_tmp.str("");
                oss_macro_tmp << table_name
                            << "_del(ARG_INDEX)";
                oss_replace_tmp << "\t__mantis__status_tmp="
                                << prefix_str
                                << table_name
                                << "_table_delete"
                                << "(sess_hdl,pipe_mgr_dev_tgt.device_id,handlers[ARG_INDEX+"
                                << std::to_string(kHandlerOffset)
                                << "]);\\\n"
                                << kMantisNl;
                oss_preprocessor << "\n#define "
                                << oss_macro_tmp.str()
                                << " "
                                << oss_replace_tmp.str()
                                << "\t"
                                << kErrorCheckStr;   

                // Add and modify operations are attached to a specific action
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
                                if(match_field_name.compare(string(kP4rIngMetadataName)+"_"+"__vv")==0) {
                                    oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name+"="
                                                << "__mantis__vv"
                                                << ";\\\n"
                                                << kMantisNl;
                                    // Note programmar does not provide the argument
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
                                if(match_field_name.compare(string(kP4rIngMetadataName)+"_"+"__vv")==0) {
                                    oss_replace_tmp << "\t"
                                                << table_name
                                                << "_match_spec_##ARG_INDEX."
                                                << match_field_name+"="
                                                << "__mantis__vv"
                                                << ";\\\n"
                                                << kMantisNl;
                                    // Note programmar does not provide the argument
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
                    // Declare action spec data struct
                    oss_replace_tmp << "\t"
                                    << prefix_str
                                    << action_name
                                    << "_action_spec_t "
                                    << action_name
                                    << "_action_spec_##ARG_INDEX;\\\n"
                                    << kMantisNl;
                    for (auto tmp_node : nodeArray) {
                        if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
                            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                            string tmp_action_name = tmp_action_node->name_->toString();
                            if(tmp_action_name.compare(action_name)==0) {
                                ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                                int action_arg_index = 0;
                                for (ActionParamNode* apn : *tmp_action_params->list_) {
                                    oss_macro_tmp << ",ARG_ACTION_"
                                                << std::to_string(action_arg_index);
                                    oss_replace_tmp << "\t"
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
                                    << "_match_spec_##ARG_INDEX,priority_##ARG_INDEX,&"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX,&handlers[ARG_INDEX+"
                                    << std::to_string(kHandlerOffset)
                                    << "]);\\\n"
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
                    for (auto tmp_node : nodeArray) {
                        if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
                            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                            string tmp_action_name = tmp_action_node->name_->toString();
                            if(tmp_action_name.compare(action_name)==0) {
                                ActionParamsNode* tmp_action_params = tmp_action_node->params_;
                                int action_arg_index = 0;
                                for (ActionParamNode* apn : *tmp_action_params->list_) {
                                    oss_macro_tmp << ",ARG_ACTION_"
                                                << std::to_string(action_arg_index);
                                    oss_replace_tmp << "\t"
                                                    << action_name
                                                    << "_action_spec_##ARG_INDEX.action_"
                                                    << apn->toString()
                                                    << "=ARG_ACTION_"
                                                    << std::to_string(action_arg_index)
                                                    << ";\\\n"
                                                    << kMantisNl;
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
                                    << "(sess_hdl,pipe_mgr_dev_tgt.device_id,handlers[ARG_INDEX+"
                                    << std::to_string(kHandlerOffset)
                                    << "],&"
                                    << action_name
                                    << "_action_spec_##ARG_INDEX);\\\n"
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
                        if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
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
                                        << "&handlers[ARG_INDEX+"
                                        << std::to_string(kHandlerOffset)
                                        << "]);\\\n"
                                        << kMantisNl;
                    } else {
                        oss_replace_tmp << "\t__mantis__status_tmp="
                                        << prefix_str
                                        << table_name
                                        << "_set_default_action_"
                                        << action_name
                                        << "(sess_hdl,pipe_mgr_dev_tgt,&"
                                        << action_name
                                        << "_action_spec,&handlers[ARG_INDEX+"
                                        << std::to_string(kHandlerOffset)
                                        << "]);\\\n"
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

// Generate mantis-syntax single mbl mod macros and __mantis__add_vars, __mantis__mod_vars
void generateMacroInitMbls(std::vector<AstNode*> nodeArray, ostringstream& oss_mbl_init, ostringstream& oss_reaction_mirror, 
                             ostringstream& oss_preprocessor, int num_mbls, int iso_opt, string prefix_str) {

    ostringstream oss_replace_mantis_add_vars;
    oss_mbl_init << "  int __mantis__var_updated=0;\n";

    // Same as oss_replace_mantis_add_vars for monolithic master init table (even with update isolation)
    ostringstream oss_replace_mantis_mod_vars;
    oss_reaction_mirror << "  int __mantis__var_updated=0;\n";

    oss_replace_mantis_add_vars << "if(__mantis__var_updated==1) {\\\n"
                                << kMantisNl;
    oss_replace_mantis_mod_vars << "if(__mantis__var_updated==1) {\\\n"
                                << kMantisNl;

    // num_mbls includes version bits as well
    if(num_mbls!=0) {
        // Declare p4r_init action variable
        // Here the conditional excludes the version bits
        oss_replace_mantis_add_vars << "\t"
                                    << prefix_str
                                    << kP4rIngInitAction
                                    << "_"
                                    << kActionSuffixStr
                                    << " "
                                    << kP4rIngInitAction
                                    << ";\\\n"
                                    << kMantisNl;

        oss_replace_mantis_mod_vars << "\t"
                                    << prefix_str
                                    << kP4rIngInitAction
                                    << "_"
                                    << kActionSuffixStr
                                    << " "
                                    << kP4rIngInitAction
                                    << ";\\\n"
                                    << kMantisNl;                                    
    }
    // Initialize version bits
    if(((unsigned int)iso_opt) & 0b1) {
        oss_mbl_init << "  unsigned int __mantis__mv = 0x0;\n";

        oss_replace_mantis_add_vars << "\t"
                                    << kP4rIngInitAction
                                    << ".action___mv"
                                    << "=__mantis__mv"
                                    << ";\\\n"
                                    << kMantisNl;  
                                    
        oss_replace_mantis_mod_vars << "\t"
                                    << kP4rIngInitAction
                                    << ".action___mv"
                                    << "=__mantis__mv"
                                    << ";\\\n"
                                    << kMantisNl;
    } else if(((unsigned int)iso_opt) & 0b10) {
        oss_replace_mantis_add_vars << "\t"
                                    << kP4rIngInitAction
                                    << ".action___vv"
                                    << "=__mantis__vv"
                                    << ";\\\n"
                                    << kMantisNl;
        oss_reaction_mirror << "  static unsigned int __mantis__vv"
                            << "=0x0;\n";        
        oss_replace_mantis_mod_vars << "\t"
                                    << kP4rIngInitAction
                                    << ".action___vv"
                                    << "=__mantis__vv"
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

            oss_replace_mantis_add_vars << "\t"
                                        << kP4rIngInitAction
                                        << ".action_p__"
                                        << var_name
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;
            oss_replace_mantis_mod_vars << "\t"
                                        << kP4rIngInitAction
                                        << ".action_p__"
                                        << var_name
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;

            oss_mbl_init << "  int __mantis__"
                              << var_name
                              << ";\n";
            // Prepare static C variables to remember the previous value
            // As each call for modification involves providing values for all variables            
            oss_reaction_mirror << "  static int __mantis__"
                               << var_name
                               << "=-1;\n";

            auto init_tmp = dynamic_cast<VarInitNode*>(v->varInit_);
            string var_init_val = init_tmp->val_->toString();
            oss_mbl_init << "  __mantis__mod_var_"
                              << var_name
                              << "("
                              << var_init_val
                              << ");\n";

            oss_preprocessor << "\n#define ";
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
                             << "\t__mantis__var_updated=1;"
                             << kMantisNl;  
        } else if (typeContains(n, "P4RMalleableFieldNode")) {
            auto v = dynamic_cast<P4RMalleableFieldNode*>(n);
            string var_name = *v->name_->word_;

            std::replace(var_name.begin(), var_name.end(), '.', '_');
            oss_replace_mantis_add_vars << "\t"
                                        << kP4rIngInitAction
                                        << ".action_p__"
                                        << var_name
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;            
            oss_replace_mantis_mod_vars << "\t"
                                        << kP4rIngInitAction
                                        << ".action_p__"
                                        << var_name
                                        << "=__mantis__"
                                        << var_name
                                        << ";\\\n"
                                        << kMantisNl;   

            oss_mbl_init << "  int __mantis__"
                              << var_name
                              << ";\n";
            oss_reaction_mirror << "  static int __mantis__"
                               << var_name
                               << "=-1;\n";

            auto init_tmp = dynamic_cast<VarInitNode*>(v->varInit_);
            string var_init_val = init_tmp->val_->toString();
            std::replace(var_init_val.begin(), var_init_val.end(), '.', '_');
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
                oss_preprocessor << "  __mantis__mod_var_"
                                 << var_name
                                 << "_"
                                 << tmp_str
                                 << " mantis_"
                                 << var_name
                                 << "="
                                 << std::to_string(v->mapAltToInt(tmp_alt->toString()))
                                 << ";\\\n"
                                 << kMantisNl
                                 << "\t__mantis__var_updated=1;"
                                 << kMantisNl;
            }
        }
    }

    // Init table is always there
    oss_replace_mantis_add_vars << "\t__mantis__status_tmp="
                                << prefix_str
                                << "__tiSetVars_set_default_action_"
                                << kP4rIngInitAction
                                << "(sess_hdl, pipe_mgr_dev_tgt, ";
    if(num_mbls!=0) {
        oss_replace_mantis_add_vars << "&"
                                    << kP4rIngInitAction
                                    << ", ";
    }
    oss_replace_mantis_add_vars << "&handlers["
                                << std::to_string(kInitEntryHandlerIndex)
                                << "]);\\\n"
                                << kMantisNl;
    // Define the macro mantis_add_vars                            
    oss_preprocessor << "\n#define "
                     << "__mantis__add_vars "
                     << oss_replace_mantis_add_vars.str()
                     << "\t"
                     << kErrorCheckStr
                     << " }";

    oss_replace_mantis_mod_vars << "\t__mantis__status_tmp="
                                << prefix_str
                                << "__tiSetVars_set_default_action_"
                                << kP4rIngInitAction
                                << "(sess_hdl, pipe_mgr_dev_tgt, ";
    if(num_mbls!=0) {
        oss_replace_mantis_mod_vars << "&"
                                    << kP4rIngInitAction
                                    << ", ";
    }                                
    oss_replace_mantis_mod_vars << "&handlers["
                                << std::to_string(kInitEntryHandlerIndex)
                                << "]);\\\n"
                                << kMantisNl;
    oss_preprocessor << "\n#define "
                     << "__mantis__mod_vars "
                     << oss_replace_mantis_mod_vars.str()
                     << "\t"
                     << kErrorCheckStr
                     << " }\n";

}

void generateDialogueEnd(ostringstream& oss_reaction_update, int ing_iso_opt, int egr_iso_opt) {

    // __vv points to shallow copy under isolation, now update version bit commit together with other mbls)
    oss_reaction_update << "\n  __mantis__mod_vars;";

    if(((unsigned int)ing_iso_opt) & 0b10) {
        // Flip __vv to point to shallow copy
        oss_reaction_update << "  __mantis__flip_vv;\n";
        // Under isolation, mirror user mbl table operations to shallow copies

        // Point __vv back to working copy for next dialogue
        oss_reaction_update << "  __mantis__flip_vv;\n";
    }  
}
