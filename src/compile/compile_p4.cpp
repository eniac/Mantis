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
#include <math.h>

#include "../../include/find_nodes.h"
#include "../../include/helper.h"
#include "compile_p4.h"
#include "compile_const.h"

void transformPragma(vector<AstNode*>* astNodes) {
    // For now, a simple increment of stage index
	for (auto node : *astNodes) {
        if(typeContains(node, "TableNode")) {
            if (typeContains(node, "P4R")) {
                P4RMalleableTableNode* tablenode = dynamic_cast<P4RMalleableTableNode*>(node);
                tablenode->transformPragma();
            } else {
                TableNode* tablenode = dynamic_cast<TableNode*>(node);
                tablenode->transformPragma();
            }
        }
    }    
}

int inferIngIsolationOpt(vector<AstNode*>* nodeArray) {

    int inferred_iso = -1;
    bool require_meas_iso = true;
    bool require_react_iso = true;
    vector<ReactionArgNode*> reaction_args = findReactionArgs(*nodeArray);
    int num_args = 0;
    bool has_regarg = false;
    bool has_fieldarg = false;

    for (auto ra : reaction_args) {    
        if (ra->argType_==ReactionArgNode::REGISTER) {
            if(findRegargInIng(ra, *nodeArray)) {
                num_args += 1;
                has_regarg = true;
            }
        }
        if (ra->argType_==ReactionArgNode::INGRESS_FIELD || ra->argType_==ReactionArgNode::INGRESS_MBL_FIELD) {
            num_args += 1;
            has_fieldarg = true;
        }
    }
    if (num_args <= 1) {
        require_meas_iso = false;
    } else {
        // Potentially being false after bin-packing (single packed register, 2 single element reg)
        // Also single element 32b reg could be packed with field arg into a single 64b reg
    }

    int num_mbls = 0;
    for (auto node : *nodeArray) {
        if (typeContains(node, "P4RMalleableValueNode")) {
            num_mbls += 1;
        } else if (typeContains(node, "P4RMalleableFieldNode")) {
            num_mbls += 1;
        }
    }
    PRINT_VERBOSE("Number of mbl value/field: %d\n", num_mbls);

    // Simple static analysis to differentiate the case requiring no update isolation
    P4RReactionNode * react_node = findReaction(*nodeArray);
    string user_dialogue = "";
    if (react_node != 0) {
        user_dialogue = react_node->body_->toString();
    }
    bool foundMblOperation = false;
    // If malleable table operations specified, break if any
    for (auto node : *nodeArray) {
        if(typeContains(node, "P4RMalleableTableNode")) {
            P4RMalleableTableNode* table = dynamic_cast<P4RMalleableTableNode*>(node);
            string table_name = *(table->table_->name_->word_);
            if(user_dialogue.find(table_name+"_mod")!=std::string::npos || user_dialogue.find(table_name+"_add")!=std::string::npos || user_dialogue.find(table_name+"_del")!=std::string::npos) {
                foundMblOperation = true;
                break;
            }
        }
    } 
    if (!foundMblOperation) {
        require_react_iso = false;
    }

    if (require_meas_iso && require_react_iso) {
        inferred_iso = 3;
    } else if (require_meas_iso && !require_react_iso) {
        inferred_iso = 1;
    } else if (!require_meas_iso && require_react_iso) {
        inferred_iso = 2;
    } else {
        inferred_iso = 0;
    }

    PRINT_VERBOSE("Inferred ing isolation option: %d\n", inferred_iso);
    return inferred_iso;

}

int inferEgrIsolationOpt(vector<AstNode*>* nodeArray) {

    int inferred_iso = -1;
    bool require_meas_iso = true;
    // For egr, presumbly only mv update if any
    bool require_react_iso = false;
    vector<ReactionArgNode*> reaction_args = findReactionArgs(*nodeArray);
    int num_args = 0;
    bool has_regarg = false;
    bool has_fieldarg = false;
    for (auto ra : reaction_args) {    
        if (ra->argType_==ReactionArgNode::REGISTER) {
            if(!findRegargInIng(ra, *nodeArray)) {
                num_args += 1;
                has_regarg = true;
            }            
        }
        if (ra->argType_==ReactionArgNode::EGRESS_FIELD || ra->argType_==ReactionArgNode::EGRESS_MBL_FIELD) {
            num_args += 1;
            has_fieldarg = true;
        }
    }
    if (num_args <= 1) {
        require_meas_iso = false;
    }

    if (require_meas_iso) {
        inferred_iso = 1;
    } else {
        inferred_iso = 0;
    }
    PRINT_VERBOSE("Inferred egr isolation option: %d\n", inferred_iso);
    return inferred_iso;

}

// Wrap ingress with calls to setup and finalize
bool augmentIngress(vector<AstNode*>* astNodes) {
    // Fetch ingress
    P4ExprNode* ingressNode = findIngress(*astNodes);
    if (!ingressNode) {
        cout << "Error: could not find the ingress control block.  "
             << "perhaps you didn't call it 'ingress'?" << endl;
        exit(1);
    }

    // Rename ingress
    ingressNode->name1_->word_ = new string(kOrigIngControlName);

    // Generate new ingress function that wraps original
    ostringstream oss;
    oss << "control ingress {\n"
            // Separate ing and egr for cases when queueing > PCIe latency (large packet buffer+congested link)
            << "  " << kSetmblIngControlName << "();\n"
            << "  " << kOrigIngControlName << "();\n" 
            << "  " << kSetargsIngControlName << "();\n"
            << "  " << kRegArgGateIngControlName << "();\n"            
        << "}\n\n";
    StrNode* newIngressNode = new StrNode(new string(oss.str()));

    // Inject wrapper right before original
    InputNode* firstInput = dynamic_cast<InputNode*>(ingressNode->parent_);
    InputNode* secondInput = new InputNode(firstInput->next_,
                                           firstInput->expression_);
    firstInput->next_ = secondInput;
    secondInput->parent_ = firstInput;

    firstInput->expression_ = newIngressNode;
    astNodes->push_back(secondInput);

    return true;
}

// Wrap ingress with calls to setup and finalize
bool augmentEgress(vector<AstNode*>* astNodes) {
    // Fetch egress
    P4ExprNode* egressNode = findEgress(*astNodes);
    if (!egressNode) {
        cout << "Error: could not find the egress control block.  "
             << "perhaps you didn't call it 'egress'?" << endl;
        exit(1);
    }

    // Rename egress
    egressNode->name1_->word_ = new string(kOrigEgrControlName);

    // Generate new ingress function that wraps original
    ostringstream oss;
    oss << "control egress {\n"
            << "  " << kSetmblEgrControlName << "();\n"
            << "  " << kOrigEgrControlName << "();\n" 
            << "  " << kSetargsEgrControlName << "();\n"
            << "  " << kRegArgGateEgrControlName << "();\n"
        << "}\n\n";
    StrNode* newEgressNode = new StrNode(new string(oss.str()));

    // Inject wrapper right before original
    InputNode* firstInput = dynamic_cast<InputNode*>(egressNode->parent_);
    InputNode* secondInput = new InputNode(firstInput->next_,
                                           firstInput->expression_);
    firstInput->next_ = secondInput;
    secondInput->parent_ = firstInput;

    firstInput->expression_ = newEgressNode;
    astNodes->push_back(secondInput);

    return true;
}

void transformTableWithRefRead(MblRefNode* ref, TableNode* table,
                               const P4RMalleableFieldNode& variable) {
    auto readStmtsNode = dynamic_cast<TableReadStmtsNode*>(ref->parent_->parent_);

    // Assemble list of alts
    vector<FieldNode*> alts = findAllAlts(variable);

    // Add exact match bit for the meta field of the variable
    const string variableName = *ref->name_->word_;
    ostringstream oss;
    oss << kP4rIngMetadataName << "." << variableName << kP4rIndexSuffix;
    if (!findTableReadStmt(*table, oss.str())) {
        auto newReadStmtNode =
            new TableReadStmtNode(TableReadStmtNode::EXACT,
                                  new StrNode(new string(oss.str())));
        table->reads_->push_back(newReadStmtNode);
    }

    bool first = true;
    TableReadStmtNode::MatchType matchType;
    for (FieldNode* fn : alts) {
        const string& headerName = *fn->headerName_->word_;
        const string& fieldName = *fn->fieldName_->word_;

        if (first) {
            ref->transform(headerName, fieldName);
            auto readStmt = dynamic_cast<TableReadStmtNode*>(ref->parent_);
            if (readStmt->matchType_ == TableReadStmtNode::EXACT) {
                readStmt->matchType_ = TableReadStmtNode::TERNARY;
            }
            matchType = readStmt->matchType_;

            first = false;
        } else {
            auto newStmtNode = new TableReadStmtNode(matchType, fn);
            readStmtsNode->push_back(newStmtNode);
        }
    }
}

void duplicateActions(MblRefNode* ref, ActionNode* action,
                      vector<MblRefNode*>* mblRefs,
                      vector<AstNode*>* nodeArray,
                      const P4RMalleableFieldNode& variable) {
    const string actionName = *action->name_->word_;
    const string variableName = *ref->name_->word_;

    // Assemble list of alts
    vector<FieldNode*> alts = findAllAlts(variable);

    // Instantiate the duplicate actions
    // Keep a copy of the names for when we fiddle with the table actions
    vector<string> altNames = vector<string>();
    bool first = true;
    for (FieldNode* fn : alts) {
        const string& headerName = *fn->headerName_->word_;
        const string& fieldName = *fn->fieldName_->word_;

        if (first) {
            ostringstream oss;
            oss << "__" << headerName << "__" << fieldName
                << "__" << actionName;
            altNames.push_back(oss.str());

            // Change action to the instantiated name
            *action->name_->word_ = oss.str();

            // Replace the varref inside the alt with an actual ref
            findAndTransformMblRefsInAction(action, NULL, variableName,
                                            headerName, fieldName);

            first = false;
        } else {
            ostringstream oss;
            oss << "__" << headerName << "__" << fieldName
                << "__" << actionName;
            altNames.push_back(oss.str());

            // Create action with the instantiated name
            ActionNode* newAction = action->duplicateAction(oss.str());

            // Inject Action right after the first
            InputNode* firstInput = dynamic_cast<InputNode*>(action->parent_);
            InputNode* secondInput = new InputNode(firstInput->next_, newAction);
            firstInput->next_ = secondInput;
            secondInput->parent_ = firstInput;
            newAction->parent_ = secondInput;

            // Add any newly created mblRefs from the action
            findAndTransformMblRefsInAction(newAction, mblRefs, variableName,
                                            headerName, fieldName);
        }
    }

    // Modify all tables that use the original action
    auto actionStmtMap = findTableActionStmts(*nodeArray, actionName);
    for (auto kv : actionStmtMap) {
        // Change all tables to have all possible actions
        first = true;
        for (auto altName : altNames) {
            if (first) {
                *kv.first->name_->word_ = altName;
                first = false;
            } else {
                auto newActionStmtNode =
                    new TableActionStmtNode(new NameNode(new string(altName)));
                kv.second->actions_->push_back(newActionStmtNode);
            }
        }

        // Add the alt field match to the table if it does not already exist
        ostringstream oss;
        oss << kP4rIngMetadataName << "." << variableName << kP4rIndexSuffix;

        if (!findTableReadStmt(*kv.second, oss.str())) {
            auto newReadStmtNode =
                new TableReadStmtNode(TableReadStmtNode::EXACT,
                                      new StrNode(new string(oss.str())));
            kv.second->reads_->push_back(newReadStmtNode);
        }
    }
}

void transformMalleableFieldRef(MblRefNode* ref, vector<MblRefNode*>* mblRefs,
                               vector<AstNode*>* nodeArray,
                               const P4RMalleableFieldNode& variable) {
    // malleable field references can be in (1) actions, (2) table match fields,
    // and (3) reaction arguments
    AstNode* parent = ref->parent_;
    while (parent != NULL) {
        if (typeContains(parent, "ActionNode")) {
            ActionNode* action = dynamic_cast<ActionNode*>(parent);
            duplicateActions(ref, action, mblRefs, nodeArray, variable);
            return;
        } else if (typeContains(parent, "TableNode")) {
            TableNode* table = dynamic_cast<TableNode*>(parent);
            transformTableWithRefRead(ref, table, variable);
            return;
        } else if (typeContains(parent, "P4RReactionNode")) {
            return;
        }
        parent = parent->parent_;
    }

    cout << "Syntax Error: Could not find parent of variable reference" << endl;
    exit(1);
}

void transformMalleableRefs(
            vector<MblRefNode*>* mblRefs,
            const unordered_map<string, P4RMalleableValueNode*>& mblValues,
            const unordered_map<string, P4RMalleableFieldNode*>& mblFields,
            vector<AstNode*>* nodeArray) {
    while (!mblRefs->empty()) {
        MblRefNode* varRef = mblRefs->front();
        mblRefs->erase(mblRefs->begin());

        if (varRef->transformed_) {
            // We must have processed this variable reference in a previous pass
            // e.g., multiple references in the same action
            continue;
        }

        string* varName = varRef->name_->word_;

        if (mblValues.find(*varName) != mblValues.end()) {
            // we should treat variable value references in reaction and others differently
            bool is_in_reaction = false;
            AstNode* parent = varRef->parent_;
            while (parent != NULL) {
                if (typeContains(parent, "P4RReactionNode")) {
                    is_in_reaction = true;
                    break;
                }
                parent = parent->parent_;
            }
            if (is_in_reaction) {
                // For c code, we skip the transformation
                varRef->inReaction_ = true;
            } else {
                // Otherwise, transform it to valid meta data
                varRef->transform(string(kP4rIngMetadataName), *varRef->name_->word_);
            }
        } else if (mblFields.find(*varName) != mblFields.end()) {
            // Reference to a variable field
            transformMalleableFieldRef(varRef, mblRefs, nodeArray,
                                      *mblFields.find(*varName)->second);
        } else {
            cout << "ERROR: Unknown malleable ref!!" << endl;
        }
    }
}

void transformMalleableTables(
            unordered_map<string, P4RMalleableTableNode*>* varTables, int ing_iso_opt) {
    if (((unsigned int)ing_iso_opt) & 0b10) {
        for (auto t : *varTables) {
            auto field = new FieldNode(new NameNode(new string(kP4rIngMetadataName)),
                                    new NameNode(new string("__vv")));
            auto readstmt = new TableReadStmtNode(TableReadStmtNode::EXACT, field);
            t.second->table_->reads_->list_->push_back(readstmt);
        }
    }
}

void generateExportControl(vector<AstNode*>* newNodes,
                           const vector<ReactionArgBin>& argBinsIng, const vector<ReactionArgBin>& argBinsEgr){
    ostringstream oss;
    oss << "control " << kSetargsIngControlName << " {\n";
    for (int i = 0; i < argBinsIng.size(); ++i) {
        oss << "  apply(__tiPack" << i << ");\n";
        oss << "  apply(__tiSetArgs" << i << ");\n";
    }
    oss << "}\n\n";

    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("control"),
                                           new string(kSetargsIngControlName)));

    oss.str("");
    oss << "control " << kSetargsEgrControlName << " {\n";
    for (int i = 0; i < argBinsEgr.size(); ++i) {
        oss << "  apply(__tePack" << i << ");\n";
        oss << "  apply(__teSetArgs" << i << ");\n";
    }
    oss << "}\n\n";

    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("control"),
                                           new string(kSetargsEgrControlName)));

}

void generateRegArgGateControl(vector<AstNode*>* newNodes,
                         vector<AstNode*>* nodeArray,
                         const vector<ReactionArgNode*>& reaction_args,
                         int ing_iso_opt, int egr_iso_opt) {

    // Generate control flow branching on mv bit
    ostringstream oss;
    oss << "control " << kRegArgGateIngControlName << " {\n";
    
    // An alternative is to use single reg with double the size and index elements with original_index<<1+mv
    // Though less memory overhead but larger latency
    if (((unsigned int)ing_iso_opt) & 0b1) {
        for (auto ra : reaction_args) {
            if (ra->argType_==ReactionArgNode::REGISTER && findRegargInIng(ra, *nodeArray)) {
                oss << "  if (" << kP4rIngMetadataName << ".__mv == 0 ) {\n"
                << "    apply (" << kP4rRegReplicasTablePrefix << ra->toString() << kP4rRegReplicasSuffix0 << ");\n"
                << "  }\n"
                << "  else { \n"
                << "    apply (" << kP4rRegReplicasTablePrefix << ra->toString() << kP4rRegReplicasSuffix1 << ");\n"
                << "  }\n";
            }
        }    
    }
    oss << "}\n\n";
    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("control"),
                                           new string(kRegArgGateIngControlName)));  

    oss.str("");
    oss << "control " << kRegArgGateEgrControlName << " {\n";
    
    if (((unsigned int)egr_iso_opt) & 0b1) {
        for (auto ra : reaction_args) {
            if (ra->argType_==ReactionArgNode::REGISTER && !findRegargInIng(ra, *nodeArray)) {
                oss << "  if (" << kP4rEgrMetadataName << ".__mv == 0 ) {\n"
                    << "    apply (" << kP4rRegReplicasTablePrefix << ra->toString() << kP4rRegReplicasSuffix0 << ");\n"
                    << "  }\n"
                    << "  else { \n"
                    << "    apply (" << kP4rRegReplicasTablePrefix << ra->toString() << kP4rRegReplicasSuffix1 << ");\n"
                    << "  }\n";
            }
        }         
    }
    oss << "}\n\n";    

    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("control"),
                                           new string(kRegArgGateEgrControlName)));    
}

void generateDupRegArgProg(vector<AstNode*>* newNodes,
                         vector<AstNode*>* nodeArray,
                         const vector<ReactionArgNode*>& reaction_args,
                         int ing_iso_opt, int egr_iso_opt) {
    ostringstream oss;
    vector<P4RegisterNode*> regNodes = findP4RegisterNode(*nodeArray);
    vector<P4ExprNode*> blackboxes = findBlackbox(*nodeArray);

    for (auto ra : reaction_args) {
        if (ra->argType_==ReactionArgNode::REGISTER) {
            if ((findRegargInIng(ra, *nodeArray) && (((unsigned int)ing_iso_opt) & 0b1)) || 
                (!findRegargInIng(ra, *nodeArray) && (((unsigned int)egr_iso_opt) & 0b1))
                ) {
                PRINT_VERBOSE("Duplicate for %s\n", ra->toString().c_str());
            } else {
                continue;
            }
        } else {
            continue;
        }
        for(auto reg : regNodes) {
            if(reg->name_->toString().compare(ra->toString())==0) {
                // Duplicate registers
                oss.str(""); 
                oss << "register " << reg->name_->toString() << kP4rRegReplicasSuffix0
                    << "{\n"
                    << reg->body_->toString()
                    << "}\n\n";
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("register"),
                                                   new string(reg->name_->toString()+kP4rRegReplicasSuffix0)));         
                oss.str("");
                oss << "register " << reg->name_->toString() << kP4rRegReplicasSuffix1
                    << "{\n"
                    << reg->body_->toString()
                    << "}\n\n";  
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("register"),
                                                   new string(reg->name_->toString()+kP4rRegReplicasSuffix1)));

                // Duplicate blackbox
                // Currently assuming 32b reg with only LO, for 64b, extra tstamp reg required
                oss.str("");
                oss << "blackbox stateful_alu " << kP4rRegReplicasBlackboxPrefix << reg->name_->toString() << kP4rRegReplicasSuffix0
                    << "{\n"
                    << "  reg : " << reg->name_->toString() << kP4rRegReplicasSuffix0 << ";\n"
                    << "  update_hi_1_value : register_hi + 1;\n"
                    << "  update_lo_1_value : " << kP4rIngRegMetadataName << "." << reg->name_->toString() << kP4rRegMetadataOutputSuffix << ";\n"
                    << "}\n\n";
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("blackbox"),
                                                   new string(kP4rRegReplicasBlackboxPrefix+reg->name_->toString()+kP4rRegReplicasSuffix0)));                        

                oss.str("");
                oss << "blackbox stateful_alu " << kP4rRegReplicasBlackboxPrefix << reg->name_->toString() << kP4rRegReplicasSuffix1
                    << "{\n"
                    << "  reg : " << reg->name_->toString() << kP4rRegReplicasSuffix1 << ";\n"
                    << "  update_hi_1_value : register_hi + 1;\n"
                    << "  update_lo_1_value : " << kP4rIngRegMetadataName << "." << reg->name_->toString() << kP4rRegMetadataOutputSuffix << ";\n"
                    << "}\n\n";
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("blackbox"),
                                                   new string(kP4rRegReplicasBlackboxPrefix+reg->name_->toString()+kP4rRegReplicasSuffix1)));    

                // Duplicate actions
                oss.str("");
                oss << "action " << kP4rRegReplicasActionPrefix << reg->name_->toString() << kP4rRegReplicasSuffix0 << "(){\n"
                    << "  " << kP4rRegReplicasBlackboxPrefix << reg->name_->toString() << kP4rRegReplicasSuffix0 
                    << ".execute_stateful_alu("
                    << kP4rIngRegMetadataName << "." << reg->name_->toString() << kP4rRegMetadataIndexSuffix
                    << ");\n"
                    << "}\n\n";
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("action"),
                                                   new string(kP4rRegReplicasActionPrefix+reg->name_->toString()+kP4rRegReplicasSuffix0)));    

                oss.str("");
                oss << "action " << kP4rRegReplicasActionPrefix << reg->name_->toString() << kP4rRegReplicasSuffix1 << "(){\n"
                    << "  " << kP4rRegReplicasBlackboxPrefix << reg->name_->toString() << kP4rRegReplicasSuffix1 
                    << ".execute_stateful_alu("
                    << kP4rIngRegMetadataName << "." << reg->name_->toString() << kP4rRegMetadataIndexSuffix
                    << ");\n"
                    << "}\n\n";
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("action"),
                                                   new string(kP4rRegReplicasActionPrefix+reg->name_->toString()+kP4rRegReplicasSuffix1)));    

                // Duplicate tables
                oss.str("");
                oss << "table " << kP4rRegReplicasTablePrefix << reg->name_->toString() << kP4rRegReplicasSuffix0 << "{\n"
                    << "  actions {\n"
                    << "    " << kP4rRegReplicasActionPrefix << reg->name_->toString() << kP4rRegReplicasSuffix0 << ";\n"
                    << "}\n"
                    << "  default_action: " << kP4rRegReplicasActionPrefix << reg->name_->toString() << kP4rRegReplicasSuffix0 << "();\n"
                    << "}\n\n";
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("table"),
                                                   new string(kP4rRegReplicasTablePrefix+reg->name_->toString()+kP4rRegReplicasSuffix0)));    

                oss.str("");
                oss << "table " << kP4rRegReplicasTablePrefix << reg->name_->toString() << kP4rRegReplicasSuffix1 << "{\n"
                    << "  actions {\n"
                    << "    " << kP4rRegReplicasActionPrefix << reg->name_->toString() << kP4rRegReplicasSuffix1 << ";\n"
                    << "}\n"
                    << "  default_action: " << kP4rRegReplicasActionPrefix << reg->name_->toString() << kP4rRegReplicasSuffix1 << "();\n"
                    << "}\n\n";
                newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                                   new string("table"),
                                                   new string(kP4rRegReplicasTablePrefix+reg->name_->toString()+kP4rRegReplicasSuffix1)));    

                break;
            }
        }
    }                       
}      

void augmentRegisterArgProgForIng(vector<AstNode*>* newNodes,
                         vector<AstNode*>* nodeArray,
                         const vector<ReactionArgNode*>& reaction_args,
                         int iso_opt, bool forIng) {
    string p4rRegMetadataType;
    string p4rRegMetadataName;
    if(forIng) {
        p4rRegMetadataType = string(kP4rIngRegMetadataType);
        p4rRegMetadataName = string(kP4rIngRegMetadataName);
    } else {
        p4rRegMetadataType = string(kP4rEgrRegMetadataType);
        p4rRegMetadataName = string(kP4rEgrRegMetadataName);
    }

    // Generate meta data for storing latest value of register index, value
    if (((unsigned int)iso_opt) & 0b1) {
        ostringstream oss;
        oss << "header_type "<< p4rRegMetadataType << " {\n"
            << " fields {\n";        
        vector<P4RegisterNode*> regNodes = findP4RegisterNode(*nodeArray);

        for (auto ra : reaction_args) {    
            if (ra->argType_==ReactionArgNode::REGISTER) {
                if(forIng) {
                    if(!findRegargInIng(ra, *nodeArray)) {
                        continue;
                    }
                } else {
                    if(findRegargInIng(ra, *nodeArray)) {
                        continue;
                    }
                }
                for(auto reg : regNodes) {
                    if(reg->name_->toString().compare(ra->toString())==0) {
                        oss << "  " << ra->toString() << kP4rRegMetadataOutputSuffix << "  : "<< reg->width_ << ";\n";
                        int index_width = int(ceil(log2(reg->instanceCount_)));
                        if (index_width==0) {
                            index_width = 1;
                        }
                        oss << "  " << ra->toString() << kP4rRegMetadataIndexSuffix << "  : "<< index_width << ";\n";
                        break;
                    }
                }
            }
        }        

        oss << " }" << endl;
        oss << "}" << endl;
        oss << "metadata " << p4rRegMetadataType << " "
                           << p4rRegMetadataName << ";\n\n";

        newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                               new string("metadata"),
                                               new string(p4rRegMetadataName)));

        // Transform original blackbox program 
        // Currently assume the reg to isolation has no HI member and no output instruction
        // If original blackbox has output instruction already, just mirror that the output meta data

        vector<P4ExprNode*> blackboxes = findBlackbox(*nodeArray);
        for (auto blackbox : blackboxes) {
            std::string prog_name = blackbox->name2_->toString();
            std::stringstream ss(blackbox->body_->toString());
            std::string reg_name;
            std::string item;
            while (std::getline(ss, item, ';'))
            {
                std::stringstream ss_(item);
                std::string item_;
                while (std::getline(ss_, item_, ':')) {
                    boost::algorithm::trim(item_);
                    if(item_.compare("reg")==0) {
                        std::getline(ss_, item_, ':');
                        boost::algorithm::trim(item_);
                        reg_name = item_;
                    }
                }
            }
            ostringstream oss_dst_field, oss_index_field;
            oss_dst_field << p4rRegMetadataName
                             << "."
                             << reg_name
                             << kP4rRegMetadataOutputSuffix;       
            oss_index_field << p4rRegMetadataName
                            << "."
                            << reg_name
                            << kP4rRegMetadataIndexSuffix;                                      
            ostringstream oss_cmd;
            oss_cmd << "  " << "output_value : alu_lo;\n"
                    << "  " << "output_dst : "
                    << oss_dst_field.str()
                    << ";\n";
            string newbodystr = blackbox->body_->toString()+oss_cmd.str();
            blackbox->body_ = new BodyNode(NULL, NULL, new BodyWordNode(BodyWordNode::STRING, new StrNode(new string(newbodystr))));

            // Singleton action that executes the prog
            // Locate the action that executes the stateful prog and mirror the index to meta
            bool transformed = false;
            for (auto tmp_node : *nodeArray) {
                if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
                    ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
                    string tmp_action_name = tmp_action_node->name_->toString();
                    ActionStmtsNode* actionstmts = tmp_action_node->stmts_;
                    for (ActionStmtNode* as : *actionstmts->list_) {
                        if(as->toString().find("execute_stateful_alu")!=string::npos &&
                            as->toString().find(prog_name)!=string::npos) {
                            // Extract the index or index metadata/field
                            unsigned first = as->toString().find("(");
                            unsigned last = as->toString().find(")");
                            string index = as->toString().substr (first+1,last-first-1);
                            
                            // Mirror the index to p4r reg metadata
                            auto tmp_args = new ArgsNode();
                            tmp_args->push_back(new BodyWordNode(
                                BodyWordNode::STRING,
                                new StrNode(new string(oss_index_field.str()))));
                            tmp_args->push_back(new BodyWordNode(
                                BodyWordNode::STRING,
                                new StrNode(new string(index))));                            
                            actionstmts->push_back(new ActionStmtNode(
                                                        new NameNode(new string("modify_field")),
                                                        tmp_args,
                                                        ActionStmtNode::NAME_ARGLIST,
                                                        NULL,
                                                        NULL
                                                    ));     
                            transformed = true;
                            break;
                        }
                    }
                    if (transformed) {
                        break;
                    }
                }
            }
        }
    }
}

static vector<ReactionArgBin> runBinPackForIng(
            vector<pair<ReactionArgNode*, int> >* argSizes, vector<AstNode*> nodeArray, bool forIng) {

    sort(argSizes->begin(), argSizes->end(),
            [](const pair<ReactionArgNode*, int>& l,
               const pair<ReactionArgNode*, int>& r) {
                if (l.second != r.second) {
                    return l.second > r.second;
                }
                return l.first > r.first;
            });

    vector<ReactionArgBin> bins;
    for (const pair<ReactionArgNode*, int>& p : *argSizes) {
        assert(p.second <= REGISTER_SIZE);

        // Packing
        if(forIng) {
            if(p.first->argType_!=ReactionArgNode::INGRESS_FIELD && p.first->argType_!=ReactionArgNode::INGRESS_MBL_FIELD) {
                continue;
            }
        } else {
            if(p.first->argType_!=ReactionArgNode::EGRESS_FIELD && p.first->argType_!=ReactionArgNode::EGRESS_MBL_FIELD) {
                continue;
            }
        }

        bool added = false;
        for (ReactionArgBin& bin : bins) {
            if (p.second + bin.second <= REGISTER_SIZE) {
                bin.first.push_back(p);
                bin.second += p.second;
                added = true;
                break;
            }
        }

        if (!added) {
            vector<ReactionArgSize> newBin;
            newBin.push_back(p);
            bins.emplace_back(move(newBin), p.second);
        }
    }

    return bins;
}

static void generateArgRegistersForIng(vector<AstNode*>* newNodes,
                          const vector<ReactionArgBin>& bins, int ing_iso_opt, bool forIng) {
    string p4rArgHdrName;
    string p4rMetaName;
    string p4rSetArgsTableNameBase;
    string p4rSetArgsActionNameBase;
    string p4rSetArgsBlackboxNameBase;
    string p4rSetArgsRegNameBase;
    if(forIng) {
        p4rArgHdrName = std::string(kP4rIngArghdrName);
        p4rMetaName = std::string(kP4rIngMetadataName);
        p4rSetArgsTableNameBase = "__tiSetArgs";
        p4rSetArgsActionNameBase = "__aiSetArgs";
        p4rSetArgsBlackboxNameBase = "__biSetArgs";
        p4rSetArgsRegNameBase = "__riSetArgs";
    } else {
        p4rArgHdrName = std::string(kP4rEgrArghdrName);
        p4rMetaName = std::string(kP4rEgrMetadataName);
        p4rSetArgsTableNameBase = "__teSetArgs";
        p4rSetArgsActionNameBase = "__aeSetArgs";
        p4rSetArgsBlackboxNameBase = "__beSetArgs";
        p4rSetArgsRegNameBase = "__reSetArgs";
    }

    for (int i = 0; i < bins.size(); ++i) {
        ostringstream oss;
        oss << "table " << p4rSetArgsTableNameBase << i << " {\n"
            << "  actions { " << p4rSetArgsActionNameBase << i << "; }\n"
            << "  default_action : " << p4rSetArgsActionNameBase << i << "();\n"
            << "}\n\n"
            << "action " << p4rSetArgsActionNameBase << i << "() {\n";
        if (((unsigned int)ing_iso_opt) & 0b1) {
            oss << "  " << p4rSetArgsBlackboxNameBase << i << ".execute_stateful_alu("<< p4rMetaName << ".__mv);\n";
        } else {
            oss << "  " << p4rSetArgsBlackboxNameBase << i << ".execute_stateful_alu(0);\n";
        }
        oss << "}\n\n"
            << "blackbox stateful_alu " << p4rSetArgsBlackboxNameBase << i << " {\n"
            << "  reg : " << p4rSetArgsRegNameBase << i << ";\n"
            << "  update_lo_1_value : " << p4rArgHdrName
                                       << ".reg" << i << ";\n"
            << "}\n\n"
            << "register " << p4rSetArgsRegNameBase << i << " {\n"
            << "  width : " << bins[i].second << ";\n"
            << "  instance_count : 2;\n"
            << "}\n\n";

        auto newSetArgsNode = new UnanchoredNode(new string(oss.str()),
                                                 new string("table"),
                                                 new string("__tiSetArgs"));
        newNodes->push_back(newSetArgsNode);
    }
}


static vector<MblRefNode*> generatePackingTablesForIng(vector<AstNode*>* newNodes,
                                          const vector<ReactionArgBin>& bins, bool forIng) {
    vector<MblRefNode*> reactionArgRefs;

    string p4rArgHdrType;
    string p4rArgHdrName;
    string p4rPackTableNameBase;
    string p4rPackActionNameBase;
    string p4rPackFlcNameBase;
    string p4rPackFlNameBase;
    if(forIng) {
        p4rArgHdrType = std::string(kP4rIngArghdrType);
        p4rArgHdrName = std::string(kP4rIngArghdrName);
        p4rPackTableNameBase = "__tiPack";
        p4rPackActionNameBase = "__aiPack";
        p4rPackFlcNameBase = "__flci_packedArgs_reg";
        p4rPackFlNameBase = "__fli_packedArgs_reg";
    } else {
        p4rArgHdrType = std::string(kP4rEgrArghdrType);
        p4rArgHdrName = std::string(kP4rEgrArghdrName);
        p4rPackTableNameBase = "__tePack";
        p4rPackActionNameBase = "__aePack";
        p4rPackFlcNameBase = "__flce_packedArgs_reg";
        p4rPackFlNameBase = "__fle_packedArgs_reg";
    }

    // Generate packed fields
    ostringstream oss_fld;
    oss_fld << "header_type " << p4rArgHdrType << " {\n"
            << "  fields {\n";
    for (int i = 0; i < bins.size(); ++i) {
        oss_fld << "  reg" << i << " : " << bins[i].second << ";\n";
    }
    oss_fld << " }\n"
            << "}\n\n"
            << "metadata " << p4rArgHdrType << " "
                           << p4rArgHdrName << ";\n\n";
    auto packedMetaNode = new UnanchoredNode(
            new string(oss_fld.str()), new string("metadata"),
            new string(p4rArgHdrType));
    newNodes->push_back(packedMetaNode);

    // Synthesize table/action for each pack
    for (int i = 0; i < bins.size(); ++i) {
        // Generate packing table
        auto tiPackName = new NameNode(new string(p4rPackTableNameBase+std::to_string(i)));
        auto tiPackReads = new TableReadStmtsNode();
        auto tiPackActions = new TableActionStmtsNode();
        tiPackActions->push_back(
                new TableActionStmtNode(new NameNode(new string(p4rPackActionNameBase+std::to_string(i)))));
        auto tiPackTable = new TableNode(tiPackName, tiPackReads, tiPackActions,
                                        "  default_action : "+p4rPackActionNameBase+std::to_string(i)+"();\nsize:1;", "");
        newNodes->push_back(tiPackTable);

        // Generate packing action
        auto aiPackName = new NameNode(new string(p4rPackActionNameBase+std::to_string(i)));
        auto aiPackParams = new ActionParamsNode();
        auto aiPackStmts = new ActionStmtsNode(); 

        // Actual size of the reg (<= 32)
        int regsize = 0;
        // Generate field list node
        ostringstream oss_fl;
        oss_fl << "field_list " << p4rPackFlNameBase << i << "{\n";
        for (int j = 0; j < bins[i].first.size(); ++j) {

            AstNode* arg = bins[i].first[j].first->arg_;
            int width = bins[i].first[j].second;
            regsize += width;

            if (typeContains(arg, "FieldNode")) {
                oss_fl << arg->toString() << ";\n";
            } else if (typeContains(arg, "MblRefNode")) {
                oss_fl << arg->toString() << ";\n";
                // Mark this ref for future processing
                // Currently not supporting field list and hash duplication
                reactionArgRefs.push_back(dynamic_cast<MblRefNode*>(arg));
            } else {
                assert(false);
            }            

        }
        // Close field_list
        oss_fl << "\n}\n\n";
        newNodes->push_back(
            new UnanchoredNode(
                new string(oss_fl.str()), new string("field_list"),
                new string("field_list"+std::to_string(i)))
        );  

        // Generate field_list_calculation
        ostringstream oss_flc;
        oss_flc << "field_list_calculation " << p4rPackFlcNameBase << i << " {\n"
                << "  input {\n"
                << "    " << p4rPackFlNameBase << i << ";\n"
                << "  }\n"
                << "  algorithm : identity;\n"
                << "  output_width : " << regsize << ";\n"
                << "}\n\n";

        newNodes->push_back(
            new UnanchoredNode(
                new string(oss_flc.str()), new string("field_list_calc"),
                new string(p4rPackFlcNameBase+std::to_string(i)))
        ); 

        // Example: modify_field_with_hash_based_offset(dst, 0, __flce_packedArgs_reg0, 4294967296);
        // Note: alternative approach using (modify - shift_left - add_to_field) would not fit into a single action due to data plane constraints
        auto aiPackStmtName = new NameNode(new string("modify_field_with_hash_based_offset"));
        auto aiPackStmtArgs = new ArgsNode();
        // dst
        ostringstream oss_postfix;
        oss_postfix << "reg" << i;
        BodyWordNode* targetBodyWord = new BodyWordNode(
            BodyWordNode::FIELD,
                new FieldNode(
                    new NameNode(new string(p4rArgHdrName)),
                    new NameNode(new string(oss_postfix.str()))));
        aiPackStmtArgs->push_back(targetBodyWord);
        // base
        aiPackStmtArgs->push_back(
                new BodyWordNode(
                    BodyWordNode::INTEGER,
                    new IntegerNode(new string(to_string(0))))); 
        // __calc_packedArgs_regx
        aiPackStmtArgs->push_back(
                new BodyWordNode(
                    BodyWordNode::STRING,
                    new StrNode(new string(p4rPackFlcNameBase+std::to_string(i)))));             
        // size (power of 2)           
        aiPackStmtArgs->push_back(
                new BodyWordNode(
                    BodyWordNode::INTEGER,
                    new IntegerNode(new string(to_string((long long)(pow(2, regsize))))))); 

        aiPackStmts->push_back(
                new ActionStmtNode(aiPackStmtName, aiPackStmtArgs, ActionStmtNode::NAME_ARGLIST, NULL, NULL));        
        
        auto aiPackAction = new ActionNode(aiPackName, aiPackParams, aiPackStmts);
        auto aiPackActionWrapper = new InputNode(NULL, aiPackAction);
        newNodes->push_back(aiPackActionWrapper);
    }
    
    return reactionArgRefs;
}

vector<ReactionArgBin> generateIngDigestPacking(
            vector<AstNode*>* newNodes,
            const vector<ReactionArgNode*>& reaction_args,
            const HeaderDecsMap& headerDecsMap,
            const unordered_map<string, P4RMalleableValueNode*>& mblValues,
            const unordered_map<string, P4RMalleableFieldNode*>& mblFields,
            const vector<AstNode*>& astNodes, int* ing_iso_opt) {
    if (reaction_args.size() == 0) {
        return vector<ReactionArgBin>();
    }

    vector<pair<ReactionArgNode*, int> > argSizes =
            findAllReactionArgSizes(reaction_args, headerDecsMap, mblValues,
                                    mblFields);

    // Currently pack arguments into 32-bit bins
    // Note HW target could support 64-bit reg --- further reducing the number of packed regs by half (therefore latency)
    vector<ReactionArgBin> argBins = runBinPackForIng(&argSizes, astNodes, true);
    vector<MblRefNode*> mblRefs = generatePackingTablesForIng(newNodes, argBins, true);

    // Update meas isolation option
    if(argBins.size()<=1) {
        vector<ReactionArgNode*> reaction_args = findReactionArgs(astNodes);
        bool has_regarg = false;
        for (auto ra : reaction_args) {    
            if (ra->argType_==ReactionArgNode::REGISTER) {
                if(findRegargInIng(ra, astNodes)) {
                    has_regarg = true;
                }
            }    
        }
        if(!has_regarg) {
            if((*ing_iso_opt)==1) {
                *ing_iso_opt = 0;
            } else if ((*ing_iso_opt)==3) {
                *ing_iso_opt = 2;
            }
            PRINT_VERBOSE("Update ing isolation opt to %d\n", *ing_iso_opt);
        }
    }

    generateArgRegistersForIng(newNodes, argBins, *ing_iso_opt, true);

    // Redo the transformations to capture any malleable references in the generated code
    PRINT_VERBOSE("Found %d generated malleable refs for ing\n", mblRefs.size());
    transformMalleableRefs(&mblRefs, mblValues, mblFields, newNodes);

    return argBins;
}

vector<ReactionArgBin> generateEgrDigestPacking(
            vector<AstNode*>* newNodes,
            const vector<ReactionArgNode*>& reaction_args,
            const HeaderDecsMap& headerDecsMap,
            const unordered_map<string, P4RMalleableValueNode*>& mblValues,
            const unordered_map<string, P4RMalleableFieldNode*>& mblFields,
            const vector<AstNode*>& astNodes, int* egr_iso_opt) {
    if (reaction_args.size() == 0) {
        return vector<ReactionArgBin>();
    }

    // Get all reaction argument sizes
    vector<pair<ReactionArgNode*, int> > argSizes =
            findAllReactionArgSizes(reaction_args, headerDecsMap, mblValues,
                                    mblFields);

    vector<ReactionArgBin> argBins = runBinPackForIng(&argSizes, astNodes, false);
    vector<MblRefNode*> mblRefs = generatePackingTablesForIng(newNodes, argBins, false);

    // Update meas isolation
    if(argBins.size()<=1) {
        vector<ReactionArgNode*> reaction_args = findReactionArgs(astNodes);
        bool has_regarg = false;
        for (auto ra : reaction_args) {    
            if (ra->argType_==ReactionArgNode::REGISTER) {
                if(findRegargInIng(ra, astNodes)) {
                    has_regarg = true;
                }
            }    
        }
        if(!has_regarg) {
            if((*egr_iso_opt)==1) {
                *egr_iso_opt = 0;
            } else if ((*egr_iso_opt)==3) {
                *egr_iso_opt = 2;
            }
            PRINT_VERBOSE("Update ing isolation opt to %d\n", *egr_iso_opt);
        }
    }

    generateArgRegistersForIng(newNodes, argBins, *egr_iso_opt, false);

    PRINT_VERBOSE("Found %d generated malleable refs for egr\n", mblRefs.size());
    transformMalleableRefs(&mblRefs, mblValues, mblFields, newNodes);

    return argBins;
}

// Generate metadata for dynamic variables
void generateMetadata(vector<AstNode*>* newNodes,
                      const unordered_map<string,
                                          P4RMalleableValueNode*>& mblValues,
                      const unordered_map<string,
                                          P4RMalleableFieldNode*>& mblFields,
                      int ing_iso_opt, int egr_iso_opt) {
    ostringstream oss;
    // Ing
    oss << "header_type "<< kP4rIngMetadataType << " {\n"
        << "  fields {\n";
    if (((unsigned int)ing_iso_opt) & 0b1) {
        oss << "  __mv : 1;\n";
    } 
    if (((unsigned int)ing_iso_opt) & 0b10) {
        oss << "  __vv : 1;\n";        
    }

    // Presume malleables at ing
    for (auto kv : mblValues){
        // Values need to be as wide as specified
        VarWidthNode* widthNode =
                dynamic_cast<VarWidthNode*>(kv.second->varWidth_);
        string varWidth = widthNode->val_->toString();
        oss << "  " << kv.first << " : " << varWidth << ";" << endl;
    }
    for (auto kv : mblFields){
        // Ref-vars only need to be wide enough to store an index
        auto alts = findAllAlts(*kv.second);
        int indexWidth = 1;
        if(alts.size()!=1) {
            indexWidth = int(ceil(log2(alts.size())));
        }
        oss << "  " << kv.first << kP4rIndexSuffix << " : "
            << indexWidth << ";" << endl;
    }

    oss << " }" << endl;
    oss << "}" << endl;
    oss << "metadata " << kP4rIngMetadataType << " "
                       << kP4rIngMetadataName << ";\n\n";

    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("metadata"),
                                           new string(kP4rIngMetadataName)));
    // Egr
    oss.str("");
    oss << "header_type "<< kP4rEgrMetadataType << " {\n"
        << "  fields {\n";
    if (((unsigned int)egr_iso_opt) & 0b1) {
        oss << "  __mv : 1;\n";
    }

    oss << " }" << endl;
    oss << "}" << endl;
    oss << "metadata " << kP4rEgrMetadataType << " "
                       << kP4rEgrMetadataName << ";\n\n";

    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("metadata"),
                                           new string(kP4rEgrMetadataName)));    
}

// Generate a merged table that sets all mbls
int generateIngInitTable(vector<AstNode*>* newNodes,
                       const unordered_map<string,
                                           P4RMalleableValueNode*>& mblValues,
                       const unordered_map<string,
                                           P4RMalleableFieldNode*>& mblFields,
                       int ing_iso_opt) {

    // As the set of mbls in practice is small, currently a monolithic init for ing or egr
    ostringstream oss;
    oss << "action " << kP4rIngInitAction<< "(";
    bool first = true;
    bool has_var = false;
    int num_vars = 0;
    for (auto kv : mblValues) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        has_var = true;
        oss << "p__" << kv.first;
        num_vars += 1;
    }
    for (auto kv : mblFields) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        has_var = true;
        oss << "p__" << kv.first << kP4rIndexSuffix;
        num_vars += 1;
    }

    if(has_var && ing_iso_opt != 0) {
        oss << ", ";
    }

    if(ing_iso_opt == 1) {
        oss << "__mv";
        num_vars += 1;
    } else if(ing_iso_opt == 2) {
        oss << "__vv";
        num_vars += 1;
    } else if (ing_iso_opt == 3) {
        oss << "__mv , __vv";
        num_vars += 2;
    }
    PRINT_VERBOSE("Number of ing malleables to set: %d\n", num_vars);

    oss << ") {\n";

    for (auto kv : mblValues) {
        oss << "  modify_field(" << kP4rIngMetadataName << "." << kv.first
                                << ", p__" << kv.first << ");\n";
    }

    for (auto kv : mblFields) {
        oss << "  modify_field(" << kP4rIngMetadataName << "."
                                << kv.first << kP4rIndexSuffix
                                << ", p__" << kv.first << kP4rIndexSuffix
                                << ");\n";
    }

    if(((unsigned int)ing_iso_opt) & 0b1) {
        oss << "  modify_field(" << kP4rIngMetadataName << "."
                                << "__mv, __mv"
                                << ");\n";
    } 
    if (((unsigned int)ing_iso_opt) & 0b10) {
        oss << "  modify_field(" << kP4rIngMetadataName << "."
                                << "__vv, __vv"
                                << ");\n";
    } 

    // Typically, pragma stage 0 not required
    oss << "}\n\n"
        << "table __tiSetVars {\n"
        << "  actions {\n"
        << "    " << kP4rIngInitAction<< ";\n"
        << "  }\n"
        << "  size : 1;\n"
        << "}\n\n";

    string* codeStr = new string(oss.str());
    string* objType = new string("table");
    string* tableName = new string("__tiSetVars");

    newNodes->push_back(new UnanchoredNode(codeStr, objType, tableName));

    return num_vars;
}

void generateEgrInitTable(vector<AstNode*>* newNodes,
                       const unordered_map<string,
                                           P4RMalleableValueNode*>& mblValues,
                       const unordered_map<string,
                                           P4RMalleableFieldNode*>& mblFields,
                       int egr_iso_opt) {
    ostringstream oss;
    oss << "action " << kP4rEgrInitAction << "(";

    if(((unsigned int)egr_iso_opt) & 0b1) {
        oss << "__mv";
    }

    oss << ") {\n";

    if(((unsigned int)egr_iso_opt) & 0b1) {
        oss << " modify_field(" << kP4rEgrMetadataName << "."
                                << "__mv, __mv"
                                << ");\n";
    }

    oss << "}\n\n"
        << "table __teSetVars {\n"
        << "  actions {\n"
        << "    " << kP4rEgrInitAction<< ";\n"
        << "  }\n"
        << "  size : 1;\n"
        << "}\n\n";

    string* codeStr = new string(oss.str());
    string* objType = new string("table");
    string* tableName = new string("__teSetVars");

    newNodes->push_back(new UnanchoredNode(codeStr, objType, tableName));

}

void generateSetvarControl(vector<AstNode*>* newNodes) {
    ostringstream oss;
    oss << "control "<< kSetmblIngControlName << " {\n";
    oss << "  apply(__tiSetVars);\n";
    oss << "}\n\n";
    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("control"),
                                           new string(kSetmblIngControlName)));

    oss.str("");
    oss << "control "<< kSetmblEgrControlName << " {\n";
    oss << "  apply(__teSetVars);\n";
    oss << "}\n\n";
    newNodes->push_back(new UnanchoredNode(new string(oss.str()),
                                           new string("control"),
                                           new string(kSetmblEgrControlName)));

}