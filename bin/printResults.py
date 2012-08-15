#!/usr/bin/env python

import numpy, csv, os

def loadResults(path):
  if os.path.isfile(path):
    res = loadResultsFromFile(path)
  else:
    res = loadResultsFromDirectory(path)
    if res is None:
      res = loadResultsFromDirectory(os.path.join(path,'results'))
  return res

def loadResultsFromFile(filename):
  numSteps = []
  with open(filename,'r') as f:
    reader = csv.reader(f)
    for row in reader:
      iteration = int(row[0])
      stepsPerTrial = map(int,row[1:])
      numSteps.append([iteration] + stepsPerTrial)
  numSteps = numpy.array(numSteps)
  return numSteps

def validResultFile(filename):
  _,ext =  os.path.splitext(filename)
  return ext == '.csv'

def loadResultsFromDirectory(directory):
  if not(os.path.isdir(directory)):
    return []
  tempFilenames = [os.path.join(directory,x) for x in os.listdir(directory)]
  filenames = filter(validResultFile,tempFilenames)
  if len(filenames) == 0:
    return None
  #filenames.sort(key=lambda x:int(os.path.basename(x).replace('.csv','')))
  return loadResultsFromFileSet(filenames)

def loadResultsFromFileSet(filenames):
  numSteps = None
  for filename in filenames:
    res = loadResults(filename)
    if numSteps is None:
      numSteps = res
    else:
      try:
        numSteps = numpy.vstack((numSteps,res))
      except:
        pass
  return numSteps

def printResults(episodeLengths,label,outputCsv,outputHeader):
  #for i in range(len(episodeLengths)):
    #for j in range(len(episodeLengths[i])):
      #if episodeLengths[i][j] > 1000:
        #episodeLengths[i][j] = 1000
  if outputCsv:
    if outputHeader:
      #print 'label, Num episodes, mean, means, median, std, min, max'
      print 'label, Num episodes, mean, median, std, min, max'
    vals = [label]
    if (episodeLengths is None) or (len(episodeLengths) == 0):
      vals.append(0)
    else:
      vals.append(len(episodeLengths))
      vals.append(numpy.mean(episodeLengths))
      #vals.append(numpy.mean(episodeLengths,0))
      vals.append(numpy.median(episodeLengths))
      vals.append(numpy.std(episodeLengths))
      vals.append(numpy.min(episodeLengths))
      vals.append(numpy.max(episodeLengths))
    print ','.join(map(str,vals))
  else:
    print '-----------------------------------'
    print label
    if episodeLengths is None:
      print 'Num episodes = ',0
      return
    print 'Num episodes = ',len(episodeLengths)
    print 'mean=',numpy.mean(episodeLengths)
    #print 'means=',numpy.mean(episodeLengths,0)
    print 'median=',numpy.median(episodeLengths)
    print 'std=',numpy.std(episodeLengths)
    print 'min,max=',numpy.min(episodeLengths),numpy.max(episodeLengths)
    #print episodeLengths[:10]
    #print numpy.median(episodeLengths)
    #from scipy import stats
    #print [stats.scoreatpercentile(episodeLengths,x)[0] for x in [10,90]]
    #episodeLengths.sort(axis=0)
    #for amount in [10,25,50,100]:
      #print amount,numpy.mean(episodeLengths[amount:len(episodeLengths)-amount])

def getStudentInds(path,includeStudents,excludeStudents):
  with open(path,'r') as f:
    students = [x.strip() for x in f.readlines()]
  # sanity check some stuff
  for student in includeStudents:
    if student not in students:
      print >>sys.stderr,'ERROR: Unknown include student: %s' % student
      sys.exit(1)
  for student in excludeStudents:
    if student not in students:
      print >>sys.stderr,'ERROR: Unknown exclude student: %s' % student
      sys.exit(1)
  # get the indices
  inds = []
  for i,student in enumerate(students):
    if (len(includeStudents) > 0) and (student not in includeStudents):
      continue
    if student in excludeStudents:
      continue
    inds.append(i)
  return inds

def main(paths,options):
  studentInds = getStudentInds('data/newStudents29.txt',options.includeStudents,options.excludeStudents)
  numEpisodes = len(studentInds)
  if options.useQuantile:
    fracToRemove = numpy.floor((1.0 - options.quantile) * 0.5 * numEpisodes + 0.5)
    print >>sys.stderr,'Removing bottom and top %i episodes for quantile %g' % (fracToRemove,options.quantile)
  if options.matchNumEpisodes:
    print 'MATCHING number of episodes, via sorting first axis (MIGHT BE WRONG)'
  for i,path in enumerate(paths):
    res = loadResults(path)
    if res is None:
      numSteps = None
    else:
      trials = res[:,0]
      numSteps = res[:,1:]
      inds = []
      for j,trial in enumerate(trials):
        if trial in studentInds:
          inds.append(j)
      numSteps = numSteps[inds,:]
    if options.matchNumEpisodes:
      numEpisodes = min(numEpisodes,len(numSteps))
      numSteps.sort(axis=0)
      numSteps = numSteps[:numEpisodes,:]
    else:
      if options.useQuantile and (numSteps is not None):
        numSteps.sort(axis=0)
        numSteps = numSteps[fracToRemove:-fracToRemove]
        #print 'resulting size: %i' % len(numSteps)
    printResults(numSteps,path,options.outputCsv,i==0)
  #for filenameList in filenames:
    #filenameList = flatten(map(getFilenames,filenameList))
    #numSteps = loadResultsFromFileSet(filenames)
    #print numSteps.size,numSteps.mean()
  #else:
    #for filename in filenames:
      #print filename
      #numSteps = loadResults(filename)
      #print numSteps.size,numSteps.mean()

def mainArgs(args):
  from optparse import OptionParser
  parser = OptionParser('printResults.py [options] result1.csv [result2.csv ...]\nNOTE: can take directories or files')
  parser.add_option('-c','--csv',action='store_true',dest='outputCsv',default=False,help='output in csv format')
  parser.add_option('-i','--include',action='append',dest='includeStudents',default=[],help='output only for specified students',metavar='STUDENT')
  parser.add_option('-x','--exclude',action='append',dest='excludeStudents',default=[],help='output excluding specified students',metavar='STUDENT')
  parser.add_option('-m','--match',action='store_true',dest='matchNumEpisodes',default=False,help='matches the number of episodes between the results')
  parser.add_option('-q','--quantile',action='store',dest='quantile',default=1.0,help='fraction of data to use, 0.9 removes the lowest and highest 0.05',type='float')
  options,args = parser.parse_args(args)
  options.useQuantile = (options.quantile < 0.9999)

  if options.matchNumEpisodes and options.useQuantile:
    print >>sys.stderr,'Do not support matching with quantiles'
    return 2
  return main(args,options)

if __name__ == '__main__':
  import sys
  sys.exit(mainArgs(sys.argv[1:]))
