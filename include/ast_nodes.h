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

#ifndef AST_NODES_H
#define AST_NODES_H

#include <string>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <typeinfo>
#include <vector>
#include <boost/algorithm/string.hpp>

using namespace std;


// Base syntax tree node
class AstNode {
public:
    bool valid_ = true;
    string nodeType_ = string("base");
    AstNode* parent_ = NULL;
    bool removed_ = false;

    virtual string toString() {
        return "";
    }
};

class EmptyNode : public AstNode {
public:
    string* emptyStr_ = new string("");
    EmptyNode() {    
        nodeType_ = typeid(*this).name();
        valid_ = false;    
    }
    string toString() {
        return *(emptyStr_);
    }
};

// A P4 identifier
class NameNode : public AstNode {
public:
    string* word_;
    NameNode(string* word) {
        nodeType_ = typeid(*this).name();
        word_ = word;
    }
    NameNode* deepCopy() {
        return new NameNode(new string(*word_));
    }
    string toString() {
        return *(word_);
    }
};

class StrNode : public AstNode {
public:
    string* word_;
    StrNode(string* word) {
        nodeType_ = typeid(*this).name();
        word_ = word;
    }
    string toString() {
        return *(word_);
    }
};

class IntegerNode : public AstNode {
public:
    string* word_;
    IntegerNode(string* word) {
        nodeType_ = typeid(*this).name();
        word_ = word;
    }
    string toString() {
        return *(word_);
    }
};

class SpecialCharNode : public AstNode {
public:
    string* word_;
    SpecialCharNode(string* word) {
        nodeType_ = typeid(*this).name();
        word_ = word;
    }
    string toString() {
        return *(word_);
    }
};

class IncludeNode : public AstNode {
public:
    enum MacroType {P4, C};

    string* line_;
    MacroType macrotype_;
    IncludeNode(string* line, MacroType macrotype) {
        nodeType_ = typeid(*this).name();
        line_ = line;
        macrotype_ = macrotype;
    }

    string toString() {
        return *(line_);
    }   
};

class InputNode : public AstNode {
public: 
    InputNode* next_;
    AstNode* expression_;

    InputNode(AstNode* next, AstNode* expression) {
        nodeType_ = typeid(*this).name();
        next_ = dynamic_cast<InputNode*>(next);
        expression_ = expression;
        expression_->parent_ = this;
    }
    string toString() {
        ostringstream oss;
        if (next_) {
            oss << next_->toString() << "\n";
        }
        oss << expression_->toString();
        return oss.str();
    }
};

#endif