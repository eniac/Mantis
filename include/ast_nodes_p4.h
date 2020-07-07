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

#ifndef AST_NODES_P4_H
#define AST_NODES_P4_H

#include "ast_nodes.h"


template<class T>
class ListNode : public AstNode {
public:
    ListNode() {
        list_ = new std::vector<T*>();
    }
    void push_back(T* node) {
        list_->push_back(node);
        node->parent_ = this;
    }
    std::vector<T*>* list_;
};

// An identifier, word, int, or special char
class BodyWordNode : public AstNode {
public:
    enum WordType { VARREF, NAME, FIELD, STRING, INTEGER, SPECIAL };

    BodyWordNode(WordType wordType, AstNode* contents);
    BodyWordNode* deepCopy();
    std::string toString();

    const WordType wordType_;
    AstNode* contents_;

    int indent_=1;
};

class BodyNode : public AstNode {
public:
    BodyNode(AstNode* bodyOuter, AstNode* bodyInner, AstNode* str);
    std::string toString();

    BodyNode* bodyOuter_;
    BodyNode* bodyInner_;
    BodyWordNode* str_;

    int indent_=1;
};

class KeywordNode : public AstNode {
public: 
    KeywordNode(std::string* word);
    std::string toString();

    std::string* word_;
};

class P4RegisterNode : public AstNode {
public:
    P4RegisterNode(AstNode* name, AstNode* body);
    std::string toString();
    
    AstNode* name_;
    BodyNode* body_;
    
    int width_;
    int instanceCount_;    
};

class P4ExprNode : public AstNode {
public:
    P4ExprNode(AstNode* keyword, AstNode* name1, AstNode* name2,
               AstNode* opts, AstNode* body);
    std::string toString();

    KeywordNode* keyword_;
    NameNode* name1_;
    AstNode* name2_;
    AstNode* opts_;
    BodyNode* body_;
};

class OptsNode : public AstNode {
public:
    OptsNode(AstNode* nameList);
    std::string toString();

    AstNode* nameList_;
};

class NameListNode : public AstNode {
public: 
    NameListNode(AstNode* nameList, AstNode* name);
    std::string toString();

    AstNode *nameList_, *name_;
};

class TableReadStmtNode : public AstNode {
public:
    enum MatchType { EXACT, TERNARY };

    TableReadStmtNode(MatchType matchType, AstNode* field);
    std::string toString();

    MatchType matchType_;
    AstNode* field_;
};

class TableReadStmtsNode : public ListNode<TableReadStmtNode> {
public:
    TableReadStmtsNode();
    std::string toString();
};

class TableActionStmtNode : public AstNode {
public:
    TableActionStmtNode(AstNode* name);
    std::string toString();

    NameNode* name_;
};

class TableActionStmtsNode : public ListNode<TableActionStmtNode> {
public:
    TableActionStmtsNode();
    std::string toString();
};

// A P4 table
class TableNode : public AstNode {
public:
    TableNode(AstNode* name, AstNode* reads, AstNode* actions,
              std::string options, std::string pragma);
    std::string toString();
    void transformPragma();

    NameNode* name_;
    TableReadStmtsNode* reads_;
    TableActionStmtsNode* actions_;
    // size, default action, etc.
    vector<std::string> options_;

    std::string pragma_;
    bool pragmaTransformed_;

    // for malleable table, parser gives TableNode as well 
    // need to keep this meta data to indicate if the table node is variable
    bool isMalleable_;
};

class FieldDecNode : public AstNode {
public:
    FieldDecNode(AstNode* name, AstNode* size);
    std::string toString();

    NameNode* name_;
    IntegerNode* size_;
};

class FieldDecsNode : public ListNode<FieldDecNode> {
public:
    FieldDecsNode();
    std::string toString();
};

class HeaderTypeDeclarationNode : public AstNode {
public:
    HeaderTypeDeclarationNode(AstNode* name, AstNode* field_decs,
                              AstNode* other_stmts);
    std::string toString();

    NameNode* name_;
    FieldDecsNode* field_decs_;
    AstNode* other_stmts_;
};

class HeaderInstanceNode : public AstNode {
public:
    HeaderInstanceNode(AstNode* type, AstNode* name);
    std::string toString();

    NameNode* type_;
    NameNode* name_;
};

class MetadataInstanceNode : public AstNode {
public:
    MetadataInstanceNode(AstNode* type, AstNode* name);
    std::string toString();

    NameNode* type_;
    NameNode* name_;
};

class ArgsNode : public ListNode<BodyWordNode> {
public:
    ArgsNode();
    ArgsNode* deepCopy();
    std::string toString();
};

class ActionParamNode : public AstNode {
public:
    ActionParamNode(AstNode* param);
    ActionParamNode* deepCopy();
    std::string toString();

    AstNode* param_;
};

class ActionParamsNode : public ListNode<ActionParamNode> {
public:
    ActionParamsNode();
    ActionParamsNode* deepCopy();
    std::string toString();
};

class ActionStmtNode : public AstNode {
public:
    enum ActionStmtType { NAME_ARGLIST, PROG_EXEC };

    ActionStmtNode(AstNode* name1, AstNode* args, ActionStmtType type, AstNode* name2, AstNode* index);
    ActionStmtNode* deepCopy();
    std::string toString();

    ActionStmtType type_;

    NameNode* name1_;
    NameNode* name2_;
    IntegerNode* index_;
    ArgsNode* args_;
};

class ActionStmtsNode : public ListNode<ActionStmtNode> {
public:
    ActionStmtsNode();
    ActionStmtsNode* deepCopy();
    std::string toString();
};

class ActionNode : public AstNode {
public:
    ActionNode(AstNode* name, AstNode* params, AstNode* stmts);
    ActionNode* duplicateAction(const std::string& name);
    std::string toString();

    NameNode* name_;
    ActionParamsNode* params_;
    ActionStmtsNode* stmts_;
};

#endif