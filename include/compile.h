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

#ifndef COMPILE_H
#define COMPILE_H

#include <vector>

#include "ast_nodes.h"
#include "ast_nodes_p4r.h"

using namespace std;

vector<AstNode*> compileP4Code(vector<AstNode*>* nodeArray);

vector<UnanchoredNode *> compileCCode(std::vector<AstNode*> nodeArray, char * outFnBase);

#endif
