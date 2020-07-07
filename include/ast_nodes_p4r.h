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

#ifndef AST_NODES_P4R_H
#define AST_NODES_P4R_H

#include "ast_nodes.h"
#include "ast_nodes_p4.h"

/*========================================
=            p4r expressions.            =
========================================*/
class P4RExprNode : public AstNode {
public:
    P4RExprNode(AstNode* varOrReaction);
    std::string toString();

    AstNode *varOrReaction_;
};


/**
 *
 * Malleabless and subnodes
 *
 */
class VarWidthNode : public AstNode {
public:
    VarWidthNode(AstNode* val);
    std::string toString();

    IntegerNode *val_;
};

class VarInitNode : public AstNode {
public:
    VarInitNode(AstNode* val);
    std::string toString();

    AstNode *val_;
};

class P4RSettableMalleableNode : public AstNode {
public:
    enum MalleableType { VALUE, FIELD };

    P4RSettableMalleableNode(std::string nodeType, AstNode* name,
                            AstNode* varWidth, MalleableType MalleableType);

    const MalleableType malleableType_;
    NameNode* name_;
    VarWidthNode* varWidth_;
};

class P4RMalleableValueNode : public P4RSettableMalleableNode {
public:
    P4RMalleableValueNode(AstNode* name, AstNode* varWidth, AstNode* varInit);
    std::string toString();

    AstNode* varInit_;
};

class FieldNode : public AstNode {
public: 
    FieldNode(AstNode* headerName, AstNode* fieldName);
    std::string toString();

    NameNode* headerName_;
    NameNode* fieldName_;
};

class FieldsNode : public ListNode<FieldNode> {
public :
    FieldsNode();
    std::string toString();
};

class VarAltNode : public AstNode {
public:
    VarAltNode(AstNode* fieldList);
    std::string toString();

    FieldsNode* fields_;
};

class P4RMalleableFieldNode : public P4RSettableMalleableNode {
public:
    P4RMalleableFieldNode(AstNode* name, AstNode* varWidth,
                         AstNode* varInit, AstNode* varAlts);
    std::string toString();

    // during prologue/dialogue, each alternative corresponds to a unique value in pointer metadata
    int mapAltToInt(std::string alt);

    AstNode* varInit_;
    VarAltNode* varAlts_;
};

class P4RMalleableTableNode : public AstNode {
public:
    P4RMalleableTableNode(AstNode* table, std::string pragma);
    std::string toString();
    void transformPragma();

    TableNode* table_;
    bool pragmaTransformed_;
    std::string pragma_;
};


/**
 *
 * Reaction and subnodes.
 *
 */
class ReactionArgNode : public AstNode {
public:
    enum ArgType { INGRESS_FIELD, EGRESS_FIELD, INGRESS_MBL_FIELD, EGRESS_MBL_FIELD, REGISTER };

    ReactionArgNode(const ArgType& argType, AstNode* arg, AstNode* index1, AstNode* index2);
    std::string toString();

    const ArgType argType_;
    AstNode* arg_;
    IntegerNode* index1_;
    IntegerNode* index2_; 
};

class ReactionArgsNode : public ListNode<ReactionArgNode> {
public:
    ReactionArgsNode();
    std::string toString();
};

class P4RReactionNode : public AstNode {
public:
    P4RReactionNode(AstNode* name, AstNode* args, AstNode* body);
    std::string toString();

    NameNode* name_;
    ReactionArgsNode* args_;
    BodyNode* body_;
};

class P4RInitBlockNode : public AstNode {
public:
    P4RInitBlockNode(AstNode* name, AstNode* body);
    std::string toString();

    NameNode* name_;
    BodyNode* body_;
};

class MblRefNode : public AstNode {
public:
    MblRefNode(AstNode* name);
    void transform(const std::string &transformedHeader,
                   const std::string &transformedField);
    MblRefNode* deepCopy();
    std::string toString();

    bool transformed_ = false;
    bool inReaction_ = false;
    std::string transformedHeader_;
    std::string transformedField_;
    NameNode* name_;
};

/*=====  End of p4r expressions  ======*/


/*============================================================
=            Custom nodes to add transformations            =
============================================================*/

class UnanchoredNode : public AstNode {
// A block of code that is not anchored anywhere in the syntax tree
public:
    UnanchoredNode(std::string* newCode, std::string* objType,
                   std::string* objName);
    std::string toString();

    std::string *codeBlob_; // The P4 code
    std::string *objType_;  // The type of the P4 object (ex: table, metadata)
    std::string *objName_;  // A P4 object to invoke
};

/*=====  End of Custom nodes to add transformations  ======*/


#endif