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

#include "../../include/ast_nodes_p4.h"
#include "../../include/ast_nodes_p4r.h"

using namespace std;

static string operator*(std::string str, int count)
{
    string ret;
    for(auto i = 0; i<count; i++) {
        ret = ret + str;
    }
    return ret;
}

BodyWordNode::BodyWordNode(WordType wordType, AstNode* contents)
                           : wordType_(wordType) {

    nodeType_ = typeid(*this).name();
    contents_ = contents;
    contents_->parent_ = this;
}

BodyWordNode* BodyWordNode::deepCopy() {
    AstNode* newContents = contents_;
    if (wordType_ == WordType::VARREF) {
        newContents = dynamic_cast<MblRefNode*>(contents_)->deepCopy();
    }

    auto newNode = new BodyWordNode(wordType_, newContents);
    return newNode;
}

string BodyWordNode::toString() {
    ostringstream oss; 
    if (wordType_ == WordType::INTEGER || wordType_ == WordType::SPECIAL) {
        oss << contents_->toString();
    } else if (dynamic_cast<BodyNode*>(parent_) != NULL && dynamic_cast<BodyNode*>(parent_)->bodyOuter_ != NULL && 
        dynamic_cast<BodyNode*>(parent_)->bodyOuter_->str_ != NULL && dynamic_cast<BodyNode*>(parent_)->bodyOuter_->str_->wordType_ == WordType::SPECIAL &&
        dynamic_cast<BodyNode*>(parent_)->bodyOuter_->str_->contents_->toString() != ";" ) {
        oss << contents_->toString();
    } else if (dynamic_cast<BodyNode*>(parent_) != NULL && dynamic_cast<BodyNode*>(parent_)->bodyOuter_ != NULL && 
        dynamic_cast<BodyNode*>(parent_)->bodyOuter_->str_ != NULL && dynamic_cast<BodyNode*>(parent_)->bodyOuter_->str_->wordType_ == WordType::INTEGER) {
        oss << contents_->toString();
    } else {
        oss << string("  ")*indent_ << contents_->toString();
    }
    if (contents_->toString() == ";") {
        oss << "\n";
    }    
    return oss.str();
}

BodyNode::BodyNode(AstNode* bodyOuter, AstNode* bodyInner, AstNode* str) {

    nodeType_ = typeid(*this).name();
    bodyOuter_ = dynamic_cast<BodyNode*>(bodyOuter);
    if (bodyOuter_) bodyOuter_->parent_ = this;
    bodyInner_ = dynamic_cast<BodyNode*>(bodyInner);
    if (bodyInner_) {
        bodyInner_->parent_ = this;
    }
    str_ = dynamic_cast<BodyWordNode*>(str);
    if (str_) {
        str_->parent_ = this;
    }
}

string BodyNode::toString() {
    ostringstream oss;
    // Called from the root BodyNode
    if (bodyOuter_) {
        bodyOuter_->indent_ = indent_;
        oss << bodyOuter_->toString();
    }
    if (str_) {
        str_->indent_ = indent_;
        oss << str_->toString();
        return oss.str();
    } else if (bodyInner_) {
        bodyInner_->indent_ = indent_ + 1; 
        oss << string("  ")*indent_ << "{\n"
            << bodyInner_->toString()
            << string("  ")*indent_ << "}\n";
        return oss.str();
    } else {
        assert(false);
    }
}

P4RegisterNode::P4RegisterNode(AstNode* name, AstNode* body) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
    if(body->nodeType_.find("EmptyNode")!=string::npos) {
        body_ = new BodyNode(NULL, NULL, new BodyWordNode(BodyWordNode::STRING, new StrNode(new string(""))));
        width_ = -1;
        instanceCount_ = -1;        
    } else {
        body_ = dynamic_cast<BodyNode*>(body);
        std::stringstream ss(body_->toString());
        std::string item;
        while (std::getline(ss, item, ';'))
        {
            std::stringstream ss_(item);
            std::string item_;
            while (std::getline(ss_, item_, ':')) {
                boost::algorithm::trim(item_);
                if(item_.compare("width")==0) {
                    std::getline(ss_, item_, ':');
                    boost::algorithm::trim(item_);
                    width_ = stoi(item_);
                } else if (item_.compare("instance_count")==0) {
                    std::getline(ss_, item_, ':');
                    boost::algorithm::trim(item_);
                    instanceCount_ = stoi(item_);
                }
            }
        }        
    }
    if (body_) body_->parent_ = this;
}

string P4RegisterNode::toString() {
    ostringstream oss;
    oss << "register " 
        << name_->toString() 
        << " {\n"
        << "  " << body_->toString()
        << "}\n\n";
    return oss.str();
}

P4ExprNode::P4ExprNode(AstNode* keyword, AstNode* name1, AstNode* name2,
           AstNode* opts, AstNode* body) {
    nodeType_ = typeid(*this).name();
    keyword_ = dynamic_cast<KeywordNode*>(keyword);
    name1_ = dynamic_cast<NameNode*>(name1);
    if (name1_) name1_->parent_ = this;
    name2_ = name2;
    if (name2_) name2_->parent_ = this;
    opts_ = opts;
    if (opts_) opts_->parent_ = this;
    if(body->nodeType_.find("EmptyNode")!=string::npos) {
        body_ = new BodyNode(NULL, NULL, new BodyWordNode(BodyWordNode::STRING, new StrNode(new string(""))));
    } else {
        body_ = dynamic_cast<BodyNode*>(body);
    }    
    if (body_) body_->parent_ = this;
}

string P4ExprNode::toString() {
    ostringstream oss;
    oss << keyword_->toString() << " "
        << name1_->toString() << " ";

    // Append name2 if its present
    if (name2_) {
        if (keyword_->toString().compare("calculated_field")==0) {
            oss << ". "
                << name2_->toString();
        } else {
            oss << name2_->toString();
        }
    }

    // Append opts if they're present
    if (opts_) {
        oss << "(" << opts_->toString() << ") ";
    }

    // Append body if its present.
    if (body_) {
        string res = body_->toString();
        oss << "{\n" << res << "}\n";
    } else {
        // If there's no body, its a statement that 
        // ends with a semicolon
        oss << ";";
    }
    return oss.str();
}

KeywordNode::KeywordNode(string* word) {
    nodeType_ = typeid(*this).name();
    word_ = word;
}

string KeywordNode::toString() {
    return *word_;
}

OptsNode::OptsNode(AstNode* nameList) {
    nodeType_ = typeid(*this).name();
    nameList_ = nameList;
}

string OptsNode::toString() {
    if (nameList_) {
        return nameList_->toString();
    }
    return "";
}

NameListNode::NameListNode(AstNode* nameList, AstNode* name) {
    nodeType_ = typeid(*this).name();
    nameList_ = nameList;
    name_ = name;
}

string NameListNode::toString() {
    if (nameList_) {
        ostringstream oss;
        oss << nameList_->toString()
            << ", "
            << name_->toString();
        return oss.str();
    } else {
        return name_->toString();
    }

}

TableReadStmtNode::TableReadStmtNode(MatchType matchType, AstNode* field) {
    nodeType_ = typeid(*this).name();
    matchType_ = matchType;
    field_ = field;
    field_->parent_ = this;
}

string TableReadStmtNode::toString() {
    ostringstream oss;
    oss << "    " << field_->toString() << " : ";
    switch(matchType_) {
        case EXACT: oss << "exact"; break;
        case TERNARY: oss << "ternary"; break;
    }
    oss << ";\n";
    return oss.str();
}

TableReadStmtsNode::TableReadStmtsNode() {
    nodeType_ = typeid(*this).name();
}

string TableReadStmtsNode::toString() {
    ostringstream oss;
    for (auto rsn : *list_) {
        oss << rsn->toString();
    }
    return oss.str();
}

TableActionStmtNode::TableActionStmtNode(AstNode* name) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
}

string TableActionStmtNode::toString() {
    ostringstream oss;
    oss << "    " << name_->toString() << ";\n";
    return oss.str();
}

TableActionStmtsNode::TableActionStmtsNode() {
    nodeType_ = typeid(*this).name();
}

string TableActionStmtsNode::toString() {
    ostringstream oss;
    for (auto asn : *list_) {
        oss << asn->toString();
    }
    return oss.str();
}

TableNode::TableNode(AstNode* name, AstNode* reads, AstNode* actions,
                     string options, string pragma) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
    name_->parent_ = this;
    reads_ = dynamic_cast<TableReadStmtsNode*>(reads);
    if (reads_) reads_->parent_ = this;
    actions_ = dynamic_cast<TableActionStmtsNode*>(actions);
    actions_->parent_ = this;

    pragma_ = pragma;

    boost::algorithm::trim_right_if(options, boost::is_any_of("; \t\n"));
    if (options != "") {
        boost::split(options_, options, boost::is_any_of(";"));
    }
    for (int i = 0; i < options_.size(); i++){
        boost::algorithm::trim(options_[i]);
    }
    isMalleable_ = false;
    pragmaTransformed_ = false;
}

string TableNode::toString() {
    ostringstream oss;
    if(pragma_.compare("")!=0) {
        oss << pragma_ << "\n";
    }
    oss << "table " << name_->toString() << " {\n";
    if (reads_ && reads_->list_->size() > 0) {
        oss << "  reads {\n"
            << reads_->toString()
            << "  }\n";
    }
    oss << "  actions {\n"
        << actions_->toString()
        << "  }\n";
    for (auto str : options_) {
        oss << "  " << str << ";\n";
    }
    oss << "}\n\n";
    return oss.str();
}

void TableNode::transformPragma() {
    if(pragma_.compare("")!=0 && !pragmaTransformed_) {
        std::vector<string> tmp_v;
        boost::algorithm::split(tmp_v, pragma_, boost::algorithm::is_space());
        pragma_ = "";
        for(int i=0; i<tmp_v.size()-1; i++) {
            pragma_ += tmp_v[i];
            pragma_ += " ";            
        }
        int stage_num = stoi(tmp_v[tmp_v.size()-1]);
        stage_num ++;
        pragma_ += std::to_string(stage_num);

        pragmaTransformed_=true;
    }
}

FieldDecNode::FieldDecNode(AstNode* name, AstNode* size) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
    size_ = dynamic_cast<IntegerNode*>(size);
}

string FieldDecNode::toString() {
    ostringstream oss;
    oss << "  " << name_->toString() << " : " << size_->toString() << ";";
    return oss.str();
}

FieldDecsNode::FieldDecsNode() {
    nodeType_ = typeid(*this).name();
}

string FieldDecsNode::toString() {
    ostringstream oss;
    for (auto fdn : *list_) {
        oss << fdn->toString() << endl;
    }
    return oss.str();
}

HeaderTypeDeclarationNode::HeaderTypeDeclarationNode(AstNode* name,
                                                     AstNode* field_decs,
                                                     AstNode* other_stmts) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
    field_decs_ = dynamic_cast<FieldDecsNode*>(field_decs);
    other_stmts_ = other_stmts;
}

string HeaderTypeDeclarationNode::toString() {
    ostringstream oss;
    oss << "header_type " << name_->toString() << " {\n";
    oss << "  fields {\n";
    oss << field_decs_->toString();
    oss << "  }\n";
    oss << other_stmts_->toString();
    oss << "}\n\n";
    return oss.str();
}

HeaderInstanceNode::HeaderInstanceNode(AstNode* type, AstNode* name) {
    nodeType_ = typeid(*this).name();
    type_ = dynamic_cast<NameNode*>(type);
    name_ = dynamic_cast<NameNode*>(name);
}

string HeaderInstanceNode::toString() {
    ostringstream oss;
    oss << "header " << type_->toString() << " " << name_->toString() << ";\n\n";
    return oss.str();
}

MetadataInstanceNode::MetadataInstanceNode(AstNode* type, AstNode* name) {
    nodeType_ = typeid(*this).name();
    type_ = dynamic_cast<NameNode*>(type);
    name_ = dynamic_cast<NameNode*>(name);
}

string MetadataInstanceNode::toString() {
    ostringstream oss;
    oss << "metadata " << type_->toString() << " " << name_->toString() << ";\n\n";
    return oss.str();
}

ArgsNode::ArgsNode() {
    nodeType_ = typeid(*this).name();
}

ArgsNode* ArgsNode::deepCopy() {
    auto newNode = new ArgsNode();
    for (auto bw : *list_) {
        auto newArg = bw->deepCopy();
        newNode->push_back(newArg);
    }

    return newNode;
}

string ArgsNode::toString() {
    bool first = true;
    ostringstream oss;
    for (auto bw : *list_) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        oss << bw->toString();
    }
    return oss.str();
}

ActionParamNode::ActionParamNode(AstNode* param) {
    nodeType_ = typeid(*this).name();
    param_ = param;
    param->parent_ = this;
}

ActionParamNode* ActionParamNode::deepCopy() {
    AstNode* newParam = param_;
    if (param_->nodeType_.find("MblRefNode") != string::npos) {
        newParam = dynamic_cast<MblRefNode*>(param_)->deepCopy();
    }
    auto newNode = new ActionParamNode(newParam);
    return newNode;
}

string ActionParamNode::toString() {
    return param_->toString();
}

ActionParamsNode::ActionParamsNode() {
    nodeType_ = typeid(*this).name();
}

ActionParamsNode* ActionParamsNode::deepCopy() {
    auto newNode = new ActionParamsNode();
    for (auto ap : *list_) {
        auto newParam = ap->deepCopy();
        newNode->push_back(newParam);
    }

    return newNode;
}

string ActionParamsNode::toString(){
    bool first = true;
    ostringstream oss;
    for (auto ap : *list_) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        oss << ap->toString();
    }
    return oss.str();
}

ActionStmtNode::ActionStmtNode(AstNode* name1, AstNode* args, ActionStmtType type, AstNode* name2, AstNode* index) {
    nodeType_ = typeid(*this).name();
    name1_ = dynamic_cast<NameNode*>(name1);
    name2_ = dynamic_cast<NameNode*>(name2);
    if(args) {
        args_ = dynamic_cast<ArgsNode*>(args);
        args_->parent_ = this;
    }

    index_ = dynamic_cast<IntegerNode*>(index);
    type_ = type;
}

ActionStmtNode* ActionStmtNode::deepCopy() {
    return new ActionStmtNode(name1_, args_->deepCopy(), type_, name2_, index_);
}

string ActionStmtNode::toString() {
    ostringstream oss;
    if(type_==ActionStmtType::NAME_ARGLIST) {
        oss << " " 
            << name1_->toString()
            << "("
            << args_->toString() 
            << ");\n";
    } else if (type_==ActionStmtType::PROG_EXEC) {
        oss << " " 
            << name1_->toString() 
            << " . "
            << name2_->toString()
            << " ( "
            << args_->toString()
            << " );\n";
    } else {
        oss << "parsing error!\n";
    }
    return oss.str();
}

ActionStmtsNode::ActionStmtsNode() {
    nodeType_ = typeid(*this).name();
}

ActionStmtsNode* ActionStmtsNode::deepCopy() {
    auto newNode = new ActionStmtsNode();
    for (auto as : *list_) {
        auto newStmt = as->deepCopy();
        newNode->push_back(newStmt);
    }

    return newNode;
}

string ActionStmtsNode::toString() {
    ostringstream oss;
    for (auto as : *list_) {
        oss << as->toString();
    }
    return oss.str();
}

ActionNode::ActionNode(AstNode* name, AstNode* params, AstNode* stmts) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
    name_->parent_ = this;
    params_ = dynamic_cast<ActionParamsNode*>(params);
    params_->parent_ = this;
    stmts_ = dynamic_cast<ActionStmtsNode*>(stmts);
    stmts_->parent_ = this;
}

ActionNode* ActionNode::duplicateAction(const string& name) {
    auto newNode = new ActionNode(new NameNode(new string(name)),
                                  params_->deepCopy(), stmts_->deepCopy());
    return newNode;
}

string ActionNode::toString() {
    ostringstream oss;
    oss << "action " << name_->toString()
                     << "(" << params_->toString() << ") {\n"
        << stmts_->toString()
        << "}\n";
    return oss.str();
}
