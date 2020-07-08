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

#include <regex>
#include <unordered_map>

#include "../../include/ast_nodes.h"
#include "../../include/ast_nodes_p4.h"
#include "../../include/ast_nodes_p4r.h"
#include "../../include/helper.h"

#include "compile_const.h"


bool typeContains(AstNode* n, const char* type) {
    if (n->nodeType_.find(string(type)) != string::npos) {
        return true;    
    } else {
        return false;
    }
}

bool p4KeywordMatches(P4ExprNode* n, const char* type) {
    if (n->keyword_->word_->compare(string(type)) == 0) {
        return true;    
    } else {
        return false;
    }
}

P4ExprNode* findIngress(const std::vector<AstNode*>& astNodes) {
    for (auto node : astNodes) {
        if (typeContains(node, "P4ExprNode")) {
            P4ExprNode* exprNode = dynamic_cast<P4ExprNode*>(node);
            // Might be called after ing transformation
            if (exprNode->name1_->toString().find("ingress") != string::npos || exprNode->name1_->toString().find(string(kOrigIngControlName)) != string::npos) {
                return exprNode;
            }
        }
    }

    return NULL;
}

P4ExprNode* findEgress(const std::vector<AstNode*>& astNodes) {
    for (auto node : astNodes) {
        if (typeContains(node, "P4ExprNode")) {
            P4ExprNode* exprNode = dynamic_cast<P4ExprNode*>(node);
            if (exprNode->name1_->toString().find("egress") != string::npos || exprNode->name1_->toString().find(string(kOrigEgrControlName)) != string::npos) {
                return exprNode;
            }
        }
    }

    return NULL;
}

vector<P4ExprNode*> findBlackbox(const std::vector<AstNode*>& astNodes) {
    vector<P4ExprNode*> ret;
    for (auto node : astNodes) {
        if (typeContains(node, "P4ExprNode")) {
            P4ExprNode* exprNode = dynamic_cast<P4ExprNode*>(node);
            if (exprNode->keyword_->toString().find("blackbox") != string::npos) {
                ret.push_back(exprNode);
            }
        }
    }
    return ret;
}

void findAndRemoveMalleables(
            unordered_map<string, P4RMalleableValueNode*>* mblValues,
            unordered_map<string, P4RMalleableFieldNode*>* mblFields,
            unordered_map<string, P4RMalleableTableNode*>* mblTables,
            const vector<AstNode*>& astNodes) {
    // Find all the Malleables
    for (auto n : astNodes) {
        if (typeContains(n, "P4RMalleableValueNode")) {
            auto v = dynamic_cast<P4RMalleableValueNode*>(n);
            mblValues->emplace(*v->name_->word_, v);
            n->removed_ = true;
        } else if (typeContains(n, "P4RMalleableFieldNode")) {
            auto v = dynamic_cast<P4RMalleableFieldNode*>(n);
            mblFields->emplace(*v->name_->word_, v);
            n->removed_ = true;
        } else if (typeContains(n, "P4RMalleableTableNode")) {
            auto v = dynamic_cast<P4RMalleableTableNode*>(n);
            mblTables->emplace(*v->table_->name_->word_, v);
            // for a Malleable table, the parser will give both MalleableTableNode and TableNode, 
            // we need to tag the latter as Malleable
            v->table_->isMalleable_ = true;
            n->removed_ = true;
        } else if (typeContains(n, "P4RInitBlockNode")) {
            n->removed_ = true;
        }
    }
}

void findMalleableRefs(vector<MblRefNode*>* varRefs,
                      const vector<AstNode*>& astNodes) {
    for (AstNode* node : astNodes) {
        if (typeContains(node, "MblRefNode")) {
            MblRefNode* varRef = dynamic_cast<MblRefNode*>(node);
            varRefs->push_back(varRef);
        }
    }
}

unordered_map<TableActionStmtNode*, TableNode*> findTableActionStmts(
            const vector<AstNode*>& astNodes, const string& actionName) {
    auto ret = unordered_map<TableActionStmtNode*, TableNode*>();

    for (auto node : astNodes) {
        if (typeContains(node, "TableNode") && !typeContains(node, "P4R")) {
            TableNode* table = dynamic_cast<TableNode*>(node);
            TableActionStmtsNode* actions = table->actions_;
            for (TableActionStmtNode* tas : *actions->list_) {
                if (*tas->name_->word_ == actionName) {
                    ret.emplace(tas, table);
                    break;
                }
            }
        }
    }
    return ret;
}

bool findTableReadStmt(const TableNode& table, const string& fieldName) {
    TableReadStmtsNode* reads = table.reads_;
    for (TableReadStmtNode* trs : *reads->list_) {
        if (typeContains(trs->field_, "StrNode")) {
            StrNode* sn = dynamic_cast<StrNode*>(trs->field_);
            if (*sn->word_ == fieldName) {
                return true;
            }
        }
    }

    return false;
}

void findAndTransformMblRefsInAction(ActionNode* action,
                                     vector<MblRefNode*>* varRefs,
                                     const string& varName,
                                     const string& altHeader,
                                     const string& altField) {

    for (ActionStmtNode* as : *action->stmts_->list_) {
        for (BodyWordNode* bw : *as->args_->list_) {
            if (bw->wordType_ != BodyWordNode::VARREF) {
                continue;
            }

            auto varRef = dynamic_cast<MblRefNode*>(bw->contents_);
            if (*varRef->name_->word_ == varName) {
                varRef->transform(altHeader, altField);
            } else if (varRefs && !varRef->transformed_) {
                varRefs->push_back(dynamic_cast<MblRefNode*>(bw->contents_));
            }
        }
    }
}

vector<FieldNode*> findAllAlts(const P4RMalleableFieldNode& malleable) {
    return vector<FieldNode*>(*malleable.varAlts_->fields_->list_);
}

P4RInitBlockNode* findInitBlock(std::vector<AstNode*> astNodes){
    // Currently get a single node, easily generalize to multiple distributed init/reaction block
	for (auto node : astNodes) {
		if (node->nodeType_.find(string("InitBlockNode")) != string::npos) {
			P4RInitBlockNode * iNode = dynamic_cast<P4RInitBlockNode *>(node);
			return iNode;
		}
	}
	return 0;
}

P4RReactionNode* findReaction(std::vector<AstNode*> astNodes){
	for (auto node : astNodes) {
		if (node->nodeType_.find(string("ReactionNode")) != string::npos) {
			P4RReactionNode * reactionNode = dynamic_cast<P4RReactionNode *>(node);
			return reactionNode;
		}
	}
	return 0;
}

vector<P4RegisterNode*> findP4RegisterNode(const vector<AstNode*>& astNodes) {
    vector<P4RegisterNode*> ret;
    for (auto node : astNodes) {
        if (typeContains(node, "P4RegisterNode")) {
            ret.push_back(dynamic_cast<P4RegisterNode*>(node));
        }
    }
    return ret;
}

vector<ReactionArgNode*> findReactionArgs(const vector<AstNode*>& astNodes) {
    vector<ReactionArgNode*> ret;
    // Find all the argument nodes
    for (auto node : astNodes) {
        if (typeContains(node, "ReactionArgNode")) {
            ret.push_back(dynamic_cast<ReactionArgNode*>(node));
        }
    }
    return ret;
}

typedef unordered_map<string /* instanceName */,
                      std::vector<FieldDecNode*>*> HeaderDecsMap;
HeaderDecsMap findHeaderDecs(const vector<AstNode*>& astNodes) {
    unordered_map<string /* type */,
                  std::vector<FieldDecNode*>*> typeDecsMap;
    for (auto node : astNodes) {
        if (typeContains(node, "HeaderTypeDeclarationNode")) {
            auto headerDecl = dynamic_cast<HeaderTypeDeclarationNode*>(node);
            typeDecsMap.emplace(headerDecl->name_->toString(),
                                headerDecl->field_decs_->list_);
        }       
    }

    HeaderDecsMap ret;
    for (auto node : astNodes) {
        if (typeContains(node, "HeaderInstanceNode")) {
            auto headerInstance = dynamic_cast<HeaderInstanceNode*>(node);
            auto fieldDecVector = typeDecsMap.at(*headerInstance->type_->word_);
            ret.emplace(*headerInstance->name_->word_, fieldDecVector);
        }
    }

    return ret;
}

vector<pair<ReactionArgNode*, int> > findAllReactionArgSizes(
            const vector<ReactionArgNode*>& reactionArgs,
            const HeaderDecsMap& headerDecsMap,
            const unordered_map<string, P4RMalleableValueNode*>& mblValues,
            const unordered_map<string, P4RMalleableFieldNode*>& mblFields) {
    vector<pair<ReactionArgNode*, int> > argSizes;

    for (auto ra : reactionArgs) {
        switch (ra->argType_) {
        case ReactionArgNode::INGRESS_FIELD:
        case ReactionArgNode::EGRESS_FIELD:
        case ReactionArgNode::INGRESS_MBL_FIELD:
        case ReactionArgNode::EGRESS_MBL_FIELD:
            if (typeContains(ra->arg_, "FieldNode")) {
                // If it's a field node, grab from headerDecsMap
                auto fieldArg = dynamic_cast<FieldNode*>(ra->arg_);
                auto fieldDecs = headerDecsMap.at(*fieldArg->headerName_->word_);
                bool found = false;

                for (FieldDecNode* fd : *fieldDecs) {
                    if (*fd->name_->word_ == *fieldArg->fieldName_->word_) {
                        argSizes.emplace_back(ra, stoi(*fd->size_->word_));
                        found = true;
                        break;
                    }
                }
                assert(found);
            } else if (typeContains(ra->arg_, "MblRefNode")) {
                // If it's a var ref, find it
                auto varRefArg = dynamic_cast<MblRefNode*>(ra->arg_);
                if (mblValues.find(*varRefArg->name_->word_) != mblValues.end()) {
                    auto size = mblValues.at(*varRefArg->name_->word_)->varWidth_->val_->word_;
                    argSizes.emplace_back(ra, stoi(*size));
                } else {
                    auto size = mblFields.at(*varRefArg->name_->word_)->varWidth_->val_->word_;
                    argSizes.emplace_back(ra, stoi(*size));
                }
            } else {
                assert(false && "Should never happen.");
            }
            break;
        case ReactionArgNode::REGISTER:
            // Not taken into account when bin-packing
            break;
        }
    }

    return argSizes;
}

void findAndRemoveReactions(vector<P4RReactionNode*>* reactions,
                            const vector<AstNode*>& astNodes) {
    // Find all the malleables
    for (auto n : astNodes) {
        if (typeContains(n, "P4RReactionNode")) {
            auto r = dynamic_cast<P4RReactionNode*>(n);
            reactions->push_back(r);
            n->removed_ = true;
        }
    }
}

bool findTblInIng(string tableName, const vector<AstNode*>& nodeArray) {
    P4ExprNode* ing_node = findIngress(nodeArray);
    P4ExprNode* egr_node = findEgress(nodeArray);
    // Currently assume that control ing/egr doesn't wrap other control blocks (otherwise requires recursive search)
    if(ing_node==NULL || egr_node==NULL) {
        PANIC("Missing ing/egr node for %s\n", tableName.c_str());
    }
    if(ing_node->body_->toString().find(tableName) != string::npos) {
        return true;
    } else if (egr_node->body_->toString().find(tableName) != string::npos) {
        return false;
    } else {
        PANIC("Failed to locate %s in ing/egr control block\n", tableName.c_str());
    }
}

bool findRegargInIng(ReactionArgNode* regarg, std::vector<AstNode*> nodeArray) {

    if(regarg->argType_!=ReactionArgNode::REGISTER) {
        PRINT_VERBOSE("Miscall findRegargInIng for arg %s\n", regarg->toString().c_str());
        return false;
    }

    vector<P4ExprNode*> blackboxes = findBlackbox(nodeArray);
    
    std::string prog_name;
    for (auto blackbox : blackboxes) {
        prog_name = blackbox->name2_->toString();
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
        if(reg_name.compare(regarg->toString())==0) {
            break;
        }
    }
    string action_name;
    bool found = false;
    for (auto tmp_node : nodeArray) {
        if(typeContains(tmp_node, "ActionNode") && !typeContains(tmp_node, "P4R")) {
            ActionNode* tmp_action_node = dynamic_cast<ActionNode*>(tmp_node);
            string tmp_action_name = tmp_action_node->name_->toString();
            ActionStmtsNode* actionstmts = tmp_action_node->stmts_;
            for (ActionStmtNode* as : *actionstmts->list_) {
                if(as->toString().find("execute_stateful_alu")!=string::npos &&
                    as->toString().find(prog_name)!=string::npos) {
                    found = true;
                    action_name = tmp_action_node->name_->toString();
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    }
    if(!found) {
        PANIC("Action missing to execute the stateful alu for %s\n", regarg->toString().c_str());
    }
    string table_name;
    found = false;
    for (auto node : nodeArray) {
        if(typeContains(node, "TableNode") && !typeContains(node, "P4R")) {
            TableNode* table = dynamic_cast<TableNode*>(node);
            table_name = *(table->name_->word_);
            TableActionStmtsNode* actions = table->actions_;
            for (TableActionStmtNode* tas : *actions->list_) {
                string tmp_action_name = *tas->name_->word_;
                if(tmp_action_name.compare(action_name)==0) {
                    found = true;
                    break;
                }
            }
            if(found) {
                break;
            }        
        }
    }
    if(!found) {
        PANIC("Table missing for %s\n", regarg->toString().c_str());
    }    
    // Now locate ing/egr for the table
    P4ExprNode* ing_node = findIngress(nodeArray);
    P4ExprNode* egr_node = findEgress(nodeArray);
    if(ing_node==NULL || egr_node==NULL) {
        PANIC("Missing ing/egr node for %s\n", regarg->toString().c_str());
    }
    // A valid table will either be applied at ing or egr
    if(ing_node->body_->toString().find(table_name) != string::npos) {
        return true;
    } else if (egr_node->body_->toString().find(table_name) != string::npos) {
        return false;
    } else {
        PANIC("Failed to locate %s for %s\n", table_name.c_str(), regarg->toString().c_str());
    }
}

int findRegargWidth(ReactionArgNode* regarg, std::vector<AstNode*> nodeArray) {
    vector<P4RegisterNode*> regNodes = findP4RegisterNode(nodeArray);
    for(auto reg : regNodes) {
        if(reg->name_->toString().compare(regarg->toString())==0) {
            return reg->width_;
        }
    }
    PANIC("Non existing reg arg %s\n", regarg->toString().c_str());
    return -1;
}