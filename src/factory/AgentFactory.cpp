#include "AgentFactory.h"

#include <cstdarg>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>

#include <controller/AgentRandom.h>
#include <controller/AgentDummy.h>
#include <controller/PredatorDecisionTree.h>
#include <controller/PredatorGreedy.h>
#include <controller/PredatorGreedyProbabilistic.h>
#include <controller/PredatorMCTS.h>
#include <controller/PredatorProbabilisticDestinations.h>
#include <controller/PredatorStudentCpp.h>
#include <controller/PredatorStudentPython.h>
#include <controller/PredatorTeammateAware.h>
#include <controller/WorldMDP.h>
#include <planning/UCTEstimator.h>
#include <factory/PlanningFactory.h>

#define NAME_IN_SET(...) nameInSet(name,__VA_ARGS__,NULL)

bool nameInSet(const std::string &name, ...) {
  va_list vl;
  char *s;
  bool found = false;
  va_start(vl,name);
  while (true) {
    s = va_arg(vl,char *);
    if (s == NULL)
      break;
    else if (name == s) {
      found = true;
      break;
    }
  }
  va_end(vl);
  return found;
}

bool pickStudentFromFile(const std::string &filename, std::string &student, unsigned int randomNum) {
  std::ifstream in(filename.c_str());
  if (!in.good())
    return false;
  std::vector<std::string> students;
  std::string name;
  in >> name;
  while (in.good()) {
    //std::cout << "adding: " << name << std::endl;
    students.push_back(name);
    in >> name;
  }
  in.close();
  //std::cout << "NUM STUDENTS: " << students.size() << std::endl;
  if (students.size() == 0)
    return false;
  student = students[randomNum % students.size()];
  //std::cout << student << std::endl;
  return true;
}

boost::shared_ptr<Agent> createAgent(boost::shared_ptr<RNG> rng, const Point2D &dims, std::string name, unsigned int randomNum, const Json::Value &options, const Json::Value &rootOptions) {
  typedef boost::shared_ptr<Agent> ptr;
  
  boost::to_lower(name);
  if (NAME_IN_SET("prey","preyrandom","random"))
    return ptr(new AgentRandom(rng,dims));
  else if (NAME_IN_SET("greedy","gr"))
    return ptr(new PredatorGreedy(rng,dims));
  else if (NAME_IN_SET("greedyprobabilistic","greedyprob","gp"))
    return ptr(new PredatorGreedyProbabilistic(rng,dims));
  else if (NAME_IN_SET("probabilisticdestinations","probdests","pd"))
    return ptr(new PredatorProbabilisticDestinations(rng,dims));
  else if (NAME_IN_SET("teammate-aware","ta"))
    return ptr(new PredatorTeammateAware(rng,dims));
  else if (NAME_IN_SET("dummy"))
    return ptr(new AgentDummy(rng,dims));
  else if (NAME_IN_SET("dt","decision","decisiontree","decision-tree")) {
    std::string filename = options.get("filename","").asString();
    return ptr(new PredatorDecisionTree(rng,dims,filename));
  } else if (NAME_IN_SET("student")) {
    std::string student = options.get("student","").asString();
    if (student == "") {
      std::cerr << "createAgent: ERROR: no student type specified" << std::endl;
      exit(3);
    }

    pickStudentFromFile(student,student,randomNum);

    int predatorInd = options.get("predatorInd",-1).asInt();
    if ((predatorInd < 0) || (predatorInd >= 4)) {
      std::cerr << "createAgent: ERROR: bad predator ind specified for student: " << student << std::endl;
      exit(3);
    }
    if (PredatorStudentCpp::handlesStudent(student))
      return ptr(new PredatorStudentCpp(rng,dims,student,predatorInd));
    else
      return ptr(new PredatorStudentPython(rng,dims,student,predatorInd));
  } else if (NAME_IN_SET("mcts","uct")) {
    Json::Value plannerOptions = rootOptions["planner"];
    // process the depth if necessary
    unsigned int depth = plannerOptions.get("depth",0).asUInt();
    if (depth == 0) {
      unsigned int depthFactor = plannerOptions.get("depthFactor",0).asUInt();
      plannerOptions["depth"] = depthFactor * (dims.x + dims.y);
    }
    
    // create the mdp
    boost::shared_ptr<WorldMultiModelMDP> mdp = createWorldMultiModelMDP(rng,dims,plannerOptions);
    // create the value estimator
    boost::shared_ptr<UCTEstimator<State_t,Action::Type> > uct = createUCTEstimator(rng->randomUInt(),Action::NUM_ACTIONS,plannerOptions);
    // create the planner
    boost::shared_ptr<MCTS<State_t,Action::Type> > mcts = createMCTS(mdp,uct,plannerOptions);

    return ptr(new PredatorMCTS(rng,dims,mcts,mdp));
  } else {
    std::cerr << "createAgent: unknown agent name: " << name << std::endl;
    assert(false);
  }
}

boost::shared_ptr<Agent> createAgent(unsigned int randomSeed, const Point2D &dims, std::string name, unsigned int randomNum, const Json::Value &options, const Json::Value &rootOptions) {
  boost::shared_ptr<RNG> rng(new RNG(randomSeed));
  return createAgent(rng,dims,name,randomNum,options,rootOptions);
}

boost::shared_ptr<Agent> createAgent(unsigned int randomSeed, const Point2D &dims, unsigned int randomNum, const Json::Value &options, const Json::Value &rootOptions) {
  std::string name = options.get("behavior","NONE").asString();
  if (name == "NONE") {
    std::cerr << "createAgent: WARNING: no agent type specified, using random" << std::endl;
    name = "random";
  }

  return createAgent(randomSeed,dims,name,randomNum,options,rootOptions);
}
