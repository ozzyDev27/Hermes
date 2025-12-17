#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <cmath>
#include <algorithm>
#include <memory>
#include <random>

using namespace std;

enum class ValueType {
    INT, FLOAT, STRING, BOOL, MAP, LIST, FUNCTION, CLASS_INSTANCE, NONE
};

class Value;
class Interpreter;
class ClassDefinition;

class Value {
public:
    ValueType type;
    int intVal;
    double floatVal;
    string stringVal;
    bool boolVal;
    map<string, Value> mapVal;
    vector<Value> listVal;
    shared_ptr<ClassDefinition> classInstance;
    map<string, Value> instanceVars;
    
    Value() : type(ValueType::NONE), intVal(0), floatVal(0.0), boolVal(false) {}
    Value(int v) : type(ValueType::INT), intVal(v), floatVal(0.0), boolVal(false) {}
    Value(double v) : type(ValueType::FLOAT), intVal(0), floatVal(v), boolVal(false) {}
    Value(string v) : type(ValueType::STRING), intVal(0), floatVal(0.0), stringVal(v), boolVal(false) {}
    Value(bool v) : type(ValueType::BOOL), intVal(0), floatVal(0.0), boolVal(v) {}
    
    string toString() const {
        switch(type) {
            case ValueType::INT: return to_string(intVal);
            case ValueType::FLOAT: return to_string(floatVal);
            case ValueType::STRING: return stringVal;
            case ValueType::BOOL: return boolVal ? "true" : "false";
            case ValueType::LIST: {
                string result = "[";
                for(size_t i = 0; i < listVal.size(); i++) {
                    result += listVal[i].toString();
                    if(i < listVal.size() - 1) result += ", ";
                }
                return result + "]";
            }
            default: return "none";
        }
    }
    
    bool toBool() const {
        if(type == ValueType::BOOL) return boolVal;
        if(type == ValueType::INT) return intVal != 0;
        if(type == ValueType::FLOAT) return floatVal != 0.0;
        if(type == ValueType::STRING) return !stringVal.empty();
        return false;
    }
};

class ClassDefinition {
public:
    string name;
    map<string, Value> variables;
    map<string, vector<string>> functions;
    map<string, vector<string>> functionParams;
    
    ClassDefinition() : name("") {}
    ClassDefinition(string n) : name(n) {}
};

class Interpreter {
private:
    map<string, Value> variables;
    map<string, ClassDefinition> classes;
    string mainClass;
    mt19937 rng;
    
    string trim(const string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }
    
    string removeComments(const string& line) {
        size_t pos = line.find("//");
        return pos != string::npos ? line.substr(0, pos) : line;
    }
    
    string interpolate(const string& str) {
        string result = str;
        regex pattern(R"(\{([^}]+)\})");
        smatch match;
        while (regex_search(result, match, pattern)) {
            Value val = evaluateExpression(match[1].str());
            result = result.substr(0, match.position()) + val.toString() + 
                     result.substr(match.position() + match.length());
        }
        return result;
    }
    
    string parseStringLiteral(string str) {
        if (str.size() >= 2 && str[0] == '"' && str.back() == '"')
            str = str.substr(1, str.size() - 2);
        
        string result;
        for (size_t i = 0; i < str.size(); i++) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                if (str[i+1] == 'n') result += '\n';
                else if (str[i+1] == 't') result += '\t';
                else if (str[i+1] == '\\') result += '\\';
                else if (str[i+1] == '"') result += '"';
                else result += str[i];
                i++;
            } else {
                result += str[i];
            }
        }
        return interpolate(result);
    }
    
    Value evaluateExpression(string expr) {
        expr = trim(expr);
        
        if (expr.size() >= 2 && expr[0] == '"' && expr.back() == '"')
            return Value(parseStringLiteral(expr));
        if (expr == "true") return Value(true);
        if (expr == "false") return Value(false);
        if (regex_match(expr, regex("-?[0-9]+"))) return Value(stoi(expr));
        if (regex_match(expr, regex("-?[0-9]+\\.[0-9]+"))) return Value(stod(expr));
        if (expr.find("[") != string::npos && expr.find(" for ") != string::npos)
            return evaluateListComprehension(expr);
        if (expr.find("(") != string::npos && expr.find("[") == string::npos)
            return evaluateFunctionCall(expr);
        if (expr.find("[") != string::npos && expr.find("]") != string::npos && expr.find(" for ") == string::npos)
            return evaluateArrayAccess(expr);
        if (expr.find(".") != string::npos) return evaluateMemberAccess(expr);
        if (expr.find("?") != string::npos && expr.find(":") != string::npos)
            return evaluateTernary(expr);
        
        if (expr.find(" or ") != string::npos) {
            size_t pos = expr.find(" or ");
            return Value(evaluateExpression(expr.substr(0, pos)).toBool() || 
                        evaluateExpression(expr.substr(pos + 4)).toBool());
        }
        
        if (expr.find(" and ") != string::npos) {
            size_t pos = expr.find(" and ");
            return Value(evaluateExpression(expr.substr(0, pos)).toBool() && 
                        evaluateExpression(expr.substr(pos + 5)).toBool());
        }
        
        for (string op : {"==", "!=", "<=", ">="}) {
            size_t pos = expr.find(op);
            if (pos != string::npos && pos > 0) {
                return evaluateComparison(evaluateExpression(trim(expr.substr(0, pos))), 
                                        op, evaluateExpression(trim(expr.substr(pos + op.length()))));
            }
        }
        
        for (string op : {"<", ">"}) {
            size_t pos = expr.find(op);
            if (pos != string::npos && pos > 0 && (pos + 1 >= expr.size() || expr[pos + 1] != '=')) {
                return evaluateComparison(evaluateExpression(trim(expr.substr(0, pos))), 
                                        op, evaluateExpression(trim(expr.substr(pos + 1))));
            }
        }
        
        for (string op : {"+", "-", "*", "/"}) {
            size_t pos = expr.rfind(op);
            if (pos != string::npos && pos > 0 && pos < expr.size() - 1) {
                return evaluateArithmetic(evaluateExpression(expr.substr(0, pos)), 
                                         op, evaluateExpression(expr.substr(pos + 1)));
            }
        }
        
        if (variables.find(expr) != variables.end()) return variables[expr];
        return Value();
    }
    
    Value evaluateComparison(const Value& left, const string& op, const Value& right) {
        bool result = false;
        if (left.type == ValueType::INT && right.type == ValueType::INT) {
            if (op == "==") result = left.intVal == right.intVal;
            else if (op == "!=") result = left.intVal != right.intVal;
            else if (op == "<") result = left.intVal < right.intVal;
            else if (op == ">") result = left.intVal > right.intVal;
            else if (op == "<=") result = left.intVal <= right.intVal;
            else if (op == ">=") result = left.intVal >= right.intVal;
        } else if (left.type == ValueType::STRING && right.type == ValueType::STRING && op == "==") {
            result = left.stringVal == right.stringVal;
        }
        return Value(result);
    }
    
    Value evaluateArithmetic(const Value& left, const string& op, const Value& right) {
        if (left.type == ValueType::INT && right.type == ValueType::INT) {
            int result = 0;
            if (op == "+") result = left.intVal + right.intVal;
            else if (op == "-") result = left.intVal - right.intVal;
            else if (op == "*") result = left.intVal * right.intVal;
            else if (op == "/") result = left.intVal / right.intVal;
            return Value(result);
        }
        if ((left.type == ValueType::FLOAT || left.type == ValueType::INT) &&
            (right.type == ValueType::FLOAT || right.type == ValueType::INT)) {
            double l = left.type == ValueType::FLOAT ? left.floatVal : left.intVal;
            double r = right.type == ValueType::FLOAT ? right.floatVal : right.intVal;
            double result = 0;
            if (op == "+") result = l + r;
            else if (op == "-") result = l - r;
            else if (op == "*") result = l * r;
            else if (op == "/") result = l / r;
            return Value(result);
        }
        return Value();
    }
    
    Value evaluateTernary(const string& expr) {
        size_t qpos = expr.find("?");
        size_t cpos = expr.find(":", qpos);
        return evaluateExpression(expr.substr(0, qpos)).toBool() ?
               evaluateExpression(expr.substr(qpos + 1, cpos - qpos - 1)) :
               evaluateExpression(expr.substr(cpos + 1));
    }
    
    Value evaluateArrayAccess(const string& expr) {
        size_t bracketPos = expr.find("[");
        string varName = trim(expr.substr(0, bracketPos));
        string indexExpr = expr.substr(bracketPos + 1, expr.rfind("]") - bracketPos - 1);
        
        if (indexExpr.find(":") != string::npos) {
            vector<string> parts;
            string part;
            for (size_t i = 0; i <= indexExpr.size(); i++) {
                if (i == indexExpr.size() || indexExpr[i] == ':') {
                    parts.push_back(part);
                    part = "";
                } else part += indexExpr[i];
            }
            
            if (parts.size() == 3 && parts[0].empty() && parts[1].empty() && parts[2] == "-1") {
                if (variables.find(varName) != variables.end()) {
                    if (variables[varName].type == ValueType::STRING) {
                        string str = variables[varName].stringVal;
                        reverse(str.begin(), str.end());
                        return Value(str);
                    } else if (variables[varName].type == ValueType::LIST) {
                        Value result;
                        result.type = ValueType::LIST;
                        result.listVal = variables[varName].listVal;
                        reverse(result.listVal.begin(), result.listVal.end());
                        return result;
                    }
                }
            }
            
            if (parts.size() >= 2 && variables.find(varName) != variables.end()) {
                int start = parts[0].empty() ? 0 : stoi(parts[0]);
                Value& var = variables[varName];
                int end = parts[1].empty() ? (var.type == ValueType::STRING ? var.stringVal.size() : var.listVal.size()) : stoi(parts[1]);
                
                if (var.type == ValueType::STRING) return Value(var.stringVal.substr(start, end - start));
                if (var.type == ValueType::LIST) {
                    Value result;
                    result.type = ValueType::LIST;
                    for (int i = start; i < end && i < (int)var.listVal.size(); i++)
                        result.listVal.push_back(var.listVal[i]);
                    return result;
                }
            }
        }
        
        Value index = evaluateExpression(indexExpr);
        if (variables.find(varName) != variables.end()) {
            Value& var = variables[varName];
            if (var.type == ValueType::LIST && index.type == ValueType::INT) {
                int idx = index.intVal < 0 ? var.listVal.size() + index.intVal : index.intVal;
                if (idx >= 0 && idx < (int)var.listVal.size()) return var.listVal[idx];
            }
            if (var.type == ValueType::STRING && index.type == ValueType::INT) {
                int idx = index.intVal < 0 ? var.stringVal.size() + index.intVal : index.intVal;
                if (idx >= 0 && idx < (int)var.stringVal.size()) return Value(string(1, var.stringVal[idx]));
            }
        }
        return Value();
    }
    
    Value evaluateMemberAccess(const string& expr) {
        size_t dotPos = expr.find(".");
        string objName = trim(expr.substr(0, dotPos));
        string member = trim(expr.substr(dotPos + 1));
        
        if (variables.find(objName) != variables.end()) {
            Value& obj = variables[objName];
            
            if (obj.type == ValueType::STRING && member.find("lower") != string::npos) {
                string result = obj.stringVal;
                transform(result.begin(), result.end(), result.begin(), ::tolower);
                return Value(result);
            }
            
            if (obj.type == ValueType::LIST) {
                if (member == "len()" || member == "len") return Value((int)obj.listVal.size());
                if (member == "sum()" || member == "sum") {
                    int sum = 0;
                    for (const auto& v : obj.listVal)
                        sum += (v.type == ValueType::INT ? v.intVal : (v.type == ValueType::BOOL ? v.boolVal : 0));
                    return Value(sum);
                }
                if (member.find("append(") != string::npos) {
                    size_t start = member.find("(") + 1;
                    size_t end = member.find(")");
                    obj.listVal.push_back(evaluateExpression(member.substr(start, end - start)));
                    return Value();
                }
            }
            
            if (obj.type == ValueType::CLASS_INSTANCE && obj.instanceVars.find(member) != obj.instanceVars.end())
                return obj.instanceVars[member];
        }
        
        if (objName == "math" && member.find("sqrt(") != string::npos) {
            size_t start = member.find("(") + 1, end = member.find(")");
            Value arg = evaluateExpression(member.substr(start, end - start));
            return Value(sqrt(arg.type == ValueType::INT ? arg.intVal : arg.floatVal));
        }
        
        if (objName == "random" && member.find("rng(") != string::npos)
            return Value((int)(rng() % 2));
        
        return Value();
    }
    
    Value evaluateListComprehension(const string& expr) {
        size_t bracketStart = expr.find("["), bracketEnd = expr.rfind("]");
        string comprehension = expr.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
        size_t forPos = comprehension.find(" for "), inPos = comprehension.find(" in ", forPos);
        
        string outputExpr = trim(comprehension.substr(0, forPos));
        string varDecl = trim(comprehension.substr(forPos + 5, inPos - forPos - 5));
        string iterableExpr = trim(comprehension.substr(inPos + 4));
        
        string varName = varDecl.find(" ") != string::npos ? 
                        trim(varDecl.substr(varDecl.find(" ") + 1)) : varDecl;
        
        Value iterable = evaluateExpression(iterableExpr);
        Value result;
        result.type = ValueType::LIST;
        
        Value oldVar;
        bool hadVar = variables.find(varName) != variables.end();
        if (hadVar) oldVar = variables[varName];
        
        if (iterable.type == ValueType::LIST) {
            for (const auto& item : iterable.listVal) {
                variables[varName] = item;
                result.listVal.push_back(evaluateExpression(outputExpr));
            }
        } else if (iterable.type == ValueType::STRING) {
            for (char c : iterable.stringVal) {
                variables[varName] = Value(string(1, c));
                result.listVal.push_back(evaluateExpression(outputExpr));
            }
        } else if (iterable.type == ValueType::INT) {
            for (int i = 0; i < iterable.intVal; i++) {
                variables[varName] = Value(i);
                result.listVal.push_back(evaluateExpression(outputExpr));
            }
        }
        
        if (hadVar) variables[varName] = oldVar;
        else variables.erase(varName);
        
        return result;
    }
    
    Value evaluateFunctionCall(const string& expr) {
        size_t parenPos = expr.find("(");
        string funcName = trim(expr.substr(0, parenPos));
        string argsStr = expr.substr(parenPos + 1, expr.rfind(")") - parenPos - 1);
        
        vector<Value> args;
        if (!argsStr.empty()) {
            stringstream ss(argsStr);
            string arg;
            while (getline(ss, arg, ',')) args.push_back(evaluateExpression(trim(arg)));
        }
        
        if (funcName == "print") {
            for (const auto& arg : args) cout << arg.toString();
            return Value();
        }
        if (funcName == "input") {
            string input;
            getline(cin, input);
            return Value(input);
        }
        if (funcName == "int") {
            if (!args.empty()) {
                if (args[0].type == ValueType::STRING) return Value(stoi(args[0].stringVal));
                if (args[0].type == ValueType::FLOAT) return Value((int)args[0].floatVal);
                if (args[0].type == ValueType::BOOL) return Value(args[0].boolVal ? 1 : 0);
            }
            return Value(0);
        }
        if (funcName == "float") {
            if (!args.empty()) {
                if (args[0].type == ValueType::INT) return Value((double)args[0].intVal);
                if (args[0].type == ValueType::STRING) return Value(stod(args[0].stringVal));
            }
            return Value(0.0);
        }
        if (funcName == "bool") return !args.empty() ? Value(args[0].toBool()) : Value(false);
        if (funcName == "round" && args.size() >= 2) {
            double val = args[0].type == ValueType::FLOAT ? args[0].floatVal : args[0].intVal;
            double multiplier = pow(10, args[1].intVal);
            return Value(::round(val * multiplier) / multiplier);
        }
        if (funcName == "ceil" && !args.empty()) {
            double val = args[0].type == ValueType::FLOAT ? args[0].floatVal : args[0].intVal;
            return Value(ceil(val));
        }
        if (classes.find(funcName) != classes.end()) {
            Value instance;
            instance.type = ValueType::CLASS_INSTANCE;
            instance.classInstance = make_shared<ClassDefinition>(classes[funcName]);
            return instance;
        }
        return Value();
    }
    
    void executeStatement(const string& line) {
        string stmt = trim(removeComments(line));
        if (stmt.empty()) return;
        
        regex varDeclPattern(R"((int|float|str|bool|map)\s+(\w+)\s*=\s*(.+))");
        smatch match;
        if (regex_match(stmt, match, varDeclPattern)) {
            variables[match[2]] = evaluateExpression(match[3]);
            return;
        }
        
        regex varDeclPattern2(R"((int|float|str|bool|map|str\[\]|bool\[\])\s+(\w+))");
        if (regex_match(stmt, match, varDeclPattern2)) {
            variables[match[2]] = match[1] == "int" ? Value(0) : Value();
            if (match[1] == "map") variables[match[2].str()].type = ValueType::MAP;
            else if (match[1].str().find("[]") != string::npos) variables[match[2].str()].type = ValueType::LIST;
            return;
        }
        
        regex assignPattern(R"((\w+)\s*=\s*(.+))");
        if (regex_match(stmt, match, assignPattern)) {
            variables[match[1]] = evaluateExpression(match[2]);
            return;
        }
        
        regex memberAssignPattern(R"((\w+)\.(\w+)\s*=\s*(.+))");
        if (regex_match(stmt, match, memberAssignPattern)) {
            if (variables.find(match[1]) != variables.end() && 
                variables[match[1].str()].type == ValueType::CLASS_INSTANCE) {
                variables[match[1].str()].instanceVars[match[2]] = evaluateExpression(match[3]);
            }
            return;
        }
        
        if (stmt.find("++") != string::npos) {
            string varName = trim(stmt.substr(0, stmt.find("++")));
            if (variables.find(varName) != variables.end() && variables[varName].type == ValueType::INT)
                variables[varName].intVal++;
            return;
        }
        
        if (stmt.find("*=") != string::npos) {
            size_t pos = stmt.find("*=");
            string varName = trim(stmt.substr(0, pos));
            Value val = evaluateExpression(stmt.substr(pos + 2));
            if (variables.find(varName) != variables.end() && 
                variables[varName].type == ValueType::INT && val.type == ValueType::INT)
                variables[varName].intVal *= val.intVal;
            return;
        }
        
        if (stmt.find("(") != string::npos) evaluateFunctionCall(stmt);
    }
    
    void executeBlock(const vector<string>& lines, size_t& i) {
        while (i < lines.size()) {
            string line = trim(removeComments(lines[i]));
            if (line.empty()) { i++; continue; }
            
            if (line.find("while") == 0) {
                string condition = line.substr(line.find("(") + 1, line.find(")") - line.find("(") - 1);
                vector<string> loopBlock;
                i += 2;
                int braceCount = 1;
                while (i < lines.size() && braceCount > 0) {
                    if (lines[i].find("{") != string::npos) braceCount++;
                    if (lines[i].find("}") != string::npos && --braceCount == 0) break;
                    loopBlock.push_back(lines[i++]);
                }
                while (evaluateExpression(condition).toBool()) {
                    size_t idx = 0;
                    executeBlock(loopBlock, idx);
                }
                i++;
                continue;
            }
            
            if (line.find("for") == 0) {
                string forHeader = line.substr(line.find("(") + 1, line.find(")") - line.find("(") - 1);
                size_t inPos = forHeader.find(" in ");
                string varName = trim(forHeader.substr(forHeader.find(" ", 0) + 1, inPos - forHeader.find(" ", 0) - 1));
                string iterableExpr = trim(forHeader.substr(inPos + 4));
                
                vector<string> loopBlock;
                i += 2;
                int braceCount = 1;
                while (i < lines.size() && braceCount > 0) {
                    if (lines[i].find("{") != string::npos) braceCount++;
                    if (lines[i].find("}") != string::npos && --braceCount == 0) break;
                    loopBlock.push_back(lines[i++]);
                }
                
                Value iterable = evaluateExpression(iterableExpr);
                if (iterable.type == ValueType::INT) {
                    for (int idx = 0; idx < iterable.intVal; idx++) {
                        variables[varName] = Value(idx);
                        size_t blockIdx = 0;
                        executeBlock(loopBlock, blockIdx);
                    }
                } else if (iterable.type == ValueType::STRING) {
                    for (char c : iterable.stringVal) {
                        variables[varName] = Value(string(1, c));
                        size_t blockIdx = 0;
                        executeBlock(loopBlock, blockIdx);
                    }
                } else if (iterable.type == ValueType::LIST) {
                    for (const auto& item : iterable.listVal) {
                        variables[varName] = item;
                        size_t blockIdx = 0;
                        executeBlock(loopBlock, blockIdx);
                    }
                }
                i++;
                continue;
            }
            
            if (line.find("return") == 0) { i++; continue; }
            executeStatement(line);
            i++;
        }
    }
    
public:
    Interpreter() {
        random_device rd;
        rng.seed(rd());
    }
    
    void loadProgram(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Could not open file " << filename << endl;
            return;
        }
        
        vector<string> lines;
        string line;
        while (getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
        
        // Parse the program
        for (size_t i = 0; i < lines.size(); i++) {
            string ln = trim(removeComments(lines[i]));
            
            // Parse main class declaration
            if (ln.find("$") == 0) {
                mainClass = trim(ln.substr(1));
            }
            
            // Parse imports (simplified - not fully implemented)
            if (ln.find("#") == 0 || ln.find("@") == 0) {
                // Import handling would go here
                continue;
            }
            
            // Parse class definitions
            if (ln.find("class ") == 0) {
                size_t nameEnd = ln.find(" {");
                if (nameEnd == string::npos) nameEnd = ln.find("{");
                string className = trim(ln.substr(6, nameEnd - 6));
                
                ClassDefinition classDef(className);
                i++;
                
                // Parse class body
                int braceCount = 1;
                vector<string> classBody;
                
                while (i < lines.size() && braceCount > 0) {
                    string classLine = lines[i];
                    if (classLine.find("{") != string::npos) braceCount++;
                    if (classLine.find("}") != string::npos) {
                        braceCount--;
                        if (braceCount == 0) break;
                    }
                    classBody.push_back(classLine);
                    i++;
                }
                
                classes[className] = classDef;
            }
        }
    }
    
    void run() {
        if (mainClass.empty() || classes.find(mainClass) == classes.end()) {
            cerr << "Error: Main class not found" << endl;
            return;
        }
        
        // Load the main class and execute it
        loadProgram("program.hm");
        
        // Re-parse to execute main class
        ifstream file("program.hm");
        vector<string> lines;
        string line;
        while (getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
        
        // Find and execute main class
        bool inMainClass = false;
        vector<string> mainClassBody;
        
        for (size_t i = 0; i < lines.size(); i++) {
            string ln = trim(removeComments(lines[i]));
            
            if (ln.find("class " + mainClass) == 0) {
                inMainClass = true;
                i++;
                int braceCount = 1;
                
                while (i < lines.size() && braceCount > 0) {
                    string classLine = lines[i];
                    if (classLine.find("{") != string::npos) braceCount++;
                    if (classLine.find("}") != string::npos) {
                        braceCount--;
                        if (braceCount == 0) break;
                    }
                    
                    // Skip function definitions in first pass
                    string trimmedLine = trim(removeComments(classLine));
                    if (trimmedLine.find("fn ") != 0) {
                        mainClassBody.push_back(classLine);
                    } else {
                        // Skip function body
                        int fnBraceCount = 0;
                        while (i < lines.size()) {
                            if (lines[i].find("{") != string::npos) {
                                fnBraceCount++;
                                break;
                            }
                            i++;
                        }
                        i++;
                        while (i < lines.size() && fnBraceCount > 0) {
                            if (lines[i].find("{") != string::npos) fnBraceCount++;
                            if (lines[i].find("}") != string::npos) fnBraceCount--;
                            i++;
                        }
                        i--;
                    }
                    i++;
                }
                break;
            }
        }
        
        // Execute main class body
        size_t idx = 0;
        executeBlock(mainClassBody, idx);
    }
};

int main() {
    Interpreter interpreter;
    interpreter.loadProgram("program.hm");
    interpreter.run();
    
    return 0;
}
