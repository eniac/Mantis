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

#ifndef COMPILE_C_H
#define COMPILE_C_H

#include <unordered_map>
#include <vector>

void extractReactionMacro(std::vector<AstNode*> nodeArray, ostringstream& oss_preprocessor, string out_fn_base);

UnanchoredNode* generatePrologueNode(std::vector<AstNode*> nodeArray, ostringstream& oss_variable_init, ostringstream& oss_init_end);

UnanchoredNode* generateDialogueNode(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_start, ostringstream& oss_reaction_end);

UnanchoredNode * generateMacroNode(ostringstream& oss_preprocessor);

void mirrorFieldArg(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_start, 
                    ostringstream& oss_preprocessor, vector<ReactionArgBin> bins,
                    string prefix_str, bool forIng);

void mirrorRegisterArgForIng(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_start, int iso_opt, string prefix_str, bool forIng);

void generateMacroXorVersionBits(ostringstream& oss_reaction_start, ostringstream& oss_preprocessor, int ing_iso_opt, int egr_iso_opt);

void generateDialogueArgStart(ostringstream& oss_reaction_start, int ing_iso_opt, int egr_iso_opt);

void generateMacroNonMblTable(std::vector<AstNode*> nodeArray, ostringstream& oss_preprocessor, string prefix_str);

void generateMacroMblTable(std::vector<AstNode*> nodeArray, ostringstream& oss_preprocessor, string prefix_str, int ing_iso_opt, int egr_iso_opt, ostringstream& oss_reaction_mirror);

void generateMacroInitMblsForIng(std::vector<AstNode*> nodeArray, unordered_map<string, int>* mblUsages, ostringstream& oss_variable_init, ostringstream& oss_reaction_start, 
                             ostringstream& oss_preprocessor, int num_vars, int iso_opt, string prefix_str, bool forIng);

void generatePrologueEnd(std::vector<AstNode*> nodeArray, ostringstream& oss_init_end, int ing_iso_opt, int egr_iso_opt);

void generateDialogueEnd(std::vector<AstNode*> nodeArray, ostringstream& oss_reaction_end, int ing_iso_opt, int egr_iso_opt);

#endif