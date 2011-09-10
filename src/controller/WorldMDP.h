#ifndef WORLDMDP_CNHINAVX
#define WORLDMDP_CNHINAVX

/*
File: WorldMDP.h
Author: Samuel Barrett
Description: an mdp wrapper of a world
Created:  2011-08-23
Modified: 2011-08-23
*/

#include <boost/shared_ptr.hpp>
#include <ostream>

#include <planning/Model.h>
#include <common/RNG.h>
#include <model/WorldModel.h>
#include <controller/World.h>
#include <controller/AgentDummy.h>
  
const unsigned int STATE_SIZE = 5;

struct State_t {
  State_t() {};
  State_t(const Observation &obs);
  Point2D positions[STATE_SIZE];
  bool operator<(const State_t &other) const;
  bool operator==(const State_t &other) const;
};
std::ostream& operator<<(std::ostream &out, const State_t &state);
std::size_t hash_value(const State_t &s);

class WorldMDP: public Model<State_t,Action::Type> {
public:
  WorldMDP(boost::shared_ptr<RNG> rng, boost::shared_ptr<WorldModel> model, boost::shared_ptr<World> controller, boost::shared_ptr<AgentDummy> adhocAgent);

  virtual void setState(const State_t &state);
  virtual void takeAction(const Action::Type &action, float &reward, State_t &state, bool &terminal);
  virtual float getRewardRangePerStep();
  virtual std::string generateDescription(unsigned int indentation = 0);

protected:
  boost::shared_ptr<RNG> rng;
  boost::shared_ptr<WorldModel> model;
  boost::shared_ptr<World> controller;
  boost::shared_ptr<AgentDummy> adhocAgent;
  State_t rolloutStartState;
};

#endif /* end of include guard: WORLDMDP_CNHINAVX */
