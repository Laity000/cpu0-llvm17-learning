#include "fstream"
#include "iostream"
#include "stack"
#include "cstdio"
#include "string.h"

#define SUPPORT_ELIF
//#define DEBUG

using namespace std;

enum FileType { CPP_FILE=0, CMAKE_FILE, LLVMBUILD_FILE, TD_FILE };
enum IfType {IF_CH, IF_OTHER};
enum {SUCCESS, FAIL};

struct chapterIdMap {
  char symbol[25];
  int id;
};

struct IfCtrType {
  IfType type; // IF_CH: my document definition e.g. #if CH >== CH3_1
  bool hidden; // true/false: this #if, #elif or #else ative block should be output or not
#ifdef SUPPORT_ELIF
  bool satisfied; // Has any this #if block satisfied in the condition, i.e. has been output
#endif
};

const struct chapterIdMap ch[] = {
  {"CH2", 20}, {"CH3_1", 31}, {"CH3_2", 32}, {"CH3_3", 33}, {"CH3_4", 34}, 
  {"CH3_5", 35}, {"CH4_1", 41}, {"CH4_2", 42}, {"CH5_1", 51}, {"CH6_1", 61}, 
  {"CH7_1", 71}, {"CH8_1", 81}, {"CH8_2", 82}, {"CH9_1", 91}, {"CH9_2", 92}, 
  {"CH9_3", 93}, {"CH9_4", 94}, {"CH10_1", 101}, {"CH11_1", 111},
  {"CH11_2", 112}, {"CH12_1", 141}, {"CHEND", -1}
};

class Preprocess {
private:
  char PatternIf[25];
  char PatternIfSpace[25];
#ifdef SUPPORT_ELIF
  char PatternElif[25];
  char PatternElifSpace[25];
#endif
  char PatternElse[25];
  char PatternEndif[25];
  enum {IFCH_IN_CONDITION=1, ELIFCH_IN_CONDITION, IFCH_OUT_CONDITION, 
        ELIFCH_OUT_CONDITION, IF, ELIF, ELSE, ENDIF, OTHER}; 
  
public:
  int currentChapter;
  void setFileType(FileType fileType);
  int getChapter(char* chapter);
  int checkPatternIfMacro(char* line);
  int run(ifstream& in, ofstream& out);
  void removeSuccessiveEmptyLines(ifstream& in, ofstream& out);
};

void Preprocess::setFileType(FileType fileType) {
  if (fileType == CPP_FILE || fileType == CMAKE_FILE || fileType == 
      LLVMBUILD_FILE) {
     strcpy(PatternIf, "#if");
     strcpy(PatternIfSpace, "#if ");
#ifdef SUPPORT_ELIF
     strcpy(PatternElif, "#elif");
     strcpy(PatternElifSpace, "#elif ");
#endif
     strcpy(PatternElse, "#else");
     strcpy(PatternEndif, "#endif");
  }
  else if (fileType == TD_FILE) {
     strcpy(PatternIf, "//#if");
     strcpy(PatternIfSpace, "//#if ");
#ifdef SUPPORT_ELIF
     strcpy(PatternElif, "//#elif");
     strcpy(PatternElifSpace, "//#elif ");
#endif
     strcpy(PatternElse, "//#else");
     strcpy(PatternEndif, "//#endif");
  }
}

int Preprocess::getChapter(char* chapter) {
  for (int i = 0; ch[i].id != -1; i++) {
    if (strcmp(ch[i].symbol, chapter) == 0)
	  return ch[i].id;
  }
  return -1;
}

int Preprocess::checkPatternIfMacro(char* line) {
  char If[20];
  char CH[20];
  char compareCond[20];
  char CHXX[20];
  int type = OTHER;
  
  if (strncmp(line, PatternIfSpace, strlen(PatternIfSpace)) == 0) {
	  sscanf(line, "%s %s %s", If, CH, compareCond);
	  if (strcmp(CH, "CH") == 0 && strcmp(compareCond, ">=") == 0) {
	  // #if CH ... (cpu0 chapter control pattern).
	    sscanf(line, "%s %s %s %s", If, CH, compareCond, CHXX);
	  #ifdef DEBUG
	    printf("if: %s CH: %s compareCond: %s CHXX: %s\n", If, CH, compareCond, CHXX);
	  #endif
        if (currentChapter >= getChapter(CHXX)) {
	      type = IFCH_IN_CONDITION;
        }	
        else {
	      type = IFCH_OUT_CONDITION;
	    }
	  }
	  else {
	    type = IF;
	  }
  }
  else if (strncmp(line, PatternIf, strlen(PatternIf)) == 0) {
    type = IF;
  }
#ifdef SUPPORT_ELIF
  else if (strncmp(line, PatternElifSpace, strlen(PatternElifSpace)) == 0) {
	  sscanf(line, "%s %s %s", If, CH, compareCond);
	  if (strcmp(CH, "CH") == 0 && strcmp(compareCond, ">=") == 0) {
	  // #if CH ... (cpu0 chapter control pattern).
	    sscanf(line, "%s %s %s %s", If, CH, compareCond, CHXX);
	  #ifdef DEBUG
	    printf("if: %s CH: %s compareCond: %s CHXX: %s\n", If, CH, compareCond, CHXX);
	  #endif
        if (currentChapter >= getChapter(CHXX)) {
	      type = ELIFCH_IN_CONDITION;
        }	
        else {
	      type = ELIFCH_OUT_CONDITION;
	    }
	  }
	  else {
	    type = ELIF;
	  }
  }
  else if (strncmp(line, PatternElif, strlen(PatternElif)) == 0) {
    type = ELIF;
  }
 #endif
  else if (strncmp(line, PatternElse, strlen(PatternElse)) == 0) {
    type = ELSE;
  }
  else if (strncmp(line, PatternEndif, strlen(PatternEndif)) == 0) {
    type = ENDIF;
  }
  else {
    type = OTHER;
  }
  
  return type;
}

// expand chapter added code through directive (eg. #if CH > CH3_1)
// for example, #define CH CH9_1:
// 1:  #if CH >= CH3_2
// 2:    addRegisterClass();
// 3:  #if 0
// 4:    setBooleanContents();
// 5:  #endif
// 6:  #if CH >= CH8_1
// 7:    unsigned Opc = MI->getOpcode();
// 8:  #else
// 9:    unsigned LO = Cpu0::LO;
// 10: #endif
// 11:   SmallString<128> Str;
// 12: #endif
//
// translate into:
// 1:    addRegisterClass();
// 2:  #if 0
// 3:    setBooleanContents();
// 4:  #endif
// 5:    unsigned Opc = MI->getOpcode();
// 6:    SmallString<128> Str;

// #if #endif pair processing belong to push automata (not finite state machine),
// So, solve this problem by stack.

// When read at line 1 (after process line 1), the output and stack content as follows:
// output: nothing // since (inCondition(CH, compareCond, CHXX))
// (IFCH, false)

// When read at line 2 (after process line ), output as follows:
// output: addRegisterClass(); // since !(stack.top().type == IF_CH && stack.top().hidden) == true

// When read at line 3, the output and stack content as follows:
// (IFOTHER, false)
// (IFCH, false)
// output: #if 0 // since strncmp(line, PatternIf, strlen(PatternIf)) == 0

// When read at line 4, output as follows:
// output: setBooleanContents(); // since !(stack.top().type == IF_CH && stack.top().hidden) == true

// When read at line 5, the output and stack content as follows:
// output: #endif // since (strncmp(line, PatternEndif, strlen(PatternEndif)) == 0) && (stack.top().type == IF_OTHER)
// (IFCH, false)

// When read at line 6, the output and stack content as follows:
// output: nothing // since (inCondition(CH, compareCond, CHXX))
// (IFCH, false)
// (IFCH, false)

// When read at line 7, output as follows:
// output: unsigned Opc = MI->getOpcode(); // since !(stack.top().type == IF_CH && stack.top().hidden) == true

// When read at line 8, the output and stack content as follows:
// output: nothing // since (strncmp(line, PatternElse, strlen(PatternElse)) == 0) && (stack.top().type == IF_CH)
// (IFCH, true)
// (IFCH, false)

// When read at line 9, output as follows:
// output: nothing // since !(stack.top().type == IF_CH && stack.top().hidden) == false

// When read at line 10, the stack content as follows:
// (IFCH, false)

int Preprocess::run(ifstream& in, ofstream& out) {
  char line[256];
  IfCtrType data;
  stack<IfCtrType> stack;
  
  // When IfCtrType.hidden == true, meaning don't output statement.

  while (!in.eof()) {
    in.getline(line, 255);
  #ifdef DEBUG
    printf("stack.size(): %lu\n", stack.size());
    printf("%s\n", line);
  #endif
    int type = checkPatternIfMacro(line);
    if (type == IFCH_IN_CONDITION) {
      data.type = IF_CH;
      if (stack.empty())
        data.hidden = false;
      else
        data.hidden = stack.top().hidden;
#ifdef SUPPORT_ELIF
      data.satisfied = true;
#endif
      stack.push(data);
    }
    else if (type == IFCH_OUT_CONDITION) {
      data.type = IF_CH;
      data.hidden = true;
#ifdef SUPPORT_ELIF
      data.satisfied = false;
#endif
      stack.push(data);
    }
    else if (type == IF) {
      if (stack.empty() || (stack.top().type != IF_CH) || !(stack.top().hidden))
        out << line << endl;
      data.type = IF_OTHER;
      if (stack.empty())
        data.hidden = false;
      else
        data.hidden = stack.top().hidden;
#ifdef SUPPORT_ELIF
      data.satisfied = true;
#endif
      stack.push(data);
    }
#ifdef SUPPORT_ELIF
    else if (type == ELIFCH_IN_CONDITION || type == ELIFCH_OUT_CONDITION) {
      if (stack.empty())
        return FAIL;
      if (stack.top().type == IF_CH) {
        data = stack.top();
        stack.pop();
        data.type = IF_CH;
        if (data.hidden) {
          if (type == ELIFCH_IN_CONDITION) {
            if (stack.empty()) {
              data.hidden = false;
              data.satisfied = true;
            }
            else {
              data.hidden = stack.top().hidden;
            }
          }
          else if (type == ELIFCH_OUT_CONDITION) {
            data.hidden = true;
          }
          stack.push(data);
        }
        else {
          data.hidden = true;
          stack.push(data);
        }
      }
    }
    else if (type == ELIF) {
      if (stack.empty())
        return FAIL;
      if ((stack.top().type != IF_CH) || !(stack.top().hidden))
        out << line << endl;
      data.type = IF_OTHER;
      if (stack.empty())
        data.hidden = false;
      else
        data.hidden = stack.top().hidden;
      data.satisfied = true;
      stack.push(data);
    }
#endif
    else if (type == ELSE) {
      if (stack.empty())
        return FAIL;
      if (stack.top().type == IF_CH) {
        data = stack.top();
        stack.pop();
        data.type = IF_CH;
#ifdef SUPPORT_ELIF
        if (data.satisfied) {
          data.hidden = true;
        }
        else {
#endif
          if (stack.empty()) {
            data.hidden = !data.hidden;
          }
          else
            if (stack.top().hidden)
              data.hidden = true;
            else
              data.hidden = !data.hidden;
#ifdef SUPPORT_ELIF
        }
#endif
        stack.push(data);
      }
      else {
      // stack.top().type == IF_OTHER
        if (!(stack.top().hidden))
          out << line << endl;
      }
    }
    else if (type == ENDIF) {
      if (stack.empty())
        return FAIL;
      if (!(stack.top().hidden) && (stack.top().type != IF_CH)) {
        out << line << endl;
      }
      stack.pop();
    }
    else if (type == OTHER) {
      if (stack.empty() || !(stack.top().hidden)) {
        out << line << endl;
      }
    }
  } // while
  if (stack.empty()) {
    return SUCCESS;
  }
  else {
    return FAIL;
  }
}

void Preprocess::removeSuccessiveEmptyLines(ifstream& in, ofstream& out) {
  char line[256];
  bool lastLineIsEmpty = false;
  
  while (!in.eof()) {
    in.getline(line, 255);
    if (strlen(line) == 0) {
      if (!lastLineIsEmpty)
        out << endl;
      lastLineIsEmpty = true;
    }
    else {
      out << line << endl;
      lastLineIsEmpty = false;
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    cout << "example: ./cpu0filepreprocess Cpu0/Cpu0AsmPrinter.cpp Chapter6_1/\
            Cpu0AsmPrinter.cpp CH6_1" << endl;
	  return 1;
  }
#ifdef DEBUG
  cout << "argv[1]: " << argv[1] << endl;
  cout << "argv[2]: " << argv[2] << endl;
  cout << "argv[3]: " << argv[3] << endl;
#endif
  ifstream in(argv[1]);
  if (!in.is_open()) {
    cout << "input file " << argv[1] << " not exist!" << endl;
    return 1;
  }
  
  Preprocess preprocess;
  if (strncmp(argv[2]+strlen(argv[2])-strlen(".cpp"), ".cpp", strlen(".cpp")) 
              == 0) {
    preprocess.setFileType(CPP_FILE);
  }
  else if (strncmp(argv[2]+strlen(argv[2])-strlen(".h"), ".h", strlen(".h")) 
                   == 0) {
    preprocess.setFileType(CPP_FILE);
  }
  else if (strncmp(argv[2]+strlen(argv[2])-strlen(".td"), ".td", strlen(".td")) 
                   == 0) {
    preprocess.setFileType(TD_FILE);
  }
  else if (strncmp(argv[2]+strlen(argv[2])-strlen("CMakeLists.txt"), 
                   "CMakeLists.txt", strlen("CMakeLists.txt")) == 0) {
    preprocess.setFileType(CMAKE_FILE);
  }
  else if (strncmp(argv[2]+strlen(argv[2])-strlen("LLVMBuild.txt"), 
                   "LLVMBuild.txt", strlen("LLVMBuild.txt")) == 0) {
    preprocess.setFileType(LLVMBUILD_FILE);
  }
  else {
    return 0;
  }

  ofstream out("tmp.txt");

  preprocess.currentChapter = preprocess.getChapter(argv[3]);
  int result = preprocess.run(in, out);
  in.close();
  out.close();
  if (result != SUCCESS) {
    cout << "Fail!!! #if #endif not matched" << endl;
    return FAIL;
  }
  in.open("tmp.txt");
  out.open(argv[2]);
  preprocess.removeSuccessiveEmptyLines(in, out);
  in.close();
  out.close();

  return SUCCESS;
}
