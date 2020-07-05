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

#include "../../include/compile.h"
#include "../../include/find_nodes.h"
#include "../../include/helper.h"

#include "compile_const.h"
#include "compile_p4.h"
#include "compile_c.h"

static int ing_iso_opt = -1;
static int egr_iso_opt = -1;
static int num_init_vars = -1;
static vector<ReactionArgBin> global_ing_bins;
static vector<ReactionArgBin> global_egr_bins;


vector<AstNode*> compileP4Code(vector<AstNode*>* nodeArray) {

    ing_iso_opt = inferIngIsolationOpt(nodeArray);
    egr_iso_opt = inferEgrIsolationOpt(nodeArray);

    transformPragma(nodeArray);

    unordered_map<string, P4RMalleableValueNode*> varValues;
    unordered_map<string, P4RMalleableFieldNode*> varFields;
    unordered_map<string, P4RMalleableTableNode*> varTables;
    findAndRemoveMalleables(&varValues, &varFields, &varTables, *nodeArray);

    vector<MblRefNode*> varRefs;
    findMalleableRefs(&varRefs, *nodeArray);

    // Transform all references to Malleables into references to the appropriate metadata
    transformMalleableRefs(&varRefs, varValues, varFields, nodeArray);
    transformMalleableTables(&varTables, ing_iso_opt);

    auto newNodes = vector<AstNode*>();

    generateMetadata(&newNodes, varValues, varFields, ing_iso_opt, egr_iso_opt);
    num_init_vars = generateIngInitTable(&newNodes, varValues, varFields, ing_iso_opt);
    generateEgrInitTable(&newNodes, varValues, varFields, egr_iso_opt);

    generateSetvarControl(&newNodes);

    // Measurement code
    HeaderDecsMap headerDecsMap = findHeaderDecs(*nodeArray);
    vector<ReactionArgNode*> reaction_args = findReactionArgs(*nodeArray);

    global_ing_bins = generateIngDigestPacking(&newNodes, reaction_args, headerDecsMap,
                    varValues, varFields, *nodeArray, &ing_iso_opt);
    global_egr_bins = generateEgrDigestPacking(&newNodes, reaction_args, headerDecsMap,
                    varValues, varFields, *nodeArray, &egr_iso_opt);    

    // After packing, iso_opt is firm
    augmentRegisterArgProgForIng(&newNodes, nodeArray, reaction_args, ing_iso_opt, true);   
    augmentRegisterArgProgForIng(&newNodes, nodeArray, reaction_args, egr_iso_opt, false);  

    generateDupRegArgProg(&newNodes, nodeArray, reaction_args, ing_iso_opt, egr_iso_opt);

    generateRegArgGateControl(&newNodes, nodeArray, reaction_args, ing_iso_opt, egr_iso_opt);

    generateExportControl(&newNodes, global_ing_bins, global_egr_bins);

    // Finally, assemble ingress/egress
    augmentIngress(nodeArray);
    augmentEgress(nodeArray); 

    return newNodes;
}

vector<UnanchoredNode *> compileCCode(std::vector<AstNode*> nodeArray, char * outFnBase) {
	vector<UnanchoredNode *> ret_vec = vector<UnanchoredNode *>(); 
    
    string out_fn_base_str = string(outFnBase);
    string base_fn = out_fn_base_str.substr(out_fn_base_str.find_last_of("/\\") + 1);
    PRINT_VERBOSE("Output filename base: %s\n", base_fn.c_str());    
    string prefix_str = "p4_pd_" + string(base_fn) + "_mantis_";

    ostringstream oss_preprocessor;
    ostringstream oss_mbl_init;
    ostringstream oss_init_end;
    ostringstream oss_reaction_mirror;
    ostringstream oss_reaction_update;

    extractReactionMacro(nodeArray, oss_preprocessor, string(outFnBase));

    generateMacroNonMblTable(nodeArray, oss_preprocessor, prefix_str);
 
    generatePrologueEnd(nodeArray, oss_init_end, ing_iso_opt);

    generateMacroInitMbls(nodeArray, oss_mbl_init, oss_reaction_mirror, oss_preprocessor, num_init_vars, ing_iso_opt, prefix_str);

    generateMacroXorVersionBits(oss_reaction_mirror, oss_preprocessor, ing_iso_opt);

    generateDialogueArgStart(oss_reaction_mirror, ing_iso_opt);

    mirrorFieldArg(nodeArray, oss_reaction_mirror, oss_preprocessor, global_ing_bins, prefix_str, ing_iso_opt);

    mirrorRegisterArgForIng(nodeArray, oss_reaction_mirror, ing_iso_opt, prefix_str, true);
    mirrorRegisterArgForIng(nodeArray, oss_reaction_mirror, egr_iso_opt, prefix_str, false);

    generateMacroMblTable(nodeArray, oss_preprocessor, prefix_str, ing_iso_opt, oss_reaction_mirror);

    generateDialogueEnd(nodeArray, oss_reaction_update, ing_iso_opt, egr_iso_opt);

    ret_vec.push_back(generateMacroNode(oss_preprocessor));
    ret_vec.push_back(generatePrologueNode(nodeArray, oss_mbl_init, oss_init_end));
    ret_vec.push_back(generateDialogueNode(nodeArray, oss_reaction_mirror, oss_reaction_update));    

	return ret_vec;
}