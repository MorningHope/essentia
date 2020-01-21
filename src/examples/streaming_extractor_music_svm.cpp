/*
 * Copyright (C) 2006-2020  Music Technology Group - Universitat Pompeu Fabra
 *
 * This file is part of Essentia
 *
 * Essentia is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation (FSF), either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the Affero GNU General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */

// Streaming extractor designed for high-level (classifier-based) analysis of
// music collections.

//#include <essentia/algorithm.h>
//#include <essentia/algorithmfactory.h>
//#include <essentia/pool.h>
#include <essentia/utils/extractor_music/extractor_version.h>
#include <gaia2/gaia.h>
#include "music_extractor/extractor_utils.h"

using namespace std;
using namespace essentia;
using namespace essentia::standard;

void usage(char *progname) {
  cout << "Error: wrong number of arguments" << endl;
  cout << "Usage: " << progname << " input_descriptorfile output_textfile [...] [profile]" << endl;
  cout << endl <<
"This extractor generates semantic annotation of music in terms of genres, mood,\n"
"type of rhythm, and instrumentation qualities, using a set of pretrained SVM\n"
"classifiers. It expects a json/yaml file generated by 'streaming_extractor_music'\n"
"extractor as its input" << endl;
  cout << endl << "Music extractor version '" << MUSIC_EXTRACTOR_VERSION << "'" << endl
       << "built with Essentia version " << essentia::version_git_sha << endl;

  exit(1);
}


int process_single_file(Algorithm *extractor, string descriptorsFilename, string outputFilename, string format, Pool& options) {

  Pool pool;
  // load descriptor file
  AlgorithmFactory& factory = AlgorithmFactory::instance();
  Algorithm* yamlInput = factory.create("YamlInput",
                                        "filename", descriptorsFilename,
                                        "format", format);
  yamlInput->output("pool").set(pool);
  yamlInput->compute();

  // apply SVM models and save
  extractor->input("pool").set(pool);
  extractor->output("pool").set(pool);
  extractor->compute();

  pool.removeNamespace("lowlevel");
  pool.removeNamespace("rhythm");
  pool.removeNamespace("tonal");

  vector<string> keys = pool.descriptorNames("metadata.version");
  for (int i=0; i<(int) keys.size(); ++i) {
      string k = keys[i];
      string newk = "metadata.version.lowlevel." + k.substr(17);
      pool.set(newk, pool.value<string>(keys[i]));
      pool.remove(keys[i]);
  }

  pool.set("metadata.version.highlevel.essentia", essentia::version);
  pool.set("metadata.version.highlevel.essentia_git_sha", essentia::version_git_sha);
  pool.set("metadata.version.highlevel.extractor", MUSIC_EXTRACTOR_VERSION);
  pool.set("metadata.version.highlevel.gaia", gaia2::version);
  pool.set("metadata.version.highlevel.gaia_git_sha", gaia2::version_git_sha);

  mergeValues(pool, options);
  outputToFile(pool, outputFilename, options);

  return 0;
}

int main(int argc, char* argv[]) {
  // Returns: 1 on essentia error

  string profileFilename;
  vector<string> inputFilenames;
  vector<string> outputFilenames;

  if (argc < 3) {
      usage(argv[0]);
  } else if (argc % 2 == 0) { // even, we have a profile name
      // argc 4 = prog, in, out, profile
      profileFilename = argv[argc-1];
  } else { // c%2 ==1, odd, no profile

  }

  //argc = 3: 1-3
  //argc = 4: 1->3
  //argc = 5: 1->5
  //argc = 6: 1->5

  // Number of arguments minus 1 if args are odd
  // and we have a profile filename
  int numinputs = argc - 1 + (argc%2);
  for (int i = 1; i < numinputs; i += 2) {
      inputFilenames.push_back(argv[i]);
      outputFilenames.push_back(argv[i+1]);
  }


  Algorithm* extractor;
  Pool options;

  try {
    essentia::init();
    cout.precision(10); // TODO ????

    setExtractorDefaultOptions(options);
    setExtractorOptions(profileFilename, options);  
    vector<string> svmModels = options.value<vector<string> >("highlevel.svm_models");
    
    extractor = AlgorithmFactory::create("MusicExtractorSVM", "svms", svmModels);
  }
  catch (EssentiaException& e) {
    cerr << e.what() << endl;
    return 1;
  }

  string format = options.value<string>("highlevel.inputFormat");
  if (format != "json" && format != "yaml") {
    cerr << "incorrect format specified: " << format << endl;
    return 1;
  }

  for (int i = 0; i < (int)inputFilenames.size(); i++) {
      string inputFilename = inputFilenames[i];
      string outputFilename = outputFilenames[i];
      try {
        process_single_file(extractor, inputFilename, outputFilename, format, options);
      // On Essentia Exception for a single file, skip it
      } catch (EssentiaException& e) {
        cerr << "skipping " << inputFilename << " due to error: " << e.what() << endl;
      }
  }

  try {
    delete extractor;
    essentia::shutdown();
  }
  catch (EssentiaException& e) {
    cout << e.what() << endl;
    return 1;
  }
  return 0;
}
