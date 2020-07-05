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

#ifndef FIND_NODES_H
#define FIND_NODES_H

#include <regex>
#include <unordered_map>

#include "ast_nodes.h"
#include "ast_nodes_p4.h"
#include "ast_nodes_p4r.h"


bool typeContains(AstNode* n, const char* type);

bool p4KeywordMatches(P4ExprNode* n, const char* type);

P4ExprNode* findIngress(const std::vector<AstNode*>& astNodes);

P4ExprNode* findEgress(const std::vector<AstNode*>& astNodes);

vector<P4ExprNode*> findBlackbox(const std::vector<AstNode*>& astNodes);

void findAndRemoveMalleables(
            unordered_map<string, P4RMalleableValueNode*>* varValues,
            unordered_map<string, P4RMalleableFieldNode*>* varFields,
            unordered_map<string, P4RMalleableTableNode*>* varTables,
            const vector<AstNode*>& astNodes);

void findMalleableRefs(vector<MblRefNode*>* mblRefs,
                      const vector<AstNode*>& astNodes);

unordered_map<TableActionStmtNode*, TableNode*> findTableActionStmts(
            const vector<AstNode*>& astNodes, const string& actionName);

bool findTableReadStmt(const TableNode& table, const string& fieldName);

void findAndTransformMblRefsInAction(ActionNode* action,
                                     vector<MblRefNode*>* varRefs,
                                     const string& varName,
                                     const string& altHeader,
                                     const string& altField);

vector<FieldNode*> findAllAlts(const P4RMalleableFieldNode& malleable);

P4RInitBlockNode* findInitBlock(std::vector<AstNode*> astNodes);

P4RReactionNode* findReaction(std::vector<AstNode*> astNodes);

vector<P4RegisterNode*> findP4RegisterNode(const vector<AstNode*>& astNodes);

vector<ReactionArgNode*> findReactionArgs(const vector<AstNode*>& astNodes);

typedef unordered_map<string /* instanceName */,
                      std::vector<FieldDecNode*>*> HeaderDecsMap;
HeaderDecsMap findHeaderDecs(const vector<AstNode*>& astNodes);

vector<pair<ReactionArgNode*, int> > findAllReactionArgSizes(
            const vector<ReactionArgNode*>& reactionArgs,
            const HeaderDecsMap& headerDecsMap,
            const unordered_map<string, P4RMalleableValueNode*>& varValues,
            const unordered_map<string, P4RMalleableFieldNode*>& varFields);

void findAndRemoveReactions(vector<P4RReactionNode*>* reactions,
                            const vector<AstNode*>& astNodes);

bool findRegargInIng(ReactionArgNode* regarg, std::vector<AstNode*> astNodes);

int findRegargWidth(ReactionArgNode* regarg, std::vector<AstNode*> nodeArray);

#endif