/*
File: TrBagg.h
Author: Samuel Barrett
Description: implementation of the TrBagg algorithm
Created:  2012-01-18
Modified: 2012-01-18
*/

#include "TrBagg.h"
#include <rl_pursuit/common/Util.h>
#include <fstream>
  
TrBagg::TrBagg(const std::vector<Feature> &features, bool caching, SubClassifierGenerator baseLearner, const Json::Value &baseLearnerOptions, unsigned int maxBoostingIterations, SubClassifierGenerator fallbackLearner, const Json::Value &fallbackLearnerOptions):
  Classifier(features,caching),
  baseLearner(baseLearner),
  baseLearnerOptions(baseLearnerOptions),
  fallbackLearner(fallbackLearner),
  fallbackLearnerOptions(fallbackLearnerOptions),
  data(numClasses),
  maxBoostingIterations(maxBoostingIterations),
  targetDataStart(-1),
  didPartialLoad(false)
{
}

void TrBagg::addData(const InstancePtr &instance) {
  if (targetDataStart < 0)
    targetDataStart = data.size();
  data.add(instance);
}

void TrBagg::addSourceData(const InstancePtr &instance) {
  assert(targetDataStart < 0);
  data.add(instance);
}

void TrBagg::outputDescription(std::ostream &out) const {
  out << "TrBagg: " << std::endl;
  for (unsigned int i = 0; i < classifiers.size(); i++)
    out << *(classifiers[i].classifier) << std::endl;
}

void TrBagg::save(const std::string &filename) const {
  saveSubClassifiers(classifiers,filename,getSubFilenames(filename,classifiers.size()));
}
  
bool TrBagg::load(const std::string &filename) {
  std::vector<SubClassifierGenerator> learners(2);
  learners[0] = baseLearner;
  learners[1] = fallbackLearner;
  std::vector<Json::Value> options(2);
  options[0] = baseLearnerOptions;
  options[1] = fallbackLearnerOptions;
  return createAndLoadSubClassifiers(classifiers,filename,features,learners,options);
}

bool TrBagg::partialLoad(const std::string &filename) {
  didPartialLoad = true;

  for (unsigned int i = 0; i < maxBoostingIterations; i++) {
    SubClassifier c;
    c.alpha = 1.0;
    c.classifier = baseLearner(features,baseLearnerOptions);
    c.classifier->load(getSubFilename(filename,i));
    partiallyLoadedClassifiers.push_back(c);
  }

  return true;
}

void TrBagg::clearData() {
  data.clearData();
  for (unsigned int i = 0; i < classifiers.size(); i++)
    classifiers[i].classifier->clearData();
}
  
void TrBagg::trainInternal(bool /*incremental*/) {
  classifiers.clear();
  if (targetDataStart < 0) {
    std::cerr << "WARNING: Trying to train TrBagg with no target data, just falling back on the fallback learner applied to the source data" << std::endl;
    SubClassifier fallbackModel;
    fallbackModel.classifier = fallbackLearner(features,fallbackLearnerOptions);
    for (unsigned int i = 0; i < data.size(); i++)
      fallbackModel.classifier->addData(data[i]);
    fallbackModel.classifier->train(false);
    fallbackModel.classifier->clearData();
    fallbackModel.alpha = 1.0;
    convertWekaToDT(fallbackModel);
    classifiers.push_back(fallbackModel);
    return;
  }
  
  //std::cout << "targetDataStart: " << targetDataStart << "  " << "data.size: " << data.size() << std::endl;
  int targetSize = data.size() - targetDataStart;
  assert(targetSize > 0);
  int sampleSize = 1 * targetSize;
  //int sampleSize = 1000;
  if (didPartialLoad) {
    for (unsigned int n = 0; n < partiallyLoadedClassifiers.size(); n++) {
      calcErrorOfClassifier(partiallyLoadedClassifiers[n]);
      insertClassifier(partiallyLoadedClassifiers[n]);
    }
    partiallyLoadedClassifiers.clear();
    didPartialLoad = false;
  } else {
    // LEARNING PHASE
    for (unsigned int n = 0; n < maxBoostingIterations; n++) {
      //if (n % 10 == 0)
        //std::cout << "BOOSTING ITERATION: " << n << std::endl;
      SubClassifier c;
      c.classifier = baseLearner(features,baseLearnerOptions);
      // sample data set with replacements
      //std::cout << "TRAINING DATA: " << n << std::endl;
      for (int i = 0; i < sampleSize; i++) {
        int32_t ind = rng->randomInt(data.size());
        //std::cout << "  " << ind << " " << *data[ind] << std::endl;
        c.classifier->addData(data[ind]);
      }
      c.classifier->train(false);
      convertWekaToDT(c);
      c.classifier->clearData();
      //std::cout << "CLASSIFIER: " << *c.classifier << std::endl;
      calcErrorOfClassifier(c);
      insertClassifier(c);
    }
  }
  // create the fallback model
  SubClassifier fallbackModel;
  fallbackModel.classifier = fallbackLearner(features,fallbackLearnerOptions);
  if (targetDataStart >= 0)  {
    for (int i = targetDataStart; i < (int)data.size(); i++)
      fallbackModel.classifier->addData(data[i]);
  }
  fallbackModel.classifier->train(false);
  convertWekaToDT(fallbackModel);
  calcErrorOfClassifier(fallbackModel);
  // add the fallback model to the beginning of the list
  classifiers.insert(classifiers.begin(),fallbackModel);

  //for (std::vector<SubClassifier>::iterator it = classifiers.begin(); it != classifiers.end(); it++) {
    //std::cout << it->alpha << " ";
  //}
  //std::cout << std::endl;
  
  unsigned int bestSize2 = selectSize(classifiers);
  std::cout << "CHOOSING SIZE: " << bestSize2 << std::endl;
  // remove other classifiers
  classifiers.resize(bestSize2);
}

void TrBagg::insertClassifier(const SubClassifier &c) {
  // add it to the list of models, but sort the list by error
  std::vector<SubClassifier>::iterator it;
  std::vector<SubClassifier>::iterator prev = classifiers.begin();
  if ((prev != classifiers.end()) && (prev->alpha > c.alpha)) {
    classifiers.insert(prev,c);
  } else {
    for (it = classifiers.begin(); it != classifiers.end(); it++) {
      if (it->alpha > c.alpha)
        break;
      prev = it;
    }
    classifiers.insert(it,c);
  }
}

void TrBagg::classifyInternal(const InstancePtr &instance, Classification &classification) {
  classifyInternal(instance,classification,classifiers);
}
  
void TrBagg::classifyInternal(const InstancePtr &instance, Classification &classification, const std::vector<SubClassifier> &classifiers) {
  float factor = 1.0 / classifiers.size();
  for (unsigned int j = 0; j < classifiers.size(); j++) {
    Classification temp(numClasses,0);
    classifiers[j].classifier->classify(instance,temp);
    for (unsigned int k = 0; k < numClasses; k++) {
      classification[k] += factor * temp[k];
    }
  }
}
  
void TrBagg::calcErrorOfClassifier(SubClassifier &c) {
  std::vector<SubClassifier> subset(1,c);
  c.alpha = calcErrorOfSet(subset);
}

double TrBagg::calcErrorOfSet(unsigned int size) {
  std::vector<SubClassifier> subset(classifiers.begin(),classifiers.begin() + size);
  return calcErrorOfSet(subset);
}

double TrBagg::calcErrorOfSet(const std::vector<SubClassifier> &classifiers) {
  double error = 0.0;
  //std::cout << "  -" << std::endl;
  for (unsigned int i = targetDataStart; i < data.size(); i++) {
    Classification c(numClasses,0);
    classifyInternal(data[i],c,classifiers);
    //std::cout << "  " << *data[i] << " -> " << c[data[i]->label] << std::endl;
    error += fabs(1.0 - c[data[i]->label]);
    //std::cout << "  " << i - targetDataStart << " " << error << std::endl;
  }
  return error;
}
  
unsigned int TrBagg::selectSize(const std::vector<SubClassifier> &classifiers) {
  // want to store classification per inst per classifier
  int targetSize = data.size() - targetDataStart;
  assert(targetSize > 0);
  std::vector<std::vector<Classification> > classifications(classifiers.size(),std::vector<Classification>(targetSize,Classification(numClasses,0)));
  for (unsigned int classifierInd = 0; classifierInd < classifiers.size(); classifierInd++) {
    for (int dataInd = 0; dataInd < targetSize; dataInd++) {
      classifiers[classifierInd].classifier->classify(data[dataInd + targetDataStart],classifications[classifierInd][dataInd]);
    }
  }

  unsigned int bestSize = 0;
  double bestError = std::numeric_limits<double>::infinity();
  double err;
  for (unsigned int size = 1; size < classifiers.size(); size++) {
    //std::cout << *(classifiers[size-1].classifier) << std::endl;
    err = calcErrorOfSet(size,classifications);
    //std::cout << size << ": " << err << std::endl;
    assert(!isnan(err));
    if (err < bestError) {
      bestError = err;
      bestSize = size;
    }
  }
  return bestSize;
}
  
double TrBagg::calcErrorOfSet(unsigned int size, const std::vector<std::vector<Classification> > &classifications) {
  float factor = 1.0 / size;
  double err = 0.0;
  //std::cout << "  -" << std::endl;
  for (int dataInd = targetDataStart; dataInd < (int)data.size(); dataInd++) {
    unsigned int &label = data[dataInd]->label;
    err += 1.0; // will reduce by the amount correct in the loop below
    //double temp = err;
    for (unsigned int classifierInd = 0; classifierInd < size; classifierInd++) {
      err -= factor * classifications[classifierInd][dataInd - targetDataStart][label];
    }
    //std::cout << "  " << *data[dataInd+targetSize] << " -> " << temp - err << std::endl;
    //std::cout << "  " << dataInd << " " << err << std::endl;
  }
  return err;
} 
