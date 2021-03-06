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

#ifndef COMPILE_P4_H
#define COMPILE_P4_H

#include <unordered_map>
#include <vector>

#define REGISTER_SIZE 32

typedef pair<ReactionArgNode*, int /* size */> ReactionArgSize;
typedef pair<vector<ReactionArgSize>, int /* size */> ReactionArgBin;

void transformPragma(vector<AstNode*>* astNodes);

int inferIsoOptForIng(vector<AstNode*>* astNodes, bool forIng);

bool augmentIngress(vector<AstNode*>* astNodes);

bool augmentEgress(vector<AstNode*>* astNodes);

enum USAGE {
    INGRESS = 0,
    EGRESS = 1,
    BOTH = 2
};
void findMalleableUsage(
            vector<MblRefNode*> mblRefs,
            const unordered_map<string, P4RMalleableValueNode*> mblValues,
            const unordered_map<string, P4RMalleableFieldNode*> mblFields,
            vector<AstNode*>* nodeArray,
            unordered_map<string, int>* mblUsages);

void transformMalleableRefs(
            vector<MblRefNode*>* mblRefs,
            const unordered_map<string, P4RMalleableValueNode*>& mblValues,
            const unordered_map<string, P4RMalleableFieldNode*>& mblFields,
            vector<AstNode*>* nodeArray);

void transformMalleableTables(
            unordered_map<string, P4RMalleableTableNode*>* mblTables, vector<AstNode*> nodeArray, int ing_iso_opt, int egr_iso_opt);

void generateExportControl(vector<AstNode*>* newNodes,
                           const vector<ReactionArgBin>& argBins, const vector<ReactionArgBin>& argBinsEgr);

void augmentRegisterArgProgForIng(vector<AstNode*>* newNodes,
                         vector<AstNode*>* nodeArray,
                         const vector<ReactionArgNode*>& reaction_args,
                         int iso_opt, bool forIng);

void generateRegArgGateControl(vector<AstNode*>* newNodes,
                         vector<AstNode*>* nodeArray,
                         const vector<ReactionArgNode*>& reaction_args,
                         int isolation_opt, int egr_iso_opt);

void generateDupRegArgProg(vector<AstNode*>* newNodes,
                         vector<AstNode*>* nodeArray,
                         const vector<ReactionArgNode*>& reaction_args,
                         int isolation_opt, int egr_iso_opt);

vector<ReactionArgBin> generateIngDigestPacking(
            vector<AstNode*>* newNodes,
            const vector<ReactionArgNode*>& reaction_args,
            const HeaderDecsMap& headerDecsMap,
            const unordered_map<string, P4RMalleableValueNode*>& mblValues,
            const unordered_map<string, P4RMalleableFieldNode*>& mblFields,
            const vector<AstNode*>& astNodes,
            int* ing_iso_opt);

vector<ReactionArgBin> generateEgrDigestPacking(
            vector<AstNode*>* newNodes,
            const vector<ReactionArgNode*>& reaction_args,
            const HeaderDecsMap& headerDecsMap,
            const unordered_map<string, P4RMalleableValueNode*>& mblValues,
            const unordered_map<string, P4RMalleableFieldNode*>& mblFields,
            const vector<AstNode*>& astNodes,
            int* egr_iso_opt);

// Generate metadata for dynamic malleables
void generateMetadata(vector<AstNode*>* newNodes,
                      const unordered_map<string,
                                          P4RMalleableValueNode*>& mblValues,
                      const unordered_map<string,
                                          P4RMalleableFieldNode*>& mblFields,
                      int ing_iso_opt, int egr_iso_opt);

// Generate a merged table that sets all malleables
int generateInitTableForIng(unordered_map<string, int>* mblUsages,
                             vector<AstNode*>* newNodes,
                             const unordered_map<string,
                                                 P4RMalleableValueNode*>& mblValues,
                             const unordered_map<string,
                                                 P4RMalleableFieldNode*>& mblFields,
                             int iso_opt, bool forIng);

void generateSetvarControl(vector<AstNode*>* newNodes);

#endif