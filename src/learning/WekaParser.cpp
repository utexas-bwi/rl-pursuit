#include "WekaParser.h"
#include <iostream>
#include <cassert>
#include <model/Common.h>
#include "ArffReader.h"

WekaParser::WekaParser(const std::string &filename, unsigned int numClasses, bool useClassDistributions):
  in(filename.c_str()),
  numClasses(numClasses),
  useClassDistributions(useClassDistributions)
{
  assert(in.good());
  ArffReader arff(in);
  featureTypes = arff.getFeatureTypes();
  //classFeature = arff.getClassFeature();
  
  //valueMap["U"] = Action::UP;
  //valueMap["D"] = Action::DOWN;
  //valueMap["L"] = Action::LEFT;
  //valueMap["R"] = Action::RIGHT;
  //valueMap["S"] = Action::NOOP;

  tokenizeFile();
}

boost::shared_ptr<DecisionTree> WekaParser::makeDecisionTree() {
  for(unsigned int i = 0; i < lines.size(); i++)
    lines[i].used = false;
  boost::shared_ptr<DecisionTree::Node> root = readDecisionTreeNode(0,0);
  return boost::shared_ptr<DecisionTree>(new DecisionTree(featureTypes,root));
}

boost::shared_ptr<DecisionTree::Node> WekaParser::readDecisionTreeNode(unsigned int lineInd, unsigned int currentDepth) {
  //std::cout << lineInd << " " << currentDepth << std::endl;
  if (lines[lineInd].used && !lines[lineInd].leaf) {
    // finished with this line
    //std::cout << "FINISHED THIS LINE" << std::endl;
    return boost::shared_ptr<DecisionTree::Node>();
  }

  // handle leaves specially
  if (lines[lineInd].used && lines[lineInd].leaf && (currentDepth == lines[lineInd].depth + 1)) {
    // make a leaf for this line
    //boost::shared_ptr<DecisionTree::Node> node(new DecisionTree::LeafNode((int)(lines[lineInd].classification + 0.5)));
    InstanceSetPtr instances(new InstanceSet(numClasses));
    instances->classification = lines[lineInd].classDistribution;
    DecisionTree::NodePtr node(new DecisionTree::LeafNode(instances));
    //boost::shared_ptr<DecisionTree::Node> node(new DecisionTree::LeafNode(lines[lineInd].classDistribution));
    //std::cout << "Making leaf: " << node;
    return node;
  }
  
  // check the depth
  if (currentDepth != lines[lineInd].depth) {
    std::cout << "BAD DEPTH" << std::endl;
    return boost::shared_ptr<DecisionTree::Node>();
  }

 
  lines[lineInd].used = true;
  //std::cout << "MAKING INTERIOR: " << lines[lineInd].name << std::endl;
  boost::shared_ptr<DecisionTree::InteriorNode> node(new DecisionTree::InteriorNode(lines[lineInd].op,lines[lineInd].name));
  boost::shared_ptr<DecisionTree::Node> child;
  for (unsigned int i = lineInd; i < lines.size(); i++) {
    if (lines[i].depth == currentDepth) {
      lines[i].used = true;
      if (lines[i].leaf)
        child = readDecisionTreeNode(i, currentDepth + 1);
      else
        child = readDecisionTreeNode(i + 1, currentDepth + 1);
      node->addChild(child,lines[i].val);
    }
    if (lines[i].depth < currentDepth)
      break;
  }

  //std::cout << "DONE MAKING INTERIOR: "  << lines[lineInd].name << std::endl;
  return node;
}

void WekaParser::tokenizeFile() {
  Line line;
  tokenizeLine(line);
  while (!in.eof()) {
    lines.push_back(line);
    tokenizeLine(line);
  }
  //std::cout << "NUM LINES: " << lines.size() << std::endl;
}

void WekaParser::tokenizeLine(Line &line) {
  //std::cout << "top TokenizeLine" << std::endl;
  line.used = false;
  line.depth = 0;
  
  std::string str;
  while (true) {
    str = readWekaToken(false);
    if (str == "|")
      line.depth++;
    else
      break;
  }
  if (in.eof()) {
    return;
  }
  // read the name
  // don't need to read token, since we did above
  line.name = str;
  // read the operator
  str = readWekaToken(false);
  line.op = stringToOperator(str);
  // read the splitValue
  str = readWekaToken(false);
  line.val = stringToVal(str,line.name);
  // read the rest of the line
  str = readWekaToken(true);
  if (str == ":") {
    // read the class
    line.leaf = true;
    line.classDistribution = Classification(numClasses,0);
    if (useClassDistributions) {
      for (unsigned int i = 0; i < numClasses; i++) {
        //std::cout << i << " ";
        str = readWekaToken(false,true);
        //std::cout << str << std::endl;
        line.classDistribution[i] = stringToVal(str,"classification");
      }
    } else {
      str = readWekaToken(false);
      float val = stringToVal(str,"classification");
      int ind = (int)(val + 0.5);
      assert ((ind >= 0) && (ind < (int)numClasses));
      line.classDistribution[ind] = 1;
    }
  } else {
    line.leaf = false;
  }
  while (str != "\n")
    str = readWekaToken(true);

  //std::cout << "LINE: " << line.depth << " " << line.name << " " << line.op << " " << line.val;
  //if (line.leaf)
    //std::cout << " class:" << line.classification;
  //std::cout << std::endl;
}

std::string WekaParser::readWekaToken(bool acceptNewline, bool breakOnSpace) {
  std::string token;
  std::string spaces;
  bool readingOperator = false;
  char ch;
  while (true) {
    if (in.eof())
      return token;
    ch = in.get();
    if (ch == ' ') {
      if ((breakOnSpace) && (token.size() > 0))
        return token;
      spaces += ch;
      continue;
    }
    if (ch == '\n') {
      if (token.size() == 0) {
        token += ch;
        if (!acceptNewline) {
          std::cerr << "WekaParser::readWekaToken: ERROR unexpected newline" << std::endl;
          exit(5);
        }
      }
      else
        in.unget();
      return token;
    }
    if ((ch == '|') || (ch == ':')) {
      if (token.size() == 0)
        token += ch;
      else
        in.unget();
      return token;
    }

    if (ch == '(') {
      if (token.size() == 0)
        token += ch;
      else {
        in.unget();
        return token;
      }
    }
    if ((ch == '=') || (ch == '<') || (ch == '>')) {
      if (token.size() == 0) { // first character
        readingOperator = true;
        token += ch;
      } else if (readingOperator)
        token += ch;
      else {
        in.unget();
        return token;
      }
    } else if (readingOperator) {
      in.unget();
      return token;
    }
    else {
      if (token.size() > 0)
        token += spaces;
      spaces.clear();
      token += ch;
    }
  }
}

DecisionTree::ComparisonOperator WekaParser::stringToOperator(const std::string &str) {
  if (str == "=")
    return DecisionTree::EQUALS;
  else if (str == "<")
    return DecisionTree::LESS;
  else if (str == ">=")
    return DecisionTree::GEQ;
  else if (str == "<=")
    return DecisionTree::LEQ;
  else if (str == ">")
    return DecisionTree::GREATER;
  std::cerr << "WekaParser::stringToOperator: ERROR bad operator string: " << str << std::endl;
  exit(5);
}

float WekaParser::stringToVal(const std::string &str, const std::string &) {
  float val;
  try {
    val = boost::lexical_cast<float>(str);
  } catch (boost::bad_lexical_cast) {
    //val = valueMap[name][str];
    //val = valueMap[str];
    throw;
  }
  //std::cout << "stringToVal(" << str << "," << name << "): " << val << std::endl;
  return val;
}
