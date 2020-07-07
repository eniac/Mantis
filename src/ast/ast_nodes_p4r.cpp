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

#include "../../include/ast_nodes_p4r.h"

using namespace std;


P4RExprNode::P4RExprNode(AstNode* varOrReaction) {
    nodeType_ = typeid(*this).name();
    varOrReaction_ = varOrReaction;
}

string P4RExprNode::toString() {
    return varOrReaction_->toString();
}

P4RSettableMalleableNode::P4RSettableMalleableNode(string nodeType, AstNode* name,
                                                 AstNode* varWidth,
                                                 MalleableType malleableType)
                                                 : malleableType_(malleableType) {
    nodeType_ = nodeType;
    name_ = dynamic_cast<NameNode*>(name);
    varWidth_ = dynamic_cast<VarWidthNode*>(varWidth);
}

P4RMalleableValueNode::P4RMalleableValueNode(AstNode* name, AstNode* varWidth,
                                           AstNode* varInit)
        : P4RSettableMalleableNode(typeid(*this).name(), name, varWidth,
                                  P4RSettableMalleableNode::VALUE) {
    varInit_ = varInit;
}

string P4RMalleableValueNode::toString() {
    if (removed_) {
        return "";
    }

    ostringstream oss;
    oss << "Malleable value " << name_->toString() << " {\n" 
        << " " << varWidth_->toString() << "\n"
        << " " << varInit_->toString() << "\n"
        << "}";
    return oss.str();
}

FieldNode::FieldNode(AstNode* headerName, AstNode* fieldName) {
    nodeType_ = typeid(*this).name();
    headerName_ = dynamic_cast<NameNode*>(headerName);
    fieldName_ = dynamic_cast<NameNode*>(fieldName);
}

string FieldNode::toString() {
    ostringstream oss;
    oss << headerName_->toString()
        << "."
        << fieldName_->toString();
    return oss.str();
}

FieldsNode::FieldsNode() {
    nodeType_ = typeid(*this).name();
}

string FieldsNode::toString() {
    bool first = true;
    ostringstream oss;

    for (auto fld : *this->list_) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        oss << fld->toString();
    }
    return oss.str();
}

VarAltNode::VarAltNode(AstNode* fields) {
    nodeType_ = typeid(*this).name();
    fields_ = dynamic_cast<FieldsNode*>(fields);
}

string VarAltNode::toString() {
    ostringstream oss;
    oss << "alts: {" << fields_->toString() << "}";
    return oss.str();
}

P4RMalleableFieldNode::P4RMalleableFieldNode(AstNode* name, AstNode* varWidth,
                                           AstNode* varInit, AstNode* varAlts)
        : P4RSettableMalleableNode(typeid(*this).name(), name, varWidth,
                                  P4RSettableMalleableNode::FIELD) {
    varInit_ = varInit;
    varAlts_ = dynamic_cast<VarAltNode*>(varAlts);
}

string P4RMalleableFieldNode::toString() {
    if (removed_) {
        return "";
    }

    ostringstream oss;
    oss << "malleable field " << name_->toString() << " {\n" 
        << " " << varWidth_->toString() << "\n"
        << " " << varInit_->toString() << "\n"
        << " " << varAlts_->toString() << "\n"
        << "}";
    return oss.str();
}

int P4RMalleableFieldNode::mapAltToInt(std::string input) {
    int val_int = 0;
    for (FieldNode* alt : *varAlts_->fields_->list_) {
        if (alt->toString().compare(input)==0) {
            return val_int;
        } else {
            val_int += 1;
        }
    }
    return -1;    
}

P4RMalleableTableNode::P4RMalleableTableNode(AstNode* table, std::string pragma) {
    nodeType_ = typeid(*this).name();
    table_ = dynamic_cast<TableNode*>(table);

    pragmaTransformed_ = false;
    pragma_ = pragma;
}

void P4RMalleableTableNode::transformPragma() {
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

string P4RMalleableTableNode::toString() {
    ostringstream oss;
    if(pragma_.compare("")!=0) {
        oss << pragma_ << "\n";
    }
        
    if (!removed_) {
        oss << " malleable ";
    }

    oss << table_->toString();
    return oss.str();
}

VarWidthNode::VarWidthNode(AstNode* val) {
    nodeType_ = typeid(*this).name();
    val_ = dynamic_cast<IntegerNode*>(val);
}

string VarWidthNode::toString() {
    ostringstream oss;
    oss << "width: " << val_->toString() << ";";
    return oss.str();
}

VarInitNode::VarInitNode(AstNode* val) {
    nodeType_ = typeid(*this).name();
    val_ = val;
}

string VarInitNode::toString() {
    ostringstream oss;
    oss << "init: " << val_->toString() << ";";
    return oss.str();
}

/**
 *
 * Initialization block (assume a global block).
 *
 */

P4RInitBlockNode::P4RInitBlockNode(AstNode* name, AstNode* body) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
    name_->parent_ = this;
    body_ = dynamic_cast<BodyNode*>(body);
    body_->parent_ = this;
}

string P4RInitBlockNode::toString() {
    if (removed_) {
        return "";
    }

    ostringstream oss;
    oss << "initialization " 
        << name_->toString()
        << " {\n"
        << body_->toString()
        << "}";
    return oss.str();
}

/**
 *
 * Reaction and subnodes.
 *
 */
P4RReactionNode::P4RReactionNode(AstNode* name, AstNode* args, AstNode* body) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
    name_->parent_ = this;
    args_ = dynamic_cast<ReactionArgsNode*>(args);
    args_->parent_ = this;
    if(body->nodeType_.find("EmptyNode")!=string::npos) {
        // if body is empty node
        body_ = new BodyNode(NULL, NULL, new BodyWordNode(BodyWordNode::STRING, new StrNode(new string(""))));
    } else {
        body_ = dynamic_cast<BodyNode*>(body);
    }
    body_->parent_ = this;
}

string P4RReactionNode::toString() {
    if (removed_) {
        return "";
    }

    ostringstream oss;
    oss << "reaction " 
        << name_->toString()
        << " (" 
        << args_->toString()
        << " ) {\n"
        << body_->toString()
        << "}";
    return oss.str();
}

ReactionArgsNode::ReactionArgsNode() {
    nodeType_ = typeid(*this).name();
}

string ReactionArgsNode::toString() {
    bool first = true;
    ostringstream oss;
    for (auto ra : *list_) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        oss << ra->toString();
    }
    return oss.str();
}

ReactionArgNode::ReactionArgNode(const ArgType& argType, AstNode* arg, AstNode* index1, AstNode* index2)
                                 : argType_(argType) {
    nodeType_ = typeid(*this).name();
    arg_ = arg;
    arg_->parent_ = this;

    index1_ = dynamic_cast<IntegerNode*>(index1);
    index2_ = dynamic_cast<IntegerNode*>(index2);
}

string ReactionArgNode::toString() {
    return arg_->toString();
}

MblRefNode::MblRefNode(AstNode* name) {
    nodeType_ = typeid(*this).name();
    name_ = dynamic_cast<NameNode*>(name);
}

void MblRefNode::transform(const string& transformedHeader,
                           const string& transformedField) {
    transformed_ = true;
    transformedHeader_ = transformedHeader;
    transformedField_ = transformedField;
}

MblRefNode* MblRefNode::deepCopy() {
    auto newNode = new MblRefNode(name_->deepCopy());
    newNode->transformed_ = transformed_;
    newNode->transformedHeader_ = transformedHeader_;
    newNode->transformedField_ = transformedField_;

    return newNode;
}

string MblRefNode::toString() {
    if (transformed_) {
        if (!inReaction_) {
            return transformedHeader_ + "." + transformedField_;
        }
    } else {
        ostringstream oss;
        oss << "${" << name_->toString() << "}";
        return oss.str();
    }
}

UnanchoredNode::UnanchoredNode(string* newCode, string* objType,
                               string* objName) {
    nodeType_ = typeid(*this).name();
    codeBlob_ = newCode;
    objType_ = objType;
    objName_ = objName;
}

string UnanchoredNode::toString() {
    ostringstream oss;
    oss << *codeBlob_;
    return oss.str();
}
